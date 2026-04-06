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

#define VIMANA_TITLEBAR_HEIGHT 24
#define VIMANA_TITLEBAR_BAR_HEIGHT 21
#define VIMANA_TB_BOX_SIZE 13  /* System 7 close/button box size (pixels) */
#define VIMANA_TB_BOX_Y    4   /* y offset of box within bar: (21-13)/2 */
#define VIMANA_TB_CLOSE_X  4   /* close box left edge x */

#define VIMANA_KEY_CAP 512
#define VIMANA_MOUSE_CAP 8
#define VIMANA_TILE_SIZE 8
#define VIMANA_GLYPH_HEIGHT 16
#define VIMANA_UF2_BYTES 32  /* per-glyph: 2 tiles × 16 bytes (2bpp planar) */
#define VIMANA_FONT_ROW_BYTES 3   /* max 24px wide = 3 bytes per row */
#define VIMANA_FONT_MAX_HEIGHT 24
#define VIMANA_FONT_GLYPH_BYTES (VIMANA_FONT_MAX_HEIGHT * VIMANA_FONT_ROW_BYTES) /* 72 */
#define VIMANA_TEXT_INPUT_CAP 256
#define VIMANA_AUDIO_SAMPLE_RATE 44100
#define VIMANA_AUDIO_CHANNELS 1
#define VIMANA_SPRITE_1BPP_BYTES VIMANA_TILE_SIZE
#define VIMANA_SPRITE_2BPP_BYTES (VIMANA_TILE_SIZE * 2)
#define VIMANA_SPRITE_MEM_CAP 4192
#define VIMANA_RAM_CAP 0x20000  /* 131 072 bytes (128 KB) hard limit */
#define VIMANA_KEY_WORDS (VIMANA_KEY_CAP / 64)  /* 8 uint64_t = 512 bits */

/* uxn2 blend look-up table: blend_lut[mode][layer(0=bg,1=fg)][color] */
static const uint8_t blend_lut[16][2][4] = {
    {{0, 0, 1, 2}, {0, 0, 4, 8}},
    {{0, 1, 2, 3}, {0, 4, 8, 12}},
    {{0, 2, 3, 1}, {0, 8, 12, 4}},
    {{0, 3, 1, 2}, {0, 12, 4, 8}},
    {{1, 0, 1, 2}, {4, 0, 4, 8}},
    {{1, 1, 2, 3}, {4, 4, 8, 12}},
    {{1, 2, 3, 1}, {4, 8, 12, 4}},
    {{1, 3, 1, 2}, {4, 12, 4, 8}},
    {{2, 0, 1, 2}, {8, 0, 4, 8}},
    {{2, 1, 2, 3}, {8, 4, 8, 12}},
    {{2, 2, 3, 1}, {8, 8, 12, 4}},
    {{2, 3, 1, 2}, {8, 12, 4, 8}},
    {{3, 0, 1, 2}, {12, 0, 4, 8}},
    {{3, 1, 2, 3}, {12, 4, 8, 12}},
    {{3, 2, 3, 1}, {12, 8, 12, 4}},
    {{3, 3, 1, 2}, {12, 12, 4, 8}}};

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
  float tone_phase;
  float tone_freq_hz;
  float tone_amp;
  int tone_samples_left;
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
    wanted = (int)(sizeof(float) * 256);
  int sample_count = wanted / (int)sizeof(float);
  if (sample_count <= 0)
    sample_count = 256;

  float *buffer = (float *)calloc((size_t)sample_count, sizeof(float));
  if (!buffer)
    return;

  float phase = system->tone_phase;
  float freq = system->tone_freq_hz;
  float amp = system->tone_amp;
  int left = system->tone_samples_left;
  float step = (system->audio_sample_rate > 0)
                   ? (freq / (float)system->audio_sample_rate)
                   : 0.0f;
  int tail = system->audio_sample_rate / 200; /* ~5ms release ramp */
  if (tail < 1)
    tail = 1;

  for (int i = 0; i < sample_count; i++) {
    float s = 0.0f;
    if (left > 0 && step > 0.0f && amp > 0.0f) {
      float env = (left < tail) ? ((float)left / (float)tail) : 1.0f;
      s = ((phase < 0.5f) ? 1.0f : -1.0f) * amp * env;
      left -= 1;
      phase += step;
      if (phase >= 1.0f)
        phase -= floorf(phase);
    }
    buffer[i] = s;
  }

  system->tone_phase = phase;
  system->tone_samples_left = left;
  (void)SDL_PutAudioStreamData(stream, buffer,
                               sample_count * (int)sizeof(float));
  free(buffer);
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
  system->tone_phase = 0.0f;
  system->tone_freq_hz = 0.0f;
  system->tone_amp = 0.0f;
  system->tone_samples_left = 0;
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

struct VimanaScreen {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  char *title;
  unsigned int width;
  unsigned int height;
  unsigned int scale;
  unsigned int width_mar;   /* width + 16 (8px margin each side) */
  unsigned int height_mar;  /* height + 16 */
  uint32_t base_colors[4];  /* ARGB palette, slots 0-3 */
  uint32_t palette[16];     /* expanded 16-entry composite */
  /* ROM-class data: font glyphs, sprite sheet (loaded once) */
  uint8_t font_bitmap[128][VIMANA_FONT_GLYPH_BYTES];
  uint8_t font_ascii[128][VIMANA_UF2_BYTES];
  uint8_t font_widths[128];
  uint8_t sprite_mem[VIMANA_SPRITE_MEM_CAP];
  /* RAM-class state */
  uint16_t port_x;
  uint16_t port_y;
  uint16_t port_addr;
  uint8_t port_auto;
  uint8_t font_height;       /* UF1=8, UF2=16, UF3=24 */
  uint8_t font_glyph_width;  /* bitmap pixel width: 8 (UF1/UF2) or 24 (UF3) */
  uint8_t *layers;    /* width_mar * height_mar bytes: bits[1:0]=bg, bits[3:2]=fg */
  char *titlebar_title;          /* NULL = use screen->title */
  time_t theme_mtime;            /* last known mtime of ~/.theme */
  int16_t titlebar_height;       /* total titlebar height: font_height + 5 */
  int16_t titlebar_bar_height;   /* drawn bar height: font_height + 5 */
  int16_t titlebar_box_size;     /* close/button box size: min(13, bar_height-4) */
  int16_t titlebar_box_y;        /* y of close/button box within bar */
  uint8_t titlebar_bg_slot;      /* palette slot for titlebar background, default 0 */
  bool titlebar_has_button;      /* show optional right button */
  bool titlebar_button_pressed;  /* set for one frame when right button clicked */
  bool theme_swap_fg_bg;         /* swap base_colors[0] and [1] after loading */
  int16_t theme_poll_counter;    /* frame counter for periodic theme check */
};

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
  for (unsigned int i = 0; i < 16; i++)
    screen->palette[i] = screen->base_colors[i >> 2 ? i >> 2 : i & 3];
}

static void vimana_screen_reset_palette(vimana_screen *screen) {
  if (!screen)
    return;
  screen->base_colors[0] = 0xFFFFFFFFu;
  screen->base_colors[1] = 0xFF000000u;
  screen->base_colors[2] = 0xFF77DDCCu;
  screen->base_colors[3] = 0xFFFFBB22u;
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
  screen->titlebar_bar_height = fh + 5;
  screen->titlebar_height     = fh + 5;
  int bsz_max = VIMANA_TB_BOX_SIZE + (screen->titlebar_bar_height > 21 ? 2 : 0);
  int bsz = bsz_max < screen->titlebar_bar_height - 4
            ? bsz_max : screen->titlebar_bar_height - 4;
  screen->titlebar_box_size   = bsz;
  screen->titlebar_box_y      = (screen->titlebar_bar_height - bsz) / 2;
}

static void vimana_screen_reset_font(vimana_screen *screen) {
  if (!screen)
    return;
  memset(screen->font_bitmap, 0, sizeof(screen->font_bitmap));
  memset(screen->font_ascii, 0, sizeof(screen->font_ascii));
  memset(screen->font_widths, 0, sizeof(screen->font_widths));
  for (int i = 0; i < 128; i++)
    screen->font_widths[i] = VIMANA_TILE_SIZE;
  screen->font_height = VIMANA_TILE_SIZE; /* UF1 default */
  screen->font_glyph_width = VIMANA_TILE_SIZE;
  memset(screen->sprite_mem, 0, sizeof(screen->sprite_mem));
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
  return mode ? VIMANA_SPRITE_2BPP_BYTES : VIMANA_SPRITE_1BPP_BYTES;
}

static unsigned int vimana_screen_addr_clamp(vimana_screen *screen,
                                              unsigned int addr) {
  (void)screen;
  if (addr >= VIMANA_SPRITE_MEM_CAP)
    return VIMANA_SPRITE_MEM_CAP - 1;
  return addr;
}

static void vimana_screen_store_sprite_bytes(vimana_screen *screen,
                                             unsigned int addr,
                                             const uint8_t *sprite,
                                             unsigned int mode) {
  if (!screen || !sprite)
    return;
  unsigned int stride = vimana_sprite_stride(mode);
  if (addr + stride > VIMANA_SPRITE_MEM_CAP)
    return;
  memcpy(screen->sprite_mem + addr, sprite, (size_t)stride);
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
        /* System 7 titlebar button clicks */
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
  float amp = ((float)volume / 15.0f) * 0.25f;

  (void)SDL_LockAudioStream(system->audio_stream);
  system->tone_freq_hz = freq;
  system->tone_amp = amp;
  system->tone_samples_left = total_samples;
  (void)SDL_UnlockAudioStream(system->audio_stream);
  (void)SDL_ResumeAudioStreamDevice(system->audio_stream);
}

void vimana_system_run(vimana_system *system, vimana_screen *screen,
                       vimana_frame_fn frame, void *user) {
  if (!system || !screen || !screen->window || !screen->renderer)
    return;
  system->quit = false;
  system->running = true;
  SDL_StartTextInput(screen->window);
  while (!system->quit) {
    vimana_reset_pressed(system);
    vimana_pump_events(system, screen);
    vimana_screen_poll_theme(screen);
    if (frame)
      frame(system, screen, user);
    if (!SDL_HasEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST))
      SDL_WaitEventTimeout(NULL, 16);
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

  screen->width = width;
  screen->height = height;
  screen->scale = scale;
  screen->width_mar = width + 16;
  screen->height_mar = height + 16;
  screen->title = strdup(title ? title : "vimana");
  screen->titlebar_bg_slot = 2; /* default to palette slot 2 (accent color) */
  vimana_screen_reset_palette(screen);
  vimana_screen_load_theme(screen);
  vimana_screen_reset_font(screen);
  vimana_screen_reset_ports(screen);

  screen->layers =
      (uint8_t *)calloc((size_t)screen->width_mar * (size_t)screen->height_mar,
                        sizeof(uint8_t));
  if (!screen->layers) {
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
    free(screen->title);
    free(screen);
    return NULL;
  }

  screen->renderer = SDL_CreateRenderer(screen->window, NULL);
  if (!screen->renderer) {
    SDL_DestroyWindow(screen->window);
    free(screen->layers);
    free(screen->title);
    free(screen);
    return NULL;
  }

  screen->texture =
      SDL_CreateTexture(screen->renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!screen->texture) {
    SDL_DestroyRenderer(screen->renderer);
    SDL_DestroyWindow(screen->window);
    free(screen->layers);
    free(screen->title);
    free(screen);
    return NULL;
  }
  SDL_SetTextureScaleMode(screen->texture, SDL_SCALEMODE_NEAREST);
  SDL_SetWindowHitTest(screen->window, vimana_hit_test, screen);

  vimana_screen_clear(screen, 0);
  return screen;
}

void vimana_screen_clear(vimana_screen *screen, unsigned int bg) {
  if (!screen || !screen->layers)
    return;
  uint8_t bg_val = (uint8_t)(bg & 0x3);
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
      SDL_SetTextureScaleMode(screen->texture, SDL_SCALEMODE_NEAREST);
  }
}

void vimana_screen_set_palette(vimana_screen *screen, unsigned int slot,
                               const char *hex) {
  if (!screen)
    return;
  unsigned int s = slot & 3;
  screen->base_colors[s] = vimana_parse_hex_color(hex);
  vimana_colorize(screen);
}

void vimana_screen_set_font_glyph(vimana_screen *screen, unsigned int code,
                                  const uint16_t rows[16]) {
  if (!screen || !rows)
    return;
  if (code >= 128)
    return;
  /* UF2 tileset storage */
  vimana_rows_to_uf2(rows, screen->font_ascii[code]);
  /* 1bpp bitmap: OR both planes together */
  uint8_t *bmp = screen->font_bitmap[code];
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
  if (!screen || !chr)
    return;
  if (code >= 128)
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
  uint8_t *bmp = screen->font_bitmap[code];
  memset(bmp, 0, VIMANA_FONT_GLYPH_BYTES);
  for (unsigned int i = 0; i < n; i++) {
    unsigned int row = i / bpr;
    unsigned int col_byte = i % bpr;
    bmp[row * VIMANA_FONT_ROW_BYTES + col_byte] = chr[i];
  }
  /* UF2 tileset storage (for 8px-wide glyphs only) */
  if (gw <= 8) {
    uint8_t *uf2 = screen->font_ascii[code];
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
  if (!screen || code >= 128)
    return;
  screen->font_widths[code] = (uint8_t)(width > 0 && width <= 255 ? width : VIMANA_TILE_SIZE);
}

void vimana_screen_set_font_size(vimana_screen *screen, unsigned int size) {
  if (!screen)
    return;
  int w;
  if (size == 3) {
    screen->font_height = VIMANA_FONT_MAX_HEIGHT;   /* 24 */
    w = VIMANA_FONT_MAX_HEIGHT;                      /* 24 */
  } else if (size == 2) {
    screen->font_height = VIMANA_GLYPH_HEIGHT;      /* 16 */
    w = VIMANA_TILE_SIZE;                            /*  8 */
  } else {
    screen->font_height = VIMANA_TILE_SIZE;          /*  8 */
    w = VIMANA_TILE_SIZE;                            /*  8 */
  }
  screen->font_glyph_width = w;
  for (unsigned int i = 0; i < 128; i++)
    screen->font_widths[i] = (uint8_t)w;
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
      screen, vimana_screen_addr_clamp(screen, addr), sprite, mode != 0);
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

void vimana_screen_sprite(vimana_screen *screen, unsigned int ctrl) {
  if (!screen || !screen->layers)
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
  const unsigned int layer_mask = layer ? 0x3 : 0xc;
  const unsigned int is_2bpp = ctrl & 0x80;
  const unsigned int addr_incr = rMA << (is_2bpp ? 2 : 1);
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
          (a1 < VIMANA_SPRITE_MEM_CAP) ? screen->sprite_mem[a1] : 0;
      const uint8_t ch2 =
          (is_2bpp && a2 < VIMANA_SPRITE_MEM_CAP)
              ? screen->sprite_mem[a2]
              : 0;
      uint8_t *d = dst_row;
      for (unsigned int k = 0, row = row_start; k < 8;
           k++, d++, row += row_delta) {
        const unsigned int bit = 1u << row;
        const uint8_t color =
            (uint8_t)((!!(ch1 & bit)) | ((!!(ch2 & bit)) << 1));
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
  const uint8_t layer_mask = (uint8_t)(layer ? 0x3 : 0xc);
  const uint8_t color =
      layer ? (uint8_t)((ctrl & 0x3) << 2) : (uint8_t)(ctrl & 0x3);
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
  if (!screen || !screen->layers)
    return;
  unsigned char ch = (unsigned char)((glyph && glyph[0]) ? glyph[0] : ' ');
  if (ch >= 128)
    ch = ' ';
  const uint8_t *bmp = screen->font_bitmap[ch];
  unsigned int bg_slot = bg & 0x3;
  unsigned int fg_slot = fg & 0x3;
  unsigned int gh = screen->font_height;
  unsigned int gw = screen->font_widths[ch];
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
      *dst++ = (uint8_t)((slot << 2) | slot);
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
  unsigned int bg_slot = bg & 0x3;
  unsigned int fg_slot = fg & 0x3;
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
      *dst++ = (uint8_t)((slot << 2) | slot);
    }
  }
}

void vimana_screen_put_text(vimana_screen *screen, unsigned int x,
                            unsigned int y, const char *text,
                            unsigned int fg, unsigned int bg) {
  if (!screen || !text)
    return;
  unsigned int cx = x;
  for (int i = 0; text[i] != 0; i++) {
    unsigned char ch = (unsigned char)text[i];
    if (ch >= 128) ch = ' ';
    vimana_screen_put(screen, cx, y, &text[i], fg, bg);
    cx += screen->font_widths[ch];
  }
}

static void vimana_draw_titlebar_content(vimana_screen *screen) {
  if (!screen || !screen->renderer)
    return;
  SDL_Renderer *rend = screen->renderer;
  int win_w = screen->width * screen->scale;
  uint32_t bg = screen->base_colors[screen->titlebar_bg_slot & 3];

  /* Stripe and contrast colors from the 2-bit palette */
  uint32_t stripe   = screen->base_colors[1];
  uint32_t contrast = screen->base_colors[1];

  /* Pinstripe background: solid bg fill, then stripes with top/bottom margins */
  SDL_SetRenderDrawColor(rend, (uint8_t)((bg >> 16) & 0xFF),
                         (uint8_t)((bg >> 8) & 0xFF), (uint8_t)(bg & 0xFF), 255);
  SDL_FRect bar_bg = {0.0f, 0.0f, (float)win_w, (float)screen->titlebar_height};
  SDL_RenderFillRect(rend, &bar_bg);
  int stripe_top = 3;
  int stripe_bot = screen->titlebar_bar_height - 3;
  SDL_SetRenderDrawColor(rend, (uint8_t)((stripe >> 16) & 0xFF),
                         (uint8_t)((stripe >> 8) & 0xFF), (uint8_t)(stripe & 0xFF), 255);
  for (int row = stripe_top; row < stripe_bot; row++) {
    if (row & 1) {
      SDL_FRect line = {0.0f, (float)row, (float)(win_w - 4), 1.0f};
      SDL_RenderFillRect(rend, &line);
    }
  }

  /* Close box (left side, System 6 style: halo gap around box) */
  {
    int bx = VIMANA_TB_CLOSE_X, by = screen->titlebar_box_y, bsz = screen->titlebar_box_size;
    /* Clear halo around close box — clamped so it never bleeds outside bar height
       and always covers the full stripe zone (stripe_top..stripe_bot). */
    int halo_y   = (by - 4 < stripe_top) ? by - 4 : stripe_top;
    if (halo_y < 0) halo_y = 0;
    int halo_bot = (by + bsz + 4 > stripe_bot) ? by + bsz + 4 : stripe_bot;
    if (halo_bot > screen->titlebar_bar_height) halo_bot = screen->titlebar_bar_height;
    SDL_SetRenderDrawColor(rend, (uint8_t)((bg >> 16) & 0xFF),
                           (uint8_t)((bg >> 8) & 0xFF), (uint8_t)(bg & 0xFF), 255);
    SDL_FRect halo = {(float)(bx - 4), (float)halo_y,
                      (float)(bsz + 8), (float)(halo_bot - halo_y)};
    SDL_RenderFillRect(rend, &halo);
    /* 1px border */
    SDL_SetRenderDrawColor(rend, (uint8_t)((contrast >> 16) & 0xFF),
                           (uint8_t)((contrast >> 8) & 0xFF),
                           (uint8_t)(contrast & 0xFF), 255);
    SDL_FRect top    = {(float)bx,           (float)by,            (float)bsz, 1.0f};
    SDL_FRect bot    = {(float)bx,           (float)(by + bsz - 1),(float)bsz, 1.0f};
    SDL_FRect left   = {(float)bx,           (float)by,            1.0f, (float)bsz};
    SDL_FRect right  = {(float)(bx + bsz-1), (float)by,            1.0f, (float)bsz};
    SDL_RenderFillRect(rend, &top);
    SDL_RenderFillRect(rend, &bot);
    SDL_RenderFillRect(rend, &left);
    SDL_RenderFillRect(rend, &right);
  }

  /* Optional right button (zoom-box style: halo gap + outer box + inner offset box) */
  if (screen->titlebar_has_button) {
    int bx = win_w - VIMANA_TB_CLOSE_X - screen->titlebar_box_size;
    int by = screen->titlebar_box_y, bsz = screen->titlebar_box_size;
    /* Clear halo around button — same clamping as close box */
    int bhalo_y   = (by - 4 < stripe_top) ? by - 4 : stripe_top;
    if (bhalo_y < 0) bhalo_y = 0;
    int bhalo_bot = (by + bsz + 4 > stripe_bot) ? by + bsz + 4 : stripe_bot;
    if (bhalo_bot > screen->titlebar_bar_height) bhalo_bot = screen->titlebar_bar_height;
    SDL_SetRenderDrawColor(rend, (uint8_t)((bg >> 16) & 0xFF),
                           (uint8_t)((bg >> 8) & 0xFF), (uint8_t)(bg & 0xFF), 255);
    SDL_FRect halo = {(float)(bx - 4), (float)bhalo_y,
                      (float)(bsz + 8), (float)(bhalo_bot - bhalo_y)};
    SDL_RenderFillRect(rend, &halo);
    SDL_SetRenderDrawColor(rend, (uint8_t)((contrast >> 16) & 0xFF),
                           (uint8_t)((contrast >> 8) & 0xFF),
                           (uint8_t)(contrast & 0xFF), 255);
    /* Outer border */
    SDL_FRect t  = {(float)bx,           (float)by,            (float)bsz, 1.0f};
    SDL_FRect b  = {(float)bx,           (float)(by + bsz - 1),(float)bsz, 1.0f};
    SDL_FRect l  = {(float)bx,           (float)by,            1.0f, (float)bsz};
    SDL_FRect r  = {(float)(bx + bsz-1), (float)by,            1.0f, (float)bsz};
    SDL_RenderFillRect(rend, &t);
    SDL_RenderFillRect(rend, &b);
    SDL_RenderFillRect(rend, &l);
    SDL_RenderFillRect(rend, &r);
    /* Inner offset box (zoom box indicator): top-right quadrant */
    int ix = bx + 4, iy = by + 2, isz = bsz - 6;
    SDL_FRect it = {(float)ix,           (float)iy,            (float)isz, 1.0f};
    SDL_FRect ib = {(float)ix,           (float)(iy + isz - 1),(float)isz, 1.0f};
    SDL_FRect il = {(float)ix,           (float)iy,            1.0f, (float)isz};
    SDL_FRect ir = {(float)(ix + isz-1), (float)iy,            1.0f, (float)isz};
    SDL_RenderFillRect(rend, &it);
    SDL_RenderFillRect(rend, &ib);
    SDL_RenderFillRect(rend, &il);
    SDL_RenderFillRect(rend, &ir);
  }

  /* Title text: centered in the full titlebar width (with safety clamps for boxes). */
  const char *title = screen->titlebar_title ? screen->titlebar_title : screen->title;
  if (title && title[0]) {
    int len = (int)strlen(title);
    /* Compute total text width using font_widths */
    int text_w = 0;
    for (int i = 0; i < len; i++) {
      unsigned char ch = (unsigned char)title[i];
      text_w += (ch < 128) ? screen->font_widths[ch] : VIMANA_TILE_SIZE;
    }
    int left_bound  = VIMANA_TB_CLOSE_X + screen->titlebar_box_size + 4;
    int right_bound = screen->titlebar_has_button
                        ? win_w - VIMANA_TB_CLOSE_X - screen->titlebar_box_size - 4
                        : win_w - 4;
    int gh = screen->font_height;
    int text_y = (screen->titlebar_bar_height - gh + 1) / 2;
    int text_x = (win_w - text_w) / 2;
    text_x -= 1; /* optical nudge toward the close button */
    if (text_x < left_bound)
      text_x = left_bound;
    if (text_x + text_w > right_bound)
      text_x = right_bound - text_w;
    if (text_x < left_bound)
      text_x = left_bound;
    /* Clear halo behind title text (4px padding) */
    int clipped_w = text_w;
    if (text_x + clipped_w > right_bound)
      clipped_w = right_bound - text_x;
    if (clipped_w > 0) {
      SDL_SetRenderDrawColor(rend, (uint8_t)((bg >> 16) & 0xFF),
                             (uint8_t)((bg >> 8) & 0xFF), (uint8_t)(bg & 0xFF), 255);
      SDL_FRect text_halo = {(float)(text_x - 4), 0.0f,
                             (float)(clipped_w + 8), (float)screen->titlebar_bar_height};
      SDL_RenderFillRect(rend, &text_halo);
    }
    SDL_SetRenderDrawColor(rend, (uint8_t)((contrast >> 16) & 0xFF),
                           (uint8_t)((contrast >> 8) & 0xFF),
                           (uint8_t)(contrast & 0xFF), 255);
    int gx = text_x;
    for (int i = 0; i < len; i++) {
      unsigned char c = (unsigned char)title[i];
      if (c >= 128) { gx += screen->font_widths[' ']; continue; }
      if (gx >= right_bound)
        break;
      const uint8_t *bmp = screen->font_bitmap[c];
      int gw = (int)screen->font_widths[c];
      if (gw < screen->font_glyph_width) gw = screen->font_glyph_width;
      /* Draw 1bpp glyph for titlebar */
      for (int row = 0; row < gh; row++) {
        int py = text_y + row;
        if (py < 0 || py >= screen->titlebar_bar_height) continue;
        const uint8_t *row_data = bmp + row * VIMANA_FONT_ROW_BYTES;
        for (int col = 0; col < gw; col++) {
          uint8_t mask = (uint8_t)(0x80u >> (col & 7));
          if (row_data[col >> 3] & mask) {
            SDL_FRect px = {(float)(gx + col), (float)py, 1.0f, 1.0f};
            SDL_RenderFillRect(rend, &px);
          }
        }
      }
      gx += screen->font_widths[c];
    }
  }

  /* Separator line at bottom edge of bar — drawn last so nothing overdaws it */
  {
    SDL_SetRenderDrawColor(rend, (uint8_t)((contrast >> 16) & 0xFF),
                           (uint8_t)((contrast >> 8) & 0xFF),
                           (uint8_t)(contrast & 0xFF), 255);
    SDL_FRect sep = {0.0f, (float)(screen->titlebar_height - 1), (float)win_w, 1.0f};
    SDL_RenderFillRect(rend, &sep);
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
  screen->titlebar_bg_slot = bg & 3;
}

void vimana_screen_set_titlebar_title(vimana_screen *screen, const char *title) {
  if (!screen)
    return;
  free(screen->titlebar_title);
  screen->titlebar_title = title ? strdup(title) : NULL;
}

void vimana_screen_set_titlebar_button(vimana_screen *screen, bool show) {
  if (!screen)
    return;
  screen->titlebar_has_button = show;
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
    waitpid(proc->pid, &status, 0);
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
  if (system->audio_stream)
    SDL_DestroyAudioStream(system->audio_stream);
  free(system);
}

void vimana_screen_free(vimana_screen *screen) {
  if (!screen)
    return;
  if (screen->texture)
    SDL_DestroyTexture(screen->texture);
  if (screen->renderer)
    SDL_DestroyRenderer(screen->renderer);
  if (screen->window)
    SDL_DestroyWindow(screen->window);
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
  /* RAM-class: mutable user-addressable state only.
     Excludes ROM-class font/sprite arrays and VRAM (layers framebuffer). */
  size_t total = 0;
  total += VIMANA_SPRITE_MEM_CAP;  /* sprite_mem: user-writable tile sheet */
  total += sizeof(screen->port_x) + sizeof(screen->port_y)
         + sizeof(screen->port_addr) + sizeof(screen->port_auto);
  total += sizeof(screen->base_colors) + sizeof(screen->palette);
  return total;
}

