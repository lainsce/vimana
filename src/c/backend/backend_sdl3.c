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
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct CogitoSDL3Window CogitoSDL3Window;

static void *sdl3_window_get_native_handle(CogitoWindow *window);
static SDL_HitTestResult sdl3_csd_hit_test_callback(SDL_Window *sdl_win,
                                                    const SDL_Point *point,
                                                    void *data);
static SDL_Renderer *sdl3_active_renderer(void);
static SDL_Renderer *sdl3_create_renderer_for_window(SDL_Window *window);
static void sdl3_sync_logical_presentation(CogitoSDL3Window *win);
static void sdl3_get_mouse_position_in_window(CogitoWindow *window, int *x,
                                              int *y);
static void sdl3_free_geometry_buffers(void);
static SDL_FColor sdl3_fcolor(CogitoColor c);
static char *sdl3_choose_font_name(CogitoWindow *window,
                                   const char *current_name);

#if defined(__APPLE__)
extern char *cogito_macos_choose_font_name(const char *current_name);
#endif

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

static char *sdl3_choose_font_name(CogitoWindow *window,
                                   const char *current_name) {
  (void)window;
#if defined(__APPLE__)
  return cogito_macos_choose_font_name(current_name);
#else
  (void)current_name;
  return NULL;
#endif
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
  bool pixel;
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

#define SDL3_POINT_BATCH_MAX 1024
static SDL_FPoint g_point_batch[SDL3_POINT_BATCH_MAX];
static int g_point_batch_count = 0;
static SDL_Renderer *g_point_batch_renderer = NULL;
static CogitoColor g_point_batch_color = {0, 0, 0, 0};

static inline void sdl3_point_batch_flush(void) {
  if (!g_point_batch_renderer || g_point_batch_count <= 0)
    return;
  sdl3_set_draw_color_cached(g_point_batch_color.r, g_point_batch_color.g,
                             g_point_batch_color.b, g_point_batch_color.a);
  SDL_RenderPoints(g_point_batch_renderer, g_point_batch, g_point_batch_count);
  g_point_batch_count = 0;
}

static inline void sdl3_point_batch_reset(void) {
  g_point_batch_count = 0;
  g_point_batch_renderer = NULL;
}

static inline void sdl3_point_batch_push(SDL_Renderer *renderer, int x, int y,
                                         CogitoColor color) {
  if (!renderer || color.a == 0)
    return;
  if (g_point_batch_count > 0 &&
      (g_point_batch_renderer != renderer || g_point_batch_color.r != color.r ||
       g_point_batch_color.g != color.g || g_point_batch_color.b != color.b ||
       g_point_batch_color.a != color.a ||
       g_point_batch_count >= SDL3_POINT_BATCH_MAX)) {
    sdl3_point_batch_flush();
  }
  if (g_point_batch_count == 0) {
    g_point_batch_renderer = renderer;
    g_point_batch_color = color;
  }
  g_point_batch[g_point_batch_count++] = (SDL_FPoint){(float)x, (float)y};
}

#define SDL3_LINE_BATCH_MAX 512
static SDL_FPoint g_line_batch[SDL3_LINE_BATCH_MAX * 2];
static int g_line_batch_count = 0;
static SDL_Renderer *g_line_batch_renderer = NULL;
static CogitoColor g_line_batch_color = {0, 0, 0, 0};

static inline void sdl3_line_batch_flush(void) {
  if (!g_line_batch_renderer || g_line_batch_count <= 0)
    return;
  sdl3_set_draw_color_cached(g_line_batch_color.r, g_line_batch_color.g,
                             g_line_batch_color.b, g_line_batch_color.a);
  SDL_RenderLines(g_line_batch_renderer, g_line_batch, g_line_batch_count);
  g_line_batch_count = 0;
}

static inline void sdl3_line_batch_reset(void) {
  g_line_batch_count = 0;
  g_line_batch_renderer = NULL;
}

static inline void sdl3_line_batch_push(SDL_Renderer *renderer, int x1, int y1,
                                       int x2, int y2, CogitoColor color) {
  if (!renderer || color.a == 0)
    return;
  if (g_line_batch_count > 0 &&
      (g_line_batch_renderer != renderer || g_line_batch_color.r != color.r ||
       g_line_batch_color.g != color.g || g_line_batch_color.b != color.b ||
       g_line_batch_color.a != color.a ||
       g_line_batch_count >= SDL3_LINE_BATCH_MAX)) {
    sdl3_line_batch_flush();
  }
  if (g_line_batch_count == 0) {
    g_line_batch_renderer = renderer;
    g_line_batch_color = color;
  }
  g_line_batch[g_line_batch_count++] = (SDL_FPoint){(float)x1, (float)y1};
  g_line_batch[g_line_batch_count++] = (SDL_FPoint){(float)x2, (float)y2};
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
  uint16_t text_len;
  uint16_t raster_size_q64;
  TTF_Font *font;
  SDL_Renderer *renderer; // textures are renderer-specific in SDL3
  uint8_t rtl; // 0=LTR, 1=RTL — part of cache key
  uint8_t font_style; // TTF style flags (italic, strikethrough, etc.)
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

#if (COGITO_TEXT_CACHE_SIZE & (COGITO_TEXT_CACHE_SIZE - 1)) != 0
#error "COGITO_TEXT_CACHE_SIZE must be a power of two"
#endif

static uint32_t cogito_text_cache_hash(TTF_Font *font, const char *text,
                                       size_t text_len,
                                       uint16_t raster_size_q64,
                                       uint8_t rtl,
                                       SDL_Renderer *renderer,
                                       uint8_t font_style) {
  uint32_t h = 5381;
  for (size_t i = 0; i < text_len; i++) {
    h = ((h << 5) + h) ^ (uint8_t)text[i];
  }
  h ^= (uint32_t)(uintptr_t)font;
  h ^= (uint32_t)text_len;
  h ^= ((uint32_t)raster_size_q64 << 1);
  h ^= ((uint32_t)rtl << 17);
  h ^= (uint32_t)(uintptr_t)renderer;
  h ^= ((uint32_t)font_style << 21);
  return h;
}

static bool cogito_text_cache_key_eq(const CogitoTextCacheKey *a,
                                     const CogitoTextCacheKey *b) {
  return a->font == b->font && a->text_len == b->text_len &&
         a->rtl == b->rtl && a->renderer == b->renderer &&
         a->font_style == b->font_style &&
         memcmp(a->text, b->text, a->text_len) == 0;
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
cogito_text_cache_lookup(TTF_Font *font, const char *text, size_t text_len,
                         uint16_t raster_size_q64, uint8_t rtl) {
  CogitoTextCacheKey key = {0};
  if (text_len >= (size_t)COGITO_TEXT_CACHE_MAX_LEN) {
    text_len = (size_t)COGITO_TEXT_CACHE_MAX_LEN - 1;
  }
  if (text_len > 0) {
    memcpy(key.text, text, text_len);
  }
  key.text[text_len] = '\0';
  key.text_len = (uint16_t)text_len;
  key.raster_size_q64 = raster_size_q64;
  key.font = font;
  key.renderer = g_current_renderer;
  key.rtl = rtl;
  key.font_style = (uint8_t)TTF_GetFontStyle(font);

  uint32_t hash =
      cogito_text_cache_hash(font, key.text, text_len, raster_size_q64, rtl, g_current_renderer, key.font_style);
  int idx = (int)(hash & (COGITO_TEXT_CACHE_SIZE - 1));

  // Linear probing
  for (int i = 0, probe = idx; i < COGITO_TEXT_CACHE_SIZE;
       i++, probe = (probe + 1) & (COGITO_TEXT_CACHE_SIZE - 1)) {
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

static void sdl3_sync_logical_presentation(CogitoSDL3Window *win) {
  if (!win || !win->sdl_window || !win->renderer)
    return;

  // Disable the intermediate render-target that SDL logical presentation
  // creates.  The extra texture copy adds a full frame of latency during
  // live window resize on macOS, because the compositor stretches the stale
  // back-buffer before the app can present a fresh frame — visible as
  // jitter / ghosting on every element.
  //
  // Instead we apply the display-pixel-density via SDL_SetRenderScale so
  // that all drawing coordinates stay in logical (point) space while
  // rendering goes directly to the swap-chain.
  int current_w = 0, current_h = 0;
  SDL_RendererLogicalPresentation current_mode =
      SDL_LOGICAL_PRESENTATION_DISABLED;
  SDL_GetRenderLogicalPresentation(win->renderer, &current_w, &current_h,
                                   &current_mode);
  if (current_mode != SDL_LOGICAL_PRESENTATION_DISABLED) {
    SDL_SetRenderLogicalPresentation(win->renderer, 0, 0,
                                     SDL_LOGICAL_PRESENTATION_DISABLED);
  }

  float density = SDL_GetWindowPixelDensity(win->sdl_window);
  if (density <= 0.0f)
    density = 1.0f;
  SDL_SetRenderScale(win->renderer, density, density);
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
  sdl3_sync_logical_presentation(win);
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

static void sdl3_window_set_min_size(CogitoWindow *window, int min_w,
                                     int min_h) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->sdl_window)
    return;
  if (min_w < 0)
    min_w = 0;
  if (min_h < 0)
    min_h = 0;
  SDL_SetWindowMinimumSize(win->sdl_window, min_w, min_h);
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

static char *sdl3_get_clipboard_text(void) {
  char *text = SDL_GetClipboardText();
  if (!text || !text[0]) return NULL;
  char *copy = strdup(text);
  SDL_free(text);
  return copy;
}

static bool sdl3_clipboard_has(const char *mime_type) {
  if (!mime_type) return false;
  return SDL_HasClipboardData(mime_type);
}

static void *sdl3_clipboard_get_data(const char *mime_type, size_t *size) {
  if (!mime_type || !size) return NULL;
  void *data = SDL_GetClipboardData(mime_type, size);
  if (!data || *size == 0) return NULL;
  void *copy = malloc(*size);
  if (!copy) { SDL_free(data); *size = 0; return NULL; }
  memcpy(copy, data, *size);
  SDL_free(data);
  return copy;
}

typedef struct {
  void *data;
  char *mime_type;
  size_t size;
} CogitoClipboardPayload;

static const void *sdl3_clipboard_data_callback(
    void *userdata, const char *mime_type, size_t *size) {
  CogitoClipboardPayload *p = (CogitoClipboardPayload *)userdata;
  if (!p) { *size = 0; return NULL; }
  *size = p->size;
  const char *mime_types[] = { mime_type };
  if (!mime_types[0]) { *size = 0; return NULL; }
  p->mime_type = strdup(mime_type);
  if (!p->mime_type) { *size = 0; return NULL; }
  return p->data;
}

static void sdl3_clipboard_cleanup_callback(void *userdata) {
  CogitoClipboardPayload *p = (CogitoClipboardPayload *)userdata;
  if (p) { free(p->data); free(p->mime_type); free(p); }
}

static bool sdl3_clipboard_set_data(const char *mime_type,
                                     const void *data, size_t size) {
  if (!mime_type || !data || size == 0) return false;
  CogitoClipboardPayload *p = (CogitoClipboardPayload *)malloc(sizeof(*p));
  if (!p) return false;
  p->data = malloc(size);
  if (!p->data) { free(p); return false; }
  memcpy(p->data, data, size);
  p->size = size;
  const char *mime_types[] = { mime_type };
  return SDL_SetClipboardData(sdl3_clipboard_data_callback,
                              sdl3_clipboard_cleanup_callback, p,
                              mime_types, 1);
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
  sdl3_sync_logical_presentation(win);

  g_current_renderer = win->renderer;
  g_current_window = win;
  g_draw_color_valid = false;
  g_draw_color_renderer = win->renderer;
  sdl3_rect_batch_reset();
  sdl3_point_batch_reset();
  sdl3_line_batch_reset();
  g_render_state.window_width = w;
  g_render_state.window_height = h;
  SDL_SetRenderClipRect(g_current_renderer, NULL);
  scissor_stack_count = 0; // Reset scissor stack for new frame
}

static void sdl3_end_frame(CogitoWindow *window) { (void)window; }

static void sdl3_set_vsync(CogitoWindow *window, int vsync) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->renderer)
    return;
  SDL_SetRenderVSync(win->renderer, vsync);
}

static void sdl3_present(CogitoWindow *window) {
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  if (!win || !win->renderer)
    return;
  sdl3_rect_batch_flush();
  sdl3_point_batch_flush();
  sdl3_line_batch_flush();
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
  sdl3_point_batch_reset();
  sdl3_line_batch_reset();
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

// ---- File drop queue (per-window) ----
#define COGITO_DROP_MAX 64
static struct {
  uint32_t window_id;
  char *paths[COGITO_DROP_MAX];
  int count;
} cogito_drop_queue = {0};

void cogito_drop_queue_reset(void) {
  for (int i = 0; i < cogito_drop_queue.count; i++)
    free(cogito_drop_queue.paths[i]);
  cogito_drop_queue.count = 0;
  cogito_drop_queue.window_id = 0;
}

int cogito_drop_queue_count(void) { return cogito_drop_queue.count; }
const char *cogito_drop_queue_path(int i) {
  if (i < 0 || i >= cogito_drop_queue.count) return NULL;
  return cogito_drop_queue.paths[i];
}
uint32_t cogito_drop_queue_window_id(void) { return cogito_drop_queue.window_id; }

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

    case SDL_EVENT_DROP_FILE: {
      const char *file = event.drop.data;
      if (file && cogito_drop_queue.count < COGITO_DROP_MAX) {
        cogito_drop_queue.window_id = event.drop.windowID;
        cogito_drop_queue.paths[cogito_drop_queue.count++] = strdup(file);
      }
      break;
    }

    case SDL_EVENT_DROP_TEXT: {
      const char *text = event.drop.data;
      if (text && cogito_drop_queue.count < COGITO_DROP_MAX) {
        cogito_drop_queue.window_id = event.drop.windowID;
        cogito_drop_queue.paths[cogito_drop_queue.count++] = strdup(text);
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
  // On macOS, SDL_WaitEventTimeout can intermittently stall timer-driven
  // updates in our loop. Poll in small slices so timer wakeups remain reliable.
  uint64_t start = SDL_GetTicks();
  for (;;) {
    if (SDL_PollEvent(&event)) {
      SDL_PushEvent(&event);
      return;
    }
    uint64_t elapsed = SDL_GetTicks() - start;
    if (elapsed >= (uint64_t)timeout_ms) {
      return;
    }
    uint32_t remain = (uint32_t)((uint64_t)timeout_ms - elapsed);
    uint32_t slice = remain > 4 ? 4 : remain;
    if (slice == 0) {
      return;
    }
    SDL_Delay(slice);
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

static void sdl3_draw_points(int count, const int *x, const int *y, CogitoColor color) {
  if (!g_current_renderer || count <= 0 || !x || !y || color.a == 0)
    return;
  for (int i = 0; i < count; i++) {
    sdl3_point_batch_push(g_current_renderer, x[i], y[i], color);
  }
}

static void sdl3_draw_rect_linear_gradient(int x, int y, int w, int h,
                                           CogitoColor start,
                                           CogitoColor end,
                                           float angle_deg) {
  if (!g_current_renderer || w <= 0 || h <= 0)
    return;

  sdl3_rect_batch_flush();
  sdl3_point_batch_flush();
  sdl3_line_batch_flush();

  float rad = angle_deg * (float)M_PI / 180.0f;
  float dx = cosf(rad);
  float dy = sinf(rad);
  if (fabsf(dx) < 1e-6f && fabsf(dy) < 1e-6f) {
    dx = 1.0f;
    dy = 0.0f;
  }

  float x0 = (float)x;
  float y0 = (float)y;
  float x1 = (float)(x + w);
  float y1 = (float)(y + h);
  float cx = x0 + ((float)w * 0.5f);
  float cy = y0 + ((float)h * 0.5f);

  float proj_tl = (x0 - cx) * dx + (y0 - cy) * dy;
  float proj_tr = (x1 - cx) * dx + (y0 - cy) * dy;
  float proj_br = (x1 - cx) * dx + (y1 - cy) * dy;
  float proj_bl = (x0 - cx) * dx + (y1 - cy) * dy;

  float proj_min = proj_tl;
  float proj_max = proj_tl;
  if (proj_tr < proj_min) proj_min = proj_tr;
  if (proj_br < proj_min) proj_min = proj_br;
  if (proj_bl < proj_min) proj_min = proj_bl;
  if (proj_tr > proj_max) proj_max = proj_tr;
  if (proj_br > proj_max) proj_max = proj_br;
  if (proj_bl > proj_max) proj_max = proj_bl;

  float denom = proj_max - proj_min;
  if (fabsf(denom) < 1e-6f) denom = 1.0f;

  float t_tl = (proj_tl - proj_min) / denom;
  float t_tr = (proj_tr - proj_min) / denom;
  float t_br = (proj_br - proj_min) / denom;
  float t_bl = (proj_bl - proj_min) / denom;

  SDL_FColor c_tl = sdl3_fcolor(cogito_color_lerp(start, end, t_tl));
  SDL_FColor c_tr = sdl3_fcolor(cogito_color_lerp(start, end, t_tr));
  SDL_FColor c_br = sdl3_fcolor(cogito_color_lerp(start, end, t_br));
  SDL_FColor c_bl = sdl3_fcolor(cogito_color_lerp(start, end, t_bl));

  SDL_Vertex verts[4] = {
    {.position = {x0, y0}, .color = c_tl, .tex_coord = {0.0f, 0.0f}},
    {.position = {x1, y0}, .color = c_tr, .tex_coord = {0.0f, 0.0f}},
    {.position = {x1, y1}, .color = c_br, .tex_coord = {0.0f, 0.0f}},
    {.position = {x0, y1}, .color = c_bl, .tex_coord = {0.0f, 0.0f}},
  };
  int indices[6] = {0, 1, 2, 0, 2, 3};

  SDL_RenderGeometry(g_current_renderer, NULL, verts, 4, indices, 6);
}

static void sdl3_draw_point_alpha(int x, int y, CogitoColor color,
                                  float coverage) {
  if (!g_current_renderer || color.a == 0)
    return;
  if (coverage <= 0.0f)
    return;
  if (coverage > 1.0f)
    coverage = 1.0f;
  uint8_t a = (uint8_t)lroundf((float)color.a * coverage);
  if (a == 0)
    return;
  CogitoColor adjusted = color;
  adjusted.a = a;
  sdl3_point_batch_push(g_current_renderer, x, y, adjusted);
}

static void sdl3_draw_hspan_aa(int y, float left, float right,
                               CogitoColor color) {
  if (!g_current_renderer || color.a == 0)
    return;
  if (right < left)
    return;
  int full_l = (int)ceilf(left);
  int full_r = (int)floorf(right);

  if (full_l > full_r) {
    return;
  }

  sdl3_line_batch_push(g_current_renderer, full_l, y, full_r, y, color);
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

static void sdl3_draw_rect_radial_gradient(int x, int y, int w, int h,
                                           CogitoColor inner,
                                           CogitoColor outer,
                                           float center_x, float center_y,
                                           float radius) {
  if (!g_current_renderer || w <= 0 || h <= 0)
    return;

  sdl3_rect_batch_flush();
  sdl3_draw_rect(x, y, w, h, outer);
  sdl3_rect_batch_flush();

  int icx = (int)center_x;
  int icy = (int)center_y;
  if (radius <= 0.0f) {
    int dx0 = abs(icx - x);
    int dx1 = abs(icx - (x + w));
    int dy0 = abs(icy - y);
    int dy1 = abs(icy - (y + h));
    int max_dx = dx0 > dx1 ? dx0 : dx1;
    int max_dy = dy0 > dy1 ? dy0 : dy1;
    int val = max_dx * max_dx + max_dy * max_dy;
    int s = 1;
    if (val > 0) { s = val; int t = (s + val / s) / 2; while (t < s) { s = t; t = (s + val / s) / 2; } }
    radius = (float)s;
  }
  float cx = (float)icx;
  float cy = (float)icy;
  if (radius < 1.0f)
    radius = 1.0f;

  int segments = (int)(radius * 0.35f);
  if (segments < 24)
    segments = 24;
  if (segments > 96)
    segments = 96;

  int rings = (int)(radius * 0.08f);
  if (rings < 10)
    rings = 10;
  if (rings > 48)
    rings = 48;

  int vert_count = 1 + (rings * segments);
  int index_count = (segments * 3) + ((rings - 1) * segments * 6);

  if (!cogito_ensure_cap((void **)&g_rounded_rect_verts,
                         &g_rounded_rect_verts_cap, vert_count,
                         sizeof(SDL_Vertex)) ||
      !cogito_ensure_cap((void **)&g_rounded_rect_indices,
                         &g_rounded_rect_indices_cap, index_count,
                         sizeof(int))) {
    return;
  }

  g_rounded_rect_verts[0] = (SDL_Vertex){
      .position = {cx, cy},
      .color = sdl3_fcolor(inner),
      .tex_coord = {0.0f, 0.0f}};

  int vi = 1;
  for (int r = 1; r <= rings; r++) {
    float t = (float)r / (float)rings;
    float rr = radius * t;
    SDL_FColor ring_color = sdl3_fcolor(cogito_color_lerp(inner, outer, t));
    for (int i = 0; i < segments; i++) {
      float a = ((float)i / (float)segments) * (float)(2.0 * M_PI);
      float px = cx + cosf(a) * rr;
      float py = cy + sinf(a) * rr;
      g_rounded_rect_verts[vi++] = (SDL_Vertex){
          .position = {px, py}, .color = ring_color, .tex_coord = {0.0f, 0.0f}};
    }
  }

  int ii = 0;
  int ring0 = 1;
  for (int i = 0; i < segments; i++) {
    int a = ring0 + i;
    int b = ring0 + ((i + 1) % segments);
    g_rounded_rect_indices[ii++] = 0;
    g_rounded_rect_indices[ii++] = a;
    g_rounded_rect_indices[ii++] = b;
  }

  for (int r = 1; r < rings; r++) {
    int inner_start = 1 + ((r - 1) * segments);
    int outer_start = 1 + (r * segments);
    for (int i = 0; i < segments; i++) {
      int i0 = inner_start + i;
      int i1 = inner_start + ((i + 1) % segments);
      int o0 = outer_start + i;
      int o1 = outer_start + ((i + 1) % segments);
      g_rounded_rect_indices[ii++] = i0;
      g_rounded_rect_indices[ii++] = i1;
      g_rounded_rect_indices[ii++] = o1;
      g_rounded_rect_indices[ii++] = i0;
      g_rounded_rect_indices[ii++] = o1;
      g_rounded_rect_indices[ii++] = o0;
    }
  }

  SDL_RenderGeometry(g_current_renderer, NULL, g_rounded_rect_verts,
                     vert_count, g_rounded_rect_indices, index_count);
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

  int adx = abs(x2 - x1);
  int ady = abs(y2 - y1);
  if (adx == 0 && ady == 0) {
    SDL_RenderPoint(g_current_renderer, (float)x1, (float)y1);
    return;
  }
  int half = thickness / 2;
  if (adx >= ady) {
    for (int i = 0; i < thickness; i++) {
      int off = i - half;
      sdl3_line_batch_push(g_current_renderer, x1, y1 + off, x2, y2 + off, color);
    }
  } else {
    for (int i = 0; i < thickness; i++) {
      int off = i - half;
      sdl3_line_batch_push(g_current_renderer, x1 + off, y1, x2 + off, y2, color);
    }
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

  sdl3_rect_batch_push(g_current_renderer, x, y, w, thickness, color);
  sdl3_rect_batch_push(g_current_renderer, x, y + h - thickness, w, thickness, color);

  int inner_h = h - thickness * 2;
  if (inner_h > 0) {
    sdl3_rect_batch_push(g_current_renderer, x, y + thickness, thickness, inner_h, color);
    sdl3_rect_batch_push(g_current_renderer, x + w - thickness, y + thickness, thickness, inner_h, color);
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

static CogitoFont *sdl3_font_load_pixel(const char *path, int size) {
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
  font->pixel = true;
  TTF_SetFontKerning(ttf, false);
  TTF_SetFontHinting(ttf, TTF_HINTING_NONE);
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

#define COGITO_FONT_VARIATION_STACK_AXES 16
  FT_Fixed stack_coords[COGITO_FONT_VARIATION_STACK_AXES];
  FT_Fixed *coords = stack_coords;
  bool coords_heap = false;
  if (mm->num_axis > COGITO_FONT_VARIATION_STACK_AXES) {
    coords = (FT_Fixed *)malloc(sizeof(FT_Fixed) * mm->num_axis);
    if (!coords) {
      FT_Done_MM_Var(lib, mm);
      return false;
    }
    coords_heap = true;
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
  if (coords_heap)
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

  // Set font direction for correct HarfBuzz measurement
  TTF_SetFontDirection(f->ttf_font,
                       cogito_is_rtl() ? TTF_DIRECTION_RTL : TTF_DIRECTION_LTR);

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

static float sdl3_text_render_density(void) {
  if (!g_current_window || !g_current_window->sdl_window)
    return 1.0f;

  float density = SDL_GetWindowPixelDensity(g_current_window->sdl_window);
  if (density <= 0.0f)
    density = SDL_GetWindowDisplayScale(g_current_window->sdl_window);
  if (density <= 0.0f)
    density = 1.0f;

  if (density < 1.0f)
    density = 1.0f;
  if (density > 4.0f)
    density = 4.0f;
  return density;
}

static void sdl3_draw_text(CogitoFont *font, const char *text, int x, int y,
                           int size, CogitoColor color) {
  CogitoSDL3Font *f = (CogitoSDL3Font *)font;
  if (!g_current_renderer || !f || !f->ttf_font || !text || !text[0])
    return;
  sdl3_rect_batch_flush();

  // Use global direction for shaping
  uint8_t rtl = cogito_is_rtl() ? 1 : 0;
  TTF_SetFontDirection(f->ttf_font,
                       rtl ? TTF_DIRECTION_RTL : TTF_DIRECTION_LTR);

  float logical_pt = (float)(size > 0 ? size : f->size);
  if (logical_pt <= 0.0f)
    logical_pt = (float)(f->size > 0 ? f->size : 12);
  float density = sdl3_text_render_density();
  float raster_pt = logical_pt * density;
  uint16_t raster_size_q64 = (uint16_t)SDL_clamp((int)lroundf(raster_pt * 64.0f),
                                                 1, 65535);
  float draw_scale = raster_pt / logical_pt;
  if (draw_scale <= 0.0f)
    draw_scale = 1.0f;

  size_t text_len = strnlen(text, COGITO_TEXT_CACHE_MAX_LEN);
  bool cacheable = text_len < COGITO_TEXT_CACHE_MAX_LEN;
  CogitoTextCacheEntry *entry = NULL;

    if (cacheable) {
      // Look up in cache (direction-aware)
      entry =
          cogito_text_cache_lookup(f->ttf_font, text, text_len, raster_size_q64, rtl);
      if (entry->valid && entry->texture) {
        // Cache hit - use existing texture
        SDL_FRect dst = {(float)x,
                         (float)y,
                         (float)entry->width / draw_scale,
                         (float)entry->height / draw_scale};
        SDL_SetTextureScaleMode(entry->texture,
                                f->pixel ? SDL_SCALEMODE_NEAREST
                                         : SDL_SCALEMODE_LINEAR);
        SDL_SetTextureColorMod(entry->texture, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(entry->texture, color.a);
        SDL_RenderTexture(g_current_renderer, entry->texture, NULL, &dst);
        return;
      }
    }

  float prev_pt = TTF_GetFontSize(f->ttf_font);
  bool font_size_changed = fabsf(prev_pt - raster_pt) > 0.01f;
  if (font_size_changed) {
    if (!TTF_SetFontSize(f->ttf_font, raster_pt)) {
      font_size_changed = false;
      raster_pt = prev_pt;
      draw_scale = raster_pt / logical_pt;
      if (draw_scale <= 0.0f)
        draw_scale = 1.0f;
    }
  }

  // Cache miss - render a white glyph texture; tint at draw time to avoid
  // duplicating cache entries per color.
  SDL_Color sdl_color = {255, 255, 255, 255};
  SDL_Surface *surface =
      f->pixel
          ? TTF_RenderText_Solid(f->ttf_font, text, 0, sdl_color)
          : TTF_RenderText_Blended(f->ttf_font, text, 0, sdl_color);
  if (!surface) {
    if (font_size_changed) {
      TTF_SetFontSize(f->ttf_font, prev_pt);
    }
    return;
  }

  SDL_Texture *tex = SDL_CreateTextureFromSurface(g_current_renderer, surface);
  if (!tex) {
    SDL_DestroySurface(surface);
    if (font_size_changed) {
      TTF_SetFontSize(f->ttf_font, prev_pt);
    }
    return;
  }
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(tex, f->pixel ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);

  SDL_FRect dst = {(float)x, (float)y, (float)surface->w / draw_scale,
                   (float)surface->h / draw_scale};
  SDL_SetTextureColorMod(tex, color.r, color.g, color.b);
  SDL_SetTextureAlphaMod(tex, color.a);
  SDL_RenderTexture(g_current_renderer, tex, NULL, &dst);

  if (font_size_changed) {
    TTF_SetFontSize(f->ttf_font, prev_pt);
  }

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

// Set font direction for HarfBuzz shaping (used by bidi pipeline per-run)
static void sdl3_font_set_direction(CogitoFont *font, bool rtl) {
  CogitoSDL3Font *f = (CogitoSDL3Font *)font;
  if (!f || !f->ttf_font) return;
  TTF_SetFontDirection(f->ttf_font,
                       rtl ? TTF_DIRECTION_RTL : TTF_DIRECTION_LTR);
}

// Set TTF style flags (italic, strikethrough, etc.)
static void sdl3_font_set_style(CogitoFont *font, int style) {
  CogitoSDL3Font *f = (CogitoSDL3Font *)font;
  if (!f || !f->ttf_font) return;
  TTF_SetFontStyle(f->ttf_font, (TTF_FontStyleFlags)style);
}

// Draw text with explicit direction (bypasses global cogito_is_rtl())
static void sdl3_draw_text_dir(CogitoFont *font, const char *text, int x, int y,
                                int size, CogitoColor color, bool rtl) {
  CogitoSDL3Font *f = (CogitoSDL3Font *)font;
  if (!f || !f->ttf_font) return;
  TTF_SetFontDirection(f->ttf_font,
                       rtl ? TTF_DIRECTION_RTL : TTF_DIRECTION_LTR);
  sdl3_draw_text(font, text, x, y, size, color);
}

// Measure text width with explicit direction
static int sdl3_text_measure_width_dir(CogitoFont *font, const char *text,
                                        int size, bool rtl) {
  CogitoSDL3Font *f = (CogitoSDL3Font *)font;
  if (!f || !f->ttf_font || !text || !text[0]) return 0;
  TTF_SetFontDirection(f->ttf_font,
                       rtl ? TTF_DIRECTION_RTL : TTF_DIRECTION_LTR);
  int w, h;
  if (!TTF_GetStringSize(f->ttf_font, text, 0, &w, &h)) return 0;
  (void)h; (void)size;
  return w;
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

static void sdl3_texture_set_nearest(CogitoTexture *tex) {
  CogitoSDL3Texture *t = (CogitoSDL3Texture *)tex;
  if (t && t->sdl_texture) {
    SDL_SetTextureScaleMode(t->sdl_texture, SDL_SCALEMODE_NEAREST);
  }
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
// Render Targets
// ============================================================================

static CogitoTexture *sdl3_render_target_create(int w, int h) {
  SDL_Renderer *renderer = sdl3_active_renderer();
  if (!renderer || w <= 0 || h <= 0)
    return NULL;
  sdl3_rect_batch_flush();

  CogitoSDL3Texture *tex = calloc(1, sizeof(CogitoSDL3Texture));
  if (!tex)
    return NULL;
  tex->width = w;
  tex->height = h;
  tex->channels = 4;
  tex->gpu_texture = NULL;
  tex->sdl_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                       SDL_TEXTUREACCESS_TARGET, w, h);
  if (!tex->sdl_texture) {
    free(tex);
    return NULL;
  }
  SDL_SetTextureBlendMode(tex->sdl_texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(tex->sdl_texture, SDL_SCALEMODE_LINEAR);
  return (CogitoTexture *)tex;
}

static void sdl3_set_render_target(CogitoTexture *target) {
  SDL_Renderer *renderer = sdl3_active_renderer();
  if (!renderer) return;
  sdl3_rect_batch_flush();
  g_draw_color_valid = false;
  if (target) {
    CogitoSDL3Texture *t = (CogitoSDL3Texture *)target;
    SDL_SetRenderTarget(renderer, t->sdl_texture);
    // Offscreen targets are sized in pixels — draw 1:1 without DPI scale.
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
  } else {
    SDL_SetRenderTarget(renderer, NULL);
    // Restore DPI scale for the main window output.
    float density = 1.0f;
    if (g_current_window && g_current_window->sdl_window)
      density = SDL_GetWindowPixelDensity(g_current_window->sdl_window);
    if (density <= 0.0f) density = 1.0f;
    SDL_SetRenderScale(renderer, density, density);
  }
}

// ---------------------------------------------------------------------------
// Separable Gaussian blur via CPU convolution
// ---------------------------------------------------------------------------
static CogitoTexture *sdl3_texture_gaussian_blur(CogitoTexture *src,
                                                  float sigma) {
  CogitoSDL3Texture *stex = (CogitoSDL3Texture *)src;
  if (!stex || !stex->sdl_texture || sigma <= 0.0f)
    return NULL;

  int w = stex->width;
  int h = stex->height;
  if (w <= 0 || h <= 0)
    return NULL;

  SDL_Renderer *renderer = sdl3_active_renderer();
  if (!renderer)
    return NULL;

  sdl3_rect_batch_flush();

  // Save current render target
  SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);

  // Create temp target and blit source into it for pixel readback
  SDL_Texture *rt = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
  if (!rt) return NULL;

  SDL_SetRenderTarget(renderer, rt);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
  SDL_RenderClear(renderer);
  // Copy pixels without alpha blending to get raw data
  SDL_BlendMode prev_blend;
  SDL_GetTextureBlendMode(stex->sdl_texture, &prev_blend);
  SDL_SetTextureBlendMode(stex->sdl_texture, SDL_BLENDMODE_NONE);
  SDL_RenderTexture(renderer, stex->sdl_texture, NULL, NULL);
  SDL_SetTextureBlendMode(stex->sdl_texture, prev_blend);

  // Read pixels from the render target
  SDL_Surface *readback = SDL_RenderReadPixels(renderer, NULL);
  SDL_SetRenderTarget(renderer, prev_target);
  SDL_DestroyTexture(rt);
  if (!readback) return NULL;

  // Convert to RGBA32 for portable byte-order (R=0, G=1, B=2, A=3)
  SDL_Surface *rgba = SDL_ConvertSurface(readback, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(readback);
  if (!rgba) return NULL;

  uint8_t *pixels = (uint8_t *)rgba->pixels;
  int pitch = rgba->pitch;

  // ---- Generate 1-D Gaussian kernel ----
  int radius = (int)ceilf(3.0f * sigma);
  if (radius < 1) radius = 1;
  int ksize = 2 * radius + 1;
  float *kernel = (float *)malloc((size_t)ksize * sizeof(float));
  if (!kernel) { SDL_DestroySurface(rgba); return NULL; }

  float ksum = 0.0f;
  for (int i = 0; i < ksize; i++) {
    float x = (float)(i - radius);
    kernel[i] = expf(-0.5f * x * x / (sigma * sigma));
    ksum += kernel[i];
  }
  for (int i = 0; i < ksize; i++) kernel[i] /= ksum;

  // ---- Allocate temp scanline buffer ----
  uint8_t *temp = (uint8_t *)calloc((size_t)w * (size_t)h * 4, 1);
  if (!temp) { free(kernel); SDL_DestroySurface(rgba); return NULL; }

  // ---- Horizontal pass: pixels → temp ----
  for (int y = 0; y < h; y++) {
    const uint8_t *row_in = pixels + y * pitch;
    uint8_t *row_out = temp + y * w * 4;
    for (int px = 0; px < w; px++) {
      float r = 0, g = 0, b = 0, a = 0;
      for (int k = -radius; k <= radius; k++) {
        int sx = px + k;
        if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
        float wt = kernel[k + radius];
        const uint8_t *p = row_in + sx * 4;
        r += p[0] * wt;
        g += p[1] * wt;
        b += p[2] * wt;
        a += p[3] * wt;
      }
      uint8_t *o = row_out + px * 4;
      o[0] = (uint8_t)(r > 255.0f ? 255 : (r < 0.0f ? 0 : (r + 0.5f)));
      o[1] = (uint8_t)(g > 255.0f ? 255 : (g < 0.0f ? 0 : (g + 0.5f)));
      o[2] = (uint8_t)(b > 255.0f ? 255 : (b < 0.0f ? 0 : (b + 0.5f)));
      o[3] = (uint8_t)(a > 255.0f ? 255 : (a < 0.0f ? 0 : (a + 0.5f)));
    }
  }

  // ---- Vertical pass: temp → pixels ----
  for (int px = 0; px < w; px++) {
    for (int y = 0; y < h; y++) {
      float r = 0, g = 0, b = 0, a = 0;
      for (int k = -radius; k <= radius; k++) {
        int sy = y + k;
        if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
        float wt = kernel[k + radius];
        const uint8_t *p = temp + sy * w * 4 + px * 4;
        r += p[0] * wt;
        g += p[1] * wt;
        b += p[2] * wt;
        a += p[3] * wt;
      }
      uint8_t *o = pixels + y * pitch + px * 4;
      o[0] = (uint8_t)(r > 255.0f ? 255 : (r < 0.0f ? 0 : (r + 0.5f)));
      o[1] = (uint8_t)(g > 255.0f ? 255 : (g < 0.0f ? 0 : (g + 0.5f)));
      o[2] = (uint8_t)(b > 255.0f ? 255 : (b < 0.0f ? 0 : (b + 0.5f)));
      o[3] = (uint8_t)(a > 255.0f ? 255 : (a < 0.0f ? 0 : (a + 0.5f)));
    }
  }

  free(temp);
  free(kernel);

  // Create output texture from blurred RGBA pixels
  CogitoTexture *result = sdl3_texture_create(w, h, pixels, 4);
  SDL_DestroySurface(rgba);
  return result;
}

static void sdl3_draw_texture_polygon(CogitoTexture *tex,
                                       const float *screen_x,
                                       const float *screen_y,
                                       const float *uv_x,
                                       const float *uv_y,
                                       int point_count) {
  SDL_Renderer *renderer = sdl3_active_renderer();
  CogitoSDL3Texture *t = (CogitoSDL3Texture *)tex;
  if (!renderer || !t || !t->sdl_texture || point_count < 3)
    return;
  sdl3_rect_batch_flush();

  // Triangulate as a fan from area-weighted centroid to avoid directional bias.
  float area2 = 0.0f;
  float uv_area2 = 0.0f;
  float cx_acc = 0.0f;
  float cy_acc = 0.0f;
  float cu_acc = 0.0f;
  float cv_acc = 0.0f;
  float cx = 0.0f;
  float cy = 0.0f;
  float cu = 0.0f;
  float cv = 0.0f;
  for (int i = 0; i < point_count; i++) {
    int j = (i + 1) % point_count;
    float cross = screen_x[i] * screen_y[j] - screen_x[j] * screen_y[i];
    area2 += cross;
    cx_acc += (screen_x[i] + screen_x[j]) * cross;
    cy_acc += (screen_y[i] + screen_y[j]) * cross;

    float uv_cross = uv_x[i] * uv_y[j] - uv_x[j] * uv_y[i];
    uv_area2 += uv_cross;
    cu_acc += (uv_x[i] + uv_x[j]) * uv_cross;
    cv_acc += (uv_y[i] + uv_y[j]) * uv_cross;
  }

  if (fabsf(area2) > 1e-6f && fabsf(uv_area2) > 1e-6f) {
    cx = cx_acc / (3.0f * area2);
    cy = cy_acc / (3.0f * area2);
    cu = cu_acc / (3.0f * uv_area2);
    cv = cv_acc / (3.0f * uv_area2);
  } else {
    for (int i = 0; i < point_count; i++) {
      cx += screen_x[i];
      cy += screen_y[i];
      cu += uv_x[i];
      cv += uv_y[i];
    }
    cx /= (float)point_count;
    cy /= (float)point_count;
    cu /= (float)point_count;
    cv /= (float)point_count;
  }

  int vert_count = 1 + point_count;
  int index_count = 3 * point_count;

  if (!cogito_ensure_cap((void **)&g_rounded_rect_verts,
                         &g_rounded_rect_verts_cap, vert_count,
                         sizeof(SDL_Vertex)) ||
      !cogito_ensure_cap((void **)&g_rounded_rect_indices,
                         &g_rounded_rect_indices_cap, index_count,
                         sizeof(int))) {
    return;
  }

  SDL_FColor white = {1.0f, 1.0f, 1.0f, 1.0f};
  g_rounded_rect_verts[0] = (SDL_Vertex){
      .position = {cx, cy}, .color = white, .tex_coord = {cu, cv}};
  for (int i = 0; i < point_count; i++) {
    g_rounded_rect_verts[1 + i] = (SDL_Vertex){
        .position = {screen_x[i], screen_y[i]},
        .color = white,
        .tex_coord = {uv_x[i], uv_y[i]}};
  }

  int ii = 0;
  for (int i = 0; i < point_count; i++) {
    g_rounded_rect_indices[ii++] = 0;
    g_rounded_rect_indices[ii++] = 1 + i;
    g_rounded_rect_indices[ii++] = 1 + ((i + 1) % point_count);
  }

  SDL_RenderGeometry(renderer, t->sdl_texture,
                     g_rounded_rect_verts, vert_count,
                     g_rounded_rect_indices, index_count);
}

// ============================================================================
// Scissor/Blend
// ============================================================================

static void sdl3_begin_scissor(int x, int y, int w, int h) {
  if (!g_current_renderer)
    return;
  sdl3_rect_batch_flush();

  // Save current scissor to stack for nested clipping
  bool clip_enabled = SDL_RenderClipEnabled(g_current_renderer);
  SDL_Rect current_clip = {0, 0, 0, 0};
  if (clip_enabled) {
    SDL_GetRenderClipRect(g_current_renderer, &current_clip);
  }

  // Push current state (sentinel {0,0,0,0} when no clip was active)
  if (scissor_stack_count < MAX_SCISSOR_STACK) {
    scissor_stack[scissor_stack_count++] = clip_enabled ? current_clip : (SDL_Rect){0, 0, 0, 0};
  }

  // Compute new scissor, intersecting with parent clip when active
  SDL_Rect r = {x, y, w, h};
  if (clip_enabled) {
    int cx2 = current_clip.x + current_clip.w;
    int cy2 = current_clip.y + current_clip.h;
    int rx2 = r.x + r.w;
    int ry2 = r.y + r.h;
    int ix1 = r.x > current_clip.x ? r.x : current_clip.x;
    int iy1 = r.y > current_clip.y ? r.y : current_clip.y;
    int ix2 = rx2 < cx2 ? rx2 : cx2;
    int iy2 = ry2 < cy2 ? ry2 : cy2;
    r.x = ix1;
    r.y = iy1;
    r.w = ix2 > ix1 ? ix2 - ix1 : 0;
    r.h = iy2 > iy1 ? iy2 - iy1 : 0;
  }
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
// Screenshot
// ============================================================================

// Signed distance from point (px,py) to a rounded rectangle centered at
// (cx,cy) with half-dimensions (hw,hh) and corner radius r.
// Returns negative inside, positive outside.
static float sdl3_sdf_roundrect(float px, float py,
                                float cx, float cy,
                                float hw, float hh, float r) {
  float dx = fmaxf(fabsf(px - cx) - hw + r, 0.0f);
  float dy = fmaxf(fabsf(py - cy) - hh + r, 0.0f);
  return sqrtf(dx * dx + dy * dy) - r;
}

static void sdl3_window_screenshot(CogitoWindow *window) {
  if (!window) return;
  CogitoSDL3Window *win = (CogitoSDL3Window *)window;
  SDL_Renderer *renderer = win->renderer;
  if (!renderer) return;

  SDL_Surface *raw = SDL_RenderReadPixels(renderer, NULL);
  if (!raw) return;

  int src_w = raw->w;
  int src_h = raw->h;

  // Determine pixel scale (Retina = 2x)
  float scale = SDL_GetWindowDisplayScale(win->sdl_window);
  if (scale < 1.0f) scale = 1.0f;

  // Window corner radius = 18dp
  float corner_r = 18.0f * scale;
  // 1dp border stroke at 38% black
  float border_w = 1.0f * scale;
  uint8_t border_a = 97; // 38% of 255

  // Shadow parameters (in surface pixels)
  int pad = (int)(64.0f * scale);
  float sigma = 24.0f * scale;
  int shadow_off_y = (int)(8.0f * scale);
  uint8_t shadow_max_a = 80; // ~31%

  // Convert to RGBA32 for pixel manipulation
  SDL_Surface *src = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(raw);
  if (!src) return;

  uint32_t *sp = (uint32_t *)src->pixels;
  int sp_pitch = src->pitch / 4;
  float half_w = src_w * 0.5f;
  float half_h = src_h * 0.5f;
  float cx = half_w;
  float cy = half_h;

  // --- SDF-based alpha mask (full perimeter, not just corners) ---
  // Non-transparent SDL windows have undefined alpha in the framebuffer.
  // We set alpha from SDF coverage for edge/corner pixels and force 255
  // for interior pixels.  A 1px inset ensures the outermost pixel ring
  // transitions cleanly instead of showing the raw clear-color.
  float inset = 1.0f; // pixels
  float mask_hw = half_w - inset;
  float mask_hh = half_h - inset;
  // Band width: how far inside the SDF boundary we process edge pixels
  int edge_band = (int)(corner_r + 3.0f);
  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      // Skip deep-interior pixels (they only need alpha forced)
      if (x >= edge_band && x < src_w - edge_band &&
          y >= edge_band && y < src_h - edge_band) {
        sp[y * sp_pitch + x] |= 0xFF000000;
        continue;
      }
      float d = sdl3_sdf_roundrect((float)x + 0.5f, (float)y + 0.5f,
                                   cx, cy, mask_hw, mask_hh, corner_r);
      if (d > 0.5f) {
        sp[y * sp_pitch + x] = 0; // fully outside → transparent black
      } else if (d > -0.5f) {
        float coverage = 0.5f - d;
        uint32_t px = sp[y * sp_pitch + x];
        uint8_t a = (uint8_t)(255.0f * coverage + 0.5f);
        sp[y * sp_pitch + x] = (px & 0x00FFFFFF) | ((uint32_t)a << 24);
      } else {
        sp[y * sp_pitch + x] |= 0xFF000000;
      }
    }
  }

  // --- Create output surface with padding for shadow ---
  int out_w = src_w + pad * 2;
  int out_h = src_h + pad * 2;
  SDL_Surface *output = SDL_CreateSurface(out_w, out_h, SDL_PIXELFORMAT_RGBA32);
  if (!output) { SDL_DestroySurface(src); return; }
  SDL_memset(output->pixels, 0, (size_t)out_h * output->pitch);

  // --- Render shadow via distance field ---
  uint32_t *op = (uint32_t *)output->pixels;
  int op_pitch = output->pitch / 4;
  float win_cx = (float)pad + half_w;
  float win_cy = (float)pad + half_h;
  float inv_sqrt2_sigma = 1.0f / (sigma * 1.4142135f);

  for (int y = 0; y < out_h; y++) {
    for (int x = 0; x < out_w; x++) {
      float py = (float)y - (float)shadow_off_y;
      float d = sdl3_sdf_roundrect((float)x + 0.5f, py + 0.5f,
                                   win_cx, win_cy, mask_hw, mask_hh, corner_r);
      if (d > -sigma * 3.0f) {
        float alpha_f = (float)shadow_max_a * 0.5f * erfcf(d * inv_sqrt2_sigma);
        uint8_t a = (alpha_f > 255.0f) ? 255 : (uint8_t)(alpha_f + 0.5f);
        if (a > 0) {
          op[y * op_pitch + x] = (uint32_t)a << 24; // black with alpha
        }
      }
    }
  }

  // --- Draw 1dp border stroke around the rounded window ---
  // Rendered directly into the output surface before the window composite
  // so only the exposed border ring (outside the opaque window) is visible.
  for (int y = 0; y < src_h + (int)(border_w * 2 + 2); y++) {
    for (int x = 0; x < src_w + (int)(border_w * 2 + 2); x++) {
      int ox = pad - (int)(border_w + 1) + x;
      int oy = pad - (int)(border_w + 1) + y;
      if (ox < 0 || oy < 0 || ox >= out_w || oy >= out_h) continue;
      float px = (float)x - (border_w + 1.0f) + 0.5f;
      float py_b = (float)y - (border_w + 1.0f) + 0.5f;
      float d = sdl3_sdf_roundrect(px, py_b, half_w, half_h,
                                   mask_hw, mask_hh, corner_r);
      // Ring: visible where d is between -border_w and 0 (the outer edge)
      float ring = fmaxf(0.0f, fminf(d + border_w, 1.0f)) *
                   fmaxf(0.0f, fminf(0.5f - d, 1.0f));
      if (ring <= 0.0f) continue;
      uint8_t ba = (uint8_t)((float)border_a * ring + 0.5f);
      // Source-over: black(ba) onto existing pixel
      uint32_t dpx = op[oy * op_pitch + ox];
      uint8_t da = dpx >> 24;
      if (da == 0) {
        op[oy * op_pitch + ox] = (uint32_t)ba << 24;
      } else {
        float saf = ba * (1.0f / 255.0f);
        float daf = da * (1.0f / 255.0f);
        float oaf = saf + daf * (1.0f - saf);
        uint8_t oa = (uint8_t)(oaf * 255.0f + 0.5f);
        // Both src and dst are black, so RGB stays 0
        uint32_t dr = dpx & 0xFF;
        uint32_t dg = (dpx >> 8) & 0xFF;
        uint32_t db = (dpx >> 16) & 0xFF;
        float inv_oaf = (oaf > 0.0f) ? 1.0f / oaf : 0.0f;
        uint8_t or2 = (uint8_t)(daf * (1.0f - saf) * inv_oaf * (float)dr + 0.5f);
        uint8_t og2 = (uint8_t)(daf * (1.0f - saf) * inv_oaf * (float)dg + 0.5f);
        uint8_t ob2 = (uint8_t)(daf * (1.0f - saf) * inv_oaf * (float)db + 0.5f);
        op[oy * op_pitch + ox] = (uint32_t)or2 | ((uint32_t)og2 << 8) |
                                  ((uint32_t)ob2 << 16) | ((uint32_t)oa << 24);
      }
    }
  }

  // --- Composite window over shadow + border (manual source-over) ---
  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      uint32_t s = sp[y * sp_pitch + x];
      uint8_t sa = s >> 24;
      if (sa == 0) continue; // corner cutout → shadow shows through
      int ox = x + pad;
      int oy = y + pad;
      if (sa == 255) {
        // Opaque (vast majority of pixels) → direct overwrite
        op[oy * op_pitch + ox] = s;
        continue;
      }
      // Semi-transparent AA band: source-over composite in straight alpha.
      // Shadow is black so dst RGB contribution is 0; only alpha adds up.
      uint32_t dpx = op[oy * op_pitch + ox];
      uint8_t da = dpx >> 24;
      if (da == 0) {
        op[oy * op_pitch + ox] = s;
        continue;
      }
      float saf = sa * (1.0f / 255.0f);
      float daf = da * (1.0f / 255.0f);
      float oaf = saf + daf * (1.0f - saf);
      float inv_oaf = 1.0f / oaf;
      // Shadow is (0,0,0,da) so only src RGB contributes to output RGB
      float sr = (float)(s & 0xFF);
      float sg = (float)((s >> 8) & 0xFF);
      float sb = (float)((s >> 16) & 0xFF);
      uint8_t or_ = (uint8_t)(sr * saf * inv_oaf + 0.5f);
      uint8_t og = (uint8_t)(sg * saf * inv_oaf + 0.5f);
      uint8_t ob = (uint8_t)(sb * saf * inv_oaf + 0.5f);
      uint8_t oa = (uint8_t)(oaf * 255.0f + 0.5f);
      op[oy * op_pitch + ox] = (uint32_t)or_ | ((uint32_t)og << 8) |
                                ((uint32_t)ob << 16) | ((uint32_t)oa << 24);
    }
  }
  SDL_DestroySurface(src);

  // --- Save as PNG ---
  const char *home = getenv("HOME");
  if (!home) home = "/tmp";
  char path[512];
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  snprintf(path, sizeof(path), "%s/Pictures/Screenshot_%04d%02d%02d_%02d%02d%02d.png",
           home, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
           t->tm_hour, t->tm_min, t->tm_sec);

#if defined(COGITO_HAS_SDL3_IMAGE)
  IMG_SavePNG(output, path);
#else
  snprintf(path, sizeof(path), "%s/Pictures/Screenshot_%04d%02d%02d_%02d%02d%02d.bmp",
           home, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
           t->tm_hour, t->tm_min, t->tm_sec);
  SDL_SaveBMP(output, path);
#endif
  SDL_DestroySurface(output);
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
    .window_set_min_size = sdl3_window_set_min_size,
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
    .choose_font_name = sdl3_choose_font_name,
    .set_clipboard_text = sdl3_set_clipboard_text,
    .get_clipboard_text = sdl3_get_clipboard_text,
    .clipboard_has = sdl3_clipboard_has,
    .clipboard_get_data = sdl3_clipboard_get_data,
    .clipboard_set_data = sdl3_clipboard_set_data,
    .begin_frame = sdl3_begin_frame,
    .end_frame = sdl3_end_frame,
    .present = sdl3_present,
    .clear = sdl3_clear,
    .set_vsync = sdl3_set_vsync,
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
    .draw_rect_linear_gradient = sdl3_draw_rect_linear_gradient,
    .draw_rect_radial_gradient = sdl3_draw_rect_radial_gradient,
    .draw_rect_rounded = sdl3_draw_rect_rounded,
    .draw_rect_lines = sdl3_draw_rect_lines,
    .draw_rect_rounded_lines = sdl3_draw_rect_rounded_lines,
    .draw_line = sdl3_draw_line,
    .draw_circle = sdl3_draw_circle,
    .draw_circle_lines = sdl3_draw_circle_lines,
    .draw_points = sdl3_draw_points,
    .font_load = sdl3_font_load,
    .font_load_face = sdl3_font_load_face,
    .font_load_pixel = sdl3_font_load_pixel,
    .font_unload = sdl3_font_unload,
    .font_get_metrics = sdl3_font_get_metrics,
    .font_get_internal_face = sdl3_font_get_internal_face,
    .font_set_variation = sdl3_font_set_variation,
    .font_set_direction = sdl3_font_set_direction,
    .font_set_style = sdl3_font_set_style,
    .text_measure_width = sdl3_text_measure_width,
    .text_measure_width_dir = sdl3_text_measure_width_dir,
    .text_measure_height = sdl3_text_measure_height,
    .draw_text = sdl3_draw_text,
    .draw_text_dir = sdl3_draw_text_dir,
    .texture_create = sdl3_texture_create,
    .texture_destroy = sdl3_texture_destroy,
    .texture_set_nearest = sdl3_texture_set_nearest,
    .texture_get_size = sdl3_texture_get_size,
    .draw_texture = sdl3_draw_texture,
    .draw_texture_pro = sdl3_draw_texture_pro,
    .render_target_create = sdl3_render_target_create,
    .set_render_target = sdl3_set_render_target,
    .texture_gaussian_blur = sdl3_texture_gaussian_blur,
    .draw_texture_polygon = sdl3_draw_texture_polygon,
    .begin_scissor = sdl3_begin_scissor,
    .end_scissor = sdl3_end_scissor,
    .set_blend_mode = sdl3_set_blend_mode,
    .set_cursor = sdl3_set_cursor,
    .window_set_hit_test_callback = sdl3_window_set_hit_test_callback,
    .window_set_borderless = sdl3_window_set_borderless,
    .window_is_borderless = sdl3_window_is_borderless,
    .set_debug_overlay = sdl3_set_debug_overlay,
    .window_screenshot = sdl3_window_screenshot,
};

bool cogito_backend_sdl3_init(void) {
  cogito_backend = &sdl3_backend;
  return sdl3_init();
}

CogitoBackend *cogito_backend_sdl3_get(void) { return &sdl3_backend; }
