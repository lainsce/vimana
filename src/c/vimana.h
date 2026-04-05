// Vimana pixel runtime — public C API
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VIMANA_TITLEBAR_HEIGHT 24
#define VIMANA_TB_BOX_SIZE 13
#define VIMANA_TB_BOX_Y    4
#define VIMANA_TB_CLOSE_X  4

typedef struct VimanaSystem vimana_system;
typedef struct VimanaScreen vimana_screen;

typedef void (*vimana_frame_fn)(vimana_system *system, vimana_screen *screen,
                                void *user);

// System
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

// Screen
vimana_screen *vimana_screen_new(const char *title, int width, int height,
                                 int scale);
void vimana_screen_clear(vimana_screen *screen, int bg);
void vimana_screen_resize(vimana_screen *screen, int width, int height);
void vimana_screen_set_palette(vimana_screen *screen, int slot,
                               const char *hex);
void vimana_screen_set_font_glyph(vimana_screen *screen, int code,
                                  const uint16_t rows[16]);
void vimana_screen_set_font_chr(vimana_screen *screen, int code,
                                const uint8_t *chr, int len);
void vimana_screen_set_font_width(vimana_screen *screen, int code, int width);
void vimana_screen_set_font_size(vimana_screen *screen, int size);
void vimana_screen_set_theme_swap(vimana_screen *screen, bool swap);
void vimana_screen_set_sprite(vimana_screen *screen, int addr,
                              const uint8_t *sprite, int mode);
void vimana_screen_set_x(vimana_screen *screen, int x);
void vimana_screen_set_y(vimana_screen *screen, int y);
void vimana_screen_set_addr(vimana_screen *screen, int addr);
void vimana_screen_set_auto(vimana_screen *screen, int auto_flags);
int vimana_screen_x(vimana_screen *screen);
int vimana_screen_y(vimana_screen *screen);
int vimana_screen_addr(vimana_screen *screen);
int vimana_screen_auto(vimana_screen *screen);
void vimana_screen_sprite(vimana_screen *screen, int ctrl);
void vimana_screen_pixel(vimana_screen *screen, int ctrl);
void vimana_screen_put(vimana_screen *screen, int x, int y, const char *glyph,
                       int fg, int bg);
void vimana_screen_put_icn(vimana_screen *screen, int x, int y,
                           const uint8_t rows[8], int fg, int bg);
void vimana_screen_put_text(vimana_screen *screen, int x, int y,
                            const char *text, int fg, int bg);
void vimana_screen_present(vimana_screen *screen);
void vimana_screen_draw_titlebar(vimana_screen *screen, int bg);
void vimana_screen_set_titlebar_title(vimana_screen *screen, const char *title);
void vimana_screen_set_titlebar_button(vimana_screen *screen, bool show);
bool vimana_screen_titlebar_button_pressed(vimana_screen *screen);
int vimana_screen_width(vimana_screen *screen);
int vimana_screen_height(vimana_screen *screen);
int vimana_screen_scale(vimana_screen *screen);

// Device
void vimana_device_poll(vimana_system *system);
bool vimana_device_key_down(vimana_system *system, int scancode);
bool vimana_device_key_pressed(vimana_system *system, int scancode);
bool vimana_device_mouse_down(vimana_system *system, int button);
bool vimana_device_mouse_pressed(vimana_system *system, int button);
int vimana_device_pointer_x(vimana_system *system);
int vimana_device_pointer_y(vimana_system *system);
int vimana_device_tile_x(vimana_system *system);
int vimana_device_tile_y(vimana_system *system);
int vimana_device_wheel_x(vimana_system *system);
int vimana_device_wheel_y(vimana_system *system);
const char *vimana_device_text_input(vimana_system *system);

// Datetime
int64_t vimana_datetime_now(vimana_system *system);
int vimana_datetime_year(vimana_system *system);
int vimana_datetime_month(vimana_system *system);
int vimana_datetime_day(vimana_system *system);
int vimana_datetime_hour(vimana_system *system);
int vimana_datetime_minute(vimana_system *system);
int vimana_datetime_second(vimana_system *system);
int vimana_datetime_weekday(vimana_system *system);
int vimana_datetime_year_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_month_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_day_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_hour_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_minute_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_second_at(vimana_system *system, int64_t timestamp);
int vimana_datetime_weekday_at(vimana_system *system, int64_t timestamp);

// File I/O
unsigned char *vimana_file_read_bytes(vimana_system *system, const char *path,
                                      size_t *out_size);
bool vimana_file_write_bytes(vimana_system *system, const char *path,
                             const unsigned char *bytes, size_t size);
char *vimana_file_read_text(vimana_system *system, const char *path);
bool vimana_file_write_text(vimana_system *system, const char *path,
                            const char *text);
bool vimana_file_exists(vimana_system *system, const char *path);
bool vimana_file_remove(vimana_system *system, const char *path);
bool vimana_file_rename(vimana_system *system, const char *path,
                        const char *new_path);
bool vimana_file_is_dir(vimana_system *system, const char *path);
char **vimana_file_list(vimana_system *system, const char *path,
                        int *out_count);
void vimana_file_list_free(char **items, int count);

// Subprocess IPC
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
