// Vimana pixel runtime implementation.
#include "vimana.h"
#include <SDL3/SDL.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#ifdef __APPLE__
#include <objc/message.h>
#endif

/* Blend look-up table: blend_lut[mode][layer(0=bg,1=fg)][color(0-15)]
   BG entries: palette slot (0-15) written to bits[3:0] of the layer byte.
   FG entries: palette slot pre-shifted left 4, written to bits[7:4].
   mode bits[3:2] = fill slot for color-0 (0-3); bits[1:0] = color rotation.
   Rotation 0: color1→0(transparent), 2→1,3→2,...,15→14
   Rotation 1: identity  1→1,2→2,...,15→15
   Rotation 2: cyclic+1  1→2,2→3,...,15→1
   Rotation 3: cyclic+2  1→3,2→4,...,15→2 */
static const uint8_t blend_lut[16][2][16] = {
    /* fill=0 */ {{0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14},
                 {0,  0,16,32,48,64,80,96,112,128,144,160,176,192,208,224}},
                 {{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
                 {0, 16,32,48,64,80,96,112,128,144,160,176,192,208,224,240}},
                 {{0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,1},
                 {0, 32,48,64,80,96,112,128,144,160,176,192,208,224,240,16}},
                 {{0,3,4,5,6,7,8,9,10,11,12,13,14,15,1,2},
                 {0, 48,64,80,96,112,128,144,160,176,192,208,224,240,16,32}},
    /* fill=1 */ {{1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14},
                 {16,  0,16,32,48,64,80,96,112,128,144,160,176,192,208,224}},
                 {{1,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
                 {16, 16,32,48,64,80,96,112,128,144,160,176,192,208,224,240}},
                 {{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,1},
                 {16, 32,48,64,80,96,112,128,144,160,176,192,208,224,240,16}},
                 {{1,3,4,5,6,7,8,9,10,11,12,13,14,15,1,2},
                 {16, 48,64,80,96,112,128,144,160,176,192,208,224,240,16,32}},
    /* fill=2 */ {{2,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14},
                 {32,  0,16,32,48,64,80,96,112,128,144,160,176,192,208,224}},
                 {{2,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
                 {32, 16,32,48,64,80,96,112,128,144,160,176,192,208,224,240}},
                 {{2,2,3,4,5,6,7,8,9,10,11,12,13,14,15,1},
                 {32, 32,48,64,80,96,112,128,144,160,176,192,208,224,240,16}},
                 {{2,3,4,5,6,7,8,9,10,11,12,13,14,15,1,2},
                 {32, 48,64,80,96,112,128,144,160,176,192,208,224,240,16,32}},
    /* fill=3 */ {{3,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14},
                 {48,  0,16,32,48,64,80,96,112,128,144,160,176,192,208,224}},
                 {{3,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
                 {48, 16,32,48,64,80,96,112,128,144,160,176,192,208,224,240}},
                 {{3,2,3,4,5,6,7,8,9,10,11,12,13,14,15,1},
                 {48, 32,48,64,80,96,112,128,144,160,176,192,208,224,240,16}},
                 {{3,3,4,5,6,7,8,9,10,11,12,13,14,15,1,2},
                 {48, 48,64,80,96,112,128,144,160,176,192,208,224,240,16,32}}};

/* ── Voice / Envelope types ──────────────────────────────────────────── */

enum VimanaEnvStage { VIMANA_ENV_OFF, VIMANA_ENV_ATTACK, VIMANA_ENV_DECAY,
                      VIMANA_ENV_SUSTAIN, VIMANA_ENV_RELEASE };

/* SID-style ADSR time tables (in seconds).
   Index 0–15 maps to attack / decay-release durations. */
static const float vimana_attack_table[16] = {
  0.002f, 0.008f, 0.016f, 0.024f, 0.038f, 0.056f, 0.068f, 0.080f,
  0.100f, 0.250f, 0.500f, 0.800f, 1.000f, 3.000f, 5.000f, 8.000f
};
static const float vimana_decrel_table[16] = {
  0.006f, 0.024f, 0.048f, 0.072f, 0.114f, 0.168f, 0.204f, 0.240f,
  0.300f, 0.750f, 1.500f, 2.400f, 3.000f, 9.000f, 15.00f, 24.00f
};

typedef struct {
  float    phase;        /* oscillator phase [0.0, 1.0) */
  float    prev_phase;   /* phase from previous sample (for sync detection) */
  float    freq_hz;      /* current frequency (derived from freq_reg) */
  uint16_t freq_reg;     /* 16-bit frequency register (SID-style) */
  float    amp;          /* target amplitude (volume) */
  int      waveform;     /* VIMANA_WAVE_* */
  float    pulse_width;  /* pulse duty cycle [0.0, 1.0) */
  int      adsr[4];      /* attack, decay, sustain, release (0–15 each) */
  int      env_stage;    /* VimanaEnvStage */
  float    env_level;    /* current envelope amplitude [0.0, 1.0] */
  bool     gate;         /* true = key held */
  bool     sync;         /* hard sync: reset phase when source voice wraps */
  bool     ring_mod;     /* ring modulation with source voice */
  int      samples_left; /* >0 for timed auto-release (play_tone mode) */
  uint32_t noise_lfsr;   /* Galois LFSR state for noise waveform */
  uint32_t age;          /* monotonic counter for voice-stealing */
} VimanaVoice;

typedef struct {
  int   cutoff;          /* 0–2047 (11-bit) */
  int   resonance;       /* 0–15 */
  int   mode;            /* bitmask: VIMANA_FILT_LP | BP | HP */
  bool  route[VIMANA_VOICE_COUNT]; /* per-voice filter routing */
  float bp;              /* bandpass state */
  float lp;              /* lowpass state */
} VimanaFilter;

struct VimanaSystem {
  uint64_t key_down[VIMANA_KEY_WORDS];     /* 512 bits packed */
  uint64_t key_pressed[VIMANA_KEY_WORDS];  /* 512 bits packed */
  uint8_t mouse_down;                      /* 8 bits packed */
  uint8_t mouse_pressed;                   /* 8 bits packed */
  bool quit;
  bool running;
  int16_t wheel_x;
  int16_t wheel_y;
  int16_t pointer_x;
  int16_t pointer_y;
  int16_t tile_x;
  int16_t tile_y;
  char text_input[VIMANA_TEXT_INPUT_CAP];
  SDL_AudioStream *audio_stream;
  int audio_sample_rate;
  VimanaVoice voices[VIMANA_VOICE_COUNT];
  VimanaFilter filter;
  int master_volume;       /* 0–15, default 15 */
  uint8_t paddle[VIMANA_PADDLE_COUNT]; /* 2 × 8-bit A/D converters */
  uint32_t voice_clock;    /* monotonic counter for age tracking */
  float *mix_buffer;       /* pre-allocated audio mix buffer */
  int mix_buffer_cap;      /* capacity in samples */
  bool audio_locked;       /* true while inside begin/end audio batch */
  bool audio_needs_resume; /* deferred resume until end_audio */
};

static float vimana_pitch_to_hz(int pitch) {
  /* Uxn-style pitch domain in semitone steps; 57 ~= A4 (440Hz). */
  float hz = 440.0f * powf(2.0f, ((float)pitch - 57.0f) / 12.0f);
  if (hz < 40.0f)
    hz = 40.0f;
  if (hz > 4000.0f)
    hz = 4000.0f;
  return hz;
}

/* Convert Hz to 16-bit SID frequency register value.
   freq_reg = hz × 16777216 / VIMANA_AUDIO_CLOCK */
static uint16_t vimana_hz_to_freq_reg(float hz) {
  float reg = hz * 16777216.0f / (float)VIMANA_AUDIO_CLOCK;
  if (reg < 0.0f) reg = 0.0f;
  if (reg > 65535.0f) reg = 65535.0f;
  return (uint16_t)(reg + 0.5f);
}

/* Convert 16-bit frequency register to Hz.
   freq_hz = freq_reg × VIMANA_AUDIO_CLOCK / 16777216 */
static float vimana_freq_reg_to_hz(uint16_t freq_reg) {
  return (float)freq_reg * (float)VIMANA_AUDIO_CLOCK / 16777216.0f;
}

/* ── Per-voice waveform generation ──────────────────────────────────── */

/* SID-style waveform amplitude normalization.
   The original SID chip outputs all waveforms at the same peak amplitude,
   but their harmonic content makes triangle/noise *perceive* quieter than
   pulse/sawtooth. We apply RMS normalization so all voices have equal
   perceived loudness while preserving their character.

   RMS values (for ±1 peak):
     Triangle: 1/sqrt(3) ≈ 0.577  → gain ≈ 1.732
     Sawtooth: 1/sqrt(3) ≈ 0.577  → gain ≈ 1.732
     Pulse:    1.0                 → gain =  1.0
     Noise:    ~0.577              → gain ≈ 1.732
     PSG:      1.0                 → gain =  1.0 */

static const float vimana_wave_gain[5] = {
  1.7320508f, /* VIMANA_WAVE_TRIANGLE */
  1.7320508f, /* VIMANA_WAVE_SAWTOOTH */
  1.0f,       /* VIMANA_WAVE_PULSE */
  0.25f,      /* VIMANA_WAVE_NOISE — broadband energy needs attenuation */
  1.0f        /* VIMANA_WAVE_PSG */
};

/* Triangle waveform from phase — used standalone for ring modulation source */
static inline float vimana_triangle_from_phase(float p) {
  return 2.0f * fabsf(2.0f * p - 1.0f) - 1.0f;
}

static inline float vimana_voice_sample(VimanaVoice *v, int sr) {
  float p = v->phase;
  float s;
  switch (v->waveform) {
  case VIMANA_WAVE_TRIANGLE:
    s = vimana_triangle_from_phase(p);
    break;
  case VIMANA_WAVE_SAWTOOTH:
    s = 2.0f * p - 1.0f;
    break;
  case VIMANA_WAVE_PULSE:
    s = (p < v->pulse_width) ? 1.0f : -1.0f;
    break;
  case VIMANA_WAVE_NOISE: {
    /* SID-style noise: 23-bit LFSR clocked at oscillator frequency.
       The real SID clocks the LFSR at freq_hz, so at higher pitches
       it advances many times per sample producing true white noise. */
    if (sr > 0 && v->freq_hz > 0) {
      float clocks = v->freq_hz / (float)sr;
      int n = (int)clocks;
      /* Always clock at least once per sample if freq > 0 */
      if (n == 0) n = 1;
      /* Cap to avoid CPU waste at extreme high frequencies */
      if (n > 128) n = 128;

      for (int i = 0; i < n; i++) {
        uint32_t lf = v->noise_lfsr;
        if (lf == 0) lf = 0x7FFFFF; /* 23-bit, non-zero seed */
        /* XOR feedback at bits 22 and 17 (SID 6581 taps) */
        unsigned int bit = ((lf >> 22) ^ (lf >> 17)) & 1;
        lf = (lf << 1) | bit;
        v->noise_lfsr = lf & 0x7FFFFF; /* Keep 23 bits */
      }
    }
    /* Output is the LSB of the LFSR */
    s = (v->noise_lfsr & 1) ? 1.0f : -1.0f;
    break;
  }
  case VIMANA_WAVE_PSG:
    /* 80s PSG: fixed 50% square (AY-3-8910 style) */
    s = (p < 0.5f) ? 1.0f : -1.0f;
    break;
  default: /* fallback: pulse (square) */
    s = (p < 0.5f) ? 1.0f : -1.0f;
    break;
  }
  /* Apply SID-style RMS normalization gain */
  return s * vimana_wave_gain[v->waveform];
}

/* ── ADSR envelope tick (called once per sample per voice) ──────────── */

static inline void vimana_env_tick(VimanaVoice *v, int sample_rate) {
  float rate;
  switch (v->env_stage) {
  case VIMANA_ENV_ATTACK: {
    float t = vimana_attack_table[v->adsr[0]];
    rate = 1.0f / (t * (float)sample_rate);
    v->env_level += rate;
    if (v->env_level >= 1.0f) {
      v->env_level = 1.0f;
      v->env_stage = VIMANA_ENV_DECAY;
    }
    break;
  }
  case VIMANA_ENV_DECAY: {
    float sustain_level = (float)v->adsr[2] / 15.0f;
    float t = vimana_decrel_table[v->adsr[1]];
    rate = 1.0f / (t * (float)sample_rate);
    v->env_level -= rate;
    if (v->env_level <= sustain_level) {
      v->env_level = sustain_level;
      v->env_stage = VIMANA_ENV_SUSTAIN;
    }
    break;
  }
  case VIMANA_ENV_SUSTAIN:
    v->env_level = (float)v->adsr[2] / 15.0f;
    break;
  case VIMANA_ENV_RELEASE: {
    float t = vimana_decrel_table[v->adsr[3]];
    rate = 1.0f / (t * (float)sample_rate);
    v->env_level -= rate;
    if (v->env_level <= 0.0f) {
      v->env_level = 0.0f;
      v->env_stage = VIMANA_ENV_OFF;
    }
    break;
  }
  default:
    v->env_level = 0.0f;
    break;
  }
}

static void SDLCALL vimana_audio_stream_cb(void *userdata,
                                           SDL_AudioStream *stream,
                                           int additional_amount,
                                           int total_amount) {
  (void)total_amount;
  vimana_system *system = (vimana_system *)userdata;
  if (!system || !stream)
    return;

  int wanted = additional_amount;
  if (wanted <= 0)
    wanted = (int)(sizeof(float) * 1024);
  int sample_count = wanted / (int)sizeof(float);
  if (sample_count < 256)
    sample_count = 256;

  /* Grow pre-allocated buffer if needed */
  if (sample_count > system->mix_buffer_cap) {
    float *grown = (float *)realloc(system->mix_buffer,
                                    (size_t)sample_count * sizeof(float));
    if (!grown)
      return;
    system->mix_buffer = grown;
    system->mix_buffer_cap = sample_count;
  }
  float *buffer = system->mix_buffer;
  memset(buffer, 0, (size_t)sample_count * sizeof(float));

  int sr = system->audio_sample_rate;
  VimanaFilter *filt = &system->filter;

  /* Filter cutoff: map 0–2047 to normalized angular frequency.
     Approximate SID mapping: fc ≈ cutoff / 2048 × (π/sr) scaled. */
  float fc = (float)filt->cutoff / 2048.0f;
  fc = fc * fc; /* quadratic curve for more musical sweep */
  if (fc > 0.99f) fc = 0.99f;
  /* Resonance: higher = more feedback = more resonance (SID-accurate).
     q_factor approaches 0 at resonance=15, nearing self-oscillation. */
  float q_factor = 1.0f - (float)filt->resonance / 16.0f;
  if (q_factor < 0.01f) q_factor = 0.01f;

  /* Master volume: 0–15 linear scale */
  float master_scale = (float)system->master_volume / 15.0f;

  for (int i = 0; i < sample_count; i++) {
    float voice_out[VIMANA_VOICE_COUNT];

    /* 1. Phase advance for all voices */
    for (int ch = 0; ch < VIMANA_VOICE_COUNT; ch++) {
      VimanaVoice *v = &system->voices[ch];
      v->prev_phase = v->phase;
      float step = (sr > 0) ? (v->freq_hz / (float)sr) : 0.0f;
      v->phase += step;
      if (v->phase >= 1.0f)
        v->phase -= floorf(v->phase);
    }

    /* 2. Oscillator sync (tone voices 0–2 and 4–6) */
    for (int ch = 0; ch < 3; ch++) {
      VimanaVoice *v = &system->voices[ch];
      if (!v->sync)
        continue;
      int src = (ch + 2) % 3; /* circular: 0←2, 1←0, 2←1 */
      VimanaVoice *sv = &system->voices[src];
      if (sv->phase < sv->prev_phase)
        v->phase = 0.0f;
    }
    for (int ch = 4; ch < 7; ch++) {
      VimanaVoice *v = &system->voices[ch];
      if (!v->sync)
        continue;
      int src = 4 + ((ch - 4 + 2) % 3); /* circular: 4←6, 5←4, 6←5 */
      VimanaVoice *sv = &system->voices[src];
      if (sv->phase < sv->prev_phase)
        v->phase = 0.0f;
    }

    /* 3. Waveform generation + 4. Ring modulation */
    for (int ch = 0; ch < VIMANA_VOICE_COUNT; ch++) {
      VimanaVoice *v = &system->voices[ch];
      voice_out[ch] = 0.0f;
      if (v->env_stage == VIMANA_ENV_OFF)
        continue;

      /* Auto-release timer (play_tone mode) */
      if (v->samples_left > 0) {
        v->samples_left--;
        if (v->samples_left == 0 && v->gate) {
          v->gate = false;
          v->env_stage = VIMANA_ENV_RELEASE;
        }
      }

      /* Advance envelope */
      vimana_env_tick(v, sr);
      if (v->env_stage == VIMANA_ENV_OFF)
        continue;

      float s = vimana_voice_sample(v, sr);

      /* Ring modulation (tone voices 0–2 and 4–6).
         SID-style: multiply by source voice's raw waveform output. */
      if (ch < 3 && v->ring_mod) {
        int src = (ch + 2) % 3;
        float mod = vimana_voice_sample(&system->voices[src], sr);
        s *= mod;
      } else if (ch >= 4 && ch < 7 && v->ring_mod) {
        int src = 4 + ((ch - 4 + 2) % 3);
        float mod = vimana_voice_sample(&system->voices[src], sr);
        s *= mod;
      }

      /* 5. Amplitude × Envelope */
      voice_out[ch] = s * v->amp * v->env_level;

      /* PSG 4-bit DAC quantization — AY-3-8910 style logarithmic volume.
         The AY-3-8910 volume table follows ~1.5 dB per step (logarithmic),
         not linear steps. We quantize to 16 levels matching the AY curve. */
      if (v->waveform == VIMANA_WAVE_PSG) {
        /* AY-3-8910 logarithmic amplitude table (normalized to 0-1) */
        static const float ay_vol_table[16] = {
          0.0000f, 0.0134f, 0.0168f, 0.0211f,
          0.0266f, 0.0335f, 0.0422f, 0.0531f,
          0.0668f, 0.0841f, 0.1059f, 0.1333f,
          0.1678f, 0.2112f, 0.2658f, 0.3346f
        };
        /* Map from [-1,1] to AY amplitude steps */
        float abs_val = voice_out[ch];
        if (abs_val < 0.0f) abs_val = -abs_val;
        int level = (int)(abs_val * 15.0f + 0.5f);
        if (level > 15) level = 15;
        if (level < 0) level = 0;
        float ay_amp = ay_vol_table[level];
        float sign = (voice_out[ch] >= 0.0f) ? 1.0f : -1.0f;
        voice_out[ch] = sign * ay_amp * 3.0f; /* normalize to ~[-1,1] range */
      }
    }

    /* 6. Filter routing: split into filtered/direct paths */
    float filt_in = 0.0f;
    float direct  = 0.0f;
    for (int ch = 0; ch < VIMANA_VOICE_COUNT; ch++) {
      if (filt->route[ch])
        filt_in += voice_out[ch];
      else
        direct += voice_out[ch];
    }

    /* 7. State-variable filter tick */
    float filt_out = 0.0f;
    if (filt->mode != 0 && filt_in != 0.0f) {
      float hp = filt_in - filt->lp - q_factor * filt->bp;
      filt->bp += fc * hp;
      filt->lp += fc * filt->bp;
      if (filt->mode & VIMANA_FILT_LP) filt_out += filt->lp;
      if (filt->mode & VIMANA_FILT_BP) filt_out += filt->bp;
      if (filt->mode & VIMANA_FILT_HP) filt_out += hp;
    } else if (filt->mode != 0) {
      /* Keep filter state ticking even with zero input */
      float hp = -filt->lp - q_factor * filt->bp;
      filt->bp += fc * hp;
      filt->lp += fc * filt->bp;
    }

    /* 8. Mix with master volume */
    float mix = (filt_out + direct) * master_scale;

    /* 9. Clamp to [-1, 1] */
    if (mix > 1.0f)  mix = 1.0f;
    if (mix < -1.0f) mix = -1.0f;
    buffer[i] = mix;
  }

  (void)SDL_PutAudioStreamData(stream, buffer,
                               sample_count * (int)sizeof(float));
}

static void vimana_voice_init(VimanaVoice *v) {
  v->phase = 0.0f;
  v->prev_phase = 0.0f;
  v->freq_hz = 0.0f;
  v->freq_reg = 0;
  v->amp = 0.0f;
  v->waveform = VIMANA_WAVE_PULSE;
  v->pulse_width = 0.5f;
  v->adsr[0] = 0;   /* instant attack */
  v->adsr[1] = 0;   /* instant decay */
  v->adsr[2] = 15;  /* full sustain */
  v->adsr[3] = 0;   /* fast release */
  v->env_stage = VIMANA_ENV_OFF;
  v->env_level = 0.0f;
  v->gate = false;
  v->sync = false;
  v->ring_mod = false;
  v->samples_left = 0;
  v->noise_lfsr = 0x7FFFFF; /* 23-bit LFSR seed (non-zero) */
  v->age = 0;
}

static void vimana_filter_init(VimanaFilter *f) {
  f->cutoff = 0;
  f->resonance = 0;
  f->mode = 0;
  for (int i = 0; i < VIMANA_VOICE_COUNT; i++)
    f->route[i] = false;
  f->bp = 0.0f;
  f->lp = 0.0f;
}

static void vimana_system_init_audio(vimana_system *system) {
  if (!system)
    return;
  SDL_AudioSpec spec;
  SDL_zero(spec);
  spec.freq = VIMANA_AUDIO_SAMPLE_RATE;
  spec.format = SDL_AUDIO_F32;
  spec.channels = VIMANA_AUDIO_CHANNELS;

  system->audio_stream = SDL_OpenAudioDeviceStream(
      SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
      vimana_audio_stream_cb, system);
  if (!system->audio_stream)
    return;

  system->audio_sample_rate = VIMANA_AUDIO_SAMPLE_RATE;
  system->voice_clock = 0;
  system->master_volume = 15;
  for (int i = 0; i < VIMANA_PADDLE_COUNT; i++)
    system->paddle[i] = 0;
  for (int i = 0; i < VIMANA_VOICE_COUNT; i++)
    vimana_voice_init(&system->voices[i]);
  vimana_filter_init(&system->filter);

  /* Pre-allocate mix buffer — 2048 samples (~46ms at 44100 Hz) */
  system->mix_buffer_cap = 2048;
  system->mix_buffer =
      (float *)calloc((size_t)system->mix_buffer_cap, sizeof(float));

  (void)SDL_ResumeAudioStreamDevice(system->audio_stream);
}

static inline bool vimana_bit_get(const uint64_t *bits, int idx) {
  return (bits[idx >> 6] >> (idx & 63)) & 1;
}
static inline void vimana_bit_set(uint64_t *bits, int idx) {
  bits[idx >> 6] |= (uint64_t)1 << (idx & 63);
}
static inline void vimana_bit_clr(uint64_t *bits, int idx) {
  bits[idx >> 6] &= ~((uint64_t)1 << (idx & 63));
}

/* ── ROM section accessor helpers ───────────────────────────────────────── */
/* (Implementations below VimanaScreen definition; forward-declared here.) */

static inline uint8_t *vimana_rom_font_widths(vimana_screen *s);
static inline uint8_t *vimana_rom_font_bitmap(vimana_screen *s, unsigned int code);
static inline uint8_t *vimana_rom_font_uf2(vimana_screen *s, unsigned int code);
static inline uint8_t *vimana_ensure_sprite_bank(vimana_screen *s, unsigned int bank);
static inline uint8_t *vimana_active_sprite_bank(vimana_screen *s);

struct VimanaScreen {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  char *title;
  uint16_t width;
  uint16_t height;
  uint8_t scale;
  uint16_t width_mar;   /* width + 16 (8px margin each side) */
  uint16_t height_mar;  /* height + 16 */
  uint32_t base_colors[16]; /* ARGB palette, slots 0-15 (BG,FG,CLR2..CLR15) */
  uint32_t palette[256];    /* expanded 256-entry composite (8-bit layer index) */
  /* Font ROM: always allocated (64 KB). */
  uint8_t *font_rom;                             /* VIMANA_FONT_SIZE bytes */
  /* GFX ROM: lazy-allocated on first set_gfx (384 KB). */
  uint8_t *gfx_rom;                              /* VIMANA_GFX_SIZE bytes, or NULL */
  /* Sprite banks: 16 × 64 KB, bank-switched.  Lazy-allocated (NULL until first use). */
  uint8_t *sprite_banks[VIMANA_SPRITE_BANK_COUNT];
  uint8_t active_sprite_bank;                    /* 0–15 */
  /* RAM-class state */
  uint16_t port_x;
  uint16_t port_y;
  uint16_t port_addr;
  uint8_t port_auto;
  uint8_t font_height;           /* UF1=8, UF2=16, UF3=24 */
  uint8_t font_glyph_width;      /* bitmap pixel width: 8 (UF1/UF2) or 24 (UF3) */
  uint8_t *layers;               /* width_mar * height_mar bytes: bits[1:0]=bg, bits[3:2]=fg */
  char *titlebar_title;          /* NULL = use screen->title */
  time_t theme_mtime;            /* last known mtime of ~/.theme */
  int16_t titlebar_height;       /* total titlebar height */
  int16_t titlebar_bar_height;   /* drawn bar height */
  int16_t titlebar_box_size;     /* close/button box size */
  int16_t titlebar_box_y;        /* y of close/button box within bar */
  uint8_t titlebar_bg_slot;      /* palette slot for titlebar background */
  bool titlebar_has_button;      /* show optional right button */
  bool titlebar_button_pressed;  /* set for one frame when right button clicked */
  bool theme_swap_fg_bg;         /* swap base_colors[0] and [1] after loading */
  int16_t theme_poll_counter;    /* frame counter for periodic theme check */
  SDL_Texture *titlebar_tex;     /* cached titlebar texture (rebuilt on change) */
  bool titlebar_dirty;           /* true = rebuild titlebar_tex next present */
  vimana_system *system;         /* parent system for device input access */
  /* Custom cursor */
  uint8_t cursor_icn[8];         /* 8×8 cursor icon data */
  bool cursor_visible;          /* is cursor currently visible */
  bool cursor_dirty;             /* cursor data changed */
  unsigned int cursor_fg;        /* cursor foreground color slot */
  unsigned int cursor_bg;        /* cursor background color slot */
};

/* ── ROM accessor implementations (need complete struct definition) ───── */

static inline uint8_t *vimana_rom_font_widths(vimana_screen *s) {
  return s->font_rom + VIMANA_FONT_WIDTH_OFF;
}
static inline uint8_t *vimana_rom_font_bitmap(vimana_screen *s, unsigned int code) {
  return s->font_rom + VIMANA_FONT_BMP_OFF
         + code * VIMANA_FONT_GLYPH_BYTES;
}
static inline uint8_t *vimana_rom_font_uf2(vimana_screen *s, unsigned int code) {
  return s->font_rom + VIMANA_FONT_UF2_OFF
         + code * VIMANA_UF2_BYTES;
}
static inline uint8_t *vimana_ensure_sprite_bank(vimana_screen *s, unsigned int bank) {
  if (bank >= VIMANA_SPRITE_BANK_COUNT)
    return NULL;
  if (!s->sprite_banks[bank])
    s->sprite_banks[bank] = (uint8_t *)calloc(VIMANA_SPRITE_BANK_SIZE, 1);
  return s->sprite_banks[bank];
}
static inline uint8_t *vimana_active_sprite_bank(vimana_screen *s) {
  return vimana_ensure_sprite_bank(s, s->active_sprite_bank);
}

static uint32_t vimana_parse_hex_color(const char *hex) {
  unsigned int r = 0, g = 0, b = 0;
  if (!hex)
    return 0xFF101418u;
  const char *h = hex;
  if (*h == '#')
    h++;
  size_t len = strlen(h);
  if (len == 3) {
    sscanf(h, "%1x%1x%1x", &r, &g, &b);
    r = (r << 4) | r;
    g = (g << 4) | g;
    b = (b << 4) | b;
  } else if (len >= 6) {
    sscanf(h, "%2x%2x%2x", &r, &g, &b);
  }
  return 0xFF000000u | (uint32_t)(r << 16) | (uint32_t)(g << 8) | (uint32_t)b;
}

static void vimana_colorize(vimana_screen *screen) {
  if (!screen)
    return;
  /* Layer byte: bits[7:4]=FG slot, bits[3:0]=BG slot.
     FG≠0 wins; FG=0 is transparent so BG slot is used. */
  for (unsigned int i = 0; i < 256; i++) {
    unsigned int fg_slot = i >> 4;
    unsigned int bg_slot = i & 15;
    unsigned int slot = fg_slot ? fg_slot : bg_slot;
    screen->palette[i] = screen->base_colors[slot];
  }
}

static void vimana_screen_reset_palette(vimana_screen *screen) {
  if (!screen)
    return;
  screen->base_colors[0]  = 0xFFFFFFFFu; /* BG   */
  screen->base_colors[1]  = 0xFF000000u; /* FG   */
  screen->base_colors[2]  = 0xFF77DDCCu; /* CLR2 */
  screen->base_colors[3]  = 0xFFFFBB22u; /* CLR3 */
  screen->base_colors[4]  = 0xFFFF4444u; /* CLR4 */
  screen->base_colors[5]  = 0xFF44AA44u; /* CLR5 */
  screen->base_colors[6]  = 0xFF4488FFu; /* CLR6 */
  screen->base_colors[7]  = 0xFFAA44AAu; /* CLR7 */
  screen->base_colors[8]  = 0xFF888888u; /* CLR8 */
  screen->base_colors[9]  = 0xFF666666u; /* CLR9 */
  screen->base_colors[10] = 0xFFBBBB44u; /* CLR10 */
  screen->base_colors[11] = 0xFF44BBBBu; /* CLR11 */
  screen->base_colors[12] = 0xFF8844BBu; /* CLR12 */
  screen->base_colors[13] = 0xFFBB4488u; /* CLR13 */
  screen->base_colors[14] = 0xFF44BB88u; /* CLR14 */
  screen->base_colors[15] = 0xFFBB8844u; /* CLR15 */
  vimana_colorize(screen);
}

static void vimana_screen_load_theme(vimana_screen *screen) {
  if (!screen)
    return;
  const char *home = getenv("HOME");
  if (!home)
    return;
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/.theme", home);
  FILE *f = fopen(path, "rb");
  if (!f)
    return;
  uint8_t buf[14];
  if (fread(buf, 1, 14, f) != 14 || buf[4] != 0x20 || buf[9] != 0x20) {
    fclose(f);
    return;
  }
  fclose(f);
  for (int i = 0; i < 4; i++)
    screen->base_colors[i] =
        0xFF000000u | ((uint32_t)buf[i] << 16) | ((uint32_t)buf[5 + i] << 8) |
        (uint32_t)buf[10 + i];
  if (screen->theme_swap_fg_bg) {
    uint32_t tmp = screen->base_colors[0];
    screen->base_colors[0] = screen->base_colors[1];
    screen->base_colors[1] = tmp;
  }
  vimana_colorize(screen);
  screen->titlebar_dirty = true;
  struct stat st;
  if (stat(path, &st) == 0)
    screen->theme_mtime = st.st_mtime;
}

static void vimana_screen_poll_theme(vimana_screen *screen) {
  if (!screen)
    return;
  const char *home = getenv("HOME");
  if (!home)
    return;
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/.theme", home);
  struct stat st;
  if (stat(path, &st) != 0)
    return;
  if (st.st_mtime != screen->theme_mtime) {
    vimana_screen_load_theme(screen);
    vimana_screen_present(screen);
  }
}

void vimana_screen_set_theme_swap(vimana_screen *screen, bool swap) {
  if (!screen)
    return;
  screen->theme_swap_fg_bg = swap;
  vimana_screen_load_theme(screen);
}

static void vimana_screen_update_titlebar_sizes(vimana_screen *screen) {
  if (!screen) return;
  unsigned int fh = screen->font_height;
  screen->titlebar_bar_height = fh < 16 ? fh + 7 : fh + 1;
  screen->titlebar_height     = fh < 16 ? fh + 7 : fh + 1;
  int bsz_max = VIMANA_TB_BOX_SIZE + (screen->titlebar_bar_height > VIMANA_TITLEBAR_HEIGHT ? 1 : 0);
  int bsz = bsz_max < screen->titlebar_bar_height - 1
            ? bsz_max : screen->titlebar_bar_height - 1;
  screen->titlebar_box_size   = bsz;
  screen->titlebar_box_y      = (screen->titlebar_bar_height - bsz) / 2;
  screen->titlebar_dirty = true;
}

static void vimana_screen_reset_font(vimana_screen *screen) {
  if (!screen || !screen->font_rom)
    return;
  /* Clear font region */
  memset(screen->font_rom, 0, VIMANA_FONT_SIZE);
  /* Initialize width table: default glyph width for all entries */
  uint8_t *widths = vimana_rom_font_widths(screen);
  for (unsigned int i = 0; i < VIMANA_GLYPH_COUNT; i++)
    widths[i] = VIMANA_TILE_SIZE;
  screen->font_height = VIMANA_TILE_SIZE; /* UF1 default */
  screen->font_glyph_width = VIMANA_TILE_SIZE;
  vimana_screen_update_titlebar_sizes(screen);
}

/* Convert 16 packed 2bpp rows into UF2 planar layout (32 bytes).
   UF2 layout: tile0_p0[8] tile0_p1[8] tile1_p0[8] tile1_p1[8] */
static void vimana_rows_to_uf2(const uint16_t rows[16],
                                uint8_t uf2[VIMANA_UF2_BYTES]) {
  for (int tile = 0; tile < 2; tile++) {
    for (int r = 0; r < VIMANA_TILE_SIZE; r++) {
      uint8_t plane0 = 0;
      uint8_t plane1 = 0;
      uint16_t packed = rows[tile * VIMANA_TILE_SIZE + r];
      for (int col = 0; col < VIMANA_TILE_SIZE; col++) {
        uint16_t pair = (uint16_t)((packed >> ((7 - col) * 2)) & 0x3u);
        uint8_t mask = (uint8_t)(0x80u >> col);
        if ((pair & 0x1u) != 0)
          plane0 = (uint8_t)(plane0 | mask);
        if ((pair & 0x2u) != 0)
          plane1 = (uint8_t)(plane1 | mask);
      }
      uf2[tile * 16 + r] = plane0;
      uf2[tile * 16 + r + VIMANA_TILE_SIZE] = plane1;
    }
  }
}

static void vimana_screen_reset_ports(vimana_screen *screen) {
  if (!screen)
    return;
  screen->port_x = 0;
  screen->port_y = 0;
  screen->port_addr = 0;
  screen->port_auto = 0;
}

static unsigned int vimana_sprite_stride(unsigned int mode) {
  if (mode >= 3) return VIMANA_SPRITE_4BPP_BYTES;
  if (mode == 2) return VIMANA_SPRITE_3BPP_BYTES;
  return mode ? VIMANA_SPRITE_2BPP_BYTES : VIMANA_SPRITE_1BPP_BYTES;
}

static unsigned int vimana_screen_addr_clamp(vimana_screen *screen,
                                              unsigned int addr) {
  (void)screen;
  if (addr >= VIMANA_SPRITE_BANK_SIZE)
    return VIMANA_SPRITE_BANK_SIZE - 1;
  return addr;
}

static void vimana_screen_store_sprite_bytes(vimana_screen *screen,
                                             unsigned int addr,
                                             const uint8_t *sprite,
                                             unsigned int mode) {
  if (!screen || !sprite)
    return;
  uint8_t *bank = vimana_active_sprite_bank(screen);
  if (!bank)
    return;
  unsigned int stride = vimana_sprite_stride(mode);
  if (addr + stride > VIMANA_SPRITE_BANK_SIZE)
    return;
  memcpy(bank + addr, sprite, (size_t)stride);
}

static int vimana_screen_auto_repeat(vimana_screen *screen) {
  if (!screen)
    return 1;
  return ((screen->port_auto >> 4) & 0x0F) + 1;
}

static void vimana_reset_pressed(vimana_system *system) {
  if (!system)
    return;
  memset(system->key_pressed, 0, sizeof(system->key_pressed));
  system->mouse_pressed = 0;
  system->wheel_x = 0;
  system->wheel_y = 0;
  system->text_input[0] = 0;
}

static void vimana_append_text_input(vimana_system *system, const char *text) {
  if (!system || !text || !text[0])
    return;
  size_t used = strlen(system->text_input);
  if (used >= VIMANA_TEXT_INPUT_CAP - 1)
    return;
  size_t avail = (size_t)VIMANA_TEXT_INPUT_CAP - 1 - used;
  strncat(system->text_input, text, avail);
}

static void vimana_update_pointer(vimana_system *system, vimana_screen *screen,
                                  unsigned int x, unsigned int y) {
  if (!system)
    return;
  unsigned int scale = (screen && screen->scale > 0) ? screen->scale : 1;
  system->pointer_x = x / scale;
  system->pointer_y = (y - screen->titlebar_height) / scale;
  system->tile_x = system->pointer_x / VIMANA_TILE_SIZE;
  system->tile_y = system->pointer_y / VIMANA_TILE_SIZE;
}

typedef enum VimanaDatetimePart {
  VIMANA_DT_YEAR = 0,
  VIMANA_DT_MONTH,
  VIMANA_DT_DAY,
  VIMANA_DT_HOUR,
  VIMANA_DT_MINUTE,
  VIMANA_DT_SECOND,
  VIMANA_DT_WEEKDAY,
} VimanaDatetimePart;

static bool vimana_localtime_safe(time_t ts, struct tm *out_tm) {
  if (!out_tm)
    return false;
#if defined(_WIN32)
  return localtime_s(out_tm, &ts) == 0;
#else
  return localtime_r(&ts, out_tm) != NULL;
#endif
}

static int64_t vimana_datetime_now_value(void) {
  time_t now = time(NULL);
  if (now == (time_t)-1)
    return 0;
  return (int64_t)now;
}

static int vimana_datetime_part_value(int64_t timestamp,
                                      VimanaDatetimePart part) {
  time_t ts = (time_t)timestamp;
  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  if (!vimana_localtime_safe(ts, &tmv))
    return 0;
  switch (part) {
  case VIMANA_DT_YEAR:
    return tmv.tm_year + 1900;
  case VIMANA_DT_MONTH:
    return tmv.tm_mon + 1;
  case VIMANA_DT_DAY:
    return tmv.tm_mday;
  case VIMANA_DT_HOUR:
    return tmv.tm_hour;
  case VIMANA_DT_MINUTE:
    return tmv.tm_min;
  case VIMANA_DT_SECOND:
    return tmv.tm_sec;
  case VIMANA_DT_WEEKDAY:
    return tmv.tm_wday;
  default:
    return 0;
  }
}

static void vimana_pump_events(vimana_system *system, vimana_screen *screen) {
  if (!system)
    return;
  if (screen)
    screen->titlebar_button_pressed = false;
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      system->quit = true;
      break;
    case SDL_EVENT_KEY_DOWN: {
      int scancode = (int)event.key.scancode;
      if (scancode >= 0 && scancode < VIMANA_KEY_CAP) {
        if (!vimana_bit_get(system->key_down, scancode) && !event.key.repeat)
          vimana_bit_set(system->key_pressed, scancode);
        vimana_bit_set(system->key_down, scancode);
      }
      break;
    }
    case SDL_EVENT_KEY_UP: {
      int scancode = (int)event.key.scancode;
      if (scancode >= 0 && scancode < VIMANA_KEY_CAP)
        vimana_bit_clr(system->key_down, scancode);
      break;
    }
    case SDL_EVENT_MOUSE_MOTION:
      vimana_update_pointer(system, screen, (int)event.motion.x,
                            (int)event.motion.y);
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      int button = (int)event.button.button;
      int ex = (int)event.button.x;
      int ey = (int)event.button.y;
      if (screen && ey < screen->titlebar_height) {
        /* System 6 titlebar button clicks */
        if (button == SDL_BUTTON_LEFT &&
            ey >= screen->titlebar_box_y && ey < screen->titlebar_box_y + screen->titlebar_box_size) {
          /* Close box */
          if (ex >= VIMANA_TB_CLOSE_X && ex < VIMANA_TB_CLOSE_X + screen->titlebar_box_size)
            system->quit = true;
          /* Optional right button */
          if (screen->titlebar_has_button) {
            int win_w = screen->width * screen->scale;
            int bx = win_w - VIMANA_TB_CLOSE_X - screen->titlebar_box_size;
            if (ex >= bx && ex < bx + screen->titlebar_box_size)
              screen->titlebar_button_pressed = true;
          }
        }
        break; /* don't pass titlebar clicks to canvas */
      }
      if (button >= 0 && button < VIMANA_MOUSE_CAP) {
        if (!(system->mouse_down & (1u << button)))
          system->mouse_pressed |= (uint8_t)(1u << button);
        system->mouse_down |= (uint8_t)(1u << button);
      }
      vimana_update_pointer(system, screen, ex, ey);
      break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      int button = (int)event.button.button;
      if (button >= 0 && button < VIMANA_MOUSE_CAP)
        system->mouse_down &= (uint8_t)~(1u << button);
      vimana_update_pointer(system, screen, (int)event.button.x,
                            (int)event.button.y);
      break;
    }
    case SDL_EVENT_MOUSE_WHEEL:
      system->wheel_x += (int)event.wheel.x;
      system->wheel_y += (int)event.wheel.y;
      break;
    case SDL_EVENT_TEXT_INPUT:
      vimana_append_text_input(system, event.text.text);
      break;
    default:
      break;
    }
  }
}

vimana_system *vimana_system_new(void) {
  if (!SDL_WasInit(SDL_INIT_VIDEO)) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO))
      return NULL;
  } else {
    if (!SDL_WasInit(SDL_INIT_EVENTS))
      (void)SDL_Init(SDL_INIT_EVENTS);
    if (!SDL_WasInit(SDL_INIT_AUDIO))
      (void)SDL_Init(SDL_INIT_AUDIO);
  }
  vimana_system *system = (vimana_system *)calloc(1, sizeof(vimana_system));
  if (!system)
    return NULL;
  system->pointer_x = -1;
  system->pointer_y = -1;
  system->tile_x = -1;
  system->tile_y = -1;
  vimana_system_init_audio(system);
  return system;
}

void vimana_system_quit(vimana_system *system) {
  if (!system)
    return;
  system->quit = true;
}

bool vimana_system_running(vimana_system *system) {
  return system ? system->running : false;
}

int64_t vimana_system_ticks(vimana_system *system) {
  (void)system;
  return (int64_t)SDL_GetTicks();
}

void vimana_system_sleep(vimana_system *system, int64_t ms) {
  (void)system;
  if (ms <= 0)
    return;
  if ((uint64_t)ms > UINT32_MAX)
    ms = UINT32_MAX;
  SDL_Delay((uint32_t)ms);
}

bool vimana_system_set_clipboard_text(vimana_system *system,
                                      const char *text) {
  (void)system;
  const char *value = text ? text : "";
  return SDL_SetClipboardText(value);
}

char *vimana_system_clipboard_text(vimana_system *system) {
  (void)system;
  char *text = SDL_GetClipboardText();
  if (!text || !text[0]) {
    if (text)
      SDL_free(text);
    return NULL;
  }
  char *copy = strdup(text);
  SDL_free(text);
  return copy;
}

char *vimana_system_home_dir(vimana_system *system) {
  (void)system;
  const char *home = getenv("HOME");
  if (!home || !home[0])
    home = "/tmp";
  return strdup(home);
}

/* ── Batch audio lock ─────────────────────────────────────────────────
   Call begin_audio / end_audio to take the stream lock once for a batch
   of voice/filter parameter changes instead of locking per call. */

static inline void vimana_audio_lock(vimana_system *s) {
  if (!s->audio_locked)
    (void)SDL_LockAudioStream(s->audio_stream);
}
static inline void vimana_audio_unlock(vimana_system *s) {
  if (!s->audio_locked)
    (void)SDL_UnlockAudioStream(s->audio_stream);
}

void vimana_system_begin_audio(vimana_system *system) {
  if (!system || !system->audio_stream)
    return;
  if (!system->audio_locked) {
    (void)SDL_LockAudioStream(system->audio_stream);
    system->audio_locked = true;
    system->audio_needs_resume = false;
  }
}

void vimana_system_end_audio(vimana_system *system) {
  if (!system || !system->audio_stream)
    return;
  if (system->audio_locked) {
    system->audio_locked = false;
    (void)SDL_UnlockAudioStream(system->audio_stream);
    if (system->audio_needs_resume)
      (void)SDL_ResumeAudioStreamDevice(system->audio_stream);
  }
}

/* Find the best voice to allocate: prefer OFF voices, then oldest playing. */
static int vimana_alloc_voice(vimana_system *system) {
  int best = 0;
  uint32_t oldest_age = UINT32_MAX;
  for (int i = 0; i < VIMANA_VOICE_COUNT; i++) {
    if (system->voices[i].env_stage == VIMANA_ENV_OFF)
      return i;
    if (system->voices[i].age < oldest_age) {
      oldest_age = system->voices[i].age;
      best = i;
    }
  }
  return best;
}

void vimana_system_play_tone(vimana_system *system, int pitch,
                             int duration_ms, int volume) {
  if (!system || !system->audio_stream)
    return;
  if (pitch < 0)
    return;

  if (duration_ms < 1)
    duration_ms = 1;
  else if (duration_ms > 2000)
    duration_ms = 2000;

  if (volume < 0)
    volume = 0;
  else if (volume > 15)
    volume = 15;

  int total_samples =
      (int)(((int64_t)system->audio_sample_rate * duration_ms) / 1000);
  if (total_samples < 1)
    total_samples = 1;

  float freq = vimana_pitch_to_hz(pitch);
  uint16_t freg = vimana_hz_to_freq_reg(freq);
  float amp = ((float)volume / 15.0f) * 0.25f;

  vimana_audio_lock(system);
  int ch = vimana_alloc_voice(system);
  VimanaVoice *v = &system->voices[ch];
  v->freq_reg = freg;
  v->freq_hz = vimana_freq_reg_to_hz(freg);
  v->amp = amp;
  v->phase = 0.0f;
  v->gate = true;
  v->env_stage = VIMANA_ENV_ATTACK;
  v->env_level = 0.0f;
  v->samples_left = total_samples;
  v->age = ++system->voice_clock;
  vimana_audio_unlock(system);
  if (!system->audio_locked)
    (void)SDL_ResumeAudioStreamDevice(system->audio_stream);
  else
    system->audio_needs_resume = true;
}

void vimana_system_set_voice(vimana_system *system, int channel,
                             int waveform) {
  if (!system || !system->audio_stream)
    return;
  if (channel < 0 || channel >= VIMANA_VOICE_COUNT)
    return;
  if (waveform < 0 || waveform > VIMANA_WAVE_PSG)
    waveform = VIMANA_WAVE_PULSE;
  vimana_audio_lock(system);
  system->voices[channel].waveform = waveform;
  vimana_audio_unlock(system);
}

void vimana_system_set_envelope(vimana_system *system, int channel,
                                int attack, int decay, int sustain,
                                int release) {
  if (!system || !system->audio_stream)
    return;
  if (channel < 0 || channel >= VIMANA_VOICE_COUNT)
    return;
  if (attack < 0) attack = 0; else if (attack > 15) attack = 15;
  if (decay < 0) decay = 0; else if (decay > 15) decay = 15;
  if (sustain < 0) sustain = 0; else if (sustain > 15) sustain = 15;
  if (release < 0) release = 0; else if (release > 15) release = 15;
  vimana_audio_lock(system);
  VimanaVoice *v = &system->voices[channel];
  v->adsr[0] = attack;
  v->adsr[1] = decay;
  v->adsr[2] = sustain;
  v->adsr[3] = release;
  vimana_audio_unlock(system);
}

void vimana_system_set_pulse_width(vimana_system *system, int channel,
                                   int width) {
  if (!system || !system->audio_stream)
    return;
  if (channel < 0 || channel >= VIMANA_VOICE_COUNT)
    return;
  if (width < 0) width = 0; else if (width > 255) width = 255;
  vimana_audio_lock(system);
  system->voices[channel].pulse_width = (float)width / 255.0f;
  vimana_audio_unlock(system);
}

void vimana_system_play_voice(vimana_system *system, int channel,
                              int pitch, int volume) {
  if (!system || !system->audio_stream)
    return;
  if (channel < 0 || channel >= VIMANA_VOICE_COUNT)
    return;
  if (pitch < 0)
    return;
  if (volume < 0) volume = 0; else if (volume > 15) volume = 15;

  float freq = vimana_pitch_to_hz(pitch);
  uint16_t freg = vimana_hz_to_freq_reg(freq);
  float amp = ((float)volume / 15.0f) * 0.25f;

  vimana_audio_lock(system);
  VimanaVoice *v = &system->voices[channel];
  v->freq_reg = freg;
  v->freq_hz = vimana_freq_reg_to_hz(freg);
  v->amp = amp;
  v->phase = 0.0f;
  v->gate = true;
  v->env_stage = VIMANA_ENV_ATTACK;
  v->env_level = 0.0f;
  v->samples_left = 0; /* no auto-release; use stop_voice */
  v->age = ++system->voice_clock;
  vimana_audio_unlock(system);
  if (!system->audio_locked)
    (void)SDL_ResumeAudioStreamDevice(system->audio_stream);
  else
    system->audio_needs_resume = true;
}

void vimana_system_stop_voice(vimana_system *system, int channel) {
  if (!system || !system->audio_stream)
    return;
  if (channel < 0 || channel >= VIMANA_VOICE_COUNT)
    return;
  vimana_audio_lock(system);
  VimanaVoice *v = &system->voices[channel];
  if (v->gate) {
    v->gate = false;
    v->env_stage = VIMANA_ENV_RELEASE;
  }
  vimana_audio_unlock(system);
}

void vimana_system_set_frequency(vimana_system *system, int channel,
                                 int freq16) {
  if (!system || !system->audio_stream)
    return;
  if (channel < 0 || channel >= VIMANA_VOICE_COUNT)
    return;
  if (freq16 < 0) freq16 = 0; else if (freq16 > 65535) freq16 = 65535;
  vimana_audio_lock(system);
  VimanaVoice *v = &system->voices[channel];
  v->freq_reg = (uint16_t)freq16;
  v->freq_hz = vimana_freq_reg_to_hz(v->freq_reg);
  vimana_audio_unlock(system);
}

void vimana_system_set_sync(vimana_system *system, int channel, int enable) {
  if (!system || !system->audio_stream)
    return;
  if (!((channel >= 0 && channel < 3) || (channel >= 4 && channel < 7)))
    return; /* sync for tone voices 0–2 and 4–6 */
  vimana_audio_lock(system);
  system->voices[channel].sync = (enable != 0);
  vimana_audio_unlock(system);
}

void vimana_system_set_ring_mod(vimana_system *system, int channel,
                                int enable) {
  if (!system || !system->audio_stream)
    return;
  if (!((channel >= 0 && channel < 3) || (channel >= 4 && channel < 7)))
    return; /* ring mod for tone voices 0–2 and 4–6 */
  vimana_audio_lock(system);
  system->voices[channel].ring_mod = (enable != 0);
  vimana_audio_unlock(system);
}

void vimana_system_set_filter(vimana_system *system, int cutoff,
                              int resonance, int mode) {
  if (!system || !system->audio_stream)
    return;
  if (cutoff < 0) cutoff = 0; else if (cutoff > 2047) cutoff = 2047;
  if (resonance < 0) resonance = 0; else if (resonance > 15) resonance = 15;
  mode &= (VIMANA_FILT_LP | VIMANA_FILT_BP | VIMANA_FILT_HP);
  vimana_audio_lock(system);
  system->filter.cutoff = cutoff;
  system->filter.resonance = resonance;
  system->filter.mode = mode;
  vimana_audio_unlock(system);
}

void vimana_system_set_filter_route(vimana_system *system, int channel,
                                    int enable) {
  if (!system || !system->audio_stream)
    return;
  if (channel < 0 || channel >= VIMANA_VOICE_COUNT)
    return;
  vimana_audio_lock(system);
  system->filter.route[channel] = (enable != 0);
  vimana_audio_unlock(system);
}

void vimana_system_set_master_volume(vimana_system *system, int volume) {
  if (!system || !system->audio_stream)
    return;
  if (volume < 0) volume = 0; else if (volume > 15) volume = 15;
  vimana_audio_lock(system);
  system->master_volume = volume;
  vimana_audio_unlock(system);
}

void vimana_system_set_paddle(vimana_system *system, int paddle, int value) {
  if (!system)
    return;
  if (paddle < 0 || paddle >= VIMANA_PADDLE_COUNT)
    return;
  if (value < 0) value = 0; else if (value > 255) value = 255;
  system->paddle[paddle] = (uint8_t)value;
}

int vimana_system_get_paddle(vimana_system *system, int paddle) {
  if (!system)
    return 0;
  if (paddle < 0 || paddle >= VIMANA_PADDLE_COUNT)
    return 0;
  return (int)system->paddle[paddle];
}

void vimana_system_run(vimana_system *system, vimana_screen *screen,
                       vimana_frame_fn frame, void *user) {
  if (!system || !screen || !screen->window || !screen->renderer)
    return;
  system->quit = false;
  system->running = true;
  screen->system = system;
  SDL_StartTextInput(screen->window);
  const uint64_t freq = SDL_GetPerformanceFrequency();
  const uint64_t frame_ns = freq / 60; /* 60 fps cap */
  uint64_t last = SDL_GetPerformanceCounter();
  while (!system->quit) {
    vimana_reset_pressed(system);
    vimana_pump_events(system, screen);
    vimana_screen_poll_theme(screen);
    if (frame)
      frame(system, screen, user);
    /* Cap at 60 fps: sleep until the next frame boundary */
    uint64_t now = SDL_GetPerformanceCounter();
    uint64_t elapsed = now - last;
    if (elapsed < frame_ns) {
      uint64_t remain = frame_ns - elapsed;
      uint32_t ms = (uint32_t)((remain * 1000) / freq);
      if (ms > 0)
        SDL_Delay(ms);
      /* Spin the remainder for precision */
      while (SDL_GetPerformanceCounter() - last < frame_ns)
        ;
    }
    last = SDL_GetPerformanceCounter();
  }
  SDL_StopTextInput(screen->window);
  system->running = false;
}

static SDL_HitTestResult vimana_hit_test(SDL_Window *win,
                                        const SDL_Point *area,
                                        void *data) {
  (void)win;
  vimana_screen *screen = (vimana_screen *)data;
  if (area->y < screen->titlebar_height) {
    int x = area->x;
    int y = area->y;
    /* Close box */
    if (y >= screen->titlebar_box_y && y < screen->titlebar_box_y + screen->titlebar_box_size &&
        x >= VIMANA_TB_CLOSE_X && x < VIMANA_TB_CLOSE_X + screen->titlebar_box_size)
      return SDL_HITTEST_NORMAL;
    /* Optional right button */
    if (screen && screen->titlebar_has_button) {
      int win_w = screen->width * screen->scale;
      int bx = win_w - VIMANA_TB_CLOSE_X - screen->titlebar_box_size;
      if (y >= screen->titlebar_box_y && y < screen->titlebar_box_y + screen->titlebar_box_size &&
          x >= bx && x < bx + screen->titlebar_box_size)
        return SDL_HITTEST_NORMAL;
    }
    return SDL_HITTEST_DRAGGABLE;
  }
  return SDL_HITTEST_NORMAL;
}

vimana_screen *vimana_screen_new(const char *title, unsigned int width, unsigned int height,
                                 unsigned int scale) {
  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;
  if (scale < 1)
    scale = 1;

  vimana_screen *screen = (vimana_screen *)calloc(1, sizeof(vimana_screen));
  if (!screen)
    return NULL;

  /* Allocate font ROM (always needed). GFX ROM is lazy-allocated on first use. */
  screen->font_rom = (uint8_t *)calloc(VIMANA_FONT_SIZE, 1);
  if (!screen->font_rom) {
    free(screen);
    return NULL;
  }
  /* Sprite banks are lazy-allocated on first use (calloc'd in vimana_ensure_sprite_bank). */
  /* GFX ROM is lazy-allocated on first set_gfx call (calloc'd in vimana_ensure_gfx_rom). */

  screen->width = width;
  screen->height = height;
  screen->scale = scale;
  screen->width_mar = width + 16;
  screen->height_mar = height + 16;
  screen->title = strdup(title ? title : "vimana");
  screen->titlebar_bg_slot = 2; /* default to palette slot 2 (accent color) */
  screen->titlebar_dirty = true;
  screen->cursor_visible = false; /* cursor hidden by default */
  screen->cursor_dirty = false;
  screen->system = NULL;
  SDL_ShowCursor(); /* show system cursor by default; apps set custom cursor via set_cursor() */
  vimana_screen_reset_palette(screen);
  vimana_screen_load_theme(screen);
  vimana_screen_reset_font(screen);
  vimana_screen_reset_ports(screen);

  screen->layers =
      (uint8_t *)calloc((size_t)screen->width_mar * (size_t)screen->height_mar,
                        sizeof(uint8_t));
  if (!screen->layers) {
    free(screen->font_rom);
    free(screen->title);
    free(screen);
    return NULL;
  }

  screen->window =
      SDL_CreateWindow(screen->title, width * scale,
                       height * scale + screen->titlebar_height,
                       SDL_WINDOW_BORDERLESS);
  if (!screen->window) {
    free(screen->layers);
    free(screen->font_rom);
    free(screen->title);
    free(screen);
    return NULL;
  }

  /* Reduce Metal memory footprint: prefer integrated GPU, enable vsync */
  SDL_SetHint(SDL_HINT_RENDER_METAL_PREFER_LOW_POWER_DEVICE, "1");
  SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

  screen->renderer = SDL_CreateRenderer(screen->window, NULL);
  if (!screen->renderer) {
    SDL_DestroyWindow(screen->window);
    free(screen->layers);
    free(screen->font_rom);
    free(screen->title);
    free(screen);
    return NULL;
  }

#ifdef __APPLE__
  /* Reduce Metal drawable pool from 3 to 2 to save one full back buffer. */
  {
    void *layer = SDL_GetRenderMetalLayer(screen->renderer);
    if (layer) {
      typedef void (*send_uint_t)(void *, SEL, unsigned long);
      SEL sel = sel_registerName("setMaximumDrawableCount:");
      ((send_uint_t)objc_msgSend)(layer, sel, 2);
    }
  }
#endif

  screen->texture =
      SDL_CreateTexture(screen->renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!screen->texture) {
    SDL_DestroyRenderer(screen->renderer);
    SDL_DestroyWindow(screen->window);
    free(screen->layers);
    free(screen->font_rom);
    free(screen->title);
    free(screen);
    return NULL;
  }
  SDL_SetTextureScaleMode(screen->texture, SDL_SCALEMODE_PIXELART);
  SDL_SetWindowHitTest(screen->window, vimana_hit_test, screen);

  vimana_screen_clear(screen, 0);
  return screen;
}

void vimana_screen_clear(vimana_screen *screen, unsigned int bg) {
  if (!screen || !screen->layers)
    return;
  uint8_t bg_val = (uint8_t)(bg & 0xF);
  size_t total =
      (size_t)screen->width_mar * (size_t)screen->height_mar;
  memset(screen->layers, (int)bg_val, total);
}

void vimana_screen_resize(vimana_screen *screen, unsigned int width, unsigned int height) {
  if (!screen)
    return;
  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  unsigned int width_mar = width + 16;
  unsigned int height_mar = height + 16;

  uint8_t *layers = (uint8_t *)calloc(
      (size_t)width_mar * (size_t)height_mar, sizeof(uint8_t));
  if (!layers) {
    return;
  }

  free(screen->layers);
  screen->layers = layers;
  screen->width = width;
  screen->height = height;
  screen->width_mar = width_mar;
  screen->height_mar = height_mar;

  if (screen->window)
    SDL_SetWindowSize(screen->window, width * screen->scale,
                      height * screen->scale + screen->titlebar_height);

  if (screen->texture) {
    SDL_DestroyTexture(screen->texture);
    screen->texture =
        SDL_CreateTexture(screen->renderer, SDL_PIXELFORMAT_ARGB8888,
                          SDL_TEXTUREACCESS_STREAMING, width, height);
    if (screen->texture)
      SDL_SetTextureScaleMode(screen->texture, SDL_SCALEMODE_PIXELART);
  }
}

void vimana_screen_set_palette(vimana_screen *screen, unsigned int slot,
                               const char *hex) {
  if (!screen)
    return;
  unsigned int s = slot & 15;
  screen->base_colors[s] = vimana_parse_hex_color(hex);
  vimana_colorize(screen);
  screen->titlebar_dirty = true;
}

void vimana_screen_set_font_glyph(vimana_screen *screen, unsigned int code,
                                  const uint16_t rows[16]) {
  if (!screen || !rows || !screen->font_rom)
    return;
  if (code >= VIMANA_GLYPH_COUNT)
    return;
  /* UF2 tileset storage */
  vimana_rows_to_uf2(rows, vimana_rom_font_uf2(screen, code));
  /* 1bpp bitmap: OR both planes together */
  uint8_t *bmp = vimana_rom_font_bitmap(screen, code);
  memset(bmp, 0, VIMANA_FONT_GLYPH_BYTES);
  for (int r = 0; r < 16; r++) {
    uint8_t bits = 0;
    uint16_t packed = rows[r];
    for (int col = 0; col < 8; col++) {
      uint16_t pair = (uint16_t)((packed >> ((7 - col) * 2)) & 0x3u);
      if (pair != 0)
        bits = (uint8_t)(bits | (0x80u >> col));
    }
    bmp[r * VIMANA_FONT_ROW_BYTES] = bits;
  }
}

void vimana_screen_set_font_chr(vimana_screen *screen, unsigned int code,
                                const uint8_t *chr, unsigned int len) {
  if (!screen || !chr || !screen->font_rom)
    return;
  if (code >= VIMANA_GLYPH_COUNT)
    return;
  unsigned int gw = screen->font_glyph_width;
  unsigned int gh = screen->font_height;
  /* Infer bytes-per-row from data length to support wide glyphs (e.g. 12px) */
  unsigned int bpr = gh > 0 ? (len + gh - 1) / gh : (gw + 7) / 8;
  if (bpr < 1) bpr = 1;
  if (bpr > VIMANA_FONT_ROW_BYTES) bpr = VIMANA_FONT_ROW_BYTES;
  unsigned int max_bytes = gh * bpr;
  unsigned int n = len < max_bytes ? len : max_bytes;
  /* 1bpp bitmap */
  uint8_t *bmp = vimana_rom_font_bitmap(screen, code);
  memset(bmp, 0, VIMANA_FONT_GLYPH_BYTES);
  for (unsigned int i = 0; i < n; i++) {
    unsigned int row = i / bpr;
    unsigned int col_byte = i % bpr;
    bmp[row * VIMANA_FONT_ROW_BYTES + col_byte] = chr[i];
  }
  /* UF2 tileset storage (for 8px-wide glyphs only) */
  if (gw <= 8) {
    uint8_t *uf2 = vimana_rom_font_uf2(screen, code);
    memset(uf2, 0, VIMANA_UF2_BYTES);
    unsigned int rows = n < VIMANA_GLYPH_HEIGHT ? n : VIMANA_GLYPH_HEIGHT;
    for (unsigned int r = 0; r < rows; r++) {
      unsigned int tile = r / VIMANA_TILE_SIZE;
      unsigned int tr = r % VIMANA_TILE_SIZE;
      uf2[tile * 16 + tr] = chr[r]; /* plane0 */
    }
  }
}

void vimana_screen_set_font_width(vimana_screen *screen, unsigned int code,
                                  unsigned int width) {
  if (!screen || !screen->font_rom || code >= VIMANA_GLYPH_COUNT)
    return;
  vimana_rom_font_widths(screen)[code] =
      (uint8_t)(width > 0 && width <= 255 ? width : VIMANA_TILE_SIZE);
}

void vimana_screen_set_font_size(vimana_screen *screen, unsigned int size) {
  if (!screen)
    return;
  int w;
  if (size == 3) {
    screen->font_height = VIMANA_FONT_MAX_HEIGHT;    /* 24 */
    w = VIMANA_TILE_SIZE * 2;                        /* 16 */
  } else if (size == 2) {
    screen->font_height = VIMANA_GLYPH_HEIGHT;       /* 16 */
    w = VIMANA_TILE_SIZE;                            /*  8 */
  } else {
    screen->font_height = VIMANA_TILE_SIZE;          /*  8 */
    w = VIMANA_TILE_SIZE;                            /*  8 */
  }
  screen->font_glyph_width = w;
  uint8_t *widths = vimana_rom_font_widths(screen);
  for (unsigned int i = 0; i < VIMANA_GLYPH_COUNT; i++)
    widths[i] = (uint8_t)w;
  int old_th = screen->titlebar_height;
  vimana_screen_update_titlebar_sizes(screen);
  if (screen->window && screen->titlebar_height != old_th)
    SDL_SetWindowSize(screen->window, screen->width * screen->scale,
                      screen->height * screen->scale + screen->titlebar_height);
}

void vimana_screen_set_sprite(vimana_screen *screen, unsigned int addr,
                              const uint8_t *sprite, unsigned int mode) {
  if (!screen || !sprite)
    return;
  vimana_screen_store_sprite_bytes(
      screen, vimana_screen_addr_clamp(screen, addr), sprite, mode);
}

void vimana_screen_set_x(vimana_screen *screen, unsigned int x) {
  if (!screen)
    return;
  screen->port_x = (uint16_t)x;
}

void vimana_screen_set_y(vimana_screen *screen, unsigned int y) {
  if (!screen)
    return;
  screen->port_y = (uint16_t)y;
}

void vimana_screen_set_addr(vimana_screen *screen, unsigned int addr) {
  if (!screen)
    return;
  screen->port_addr = (uint16_t)vimana_screen_addr_clamp(screen, addr);
}

void vimana_screen_set_auto(vimana_screen *screen, unsigned int auto_flags) {
  if (!screen)
    return;
  screen->port_auto = (uint8_t)(auto_flags & 0xFF);
}

unsigned int vimana_screen_x(vimana_screen *screen) {
  return screen ? screen->port_x : 0;
}

unsigned int vimana_screen_y(vimana_screen *screen) {
  return screen ? screen->port_y : 0;
}

unsigned int vimana_screen_addr(vimana_screen *screen) {
  return screen ? screen->port_addr : 0;
}

unsigned int vimana_screen_auto(vimana_screen *screen) {
  return screen ? screen->port_auto : 0;
}

void vimana_screen_set_sprite_bank(vimana_screen *screen, unsigned int bank) {
  if (!screen)
    return;
  screen->active_sprite_bank = (uint8_t)(bank % VIMANA_SPRITE_BANK_COUNT);
}

unsigned int vimana_screen_sprite_bank(vimana_screen *screen) {
  return screen ? screen->active_sprite_bank : 0;
}

void vimana_screen_set_gfx(vimana_screen *screen, unsigned int addr,
                            const uint8_t *data, unsigned int len) {
  if (!screen || !data)
    return;
  /* Upload to sprite bank 0 (for use with sprite() command) */
  uint8_t *bank = vimana_ensure_sprite_bank(screen, 0);
  if (!bank)
    return;
  if (addr + len > VIMANA_SPRITE_BANK_SIZE)
    len = VIMANA_SPRITE_BANK_SIZE - addr;
  memcpy(bank + addr, data, (size_t)len);
}

const uint8_t *vimana_screen_gfx(vimana_screen *screen, unsigned int addr) {
  if (!screen || !screen->gfx_rom || addr >= VIMANA_GFX_SIZE)
    return NULL;
  return screen->gfx_rom + addr;
}

void vimana_screen_sprite(vimana_screen *screen, unsigned int ctrl) {
  if (!screen || !screen->layers)
    return;
  const uint8_t *bank = vimana_active_sprite_bank(screen);
  if (!bank)
    return;
  const unsigned int rMX = screen->port_auto & 0x1;
  const unsigned int rMY = screen->port_auto & 0x2;
  const unsigned int rMA = screen->port_auto & 0x4;
  const unsigned int rML = vimana_screen_auto_repeat(screen) - 1;
  const unsigned int rDX = rMX << 3;
  const unsigned int rDY = rMY << 2;
  const unsigned int flipx = ctrl & 0x10;
  const unsigned int flipy = ctrl & 0x20;
  const int dx = flipx ? -(int)rDY : (int)rDY;
  const int dy = flipy ? -(int)rDX : (int)rDX;
  const unsigned int row_start = flipx ? 0 : 7;
  const int row_delta = flipx ? 1 : -1;
  const unsigned int col_start = flipy ? 7 : 0;
  const int col_delta = flipy ? -1 : 1;
  const unsigned int layer = ctrl & 0x40;
  const unsigned int layer_mask = layer ? 0x0F : 0xF0;
  const unsigned int is_2bpp = ctrl & 0x80;
  const unsigned int is_3bpp = ctrl & 0x100;
  const unsigned int is_4bpp = ctrl & 0x200;
  const unsigned int bytes_per_sprite = is_4bpp ? 32u : (is_3bpp ? 24u : (is_2bpp ? 16u : 8u));
  const unsigned int addr_incr = (rMA >> 2) * bytes_per_sprite;
  const unsigned int stride = (unsigned int)screen->width_mar;
  const unsigned int blend = ctrl & 0xf;
  const uint8_t opaque_mask = (uint8_t)(blend % 5);
  const uint8_t *table = blend_lut[blend][(layer != 0) ? 1 : 0];
  unsigned int x = screen->port_x * 8;  /* tile → pixel */
  unsigned int y = screen->port_y * 8;  /* tile → pixel */
  unsigned int rA = screen->port_addr;

  for (unsigned int i = 0; i <= rML; i++, x += dx, y += dy, rA += addr_incr) {
    const unsigned int x0 = x + 8;
    const unsigned int y0 = y + 8;
    if (x0 + 8 > stride || y0 + 8 > (unsigned int)screen->height_mar)
      continue;
    uint8_t *dst_row = screen->layers + y0 * stride + x0;
    for (unsigned int j = 0; j < 8; j++, dst_row += stride) {
      const unsigned int sr = col_start + j * col_delta;
      const unsigned int a1 = rA + sr;
      const unsigned int a2 = rA + sr + 8;
      const uint8_t ch1 =
          (a1 < VIMANA_SPRITE_BANK_SIZE) ? bank[a1] : 0;
      const uint8_t ch2 =
          ((is_2bpp || is_3bpp || is_4bpp) && a2 < VIMANA_SPRITE_BANK_SIZE)
              ? bank[a2]
              : 0;
      const unsigned int a3 = rA + sr + 16;
      const uint8_t ch3 =
          ((is_3bpp || is_4bpp) && a3 < VIMANA_SPRITE_BANK_SIZE) ? bank[a3] : 0;
      const unsigned int a4 = rA + sr + 24;
      const uint8_t ch4 =
          (is_4bpp && a4 < VIMANA_SPRITE_BANK_SIZE) ? bank[a4] : 0;
      uint8_t *d = dst_row;
      for (unsigned int k = 0, row = row_start; k < 8;
           k++, d++, row += row_delta) {
        const unsigned int bit = 1u << row;
        const uint8_t color =
            (uint8_t)((!!(ch1 & bit)) | ((!!(ch2 & bit)) << 1) |
                      ((!!(ch3 & bit)) << 2) | ((!!(ch4 & bit)) << 3));
        if (opaque_mask || color)
          *d = (*d & (uint8_t)layer_mask) | table[color];
      }
    }
  }

  screen->port_addr = (uint16_t)rA;
  if (rMX)
    screen->port_x += flipx ? -1 : 1;  /* ±1 tile */
  if (rMY)
    screen->port_y += flipy ? -1 : 1;  /* ±1 tile */
}

void vimana_screen_pixel(vimana_screen *screen, unsigned int ctrl) {
  if (!screen || !screen->layers)
    return;
  const unsigned int layer = ctrl & 0x40;
  const uint8_t layer_mask = (uint8_t)(layer ? 0x0F : 0xF0);
  const uint8_t color =
      layer ? (uint8_t)((ctrl & 0xF) << 4) : (uint8_t)(ctrl & 0xF);
  if (ctrl & 0x80) {
    /* fill rectangle */
    unsigned int x1, x2, y1, y2;
    if (ctrl & 0x10) {
      x1 = 0;
      x2 = screen->port_x;
    } else {
      x1 = screen->port_x;
      x2 = (unsigned int)screen->width;
    }
    if (ctrl & 0x20) {
      y1 = 0;
      y2 = screen->port_y;
    } else {
      y1 = screen->port_y;
      y2 = (unsigned int)screen->height;
    }
    if (x2 > (unsigned int)screen->width)
      x2 = (unsigned int)screen->width;
    if (y2 > (unsigned int)screen->height)
      y2 = (unsigned int)screen->height;
    for (unsigned int fy = y1; fy < y2; fy++) {
      uint8_t *row =
          screen->layers + (fy + 8) * screen->width_mar + 8;
      for (unsigned int fx = x1; fx < x2; fx++)
        row[fx] = (row[fx] & layer_mask) | color;
    }
  } else {
    /* single pixel */
    unsigned int px = screen->port_x;
    unsigned int py = screen->port_y;
    if (px < (unsigned int)screen->width &&
        py < (unsigned int)screen->height) {
      unsigned int idx = (py + 8) * (unsigned int)screen->width_mar + (px + 8);
      screen->layers[idx] = (screen->layers[idx] & layer_mask) | color;
    }
    if (screen->port_auto & 0x01)
      screen->port_x++;
    if (screen->port_auto & 0x02)
      screen->port_y++;
  }
}

void vimana_screen_put(vimana_screen *screen, unsigned int x, unsigned int y,
                       const char *glyph, unsigned int fg, unsigned int bg) {
  if (!screen || !screen->layers || !screen->font_rom)
    return;
  unsigned int ch = (unsigned char)((glyph && glyph[0]) ? glyph[0] : ' ');
  if (ch >= VIMANA_GLYPH_COUNT)
    ch = ' ';
  const uint8_t *bmp = vimana_rom_font_bitmap(screen, ch);
  const uint8_t *widths = vimana_rom_font_widths(screen);
  unsigned int bg_slot = bg & 0xF;
  unsigned int fg_slot = fg & 0xF;
  unsigned int gh = screen->font_height;
  unsigned int gw = widths[ch];
  if (gw == 0) gw = screen->font_glyph_width;
  for (unsigned int row = 0; row < gh; row++) {
    unsigned int py = y + row;
    if (py >= (unsigned int)screen->height)
      continue;
    const uint8_t *row_data = bmp + row * VIMANA_FONT_ROW_BYTES;
    uint8_t *dst =
        screen->layers + (py + 8) * screen->width_mar + (x + 8);
    for (unsigned int col = 0; col < gw; col++) {
      unsigned int px = x + col;
      if (px >= (unsigned int)screen->width) {
        dst++;
        continue;
      }
      uint8_t mask = (uint8_t)(0x80u >> (col & 7));
      uint8_t hit = (row_data[col >> 3] & mask) ? 1u : 0u;
      uint8_t slot = hit ? (uint8_t)fg_slot : (uint8_t)bg_slot;
      *dst++ = (uint8_t)((slot << 4) | slot);
    }
  }
}

void vimana_screen_put_icn(vimana_screen *screen, unsigned int x,
                           unsigned int y, const uint8_t rows[8],
                           unsigned int fg, unsigned int bg) {
  if (!screen || !rows || !screen->layers)
    return;
  x *= 8;  /* tile → pixel */
  y *= 8;  /* tile → pixel */
  unsigned int bg_slot = bg & 0xF;
  unsigned int fg_slot = fg & 0xF;
  for (unsigned int row = 0; row < VIMANA_TILE_SIZE; row++) {
    unsigned int py = y + row;
    if (py >= (unsigned int)screen->height)
      continue;
    const uint8_t plane0 = rows[row];
    uint8_t *dst =
        screen->layers + (py + 8) * screen->width_mar + (x + 8);
    for (unsigned int col = 0; col < VIMANA_TILE_SIZE; col++) {
      unsigned int px = x + col;
      if (px >= (unsigned int)screen->width) {
        dst++;
        continue;
      }
      uint8_t mask = (uint8_t)(0x80u >> col);
      uint8_t hit = (plane0 & mask) ? 1u : 0u;
      uint8_t slot = hit ? (uint8_t)fg_slot : (uint8_t)bg_slot;
      *dst++ = (uint8_t)((slot << 4) | slot);
    }
  }
}

void vimana_screen_put_text(vimana_screen *screen, unsigned int x,
                            unsigned int y, const char *text,
                            unsigned int fg, unsigned int bg) {
  if (!screen || !text || !screen->font_rom)
    return;
  const uint8_t *widths = vimana_rom_font_widths(screen);
  unsigned int cx = x;
  for (int i = 0; text[i] != 0; i++) {
    unsigned int ch = (unsigned char)text[i];
    if (ch >= VIMANA_GLYPH_COUNT) ch = ' ';
    vimana_screen_put(screen, cx, y, &text[i], fg, bg);
    cx += widths[ch];
  }
}

static void vimana_rebuild_titlebar_tex(vimana_screen *screen) {
  if (!screen || !screen->renderer)
    return;
  int win_w = screen->width * screen->scale;
  int bar_h = screen->titlebar_height;
  if (win_w <= 0 || bar_h <= 0)
    return;

  /* Skip rendering entirely if no title */
  const char *title = screen->titlebar_title ? screen->titlebar_title : screen->title;
  if (!title || !title[0])
    return;

  uint32_t bg = screen->base_colors[screen->titlebar_bg_slot & 7];
  uint32_t contrast = screen->base_colors[1];

  /* Allocate ARGB pixel buffer for the titlebar */
  uint32_t *pixels = (uint32_t *)malloc((size_t)win_w * (size_t)bar_h * sizeof(uint32_t));
  if (!pixels)
    return;

  /* Helper macros for pixel buffer access */
  #define TB_SET(px, py, col) do { \
    if ((px) >= 0 && (px) < win_w && (py) >= 0 && (py) < bar_h) \
      pixels[(py) * win_w + (px)] = (col); \
  } while(0)
  #define TB_FILL(rx, ry, rw, rh, col) do { \
    for (int _fy = (ry); _fy < (ry) + (rh); _fy++) \
      for (int _fx = (rx); _fx < (rx) + (rw); _fx++) \
        TB_SET(_fx, _fy, col); \
  } while(0)

  /* Fill background */
  for (int i = 0; i < win_w * bar_h; i++)
    pixels[i] = bg;

  /* Pinstripe pattern */
  int stripe_top = 3;
  int stripe_bot = screen->titlebar_bar_height - 3;
  for (int row = stripe_top; row < stripe_bot; row++) {
    if (row & 1) {
      for (int col = 2; col < win_w - 2; col++)
        TB_SET(col, row, contrast);
    }
  }

  /* Close box (left side, System 6 style) */
  {
    int bx = VIMANA_TB_CLOSE_X, by = screen->titlebar_box_y, bsz = screen->titlebar_box_size;
    int halo_y = (by - 3 < stripe_top) ? by - 3 : stripe_top;
    if (halo_y < 0) halo_y = 0;
    int halo_bot = (by + bsz + 3 > stripe_bot) ? by + bsz + 3 : stripe_bot;
    if (halo_bot > screen->titlebar_bar_height) halo_bot = screen->titlebar_bar_height;
    TB_FILL(bx - 1, halo_y, bsz + 2, halo_bot - halo_y, bg);
    /* 1px border */
    TB_FILL(bx, by, bsz, 1, contrast);
    TB_FILL(bx, by + bsz - 1, bsz, 1, contrast);
    TB_FILL(bx, by, 1, bsz, contrast);
    TB_FILL(bx + bsz - 1, by, 1, bsz, contrast);
  }

  /* Optional right button */
  if (screen->titlebar_has_button) {
    int bx = win_w - VIMANA_TB_CLOSE_X - screen->titlebar_box_size;
    int by = screen->titlebar_box_y, bsz = screen->titlebar_box_size;
    int bhalo_y = (by - 3 < stripe_top) ? by - 3 : stripe_top;
    if (bhalo_y < 0) bhalo_y = 0;
    int bhalo_bot = (by + bsz + 3 > stripe_bot) ? by + bsz + 3 : stripe_bot;
    if (bhalo_bot > screen->titlebar_bar_height) bhalo_bot = screen->titlebar_bar_height;
    TB_FILL(bx - 1, bhalo_y, bsz + 2, bhalo_bot - bhalo_y, bg);
    /* Outer border */
    TB_FILL(bx, by, bsz, 1, contrast);
    TB_FILL(bx, by + bsz - 1, bsz, 1, contrast);
    TB_FILL(bx, by, 1, bsz, contrast);
    TB_FILL(bx + bsz - 1, by, 1, bsz, contrast);
    /* Inner offset box */
    int ix = bx + 4, iy = by + 2, isz = bsz - 6;
    TB_FILL(ix, iy, isz, 1, contrast);
    TB_FILL(ix, iy + isz - 1, isz, 1, contrast);
    TB_FILL(ix, iy, 1, isz, contrast);
    TB_FILL(ix + isz - 1, iy, 1, isz, contrast);
  }

  /* Title text: centered, using font ROM glyphs */
  if (title && title[0]) {
    int len = (int)strlen(title);
    const uint8_t *widths = vimana_rom_font_widths(screen);
    int text_w = 0;
    for (int i = 0; i < len; i++) {
      unsigned int ch = (unsigned char)title[i];
      text_w += (ch < VIMANA_GLYPH_COUNT) ? widths[ch] : VIMANA_TILE_SIZE;
    }
    int left_bound  = VIMANA_TB_CLOSE_X + screen->titlebar_box_size + 3;
    int right_bound = screen->titlebar_has_button
                        ? win_w - VIMANA_TB_CLOSE_X - screen->titlebar_box_size - 3
                        : win_w - 3;
    int gh = screen->font_height;
    int text_y = gh < 16 ? 4 : 1; // 1 from top if UF2/3, else 4 if UF1.
    int text_x = (win_w - text_w) / 2;
    text_x -= 1; /* optical nudge */
    if (text_x < left_bound)
      text_x = left_bound;
    if (text_x + text_w > right_bound)
      text_x = right_bound - text_w;
    if (text_x < left_bound)
      text_x = left_bound;
    /* Clear halo behind title text */
    int clipped_w = text_w;
    if (text_x + clipped_w > right_bound)
      clipped_w = right_bound - text_x;
    if (clipped_w > 0)
      TB_FILL(text_x - 3, 0, clipped_w + 6, screen->titlebar_bar_height, bg);
    /* Render glyphs from font ROM */
    int gx = text_x;
    for (int i = 0; i < len; i++) {
      unsigned int c = (unsigned char)title[i];
      if (c >= VIMANA_GLYPH_COUNT) { gx += widths[' ']; continue; }
      if (gx >= right_bound) break;
      const uint8_t *bmp = vimana_rom_font_bitmap(screen, c);
      int gw = (int)widths[c];
      if (gw < screen->font_glyph_width) gw = screen->font_glyph_width;
      for (int row = 0; row < gh; row++) {
        int py = text_y + row;
        if (py < 0 || py >= screen->titlebar_bar_height) continue;
        const uint8_t *row_data = bmp + row * VIMANA_FONT_ROW_BYTES;
        for (int col = 0; col < gw; col++) {
          uint8_t mask = (uint8_t)(0x80u >> (col & 7));
          if (row_data[col >> 3] & mask)
            TB_SET(gx + col, py, contrast);
        }
      }
      gx += widths[c];
    }
  }

  /* Separator line at bottom */
  for (int x = 0; x < win_w; x++)
    TB_SET(x, bar_h - 1, contrast);

  #undef TB_SET
  #undef TB_FILL

  /* Upload to SDL_Texture */
  if (screen->titlebar_tex)
    SDL_DestroyTexture(screen->titlebar_tex);
  screen->titlebar_tex = SDL_CreateTexture(screen->renderer,
      SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, win_w, bar_h);
  if (screen->titlebar_tex) {
    SDL_UpdateTexture(screen->titlebar_tex, NULL, pixels,
                      win_w * (int)sizeof(uint32_t));
    SDL_SetTextureScaleMode(screen->titlebar_tex, SDL_SCALEMODE_PIXELART);
  }
  free(pixels);
  screen->titlebar_dirty = false;
}

static void vimana_draw_titlebar_content(vimana_screen *screen) {
  if (!screen || !screen->renderer)
    return;
  if (screen->titlebar_dirty || !screen->titlebar_tex)
    vimana_rebuild_titlebar_tex(screen);
  if (screen->titlebar_tex) {
    int win_w = screen->width * screen->scale;
    SDL_FRect dst = {0.0f, 0.0f, (float)win_w, (float)screen->titlebar_height};
    SDL_RenderTexture(screen->renderer, screen->titlebar_tex, NULL, &dst);
  }
}

void vimana_screen_present(vimana_screen *screen) {
  if (!screen || !screen->renderer || !screen->layers ||
      !screen->texture)
    return;

  int w = screen->width;
  int h = screen->height;
  int wm = screen->width_mar;

  void *tex_pixels = NULL;
  int tex_pitch = 0;
  if (!SDL_LockTexture(screen->texture, NULL, &tex_pixels, &tex_pitch))
    return;

  for (int py = 0; py < h; py++) {
    const uint8_t *src = screen->layers + (py + 8) * wm + 8;
    uint32_t *dst = (uint32_t *)((uint8_t *)tex_pixels + py * tex_pitch);
    for (int px = 0; px < w; px++)
      dst[px] = screen->palette[src[px]];
  }

  SDL_UnlockTexture(screen->texture);
  uint32_t bg_rgb = screen->base_colors[0];
  SDL_SetRenderDrawColor(screen->renderer,
                         (uint8_t)((bg_rgb >> 16) & 0xFF),
                         (uint8_t)((bg_rgb >> 8) & 0xFF),
                         (uint8_t)(bg_rgb & 0xFF), 255);
  SDL_RenderClear(screen->renderer);
  SDL_FRect dst_rect = {0.0f, (float)screen->titlebar_height,
                        (float)(w * screen->scale), (float)(h * screen->scale)};
  SDL_RenderTexture(screen->renderer, screen->texture, NULL, &dst_rect);
  vimana_draw_titlebar_content(screen);

  /* 1px window border using stripe color */
  {
    uint32_t stripe = screen->base_colors[1];
    int win_w = w * screen->scale;
    int win_h = h * screen->scale + screen->titlebar_height;
    SDL_SetRenderDrawColor(screen->renderer,
                           (uint8_t)((stripe >> 16) & 0xFF),
                           (uint8_t)((stripe >> 8) & 0xFF),
                           (uint8_t)(stripe & 0xFF), 255);
    SDL_FRect bt = {0.0f, 0.0f, (float)win_w, 1.0f};
    SDL_FRect bb = {0.0f, (float)(win_h - 1), (float)win_w, 1.0f};
    SDL_FRect bl = {0.0f, 0.0f, 1.0f, (float)win_h};
    SDL_FRect br = {(float)(win_w - 1), 0.0f, 1.0f, (float)win_h};
    SDL_RenderFillRect(screen->renderer, &bt);
    SDL_RenderFillRect(screen->renderer, &bb);
    SDL_RenderFillRect(screen->renderer, &bl);
    SDL_RenderFillRect(screen->renderer, &br);
  }

  SDL_RenderPresent(screen->renderer);
}

void vimana_screen_draw_titlebar(vimana_screen *screen, unsigned int bg) {
  if (!screen)
    return;
  screen->titlebar_bg_slot = bg & 7;
  screen->titlebar_dirty = true;
}

void vimana_screen_set_titlebar_title(vimana_screen *screen, const char *title) {
  if (!screen)
    return;
  free(screen->titlebar_title);
  screen->titlebar_title = title ? strdup(title) : NULL;
  screen->titlebar_dirty = true;
}

void vimana_screen_set_titlebar_button(vimana_screen *screen, bool show) {
  if (!screen)
    return;
  screen->titlebar_has_button = show;
  screen->titlebar_dirty = true;
}

bool vimana_screen_titlebar_button_pressed(vimana_screen *screen) {
  return screen ? screen->titlebar_button_pressed : false;
}

unsigned int vimana_screen_width(vimana_screen *screen) {
  return screen ? screen->width : 0;
}

unsigned int vimana_screen_height(vimana_screen *screen) {
  return screen ? screen->height : 0;
}

unsigned int vimana_screen_scale(vimana_screen *screen) {
  return screen ? screen->scale : 0;
}

void vimana_screen_set_cursor(vimana_screen *screen, const uint8_t rows[8],
                              unsigned int fg, unsigned int bg) {
  if (!screen || !rows)
    return;
  memcpy(screen->cursor_icn, rows, 8);
  screen->cursor_fg = fg < 16 ? fg : 1;
  screen->cursor_bg = bg < 16 ? bg : 0;
  screen->cursor_dirty = true;
  if (!screen->cursor_visible) {
    screen->cursor_visible = true;
    SDL_ShowCursor();
  }
}

void vimana_screen_hide_cursor(vimana_screen *screen) {
  if (!screen)
    return;
  if (screen->cursor_visible) {
    screen->cursor_visible = false;
    SDL_HideCursor();
  }
}

void vimana_screen_show_cursor(vimana_screen *screen) {
  if (!screen)
    return;
  if (!screen->cursor_visible && screen->cursor_dirty) {
    screen->cursor_visible = true;
    SDL_ShowCursor();
  }
}

void vimana_device_poll(vimana_system *system) { (void)system; }

bool vimana_device_key_down(vimana_system *system, int scancode) {
  if (!system || scancode < 0 || scancode >= VIMANA_KEY_CAP)
    return false;
  return vimana_bit_get(system->key_down, scancode);
}

bool vimana_device_key_pressed(vimana_system *system, int scancode) {
  if (!system || scancode < 0 || scancode >= VIMANA_KEY_CAP)
    return false;
  return vimana_bit_get(system->key_pressed, scancode);
}

bool vimana_device_mouse_down(vimana_system *system, int button) {
  if (!system || button < 0 || button >= VIMANA_MOUSE_CAP)
    return false;
  return (system->mouse_down >> button) & 1;
}

bool vimana_device_mouse_pressed(vimana_system *system, int button) {
  if (!system || button < 0 || button >= VIMANA_MOUSE_CAP)
    return false;
  return (system->mouse_pressed >> button) & 1;
}

unsigned int vimana_device_pointer_x(vimana_system *system) {
  return system ? system->pointer_x : 0;
}

unsigned int vimana_device_pointer_y(vimana_system *system) {
  return system ? system->pointer_y : 0;
}

unsigned int vimana_device_tile_x(vimana_system *system) {
  return system ? system->tile_x : 0;
}

unsigned int vimana_device_tile_y(vimana_system *system) {
  return system ? system->tile_y : 0;
}

int vimana_device_wheel_x(vimana_system *system) {
  return system ? system->wheel_x : 0;
}

int vimana_device_wheel_y(vimana_system *system) {
  return system ? system->wheel_y : 0;
}

const char *vimana_device_text_input(vimana_system *system) {
  return system ? system->text_input : "";
}

int64_t vimana_datetime_now(vimana_system *system) {
  (void)system;
  return vimana_datetime_now_value();
}

int vimana_datetime_year(vimana_system *system) {
  return vimana_datetime_year_at(system, vimana_datetime_now_value());
}

int vimana_datetime_month(vimana_system *system) {
  return vimana_datetime_month_at(system, vimana_datetime_now_value());
}

int vimana_datetime_day(vimana_system *system) {
  return vimana_datetime_day_at(system, vimana_datetime_now_value());
}

int vimana_datetime_hour(vimana_system *system) {
  return vimana_datetime_hour_at(system, vimana_datetime_now_value());
}

int vimana_datetime_minute(vimana_system *system) {
  return vimana_datetime_minute_at(system, vimana_datetime_now_value());
}

int vimana_datetime_second(vimana_system *system) {
  return vimana_datetime_second_at(system, vimana_datetime_now_value());
}

int vimana_datetime_weekday(vimana_system *system) {
  return vimana_datetime_weekday_at(system, vimana_datetime_now_value());
}

int vimana_datetime_year_at(vimana_system *system, int64_t timestamp) {
  (void)system;
  return vimana_datetime_part_value(timestamp, VIMANA_DT_YEAR);
}

int vimana_datetime_month_at(vimana_system *system, int64_t timestamp) {
  (void)system;
  return vimana_datetime_part_value(timestamp, VIMANA_DT_MONTH);
}

int vimana_datetime_day_at(vimana_system *system, int64_t timestamp) {
  (void)system;
  return vimana_datetime_part_value(timestamp, VIMANA_DT_DAY);
}

int vimana_datetime_hour_at(vimana_system *system, int64_t timestamp) {
  (void)system;
  return vimana_datetime_part_value(timestamp, VIMANA_DT_HOUR);
}

int vimana_datetime_minute_at(vimana_system *system, int64_t timestamp) {
  (void)system;
  return vimana_datetime_part_value(timestamp, VIMANA_DT_MINUTE);
}

int vimana_datetime_second_at(vimana_system *system, int64_t timestamp) {
  (void)system;
  return vimana_datetime_part_value(timestamp, VIMANA_DT_SECOND);
}

int vimana_datetime_weekday_at(vimana_system *system, int64_t timestamp) {
  (void)system;
  return vimana_datetime_part_value(timestamp, VIMANA_DT_WEEKDAY);
}

unsigned char *vimana_file_read_bytes(vimana_system *system, const char *path,
                                      size_t *out_size) {
  (void)system;
  if (out_size)
    *out_size = 0;
  if (!path || !path[0])
    return NULL;
  FILE *fp = fopen(path, "rb");
  if (!fp)
    return NULL;
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return NULL;
  }
  long size = ftell(fp);
  if (size < 0) {
    fclose(fp);
    return NULL;
  }
  rewind(fp);
  size_t wanted = (size_t)size;
  size_t alloc = wanted > 0 ? wanted : 1;
  unsigned char *buf = (unsigned char *)malloc(alloc);
  if (!buf) {
    fclose(fp);
    return NULL;
  }
  size_t nread = wanted > 0 ? fread(buf, 1, wanted, fp) : 0;
  bool failed = wanted > 0 && nread != wanted && ferror(fp);
  fclose(fp);
  if (failed) {
    free(buf);
    return NULL;
  }
  if (out_size)
    *out_size = nread;
  return buf;
}

char *vimana_file_read_text(vimana_system *system, const char *path) {
  size_t size = 0;
  unsigned char *bytes = vimana_file_read_bytes(system, path, &size);
  if (!bytes)
    return NULL;
  char *text = (char *)malloc(size + 1);
  if (!text) {
    free(bytes);
    return NULL;
  }
  if (size > 0)
    memcpy(text, bytes, size);
  text[size] = 0;
  free(bytes);
  return text;
}

bool vimana_file_write_bytes(vimana_system *system, const char *path,
                             const unsigned char *bytes, size_t size) {
  (void)system;
  if (!path || !path[0])
    return false;
  FILE *fp = fopen(path, "wb");
  if (!fp)
    return false;
  size_t nwritten = size > 0 ? fwrite(bytes, 1, size, fp) : 0;
  fclose(fp);
  return size == 0 || nwritten == size;
}

bool vimana_file_write_text(vimana_system *system, const char *path,
                            const char *text) {
  size_t len = text ? strlen(text) : 0;
  return vimana_file_write_bytes(system, path,
                                 (const unsigned char *)text, len);
}

bool vimana_file_exists(vimana_system *system, const char *path) {
  (void)system;
  struct stat st;
  return path && path[0] && stat(path, &st) == 0;
}

bool vimana_file_remove(vimana_system *system, const char *path) {
  (void)system;
  if (!path || !path[0])
    return false;
  return remove(path) == 0;
}

bool vimana_file_rename(vimana_system *system, const char *path,
                        const char *new_path) {
  (void)system;
  if (!path || !path[0] || !new_path || !new_path[0])
    return false;
  return rename(path, new_path) == 0;
}

char **vimana_file_list(vimana_system *system, const char *path,
                        int *out_count) {
  (void)system;
  if (out_count)
    *out_count = 0;
  if (!path || !path[0])
    return NULL;

  DIR *dir = opendir(path);
  if (!dir)
    return NULL;

  char **items = NULL;
  int count = 0;
  int cap = 0;
  struct dirent *entry = NULL;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    if (count >= cap) {
      int next_cap = cap == 0 ? 8 : cap * 2;
      char **grown = (char **)realloc(items, sizeof(char *) * (size_t)next_cap);
      if (!grown)
        break;
      items = grown;
      cap = next_cap;
    }
    items[count] = strdup(entry->d_name);
    if (!items[count])
      break;
    count += 1;
  }
  closedir(dir);

  if (count < cap && items) {
    char **shrunk = (char **)realloc(items, sizeof(char *) * (size_t)count);
    if (shrunk)
      items = shrunk;
  }
  if (out_count)
    *out_count = count;
  return items;
}

void vimana_file_list_free(char **items, int count) {
  if (!items)
    return;
  for (int i = 0; i < count; i++)
    free(items[i]);
  free(items);
}

bool vimana_file_is_dir(vimana_system *system, const char *path) {
  (void)system;
  if (!path || !path[0])
    return false;
  struct stat st;
  if (stat(path, &st) != 0)
    return false;
  return S_ISDIR(st.st_mode);
}

/* ── Subprocess IPC ─────────────────────────────────────────────────────── */

#define VIMANA_PROC_LINE_CAP 4096

struct VimanaProcess {
  pid_t pid;
  int stdin_fd;   /* parent writes here → child stdin */
  int stdout_fd;  /* parent reads here  ← child stdout */
  char line_buf[VIMANA_PROC_LINE_CAP];
  int line_len;
};

vimana_process *vimana_process_spawn(vimana_system *system, const char *cmd) {
  (void)system;
  if (!cmd || !cmd[0])
    return NULL;

  int pipe_in[2];   /* parent→child stdin */
  int pipe_out[2];  /* child stdout→parent */
  if (pipe(pipe_in) != 0 || pipe(pipe_out) != 0)
    return NULL;

  pid_t pid = fork();
  if (pid < 0) {
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);
    return NULL;
  }

  if (pid == 0) {
    /* child */
    close(pipe_in[1]);
    close(pipe_out[0]);
    dup2(pipe_in[0], STDIN_FILENO);
    dup2(pipe_out[1], STDOUT_FILENO);
    dup2(pipe_out[1], STDERR_FILENO);
    close(pipe_in[0]);
    close(pipe_out[1]);
    execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
    _exit(127);
  }

  /* parent */
  close(pipe_in[0]);
  close(pipe_out[1]);
  signal(SIGPIPE, SIG_IGN); /* prevent crash if child dies before a write */

  /* make stdout_fd non-blocking so read_line doesn't stall the UI */
  int flags = fcntl(pipe_out[0], F_GETFL);
  if (flags >= 0)
    fcntl(pipe_out[0], F_SETFL, flags | O_NONBLOCK);

  vimana_process *proc = (vimana_process *)calloc(1, sizeof(vimana_process));
  if (!proc) {
    close(pipe_in[1]);
    close(pipe_out[0]);
    kill(pid, SIGTERM);
    return NULL;
  }
  proc->pid = pid;
  proc->stdin_fd = pipe_in[1];
  proc->stdout_fd = pipe_out[0];
  proc->line_len = 0;
  return proc;
}

bool vimana_process_write(vimana_process *proc, const char *text) {
  if (!proc || proc->stdin_fd < 0 || !text)
    return false;
  size_t len = strlen(text);
  ssize_t written = write(proc->stdin_fd, text, len);
  return written == (ssize_t)len;
}

char *vimana_process_read_line(vimana_process *proc) {
  if (!proc || proc->stdout_fd < 0)
    return NULL;

  /* check if we already have a complete line buffered */
  for (int i = 0; i < proc->line_len; i++) {
    if (proc->line_buf[i] == '\n') {
      char *line = (char *)malloc((size_t)(i + 1));
      if (!line)
        return NULL;
      memcpy(line, proc->line_buf, (size_t)i);
      line[i] = '\0';
      /* shift remaining data */
      int rest = proc->line_len - i - 1;
      if (rest > 0)
        memmove(proc->line_buf, proc->line_buf + i + 1, (size_t)rest);
      proc->line_len = rest;
      return line;
    }
  }

  /* try to read more (non-blocking) */
  int space = VIMANA_PROC_LINE_CAP - proc->line_len - 1;
  if (space <= 0)
    return NULL;
  ssize_t n = read(proc->stdout_fd, proc->line_buf + proc->line_len,
                   (size_t)space);
  if (n > 0) {
    proc->line_len += (int)n;
    /* check again for newline */
    for (int i = 0; i < proc->line_len; i++) {
      if (proc->line_buf[i] == '\n') {
        char *line = (char *)malloc((size_t)(i + 1));
        if (!line)
          return NULL;
        memcpy(line, proc->line_buf, (size_t)i);
        line[i] = '\0';
        int rest = proc->line_len - i - 1;
        if (rest > 0)
          memmove(proc->line_buf, proc->line_buf + i + 1, (size_t)rest);
        proc->line_len = rest;
        return line;
      }
    }
  }
  return NULL;
}

bool vimana_process_running(vimana_process *proc) {
  if (!proc || proc->pid <= 0)
    return false;
  int status;
  pid_t ret = waitpid(proc->pid, &status, WNOHANG);
  if (ret == 0)
    return true; /* still running */
  proc->pid = -1;
  return false;
}

void vimana_process_kill(vimana_process *proc) {
  if (!proc)
    return;
  if (proc->pid > 0) {
    kill(proc->pid, SIGTERM);
    int status;
    waitpid(proc->pid, &status, WNOHANG);
    proc->pid = -1;
  }
}

void vimana_process_free(vimana_process *proc) {
  if (!proc)
    return;
  vimana_process_kill(proc);
  if (proc->stdin_fd >= 0)
    close(proc->stdin_fd);
  if (proc->stdout_fd >= 0)
    close(proc->stdout_fd);
  free(proc);
}

/* ── Cleanup ────────────────────────────────────────────────────────────── */

void vimana_system_free(vimana_system *system) {
  if (!system)
    return;
  if (system->audio_stream) {
    SDL_PauseAudioStreamDevice(system->audio_stream);
    SDL_DestroyAudioStream(system->audio_stream);
  }
  free(system->mix_buffer);
  free(system);
}

void vimana_screen_free(vimana_screen *screen) {
  if (!screen)
    return;
  if (screen->titlebar_tex)
    SDL_DestroyTexture(screen->titlebar_tex);
  if (screen->texture)
    SDL_DestroyTexture(screen->texture);
  if (screen->renderer)
    SDL_DestroyRenderer(screen->renderer);
  if (screen->window)
    SDL_DestroyWindow(screen->window);
  for (int i = 0; i < VIMANA_SPRITE_BANK_COUNT; i++)
    free(screen->sprite_banks[i]);
  free(screen->font_rom);
  free(screen->gfx_rom);
  free(screen->layers);
  free(screen->title);
  free(screen->titlebar_title);
  free(screen);
}

/* ── Memory budget helpers ──────────────────────────────────────────────── */

size_t vimana_system_ram_usage(vimana_system *system) {
  if (!system)
    return 0;
  return sizeof(vimana_system);
}

size_t vimana_screen_ram_usage(vimana_screen *screen) {
  if (!screen)
    return 0;
  size_t total = 0;
  /* Font ROM (always allocated) */
  total += VIMANA_FONT_SIZE;
  /* GFX ROM (only if allocated) */
  if (screen->gfx_rom)
    total += VIMANA_GFX_SIZE;
  /* Sprite banks (only count allocated ones) */
  for (int i = 0; i < VIMANA_SPRITE_BANK_COUNT; i++)
    if (screen->sprite_banks[i])
      total += VIMANA_SPRITE_BANK_SIZE;
  /* Port registers + palette */
  total += sizeof(screen->port_x) + sizeof(screen->port_y)
         + sizeof(screen->port_addr) + sizeof(screen->port_auto);
  total += sizeof(screen->base_colors) + sizeof(screen->palette);
  return total;
}

