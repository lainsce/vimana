// Cogito Backend Abstraction Layer
// Provides platform-agnostic interface for windowing, rendering, input, and
// text

#ifndef COGITO_BACKEND_H
#define COGITO_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Types
// ============================================================================

typedef struct CogitoWindow CogitoWindow;
typedef struct CogitoFont CogitoFont;
typedef struct CogitoTexture CogitoTexture;

// Cursor types for visual feedback
typedef enum {
  COGITO_CURSOR_DEFAULT = 0,
  COGITO_CURSOR_GRAB,
  COGITO_CURSOR_GRABBING,
  COGITO_CURSOR_POINTER,
  COGITO_CURSOR_TEXT,
  COGITO_CURSOR_COUNT
} CogitoCursorType;

typedef struct {
  uint8_t r, g, b, a;
} CogitoColor;

typedef struct {
  float x, y;
} CogitoVec2;

typedef struct {
  int x, y, w, h;
} CogitoRect;

// ============================================================================
// Color Helpers (inline for performance)
// ============================================================================

static inline CogitoColor cogito_color(uint8_t r, uint8_t g, uint8_t b,
                                       uint8_t a) {
  return (CogitoColor){r, g, b, a};
}

CogitoColor cogito_color_lerp(CogitoColor a, CogitoColor b, float t);
CogitoColor cogito_color_blend(CogitoColor base, CogitoColor over);
CogitoColor cogito_color_apply_opacity(CogitoColor c, float opacity);
float cogito_color_luma(CogitoColor c);
CogitoColor cogito_color_mix(CogitoColor a, CogitoColor b, float t);
CogitoColor cogito_color_alpha(CogitoColor c, float t);
CogitoColor cogito_color_on_color(CogitoColor bg);

// ============================================================================
// Backend Interface
// ============================================================================

typedef struct CogitoBackend {
  // Lifecycle
  bool (*init)(void);
  void (*shutdown)(void);

  // Window management
  CogitoWindow *(*window_create)(const char *title, int w, int h,
                                 bool resizable, bool borderless,
                                 bool initially_hidden);
  void (*window_destroy)(CogitoWindow *window);
  void (*window_set_size)(CogitoWindow *window, int w, int h);
  void (*window_get_size)(CogitoWindow *window, int *w, int *h);
  void (*window_set_position)(CogitoWindow *window, int x, int y);
  void (*window_get_position)(CogitoWindow *window, int *x, int *y);
  void (*window_set_title)(CogitoWindow *window, const char *title);
  void (*window_show)(CogitoWindow *window);
  void (*window_hide)(CogitoWindow *window);
  void (*window_raise)(CogitoWindow *window);
  void (*window_minimize)(CogitoWindow *window);
  void (*window_maximize)(CogitoWindow *window);
  void (*window_restore)(CogitoWindow *window);
  bool (*window_is_maximized)(CogitoWindow *window);
  void *(*window_get_native_handle)(CogitoWindow *window);
  bool (*window_set_icon)(CogitoWindow *window, const char *path);
  uint32_t (*window_get_id)(CogitoWindow *window);
  bool (*open_url)(const char *url);
  bool (*set_clipboard_text)(const char *text);

  // Frame rendering
  void (*begin_frame)(CogitoWindow *window);
  void (*end_frame)(CogitoWindow *window);
  void (*present)(CogitoWindow *window);
  void (*clear)(CogitoColor color);

  // Event loop
  bool (*poll_events)(void); // true if at least one event was processed
  void (*wait_event_timeout)(uint32_t ms); // block until event or timeout (when
                                           // idle, avoids busy loop)
  bool (*window_should_close)(CogitoWindow *window);

  // Input
  void (*get_mouse_position)(int *x, int *y);
  void (*get_mouse_position_in_window)(CogitoWindow *window, int *x, int *y);
  bool (*is_mouse_button_down)(int button);
  bool (*is_mouse_button_pressed)(int button);
  bool (*is_mouse_button_released)(int button);
  float (*get_mouse_wheel_move)(void);
  bool (*is_key_down)(int key);
  bool (*is_key_pressed)(int key);
  bool (*is_key_released)(int key);
  int (*get_char_pressed)(void);

  // Time
  double (*get_time)(void);
  void (*sleep)(uint32_t ms);

  // Drawing primitives
  void (*draw_rect)(int x, int y, int w, int h, CogitoColor color);
  void (*draw_rect_rounded)(int x, int y, int w, int h, CogitoColor color,
                            float roundness);
  void (*draw_rect_lines)(int x, int y, int w, int h, CogitoColor color,
                          int thickness);
  void (*draw_rect_rounded_lines)(int x, int y, int w, int h, CogitoColor color,
                                  float roundness, int thickness);
  void (*draw_line)(int x1, int y1, int x2, int y2, CogitoColor color,
                    int thickness);
  void (*draw_circle)(int x, int y, float radius, CogitoColor color);
  void (*draw_circle_lines)(int x, int y, float radius, CogitoColor color,
                            int thickness);

  // Text
  CogitoFont *(*font_load)(const char *path, int size);
  CogitoFont *(*font_load_face)(const char *path, int size, int face_index);
  void (*font_unload)(CogitoFont *font);
  void (*font_get_metrics)(CogitoFont *font, int *ascent, int *descent,
                           int *height);
  void *(*font_get_internal_face)(CogitoFont *font);
  // Set a variable font axis value at runtime (e.g. 'wght', 'wdth')
  // axis_tag is a 4-char OpenType tag packed as uint32 big-endian, e.g.
  // ('w'<<24)|('g'<<16)|('h'<<8)|'t' Returns true if the axis was successfully
  // set.
  bool (*font_set_variation)(CogitoFont *font, uint32_t axis_tag, float value);
  int (*text_measure_width)(CogitoFont *font, const char *text, int size);
  int (*text_measure_height)(CogitoFont *font, int size);
  void (*draw_text)(CogitoFont *font, const char *text, int x, int y, int size,
                    CogitoColor color);

  // Textures
  CogitoTexture *(*texture_create)(int w, int h, const uint8_t *data,
                                   int channels);
  void (*texture_destroy)(CogitoTexture *tex);
  void (*texture_get_size)(CogitoTexture *tex, int *w, int *h);
  void (*draw_texture)(CogitoTexture *tex, CogitoRect src, CogitoRect dst,
                       CogitoColor tint);
  void (*draw_texture_pro)(CogitoTexture *tex, CogitoRect src, CogitoRect dst,
                           CogitoVec2 origin, float rotation, CogitoColor tint);

  // Scissor/clip
  void (*begin_scissor)(int x, int y, int w, int h);
  void (*end_scissor)(void);

  // Blend modes
  void (*set_blend_mode)(int mode);

  // Cursor
  void (*set_cursor)(CogitoCursorType cursor);

  // CSD (Client-Side Decorations)
  void (*window_set_hit_test_callback)(CogitoWindow *window,
                                       int (*callback)(CogitoWindow *win, int x,
                                                       int y, void *user),
                                       void *user);
  void (*window_set_borderless)(CogitoWindow *window, bool borderless);
  bool (*window_is_borderless)(CogitoWindow *window);

  // Debug
  void (*set_debug_overlay)(CogitoWindow *window, bool enable);
} CogitoBackend;

// ============================================================================
// Multi-Window Support
// ============================================================================

#define COGITO_MAX_WINDOWS 8

typedef struct {
  CogitoWindow *windows[COGITO_MAX_WINDOWS];
  uint32_t window_ids[COGITO_MAX_WINDOWS];
  void *window_data[COGITO_MAX_WINDOWS]; // Per-window user data
  int count;
  int focused_index; // Index of window with keyboard focus
} CogitoWindowRegistry;

// Initialize window registry
void cogito_window_registry_init(CogitoWindowRegistry *registry);

// Register a window
bool cogito_window_registry_add(CogitoWindowRegistry *registry,
                                CogitoWindow *window);

// Unregister a window
void cogito_window_registry_remove(CogitoWindowRegistry *registry,
                                   CogitoWindow *window);

// Get window by ID
CogitoWindow *cogito_window_registry_get(CogitoWindowRegistry *registry,
                                         uint32_t window_id);

// Get window index
int cogito_window_registry_find(CogitoWindowRegistry *registry,
                                CogitoWindow *window);

// Set focused window
void cogito_window_registry_set_focused(CogitoWindowRegistry *registry,
                                        CogitoWindow *window);

// Get focused window
CogitoWindow *
cogito_window_registry_get_focused(CogitoWindowRegistry *registry);

// Check if any windows are open
bool cogito_window_registry_has_windows(CogitoWindowRegistry *registry);

// Route event to appropriate window
CogitoWindow *cogito_window_registry_route_event(CogitoWindowRegistry *registry,
                                                 uint32_t window_id);

// ============================================================================
// Debug Flags
// ============================================================================

typedef struct {
  bool debug_csd;    // COGITO_DEBUG_CSD=1 - Show CSD hit regions
  bool debug_style;  // COGITO_DEBUG_STYLE=1 - Print style dump
  bool debug_native; // COGITO_DEBUG_NATIVE=1 - Print native handle info
  bool inspector;    // COGITO_INSPECTOR=1 - Enable inspector (Ctrl+Shift+I)
} CogitoDebugFlags;

// Parse debug flags from environment variables
void cogito_debug_flags_parse(CogitoDebugFlags *flags);

// Check if inspector toggle key combo is pressed (Ctrl+Shift+I)
bool cogito_debug_inspector_toggle_pressed(CogitoBackend *backend);

// ============================================================================
// Backend Instance
// ============================================================================

extern CogitoBackend *cogito_backend;

// Initialize SDL3 backend
bool cogito_backend_sdl3_init(void);
CogitoBackend *cogito_backend_sdl3_get(void);

#ifdef __cplusplus
}
#endif

#endif // COGITO_BACKEND_H
