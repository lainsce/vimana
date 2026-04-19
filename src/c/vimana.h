// Vimana — public C API
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Input / Audio ────────────────────────────────────────────────────── */
#define VIMANA_KEY_CAP             512   /* 512 SCs (enough for every key) */
#define VIMANA_KEY_WORDS           (512 / 64)             /* 8 × uint64_t  */
#define VIMANA_MOUSE_CAP           8        /* 8 buttons (enough for mice) */
#define VIMANA_TEXT_INPUT_CAP      256 /* max UTF-8 text input length (Bs) */
#define VIMANA_CONTROLLER_A        0x01     /* controller bit 0         */
#define VIMANA_CONTROLLER_B        0x02     /* controller bit 1         */
#define VIMANA_CONTROLLER_SELECT   0x04     /* controller bit 2         */
#define VIMANA_CONTROLLER_START    0x08     /* controller bit 3         */
#define VIMANA_CONTROLLER_UP       0x10     /* controller bit 4         */
#define VIMANA_CONTROLLER_DOWN     0x20     /* controller bit 5         */
#define VIMANA_CONTROLLER_LEFT     0x40     /* controller bit 6         */
#define VIMANA_CONTROLLER_RIGHT    0x80     /* controller bit 7         */
#define VIMANA_CONSOLE_STD         0x01     /* standard input byte      */
#define VIMANA_CONSOLE_ARG         0x02     /* argv byte                */
#define VIMANA_CONSOLE_EOA         0x03     /* end of argument          */
#define VIMANA_CONSOLE_END         0x04     /* end of input             */
#define VIMANA_AUDIO_SAMPLE_RATE   44100   /* sample rate for generated tones */
#define VIMANA_AUDIO_CHANNELS      1       /* mono output for generated tones */
#define VIMANA_VOICE_COUNT         8       /* 4 SID + 3 SID accomp + 1 PSG    */
#define VIMANA_AUDIO_CLOCK         1024000 /* virtual oscillator clock (~1 MHz)*/

/* Waveform types (SID-inspired) */
#define VIMANA_WAVE_TRIANGLE       0
#define VIMANA_WAVE_SAWTOOTH       1
#define VIMANA_WAVE_PULSE          2
#define VIMANA_WAVE_NOISE          3
#define VIMANA_WAVE_PSG            4       /* 80s PSG square (4-bit DAC)      */

/* Filter mode bits */
#define VIMANA_FILT_LP             1       /* low-pass                        */
#define VIMANA_FILT_BP             2       /* band-pass                       */
#define VIMANA_FILT_HP             4       /* high-pass                       */

/* Paddle (A/D converter) */
#define VIMANA_PADDLE_COUNT        2       /* 2 × 8-bit A/D converters        */
/* ─────────────────────────────────────────────────────────────────────── */

/* ── Tile / Sprite ────────────────────────────────────────────────────── */
#define VIMANA_COLOR_COUNT         16    /* palette slots: BG, FG, CLR2..CLR15 */
#define VIMANA_TILE_SIZE           8                /* 8×8 pixels per tile */
#define VIMANA_SPRITE_1BPP_BYTES   8                 /* 8 Bs / 1bpp sprite */
#define VIMANA_SPRITE_2BPP_BYTES   (8 * 2)          /* 16 Bs / 2bpp sprite */
#define VIMANA_SPRITE_3BPP_BYTES   (8 * 3)          /* 24 Bs / 3bpp sprite */
#define VIMANA_SPRITE_4BPP_BYTES   (8 * 4)          /* 32 Bs / 4bpp sprite */
/* ─────────────────────────────────────────────────────────────────────── */

/* ── Font metrics ─────────────────────────────────────────────────────── */
#define VIMANA_GLYPH_HEIGHT       16       /* max glyph height (px)        */
#define VIMANA_UF1_BYTES           8       /* 1 tile  × 8 B  (8x8  1bpp)   */
#define VIMANA_UF2_BYTES          32       /* 4 tiles × 8 B  (16x16 1bpp)  */
#define VIMANA_UF3_BYTES          72       /* 9 tiles × 8 B  (24x24 1bpp)  */
#define VIMANA_FONT_ROW_BYTES      3       /* max 24px wide = 3 bytes/row  */
#define VIMANA_FONT_MAX_HEIGHT    24       /* max height (px) for UF3      */
#define VIMANA_FONT_GLYPH_BYTES   VIMANA_UF3_BYTES
/* ─────────────────────────────────────────────────────────────────────── */

/* ── ROM layout ───────────────────────────────────────────────────────── */
/*  Fixed-address sections: Font → Sprite bank window → General GFX data.  */
/*  Data written here is persistent (no eviction).                         */
#define VIMANA_GLYPH_COUNT       0x100    /* 256 byte-addressed glyphs     */
#define VIMANA_SPRITE_BANK_COUNT 0x10     /* 16 sprite banks               */
#define VIMANA_SPRITE_BANK_SIZE  0x10000  /* 64 KB per sprite bank         */
#define VIMANA_GFX_SIZE          0x60000  /* 384 KB general graphical data */
/* ─────────────────────────────────────────────────────────────────────── */

/* Font ROM internal layout:                                               */
/*   0x0000  16 B      Header (magic, format, height, glyph_width, count)  */
/*   0x0010  256 B     Width table  (1 byte per glyph)                     */
/*   0x0110  N B       Glyph data in compact UFX tile order                */
#define VIMANA_FONT_HDR_OFF      0x0000 /* font header: 16 B               */
#define VIMANA_FONT_HDR_SIZE     16     /* (magic, format, height, etc)    */
#define VIMANA_FONT_WIDTH_OFF    0x0010 /* width table: 256 B              */
#define VIMANA_FONT_BMP_OFF      0x0110 /* 1bpp bitmap: 18432 B            */
/* ─────────────────────────────────────────────────────────────────────── */

typedef struct VimanaSystem vimana_system;
typedef struct VimanaScreen vimana_screen;

typedef void (*vimana_frame_fn)(vimana_system *system, vimana_screen *screen,
                                void *user);

/* ── System API ─────────────────────────────────────────────────────────── */
vimana_system *vimana_system_new(void);
void vimana_system_quit(vimana_system *system);
bool vimana_system_running(vimana_system *system);
void vimana_system_run(vimana_system *system, vimana_screen *screen,
                       vimana_frame_fn frame, void *user);
int64_t vimana_system_ticks(vimana_system *system);
void vimana_system_sleep(vimana_system *system, int64_t ms);
bool vimana_system_set_clipboard_text(vimana_system *system, const char *text);
char *vimana_system_clipboard_text(vimana_system *system);
char *vimana_system_home_dir(vimana_system *system);
void vimana_system_play_tone(vimana_system *system, int pitch,
                             int duration_ms, int volume);
void vimana_system_set_voice(vimana_system *system, int channel,
                             int waveform);
void vimana_system_set_envelope(vimana_system *system, int channel,
                                int attack, int decay, int sustain,
                                int release);
void vimana_system_set_pulse_width(vimana_system *system, int channel,
                                   int width);
void vimana_system_play_voice(vimana_system *system, int channel,
                              int pitch, int volume);
void vimana_system_stop_voice(vimana_system *system, int channel);
void vimana_system_set_frequency(vimana_system *system, int channel,
                                 int freq16);
void vimana_system_set_sync(vimana_system *system, int channel, int enable);
void vimana_system_set_ring_mod(vimana_system *system, int channel,
                                int enable);
void vimana_system_set_filter(vimana_system *system, int cutoff,
                              int resonance, int mode);
void vimana_system_set_filter_route(vimana_system *system, int channel,
                                    int enable);
void vimana_system_set_master_volume(vimana_system *system, int volume);
void vimana_system_begin_audio(vimana_system *system);
void vimana_system_end_audio(vimana_system *system);
void vimana_system_set_paddle(vimana_system *system, int paddle, int value);
int vimana_system_get_paddle(vimana_system *system, int paddle);
void vimana_system_free(vimana_system *system);
size_t vimana_system_ram_usage(vimana_system *system);

/* ── Screen API ─────────────────────────────────────────────────────────── */
vimana_screen *vimana_screen_new(const char *title, unsigned int width,
                                 unsigned int height, unsigned int scale);
void vimana_screen_clear(vimana_screen *screen, unsigned int bg);
void vimana_screen_set_palette(vimana_screen *screen, unsigned int slot,
                               const char *hex);
void vimana_screen_set_font(vimana_screen *screen, unsigned int code,
                            const uint16_t *glyph, unsigned int len);
void vimana_screen_set_font_width(vimana_screen *screen, unsigned int code,
                                  unsigned int width);
void vimana_screen_set_font_size(vimana_screen *screen, unsigned int size);
void vimana_screen_set_theme_swap(vimana_screen *screen, bool swap);
void vimana_screen_set_sprite(vimana_screen *screen, unsigned int addr,
                              const uint8_t *sprite, unsigned int mode,
                              size_t len);
/* Port registers — sprite() interprets as tiles, pixel() as pixels. */
void vimana_screen_set_x(vimana_screen *screen, unsigned int x);
void vimana_screen_set_y(vimana_screen *screen, unsigned int y);
void vimana_screen_set_addr(vimana_screen *screen, unsigned int addr);
void vimana_screen_set_auto(vimana_screen *screen, unsigned int auto_flags);
void vimana_screen_set_sprite_bank(vimana_screen *screen, unsigned int bank);
unsigned int vimana_screen_sprite_bank(vimana_screen *screen);
void vimana_screen_set_gfx(vimana_screen *screen, unsigned int addr,
                           const uint8_t *data, unsigned int len);
const uint8_t *vimana_screen_gfx(vimana_screen *screen, unsigned int addr);
/* Tile-addressed: sprite x/y = port × 8, auto-increments ±1 tile. */
void vimana_screen_sprite(vimana_screen *screen, unsigned int ctrl);
/* Pixel-addressed: x/y = port value, auto-increments ±1 pixel. */
void vimana_screen_pixel(vimana_screen *screen, unsigned int ctrl);
void vimana_screen_put_icn(vimana_screen *screen, unsigned int x,
                           unsigned int y, const uint8_t rows[8],
                           unsigned int fg, unsigned int bg);
void vimana_screen_put_text(vimana_screen *screen, unsigned int x,
                            unsigned int y, const char *text,
                            unsigned int fg, unsigned int bg);
void vimana_screen_present(vimana_screen *screen);
void vimana_screen_set_drag_region(vimana_screen *screen, unsigned int h);
void vimana_screen_set_cursor(vimana_screen *screen, const uint8_t rows[8]);
void vimana_screen_hide_cursor(vimana_screen *screen);
void vimana_screen_show_cursor(vimana_screen *screen);
unsigned int vimana_screen_x(vimana_screen *screen);
unsigned int vimana_screen_y(vimana_screen *screen);
unsigned int vimana_screen_addr(vimana_screen *screen);
unsigned int vimana_screen_auto(vimana_screen *screen);
unsigned int vimana_screen_width(vimana_screen *screen);
unsigned int vimana_screen_height(vimana_screen *screen);
unsigned int vimana_screen_scale(vimana_screen *screen);
size_t vimana_screen_ram_usage(vimana_screen *screen);
void vimana_screen_free(vimana_screen *screen);
static uint8_t vimana_controller_bits(vimana_system *system);

/* ── Device API ─────────────────────────────────────────────────────────── */
void vimana_device_poll(vimana_system *system);
unsigned int vimana_device_controller(vimana_system *system);
bool vimana_device_controller_down(vimana_system *system, unsigned int mask);
bool vimana_device_controller_pressed(vimana_system *system,
                                      unsigned int mask);
bool vimana_device_key_down(vimana_system *system, int scancode);
bool vimana_device_key_pressed(vimana_system *system, int scancode);
bool vimana_device_mouse_down(vimana_system *system, int button);
bool vimana_device_mouse_pressed(vimana_system *system, int button);
unsigned int vimana_device_pointer_x(vimana_system *system);
unsigned int vimana_device_pointer_y(vimana_system *system);
unsigned int vimana_device_tile_x(vimana_system *system);
unsigned int vimana_device_tile_y(vimana_system *system);
int vimana_device_wheel_x(vimana_system *system);
int vimana_device_wheel_y(vimana_system *system);
const char *vimana_device_text_input(vimana_system *system);

/* ── Console API ────────────────────────────────────────────────────────── */
bool vimana_console_pending(vimana_system *system);
int vimana_console_input(vimana_system *system);
int vimana_console_type(vimana_system *system);
void vimana_console_next(vimana_system *system);
bool vimana_console_push(vimana_system *system, int byte, int type);
void vimana_console_stdout(vimana_system *system, int byte);
void vimana_console_stderr(vimana_system *system, int byte);
void vimana_console_stderr_hex(vimana_system *system, int byte);

/* ── Datetime API ───────────────────────────────────────────────────────── */
int64_t vimana_datetime_now(vimana_system *system);
int vimana_datetime_year(vimana_system *system);
int vimana_datetime_month(vimana_system *system);
int vimana_datetime_day(vimana_system *system);
int vimana_datetime_hour(vimana_system *system);
int vimana_datetime_minute(vimana_system *system);
int vimana_datetime_second(vimana_system *system);
int vimana_datetime_weekday(vimana_system *system);
int vimana_datetime_yday(vimana_system *system);
int vimana_datetime_dst(vimana_system *system);
int vimana_datetime_year_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_month_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_day_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_hour_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_minute_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_second_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_weekday_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_yday_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_dst_at(vimana_system *system, int64_t timestamp);

/* ── File API ───────────────────────────────────────────────────────────── */
unsigned char *vimana_file_read_bytes(vimana_system *system, const char *path,
                                      size_t *out_size);
char *vimana_file_read_text(vimana_system *system, const char *path);
bool vimana_file_write_bytes(vimana_system *system, const char *path,
                             const unsigned char *bytes, size_t size);
bool vimana_file_write_text(vimana_system *system, const char *path,
                            const char *text);
bool vimana_file_exists(vimana_system *system, const char *path);
bool vimana_file_remove(vimana_system *system, const char *path);
bool vimana_file_rename(vimana_system *system, const char *path,
                        const char *new_path);
char **vimana_file_list(vimana_system *system, const char *path,
                        int *out_count);
void vimana_file_list_free(char **items, int count);
bool vimana_file_is_dir(vimana_system *system, const char *path);

/* ── Process API ────────────────────────────────────────────────────────── */
typedef struct VimanaProcess vimana_process;
vimana_process *vimana_process_spawn(vimana_system *system, const char *cmd);
bool vimana_process_write(vimana_process *proc, const char *text);
char *vimana_process_read_line(vimana_process *proc);
bool vimana_process_running(vimana_process *proc);
void vimana_process_kill(vimana_process *proc);
void vimana_process_free(vimana_process *proc);

#ifdef __cplusplus
}
#endif
