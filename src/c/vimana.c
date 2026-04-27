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
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#ifdef __APPLE__
#include <objc/message.h>
extern void vimana_autorelease_push(void);
extern void vimana_autorelease_pop(void);
extern bool vimana_poll_event_with_autoreleasepool(SDL_Event *event);
extern void vimana_render_present_with_autoreleasepool(SDL_Renderer *renderer);
#endif

/* Blend look-up table: blend_lut[mode][layer(0=bg,1=fg)][color(0-3)]
   Each entry is a 4-bit palette slot (0-15).
   For FG layer the slot is shifted left by 4 when written to the layer byte.
   Blend modes 0-3 are fully defined; modes 4-15 are not used.
   Rotation 1: identity  1→1,2→2,...,15→15
   Rotation 2: cyclic+1  1→2,2→3,...,15→1
   Rotation 3: cyclic+2  1→3,2→4,...,15→2 */
static const uint8_t blend_lut[16][2][4] = {
    /* fill=0 */
    {{0, 0, 1, 2}, {0, 0, 4, 8}},
    {{0, 1, 2, 3}, {0, 4, 8, 12}},
    {{0, 2, 3, 1}, {0, 8, 12, 4}},
    {{0, 3, 1, 2}, {0, 12, 4, 8}},
    /* fill=1 */
    {{1, 0, 1, 2}, {4, 0, 4, 8}},
    {{1, 1, 2, 3}, {4, 4, 8, 12}},
    {{1, 2, 3, 1}, {4, 8, 12, 4}},
    {{1, 3, 1, 2}, {4, 12, 4, 8}},
    /* fill=2 */
    {{2, 0, 1, 2}, {8, 0, 4, 8}},
    {{2, 1, 2, 3}, {8, 4, 8, 12}},
    {{2, 2, 3, 1}, {8, 8, 12, 4}},
    {{2, 3, 1, 2}, {8, 12, 4, 8}},
    /* fill=3 */
    {{3, 0, 1, 2}, {12, 0, 4, 8}},
    {{3, 1, 2, 3}, {12, 4, 8, 12}},
    {{3, 2, 3, 1}, {12, 8, 12, 4}},
    {{3, 3, 1, 2}, {12, 12, 4, 8}}
};

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

#define VIMANA_CONSOLE_CAP 256

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
  uint8_t controller_key_down;             /* Uxn controller bits from keys */
  uint8_t controller_pad_button_down;      /* Uxn controller bits from pad  */
  uint8_t controller_pad_axis_down;        /* Uxn controller bits from axes */
  uint8_t controller_pressed;              /* edge-triggered controller bits */
  uint8_t console_bytes[VIMANA_CONSOLE_CAP];
  uint8_t console_types[VIMANA_CONSOLE_CAP];
  uint16_t console_head;
  uint16_t console_len;
  uint8_t mouse_down;                      /* 8 bits packed */
  uint8_t mouse_pressed;                   /* 8 bits packed */
  bool quit;
  bool running;
  int16_t wheel_x;
  int16_t wheel_y;
  int16_t pointer_x;
  int16_t pointer_y;
  int16_t pointer_x_raw;
  int16_t pointer_y_raw;
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
  bool text_input_requested;
  bool text_input_active;
  bool gamepad_initialized;
  SDL_Gamepad *gamepad;    /* primary opened gamepad, if any */
  SDL_JoystickID gamepad_id;
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
  int waveform = v->waveform;
  if (waveform < 0 || waveform > VIMANA_WAVE_PSG)
    waveform = VIMANA_WAVE_PULSE;
  return s * vimana_wave_gain[waveform];
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
    float *grown = NULL;
    grown = (float *)realloc(system->mix_buffer,
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
  if (!system || system->audio_stream)
    return;
  if (!SDL_WasInit(SDL_INIT_AUDIO))
    (void)SDL_Init(SDL_INIT_AUDIO);
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

  if (!system->mix_buffer) {
    float *mix_buffer = (float *)calloc(2048u, sizeof(float));
    if (!mix_buffer) {
      SDL_DestroyAudioStream(system->audio_stream);
      system->audio_stream = NULL;
      return;
    }
    system->mix_buffer = mix_buffer;
    system->mix_buffer_cap = 2048;
  }

  (void)SDL_ResumeAudioStreamDevice(system->audio_stream);
}

static bool vimana_system_ensure_audio(vimana_system *system) {
  if (!system)
    return false;
  vimana_system_init_audio(system);
  return system->audio_stream != NULL;
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
static inline uint8_t *vimana_sprite_bank(vimana_screen *s, unsigned int bank);
static inline uint8_t vimana_sprite_read(vimana_screen *s, unsigned int bank,
                                         unsigned int addr);
static uint8_t *vimana_ensure_sprite_bank_capacity(vimana_screen *s,
                                                   unsigned int bank,
                                                   size_t needed);
static bool vimana_screen_alloc_font(vimana_screen *screen, unsigned int size);
static void vimana_font_row_bytes(vimana_screen *screen, const uint8_t *bmp,
                                  unsigned int row, uint8_t out[3]);

struct VimanaScreen {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *canvas_texture;
  char *title;
  uint16_t width;
  uint16_t height;
  uint8_t scale;
  uint16_t width_mar;   /* layer buffer width */
  uint16_t height_mar;  /* layer buffer height */
  uint32_t base_colors[16]; /* RGB palette, slots 0-15 (BG,FG,CLR2..CLR15) */
  uint32_t palette[256];    /* expanded 256-entry composite (8-bit layer index) */
  /* Font ROM: compact UFX storage sized to the active font. */
  uint8_t *font_rom;
  size_t font_rom_size;
  uint8_t font_size;             /* UF1, UF2, UF3 */
  uint8_t font_glyph_bytes;      /* 8, 32, 72 */
  bool font_installed_by_app;    /* true once app code uploads custom font data */
  bool font_rom_owned;           /* shared default font is copy-on-write */
  /* Sprite banks: 16 × 64 KB address space, grown in small pages on demand. */
  uint8_t *sprite_banks[VIMANA_SPRITE_BANK_COUNT];
  size_t sprite_bank_sizes[VIMANA_SPRITE_BANK_COUNT];
  uint8_t active_sprite_bank;                    /* 0–15 */
  /* RAM-class state */
  uint16_t port_x;
  uint16_t port_y;
  uint16_t port_addr;
  uint8_t port_auto;
  uint8_t font_height;           /* UF1=8, UF2=16, UF3=24 */
  uint8_t font_glyph_width;      /* bitmap pixel width: 8 (UF1/UF2) or 24 (UF3) */
  uint8_t *layers;               /* width_mar * height_mar bytes: bits[3:0]=bg, bits[7:4]=fg */
   time_t theme_mtime;            /* last known mtime of ~/.theme */
   bool theme_swap_fg_bg;         /* swap base_colors[0] and [1] after loading */
   int16_t theme_poll_counter;    /* frame counter for periodic theme check */
   int16_t drag_region_height;   /* rows from top that are draggable */
   int16_t canvas_height;        /* total drawn rows (equals height) */
   vimana_system *system;         /* parent system for device input access */
  /* Custom cursor */
  uint8_t cursor_icn[8];         /* 8×8 cursor icon data */
  bool cursor_visible;          /* is cursor currently visible */
  bool cursor_dirty;             /* cursor data changed */
  unsigned int cursor_fg;        /* cursor foreground color slot */
  unsigned int cursor_bg;        /* cursor background color slot */
  bool manual_cursor_pending;    /* app-drawn cursor recorded this frame */
  uint16_t manual_cursor_x;      /* app-drawn cursor x */
  uint16_t manual_cursor_y;      /* app-drawn cursor y */
  bool frame_changed;            /* true when the last present changed pixels */
  bool dirty;
  int dirty_x1;
  int dirty_y1;
  int dirty_x2;
  int dirty_y2;
};

/* ── ROM accessor implementations (need complete struct definition) ───── */

static inline uint8_t *vimana_rom_font_widths(vimana_screen *s) {
  return s->font_rom + VIMANA_FONT_WIDTH_OFF;
}
static inline uint8_t *vimana_rom_font_bitmap(vimana_screen *s, unsigned int code) {
  return s->font_rom + VIMANA_FONT_BMP_OFF
         + code * s->font_glyph_bytes;
}
static inline uint8_t *vimana_sprite_bank(vimana_screen *s, unsigned int bank) {
  if (bank >= VIMANA_SPRITE_BANK_COUNT)
    return NULL;
  return s->sprite_banks[bank];
}
static inline uint8_t vimana_sprite_read(vimana_screen *s, unsigned int bank,
                                         unsigned int addr) {
  if (!s || bank >= VIMANA_SPRITE_BANK_COUNT ||
      addr >= s->sprite_bank_sizes[bank])
    return 0;
  return s->sprite_banks[bank][addr];
}
static uint8_t *vimana_ensure_sprite_bank_capacity(vimana_screen *s,
                                                   unsigned int bank,
                                                   size_t needed) {
  if (bank >= VIMANA_SPRITE_BANK_COUNT)
    return NULL;
  if (needed > VIMANA_SPRITE_BANK_SIZE)
    needed = VIMANA_SPRITE_BANK_SIZE;
  if (needed <= s->sprite_bank_sizes[bank])
    return s->sprite_banks[bank];
  size_t new_size = (needed + 0x0FFFu) & ~(size_t)0x0FFFu;
  if (new_size > VIMANA_SPRITE_BANK_SIZE)
    new_size = VIMANA_SPRITE_BANK_SIZE;
  uint8_t *grown = (uint8_t *)realloc(s->sprite_banks[bank], new_size);
  if (!grown)
    return NULL;
  memset(grown + s->sprite_bank_sizes[bank], 0,
         new_size - s->sprite_bank_sizes[bank]);
  s->sprite_banks[bank] = grown;
  s->sprite_bank_sizes[bank] = new_size;
  return s->sprite_banks[bank];
}

static unsigned int vimana_font_glyph_bytes_for_size(unsigned int size) {
  if (size == 3)
    return VIMANA_UF3_BYTES;
  if (size == 2)
    return VIMANA_UF2_BYTES;
  return VIMANA_UF1_BYTES;
}

static unsigned int vimana_font_height_for_size(unsigned int size) {
  if (size == 3)
    return VIMANA_FONT_MAX_HEIGHT;
  if (size == 2)
    return VIMANA_GLYPH_HEIGHT;
  return VIMANA_TILE_SIZE;
}

static unsigned int vimana_font_default_width_for_size(unsigned int size) {
  if (size == 3)
    return VIMANA_TILE_SIZE * 2;
  return VIMANA_TILE_SIZE;
}

static size_t vimana_font_storage_size_for_size(unsigned int size) {
  return (size_t)VIMANA_FONT_BMP_OFF +
         (size_t)VIMANA_GLYPH_COUNT * vimana_font_glyph_bytes_for_size(size);
}

static uint8_t *vimana_default_font_rom(unsigned int size, size_t *out_size) {
  static uint8_t *roms[3];
  if (size < 1 || size > 3)
    size = 1;
  size_t font_size = vimana_font_storage_size_for_size(size);
  if (out_size)
    *out_size = font_size;
  unsigned int idx = size - 1;
  if (!roms[idx]) {
    uint8_t *rom = (uint8_t *)calloc(font_size, 1);
    if (!rom)
      return NULL;
    uint8_t *widths = rom + VIMANA_FONT_WIDTH_OFF;
    unsigned int width = vimana_font_default_width_for_size(size);
    for (unsigned int i = 0; i < VIMANA_GLYPH_COUNT; i++)
      widths[i] = (uint8_t)width;
    roms[idx] = rom;
  }
  return roms[idx];
}

static bool vimana_screen_alloc_font(vimana_screen *screen, unsigned int size) {
  if (!screen)
    return false;
  if (size < 1 || size > 3)
    size = 1;
  size_t font_size = 0;
  uint8_t *font_rom = vimana_default_font_rom(size, &font_size);
  if (!font_rom)
    return false;
  if (screen->font_rom_owned)
    free(screen->font_rom);
  screen->font_rom = font_rom;
  screen->font_rom_size = font_size;
  screen->font_size = (uint8_t)size;
  screen->font_glyph_bytes = (uint8_t)vimana_font_glyph_bytes_for_size(size);
  screen->font_installed_by_app = false;
  screen->font_rom_owned = false;
  return true;
}

static bool vimana_screen_ensure_font_owned(vimana_screen *screen) {
  if (!screen || !screen->font_rom)
    return false;
  if (screen->font_rom_owned)
    return true;
  uint8_t *font_rom = (uint8_t *)malloc(screen->font_rom_size);
  if (!font_rom)
    return false;
  memcpy(font_rom, screen->font_rom, screen->font_rom_size);
  screen->font_rom = font_rom;
  screen->font_rom_owned = true;
  return true;
}

static void vimana_font_row_bytes(vimana_screen *screen, const uint8_t *bmp,
                                  unsigned int row, uint8_t out[3]) {
  out[0] = 0;
  out[1] = 0;
  out[2] = 0;
  if (!screen || !bmp)
    return;
  if (screen->font_size == 3) {
    if (row >= VIMANA_FONT_MAX_HEIGHT)
      return;
    unsigned int tile_row = row / VIMANA_TILE_SIZE;
    unsigned int local_row = row % VIMANA_TILE_SIZE;
    out[0] = bmp[(0 * 3 + tile_row) * VIMANA_TILE_SIZE + local_row];
    out[1] = bmp[(1 * 3 + tile_row) * VIMANA_TILE_SIZE + local_row];
    out[2] = bmp[(2 * 3 + tile_row) * VIMANA_TILE_SIZE + local_row];
    return;
  }
  if (screen->font_size == 2) {
    if (row >= VIMANA_GLYPH_HEIGHT)
      return;
    unsigned int tile_row = row / VIMANA_TILE_SIZE;
    unsigned int local_row = row % VIMANA_TILE_SIZE;
    out[0] = bmp[(tile_row == 0 ? 0 : 1) * VIMANA_TILE_SIZE + local_row];
    out[1] = bmp[(tile_row == 0 ? 2 : 3) * VIMANA_TILE_SIZE + local_row];
    return;
  }
  if (row >= VIMANA_TILE_SIZE)
    return;
  out[0] = bmp[row];
}

static uint8_t vimana_controller_bits(vimana_system *system);

static bool vimana_system_has_active_input(vimana_system *system) {
  if (!system)
    return false;
  for (int i = 0; i < VIMANA_KEY_WORDS; i++)
    if (system->key_down[i] != 0)
      return true;
  if (system->mouse_down != 0)
    return true;
  return vimana_controller_bits(system) != 0;
}

static bool vimana_is_manual_cursor_draw(vimana_screen *screen,
                                         unsigned int bank,
                                         unsigned int addr,
                                         unsigned int ctrl,
                                         unsigned int repeat) {
  if (!screen)
    return false;
  if (ctrl != 0x85 || repeat != 0)
    return false;
  if (addr + VIMANA_TILE_SIZE > VIMANA_SPRITE_BANK_SIZE)
    return false;
  for (unsigned int i = 0; i < VIMANA_TILE_SIZE; i++)
    if (vimana_sprite_read(screen, bank, addr + i) != screen->cursor_icn[i])
      return false;
  return true;
}

static void vimana_screen_mark_dirty(vimana_screen *screen, int x1, int y1,
                                     int x2, int y2) {
  if (!screen || x2 <= x1 || y2 <= y1)
    return;
  if (x1 < 0) x1 = 0;
  if (y1 < 0) y1 = 0;
  if (x2 > (int)screen->width) x2 = (int)screen->width;
  if (y2 > (int)screen->canvas_height) y2 = (int)screen->canvas_height;
  if (x2 <= x1 || y2 <= y1)
    return;
  if (!screen->dirty) {
    screen->dirty = true;
    screen->dirty_x1 = x1;
    screen->dirty_y1 = y1;
    screen->dirty_x2 = x2;
    screen->dirty_y2 = y2;
    return;
  }
  if (x1 < screen->dirty_x1) screen->dirty_x1 = x1;
  if (y1 < screen->dirty_y1) screen->dirty_y1 = y1;
  if (x2 > screen->dirty_x2) screen->dirty_x2 = x2;
  if (y2 > screen->dirty_y2) screen->dirty_y2 = y2;
}

static void vimana_screen_mark_all_dirty(vimana_screen *screen) {
  if (!screen)
    return;
  vimana_screen_mark_dirty(screen, 0, 0, (int)screen->width,
                           (int)screen->canvas_height);
}

static void vimana_draw_manual_cursor_overlay(vimana_screen *screen) {
  if (!screen || !screen->manual_cursor_pending)
    return;
  uint32_t fg_rgb = screen->base_colors[1];
  SDL_SetRenderDrawColor(screen->renderer,
                         (uint8_t)((fg_rgb >> 16) & 0xFF),
                         (uint8_t)((fg_rgb >> 8) & 0xFF),
                         (uint8_t)(fg_rgb & 0xFF), 255);
  for (unsigned int row = 0; row < VIMANA_TILE_SIZE; row++) {
    uint8_t bits = screen->cursor_icn[row];
    for (unsigned int col = 0; col < VIMANA_TILE_SIZE; col++) {
      if (!(bits & (uint8_t)(0x80u >> col)))
        continue;
      SDL_FRect px = {
        (float)((screen->manual_cursor_x + col) * screen->scale),
        (float)((screen->manual_cursor_y + row) * screen->scale),
        (float)screen->scale,
        (float)screen->scale
      };
      SDL_RenderFillRect(screen->renderer, &px);
    }
  }
  screen->manual_cursor_pending = false;
}

static void vimana_screen_draw_sprite_1bpp(vimana_screen *screen,
                                           unsigned int x,
                                           unsigned int y,
                                           const uint8_t *rows,
                                           unsigned int fg,
                                           unsigned int bg) {
  if (!screen || !rows || !screen->layers)
    return;
  if (x >= (unsigned int)screen->width ||
      y >= (unsigned int)screen->canvas_height)
    return;
  vimana_screen_mark_dirty(screen, (int)x, (int)y,
                           (int)x + VIMANA_TILE_SIZE,
                           (int)y + VIMANA_TILE_SIZE);
  unsigned int bg_slot = bg & 0xF;
  unsigned int fg_slot = fg & 0xF;
  for (unsigned int row = 0; row < VIMANA_TILE_SIZE; row++) {
    unsigned int py = y + row;
    if (py >= (unsigned int)screen->canvas_height)
      continue;
    const uint8_t plane0 = rows[row];
    for (unsigned int col = 0; col < VIMANA_TILE_SIZE; col++) {
      unsigned int px = x + col;
      if (px >= (unsigned int)screen->width)
        continue;
      uint8_t mask = (uint8_t)(0x80u >> col);
      uint8_t hit = (plane0 & mask) ? 1u : 0u;
      uint8_t slot = hit ? (uint8_t)fg_slot : (uint8_t)bg_slot;
      screen->layers[py * screen->width_mar + px] =
          (uint8_t)((slot << 4) | slot);
    }
  }
}

static uint32_t vimana_parse_hex_color(const char *hex) {
  unsigned int r = 0, g = 0, b = 0;
  if (!hex)
    return 0x101418u;
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
  return (uint32_t)(r << 16) | (uint32_t)(g << 8) | (uint32_t)b;
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
  vimana_screen_mark_all_dirty(screen);
}

static void vimana_screen_reset_palette(vimana_screen *screen) {
  if (!screen)
    return;
  screen->base_colors[0]  = 0xFFFFFFu; /* BG   */
  screen->base_colors[1]  = 0x000000u; /* FG   */
  screen->base_colors[2]  = 0x77DDCCu; /* CLR2 */
  screen->base_colors[3]  = 0xFFBB22u; /* CLR3 */
  screen->base_colors[4]  = 0xFF4444u; /* CLR4 */
  screen->base_colors[5]  = 0x44AA44u; /* CLR5 */
  screen->base_colors[6]  = 0x4488FFu; /* CLR6 */
  screen->base_colors[7]  = 0xAA44AAu; /* CLR7 */
  screen->base_colors[8]  = 0x888888u; /* CLR8 */
  screen->base_colors[9]  = 0x666666u; /* CLR9 */
  screen->base_colors[10] = 0xBBBB44u; /* CLR10 */
  screen->base_colors[11] = 0x44BBBBu; /* CLR11 */
  screen->base_colors[12] = 0x8844BBu; /* CLR12 */
  screen->base_colors[13] = 0xBB4488u; /* CLR13 */
  screen->base_colors[14] = 0x44BB88u; /* CLR14 */
  screen->base_colors[15] = 0xBB8844u; /* CLR15 */
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
  uint8_t buf[6];
  if (fread(buf, 1, 6, f) != 6) {
    fclose(f);
    return;
  }
  fclose(f);
  uint8_t r[4], g[4], b[4];
  r[0] = buf[0] >> 4;
  r[1] = buf[0] & 0x0F;
  r[2] = buf[1] >> 4;
  r[3] = buf[1] & 0x0F;
  g[0] = buf[2] >> 4;
  g[1] = buf[2] & 0x0F;
  g[2] = buf[3] >> 4;
  g[3] = buf[3] & 0x0F;
  b[0] = buf[4] >> 4;
  b[1] = buf[4] & 0x0F;
  b[2] = buf[5] >> 4;
  b[3] = buf[5] & 0x0F;
  for (int i = 0; i < 4; i++)
    screen->base_colors[i] =
        ((uint32_t)r[i] * 17 << 16) | ((uint32_t)g[i] * 17 << 8) |
        (uint32_t)b[i] * 17;
  if (screen->theme_swap_fg_bg) {
    uint32_t tmp = screen->base_colors[0];
    screen->base_colors[0] = screen->base_colors[1];
    screen->base_colors[1] = tmp;
  }
  vimana_colorize(screen);
  struct stat st;
  if (stat(path, &st) == 0)
    screen->theme_mtime = st.st_mtime;
}

static void vimana_screen_poll_theme(vimana_screen *screen) {
  if (!screen)
    return;
  if (++screen->theme_poll_counter < 30)
    return;
  screen->theme_poll_counter = 0;
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

static void vimana_screen_reset_font(vimana_screen *screen) {
  if (!screen)
    return;
  (void)vimana_screen_alloc_font(screen, screen->font_size);
  screen->font_height = (uint8_t)vimana_font_height_for_size(screen->font_size);
  screen->font_glyph_width =
      (uint8_t)vimana_font_default_width_for_size(screen->font_size);
}

static void vimana_screen_reset_ports(vimana_screen *screen) {
  if (!screen)
    return;
  screen->port_x = 0;
  screen->port_y = 0;
  screen->port_addr = 0;
  screen->port_auto = 0;
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
                                             size_t len) {
  if (!screen || !sprite || len == 0)
    return;
  addr = vimana_screen_addr_clamp(screen, addr);
  size_t max_len = VIMANA_SPRITE_BANK_SIZE - (size_t)addr;
  if (len > max_len)
    len = max_len;
  if (len == 0)
    return;
  size_t needed = (size_t)addr + len;
  uint8_t *bank = vimana_ensure_sprite_bank_capacity(
      screen, screen->active_sprite_bank, needed);
  if (!bank)
    return;
  memcpy(bank + addr, sprite, len);
}

static int vimana_screen_auto_repeat(vimana_screen *screen) {
  if (!screen)
    return 1;
  return (screen->port_auto >> 4);
}

static void vimana_reset_pressed(vimana_system *system) {
  if (!system)
    return;
  memset(system->key_pressed, 0, sizeof(system->key_pressed));
  system->controller_pressed = 0;
  system->mouse_pressed = 0;
  system->wheel_x = 0;
  system->wheel_y = 0;
  system->text_input[0] = 0;
}

static bool vimana_console_enqueue(vimana_system *system, uint8_t byte,
                                   uint8_t type) {
  if (!system || system->console_len >= VIMANA_CONSOLE_CAP)
    return false;
  uint16_t slot =
      (uint16_t)((system->console_head + system->console_len) %
                 VIMANA_CONSOLE_CAP);
  system->console_bytes[slot] = byte;
  system->console_types[slot] = type;
  system->console_len++;
  return true;
}

static void vimana_console_enqueue_text(vimana_system *system, const char *text,
                                        uint8_t type) {
  if (!system || !text)
    return;
  for (size_t i = 0; text[i] != 0; i++)
    if (!vimana_console_enqueue(system, (uint8_t)text[i], type))
      break;
}

static uint8_t vimana_controller_bits(vimana_system *system) {
  if (!system)
    return 0;
  return (uint8_t)(system->controller_key_down |
                   system->controller_pad_button_down |
                   system->controller_pad_axis_down);
}

static void vimana_controller_update_source(vimana_system *system,
                                            uint8_t *field,
                                            uint8_t mask, bool down) {
  if (!system || !field || !mask)
    return;
  uint8_t before = vimana_controller_bits(system);
  if (down)
    *field |= mask;
  else
    *field &= (uint8_t)~mask;
  uint8_t after = vimana_controller_bits(system);
  system->controller_pressed |= (uint8_t)(after & (uint8_t)~before);
}

static uint8_t vimana_controller_mask_from_scancode(SDL_Scancode scancode) {
  switch (scancode) {
  case SDL_SCANCODE_LCTRL:
    return VIMANA_CONTROLLER_A;
  case SDL_SCANCODE_LALT:
    return VIMANA_CONTROLLER_B;
  case SDL_SCANCODE_LSHIFT:
    return VIMANA_CONTROLLER_SELECT;
  case SDL_SCANCODE_HOME:
    return VIMANA_CONTROLLER_START;
  case SDL_SCANCODE_UP:
    return VIMANA_CONTROLLER_UP;
  case SDL_SCANCODE_DOWN:
    return VIMANA_CONTROLLER_DOWN;
  case SDL_SCANCODE_LEFT:
    return VIMANA_CONTROLLER_LEFT;
  case SDL_SCANCODE_RIGHT:
    return VIMANA_CONTROLLER_RIGHT;
  default:
    return 0;
  }
}

static uint8_t vimana_controller_mask_from_button(Uint8 button) {
  switch ((SDL_GamepadButton)button) {
  case SDL_GAMEPAD_BUTTON_SOUTH:
    return VIMANA_CONTROLLER_A;
  case SDL_GAMEPAD_BUTTON_EAST:
    return VIMANA_CONTROLLER_B;
  case SDL_GAMEPAD_BUTTON_WEST:
  case SDL_GAMEPAD_BUTTON_BACK:
    return VIMANA_CONTROLLER_SELECT;
  case SDL_GAMEPAD_BUTTON_NORTH:
  case SDL_GAMEPAD_BUTTON_START:
    return VIMANA_CONTROLLER_START;
  case SDL_GAMEPAD_BUTTON_DPAD_UP:
    return VIMANA_CONTROLLER_UP;
  case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
    return VIMANA_CONTROLLER_DOWN;
  case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
    return VIMANA_CONTROLLER_LEFT;
  case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
    return VIMANA_CONTROLLER_RIGHT;
  default:
    return 0;
  }
}

static uint8_t vimana_controller_axis_mask(Uint8 axis, Sint16 value) {
  switch ((SDL_GamepadAxis)axis) {
  case SDL_GAMEPAD_AXIS_LEFTX:
    if (value < -3200)
      return VIMANA_CONTROLLER_LEFT;
    if (value > 3200)
      return VIMANA_CONTROLLER_RIGHT;
    return 0;
  case SDL_GAMEPAD_AXIS_LEFTY:
    if (value < -3200)
      return VIMANA_CONTROLLER_UP;
    if (value > 3200)
      return VIMANA_CONTROLLER_DOWN;
    return 0;
  default:
    return 0;
  }
}

static void vimana_controller_update_axis(vimana_system *system, Uint8 axis,
                                          Sint16 value) {
  if (!system)
    return;
  uint8_t clear_mask = 0;
  switch ((SDL_GamepadAxis)axis) {
  case SDL_GAMEPAD_AXIS_LEFTX:
    clear_mask = VIMANA_CONTROLLER_LEFT | VIMANA_CONTROLLER_RIGHT;
    break;
  case SDL_GAMEPAD_AXIS_LEFTY:
    clear_mask = VIMANA_CONTROLLER_UP | VIMANA_CONTROLLER_DOWN;
    break;
  default:
    return;
  }
  vimana_controller_update_source(system, &system->controller_pad_axis_down,
                                  clear_mask, false);
  vimana_controller_update_source(system, &system->controller_pad_axis_down,
                                  vimana_controller_axis_mask(axis, value),
                                  true);
}

static void vimana_system_close_gamepad(vimana_system *system) {
  if (!system)
    return;
  if (system->gamepad) {
    SDL_CloseGamepad(system->gamepad);
    system->gamepad = NULL;
  }
  system->gamepad_id = 0;
  system->controller_pad_button_down = 0;
  system->controller_pad_axis_down = 0;
}

static void vimana_system_open_gamepad(vimana_system *system,
                                       SDL_JoystickID instance_id) {
  if (!system || system->gamepad || !SDL_IsGamepad(instance_id))
    return;
  system->gamepad = SDL_OpenGamepad(instance_id);
  if (system->gamepad)
    system->gamepad_id = SDL_GetGamepadID(system->gamepad);
}

static void vimana_system_open_first_gamepad(vimana_system *system);

static void vimana_system_ensure_gamepad(vimana_system *system) {
  if (!system || system->gamepad_initialized)
    return;
  if (!SDL_WasInit(SDL_INIT_GAMEPAD))
    (void)SDL_InitSubSystem(SDL_INIT_GAMEPAD);
  system->gamepad_initialized = true;
  vimana_system_open_first_gamepad(system);
}

static void vimana_system_open_first_gamepad(vimana_system *system) {
  if (!system || system->gamepad || !SDL_WasInit(SDL_INIT_GAMEPAD))
    return;
  int count = 0;
  SDL_JoystickID *ids = SDL_GetGamepads(&count);
  if (!ids)
    return;
  for (int i = 0; i < count; i++) {
    vimana_system_open_gamepad(system, ids[i]);
    if (system->gamepad)
      break;
  }
  SDL_free(ids);
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
  system->pointer_x_raw = x;
  system->pointer_y_raw = y;
  system->pointer_x = x / scale;
  system->pointer_y = y / scale;
  system->tile_x = system->pointer_x / VIMANA_TILE_SIZE;
  system->tile_y = system->pointer_y / VIMANA_TILE_SIZE;
}

static void vimana_pump_events(vimana_system *system, vimana_screen *screen) {
  if (!system)
    return;

  SDL_Event event;
  for (;;) {
#ifdef __APPLE__
    if (!vimana_poll_event_with_autoreleasepool(&event))
      break;
#else
    if (!SDL_PollEvent(&event))
      break;
#endif
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
      vimana_controller_update_source(system, &system->controller_key_down,
                                      vimana_controller_mask_from_scancode(
                                          event.key.scancode),
                                      true);
      break;
    }
    case SDL_EVENT_KEY_UP: {
      int scancode = (int)event.key.scancode;
      if (scancode >= 0 && scancode < VIMANA_KEY_CAP)
        vimana_bit_clr(system->key_down, scancode);
      vimana_controller_update_source(system, &system->controller_key_down,
                                      vimana_controller_mask_from_scancode(
                                          event.key.scancode),
                                      false);
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
      vimana_console_enqueue_text(system, event.text.text, VIMANA_CONSOLE_STD);
      break;
    case SDL_EVENT_GAMEPAD_ADDED:
      vimana_system_open_gamepad(system, event.gdevice.which);
      break;
    case SDL_EVENT_GAMEPAD_REMOVED:
      if (system->gamepad && system->gamepad_id == event.gdevice.which) {
        vimana_system_close_gamepad(system);
        vimana_system_open_first_gamepad(system);
      }
      break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
      vimana_controller_update_source(system, &system->controller_pad_button_down,
                                      vimana_controller_mask_from_button(
                                          event.gbutton.button),
                                      true);
      break;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
      vimana_controller_update_source(system, &system->controller_pad_button_down,
                                      vimana_controller_mask_from_button(
                                          event.gbutton.button),
                                      false);
      break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      vimana_controller_update_axis(system, event.gaxis.axis,
                                    event.gaxis.value);
      break;
    default:
      break;
    }
  }
}

/* ── System API ─────────────────────────────────────────────────────────── */

vimana_system *vimana_system_new(void) {
  if (!SDL_WasInit(SDL_INIT_VIDEO)) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
      return NULL;
  } else {
    if (!SDL_WasInit(SDL_INIT_EVENTS))
      (void)SDL_Init(SDL_INIT_EVENTS);
  }
  vimana_system *system = (vimana_system *)calloc(1, sizeof(vimana_system));
  if (!system)
    return NULL;
  system->master_volume = 15;
  system->pointer_x = -1;
  system->pointer_y = -1;
  system->tile_x = -1;
  system->tile_y = -1;
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
  if (!system)
    return;
  vimana_system_init_audio(system);
  if (!system->audio_stream)
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
  if (!vimana_system_ensure_audio(system))
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
  if (!vimana_system_ensure_audio(system))
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
  if (!vimana_system_ensure_audio(system))
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
  if (!vimana_system_ensure_audio(system))
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
  if (!vimana_system_ensure_audio(system))
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
  if (!vimana_system_ensure_audio(system))
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
  if (!vimana_system_ensure_audio(system))
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
  if (!vimana_system_ensure_audio(system))
    return;
  if (!((channel >= 0 && channel < 3) || (channel >= 4 && channel < 7)))
    return; /* sync for tone voices 0–2 and 4–6 */
  vimana_audio_lock(system);
  system->voices[channel].sync = (enable != 0);
  vimana_audio_unlock(system);
}

void vimana_system_set_ring_mod(vimana_system *system, int channel,
                                int enable) {
  if (!vimana_system_ensure_audio(system))
    return;
  if (!((channel >= 0 && channel < 3) || (channel >= 4 && channel < 7)))
    return; /* ring mod for tone voices 0–2 and 4–6 */
  vimana_audio_lock(system);
  system->voices[channel].ring_mod = (enable != 0);
  vimana_audio_unlock(system);
}

void vimana_system_set_filter(vimana_system *system, int cutoff,
                              int resonance, int mode) {
  if (!vimana_system_ensure_audio(system))
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
  if (!vimana_system_ensure_audio(system))
    return;
  if (channel < 0 || channel >= VIMANA_VOICE_COUNT)
    return;
  vimana_audio_lock(system);
  system->filter.route[channel] = (enable != 0);
  vimana_audio_unlock(system);
}

void vimana_system_set_master_volume(vimana_system *system, int volume) {
  if (!vimana_system_ensure_audio(system))
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
  const uint64_t freq = SDL_GetPerformanceFrequency();
  const uint64_t frame_ns = freq / 60; /* 60 fps cap */
  uint64_t last = SDL_GetPerformanceCounter();
  while (!system->quit) {
#ifdef __APPLE__
    vimana_autorelease_push();
#endif
    vimana_reset_pressed(system);
    if (system->text_input_requested && !system->text_input_active) {
      SDL_StartTextInput(screen->window);
      system->text_input_active = true;
    }
    vimana_pump_events(system, screen);
    vimana_screen_poll_theme(screen);
    if (frame)
      frame(system, screen, user);
#ifdef __APPLE__
    vimana_autorelease_pop();
#endif
    /* Back off when the last frame was identical and there is no live input. */
    if (!screen->frame_changed && !vimana_system_has_active_input(system)) {
      SDL_Delay(50);
      last = SDL_GetPerformanceCounter();
      continue;
    }
    /* Cap at 60 fps: sleep until the next frame boundary */
    uint64_t now = SDL_GetPerformanceCounter();
    uint64_t elapsed = now - last;
    if (elapsed < frame_ns) {
      uint64_t remain = frame_ns - elapsed;
      uint32_t ms = (uint32_t)((remain * 1000 + freq - 1) / freq);
      if (ms > 0)
        SDL_Delay(ms);
    }
    last = SDL_GetPerformanceCounter();
  }
  if (system->text_input_active) {
    SDL_StopTextInput(screen->window);
    system->text_input_active = false;
  }
  system->running = false;
}

void vimana_system_free(vimana_system *system) {
  if (!system)
    return;
  vimana_system_close_gamepad(system);
  if (system->audio_stream) {
    SDL_PauseAudioStreamDevice(system->audio_stream);
    SDL_DestroyAudioStream(system->audio_stream);
  }
  free(system->mix_buffer);
  free(system);
}

size_t vimana_system_ram_usage(vimana_system *system) {
  if (!system)
    return 0;
  return sizeof(vimana_system) +
         ((system->mix_buffer_cap > 0)
              ? (size_t)system->mix_buffer_cap * sizeof(float)
              : 0);
}

/* ── Screen API ─────────────────────────────────────────────────────────── */

static SDL_HitTestResult vimana_hit_test(SDL_Window *win,
                                        const SDL_Point *area,
                                        void *data) {
  (void)win;
  vimana_screen *screen = (vimana_screen *)data;
  vimana_system *system = screen->system;
  int scale = screen->scale > 0 ? screen->scale : 1;

  int drag_h_px = 0;
  if (screen->drag_region_height > 0)
    drag_h_px = (int)((int64_t)screen->drag_region_height * scale);

  if (drag_h_px > 0 && area->y < drag_h_px) {
    int close_x0 = (int)((int64_t)8 * scale);
    int close_x1 = (int)((int64_t)24 * scale);
    int right_hole_x0 = (int)(((int64_t)screen->width - 24) * scale);
    int right_hole_x1 = (int)(((int64_t)screen->width - 8) * scale);
    if (right_hole_x0 < 0)
      right_hole_x0 = 0;
    if (right_hole_x1 < 0)
      right_hole_x1 = 0;
    if (area->x >= close_x0 && area->x < close_x1)
      return SDL_HITTEST_NORMAL;
    if (area->x >= right_hole_x0 && area->x < right_hole_x1)
      return SDL_HITTEST_NORMAL;
    vimana_update_pointer(system, screen, (unsigned int)area->x,
                          (unsigned int)area->y);
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
  if (width > UINT16_MAX)
    width = UINT16_MAX;
  if (height > UINT16_MAX)
    height = UINT16_MAX;
  if (scale > UINT8_MAX)
    scale = UINT8_MAX;

  const char *window_title = title ? title : "vimana";
  size_t title_len = strlen(window_title) + 1;
  if ((size_t)height != 0 && (size_t)width > SIZE_MAX / (size_t)height)
    return NULL;
  size_t layers_bytes = (size_t)width * (size_t)height;
  if (title_len > SIZE_MAX - sizeof(vimana_screen))
    return NULL;
  size_t base_bytes = sizeof(vimana_screen) + title_len;
  if (layers_bytes > SIZE_MAX - base_bytes)
    return NULL;
  size_t screen_bytes = base_bytes + layers_bytes;
  vimana_screen *screen = (vimana_screen *)calloc(1, screen_bytes);
  if (!screen)
    return NULL;

  /* Pack title and layer buffer into one contiguous block. */
  uint8_t *screen_tail = (uint8_t *)screen + sizeof(vimana_screen);
  screen->title = (char *)screen_tail;
  memcpy(screen->title, window_title, title_len);
  screen_tail += title_len;
  screen->layers = screen_tail;
  /* Sprite banks grow in small pages on first write. */

  screen->width = width;
  screen->height = height;
  screen->scale = scale;
  screen->width_mar = width;
  screen->height_mar = height;
  screen->canvas_height = height;

  screen->drag_region_height = 0;

  screen->cursor_visible = false;
  screen->cursor_dirty = false;
  screen->cursor_fg = 1;
  screen->cursor_bg = 0;
  screen->manual_cursor_pending = false;
  screen->manual_cursor_x = 0;
  screen->manual_cursor_y = 0;
  screen->frame_changed = true;
  screen->dirty = false;
  screen->system = NULL;
  if (!vimana_screen_alloc_font(screen, 1)) {
    free(screen);
    return NULL;
  }
  SDL_HideCursor(); /* apps draw their own cursors */
  vimana_screen_reset_palette(screen);
  vimana_screen_load_theme(screen);
  vimana_screen_reset_font(screen);
  vimana_screen_reset_ports(screen);

  screen->window =
      SDL_CreateWindow(screen->title, width * scale,
                       screen->canvas_height * scale,
                       SDL_WINDOW_BORDERLESS);
  if (!screen->window) {
    if (screen->font_rom_owned)
      free(screen->font_rom);
    free(screen);
    return NULL;
  }

  /* Ensure system cursor is hidden on window creation */
  SDL_HideCursor();

  /* Reduce Metal memory footprint: prefer integrated GPU, enable vsync */
  SDL_SetHint(SDL_HINT_RENDER_METAL_PREFER_LOW_POWER_DEVICE, "1");
  SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

  screen->renderer = SDL_CreateRenderer(screen->window, NULL);
  if (!screen->renderer) {
    SDL_DestroyWindow(screen->window);
    if (screen->font_rom_owned)
      free(screen->font_rom);
    free(screen);
    return NULL;
  }

  /* Disable texture filtering for sharp nearest-neighbor scaling */
  SDL_SetDefaultTextureScaleMode(screen->renderer, SDL_SCALEMODE_PIXELART);

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

  screen->canvas_texture = SDL_CreateTexture(
      screen->renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
      screen->width, screen->canvas_height);
  if (screen->canvas_texture) {
    SDL_SetTextureScaleMode(screen->canvas_texture, SDL_SCALEMODE_NEAREST);
  }

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
  vimana_screen_mark_all_dirty(screen);
}

void vimana_screen_set_palette(vimana_screen *screen, unsigned int slot,
                               const char *hex) {
  if (!screen)
    return;
  unsigned int s = slot & 15;
  screen->base_colors[s] = vimana_parse_hex_color(hex);
  vimana_colorize(screen);
}

void vimana_screen_set_theme_swap(vimana_screen *screen, bool swap) {
  if (!screen)
    return;
  screen->theme_swap_fg_bg = swap;
  vimana_screen_load_theme(screen);
}

void vimana_screen_set_font(vimana_screen *screen, unsigned int code,
                            const uint16_t *glyph, unsigned int len) {
  if (!screen || !glyph || !screen->font_rom)
    return;
  if (code >= VIMANA_GLYPH_COUNT)
    return;
  if (!vimana_screen_ensure_font_owned(screen))
    return;
  unsigned int max_words = screen->font_glyph_bytes / 2;
  unsigned int n = len < max_words ? len : max_words;
  uint8_t *bmp = vimana_rom_font_bitmap(screen, code);
  memset(bmp, 0, screen->font_glyph_bytes);
  for (unsigned int i = 0; i < n; i++) {
    bmp[i * 2] = (uint8_t)((glyph[i] >> 8) & 0xFFu);
    bmp[i * 2 + 1] = (uint8_t)(glyph[i] & 0xFFu);
  }
  screen->font_installed_by_app = true;
}

void vimana_screen_set_font_width(vimana_screen *screen, unsigned int code,
                                  unsigned int width) {
  if (!screen || !screen->font_rom || code >= VIMANA_GLYPH_COUNT)
    return;
  if (!vimana_screen_ensure_font_owned(screen))
    return;
  vimana_rom_font_widths(screen)[code] =
      (uint8_t)(width > 0 && width <= 255 ? width : VIMANA_TILE_SIZE);
  screen->font_installed_by_app = true;
}

void vimana_screen_set_font_size(vimana_screen *screen, unsigned int size) {
  if (!screen)
    return;
  if (size < 1 || size > 3)
    size = 1;
  if ((unsigned int)screen->font_size == size)
    return;
  if (!vimana_screen_alloc_font(screen, size))
    return;
  vimana_screen_reset_font(screen);
}

void vimana_screen_set_sprite(vimana_screen *screen, unsigned int addr,
                              const uint8_t *sprite, unsigned int mode,
                              size_t len) {
  (void)mode;
  if (!screen || !sprite || len == 0)
    return;
  vimana_screen_store_sprite_bytes(
      screen, vimana_screen_addr_clamp(screen, addr), sprite, len);
}

void vimana_screen_set_x(vimana_screen *screen, unsigned int x) {
  if (!screen)
    return;
  screen->port_x = (uint16_t)x;  /* stored as pixels */
}

void vimana_screen_set_y(vimana_screen *screen, unsigned int y) {
  if (!screen)
    return;
  screen->port_y = (uint16_t)y;  /* stored as pixels */
}

void vimana_screen_set_addr(vimana_screen *screen, unsigned int addr) {
  if (!screen)
    return;
  screen->port_addr = (uint16_t)vimana_screen_addr_clamp(screen, addr);
}

void vimana_screen_set_auto(vimana_screen *screen, unsigned int auto_flags) {
  if (!screen)
    return;
  screen->port_auto = (uint8_t)(auto_flags);
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
  addr = vimana_screen_addr_clamp(screen, addr);
  /* Upload to sprite bank 0 (for use with sprite() command) */
  size_t max_len = VIMANA_SPRITE_BANK_SIZE - (size_t)addr;
  if ((size_t)len > max_len)
    len = (unsigned int)max_len;
  if (len == 0)
    return;
  uint8_t *bank = vimana_ensure_sprite_bank_capacity(
      screen, 0, (size_t)addr + (size_t)len);
  if (!bank)
    return;
  memcpy(bank + addr, data, (size_t)len);
}

const uint8_t *vimana_screen_gfx(vimana_screen *screen, unsigned int addr) {
  if (!screen)
    return NULL;
  /* Read from sprite bank 0 (where set_gfx writes to) */
  uint8_t *bank = vimana_sprite_bank(screen, 0);
  if (!bank || addr >= screen->sprite_bank_sizes[0])
    return NULL;
  return bank + addr;
}

void vimana_screen_sprite(vimana_screen *screen, unsigned int ctrl) {
  if (!screen || !screen->layers)
    return;
  unsigned int bank = screen->active_sprite_bank;
  if (!vimana_sprite_bank(screen, bank))
    return;
  const unsigned int rMX = screen->port_auto & 0x1;
  const unsigned int rMY = screen->port_auto & 0x2;
  const unsigned int rMA = screen->port_auto & 0x4;
  const unsigned int rML = vimana_screen_auto_repeat(screen);
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
  const unsigned int layer_is_fg = (layer != 0);
  const unsigned int is_2bpp = ctrl & 0x80;
  const unsigned int is_3bpp = ctrl & 0x100;
  const unsigned int is_4bpp = ctrl & 0x200;
  const unsigned int bytes_per_sprite = is_4bpp ? 32u : (is_3bpp ? 24u : (is_2bpp ? 16u : 8u));
  const unsigned int addr_incr = (rMA >> 2) * bytes_per_sprite;
  const unsigned int stride = (unsigned int)screen->width_mar;
  const int screen_w = (int)screen->width;
  const int screen_h = (int)screen->canvas_height;
  const unsigned int blend = ctrl & 0xf;
  const uint8_t opaque_mask = (uint8_t)(blend % 5);
  int x = (int)screen->port_x;  /* already pixels */
  int y = (int)screen->port_y;
  unsigned int rA = screen->port_addr;

  if (vimana_is_manual_cursor_draw(screen, bank, rA, ctrl, rML)) {
    screen->manual_cursor_pending = true;
    screen->manual_cursor_x = (uint16_t)x;
    screen->manual_cursor_y = (uint16_t)y;
    /* When we capture a manual cursor draw, we need to advance ALL ports exactly as if the sprite had actually drawn, otherwise the auto-increment state is left trailing at the cursor position */
    rA += addr_incr * (rML + 1);
    screen->port_addr = (uint16_t)rA;
    if (rMX)
      screen->port_x += (flipx ? -VIMANA_TILE_SIZE : VIMANA_TILE_SIZE) * (rML + 1);
    if (rMY)
      screen->port_y += (flipy ? -VIMANA_TILE_SIZE : VIMANA_TILE_SIZE) * (rML + 1);
    return;
  }

  for (unsigned int i = 0; i <= rML; i++, x += dx, y += dy, rA += addr_incr) {
    vimana_screen_mark_dirty(screen, x, y, x + VIMANA_TILE_SIZE,
                             y + VIMANA_TILE_SIZE);
    for (unsigned int j = 0; j < 8; j++) {
      int sy = y + (int)j;
      if (sy < 0 || sy >= screen_h)
        continue;
      const unsigned int sr = col_start + j * col_delta;
      const unsigned int a1 = rA + sr;
      const unsigned int a2 = rA + sr + 8;
      const uint8_t ch1 =
          (a1 < VIMANA_SPRITE_BANK_SIZE) ? vimana_sprite_read(screen, bank, a1) : 0;
      const uint8_t ch2 =
          ((is_2bpp || is_3bpp || is_4bpp) && a2 < VIMANA_SPRITE_BANK_SIZE)
              ? vimana_sprite_read(screen, bank, a2)
              : 0;
      const unsigned int a3 = rA + sr + 16;
      const uint8_t ch3 =
          ((is_3bpp || is_4bpp) && a3 < VIMANA_SPRITE_BANK_SIZE)
              ? vimana_sprite_read(screen, bank, a3)
              : 0;
      const unsigned int a4 = rA + sr + 24;
      const uint8_t ch4 =
          (is_4bpp && a4 < VIMANA_SPRITE_BANK_SIZE)
              ? vimana_sprite_read(screen, bank, a4)
              : 0;
      for (unsigned int k = 0, row = row_start; k < 8;
           k++, row += row_delta) {
         int sx = x + (int)k;
         if (sx < 0 || sx >= screen_w)
           continue;
         const unsigned int bit = 1u << row;
         const uint8_t raw_color =
             (uint8_t)((!!(ch1 & bit)) | ((!!(ch2 & bit)) << 1) |
                       ((!!(ch3 & bit)) << 2) | ((!!(ch4 & bit)) << 3));
         const uint8_t wide_color = is_3bpp || is_4bpp;
         const uint8_t color_idx =
             wide_color ? (raw_color & 0x0F) : (raw_color & 0x03);
         if (opaque_mask || color_idx) {
           uint8_t slot_val =
               wide_color ? color_idx
                          : blend_lut[blend][layer_is_fg ? 1 : 0][color_idx];
           uint8_t *d = screen->layers + (size_t)sy * stride + (size_t)sx;
           if (layer_is_fg) {
             uint8_t fg_idx = wide_color ? slot_val : (slot_val >> 2);
             *d = (uint8_t)((*d & 0x0F) | (fg_idx << 4));
           } else {
             *d = (uint8_t)((*d & 0xF0) | (slot_val & 0x0F));
           }
         }
      }
    }
  }

  screen->port_addr = (uint16_t)rA;
  if (rMX)
    screen->port_x += flipx ? -VIMANA_TILE_SIZE : VIMANA_TILE_SIZE;  /* ±1 sprite (8px) */
  if (rMY)
    screen->port_y += flipy ? -VIMANA_TILE_SIZE : VIMANA_TILE_SIZE;
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
      y2 = (unsigned int)screen->canvas_height;
    }
    if (x2 > (unsigned int)screen->width)
      x2 = (unsigned int)screen->width;
    if (y2 > (unsigned int)screen->canvas_height)
      y2 = (unsigned int)screen->canvas_height;
    vimana_screen_mark_dirty(screen, (int)x1, (int)y1, (int)x2, (int)y2);
    for (unsigned int fy = y1; fy < y2; fy++) {
      uint8_t *row = screen->layers + fy * screen->width_mar;
      for (unsigned int fx = x1; fx < x2; fx++)
        row[fx] = (row[fx] & layer_mask) | color;
    }
  } else {
    /* single pixel */
    unsigned int px = screen->port_x;
    unsigned int py = screen->port_y;
    if (px < (unsigned int)screen->width &&
        py < (unsigned int)screen->canvas_height) {
      unsigned int idx = py * (unsigned int)screen->width_mar + px;
      screen->layers[idx] = (screen->layers[idx] & layer_mask) | color;
      vimana_screen_mark_dirty(screen, (int)px, (int)py, (int)px + 1,
                               (int)py + 1);
    }
    if (screen->port_auto & 0x01)
      screen->port_x++;
    if (screen->port_auto & 0x02)
      screen->port_y++;
  }
}

static void vimana_screen_put_glyph_fg(vimana_screen *screen, unsigned int x,
                                       unsigned int y, unsigned int ch,
                                       unsigned int fg) {
  if (!screen || !screen->layers || !screen->font_rom ||
      ch >= VIMANA_GLYPH_COUNT)
    return;
  if (x >= (unsigned int)screen->width || y >= (unsigned int)screen->canvas_height)
    return;
  const uint8_t *bmp = vimana_rom_font_bitmap(screen, ch);
  const uint8_t *widths = vimana_rom_font_widths(screen);
  unsigned int fg_slot = fg & 0x0F;
  unsigned int gh = screen->font_height;
  unsigned int gw = widths[ch];
  if (gw == 0)
    gw = screen->font_glyph_width;
  vimana_screen_mark_dirty(screen, (int)x, (int)y, (int)(x + gw),
                           (int)(y + gh));

  for (unsigned int row = 0; row < gh; row++) {
    unsigned int py = y + row;
    if (py >= (unsigned int)screen->canvas_height)
      continue;
    uint8_t row_data[3];
    vimana_font_row_bytes(screen, bmp, row, row_data);
    for (unsigned int col = 0; col < gw; col++) {
      unsigned int px = x + col;
      if (px >= (unsigned int)screen->width)
        continue;
      uint8_t mask = (uint8_t)(0x80u >> (col & 7));
      if (row_data[col >> 3] & mask)
        screen->layers[py * screen->width_mar + px] = (uint8_t)fg_slot;
    }
  }
}

void vimana_screen_put_icn(vimana_screen *screen, unsigned int x,
                           unsigned int y, const uint8_t rows[8],
                           unsigned int fg, unsigned int bg) {
  if (!screen || !rows)
    return;
  vimana_screen_draw_sprite_1bpp(screen, x, y, rows, fg, bg);
}

void vimana_screen_put_text(vimana_screen *screen, unsigned int x,
                            unsigned int y, const char *text,
                            unsigned int fg, unsigned int bg) {
  if (!screen || !text || !screen->font_rom)
    return;
  if (x >= (unsigned int)screen->width || y >= (unsigned int)screen->canvas_height)
    return;
  const uint8_t *widths = vimana_rom_font_widths(screen);
  bool any_char = false;
  unsigned int cx = x;

  for (int i = 0; text[i] != 0; i++) {
    unsigned int ch = (unsigned char)text[i];
    unsigned int advance;
    if (ch >= VIMANA_GLYPH_COUNT)
      ch = ' ';
    advance = widths[ch];
    if (advance == 0)
      advance = screen->font_glyph_width;
    if (advance > 0)
      any_char = true;
    cx += advance;
  }

   if (any_char && screen->layers && screen->width > 0 &&
       screen->canvas_height > 0 && cx > x) {
     unsigned int bg_slot = bg & 0x0F;
     unsigned int left = x;
     unsigned int right = cx - 1;
     unsigned int top = y;
     unsigned int bottom = y + screen->font_height - 1;
     if (right >= (unsigned int)screen->width)
       right = (unsigned int)screen->width - 1;
     if (bottom > (unsigned int)screen->canvas_height)
       bottom = (unsigned int)screen->canvas_height - 1;
     vimana_screen_mark_dirty(screen, (int)left, (int)top,
                              (int)right + 1, (int)bottom);
     for (unsigned int py = top; py < bottom; py++) {
       uint8_t *dst = screen->layers + py * screen->width_mar + left;
       for (unsigned int px = left; px <= right; px++)
         *dst = (uint8_t)bg_slot, dst++;
     }
   }

  cx = x;
  for (int i = 0; text[i] != 0; i++) {
    unsigned int ch = (unsigned char)text[i];
    unsigned int advance;
    if (ch >= VIMANA_GLYPH_COUNT)
      ch = ' ';
    advance = widths[ch];
    if (advance == 0)
      advance = screen->font_glyph_width;
    vimana_screen_put_glyph_fg(screen, cx, y, ch, fg);
    cx += advance;
  }
}

void vimana_screen_present(vimana_screen *screen) {
  if (!screen || !screen->renderer || !screen->layers)
    return;
  if (!screen->dirty && !screen->manual_cursor_pending) {
    screen->frame_changed = false;
    return;
  }
  screen->frame_changed = true;

  int w = screen->width;
  int h = screen->canvas_height;
  if (screen->canvas_texture) {
    if (screen->dirty) {
      SDL_Rect rect = {
        screen->dirty_x1,
        screen->dirty_y1,
        screen->dirty_x2 - screen->dirty_x1,
        screen->dirty_y2 - screen->dirty_y1
      };
      void *pixels = NULL;
      int pitch = 0;
      if (SDL_LockTexture(screen->canvas_texture, &rect, &pixels, &pitch)) {
        for (int py = 0; py < rect.h; py++) {
          const uint8_t *src = screen->layers +
              (size_t)(rect.y + py) * screen->width_mar + rect.x;
          uint32_t *dst = (uint32_t *)((uint8_t *)pixels +
              (size_t)py * (size_t)pitch);
          for (int px = 0; px < rect.w; px++)
            dst[px] = screen->palette[src[px]];
        }
        SDL_UnlockTexture(screen->canvas_texture);
        screen->dirty = false;
      }
    }
    SDL_FRect dst = {0.0f, 0.0f, (float)(w * screen->scale),
                     (float)(h * screen->scale)};
    SDL_RenderTexture(screen->renderer, screen->canvas_texture, NULL, &dst);
  } else {
    uint32_t bg_rgb = screen->base_colors[0];
    SDL_SetRenderDrawColor(screen->renderer,
                           (uint8_t)((bg_rgb >> 16) & 0xFF),
                           (uint8_t)((bg_rgb >> 8) & 0xFF),
                           (uint8_t)(bg_rgb & 0xFF), 255);
    SDL_RenderClear(screen->renderer);
    uint32_t current_rgb = UINT32_MAX;
    for (int py = 0; py < h; py++) {
      const uint8_t *src = screen->layers + py * screen->width_mar;
      int run_start = 0;
      while (run_start < w) {
        uint8_t slot = src[run_start];
        int run_end = run_start + 1;
        while (run_end < w && src[run_end] == slot)
          run_end++;
        uint32_t rgb = screen->palette[slot];
        if (rgb != current_rgb) {
          SDL_SetRenderDrawColor(screen->renderer,
                                 (uint8_t)((rgb >> 16) & 0xFF),
                                 (uint8_t)((rgb >> 8) & 0xFF),
                                 (uint8_t)(rgb & 0xFF), 255);
          current_rgb = rgb;
        }
        SDL_FRect rect = {(float)(run_start * screen->scale),
                          (float)(py * screen->scale),
                          (float)((run_end - run_start) * screen->scale),
                          (float)screen->scale};
        SDL_RenderFillRect(screen->renderer, &rect);
        run_start = run_end;
      }
    }
    screen->dirty = false;
  }

  /* 1px window border using stripe color */
  {
    uint32_t stripe = screen->base_colors[1];
    int win_w = w * screen->scale;
    int win_h = h * screen->scale;
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

  vimana_draw_manual_cursor_overlay(screen);

#ifdef __APPLE__
  vimana_render_present_with_autoreleasepool(screen->renderer);
#else
  SDL_RenderPresent(screen->renderer);
#endif
}



void vimana_screen_set_drag_region(vimana_screen *screen, unsigned int h) {
  if (!screen)
    return;
  screen->drag_region_height = (int16_t)h;
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

void vimana_screen_set_scale(vimana_screen *screen, unsigned int scale) {
  if (!screen)
    return;
  if (scale < 1)
    scale = 1;
  if (scale > UINT8_MAX)
    scale = UINT8_MAX;
  screen->scale = scale;

  /* Resize window to match new scale. */
  if (screen->window) {
    int win_w = (int)((int64_t)screen->width * scale);
    int win_h = (int)((int64_t)screen->canvas_height * scale);
    SDL_SetWindowSize(screen->window, win_w, win_h);
  }
}

void vimana_screen_set_cursor(vimana_screen *screen, const uint8_t rows[8]) {
  if (!screen || !rows)
    return;
  memcpy(screen->cursor_icn, rows, 8);
  screen->cursor_dirty = true;
}

void vimana_screen_hide_cursor(vimana_screen *screen) {
  if (!screen)
    return;
  screen->cursor_visible = false;
}

void vimana_screen_show_cursor(vimana_screen *screen) {
  if (!screen)
    return;
  screen->cursor_visible = true;
}

void vimana_screen_free(vimana_screen *screen) {
  if (!screen)
    return;
  if (screen->canvas_texture)
    SDL_DestroyTexture(screen->canvas_texture);
  if (screen->renderer)
    SDL_DestroyRenderer(screen->renderer);
  if (screen->window)
    SDL_DestroyWindow(screen->window);
  if (screen->font_rom_owned)
    free(screen->font_rom);
  for (int i = 0; i < VIMANA_SPRITE_BANK_COUNT; i++)
    free(screen->sprite_banks[i]);
  free(screen);
}

size_t vimana_screen_ram_usage(vimana_screen *screen) {
  if (!screen)
    return 0;
  size_t total = sizeof(*screen);
  if (screen->font_rom_owned)
    total += screen->font_rom_size;
  total += (size_t)screen->width_mar * (size_t)screen->height_mar;
  if (screen->title)
    total += strlen(screen->title) + 1;
  for (int i = 0; i < VIMANA_SPRITE_BANK_COUNT; i++)
    if (screen->sprite_banks[i])
      total += screen->sprite_bank_sizes[i];
  return total;
}

/* ── Device API ─────────────────────────────────────────────────────────── */

void vimana_device_poll(vimana_system *system) { (void)system; }

unsigned int vimana_device_controller(vimana_system *system) {
  vimana_system_ensure_gamepad(system);
  return vimana_controller_bits(system);
}

bool vimana_device_controller_down(vimana_system *system, unsigned int mask) {
  vimana_system_ensure_gamepad(system);
  uint8_t bits = vimana_controller_bits(system);
  return ((unsigned int)bits & mask) != 0;
}

bool vimana_device_controller_pressed(vimana_system *system,
                                      unsigned int mask) {
  if (!system)
    return false;
  vimana_system_ensure_gamepad(system);
  return ((unsigned int)system->controller_pressed & mask) != 0;
}

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
  if (!system)
    return "";
  system->text_input_requested = true;
  return system->text_input;
}

/* ── Console API ────────────────────────────────────────────────────────── */

bool vimana_console_pending(vimana_system *system) {
  return system && system->console_len > 0;
}

int vimana_console_input(vimana_system *system) {
  if (!system || system->console_len == 0)
    return 0;
  return (int)system->console_bytes[system->console_head];
}

int vimana_console_type(vimana_system *system) {
  if (!system || system->console_len == 0)
    return 0;
  return (int)system->console_types[system->console_head];
}

void vimana_console_next(vimana_system *system) {
  if (!system || system->console_len == 0)
    return;
  system->console_head =
      (uint16_t)((system->console_head + 1) % VIMANA_CONSOLE_CAP);
  system->console_len--;
  if (system->console_len == 0)
    system->console_head = 0;
}

bool vimana_console_push(vimana_system *system, int byte, int type) {
  return vimana_console_enqueue(system, (uint8_t)(byte & 0xff),
                                (uint8_t)(type & 0xff));
}

void vimana_console_stdout(vimana_system *system, int byte) {
  (void)system;
  fputc(byte & 0xff, stdout);
  fflush(stdout);
}

void vimana_console_stderr(vimana_system *system, int byte) {
  (void)system;
  fputc(byte & 0xff, stderr);
  fflush(stderr);
}

void vimana_console_stderr_hex(vimana_system *system, int byte) {
  (void)system;
  fprintf(stderr, "%02x", byte & 0xff);
  fflush(stderr);
}

/* ── Datetime API ───────────────────────────────────────────────────────── */

typedef enum VimanaDatetimePart {
  VIMANA_DT_YEAR = 0,
  VIMANA_DT_MONTH,
  VIMANA_DT_DAY,
  VIMANA_DT_HOUR,
  VIMANA_DT_MINUTE,
  VIMANA_DT_SECOND,
  VIMANA_DT_WEEKDAY,
  VIMANA_DT_YDAY,
  VIMANA_DT_DST,
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
  case VIMANA_DT_YDAY:
    return tmv.tm_yday;
  case VIMANA_DT_DST:
    return tmv.tm_isdst;
  default:
    return 0;
  }
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

int vimana_datetime_yday(vimana_system *system) {
  return vimana_datetime_yday_at(system, vimana_datetime_now_value());
}

int vimana_datetime_dst(vimana_system *system) {
  return vimana_datetime_dst_at(system, vimana_datetime_now_value());
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

int vimana_datetime_yday_at(vimana_system *system, int64_t timestamp) {
  (void)system;
  return vimana_datetime_part_value(timestamp, VIMANA_DT_YDAY);
}

int vimana_datetime_dst_at(vimana_system *system, int64_t timestamp) {
  (void)system;
  return vimana_datetime_part_value(timestamp, VIMANA_DT_DST);
}

/* ── File API ───────────────────────────────────────────────────────────── */

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
  (void)system;
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
  char *text = (char *)malloc((size_t)size + 1);
  if (!text) {
    fclose(fp);
    return NULL;
  }
  size_t wanted = (size_t)size;
  size_t nread = wanted > 0 ? fread(text, 1, wanted, fp) : 0;
  bool failed = wanted > 0 && nread != wanted && ferror(fp);
  fclose(fp);
  if (failed) {
    free(text);
    return NULL;
  }
  text[nread] = 0;
  return text;
}

bool vimana_file_write_bytes(vimana_system *system, const char *path,
                             const unsigned char *bytes, size_t size) {
  (void)system;
  if (!path || !path[0])
    return false;
  if (size > 0 && !bytes)
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

/* ── Process API ────────────────────────────────────────────────────────── */

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

  int pipe_in[2] = {-1, -1};   /* parent writes child stdin */
  int pipe_out[2] = {-1, -1};  /* parent reads child stdout */
  if (pipe(pipe_in) != 0)
    return NULL;
  if (pipe(pipe_out) != 0) {
    close(pipe_in[0]);
    close(pipe_in[1]);
    return NULL;
  }

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
    kill(pid, SIGKILL);
    int status;
    waitpid(pid, &status, 0);
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
  size_t done = 0;
  while (done < len) {
    ssize_t written = write(proc->stdin_fd, text + done, len - done);
    if (written < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    if (written == 0)
      return false;
    done += (size_t)written;
  }
  return true;
}

static char *vimana_process_take_line(vimana_process *proc, int len,
                                      int skip) {
  if (!proc || len < 0 || skip < len || len > proc->line_len ||
      skip > proc->line_len)
    return NULL;
  char *line = (char *)malloc((size_t)len + 1);
  if (!line)
    return NULL;
  memcpy(line, proc->line_buf, (size_t)len);
  line[len] = '\0';
  int rest = proc->line_len - skip;
  if (rest > 0)
    memmove(proc->line_buf, proc->line_buf + skip, (size_t)rest);
  proc->line_len = rest;
  return line;
}

static char *vimana_process_extract_line(vimana_process *proc) {
  if (!proc)
    return NULL;
  for (int i = 0; i < proc->line_len; i++) {
    if (proc->line_buf[i] == '\n')
      return vimana_process_take_line(proc, i, i + 1);
  }
  return NULL;
}

char *vimana_process_read_line(vimana_process *proc) {
  if (!proc || proc->stdout_fd < 0)
    return NULL;

  char *line = vimana_process_extract_line(proc);
  if (line)
    return line;

  /* try to read more (non-blocking) */
  int space = VIMANA_PROC_LINE_CAP - proc->line_len - 1;
  if (space <= 0)
    return vimana_process_take_line(proc, proc->line_len, proc->line_len);
  ssize_t n = read(proc->stdout_fd, proc->line_buf + proc->line_len,
                   (size_t)space);
  if (n > 0) {
    proc->line_len += (int)n;
    return vimana_process_extract_line(proc);
  }
  if (n == 0 && proc->line_len > 0)
    return vimana_process_take_line(proc, proc->line_len, proc->line_len);
  if (n < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK &&
      proc->line_len > 0)
    return vimana_process_take_line(proc, proc->line_len, proc->line_len);
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
