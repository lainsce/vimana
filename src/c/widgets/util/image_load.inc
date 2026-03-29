// Image loading (preserves original colors, no white mask conversion)

// --- Embedded resource registry (SVGs baked into the binary as base64) ---
typedef struct {
  const char *name;
  const unsigned char *data;
  size_t len;
} CogitoEmbeddedResource;

#define COGITO_MAX_EMBEDDED 64
static CogitoEmbeddedResource cogito_embedded_resources[COGITO_MAX_EMBEDDED];
static size_t cogito_embedded_count = 0;

void cogito_register_embedded_resource(const char* name, const unsigned char* data, size_t len) {
  if (!name || !data || len == 0) return;
  if (cogito_embedded_count >= COGITO_MAX_EMBEDDED) return;
  cogito_embedded_resources[cogito_embedded_count++] =
    (CogitoEmbeddedResource){ name, data, len };
}

static const unsigned char* cogito_find_embedded_resource(const char* name, size_t* out_len) {
  if (!name) return NULL;
  for (size_t i = 0; i < cogito_embedded_count; i++) {
    if (strcmp(cogito_embedded_resources[i].name, name) == 0) {
      if (out_len) *out_len = cogito_embedded_resources[i].len;
      return cogito_embedded_resources[i].data;
    }
  }
  // Try matching by basename (e.g. "examples/assets/image.jpg" -> "image.jpg")
  const char* slash = strrchr(name, '/');
  if (slash && slash[1]) {
    const char* bname = slash + 1;
    for (size_t i = 0; i < cogito_embedded_count; i++) {
      if (strcmp(cogito_embedded_resources[i].name, bname) == 0) {
        if (out_len) *out_len = cogito_embedded_resources[i].len;
        return cogito_embedded_resources[i].data;
      }
    }
  }
  return NULL;
}

static char cogito_script_dir[PATH_MAX] = {0};

void cogito_set_script_dir(const char* dir) {
  if (dir && dir[0]) {
    snprintf(cogito_script_dir, sizeof(cogito_script_dir), "%s", dir);
  } else {
    cogito_script_dir[0] = 0;
  }
}

const char* cogito_get_script_dir(void) {
  return cogito_script_dir[0] ? cogito_script_dir : NULL;
}

static bool cogito_path_is_absolute(const char* path) {
  if (!path || !path[0]) return false;
#if defined(_WIN32)
  if ((isalpha((unsigned char)path[0]) && path[1] == ':') ||
      path[0] == '\\' || path[0] == '/') return true;
  return false;
#else
  return path[0] == '/';
#endif
}

static bool cogito_try_join_path(const char* base, const char* rel,
                                 char* resolved, size_t resolved_cap) {
  if (!base || !base[0] || !rel || !rel[0] || !resolved || resolved_cap == 0) return false;
  size_t bl = strlen(base);
  bool has_sep = bl > 0 && (base[bl - 1] == '/' || base[bl - 1] == '\\');
  if (has_sep) snprintf(resolved, resolved_cap, "%s%s", base, rel);
  else snprintf(resolved, resolved_cap, "%s/%s", base, rel);
  return cogito_file_exists(resolved);
}

static const char* cogito_image_strip_legacy_prefix(const char* rel_path) {
  if (!rel_path) return rel_path;
  if (strncmp(rel_path, "cogito/", 7) == 0) return rel_path + 7;
  if (strncmp(rel_path, "Cogito/", 7) == 0) return rel_path + 7;
  return rel_path;
}

static bool cogito_image_resolve_path(const char* path, char* resolved, size_t resolved_cap) {
  if (!path || !path[0] || !resolved || resolved_cap == 0) return false;

  // Check if path is absolute - if so, try it directly first
  if (cogito_path_is_absolute(path)) {
    if (cogito_file_exists(path)) {
      snprintf(resolved, resolved_cap, "%s", path);
      return true;
    }
    return false;
  }

  // Skip leading '/' in path and treat as relative
  const char* rel_path = path;
  if (path[0] == '/') rel_path = path + 1;
  const char* rel_legacy = cogito_image_strip_legacy_prefix(rel_path);

  const char* candidates[2] = {rel_path, rel_legacy};

  // Try script directory first (and then its parent as a project root fallback)
  if (cogito_script_dir[0]) {
    for (int i = 0; i < 2; i++) {
      const char* c = candidates[i];
      if (!c || !c[0]) continue;
      if (i == 1 && strcmp(candidates[0], c) == 0) continue;
      if (cogito_try_join_path(cogito_script_dir, c, resolved, resolved_cap)) return true;
    }

    char script_parent[PATH_MAX];
    snprintf(script_parent, sizeof(script_parent), "%s", cogito_script_dir);
    size_t sl = strlen(script_parent);
    while (sl > 0 && (script_parent[sl - 1] == '/' || script_parent[sl - 1] == '\\')) {
      script_parent[--sl] = 0;
    }
    char* slash = strrchr(script_parent, '/');
#if defined(_WIN32)
    char* bslash = strrchr(script_parent, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (slash) {
      *slash = 0;
      if (script_parent[0]) {
        for (int i = 0; i < 2; i++) {
          const char* c = candidates[i];
          if (!c || !c[0]) continue;
          if (i == 1 && strcmp(candidates[0], c) == 0) continue;
          if (cogito_try_join_path(script_parent, c, resolved, resolved_cap)) return true;
        }
      }
    }
  }

  // Try app bundle Resources directory (macOS .app/Contents/Resources/,
  // Linux executable sibling directory ../share/ or ../Resources/)
#if defined(__APPLE__)
  {
    char exe_path[PATH_MAX];
    uint32_t exe_size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &exe_size) == 0) {
      // exe is at <bundle>/Contents/MacOS/<name>, Resources is at <bundle>/Contents/Resources/
      char* last_slash = strrchr(exe_path, '/');
      if (last_slash) {
        *last_slash = 0;  // now <bundle>/Contents/MacOS
        char* macos_slash = strrchr(exe_path, '/');
        if (macos_slash) {
          *macos_slash = 0;  // now <bundle>/Contents
          char res_dir[PATH_MAX];
          snprintf(res_dir, sizeof(res_dir), "%s/Resources", exe_path);
          for (int i = 0; i < 2; i++) {
            const char* c = candidates[i];
            if (!c || !c[0]) continue;
            if (i == 1 && strcmp(candidates[0], c) == 0) continue;
            if (cogito_try_join_path(res_dir, c, resolved, resolved_cap)) return true;
          }
        }
      }
    }
  }
#elif defined(__linux__)
  {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
      exe_path[len] = 0;
      char* last_slash = strrchr(exe_path, '/');
      if (last_slash) {
        *last_slash = 0;  // directory containing executable
        // Try sibling Resources/ directory
        char res_dir[PATH_MAX];
        snprintf(res_dir, sizeof(res_dir), "%s/Resources", exe_path);
        for (int i = 0; i < 2; i++) {
          const char* c = candidates[i];
          if (!c || !c[0]) continue;
          if (i == 1 && strcmp(candidates[0], c) == 0) continue;
          if (cogito_try_join_path(res_dir, c, resolved, resolved_cap)) return true;
        }
        // Try ../share/<path> for XDG-style layout
        snprintf(res_dir, sizeof(res_dir), "%s/../share", exe_path);
        for (int i = 0; i < 2; i++) {
          const char* c = candidates[i];
          if (!c || !c[0]) continue;
          if (i == 1 && strcmp(candidates[0], c) == 0) continue;
          if (cogito_try_join_path(res_dir, c, resolved, resolved_cap)) return true;
        }
      }
    }
  }
#endif

  // Try current working directory
  if (getcwd(resolved, resolved_cap)) {
    char cwd[PATH_MAX];
    snprintf(cwd, sizeof(cwd), "%s", resolved);
    for (int i = 0; i < 2; i++) {
      const char* c = candidates[i];
      if (!c || !c[0]) continue;
      if (i == 1 && strcmp(candidates[0], c) == 0) continue;
      if (cogito_try_join_path(cwd, c, resolved, resolved_cap)) return true;
    }
  }

  // Try as-is (relative to cwd)
  snprintf(resolved, resolved_cap, "%s", rel_path);
  if (cogito_file_exists(resolved)) return true;
  if (rel_legacy && rel_legacy[0] && strcmp(rel_legacy, rel_path) != 0) {
    snprintf(resolved, resolved_cap, "%s", rel_legacy);
    if (cogito_file_exists(resolved)) return true;
  }
  return false;
}

// --- Image texture cache: deduplicate loads of the same file path ---
typedef struct {
  bool used;
  char* path;
  CogitoTexture* texture;
  int w, h;
  float avg_lum;
  int refcount;
  uint64_t last_used;
  size_t size_bytes;
} CogitoImageCacheEntry;

#define COGITO_IMAGE_CACHE_MAX 64
#define COGITO_IMAGE_CACHE_MAX_BYTES (96u * 1024u * 1024u)
#define COGITO_IMAGE_CACHE_EVICT_AGE 180u
static CogitoImageCacheEntry cogito_image_cache[COGITO_IMAGE_CACHE_MAX];
static uint64_t cogito_image_cache_frame = 0;
static size_t cogito_image_cache_bytes = 0;

static size_t cogito_image_cache_estimate_bytes(int w, int h) {
  if (w <= 0 || h <= 0) return 0;
  return (size_t)w * (size_t)h * 4u;
}

static void cogito_image_cache_drop_entry(int idx) {
  if (idx < 0 || idx >= COGITO_IMAGE_CACHE_MAX) return;
  CogitoImageCacheEntry* e = &cogito_image_cache[idx];
  if (!e->used) return;
  if (e->texture && cogito_backend && cogito_backend->texture_destroy) {
    cogito_backend->texture_destroy(e->texture);
  }
  if (e->size_bytes <= cogito_image_cache_bytes) cogito_image_cache_bytes -= e->size_bytes;
  else cogito_image_cache_bytes = 0;
  free(e->path);
  memset(e, 0, sizeof(*e));
}

static int cogito_image_cache_oldest_unref_idx(int skip_idx) {
  uint64_t oldest = UINT64_MAX;
  int oldest_idx = -1;
  for (int i = 0; i < COGITO_IMAGE_CACHE_MAX; i++) {
    CogitoImageCacheEntry* e = &cogito_image_cache[i];
    if (!e->used || i == skip_idx || e->refcount > 0) continue;
    if (e->last_used < oldest) {
      oldest = e->last_used;
      oldest_idx = i;
    }
  }
  return oldest_idx;
}

static void cogito_image_cache_trim(size_t max_bytes, int keep_idx) {
  while (cogito_image_cache_bytes > max_bytes) {
    int victim = cogito_image_cache_oldest_unref_idx(keep_idx);
    if (victim < 0) break;
    cogito_image_cache_drop_entry(victim);
  }
}

static int cogito_image_cache_alloc_slot(void) {
  for (int i = 0; i < COGITO_IMAGE_CACHE_MAX; i++) {
    if (!cogito_image_cache[i].used) return i;
  }
  int victim = cogito_image_cache_oldest_unref_idx(-1);
  if (victim >= 0) {
    cogito_image_cache_drop_entry(victim);
    return victim;
  }
  return -1;
}

static CogitoImageCacheEntry* cogito_image_cache_find(const char* resolved) {
  for (int i = 0; i < COGITO_IMAGE_CACHE_MAX; i++) {
    CogitoImageCacheEntry* e = &cogito_image_cache[i];
    if (!e->used || !e->path) continue;
    if (strcmp(e->path, resolved) != 0) continue;
    e->last_used = cogito_image_cache_frame;
    return e;
  }
  return NULL;
}

static bool cogito_image_cache_add(const char* resolved, CogitoTexture* tex, int w, int h, float avg_lum) {
  if (!resolved || !resolved[0] || !tex) return false;
  int slot = cogito_image_cache_alloc_slot();
  if (slot < 0) return false;
  CogitoImageCacheEntry* e = &cogito_image_cache[slot];
  e->used = true;
  e->path = strdup(resolved);
  if (!e->path) {
    memset(e, 0, sizeof(*e));
    return false;
  }
  e->texture = tex;
  e->w = w;
  e->h = h;
  e->avg_lum = avg_lum;
  e->refcount = 1;
  e->last_used = cogito_image_cache_frame;
  e->size_bytes = cogito_image_cache_estimate_bytes(w, h);
  cogito_image_cache_bytes += e->size_bytes;
  cogito_image_cache_trim(COGITO_IMAGE_CACHE_MAX_BYTES, slot);
  return true;
}

static void cogito_image_cache_clear(void) {
  for (int i = 0; i < COGITO_IMAGE_CACHE_MAX; i++) {
    cogito_image_cache_drop_entry(i);
  }
  cogito_image_cache_bytes = 0;
}

static void cogito_image_cache_frame_start(void) {
  cogito_image_cache_frame++;
  if (cogito_image_cache_frame % 13u != 0u) return;
  for (int i = 0; i < COGITO_IMAGE_CACHE_MAX; i++) {
    CogitoImageCacheEntry* e = &cogito_image_cache[i];
    if (!e->used || e->refcount > 0) continue;
    if ((cogito_image_cache_frame - e->last_used) > COGITO_IMAGE_CACHE_EVICT_AGE) {
      cogito_image_cache_drop_entry(i);
    }
  }
  cogito_image_cache_trim(COGITO_IMAGE_CACHE_MAX_BYTES, -1);
}

static void cogito_image_cache_release_texture(CogitoTexture* tex) {
  if (!tex) return;
  for (int i = 0; i < COGITO_IMAGE_CACHE_MAX; i++) {
    CogitoImageCacheEntry* e = &cogito_image_cache[i];
    if (!e->used || e->texture != tex) continue;
    if (e->refcount > 0) e->refcount--;
    e->last_used = cogito_image_cache_frame;
    cogito_image_cache_trim(COGITO_IMAGE_CACHE_MAX_BYTES, -1);
    return;
  }
  if (cogito_backend && cogito_backend->texture_destroy) {
    cogito_backend->texture_destroy(tex);
  }
}

static int cogito_image_bucket_dim(int v) {
  if (v <= 0) return 0;
  int b = 64;
  while (b < v && b < 4096) b <<= 1;
  return b;
}

static bool cogito_image_load_file_texture(const char* path, int target_w, int target_h,
                                           CogitoTexture** out, int* out_w, int* out_h,
                                           float* out_avg_lum) {
  if (!path || !path[0] || !cogito_backend || !cogito_backend->texture_create) return false;

  // Check for embedded resources before file resolution
  size_t embedded_len = 0;
  const unsigned char* embedded_data = cogito_find_embedded_resource(path, &embedded_len);

  if (embedded_data && embedded_len > 0) {
    int bucket_w = cogito_image_bucket_dim(target_w);
    int bucket_h = cogito_image_bucket_dim(target_h);
    char cache_key[PATH_MAX + 64];
    if (bucket_w > 0 || bucket_h > 0) {
      snprintf(cache_key, sizeof(cache_key), "embedded:%s#%d:%d", path, bucket_w, bucket_h);
    } else {
      snprintf(cache_key, sizeof(cache_key), "embedded:%s#orig", path);
    }
    CogitoImageCacheEntry* cached_e = cogito_image_cache_find(cache_key);
    if (cached_e && cached_e->texture) {
      cached_e->refcount++;
      if (out) *out = cached_e->texture;
      if (out_w) *out_w = cached_e->w;
      if (out_h) *out_h = cached_e->h;
      if (out_avg_lum) *out_avg_lum = cached_e->avg_lum;
      return true;
    }

    // Try SVG rendering for embedded SVG data
#if defined(__APPLE__) || defined(COGITO_HAS_RSVG)
    {
      extern unsigned char* cogito_svg_render_native_from_data(const unsigned char*, size_t, int, int, int*, int*);
      int render_w = (bucket_w > 0) ? bucket_w : 256;
      int render_h = (bucket_h > 0) ? bucket_h : 256;
      int sw = 0, sh = 0;
      unsigned char* pixels = cogito_svg_render_native_from_data(embedded_data, embedded_len, render_w, render_h, &sw, &sh);
      if (pixels && sw > 0 && sh > 0) {
        float avg_lum = 0.5f;
        {
          double total = 0.0; int count = 0;
          int step = (sw > 64 || sh > 64) ? 8 : 2;
          for (int sy = 0; sy < sh; sy += step) {
            for (int sx = 0; sx < sw; sx += step) {
              const unsigned char* px = pixels + ((size_t)sy * (size_t)sw + (size_t)sx) * 4u;
              total += 0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2];
              count++;
            }
          }
          avg_lum = (count > 0) ? (float)(total / (count * 255.0)) : 0.5f;
        }
        CogitoTexture* tex = cogito_backend->texture_create(sw, sh, pixels, 4);
        free(pixels);
        if (tex) {
          (void)cogito_image_cache_add(cache_key, tex, sw, sh, avg_lum);
          if (out) *out = tex;
          if (out_w) *out_w = sw;
          if (out_h) *out_h = sh;
          if (out_avg_lum) *out_avg_lum = avg_lum;
          return true;
        }
      } else {
        free(pixels);
      }
    }
#endif

    // Try raster image loading from embedded data (JPG, PNG, etc.)
#if defined(COGITO_HAS_SDL3_IMAGE)
    {
      SDL_IOStream* io = SDL_IOFromMem((void*)embedded_data, embedded_len);
      if (io) {
        SDL_Surface* src = IMG_Load_IO(io, true);
        if (src) {
          SDL_Surface* rgba = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
          SDL_DestroySurface(src);
          if (rgba && rgba->w > 0 && rgba->h > 0 && rgba->pixels) {
            int w = rgba->w, h = rgba->h;
            float avg_lum = 0.5f;
            {
              const uint8_t* px_base = (const uint8_t*)rgba->pixels;
              int pitch = rgba->pitch;
              double total = 0.0; int count = 0;
              int step = (w > 64 || h > 64) ? 8 : 2;
              for (int sy = 0; sy < h; sy += step) {
                for (int sx = 0; sx < w; sx += step) {
                  const uint8_t* px = px_base + (size_t)sy * (size_t)pitch + (size_t)sx * 4u;
                  total += 0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2];
                  count++;
                }
              }
              avg_lum = (count > 0) ? (float)(total / (count * 255.0)) : 0.5f;
            }
            size_t row = (size_t)w * 4u;
            CogitoTexture* tex = NULL;
            if ((size_t)rgba->pitch == row) {
              tex = cogito_backend->texture_create(w, h, (const unsigned char*)rgba->pixels, 4);
            } else {
              unsigned char* buf = (unsigned char*)malloc(row * (size_t)h);
              if (buf) {
                for (int y = 0; y < h; y++)
                  memcpy(buf + y * row, (const uint8_t*)rgba->pixels + y * rgba->pitch, row);
                tex = cogito_backend->texture_create(w, h, buf, 4);
                free(buf);
              }
            }
            SDL_DestroySurface(rgba);
            if (tex) {
              (void)cogito_image_cache_add(cache_key, tex, w, h, avg_lum);
              if (out) *out = tex;
              if (out_w) *out_w = w;
              if (out_h) *out_h = h;
              if (out_avg_lum) *out_avg_lum = avg_lum;
              return true;
            }
          } else {
            if (rgba) SDL_DestroySurface(rgba);
          }
        }
      }
    }
#endif
  }

  // Resolve the path
  char resolved_path[PATH_MAX];
  if (!cogito_image_resolve_path(path, resolved_path, sizeof(resolved_path))) {
    return false;
  }

  int bucket_w = cogito_image_bucket_dim(target_w);
  int bucket_h = cogito_image_bucket_dim(target_h);
  char cache_key[PATH_MAX + 64];
  if (bucket_w > 0 || bucket_h > 0) {
    snprintf(cache_key, sizeof(cache_key), "%s#%d:%d", resolved_path, bucket_w, bucket_h);
  } else {
    snprintf(cache_key, sizeof(cache_key), "%s#orig", resolved_path);
  }

  // Check cache first
  CogitoImageCacheEntry* cached = cogito_image_cache_find(cache_key);
  if (cached && cached->texture) {
    cached->refcount++;
    if (out) *out = cached->texture;
    if (out_w) *out_w = cached->w;
    if (out_h) *out_h = cached->h;
    if (out_avg_lum) *out_avg_lum = cached->avg_lum;
    return true;
  }

  // Detect SVG files and try native rendering (macOS: NSImage) for full
  // filter/gradient/mask support that nanosvg drops.
  bool is_svg = false;
  {
    size_t pl = strlen(resolved_path);
    if (pl >= 4 && strcasecmp(resolved_path + pl - 4, ".svg") == 0) is_svg = true;
  }

#if defined(__APPLE__) || defined(COGITO_HAS_RSVG)
  if (is_svg) {
    extern unsigned char* cogito_svg_render_native(const char*, int, int, int*, int*);
    int render_w = (bucket_w > 0) ? bucket_w : 256;
    int render_h = (bucket_h > 0) ? bucket_h : 256;
    int sw = 0, sh = 0;
    unsigned char* pixels = cogito_svg_render_native(resolved_path, render_w, render_h, &sw, &sh);
    if (pixels && sw > 0 && sh > 0) {
      // Compute average luminance
      float avg_lum = 0.5f;
      {
        double total = 0.0; int count = 0;
        int step = (sw > 64 || sh > 64) ? 8 : 2;
        for (int sy = 0; sy < sh; sy += step) {
          for (int sx = 0; sx < sw; sx += step) {
            const unsigned char* px = pixels + ((size_t)sy * (size_t)sw + (size_t)sx) * 4u;
            total += 0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2];
            count++;
          }
        }
        avg_lum = (count > 0) ? (float)(total / (count * 255.0)) : 0.5f;
      }
      CogitoTexture* tex = cogito_backend->texture_create(sw, sh, pixels, 4);
      free(pixels);
      if (tex) {
        (void)cogito_image_cache_add(cache_key, tex, sw, sh, avg_lum);
        if (out) *out = tex;
        if (out_w) *out_w = sw;
        if (out_h) *out_h = sh;
        if (out_avg_lum) *out_avg_lum = avg_lum;
        return true;
      }
    } else {
      free(pixels);
    }
  }
#else
  (void)is_svg;
#endif

#if defined(COGITO_HAS_SDL3_IMAGE)
  SDL_Surface* src = IMG_Load(resolved_path);
  if (!src) return false;
  SDL_Surface* rgba = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(src);
  if (!rgba || rgba->w <= 0 || rgba->h <= 0 || !rgba->pixels) {
    if (rgba) SDL_DestroySurface(rgba);
    return false;
  }
  // Cap texture dimensions using a target-aware budget:
  // decode at about 2x the requested display size to preserve quality while
  // avoiding giant transient allocations.
  #define COGITO_IMAGE_MAX_DIM 2048
  #define COGITO_IMAGE_MIN_DIM 64
  int target_max_dim = COGITO_IMAGE_MAX_DIM;
  int requested_max = (target_w > target_h) ? target_w : target_h;
  if (requested_max > 0) {
    target_max_dim = requested_max * 2;
    if (target_max_dim < COGITO_IMAGE_MIN_DIM) target_max_dim = COGITO_IMAGE_MIN_DIM;
    if (target_max_dim > COGITO_IMAGE_MAX_DIM) target_max_dim = COGITO_IMAGE_MAX_DIM;
  }
  if (rgba->w > target_max_dim || rgba->h > target_max_dim) {
    int dw = rgba->w, dh = rgba->h;
    while (dw > target_max_dim || dh > target_max_dim) {
      dw /= 2; dh /= 2;
    }
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    SDL_Surface* scaled = SDL_CreateSurface(dw, dh, SDL_PIXELFORMAT_RGBA32);
    if (scaled) {
      SDL_BlitSurfaceScaled(rgba, NULL, scaled, NULL, SDL_SCALEMODE_LINEAR);
      SDL_DestroySurface(rgba);
      rgba = scaled;
    }
  }

  int w = rgba->w;
  int h = rgba->h;
  size_t row = (size_t)w * 4u;
  // Compute average luminance directly from the SDL surface (no extra copy).
  float avg_lum = 0.5f;
  {
    const uint8_t* px_base = (const uint8_t*)rgba->pixels;
    int pitch = rgba->pitch;
    double total = 0.0;
    int count = 0;
    int step = (w > 64 || h > 64) ? 8 : 2;
    for (int sy = 0; sy < h; sy += step) {
      for (int sx = 0; sx < w; sx += step) {
        const uint8_t* px = px_base + (size_t)sy * (size_t)pitch + (size_t)sx * 4u;
        total += 0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2];
        count++;
      }
    }
    avg_lum = (count > 0) ? (float)(total / (count * 255.0)) : 0.5f;
  }
  // If pitch matches expected row stride, pass surface pixels directly to
  // texture_create (channels==4 path avoids another copy). Otherwise copy
  // row-by-row into a contiguous buffer.
  CogitoTexture* tex = NULL;
  if ((size_t)rgba->pitch == row) {
    tex = cogito_backend->texture_create(w, h, (const uint8_t*)rgba->pixels, 4);
  } else {
    uint8_t* packed = (uint8_t*)malloc(row * (size_t)h);
    if (packed) {
      const uint8_t* sp = (const uint8_t*)rgba->pixels;
      for (int y = 0; y < h; y++) {
        memcpy(packed + (size_t)y * row, sp + (size_t)y * (size_t)rgba->pitch, row);
      }
      tex = cogito_backend->texture_create(w, h, packed, 4);
      free(packed);
    }
  }
  SDL_DestroySurface(rgba);
  if (!tex) return false;

  // After large image decode, nudge malloc to return freed pages to the OS.
  // Without this, the decode buffers (potentially 64 MB+) stay mapped even
  // after free(), inflating RSS.
#if defined(__APPLE__)
  extern size_t malloc_zone_pressure_relief(void*, size_t);
  malloc_zone_pressure_relief(NULL, 0);
#endif

  // Add to cache (if a slot is available); otherwise return uncached texture.
  (void)cogito_image_cache_add(cache_key, tex, w, h, avg_lum);

  if (out) *out = tex;
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
  if (out_avg_lum) *out_avg_lum = avg_lum;
  return true;
#else
  (void)out;
  (void)out_w;
  (void)out_h;
  (void)out_avg_lum;
  return false;
#endif
}

static void cogito_load_sum_file(const char* path);
