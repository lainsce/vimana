// Cogito SDL3 Backend Implementation - Phase 3
// Uses SDL3 GPU API (Metal on macOS, DX12 on Windows, Vulkan on Linux)

#include "backend.h"
#include "csd.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_ttf/SDL_ttf.h>
#if defined(COGITO_HAS_SDL3_IMAGE)
#include <SDL3_image/SDL_image.h>
#endif

// FreeType for variable font axis manipulation (wght/wdth at runtime)
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

static void *sdl3_window_get_native_handle(CogitoWindow *window);
static SDL_HitTestResult sdl3_csd_hit_test_callback(SDL_Window *sdl_win,
                                                    const SDL_Point *point,
                                                    void *data);
static SDL_Renderer *sdl3_active_renderer(void);
static SDL_Renderer *sdl3_create_renderer_for_window(SDL_Window *window);
static void sdl3_get_mouse_position_in_window(CogitoWindow *window, int *x,
                                              int *y);
static void sdl3_free_geometry_buffers(void);

// ============================================================================
// Cursor
// ============================================================================

static SDL_Cursor *sdl3_cursors[COGITO_CURSOR_COUNT] = {NULL};

static SDL_SystemCursor sdl3_system_cursor_for(CogitoCursorType cursor) {
  switch (cursor) {
  case COGITO_CURSOR_TEXT:
    return SDL_SYSTEM_CURSOR_TEXT;
  case COGITO_CURSOR_GRAB:
  case COGITO_CURSOR_GRABBING:
  case COGITO_CURSOR_POINTER:
    return SDL_SYSTEM_CURSOR_POINTER;
  case COGITO_CURSOR_DEFAULT:
  default:
    return SDL_SYSTEM_CURSOR_DEFAULT;
  }
}

static void sdl3_init_cursors(void) {
  memset(sdl3_cursors, 0, sizeof(sdl3_cursors));
}

static void sdl3_set_cursor(CogitoCursorType cursor) {
  if (cursor < 0 || cursor >= COGITO_CURSOR_COUNT)
    return;
  SDL_Cursor *c = sdl3_cursors[cursor];
  if (!c) {
    c = SDL_CreateSystemCursor(sdl3_system_cursor_for(cursor));
    sdl3_cursors[cursor] = c;
  }
  if (c)
    SDL_SetCursor(c);
}

// ============================================================================
// Internal Types
// ============================================================================

typedef struct CogitoSDL3Texture {
  SDL_GPUTexture *gpu_texture;
  SDL_Texture *sdl_texture;
  int width;
  int height;
  int channels;
} CogitoSDL3Texture;

typedef struct CogitoSDL3Window {
  SDL_Window *sdl_window;
  SDL_Renderer *renderer;
  int width, height;
  bool should_close;
  bool borderless;
  uint32_t window_id;
  CogitoCSDState csd_state;
  int (*hit_test_callback)(CogitoWindow *win, int x, int y, void *user);
  void *hit_test_userdata;
  SDL_GPUSwapchainComposition swapchain_composition;
  SDL_GPUPresentMode present_mode;
} CogitoSDL3Window;

typedef struct CogitoSDL3Font {
  char *path;
  int size;
  TTF_Font *ttf_font;
  int ascent;
  int descent;
  int height;
} CogitoSDL3Font;

// GPU render state
typedef struct {
  SDL_GPUCommandBuffer *cmd_buf;
  SDL_GPURenderPass *render_pass;
  SDL_GPUTexture *swapchain_texture;
  SDL_GPUColorTargetInfo color_target;
  int window_width;
  int window_height;
} CogitoSDL3RenderState;

static CogitoSDL3RenderState g_render_state = {0};

// ============================================================================
// Global State
// ============================================================================

static bool sdl3_initialized = false;
static bool ttf_initialized = false;
static SDL_GPUDevice *global_gpu_device = NULL;
static SDL_Renderer *g_current_renderer = NULL;
static struct CogitoSDL3Window *g_current_window = NULL;
static SDL_Renderer *g_draw_color_renderer = NULL;
static uint32_t g_draw_color_packed = 0;
static bool g_draw_color_valid = false;
static CogitoWindowRegistry window_registry = {0};
static CogitoDebugFlags debug_flags = {0};

static inline void sdl3_set_draw_color_cached(uint8_t r, uint8_t g, uint8_t b,
                                              uint8_t a) {
  if (!g_current_renderer)
    return;
  uint32_t packed = ((uint32_t)r << 24) | ((uint32_t)g << 16) |
                    ((uint32_t)b << 8) | (uint32_t)a;
  if (g_draw_color_valid && g_draw_color_renderer == g_current_renderer &&
      g_draw_color_packed == packed) {
    return;
  }
  SDL_SetRenderDrawColor(g_current_renderer, r, g, b, a);
  g_draw_color_renderer = g_current_renderer;
  g_draw_color_packed = packed;
  g_draw_color_valid = true;
}

#define SDL3_RECT_BATCH_MAX 1024
static SDL_FRect g_rect_batch[SDL3_RECT_BATCH_MAX];
static int g_rect_batch_count = 0;
static SDL_Renderer *g_rect_batch_renderer = NULL;
static CogitoColor g_rect_batch_color = {0, 0, 0, 0};

static inline void sdl3_rect_batch_flush(void) {
  if (!g_rect_batch_renderer || g_rect_batch_count <= 0)
    return;
  sdl3_set_draw_color_cached(g_rect_batch_color.r, g_rect_batch_color.g,
                             g_rect_batch_color.b, g_rect_batch_color.a);
  SDL_RenderFillRects(g_rect_batch_renderer, g_rect_batch, g_rect_batch_count);
  g_rect_batch_count = 0;
}

static inline void sdl3_rect_batch_reset(void) {
  g_rect_batch_count = 0;
  g_rect_batch_renderer = NULL;
}

static inline void sdl3_rect_batch_push(SDL_Renderer *renderer, int x, int y,
                                        int w, int h, CogitoColor color) {
  if (!renderer || w <= 0 || h <= 0 || color.a == 0)
    return;
  if (g_rect_batch_count > 0 &&
      (g_rect_batch_renderer != renderer || g_rect_batch_color.r != color.r ||
       g_rect_batch_color.g != color.g || g_rect_batch_color.b != color.b ||
       g_rect_batch_color.a != color.a ||
       g_rect_batch_count >= SDL3_RECT_BATCH_MAX)) {
    sdl3_rect_batch_flush();
  }
  if (g_rect_batch_count == 0) {
    g_rect_batch_renderer = renderer;
    g_rect_batch_color = color;
  }
  g_rect_batch[g_rect_batch_count++] =
      (SDL_FRect){(float)x, (float)y, (float)w, (float)h};
}

// Scissor stack for nested clipping support
#define MAX_SCISSOR_STACK 16
static int scissor_stack_count = 0;
static SDL_Rect scissor_stack[MAX_SCISSOR_STACK];

// Input state
static int mouse_x = 0, mouse_y = 0;
static bool mouse_buttons[3] = {false};
static bool mouse_buttons_pressed[3] = {false};
static bool mouse_buttons_released[3] = {false};
static float mouse_wheel = 0.0f;
static bool keys_down[512] = {false};
static bool keys_pressed[512] = {false};
static bool keys_released[512] = {false};
static int char_queue[16] = {0};
static int char_queue_head = 0, char_queue_tail = 0;
static double start_time = 0.0;

// ============================================================================
// Text Texture Cache (Performance Optimization)
// ============================================================================

#define COGITO_TEXT_CACHE_SIZE 512
#define COGITO_TEXT_CACHE_MAX_LEN 128
#define COGITO_TEXT_CACHE_EVICT_AGE                                            \
  24 // Keep entries warm across short bursts while still aging out quickly
#define COGITO_TEXT_CACHE_MAX_BYTES (16u * 1024u * 1024u)
#define COGITO_FRAME_BUDGET_MS                                                 \
  12 // Target frame time in ms (leaves 4ms buffer for 60fps)

typedef struct CogitoTextCacheKey {
  char text[COGITO_TEXT_CACHE_MAX_LEN];
  TTF_Font *font;
} CogitoTextCacheKey;

typedef struct CogitoTextCacheEntry {
  CogitoTextCacheKey key;
  uint32_t hash;
  SDL_Texture *texture;
  int width;
  int height;
  size_t size_bytes;
  uint64_t last_used;
  bool valid;
} CogitoTextCacheEntry;

static CogitoTextCacheEntry g_text_cache[COGITO_TEXT_CACHE_SIZE];
static uint64_t g_text_cache_frame = 0;
static size_t g_text_cache_bytes = 0;

static uint32_t cogito_text_cache_hash(const CogitoTextCacheKey *key) {
  uint32_t h = 5381;
  for (const char *p = key->text; *p; p++) {
    h = ((h << 5) + h) ^ (uint8_t)*p;
  }
  h ^= (uint32_t)(uintptr_t)key->font;
  return h;
}

static bool cogito_text_cache_key_eq(const CogitoTextCacheKey *a,
                                     const CogitoTextCacheKey *b) {
  return a->font == b->font && strcmp(a->text, b->text) == 0;
}

static void cogito_text_cache_drop_entry(CogitoTextCacheEntry *e) {
  if (!e)
    return;
  if (e->texture) {
    SDL_DestroyTexture(e->texture);
    e->texture = NULL;
  }
  if (e->size_bytes > 0) {
    if (e->size_bytes <= g_text_cache_bytes) {
      g_text_cache_bytes -= e->size_bytes;
    } else {
      g_text_cache_bytes = 0;
    }
  }
  e->width = 0;
  e->height = 0;
  e->hash = 0;
  e->size_bytes = 0;
  e->valid = false;
}

static void cogito_text_cache_trim(size_t max_bytes,
                                   CogitoTextCacheEntry *keep_entry) {
  while (g_text_cache_bytes > max_bytes) {
    uint64_t oldest = UINT64_MAX;
    int oldest_idx = -1;
    for (int i = 0; i < COGITO_TEXT_CACHE_SIZE; i++) {
      CogitoTextCacheEntry *e = &g_text_cache[i];
      if (!e->valid || !e->texture || e == keep_entry)
        continue;
      if (e->last_used < oldest) {
        oldest = e->last_used;
        oldest_idx = i;
      }
    }
    if (oldest_idx < 0)
      break;
    cogito_text_cache_drop_entry(&g_text_cache[oldest_idx]);
  }
}

static CogitoTextCacheEntry *
cogito_text_cache_lookup(TTF_Font *font, const char *text, size_t text_len) {
  CogitoTextCacheKey key = {0};
  if (text_len >= (size_t)COGITO_TEXT_CACHE_MAX_LEN) {
    text_len = (size_t)COGITO_TEXT_CACHE_MAX_LEN - 1;
  }
  if (text_len > 0) {
    memcpy(key.text, text, text_len);
  }
  key.text[text_len] = '\0';
  key.font = font;

  uint32_t hash = cogito_text_cache_hash(&key);
  int idx = hash % COGITO_TEXT_CACHE_SIZE;

  // Linear probing
  for (int i = 0; i < COGITO_TEXT_CACHE_SIZE; i++) {
    int probe = (idx + i) % COGITO_TEXT_CACHE_SIZE;
    CogitoTextCacheEntry *e = &g_text_cache[probe];

    if (!e->valid) {
      // Empty slot - will be filled by insert
      e->key = key;
      e->hash = hash;
      return e;
    }

    if (e->hash == hash && cogito_text_cache_key_eq(&e->key, &key)) {
      // Cache hit
      e->last_used = g_text_cache_frame;
      return e;
    }
  }

  // Cache full - evict oldest
  uint64_t oldest = UINT64_MAX;
  int oldest_idx = 0;
  for (int i = 0; i < COGITO_TEXT_CACHE_SIZE; i++) {
    if (g_text_cache[i].last_used < oldest) {
      oldest = g_text_cache[i].last_used;
      oldest_idx = i;
    }
  }

  CogitoTextCacheEntry *e = &g_text_cache[oldest_idx];
  cogito_text_cache_drop_entry(e);
  e->key = key;
  e->hash = hash;
  return e;
}

static void cogito_text_cache_clear(void) {
  for (int i = 0; i < COGITO_TEXT_CACHE_SIZE; i++) {
    cogito_text_cache_drop_entry(&g_text_cache[i]);
  }
  g_text_cache_bytes = 0;
}

static void cogito_text_cache_frame_start(void) {
  g_text_cache_frame++;

  // Frame Bus Method: evict every 7 frames
  if (g_text_cache_frame % 7 == 0) {
    for (int i = 0; i < COGITO_TEXT_CACHE_SIZE; i++) {
      CogitoTextCacheEntry *e = &g_text_cache[i];
      if (e->valid && e->texture &&
          (g_text_cache_frame - e->last_used) > COGITO_TEXT_CACHE_EVICT_AGE) {
        cogito_text_cache_drop_entry(e);
      }
    }
    cogito_text_cache_trim(COGITO_TEXT_CACHE_MAX_BYTES, NULL);
  }
}

// ============================================================================
// Color Helpers
// ============================================================================

CogitoColor cogito_color_lerp(CogitoColor a, CogitoColor b, float t) {
  if (t <= 0.0f)
    return a;
  if (t >= 1.0f)
    return b;
  return (CogitoColor){
      (uint8_t)(a.r + (b.r - a.r) * t), (uint8_t)(a.g + (b.g - a.g) * t),
      (uint8_t)(a.b + (b.b - a.b) * t), (uint8_t)(a.a + (b.a - a.a) * t)};
}

CogitoColor cogito_color_blend(CogitoColor base, CogitoColor over) {
  float a = over.a / 255.0f;
  float ia = 1.0f - a;
  return (CogitoColor){(uint8_t)(base.r * ia + over.r * a),
                       (uint8_t)(base.g * ia + over.g * a),
                       (uint8_t)(base.b * ia + over.b * a), base.a};
}

CogitoColor cogito_color_apply_opacity(CogitoColor c, float opacity) {
  if (opacity >= 1.0f)
    return c;
  if (opacity <= 0.0f)
    return (CogitoColor){c.r, c.g, c.b, 0};
  c.a = (uint8_t)((float)c.a * opacity);
  return c;
}

float cogito_color_luma(CogitoColor c) {
  float r = c.r / 255.0f;
  float g = c.g / 255.0f;
  float b = c.b / 255.0f;
  return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

CogitoColor cogito_color_mix(CogitoColor a, CogitoColor b, float t) {
  return cogito_color_lerp(a, b, t);
}

CogitoColor cogito_color_alpha(CogitoColor c, float t) {
  if (t >= 1.0f)
    return c;
  if (t <= 0.0f)
    return (CogitoColor){c.r, c.g, c.b, 0};
  c.a = (uint8_t)((float)c.a * t);
  return c;
}

// Linearise an sRGB channel value [0,1] into linear light
static inline float cogito_srgb_linearize(float v) {
  if (v <= 0.04045f)
    return v / 12.92f;
  return powf((v + 0.055f) / 1.055f, 2.4f);
}

// cogito_color_on_color: auto-calculate accessible foreground (black/white)
// using CIE L* (perceptual lightness) — L* > 50 → black text, else white.
// This is significantly more accurate than a raw luma threshold because it
// accounts for the non-linearity of human contrast perception.
CogitoColor cogito_color_on_color(CogitoColor bg) {
  float r = cogito_srgb_linearize(bg.r / 255.0f);
  float g = cogito_srgb_linearize(bg.g / 255.0f);
  float b = cogito_srgb_linearize(bg.b / 255.0f);
  // Relative luminance (CIE Y)
  float Y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
  // CIE L*: perceptual lightness [0,100]
  float Lstar;
  if (Y <= 0.008856f) {
    Lstar = Y * 903.3f;
  } else {
    Lstar = cbrtf(Y) * 116.0f - 16.0f;
  }
  // L* > 50 means the background is perceptually lighter than mid-grey → use
  // black
  return Lstar > 50.0f ? (CogitoColor){0, 0, 0, 255}
                       : (CogitoColor){255, 255, 255, 255};
}

// ============================================================================
// Lifecycle
// ============================================================================

static bool sdl3_init(void) {
  if (sdl3_initialized)
    return true;

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s",
                 SDL_GetError());
    return false;
  }

  // Prefer geometry-based line rendering and vsync for smoother output.
  SDL_SetHint(SDL_HINT_RENDER_LINE_METHOD, "3");
  SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

  // Best-effort multisampling request for GL-backed renderers.
  // SDL_Renderer does not expose a generic MSAA sample-count property.
  (void)SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  (void)SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  // Initialize TTF
  if (!TTF_Init()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_Init failed: %s",
                 SDL_GetError());
    SDL_Quit();
    return false;
  }
  ttf_initialized = true;

  // Parse debug flags
  cogito_debug_flags_parse(&debug_flags);
  if (debug_flags.debug_native) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Native handle debugging enabled");
  }

  // Initialize window registry
  cogito_window_registry_init(&window_registry);

  // Initialize cursors
  sdl3_init_cursors();

  start_time = (double)SDL_GetTicks() / 1000.0;
  sdl3_initialized = true;
  return true;
}

static void sdl3_shutdown(void) {
  if (!sdl3_initialized)
    return;

  // Clear text texture cache
  cogito_text_cache_clear();
  sdl3_free_geometry_buffers();

  for (int i = 0; i < COGITO_CURSOR_COUNT; i++) {
    if (sdl3_cursors[i]) {
      SDL_DestroyCursor(sdl3_cursors[i]);
      sdl3_cursors[i] = NULL;
    }
  }

  if (global_gpu_device) {
    SDL_DestroyGPUDevice(global_gpu_device);
    global_gpu_device = NULL;
  }

  if (ttf_initialized) {
    TTF_Quit();
    ttf_initialized = false;
  }

  SDL_Quit();
  sdl3_initialized = false;
}

static SDL_Renderer *sdl3_create_renderer_for_window(SDL_Window *window) {
  if (!window)
    return NULL;

  SDL_Renderer *renderer = NULL;

  const char *preferred[3] = {NULL, NULL, NULL};
  int preferred_count = 0;
#ifdef __APPLE__
  preferred[preferred_count++] = "opengl";
  preferred[preferred_count++] = "gpu";
#else
  preferred[preferred_count++] = "gpu";
#endif

  for (int i = 0; i < preferred_count && !renderer; i++) {
    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props)
      continue;
    SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER,
                           window);
    SDL_SetStringProperty(props, SDL_PROP_RENDERER_CREATE_NAME_STRING,
                          preferred[i]);
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER,
                          1);
    renderer = SDL_CreateRendererWithProperties(props);
    SDL_DestroyProperties(props);
  }

  if (!renderer) {
    renderer = SDL_CreateRenderer(window, NULL);
  }

  return renderer;
}

// ============================================================================
// Window Management
// ============================================================================

static CogitoWindow *sdl3_window_create(const char *title, int w, int h,
                                        bool resizable, bool borderless,
                                        bool initially_hidden) {
  if (!sdl3_initialized)
    return NULL;

  CogitoSDL3Window *win = calloc(1, sizeof(CogitoSDL3Window));
  if (!win)
    return NULL;

  SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (resizable)
    flags |= SDL_WINDOW_RESIZABLE;
  if (initially_hidden)
    flags |= SDL_WINDOW_HIDDEN;
  bool try_transparent = false;
  if (borderless) {
    flags |= SDL_WINDOW_BORDERLESS;
    flags |= SDL_WINDOW_TRANSPARENT;
    try_transparent = true;
  }
#ifdef __APPLE__
  // Required for OpenGL renderer path where MSAA attributes are honored.
  flags |= SDL_WINDOW_OPENGL;
#endif

  win->sdl_window = SDL_CreateWindow(title, w, h, flags);
  if (!win->sdl_window && try_transparent) {
    SDL_WindowFlags fallback_flags = flags & ~SDL_WINDOW_TRANSPARENT;
    win->sdl_window = SDL_CreateWindow(title, w, h, fallback_flags);
  }
  if (!win->sdl_window) {
    free(win);
    return NULL;
  }

  win->width = w;
  win->height = h;
  win->borderless = borderless;
  win->window_id = SDL_GetWindowID(win->sdl_window);

  win->renderer = sdl3_create_renderer_for_window(win->sdl_window);
  if (!win->renderer) {
    SDL_DestroyWindow(win->sdl_window);
    free(win);
    return NULL;
  }
  SDL_SetRenderDrawBlendMode(win->renderer, SDL_BLENDMODE_BLEND);
  SDL_SetDefaultTextureScaleMode(win->renderer, SDL_SCALEMODE_LINEAR);
  SDL_StartTextInput(win->sdl_window);

  // Initialize CSD for borderless windows with appbar
  cogito_csd_init(&win->csd_state, borderless);
  if (debug_flags.debug_csd) {
    win->csd_state.debug_overlay = true;
  }
  if (borderless) {
    SDL_SetWindowHitTest(win->sdl_window, sdl3_csd_hit_test_callback, win);
  }

  // Register in window registry
  cogito_window_registry_add(&window_registry, (CogitoWindow *)win);

  if (debug_flags.debug_native) {
    void *native = sdl3_window_get_native_handle((CogitoWindow *)win);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Window %u native handle: %p",
                win->window_id, native);
  }

  return (CogitoWindow *)win;
}

static void sdl3_window_destroy(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win)
    return;

  cogito_window_registry_remove(&window_registry, window);

  if (win->renderer) {
    SDL_DestroyRenderer(win->renderer);
    win->renderer = NULL;
  }
  if (win->sdl_window) {
    SDL_StopTextInput(win->sdl_window);
    SDL_DestroyWindow(win->sdl_window);
  }

  free(win);
}

static void sdl3_window_set_size(CogitoWindow *window, int w, int h) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return;
  SDL_SetWindowSize(win->sdl_window, w, h);
  win->width = w;
  win->height = h;
}

static void sdl3_window_get_size(CogitoWindow *window, int *w, int *h) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win) {
    if (w)
      *w = 0;
    if (h)
      *h = 0;
    return;
  }
  if (win->sdl_window) {
    int tw, th;
    SDL_GetWindowSize(win->sdl_window, &tw, &th);
    win->width = tw;
    win->height = th;
  }
  if (w)
    *w = win->width;
  if (h)
    *h = win->height;
}

static void sdl3_window_set_position(CogitoWindow *window, int x, int y) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return;
  SDL_SetWindowPosition(win->sdl_window, (float)x, (float)y);
}

static void sdl3_window_get_position(CogitoWindow *window, int *x, int *y) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window) {
    if (x)
      *x = 0;
    if (y)
      *y = 0;
    return;
  }
  int ix, iy;
  SDL_GetWindowPosition(win->sdl_window, &ix, &iy);
  if (x)
    *x = ix;
  if (y)
    *y = iy;
}

static void sdl3_window_set_title(CogitoWindow *window, const char *title) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return;
  SDL_SetWindowTitle(win->sdl_window, title);
}

static void sdl3_window_show(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return;
  SDL_ShowWindow(win->sdl_window);
}

static void sdl3_window_hide(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return;
  SDL_HideWindow(win->sdl_window);
}

static void sdl3_window_raise(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return;
  SDL_RaiseWindow(win->sdl_window);
}

static void sdl3_window_minimize(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return;
  SDL_MinimizeWindow(win->sdl_window);
}

static void sdl3_window_maximize(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return;
  SDL_MaximizeWindow(win->sdl_window);
}

static void sdl3_window_restore(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return;
  SDL_RestoreWindow(win->sdl_window);
}

static bool sdl3_window_is_maximized(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return false;
  SDL_WindowFlags flags = SDL_GetWindowFlags(win->sdl_window);
  return (flags & SDL_WINDOW_MAXIMIZED) != 0;
}

static void *sdl3_window_get_native_handle(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return NULL;

#if defined(__APPLE__)
  return SDL_GetPointerProperty(SDL_GetWindowProperties(win->sdl_window),
                                SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
#elif defined(_WIN32)
  return SDL_GetPointerProperty(SDL_GetWindowProperties(win->sdl_window),
                                SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#elif defined(__linux__)
  void *wayland =
      SDL_GetPointerProperty(SDL_GetWindowProperties(win->sdl_window),
                             SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
  if (wayland)
    return wayland;
  return (void *)(uintptr_t)SDL_GetNumberProperty(
      SDL_GetWindowProperties(win->sdl_window),
      SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
#else
  return NULL;
#endif
}

static bool sdl3_window_set_icon(CogitoWindow *window, const char *path) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window || !path || !path[0])
    return false;
#if defined(COGITO_HAS_SDL3_IMAGE)
  SDL_Surface *icon = IMG_Load(path);
  if (!icon)
    return false;
  bool ok = SDL_SetWindowIcon(win->sdl_window, icon);
  SDL_DestroySurface(icon);
  return ok;
#else
  (void)win;
  (void)path;
  return false;
#endif
}

static uint32_t sdl3_window_get_id(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win)
    return 0;
  return win->window_id;
}

static bool sdl3_open_url(const char *url) {
  if (!url || !url[0])
    return false;
  return SDL_OpenURL(url);
}

static bool sdl3_set_clipboard_text(const char *text) {
  if (!text)
    return false;
  return SDL_SetClipboardText(text);
}

// ============================================================================
// Frame Rendering
// ============================================================================

// Forward declarations for functions used in begin_frame/end_frame
static double sdl3_get_time(void);
extern void cogito_icon_cache_clear(void);

static double g_frame_start_time = 0.0;
static bool g_frame_missed_deadline = false;

static void sdl3_begin_frame(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->renderer)
    return;

  // Drain autorelease pool at start of each frame to prevent RAM accumulation
  // This is critical on macOS during window drag when modal event loop prevents
  // normal drain
#if defined(__APPLE__)
  extern void cogito_frame_start(void);
  cogito_frame_start();
#endif

  // Frame Bus Method: deadline-miss flag is tracked but we no longer
  // clear ALL caches on miss (that causes a death spiral: clearing makes
  // the next frame slower → misses again → clears again → repeat).
  // Normal 7-frame LRU eviction in cogito_text_cache_frame_start() handles it.
  g_frame_missed_deadline = false;

  // Record frame start time for Frame Bus Method
  g_frame_start_time = sdl3_get_time();

  // Increment frame counter for text cache LRU
  cogito_text_cache_frame_start();

  int w, h;
  SDL_GetWindowSize(win->sdl_window, &w, &h);
  if (w != win->width || h != win->height) {
    win->width = w;
    win->height = h;
  }

  g_current_renderer = win->renderer;
  g_current_window = win;
  g_draw_color_valid = false;
  g_draw_color_renderer = win->renderer;
  sdl3_rect_batch_reset();
  g_render_state.window_width = w;
  g_render_state.window_height = h;
  SDL_SetRenderClipRect(g_current_renderer, NULL);
  scissor_stack_count = 0; // Reset scissor stack for new frame
}

static void sdl3_end_frame(CogitoWindow *window) { (void)window; }

static void sdl3_present(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->renderer)
    return;
  sdl3_rect_batch_flush();
  // Use autoreleasepool wrapper on macOS to prevent RAM balloon during window
  // drag performWindowDragWithEvent: runs a modal event loop that stops normal
  // autorelease pool drain
#if defined(__APPLE__)
  extern void cogito_render_present_with_autoreleasepool(SDL_Renderer *
                                                         renderer);
  cogito_render_present_with_autoreleasepool(win->renderer);
#else
  SDL_RenderPresent(win->renderer);
#endif

  // Frame Bus Method: check if we exceeded frame budget
  double frame_time =
      (sdl3_get_time() - g_frame_start_time) * 1000.0; // Convert to ms
  if (frame_time > COGITO_FRAME_BUDGET_MS) {
    g_frame_missed_deadline = true; // Next frame will clear caches
  }

  g_current_renderer = NULL;
  sdl3_rect_batch_reset();
  g_draw_color_valid = false;
  g_draw_color_renderer = NULL;
  if (g_current_window == win) {
    g_current_window = NULL;
  }
}

static void sdl3_clear(CogitoColor color) {
  if (!g_current_renderer)
    return;
  sdl3_rect_batch_flush();
  sdl3_set_draw_color_cached(color.r, color.g, color.b, color.a);
  SDL_RenderClear(g_current_renderer);
}

// ============================================================================
// Event Loop
// ============================================================================

// GTK-style event classification: tracks whether the last poll had any
// non-motion event (click, key, scroll, etc.) that needs a visual update.
// Readable from 14_run.inc via extern.
bool cogito_last_poll_had_non_motion = false;

static bool process_events(void) {
  // Reset per-frame state
  for (int i = 0; i < 3; i++) {
    mouse_buttons_pressed[i] = false;
    mouse_buttons_released[i] = false;
  }
  for (int i = 0; i < 512; i++) {
    keys_pressed[i] = false;
    keys_released[i] = false;
  }
  mouse_wheel = 0.0f;

  bool had_any = false;
  cogito_last_poll_had_non_motion = false;
  SDL_Event event;
#if defined(__APPLE__)
  // On macOS, wrap event polling in autoreleasepool to prevent RAM growth
  // during window drag The modal event loop during drag prevents normal
  // autorelease pool drain
  extern bool cogito_poll_event_with_autoreleasepool(SDL_Event * event);
  while (cogito_poll_event_with_autoreleasepool(&event)) {
#else
  while (SDL_PollEvent(&event)) {
#endif
    had_any = true;
    if (event.type != SDL_EVENT_MOUSE_MOTION) {
      cogito_last_poll_had_non_motion = true;
    }
    switch (event.type) {
    case SDL_EVENT_QUIT:

      // Mark all windows for close
      for (int i = 0; i < window_registry.count; i++) {
        CogitoSDL3Window *win = (CogitoSDL3Window *)window_registry.windows[i];
        if (win)
          win->should_close = true;
      }
      break;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {

      CogitoWindow *win =
          cogito_window_registry_get(&window_registry, event.window.windowID);
      if (win) {
        ((CogitoSDL3Window *)win)->should_close = true;
      }
      break;
    }

    case SDL_EVENT_WINDOW_FOCUS_GAINED: {

      CogitoWindow *win =
          cogito_window_registry_get(&window_registry, event.window.windowID);
      if (win) {
        cogito_window_registry_set_focused(&window_registry, win);
        CogitoSDL3Window *sdl_win = (CogitoSDL3Window *)win;
        if (sdl_win->sdl_window) {
          SDL_StartTextInput(sdl_win->sdl_window);
        }
      }
      break;
    }

    case SDL_EVENT_MOUSE_MOTION:
      mouse_x = (int)event.motion.x;
      mouse_y = (int)event.motion.y;
      break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:

      if (event.button.button >= 1 && event.button.button <= 3) {
        int btn = event.button.button - 1;
        mouse_buttons[btn] = true;
        mouse_buttons_pressed[btn] = true;
      }
      break;

    case SDL_EVENT_MOUSE_BUTTON_UP:

      if (event.button.button >= 1 && event.button.button <= 3) {
        int btn = event.button.button - 1;
        mouse_buttons[btn] = false;
        mouse_buttons_released[btn] = true;
      }
      break;

    case SDL_EVENT_MOUSE_WHEEL:

      mouse_wheel = event.wheel.y;
      break;

    case SDL_EVENT_KEY_DOWN:

      if (event.key.scancode < 512) {
        keys_down[event.key.scancode] = true;
        keys_pressed[event.key.scancode] = true;
      }

      // CSD debug overlay toggle: Ctrl+Shift+D
      if (debug_flags.debug_csd && !debug_flags.inspector) {
        bool ctrl =
            keys_down[SDL_SCANCODE_LCTRL] || keys_down[SDL_SCANCODE_RCTRL];
        bool shift =
            keys_down[SDL_SCANCODE_LSHIFT] || keys_down[SDL_SCANCODE_RSHIFT];
        if (ctrl && shift && event.key.scancode == SDL_SCANCODE_D) {
          // Toggle CSD overlay for focused window
          CogitoWindow *win =
              cogito_window_registry_get_focused(&window_registry);
          if (win) {
            CogitoSDL3Window *sdl_win = (CogitoSDL3Window *)win;
            cogito_csd_set_debug_overlay(&sdl_win->csd_state,
                                         !sdl_win->csd_state.debug_overlay);
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "CSD debug overlay toggled for window %u",
                        sdl_win->window_id);
          }
        }
      }
      break;

    case SDL_EVENT_KEY_UP:

      if (event.key.scancode < 512) {
        keys_down[event.key.scancode] = false;
        keys_released[event.key.scancode] = true;
      }
      break;

    case SDL_EVENT_TEXT_INPUT: {

      const char *text = event.text.text;
      if (text) {
        while (*text) {
          unsigned char c = (unsigned char)*text;
          int cp = 0, len = 0;

          if ((c & 0x80) == 0) {
            cp = c;
            len = 1;
          } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            len = 2;
          } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            len = 3;
          } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            len = 4;
          }

          for (int i = 1; i < len && text[i]; i++) {
            cp = (cp << 6) | (text[i] & 0x3F);
          }

          int next = (char_queue_tail + 1) % 16;
          if (next != char_queue_head) {
            char_queue[char_queue_tail] = cp;
            char_queue_tail = next;
          }
          text += len;
        }
      }
      break;
    }
    }
  }
  return had_any;
}

static bool sdl3_poll_events(void) { return process_events(); }

// Block until an event is available or timeout_ms elapses. Used when idle to
// avoid busy-loop CPU use.
static void sdl3_wait_event_timeout(uint32_t timeout_ms) {
  SDL_Event event;
#if defined(__APPLE__)
  extern bool cogito_wait_event_with_autoreleasepool(SDL_Event * event,
                                                     int timeout_ms);
  if (cogito_wait_event_with_autoreleasepool(&event, (int)timeout_ms)) {
    SDL_PushEvent(&event);
  }
#else
  if (SDL_WaitEventTimeout(&event, (int)timeout_ms)) {
    SDL_PushEvent(&event);
  }
#endif
}

static bool sdl3_window_should_close(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win)
    return true;

  // Check if this is the last window
  if (win->should_close && window_registry.count <= 1) {
    return true;
  }

  return win->should_close;
}

// ============================================================================
// Input
// ============================================================================

static void sdl3_get_mouse_position(int *x, int *y) {
  // During drawing, route coordinates to the window being rendered.
  if (g_current_window) {
    sdl3_get_mouse_position_in_window((CogitoWindow *)g_current_window, x, y);
    return;
  }
  if (x)
    *x = mouse_x;
  if (y)
    *y = mouse_y;
}

static void sdl3_get_mouse_position_in_window(CogitoWindow *window, int *x,
                                              int *y) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window) {
    if (x)
      *x = 0;
    if (y)
      *y = 0;
    return;
  }
  // When this window has mouse focus, use SDL's window-relative cached
  // coordinates.
  SDL_Window *focused = SDL_GetMouseFocus();
  if (focused == win->sdl_window) {
    float lx = 0.0f;
    float ly = 0.0f;
    SDL_GetMouseState(&lx, &ly);
    if (x)
      *x = (int)lx;
    if (y)
      *y = (int)ly;
    return;
  }
  float gx = 0.0f;
  float gy = 0.0f;
  SDL_GetGlobalMouseState(&gx, &gy);
  int wx = 0;
  int wy = 0;
  SDL_GetWindowPosition(win->sdl_window, &wx, &wy);

  // SDL documents these in the same desktop coordinate space; convert to
  // window-local coordinates by subtracting the window origin.
  if (x)
    *x = (int)(gx - (float)wx);
  if (y)
    *y = (int)(gy - (float)wy);
}

static bool sdl3_is_mouse_button_down(int button) {
  if (button < 0 || button >= 3)
    return false;
  return mouse_buttons[button];
}

static bool sdl3_is_mouse_button_pressed(int button) {
  if (button < 0 || button >= 3)
    return false;
  return mouse_buttons_pressed[button];
}

static bool sdl3_is_mouse_button_released(int button) {
  if (button < 0 || button >= 3)
    return false;
  return mouse_buttons_released[button];
}

static float sdl3_get_mouse_wheel_move(void) { return mouse_wheel; }

static bool sdl3_is_key_down(int key) {
  if (key < 0 || key >= 512)
    return false;
  return keys_down[key];
}

static bool sdl3_is_key_pressed(int key) {
  if (key < 0 || key >= 512)
    return false;
  return keys_pressed[key];
}

static bool sdl3_is_key_released(int key) {
  if (key < 0 || key >= 512)
    return false;
  return keys_released[key];
}

static int sdl3_get_char_pressed(void) {
  if (char_queue_head == char_queue_tail)
    return 0;
  int cp = char_queue[char_queue_head];
  char_queue_head = (char_queue_head + 1) % 16;
  return cp;
}

// ============================================================================
// Time
// ============================================================================

static double sdl3_get_time(void) {
  return ((double)SDL_GetTicks() / 1000.0) - start_time;
}

static void sdl3_sleep(uint32_t ms) { SDL_Delay(ms); }

static SDL_Renderer *sdl3_active_renderer(void) {
  if (g_current_renderer)
    return g_current_renderer;
  CogitoWindow *focused = cogito_window_registry_get_focused(&window_registry);
  if (focused) {
    CogitoSDL3Window *win = (CogitoSDL3Window *)focused;
    if (win->renderer)
      return win->renderer;
  }
  if (window_registry.count > 0 && window_registry.windows[0]) {
    CogitoSDL3Window *win = (CogitoSDL3Window *)window_registry.windows[0];
    if (win->renderer)
      return win->renderer;
  }
  return NULL;
}

// ============================================================================
// Drawing
// ============================================================================

static void sdl3_draw_rect(int x, int y, int w, int h, CogitoColor color) {
  if (!g_current_renderer || w <= 0 || h <= 0)
    return;
  sdl3_rect_batch_push(g_current_renderer, x, y, w, h, color);
}

static void sdl3_draw_point_alpha(int x, int y, CogitoColor color,
                                  float coverage) {
  if (!g_current_renderer || color.a == 0)
    return;
  sdl3_rect_batch_flush();
  if (coverage <= 0.0f)
    return;
  if (coverage > 1.0f)
    coverage = 1.0f;
  uint8_t a = (uint8_t)lroundf((float)color.a * coverage);
  if (a == 0)
    return;
  sdl3_set_draw_color_cached(color.r, color.g, color.b, a);
  SDL_RenderPoint(g_current_renderer, (float)x, (float)y);
}

static void sdl3_draw_hspan_aa(int y, float left, float right,
                               CogitoColor color) {
  if (!g_current_renderer || color.a == 0)
    return;
  sdl3_rect_batch_flush();
  if (right < left)
    return;
  int full_l = (int)ceilf(left);
  int full_r = (int)floorf(right);

  if (full_l > full_r) {
    return;
  }

  sdl3_set_draw_color_cached(color.r, color.g, color.b, color.a);
  SDL_RenderLine(g_current_renderer, (float)full_l, (float)y, (float)full_r,
                 (float)y);
}

static float sdl3_aa_coverage_curve(float cov) {
  if (cov <= 0.05f)
    return 0.0f;
  if (cov >= 1.0f)
    return 1.0f;
  // Sharper falloff so AA is present but less visually soft.
  return powf(cov, 0.7f);
}

static float sdl3_round_inset_for_row(int row, int h, float r) {
  if (r <= 0.0f || row < 0 || row >= h)
    return 0.0f;
  float y_center = (float)row + 0.5f;
  float dy = 0.0f;
  float bottom_start = (float)h - r;
  if (y_center < r) {
    dy = r - y_center;
  } else if (y_center > bottom_start) {
    dy = y_center - bottom_start;
  } else {
    return 0.0f;
  }
  float inside = r * r - dy * dy;
  if (inside < 0.0f)
    inside = 0.0f;
  float dx = sqrtf(inside);
  float inset = r - dx;
  if (inset < 0.0f)
    inset = 0.0f;
  return inset;
}

static int sdl3_round_segments_for_radius(float radius) {
  int segments = (int)ceilf(radius * 0.9f);
  if (segments < 6)
    segments = 6;
  if (segments > 48)
    segments = 48;
  return segments;
}

static int sdl3_circle_segments_for_radius(float radius) {
  int segments = (int)ceilf(radius * 1.2f);
  if (segments < 12)
    segments = 12;
  if (segments > 128)
    segments = 128;
  return segments;
}

static float sdl3_clampf(float x, float a, float b) {
  return (x < a) ? a : ((x > b) ? b : x);
}

// ============================================================================
// Rounded Rectangle Reusable Buffers (Performance Optimization)
// ============================================================================

static SDL_FPoint *g_rounded_rect_pts = NULL;
static int g_rounded_rect_pts_cap = 0;

static SDL_FPoint *g_rounded_rect_pts2 = NULL; // For outline inner perimeter
static int g_rounded_rect_pts2_cap = 0;

static SDL_Vertex *g_rounded_rect_verts = NULL;
static int g_rounded_rect_verts_cap = 0;

static int *g_rounded_rect_indices = NULL;
static int g_rounded_rect_indices_cap = 0;

#define ROUNDED_RECT_INITIAL_CAP 64

static bool cogito_ensure_cap(void **ptr, int *cap, int needed,
                              size_t elem_size) {
  if (*cap >= needed)
    return true;

  int new_cap = *cap ? *cap * 2 : ROUNDED_RECT_INITIAL_CAP;
  while (new_cap < needed)
    new_cap *= 2;

  void *new_ptr = realloc(*ptr, (size_t)new_cap * elem_size);
  if (!new_ptr)
    return false;

  *ptr = new_ptr;
  *cap = new_cap;
  return true;
}

static void sdl3_free_geometry_buffers(void) {
  free(g_rounded_rect_pts);
  g_rounded_rect_pts = NULL;
  g_rounded_rect_pts_cap = 0;

  free(g_rounded_rect_pts2);
  g_rounded_rect_pts2 = NULL;
  g_rounded_rect_pts2_cap = 0;

  free(g_rounded_rect_verts);
  g_rounded_rect_verts = NULL;
  g_rounded_rect_verts_cap = 0;

  free(g_rounded_rect_indices);
  g_rounded_rect_indices = NULL;
  g_rounded_rect_indices_cap = 0;
}

static int sdl3_build_rounded_rect_perimeter(float x, float y, float w, float h,
                                             float radius, int segments,
                                             SDL_FPoint **out_pts,
                                             int *out_count) {
  if (!out_pts || !out_count)
    return -1;
  if (segments < 1)
    segments = 1;

  float max_r = 0.5f * ((w < h) ? w : h);
  float r = sdl3_clampf(radius, 0.0f, max_r);

  const int count = 4 * segments + 4;

  // Use static buffer instead of malloc
  if (!cogito_ensure_cap((void **)&g_rounded_rect_pts, &g_rounded_rect_pts_cap,
                         count, sizeof(SDL_FPoint))) {
    return -1;
  }

  float left = x + r;
  float right = x + w - r;
  float top = y + r;
  float bottom = y + h - r;

  struct Arc {
    float cx, cy, a0, a1;
  } arcs[4] = {
      {right, top, (float)(-0.5 * M_PI), (float)(0.0 * M_PI)},
      {right, bottom, (float)(0.0 * M_PI), (float)(0.5 * M_PI)},
      {left, bottom, (float)(0.5 * M_PI), (float)(1.0 * M_PI)},
      {left, top, (float)(1.0 * M_PI), (float)(1.5 * M_PI)},
  };

  int out = 0;
  for (int corner = 0; corner < 4; corner++) {
    float cx = arcs[corner].cx;
    float cy = arcs[corner].cy;
    float a0 = arcs[corner].a0;
    float a1 = arcs[corner].a1;

    for (int i = 0; i <= segments; i++) {
      if (corner > 0 && i == 0)
        continue;
      float t = (float)i / (float)segments;
      float a = a0 + (a1 - a0) * t;
      float px = cx + cosf(a) * r;
      float py = cy + sinf(a) * r;
      g_rounded_rect_pts[out++] = (SDL_FPoint){px, py};
    }
  }

  *out_pts = g_rounded_rect_pts;
  *out_count = out;
  return 0;
}

static SDL_FColor sdl3_fcolor(CogitoColor c) {
  return (SDL_FColor){(float)c.r / 255.0f, (float)c.g / 255.0f,
                      (float)c.b / 255.0f, (float)c.a / 255.0f};
}

static bool sdl3_draw_filled_rounded_rect_fan(int x, int y, int w, int h,
                                              float radius, int segments,
                                              CogitoColor color) {
  SDL_FPoint *perim = NULL;
  int perim_count = 0;
  if (sdl3_build_rounded_rect_perimeter((float)x, (float)y, (float)w, (float)h,
                                        radius, segments, &perim,
                                        &perim_count) != 0) {
    return false;
  }

  SDL_FColor c = sdl3_fcolor(color);
  const int vert_count = 1 + perim_count;
  const int index_count = 3 * perim_count;

  // Use static buffers instead of malloc
  if (!cogito_ensure_cap((void **)&g_rounded_rect_verts,
                         &g_rounded_rect_verts_cap, vert_count,
                         sizeof(SDL_Vertex)) ||
      !cogito_ensure_cap((void **)&g_rounded_rect_indices,
                         &g_rounded_rect_indices_cap, index_count,
                         sizeof(int))) {
    return false;
  }

  float cx = (float)x + (float)w * 0.5f;
  float cy = (float)y + (float)h * 0.5f;
  g_rounded_rect_verts[0] =
      (SDL_Vertex){.position = {cx, cy}, .color = c, .tex_coord = {0.0f, 0.0f}};

  for (int i = 0; i < perim_count; i++) {
    g_rounded_rect_verts[1 + i] = (SDL_Vertex){
        .position = perim[i], .color = c, .tex_coord = {0.0f, 0.0f}};
  }

  int ii = 0;
  for (int i = 0; i < perim_count; i++) {
    int b = 1 + i;
    int cidx = 1 + ((i + 1) % perim_count);
    g_rounded_rect_indices[ii++] = 0;
    g_rounded_rect_indices[ii++] = b;
    g_rounded_rect_indices[ii++] = cidx;
  }

  bool ok = SDL_RenderGeometry(g_current_renderer, NULL, g_rounded_rect_verts,
                               vert_count, g_rounded_rect_indices, index_count);
  // No free() calls - buffers are reused
  return ok;
}

static bool sdl3_draw_filled_circle_fan(int x, int y, float radius,
                                        int segments, CogitoColor color) {
  if (!g_current_renderer || radius <= 0.0f)
    return false;
  if (segments < 3)
    segments = 3;

  const int ring_pts = segments + 1;
  const int vert_count = 1 + ring_pts;
  const int index_count = segments * 3;
  if (!cogito_ensure_cap((void **)&g_rounded_rect_verts, &g_rounded_rect_verts_cap,
                         vert_count, sizeof(SDL_Vertex)) ||
      !cogito_ensure_cap((void **)&g_rounded_rect_indices,
                         &g_rounded_rect_indices_cap,
                         index_count, sizeof(int))) {
    return false;
  }

  SDL_FColor c = sdl3_fcolor(color);
  g_rounded_rect_verts[0] = (SDL_Vertex){
      .position = {(float)x, (float)y}, .color = c, .tex_coord = {0.0f, 0.0f}};

  float ux = 1.0f;
  float uy = 0.0f;
  float da = (float)(2.0 * M_PI) / (float)segments;
  float cda = cosf(da);
  float sda = sinf(da);
  for (int i = 0; i < segments; i++) {
    g_rounded_rect_verts[1 + i] =
        (SDL_Vertex){.position = {(float)x + ux * radius, (float)y + uy * radius},
                     .color = c,
                     .tex_coord = {0.0f, 0.0f}};
    float nux = ux * cda - uy * sda;
    float nuy = ux * sda + uy * cda;
    ux = nux;
    uy = nuy;
  }
  g_rounded_rect_verts[1 + segments] = g_rounded_rect_verts[1];

  int ii = 0;
  for (int i = 0; i < segments; i++) {
    g_rounded_rect_indices[ii++] = 0;
    g_rounded_rect_indices[ii++] = 1 + i;
    g_rounded_rect_indices[ii++] = 1 + i + 1;
  }

  return SDL_RenderGeometry(g_current_renderer, NULL, g_rounded_rect_verts,
                            vert_count, g_rounded_rect_indices, index_count);
}

static bool sdl3_draw_circle_ring(int x, int y, float radius, float thickness,
                                  int segments, CogitoColor color) {
  if (!g_current_renderer || radius <= 0.0f || thickness <= 0.0f)
    return false;
  if (segments < 3)
    segments = 3;

  float r_outer = radius;
  float r_inner = radius - thickness;
  if (r_inner <= 0.0f) {
    return sdl3_draw_filled_circle_fan(x, y, r_outer, segments, color);
  }

  const int ring_pts = segments + 1;
  const int vert_count = ring_pts * 2;
  const int index_count = segments * 6;
  if (!cogito_ensure_cap((void **)&g_rounded_rect_verts, &g_rounded_rect_verts_cap,
                         vert_count, sizeof(SDL_Vertex)) ||
      !cogito_ensure_cap((void **)&g_rounded_rect_indices,
                         &g_rounded_rect_indices_cap,
                         index_count, sizeof(int))) {
    return false;
  }

  SDL_FColor c = sdl3_fcolor(color);
  float ux = 1.0f;
  float uy = 0.0f;
  float da = (float)(2.0 * M_PI) / (float)segments;
  float cda = cosf(da);
  float sda = sinf(da);

  for (int i = 0; i < segments; i++) {
    g_rounded_rect_verts[i] =
        (SDL_Vertex){.position = {(float)x + ux * r_outer, (float)y + uy * r_outer},
                     .color = c,
                     .tex_coord = {0.0f, 0.0f}};
    g_rounded_rect_verts[ring_pts + i] =
        (SDL_Vertex){.position = {(float)x + ux * r_inner, (float)y + uy * r_inner},
                     .color = c,
                     .tex_coord = {0.0f, 0.0f}};
    float nux = ux * cda - uy * sda;
    float nuy = ux * sda + uy * cda;
    ux = nux;
    uy = nuy;
  }

  g_rounded_rect_verts[segments] = g_rounded_rect_verts[0];
  g_rounded_rect_verts[ring_pts + segments] = g_rounded_rect_verts[ring_pts];

  int ii = 0;
  for (int i = 0; i < segments; i++) {
    int o0 = i;
    int o1 = i + 1;
    int i0 = ring_pts + i;
    int i1 = ring_pts + i + 1;
    g_rounded_rect_indices[ii++] = o0;
    g_rounded_rect_indices[ii++] = o1;
    g_rounded_rect_indices[ii++] = i1;
    g_rounded_rect_indices[ii++] = o0;
    g_rounded_rect_indices[ii++] = i1;
    g_rounded_rect_indices[ii++] = i0;
  }

  return SDL_RenderGeometry(g_current_renderer, NULL, g_rounded_rect_verts,
                            vert_count, g_rounded_rect_indices, index_count);
}

static bool sdl3_draw_rounded_rect_outline(int x, int y, int w, int h,
                                           float radius, float thickness,
                                           int segments, CogitoColor color) {
  if (!g_current_renderer || w <= 0 || h <= 0)
    return true;
  if (segments < 1)
    segments = 1;

  float half_min = 0.5f * (float)((w < h) ? w : h);
  float t = sdl3_clampf(thickness, 0.0f, half_min);
  float r_outer = sdl3_clampf(radius, 0.0f, half_min);
  if (t <= 0.0001f)
    return true;

  float xi = (float)x + t;
  float yi = (float)y + t;
  float wi = (float)w - 2.0f * t;
  float hi = (float)h - 2.0f * t;

  if (wi <= 0.0001f || hi <= 0.0001f) {
    return sdl3_draw_filled_rounded_rect_fan(x, y, w, h, r_outer, segments,
                                             color);
  }

  float r_inner = r_outer - t;
  if (r_inner < 0.0f)
    r_inner = 0.0f;

  SDL_FPoint *outer_pts = NULL;
  SDL_FPoint *inner_pts = NULL;
  int outer_count = 0;
  int inner_count = 0;

  // Build outer perimeter using primary buffer
  if (sdl3_build_rounded_rect_perimeter((float)x, (float)y, (float)w, (float)h,
                                        r_outer, segments, &outer_pts,
                                        &outer_count) != 0) {
    return false;
  }

  // Build inner perimeter using secondary buffer
  const int count = 4 * segments + 4;
  if (!cogito_ensure_cap((void **)&g_rounded_rect_pts2,
                         &g_rounded_rect_pts2_cap, count, sizeof(SDL_FPoint))) {
    return false;
  }

  // Build inner perimeter manually to use secondary buffer
  float max_r_inner = 0.5f * ((wi < hi) ? wi : hi);
  float r_inner_clamped = sdl3_clampf(r_inner, 0.0f, max_r_inner);

  float left = xi + r_inner_clamped;
  float right = xi + wi - r_inner_clamped;
  float top = yi + r_inner_clamped;
  float bottom = yi + hi - r_inner_clamped;

  struct Arc {
    float cx, cy, a0, a1;
  } arcs[4] = {
      {right, top, (float)(-0.5 * M_PI), (float)(0.0 * M_PI)},
      {right, bottom, (float)(0.0 * M_PI), (float)(0.5 * M_PI)},
      {left, bottom, (float)(0.5 * M_PI), (float)(1.0 * M_PI)},
      {left, top, (float)(1.0 * M_PI), (float)(1.5 * M_PI)},
  };

  inner_count = 0;
  for (int corner = 0; corner < 4; corner++) {
    float cx = arcs[corner].cx;
    float cy = arcs[corner].cy;
    float a0 = arcs[corner].a0;
    float a1 = arcs[corner].a1;

    for (int i = 0; i <= segments; i++) {
      if (corner > 0 && i == 0)
        continue;
      float t_val = (float)i / (float)segments;
      float a = a0 + (a1 - a0) * t_val;
      float px = cx + cosf(a) * r_inner_clamped;
      float py = cy + sinf(a) * r_inner_clamped;
      g_rounded_rect_pts2[inner_count++] = (SDL_FPoint){px, py};
    }
  }
  inner_pts = g_rounded_rect_pts2;

  int n = (outer_count < inner_count) ? outer_count : inner_count;
  if (n < 3) {
    return false;
  }

  SDL_FColor c = sdl3_fcolor(color);
  const int vert_count = 2 * n;
  const int index_count = 6 * n;

  // Use static buffers
  if (!cogito_ensure_cap((void **)&g_rounded_rect_verts,
                         &g_rounded_rect_verts_cap, vert_count,
                         sizeof(SDL_Vertex)) ||
      !cogito_ensure_cap((void **)&g_rounded_rect_indices,
                         &g_rounded_rect_indices_cap, index_count,
                         sizeof(int))) {
    return false;
  }

  for (int i = 0; i < n; i++) {
    g_rounded_rect_verts[i] = (SDL_Vertex){
        .position = outer_pts[i], .color = c, .tex_coord = {0.0f, 0.0f}};
    g_rounded_rect_verts[n + i] = (SDL_Vertex){
        .position = inner_pts[i], .color = c, .tex_coord = {0.0f, 0.0f}};
  }

  int ii = 0;
  for (int i = 0; i < n; i++) {
    int next = (i + 1) % n;
    int o0 = i;
    int o1 = next;
    int i0 = n + i;
    int i1 = n + next;
    g_rounded_rect_indices[ii++] = o0;
    g_rounded_rect_indices[ii++] = o1;
    g_rounded_rect_indices[ii++] = i1;
    g_rounded_rect_indices[ii++] = o0;
    g_rounded_rect_indices[ii++] = i1;
    g_rounded_rect_indices[ii++] = i0;
  }

  bool ok = SDL_RenderGeometry(g_current_renderer, NULL, g_rounded_rect_verts,
                               vert_count, g_rounded_rect_indices, index_count);
  // No free() calls - buffers are reused
  return ok;
}

static void sdl3_draw_rect_rounded(int x, int y, int w, int h,
                                   CogitoColor color, float roundness) {
  sdl3_rect_batch_flush();
  if (!g_current_renderer || w <= 0 || h <= 0)
    return;
  int min_dim = w < h ? w : h;
  float r = roundness * (float)min_dim * 0.5f;
  if (r < 0.5f) {
    sdl3_draw_rect(x, y, w, h, color);
    return;
  }
  float max_r = (float)min_dim * 0.5f;
  if (r > max_r)
    r = max_r;
  int segments = sdl3_round_segments_for_radius(r);
  if (!sdl3_draw_filled_rounded_rect_fan(x, y, w, h, r, segments, color)) {
    // Fallback if geometry allocation fails.
    for (int iy = 0; iy < h; iy++) {
      float inset = sdl3_round_inset_for_row(iy, h, r);
      float left = (float)x + inset;
      float right = (float)(x + w - 1) - inset;
      sdl3_draw_hspan_aa(y + iy, left, right, color);
    }
  }
}

static void sdl3_draw_line(int x1, int y1, int x2, int y2, CogitoColor color,
                           int thickness) {
  sdl3_rect_batch_flush();
  if (!g_current_renderer)
    return;
  if (thickness < 1)
    thickness = 1;
  sdl3_set_draw_color_cached(color.r, color.g, color.b, color.a);
  if (thickness == 1) {
    SDL_RenderLine(g_current_renderer, (float)x1, (float)y1, (float)x2,
                   (float)y2);
    return;
  }

  float dx = (float)(x2 - x1);
  float dy = (float)(y2 - y1);
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.0001f) {
    SDL_RenderPoint(g_current_renderer, (float)x1, (float)y1);
    return;
  }
  float nx = -dy / len;
  float ny = dx / len;
  for (int i = 0; i < thickness; i++) {
    float off = (float)i - ((float)thickness - 1.0f) * 0.5f;
    SDL_RenderLine(g_current_renderer, (float)x1 + nx * off,
                   (float)y1 + ny * off, (float)x2 + nx * off,
                   (float)y2 + ny * off);
  }
}

static void sdl3_draw_rect_lines(int x, int y, int w, int h, CogitoColor color,
                                 int thickness) {
  sdl3_rect_batch_flush();
  if (!g_current_renderer || w <= 0 || h <= 0 || thickness <= 0)
    return;
  if (thickness * 2 >= w || thickness * 2 >= h) {
    sdl3_draw_rect(x, y, w, h, color);
    return;
  }

  sdl3_set_draw_color_cached(color.r, color.g, color.b, color.a);
  SDL_FRect top = {(float)x, (float)y, (float)w, (float)thickness};
  SDL_FRect bottom = {(float)x, (float)(y + h - thickness), (float)w,
                      (float)thickness};
  SDL_RenderFillRect(g_current_renderer, &top);
  SDL_RenderFillRect(g_current_renderer, &bottom);

  int inner_h = h - thickness * 2;
  if (inner_h > 0) {
    SDL_FRect left = {(float)x, (float)(y + thickness), (float)thickness,
                      (float)inner_h};
    SDL_FRect right = {(float)(x + w - thickness), (float)(y + thickness),
                       (float)thickness, (float)inner_h};
    SDL_RenderFillRect(g_current_renderer, &left);
    SDL_RenderFillRect(g_current_renderer, &right);
  }
}

static void sdl3_draw_circle_outline_aa(int cx, int cy, float radius,
                                        CogitoColor color) {
  if (!g_current_renderer || color.a == 0)
    return;
  if (radius <= 0.0f) {
    sdl3_draw_point_alpha(cx, cy, color, 1.0f);
    return;
  }
  int ir = (int)ceilf(radius);
  float r2 = radius * radius;
  for (int oy = -ir; oy <= ir; oy++) {
    float dy = fabsf((float)oy) + 0.5f;
    float rem = r2 - dy * dy;
    if (rem < 0.0f)
      continue;
    float fx = sqrtf(rem);
    int ix = (int)floorf(fx);
    float frac = fx - (float)ix;
    float cov0 = sdl3_aa_coverage_curve(1.0f - frac);
    float cov1 = sdl3_aa_coverage_curve(frac);
    int py = cy + oy;

    int pxr0 = cx + ix;
    int pxr1 = cx + ix + 1;
    int pxl0 = cx - ix;
    int pxl1 = cx - ix - 1;

    if (cov0 > 0.001f) {
      sdl3_draw_point_alpha(pxr0, py, color, cov0);
      if (ix != 0)
        sdl3_draw_point_alpha(pxl0, py, color, cov0);
    }
    if (cov1 > 0.001f) {
      sdl3_draw_point_alpha(pxr1, py, color, cov1);
      sdl3_draw_point_alpha(pxl1, py, color, cov1);
    }
  }
}

static void sdl3_draw_rect_rounded_lines(int x, int y, int w, int h,
                                         CogitoColor color, float roundness,
                                         int thickness) {
  sdl3_rect_batch_flush();
  if (!g_current_renderer || w <= 0 || h <= 0 || thickness <= 0)
    return;
  int min_dim = w < h ? w : h;
  float r = roundness * (float)min_dim * 0.5f;
  if (r < 0.5f) {
    sdl3_draw_rect_lines(x, y, w, h, color, thickness);
    return;
  }
  int segments = sdl3_round_segments_for_radius(r);
  if (!sdl3_draw_rounded_rect_outline(x, y, w, h, r, (float)thickness, segments,
                                      color)) {
    sdl3_draw_rect_lines(x, y, w, h, color, thickness);
  }
}

static void sdl3_draw_circle(int x, int y, float radius, CogitoColor color) {
  sdl3_rect_batch_flush();
  if (!g_current_renderer || radius <= 0.0f)
    return;
  float r = radius;
  if (r < 0.5f)
    r = 0.5f;
  if (r >= 2.0f) {
    int segments = sdl3_circle_segments_for_radius(r);
    if (sdl3_draw_filled_circle_fan(x, y, r, segments, color)) {
      return;
    }
  }
  int y0 = (int)floorf((float)y - r - 1.0f);
  int y1 = (int)ceilf((float)y + r + 1.0f);
  for (int py = y0; py <= y1; py++) {
    float dy = fabsf(((float)py + 0.5f) - (float)y);
    if (dy > r + 1.0f)
      continue;
    float inside = r * r - dy * dy;
    if (inside < 0.0f) {
      if (dy > r)
        continue;
      inside = 0.0f;
    }
    float span = sqrtf(inside);
    sdl3_draw_hspan_aa(py, (float)x - span, (float)x + span, color);
  }
}

static void sdl3_draw_circle_lines(int x, int y, float radius,
                                   CogitoColor color, int thickness) {
  sdl3_rect_batch_flush();
  if (!g_current_renderer || radius <= 0.0f)
    return;
  if (thickness < 1)
    thickness = 1;
  if (thickness > 1 && radius >= 6.0f) {
    int segments = sdl3_circle_segments_for_radius(radius);
    if (sdl3_draw_circle_ring(x, y, radius, (float)thickness, segments,
                              color)) {
      return;
    }
  }
  int base_r = (int)lroundf(radius);
  for (int t = 0; t < thickness; t++) {
    int rr = base_r - t;
    if (rr < 0)
      break;
    sdl3_draw_circle_outline_aa(x, y, (float)rr, color);
  }
}

// ============================================================================
// Text (SDL_ttf)
// ============================================================================

static bool sdl3_str_eq_ci(const char *a, const char *b) {
  if (!a || !b)
    return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
      return false;
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

static bool sdl3_str_contains_ci(const char *haystack, const char *needle) {
  if (!haystack || !needle || !needle[0])
    return false;
  size_t nlen = strlen(needle);
  for (const char *p = haystack; *p; p++) {
    size_t i = 0;
    while (i < nlen && p[i] &&
           tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
      i++;
    }
    if (i == nlen)
      return true;
  }
  return false;
}

static int sdl3_parse_hinting(const char *name, int fallback) {
  if (!name || !name[0])
    return fallback;
  if (sdl3_str_eq_ci(name, "normal"))
    return TTF_HINTING_NORMAL;
  if (sdl3_str_eq_ci(name, "light"))
    return TTF_HINTING_LIGHT;
  if (sdl3_str_eq_ci(name, "light-subpixel") ||
      sdl3_str_eq_ci(name, "subpixel"))
    return TTF_HINTING_LIGHT_SUBPIXEL;
  if (sdl3_str_eq_ci(name, "mono") || sdl3_str_eq_ci(name, "monochrome"))
    return TTF_HINTING_MONO;
  if (sdl3_str_eq_ci(name, "none"))
    return TTF_HINTING_NONE;
  return fallback;
}

static bool sdl3_font_path_looks_serif(const char *path) {
  if (!path || !path[0])
    return false;
  return sdl3_str_contains_ci(path, "serif") ||
         sdl3_str_contains_ci(path, "times") ||
         sdl3_str_contains_ci(path, "georgia") ||
         sdl3_str_contains_ci(path, "newyork") ||
         sdl3_str_contains_ci(path, "garamond") ||
         sdl3_str_contains_ci(path, "baskerville") ||
         sdl3_str_contains_ci(path, "palatino") ||
         sdl3_str_contains_ci(path, "cambria");
}

static int sdl3_font_hinting_for_path(const char *path) {
  int hinting = TTF_HINTING_LIGHT_SUBPIXEL;
  const char *global_override = getenv("COGITO_FONT_HINTING");
  hinting = sdl3_parse_hinting(global_override, hinting);
  if (sdl3_font_path_looks_serif(path)) {
    // Serif faces retain small anti-aliased detail better with light hinting.
    int serif_hinting = TTF_HINTING_LIGHT;
    const char *serif_override = getenv("COGITO_FONT_HINTING_SERIF");
    serif_hinting = sdl3_parse_hinting(serif_override, serif_hinting);
    return serif_hinting;
  }
  return hinting;
}

static CogitoFont *sdl3_font_load(const char *path, int size) {
  if (!path || !path[0] || size <= 0)
    return NULL;

  TTF_Font *ttf = TTF_OpenFont(path, size);
  if (!ttf) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_OpenFont failed: %s",
                 SDL_GetError());
    return NULL;
  }

  CogitoSDL3Font *font = calloc(1, sizeof(CogitoSDL3Font));
  if (!font) {
    TTF_CloseFont(ttf);
    return NULL;
  }

  font->path = strdup(path);
  font->size = size;
  font->ttf_font = ttf;
  TTF_SetFontKerning(ttf, true);
  TTF_SetFontHinting(ttf, sdl3_font_hinting_for_path(path));
  font->ascent = TTF_GetFontAscent(ttf);
  font->descent = TTF_GetFontDescent(ttf);
  font->height = TTF_GetFontHeight(ttf);

  return (CogitoFont *)font;
}

static CogitoFont *sdl3_font_load_face(const char *path, int size,
                                       int face_index) {
  if (!path || !path[0] || size <= 0)
    return NULL;

  SDL_PropertiesID props = SDL_CreateProperties();
  if (!props)
    return NULL;

  SDL_SetStringProperty(props, TTF_PROP_FONT_CREATE_FILENAME_STRING, path);
  SDL_SetFloatProperty(props, TTF_PROP_FONT_CREATE_SIZE_FLOAT, (float)size);
  SDL_SetNumberProperty(props, TTF_PROP_FONT_CREATE_FACE_NUMBER,
                        (Sint64)face_index);

  TTF_Font *ttf = TTF_OpenFontWithProperties(props);
  SDL_DestroyProperties(props);

  if (!ttf)
    return NULL;

  CogitoSDL3Font *font = calloc(1, sizeof(CogitoSDL3Font));
  if (!font) {
    TTF_CloseFont(ttf);
    return NULL;
  }

  font->path = strdup(path);
  font->size = size;
  font->ttf_font = ttf;
  TTF_SetFontKerning(ttf, true);
  TTF_SetFontHinting(ttf, sdl3_font_hinting_for_path(path));
  font->ascent = TTF_GetFontAscent(ttf);
  font->descent = TTF_GetFontDescent(ttf);
  font->height = TTF_GetFontHeight(ttf);

  return (CogitoFont *)font;
}

static void sdl3_font_unload(CogitoFont *font) {
  CogitoSDL3Font *f = (CogitoSDL3Font *)font;
  if (!f)
    return;
  if (f->ttf_font)
    TTF_CloseFont(f->ttf_font);
  free(f->path);
  free(f);
}

static void *sdl3_font_get_internal_face(CogitoFont *font) {
  CogitoSDL3Font *f = (CogitoSDL3Font *)font;
  if (!f || !f->ttf_font)
    return NULL;
  SDL_PropertiesID props = TTF_GetFontProperties(f->ttf_font);
  if (!props)
    return NULL;
  return SDL_GetPointerProperty(props, "SDL_ttf.font.face", NULL);
}

// Set a variable font axis value at runtime using FreeType design-coordinate
// API. axis_tag: 4-byte OpenType tag big-endian (e.g. 0x77676874 = 'wght',
// 0x77647468 = 'wdth'). value: CSS-style weight (100-900) / width (75-125) -
// multiplied to FT 16.16 fixed inside. Uses the internal FT_Face obtained via
// SDL3_ttf's font properties.
static bool sdl3_font_set_variation(CogitoFont *font, uint32_t axis_tag,
                                    float value) {
  CogitoSDL3Font *f = (CogitoSDL3Font *)font;
  if (!f || !f->ttf_font)
    return false;

  SDL_PropertiesID props = TTF_GetFontProperties(f->ttf_font);
  if (!props)
    return false;
  FT_Face face =
      (FT_Face)SDL_GetPointerProperty(props, "SDL_ttf.font.face", NULL);
  if (!face)
    return false;
  if (!(face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS))
    return false;

  // Use a module-level FT_Library — face->driver->root.library is opaque.
  static FT_Library s_ft_lib = NULL;
  if (!s_ft_lib)
    FT_Init_FreeType(&s_ft_lib);
  FT_Library lib = s_ft_lib;
  if (!lib)
    return false;

  FT_MM_Var *mm = NULL;
  if (FT_Get_MM_Var(face, &mm) != 0 || !mm)
    return false;

  int axis_index = -1;
  FT_ULong ft_tag = (FT_ULong)axis_tag;
  for (FT_UInt i = 0; i < mm->num_axis; i++) {
    if (mm->axis[i].tag == ft_tag) {
      axis_index = (int)i;
      break;
    }
  }
  if (axis_index < 0) {
    FT_Done_MM_Var(lib, mm);
    return false;
  }

  FT_Fixed *coords = (FT_Fixed *)malloc(sizeof(FT_Fixed) * mm->num_axis);
  if (!coords) {
    FT_Done_MM_Var(lib, mm);
    return false;
  }
  for (FT_UInt i = 0; i < mm->num_axis; i++)
    coords[i] = mm->axis[i].def;

  FT_Fixed desired =
      (FT_Fixed)((long)(value) << 16); // convert to 16.16 fixed-point
  FT_Fixed min = mm->axis[axis_index].minimum;
  FT_Fixed max = mm->axis[axis_index].maximum;
  if (desired < min)
    desired = min;
  if (desired > max)
    desired = max;
  coords[axis_index] = desired;

  bool ok = (FT_Set_Var_Design_Coordinates(face, mm->num_axis, coords) == 0);
  free(coords);
  FT_Done_MM_Var(lib, mm);
  return ok;
}

static void sdl3_font_get_metrics(CogitoFont *font, int *ascent, int *descent,
                                  int *height) {
  CogitoSDL3Font *f = (CogitoSDL3Font *)font;
  if (!f) {
    if (ascent)
      *ascent = 0;
    if (descent)
      *descent = 0;
    if (height)
      *height = 0;
    return;
  }
  if (ascent)
    *ascent = f->ascent;
  if (descent)
    *descent = f->descent;
  if (height)
    *height = f->height;
}

static int sdl3_text_measure_width(CogitoFont *font, const char *text,
                                   int size) {
  CogitoSDL3Font *f = (CogitoSDL3Font *)font;
  if (!f || !f->ttf_font || !text || !text[0])
    return 0;

  int w, h;
  if (!TTF_GetStringSize(f->ttf_font, text, 0, &w, &h))
    return 0;
  (void)h;
  (void)size;
  return w;
}

static int sdl3_text_measure_height(CogitoFont *font, int size) {
  CogitoSDL3Font *f = (CogitoSDL3Font *)font;
  if (!f || !f->ttf_font)
    return size > 0 ? size + 2 : 18;
  (void)size;
  return TTF_GetFontHeight(f->ttf_font);
}

static void sdl3_draw_text(CogitoFont *font, const char *text, int x, int y,
                           int size, CogitoColor color) {
  CogitoSDL3Font *f = (CogitoSDL3Font *)font;
  if (!g_current_renderer || !f || !f->ttf_font || !text || !text[0])
    return;
  sdl3_rect_batch_flush();
  (void)size;

  size_t text_len = strnlen(text, COGITO_TEXT_CACHE_MAX_LEN);
  bool cacheable = text_len < COGITO_TEXT_CACHE_MAX_LEN;
  CogitoTextCacheEntry *entry = NULL;

  if (cacheable) {
    // Look up in cache
    entry = cogito_text_cache_lookup(f->ttf_font, text, text_len);
    if (entry->valid && entry->texture) {
      // Cache hit - use existing texture
      SDL_FRect dst = {(float)x, (float)y, (float)entry->width,
                       (float)entry->height};
      SDL_SetTextureColorMod(entry->texture, color.r, color.g, color.b);
      SDL_SetTextureAlphaMod(entry->texture, color.a);
      SDL_RenderTexture(g_current_renderer, entry->texture, NULL, &dst);
      return;
    }
  }

  // Cache miss - render a white glyph texture; tint at draw time to avoid
  // duplicating cache entries per color.
  SDL_Color sdl_color = {255, 255, 255, 255};
  SDL_Surface *surface =
      TTF_RenderText_Blended(f->ttf_font, text, 0, sdl_color);
  if (!surface)
    return;

  SDL_Texture *tex = SDL_CreateTextureFromSurface(g_current_renderer, surface);
  if (!tex) {
    SDL_DestroySurface(surface);
    return;
  }
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);

  SDL_FRect dst = {(float)x, (float)y, (float)surface->w, (float)surface->h};
  SDL_SetTextureColorMod(tex, color.r, color.g, color.b);
  SDL_SetTextureAlphaMod(tex, color.a);
  SDL_RenderTexture(g_current_renderer, tex, NULL, &dst);

  if (cacheable && entry) {
    // Store in cache
    if (entry->texture || entry->size_bytes > 0) {
      cogito_text_cache_drop_entry(entry);
    }
    entry->texture = tex;
    entry->width = surface->w;
    entry->height = surface->h;
    entry->size_bytes =
        (size_t)surface->w * (size_t)surface->h * sizeof(uint32_t);
    entry->valid = true;
    entry->last_used = g_text_cache_frame;
    g_text_cache_bytes += entry->size_bytes;
    cogito_text_cache_trim(COGITO_TEXT_CACHE_MAX_BYTES, entry);
  } else {
    // Don't retain extremely long transient text in the cache.
    SDL_DestroyTexture(tex);
  }

  SDL_DestroySurface(surface);
}

// ============================================================================
// Textures
// ============================================================================

static CogitoTexture *sdl3_texture_create(int w, int h, const uint8_t *data,
                                          int channels) {
  SDL_Renderer *renderer = sdl3_active_renderer();
  if (!renderer || w <= 0 || h <= 0 || !data)
    return NULL;

  CogitoSDL3Texture *tex = calloc(1, sizeof(CogitoSDL3Texture));
  if (!tex)
    return NULL;

  tex->width = w;
  tex->height = h;
  tex->channels = channels;

  tex->gpu_texture = NULL;
  tex->sdl_texture = NULL;

  // For RGBA data, pass directly to SDL — no copy needed.
  // For 1-ch or 3-ch, convert to RGBA first.
  uint8_t *upload_data = NULL;
  const uint8_t *rgba_data = data;

  if (channels == 4) {
    rgba_data = data;
  } else {
    size_t pixel_count = (size_t)w * (size_t)h;
    upload_data = (uint8_t *)malloc(pixel_count * 4);
    if (!upload_data) {
      free(tex);
      return NULL;
    }
    if (channels == 1) {
      for (size_t i = 0; i < pixel_count; i++) {
        uint8_t v = data[i];
        upload_data[i * 4] = v;
        upload_data[i * 4 + 1] = v;
        upload_data[i * 4 + 2] = v;
        upload_data[i * 4 + 3] = 255;
      }
    } else { // channels == 3
      for (size_t i = 0; i < pixel_count; i++) {
        upload_data[i * 4] = data[i * 3];
        upload_data[i * 4 + 1] = data[i * 3 + 1];
        upload_data[i * 4 + 2] = data[i * 3 + 2];
        upload_data[i * 4 + 3] = 255;
      }
    }
    rgba_data = upload_data;
  }

  SDL_Surface *surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32,
                                               (void *)rgba_data, w * 4);
  if (!surface) {
    free(upload_data);
    free(tex);
    return NULL;
  }
  tex->sdl_texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_DestroySurface(surface);
  free(upload_data); // NULL for channels==4 (no-op)
  if (!tex->sdl_texture) {
    free(tex);
    return NULL;
  }
  SDL_SetTextureBlendMode(tex->sdl_texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(tex->sdl_texture, SDL_SCALEMODE_LINEAR);
  return (CogitoTexture *)tex;
}

static void sdl3_texture_destroy(CogitoTexture *tex) {
  CogitoSDL3Texture *t = (CogitoSDL3Texture *)tex;
  if (!t)
    return;
  if (t->sdl_texture) {
    SDL_DestroyTexture(t->sdl_texture);
    t->sdl_texture = NULL;
  }
  if (t->gpu_texture && global_gpu_device) {
    SDL_ReleaseGPUTexture(global_gpu_device, t->gpu_texture);
  }
  free(t);
}

static void sdl3_texture_get_size(CogitoTexture *tex, int *w, int *h) {
  CogitoSDL3Texture *t = (CogitoSDL3Texture *)tex;
  if (!t) {
    if (w)
      *w = 0;
    if (h)
      *h = 0;
    return;
  }
  if (w)
    *w = t->width;
  if (h)
    *h = t->height;
}

static void sdl3_draw_texture(CogitoTexture *tex, CogitoRect src,
                              CogitoRect dst, CogitoColor tint) {
  SDL_Renderer *renderer = sdl3_active_renderer();
  CogitoSDL3Texture *t = (CogitoSDL3Texture *)tex;
  if (!renderer || !t || !t->sdl_texture || dst.w <= 0 || dst.h <= 0)
    return;
  sdl3_rect_batch_flush();
  SDL_SetTextureColorMod(t->sdl_texture, tint.r, tint.g, tint.b);
  SDL_SetTextureAlphaMod(t->sdl_texture, tint.a);
  SDL_FRect d = {(float)dst.x, (float)dst.y, (float)dst.w, (float)dst.h};
  if (src.w > 0 && src.h > 0) {
    SDL_FRect s = {(float)src.x, (float)src.y, (float)src.w, (float)src.h};
    SDL_RenderTexture(renderer, t->sdl_texture, &s, &d);
  } else {
    SDL_RenderTexture(renderer, t->sdl_texture, NULL, &d);
  }
}

static void sdl3_draw_texture_pro(CogitoTexture *tex, CogitoRect src,
                                  CogitoRect dst, CogitoVec2 origin,
                                  float rotation, CogitoColor tint) {
  SDL_Renderer *renderer = sdl3_active_renderer();
  CogitoSDL3Texture *t = (CogitoSDL3Texture *)tex;
  if (!renderer || !t || !t->sdl_texture || dst.w <= 0 || dst.h <= 0)
    return;
  sdl3_rect_batch_flush();
  SDL_SetTextureColorMod(t->sdl_texture, tint.r, tint.g, tint.b);
  SDL_SetTextureAlphaMod(t->sdl_texture, tint.a);
  SDL_FRect d = {(float)dst.x, (float)dst.y, (float)dst.w, (float)dst.h};
  SDL_FPoint c = {origin.x, origin.y};
  if (src.w > 0 && src.h > 0) {
    SDL_FRect s = {(float)src.x, (float)src.y, (float)src.w, (float)src.h};
    SDL_RenderTextureRotated(renderer, t->sdl_texture, &s, &d, (double)rotation,
                             &c, SDL_FLIP_NONE);
  } else {
    SDL_RenderTextureRotated(renderer, t->sdl_texture, NULL, &d,
                             (double)rotation, &c, SDL_FLIP_NONE);
  }
}

// ============================================================================
// Scissor/Blend
// ============================================================================

static void sdl3_begin_scissor(int x, int y, int w, int h) {
  if (!g_current_renderer)
    return;
  sdl3_rect_batch_flush();

  // Save current scissor to stack for nested clipping
  SDL_Rect current_clip;
  bool has_current = SDL_GetRenderClipRect(g_current_renderer, &current_clip);

  // Push current scissor to stack if it exists
  if (has_current && scissor_stack_count < MAX_SCISSOR_STACK) {
    scissor_stack[scissor_stack_count++] = current_clip;
  } else if (!has_current && scissor_stack_count < MAX_SCISSOR_STACK) {
    // No current scissor - push a "null" sentinel (width=0 means no clipping)
    scissor_stack[scissor_stack_count++] = (SDL_Rect){0, 0, 0, 0};
  }

  // Set new scissor
  SDL_Rect r = {x, y, w, h};
  SDL_SetRenderClipRect(g_current_renderer, &r);
}

static void sdl3_end_scissor(void) {
  if (!g_current_renderer)
    return;
  sdl3_rect_batch_flush();

  // Pop previous scissor from stack
  if (scissor_stack_count > 0) {
    SDL_Rect prev = scissor_stack[--scissor_stack_count];
    // If width is 0, it means "no clipping" (sentinel value)
    if (prev.w == 0 && prev.h == 0) {
      SDL_SetRenderClipRect(g_current_renderer, NULL);
    } else {
      SDL_SetRenderClipRect(g_current_renderer, &prev);
    }
  } else {
    // Stack empty - remove all clipping
    SDL_SetRenderClipRect(g_current_renderer, NULL);
  }
}

static void sdl3_set_blend_mode(int mode) {
  if (!g_current_renderer)
    return;
  sdl3_rect_batch_flush();
  SDL_BlendMode bm = SDL_BLENDMODE_BLEND;
  if (mode == 0)
    bm = SDL_BLENDMODE_NONE;
  else if (mode == 2)
    bm = SDL_BLENDMODE_ADD;
  else if (mode == 3)
    bm = SDL_BLENDMODE_MOD;
  SDL_SetRenderDrawBlendMode(g_current_renderer, bm);
}

// ============================================================================
// CSD
// ============================================================================

static SDL_HitTestResult sdl3_csd_hit_test_callback(SDL_Window *sdl_win,
                                                    const SDL_Point *point,
                                                    void *data) {
  (void)sdl_win;
  CogitoSDL3Window *win = (CogitoSDL3Window *)data;
  if (!win || !point)
    return SDL_HITTEST_NORMAL;
  if (win->hit_test_callback) {
    int custom = win->hit_test_callback((CogitoWindow *)win, point->x, point->y,
                                        win->hit_test_userdata);
    return cogito_csd_to_sdl_hit_test((CogitoHitTestResult)custom);
  }
  if (!win->csd_state.enabled)
    return SDL_HITTEST_NORMAL;

  CogitoHitTestResult result = cogito_csd_hit_test(
      &win->csd_state, point->x, point->y, win->width, win->height);
  return cogito_csd_to_sdl_hit_test(result);
}

static void sdl3_window_set_hit_test_callback(
    CogitoWindow *window,
    int (*callback)(CogitoWindow *win, int x, int y, void *user), void *user) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return;

  win->hit_test_callback = callback;
  win->hit_test_userdata = user;

  if (callback || win->csd_state.enabled) {
    SDL_SetWindowHitTest(win->sdl_window, sdl3_csd_hit_test_callback, win);
  } else {
    SDL_SetWindowHitTest(win->sdl_window, NULL, NULL);
  }
}

static void sdl3_window_set_borderless(CogitoWindow *window, bool borderless) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return;
  SDL_SetWindowBordered(win->sdl_window, !borderless);
  win->borderless = borderless;
  win->csd_state.enabled = borderless;
}

static bool sdl3_window_is_borderless(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win)
    return false;
  return win->borderless;
}

// ============================================================================
// Debug
// ============================================================================

static void sdl3_set_debug_overlay(CogitoWindow *window, bool enable) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win)
    return;
  win->csd_state.debug_overlay = enable;
}

// ============================================================================
// Window Registry Implementation
// ============================================================================

void cogito_window_registry_init(CogitoWindowRegistry *registry) {
  if (!registry)
    return;
  memset(registry, 0, sizeof(CogitoWindowRegistry));
  registry->count = 0;
  registry->focused_index = -1;
}

bool cogito_window_registry_add(CogitoWindowRegistry *registry,
                                CogitoWindow *window) {
  if (!registry || !window)
    return false;
  if (registry->count >= COGITO_MAX_WINDOWS)
    return false;

  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  registry->windows[registry->count] = window;
  registry->window_ids[registry->count] = win->window_id;
  registry->count++;
  return true;
}

void cogito_window_registry_remove(CogitoWindowRegistry *registry,
                                   CogitoWindow *window) {
  if (!registry || !window)
    return;

  for (int i = 0; i < registry->count; i++) {
    if (registry->windows[i] == window) {
      // Shift remaining windows down
      for (int j = i; j < registry->count - 1; j++) {
        registry->windows[j] = registry->windows[j + 1];
        registry->window_ids[j] = registry->window_ids[j + 1];
      }
      registry->count--;
      if (registry->focused_index >= registry->count) {
        registry->focused_index = registry->count > 0 ? 0 : -1;
      }
      return;
    }
  }
}

CogitoWindow *cogito_window_registry_get(CogitoWindowRegistry *registry,
                                         uint32_t window_id) {
  if (!registry)
    return NULL;

  for (int i = 0; i < registry->count; i++) {
    if (registry->window_ids[i] == window_id) {
      return registry->windows[i];
    }
  }
  return NULL;
}

void cogito_window_registry_set_focused(CogitoWindowRegistry *registry,
                                        CogitoWindow *window) {
  if (!registry)
    return;

  for (int i = 0; i < registry->count; i++) {
    if (registry->windows[i] == window) {
      registry->focused_index = i;
      return;
    }
  }
}

CogitoWindow *
cogito_window_registry_get_focused(CogitoWindowRegistry *registry) {
  if (!registry || registry->focused_index < 0 ||
      registry->focused_index >= registry->count) {
    return NULL;
  }
  return registry->windows[registry->focused_index];
}

// ============================================================================
// Debug Flags Implementation
// ============================================================================

void cogito_debug_flags_parse(CogitoDebugFlags *flags) {
  if (!flags)
    return;

  memset(flags, 0, sizeof(CogitoDebugFlags));

  const char *env_csd = getenv("COGITO_DEBUG_CSD");
  if (env_csd && env_csd[0] && env_csd[0] != '0') {
    flags->debug_csd = true;
  }

  const char *env_style = getenv("COGITO_DEBUG_STYLE");
  if (env_style && env_style[0] && env_style[0] != '0') {
    flags->debug_style = true;
  }

  const char *env_native = getenv("COGITO_DEBUG_NATIVE");
  if (env_native && env_native[0] && env_native[0] != '0') {
    flags->debug_native = true;
  }

  const char *env_inspector = getenv("COGITO_INSPECTOR");
  if (env_inspector && env_inspector[0] && env_inspector[0] != '0') {
    flags->inspector = true;
  }
}

bool cogito_debug_inspector_toggle_pressed(CogitoBackend *backend) {
  (void)backend;
  bool ctrl = keys_down[SDL_SCANCODE_LCTRL] || keys_down[SDL_SCANCODE_RCTRL];
  bool shift = keys_down[SDL_SCANCODE_LSHIFT] || keys_down[SDL_SCANCODE_RSHIFT];
  return ctrl && shift && keys_pressed[SDL_SCANCODE_I];
}

// ============================================================================
// Backend Instance
// ============================================================================

CogitoBackend *cogito_backend = NULL;

static CogitoBackend sdl3_backend = {
    .init = sdl3_init,
    .shutdown = sdl3_shutdown,
    .window_create = sdl3_window_create,
    .window_destroy = sdl3_window_destroy,
    .window_set_size = sdl3_window_set_size,
    .window_get_size = sdl3_window_get_size,
    .window_set_position = sdl3_window_set_position,
    .window_get_position = sdl3_window_get_position,
    .window_set_title = sdl3_window_set_title,
    .window_show = sdl3_window_show,
    .window_hide = sdl3_window_hide,
    .window_raise = sdl3_window_raise,
    .window_minimize = sdl3_window_minimize,
    .window_maximize = sdl3_window_maximize,
    .window_restore = sdl3_window_restore,
    .window_is_maximized = sdl3_window_is_maximized,
    .window_get_native_handle = sdl3_window_get_native_handle,
    .window_set_icon = sdl3_window_set_icon,
    .window_get_id = sdl3_window_get_id,
    .open_url = sdl3_open_url,
    .set_clipboard_text = sdl3_set_clipboard_text,
    .begin_frame = sdl3_begin_frame,
    .end_frame = sdl3_end_frame,
    .present = sdl3_present,
    .clear = sdl3_clear,
    .poll_events = sdl3_poll_events,
    .wait_event_timeout = sdl3_wait_event_timeout,
    .window_should_close = sdl3_window_should_close,
    .get_mouse_position = sdl3_get_mouse_position,
    .get_mouse_position_in_window = sdl3_get_mouse_position_in_window,
    .is_mouse_button_down = sdl3_is_mouse_button_down,
    .is_mouse_button_pressed = sdl3_is_mouse_button_pressed,
    .is_mouse_button_released = sdl3_is_mouse_button_released,
    .get_mouse_wheel_move = sdl3_get_mouse_wheel_move,
    .is_key_down = sdl3_is_key_down,
    .is_key_pressed = sdl3_is_key_pressed,
    .is_key_released = sdl3_is_key_released,
    .get_char_pressed = sdl3_get_char_pressed,
    .get_time = sdl3_get_time,
    .sleep = sdl3_sleep,
    .draw_rect = sdl3_draw_rect,
    .draw_rect_rounded = sdl3_draw_rect_rounded,
    .draw_rect_lines = sdl3_draw_rect_lines,
    .draw_rect_rounded_lines = sdl3_draw_rect_rounded_lines,
    .draw_line = sdl3_draw_line,
    .draw_circle = sdl3_draw_circle,
    .draw_circle_lines = sdl3_draw_circle_lines,
    .font_load = sdl3_font_load,
    .font_load_face = sdl3_font_load_face,
    .font_unload = sdl3_font_unload,
    .font_get_metrics = sdl3_font_get_metrics,
    .font_get_internal_face = sdl3_font_get_internal_face,
    .font_set_variation = sdl3_font_set_variation,
    .text_measure_width = sdl3_text_measure_width,
    .text_measure_height = sdl3_text_measure_height,
    .draw_text = sdl3_draw_text,
    .texture_create = sdl3_texture_create,
    .texture_destroy = sdl3_texture_destroy,
    .texture_get_size = sdl3_texture_get_size,
    .draw_texture = sdl3_draw_texture,
    .draw_texture_pro = sdl3_draw_texture_pro,
    .begin_scissor = sdl3_begin_scissor,
    .end_scissor = sdl3_end_scissor,
    .set_blend_mode = sdl3_set_blend_mode,
    .set_cursor = sdl3_set_cursor,
    .window_set_hit_test_callback = sdl3_window_set_hit_test_callback,
    .window_set_borderless = sdl3_window_set_borderless,
    .window_is_borderless = sdl3_window_is_borderless,
    .set_debug_overlay = sdl3_set_debug_overlay,
};

bool cogito_backend_sdl3_init(void) {
  cogito_backend = &sdl3_backend;
  return sdl3_init();
}

CogitoBackend *cogito_backend_sdl3_get(void) { return &sdl3_backend; }
