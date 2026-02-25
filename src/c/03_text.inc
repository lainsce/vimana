// Cogito Text Rendering
// Font loading, measurement, editing, and variable font support via FreeType

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H

static bool cogito_node_bold(const CogitoNode* n);

// Lazy-init FreeType library for variable font operations
static FT_Library cogito_ft_lib = NULL;
static bool cogito_ft_lib_inited = false;

static FT_Library cogito_ft_library(void) {
  if (!cogito_ft_lib_inited) {
    cogito_ft_lib_inited = true;
    FT_Init_FreeType(&cogito_ft_lib);
  }
  return cogito_ft_lib;
}

static void cogito_ft_library_done(void) {
  if (cogito_ft_lib) {
    FT_Done_FreeType(cogito_ft_lib);
    cogito_ft_lib = NULL;
  }
  cogito_ft_lib_inited = false;
}

static int cogito_utf8_codepoint_count(const char* s) {
  if (!s) return 0;
  int count = 0;
  while (*s) {
    if ((*s & 0xC0) != 0x80) count++;
    s++;
  }
  return count;
}

static int cogito_font_parse_int_env(const char* name, int fallback) {
  if (!name) return fallback;
  const char* env = getenv(name);
  if (!env || !env[0]) return fallback;
  char* end = NULL;
  long v = strtol(env, &end, 10);
  if (end == env || *end != '\0') return fallback;
  if (v < 1) return fallback;
  return (int)v;
}

static int cogito_font_weight_normal(void) {
  return cogito_font_parse_int_env("COGITO_FONT_WEIGHT", 400);
}

static int cogito_font_weight_bold(void) {
  return cogito_font_parse_int_env("COGITO_FONT_BOLD_WEIGHT", 700);
}

__attribute__((unused))
static int cogito_default_codepoints(int* out, int cap) {
  int count = 0;
  if (!out || cap <= 0) return 0;
  for (int cp = 32; cp <= 126 && count < cap; cp++) out[count++] = cp;
  for (int cp = 160; cp <= 255 && count < cap; cp++) out[count++] = cp;
  return count;
}

static bool cogito_freetype_set_weight(FT_Library lib, FT_Face face, int weight, bool* out_applied) {
  if (!face || weight <= 0) return false;
  if (!(face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS)) return false;
  FT_MM_Var* mm = NULL;
  if (FT_Get_MM_Var(face, &mm) != 0 || !mm) return false;
  int axis_index = -1;
  for (FT_UInt i = 0; i < mm->num_axis; i++) {
    FT_Var_Axis* axis = &mm->axis[i];
    if (axis->tag == FT_MAKE_TAG('w', 'g', 'h', 't') || axis->tag == FT_MAKE_TAG('W', 'G', 'H', 'T')) {
      axis_index = (int)i;
      break;
    }
  }
  if (axis_index < 0) {
    FT_Done_MM_Var(lib, mm);
    return false;
  }
  FT_Fixed* coords = (FT_Fixed*)malloc(sizeof(FT_Fixed) * mm->num_axis);
  if (!coords) {
    FT_Done_MM_Var(lib, mm);
    return false;
  }
  for (FT_UInt i = 0; i < mm->num_axis; i++) coords[i] = mm->axis[i].def;
  FT_Fixed min = mm->axis[axis_index].minimum;
  FT_Fixed max = mm->axis[axis_index].maximum;
  FT_Fixed desired = (FT_Fixed)((long)weight * 65536);
  if (desired < min) desired = min;
  if (desired > max) desired = max;
  coords[axis_index] = desired;
  if (out_applied) *out_applied = true;
  FT_Set_Var_Design_Coordinates(face, mm->num_axis, coords);
  free(coords);
  FT_Done_MM_Var(lib, mm);
  return true;
}

// Find a named instance in a variable font whose weight axis matches target.
// Returns the FreeType face_index encoding, or -1 if none found.
static int cogito_find_named_instance_for_weight(const char* path, int target_weight) {
  FT_Library lib = cogito_ft_library();
  if (!lib) return -1;

  FT_Face face;
  if (FT_New_Face(lib, path, 0, &face) != 0) return -1;

  if (!(face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS)) {
    FT_Done_Face(face);
    return -1;
  }

  FT_MM_Var* mm = NULL;
  if (FT_Get_MM_Var(face, &mm) != 0 || !mm) {
    FT_Done_Face(face);
    return -1;
  }

  int weight_axis = -1;
  for (FT_UInt i = 0; i < mm->num_axis; i++) {
    if (mm->axis[i].tag == FT_MAKE_TAG('w', 'g', 'h', 't')) {
      weight_axis = (int)i;
      break;
    }
  }

  if (weight_axis < 0 || mm->num_namedstyles == 0) {
    FT_Done_MM_Var(lib, mm);
    FT_Done_Face(face);
    return -1;
  }

  int best_instance = -1;
  FT_Fixed best_diff = (FT_Fixed)INT_MAX;
  FT_Fixed target = (FT_Fixed)((long)target_weight << 16);

  for (FT_UInt i = 0; i < mm->num_namedstyles; i++) {
    FT_Fixed inst_weight = mm->namedstyle[i].coords[weight_axis];
    FT_Fixed diff = inst_weight > target ? inst_weight - target : target - inst_weight;
    if (diff < best_diff) {
      best_diff = diff;
      best_instance = (int)i;
    }
  }

  FT_Done_MM_Var(lib, mm);
  FT_Done_Face(face);

  // Only accept if the match is within 50 weight units
  if (best_instance < 0 || best_diff > ((FT_Fixed)50 << 16)) return -1;

  // FreeType named-instance face index: base_face | ((instance + 1) << 16)
  return (0 | ((best_instance + 1) << 16));
}

static bool cogito_load_font_file_ex(CogitoFont** out, const char* path, int size, int weight, bool* out_weight_applied) {
  if (out_weight_applied) *out_weight_applied = false;
  if (!out || !path || !path[0]) return false;
  if (!cogito_backend || !cogito_backend->font_load) return false;

  // Strategy 1: Load a named instance whose weight matches
  if (weight > 0 && cogito_backend->font_load_face) {
    int face_index = cogito_find_named_instance_for_weight(path, weight);
    if (face_index >= 0) {
      *out = cogito_backend->font_load_face(path, size, face_index);
      if (*out) {
        if (out_weight_applied) *out_weight_applied = true;
        return true;
      }
    }
  }

  // Strategy 2: Load normally, then set the weight axis via internal face
  *out = cogito_backend->font_load(path, size);
  if (!*out) return false;

  if (weight > 0 && cogito_backend->font_get_internal_face) {
    FT_Face face = (FT_Face)cogito_backend->font_get_internal_face(*out);
    if (face) {
      FT_Library lib = cogito_ft_library();
      if (lib) {
        bool applied = false;
        cogito_freetype_set_weight(lib, face, weight, &applied);
        if (applied && out_weight_applied) *out_weight_applied = true;
      }
    }
  }

  return true;
}

static bool cogito_load_font_file(CogitoFont** out, const char* path, int size, int weight) {
  return cogito_load_font_file_ex(out, path, size, weight, NULL);
}

static bool cogito_load_font_file_tabular(CogitoFont** out, const char* path, int size, int weight) {
  // Tabular figures (tnum) require HarfBuzz OpenType feature activation,
  // which SDL_ttf does not currently expose. Load the font normally;
  // tabular digit spacing is simulated in Cogito measurement/draw paths.
  return cogito_load_font_file(out, path, size, weight);
}

static CogitoFont* cogito_font_cache_get(CogitoFontCacheEntry* cache, int cap, const char* path, int size, int weight);

static CogitoFont* cogito_font_resolve_size(CogitoFont* f, int size) {
  if (!f) return NULL;
  if (size <= 0) size = cogito_font_size();

  if (f == cogito_font) {
    if (!cogito_font_from_env || !cogito_font_path_active || !cogito_font_path_active[0]) return f;
    if (cogito_font_loaded_size == size) return f;
    CogitoFont* cached = cogito_font_cache_get(
      cogito_font_cache,
      (int)(sizeof(cogito_font_cache) / sizeof(cogito_font_cache[0])),
      cogito_font_path_active,
      size,
      cogito_font_weight_normal()
    );
    return cached ? cached : f;
  }

  if (f == cogito_font_serif) {
    if (!cogito_font_serif_from_env || !cogito_font_serif_path_active || !cogito_font_serif_path_active[0]) return f;
    if (cogito_font_serif_loaded_size == size) return f;
    CogitoFont* cached = cogito_font_cache_get(
      cogito_font_serif_cache,
      (int)(sizeof(cogito_font_serif_cache) / sizeof(cogito_font_serif_cache[0])),
      cogito_font_serif_path_active,
      size,
      cogito_font_weight_normal()
    );
    return cached ? cached : f;
  }

  if (f == cogito_font_bold) {
    if (!cogito_font_bold_from_env || !cogito_font_bold_path_active || !cogito_font_bold_path_active[0]) return f;
    if (cogito_font_bold_loaded_size == size) return f;
    CogitoFont* cached = cogito_font_cache_get(
      cogito_font_bold_cache,
      (int)(sizeof(cogito_font_bold_cache) / sizeof(cogito_font_bold_cache[0])),
      cogito_font_bold_path_active,
      size,
      cogito_font_weight_bold()
    );
    return cached ? cached : f;
  }

  return f;
}

static CogitoFont* cogito_font_cache_get(CogitoFontCacheEntry* cache, int cap, const char* path, int size, int weight) {
  if (!cache || cap <= 0 || !path || !path[0] || size <= 0) return NULL;
  
  for (int i = 0; i < cap; i++) {
    if (cache[i].ready && cache[i].size == size) return cache[i].font;
  }
  
  int slot = -1;
  for (int i = 0; i < cap; i++) {
    if (!cache[i].ready) { slot = i; break; }
  }
  if (slot < 0) slot = 0;
  
  if (cache[slot].ready) {
    if (cogito_backend && cogito_backend->font_unload) {
      cogito_backend->font_unload(cache[slot].font);
    }
    cache[slot].ready = false;
    cache[slot].size = 0;
  }
  
  if (!cogito_load_font_file_ex(&cache[slot].font, path, size, weight, NULL)) {
    return NULL;
  }
  
  cache[slot].size = size;
  cache[slot].ready = true;
  return cache[slot].font;
}

static CogitoFont* cogito_font_tabular_cache_get(const char* path, int size, int weight) {
  if (!path || !path[0] || size <= 0) return NULL;
  int cap = (int)(sizeof(cogito_font_tabular_cache) / sizeof(cogito_font_tabular_cache[0]));
  
  for (int i = 0; i < cap; i++) {
    if (cogito_font_tabular_cache[i].ready && cogito_font_tabular_cache[i].size == size) {
      return cogito_font_tabular_cache[i].font;
    }
  }
  
  int slot = -1;
  for (int i = 0; i < cap; i++) {
    if (!cogito_font_tabular_cache[i].ready) { slot = i; break; }
  }
  if (slot < 0) slot = 0;
  
  if (cogito_font_tabular_cache[slot].ready) {
    if (cogito_backend && cogito_backend->font_unload) {
      cogito_backend->font_unload(cogito_font_tabular_cache[slot].font);
    }
    cogito_font_tabular_cache[slot].ready = false;
    cogito_font_tabular_cache[slot].size = 0;
  }
  
  if (!cogito_load_font_file_tabular(&cogito_font_tabular_cache[slot].font, path, size, weight)) {
    return NULL;
  }
  
  cogito_font_tabular_cache[slot].size = size;
  cogito_font_tabular_cache[slot].ready = true;
  return cogito_font_tabular_cache[slot].font;
}

__attribute__((unused))
static CogitoFont* cogito_font_for_size(CogitoFont* f, int size) {
  if (!f) return f;
  if (size <= 0) size = cogito_font_size();
  return cogito_font_resolve_size(f, size);
}

static CogitoFont* cogito_font_tabular_for(CogitoFont* f, int size) {
  if (!f) return f;
  if (size <= 0) size = cogito_font_size();
  
  if (f == cogito_font) {
    if (!cogito_font_from_env || !cogito_font_path_active) return f;
    CogitoFont* cached = cogito_font_tabular_cache_get(cogito_font_path_active, size, cogito_font_weight_normal());
    return cached ? cached : f;
  }
  if (f == cogito_font_serif) {
    if (!cogito_font_serif_from_env || !cogito_font_serif_path_active) return f;
    CogitoFont* cached = cogito_font_tabular_cache_get(cogito_font_serif_path_active, size, cogito_font_weight_normal());
    return cached ? cached : f;
  }
  if (f == cogito_font_bold) {
    if (!cogito_font_bold_from_env || !cogito_font_bold_path_active) return f;
    CogitoFont* cached = cogito_font_tabular_cache_get(cogito_font_bold_path_active, size, cogito_font_weight_bold());
    return cached ? cached : f;
  }
  return f;
}

static void cogito_text_measure_cache_clear(void);

static void cogito_font_cache_clear(void) {
  for (size_t i = 0; i < sizeof(cogito_font_cache) / sizeof(cogito_font_cache[0]); i++) {
    if (cogito_font_cache[i].ready) {
      if (cogito_backend && cogito_backend->font_unload) {
        cogito_backend->font_unload(cogito_font_cache[i].font);
      }
      cogito_font_cache[i].ready = false;
      cogito_font_cache[i].size = 0;
    }
  }
  for (size_t i = 0; i < sizeof(cogito_font_serif_cache) / sizeof(cogito_font_serif_cache[0]); i++) {
    if (cogito_font_serif_cache[i].ready) {
      if (cogito_backend && cogito_backend->font_unload) {
        cogito_backend->font_unload(cogito_font_serif_cache[i].font);
      }
      cogito_font_serif_cache[i].ready = false;
      cogito_font_serif_cache[i].size = 0;
    }
  }
  for (size_t i = 0; i < sizeof(cogito_font_bold_cache) / sizeof(cogito_font_bold_cache[0]); i++) {
    if (cogito_font_bold_cache[i].ready) {
      if (cogito_backend && cogito_backend->font_unload) {
        cogito_backend->font_unload(cogito_font_bold_cache[i].font);
      }
      cogito_font_bold_cache[i].ready = false;
      cogito_font_bold_cache[i].size = 0;
    }
  }
  for (size_t i = 0; i < sizeof(cogito_font_tabular_cache) / sizeof(cogito_font_tabular_cache[0]); i++) {
    if (cogito_font_tabular_cache[i].ready) {
      if (cogito_backend && cogito_backend->font_unload) {
        cogito_backend->font_unload(cogito_font_tabular_cache[i].font);
      }
      cogito_font_tabular_cache[i].ready = false;
      cogito_font_tabular_cache[i].size = 0;
    }
  }
  cogito_text_measure_cache_clear();
}

static int cogito_text_width(const char* s) {
  if (!s) return 0;
  if (!cogito_backend_ready || !cogito_font_ready) {
    int len = 0;
    while (s[len]) len++;
    return len * 8;
  }
  
  if (cogito_backend && cogito_backend->text_measure_width) {
    CogitoFont* f = cogito_font_resolve_size(cogito_font, cogito_font_size());
    return cogito_backend->text_measure_width(f ? f : cogito_font, s, cogito_font_size());
  }
  
  int len = 0;
  while (s[len]) len++;
  return len * 8;
}

static int cogito_text_width_size(const char* s, int size) {
  if (!s) return 0;
  if (size <= 0) size = cogito_font_size();
  if (!cogito_backend_ready || !cogito_font_ready) {
    int len = 0;
    while (s[len]) len++;
    return len * (size / 2);
  }
  
  if (cogito_backend && cogito_backend->text_measure_width) {
    CogitoFont* f = cogito_font_resolve_size(cogito_font, size);
    return cogito_backend->text_measure_width(f ? f : cogito_font, s, size);
  }
  
  int len = 0;
  while (s[len]) len++;
  return len * (size / 2);
}

static int cogito_text_height(void) {
  if (!cogito_backend_ready || !cogito_font_ready) return 18;
  
  if (cogito_backend && cogito_backend->text_measure_height) {
    int size = cogito_font_size();
    CogitoFont* f = cogito_font_resolve_size(cogito_font, size);
    return cogito_backend->text_measure_height(f ? f : cogito_font, size);
  }
  
  return cogito_font_size() + 2;
}

static int cogito_text_height_size(int size) {
  if (size <= 0) size = cogito_font_size();
  if (!cogito_backend_ready || !cogito_font_ready) return size + 2;
  
  if (cogito_backend && cogito_backend->text_measure_height) {
    CogitoFont* f = cogito_font_resolve_size(cogito_font, size);
    return cogito_backend->text_measure_height(f ? f : cogito_font, size);
  }
  
  return size + 2;
}

static void cogito_node_set_text(CogitoNode* n, ErgoStr* s);

static int cogito_text_width_size_n(const char* s, int len, int size);
static int cogito_text_width_size_font(CogitoFont* f, const char* s, int size);

static bool cogito_is_ascii_digit_char(char c) {
  return c >= '0' && c <= '9';
}

#define COGITO_TEXT_MEASURE_CACHE_SIZE 1024
#define COGITO_TEXT_MEASURE_KEY_MAX 96
typedef struct {
  bool valid;
  uint32_t hash;
  CogitoFont* font;
  int size;
  int spacing_q64;
  uint8_t mode; // 0: normal, 1: tabular
  uint8_t len;
  char text[COGITO_TEXT_MEASURE_KEY_MAX];
  int width;
} CogitoTextMeasureCacheEntry;

static CogitoTextMeasureCacheEntry cogito_text_measure_cache[COGITO_TEXT_MEASURE_CACHE_SIZE];

static void cogito_text_measure_cache_clear(void) {
  memset(cogito_text_measure_cache, 0, sizeof(cogito_text_measure_cache));
}

static uint32_t cogito_text_measure_hash(CogitoFont* f, const char* s, int len,
                                         int size, int spacing_q64, uint8_t mode) {
  uint32_t h = 2166136261u;
  h ^= (uint32_t)(uintptr_t)f; h *= 16777619u;
  h ^= (uint32_t)size; h *= 16777619u;
  h ^= (uint32_t)(spacing_q64 + 32768); h *= 16777619u;
  h ^= (uint32_t)mode; h *= 16777619u;
  h ^= (uint32_t)len; h *= 16777619u;
  for (int i = 0; i < len; i++) {
    h ^= (uint8_t)s[i];
    h *= 16777619u;
  }
  return h;
}

static bool cogito_text_measure_cache_lookup(CogitoFont* f, const char* s, int size,
                                             float spacing, uint8_t mode, int* out_w) {
  if (!s || !out_w) return false;
  int len = (int)strnlen(s, COGITO_TEXT_MEASURE_KEY_MAX);
  if (len <= 0 || len >= COGITO_TEXT_MEASURE_KEY_MAX) return false;
  int spacing_q64 = (int)lroundf(spacing * 64.0f);
  uint32_t h = cogito_text_measure_hash(f, s, len, size, spacing_q64, mode);
  CogitoTextMeasureCacheEntry* e = &cogito_text_measure_cache[h % COGITO_TEXT_MEASURE_CACHE_SIZE];
  if (!e->valid || e->hash != h || e->font != f || e->size != size ||
      e->spacing_q64 != spacing_q64 || e->mode != mode || e->len != (uint8_t)len ||
      memcmp(e->text, s, (size_t)len) != 0) {
    return false;
  }
  *out_w = e->width;
  return true;
}

static void cogito_text_measure_cache_store(CogitoFont* f, const char* s, int size,
                                            float spacing, uint8_t mode, int width) {
  if (!s) return;
  int len = (int)strnlen(s, COGITO_TEXT_MEASURE_KEY_MAX);
  if (len <= 0 || len >= COGITO_TEXT_MEASURE_KEY_MAX) return;
  int spacing_q64 = (int)lroundf(spacing * 64.0f);
  uint32_t h = cogito_text_measure_hash(f, s, len, size, spacing_q64, mode);
  CogitoTextMeasureCacheEntry* e = &cogito_text_measure_cache[h % COGITO_TEXT_MEASURE_CACHE_SIZE];
  e->valid = true;
  e->hash = h;
  e->font = f;
  e->size = size;
  e->spacing_q64 = spacing_q64;
  e->mode = mode;
  e->len = (uint8_t)len;
  memcpy(e->text, s, (size_t)len);
  e->text[len] = '\0';
  e->width = width;
}

static int cogito_text_width_size_span_font(CogitoFont* f, const char* s, int len, int size) {
  if (!s || len <= 0) return 0;
  char stack_buf[1024];
  char* buf = stack_buf;
  bool heap = false;
  if (len >= (int)sizeof(stack_buf)) {
    buf = (char*)malloc((size_t)len + 1);
    if (!buf) return 0;
    heap = true;
  }
  memcpy(buf, s, (size_t)len);
  buf[len] = '\0';
  int w = cogito_text_width_size_font(f, buf, size);
  if (heap) free(buf);
  return w;
}

static int cogito_tabular_digit_width(CogitoFont* f, int size) {
  int max_w = 0;
  char d[2] = {0, 0};
  for (char c = '0'; c <= '9'; c++) {
    d[0] = c;
    int w = cogito_text_width_size_font(f, d, size);
    if (w > max_w) max_w = w;
  }
  if (max_w <= 0) max_w = size > 0 ? (size / 2) : 8;
  return max_w;
}

static int cogito_text_width_size_font_spaced(CogitoFont* f, const char* s, int size, float spacing) {
  if (!s) return 0;
  if (size <= 0) size = cogito_font_size();
  f = cogito_font_resolve_size(f, size);

  int cached_w = 0;
  if (cogito_text_measure_cache_lookup(f, s, size, spacing, 0, &cached_w)) {
    return cached_w;
  }

  int base;
  if (!cogito_backend_ready || !cogito_font_ready || !f) {
    int len = 0;
    while (s[len]) len++;
    base = len * (size / 2);
  } else if (cogito_backend && cogito_backend->text_measure_width) {
    base = cogito_backend->text_measure_width(f, s, size);
  } else {
    int len = 0;
    while (s[len]) len++;
    base = len * (size / 2);
  }

  if (spacing != 0.0f) {
    int cp_count = cogito_utf8_codepoint_count(s);
    if (cp_count > 1) {
      base += (int)(spacing * (float)(cp_count - 1));
    }
  }

  cogito_text_measure_cache_store(f, s, size, spacing, 0, base);
  return base;
}

static int cogito_text_width_size_font_spaced_tabular(CogitoFont* f, const char* s, int size, float spacing) {
  if (!s) return 0;
  if (size <= 0) size = cogito_font_size();
  f = cogito_font_resolve_size(f, size);

  int cached_w = 0;
  if (cogito_text_measure_cache_lookup(f, s, size, spacing, 1, &cached_w)) {
    return cached_w;
  }

  int total = 0;
  int digit_adv = cogito_tabular_digit_width(f, size);
  const char* p = s;
  const char* seg = s;
  while (*p) {
    if (cogito_is_ascii_digit_char(*p)) {
      int seg_len = (int)(p - seg);
      if (seg_len > 0) {
        total += cogito_text_width_size_span_font(f, seg, seg_len, size);
      }
      while (*p && cogito_is_ascii_digit_char(*p)) {
        total += digit_adv;
        p++;
      }
      seg = p;
      continue;
    }
    p++;
  }
  int tail_len = (int)(p - seg);
  if (tail_len > 0) {
    total += cogito_text_width_size_span_font(f, seg, tail_len, size);
  }

  if (spacing != 0.0f) {
    int cp_count = cogito_utf8_codepoint_count(s);
    if (cp_count > 1) {
      total += (int)(spacing * (float)(cp_count - 1));
    }
  }

  cogito_text_measure_cache_store(f, s, size, spacing, 1, total);
  return total;
}

static int cogito_text_width_size_font(CogitoFont* f, const char* s, int size) {
  return cogito_text_width_size_font_spaced(f, s, size, 0.0f);
}

static int cogito_text_width_size_node(const CogitoNode* n, const char* s, int size) {
  CogitoFont* f = cogito_font_for_node(n);
  if (n && cogito_node_bold(n) && f == cogito_font && cogito_font_bold_ready) {
    f = cogito_font_bold;
  }
  if (n && n->font_tabular) {
    f = cogito_font_tabular_for(f, size);
  }
  float spacing = (n && n->letter_spacing_set) ? n->letter_spacing : 0.0f;
  if (n && n->font_tabular) {
    return cogito_text_width_size_font_spaced_tabular(f, s, size, spacing);
  }
  return cogito_text_width_size_font_spaced(f, s, size, spacing);
}

static int cogito_text_width_size_n_font(CogitoFont* f, const char* s, int len, int size) {
  if (!s || len <= 0) return 0;
  if (len > 1023) len = 1023;
  char buf[1024];
  memcpy(buf, s, (size_t)len);
  buf[len] = '\0';
  return cogito_text_width_size_font(f, buf, size);
}

static int cogito_text_width_size_n_node(const CogitoNode* n, const char* s, int len, int size) {
  CogitoFont* f = cogito_font_for_node(n);
  if (n && cogito_node_bold(n) && f == cogito_font && cogito_font_bold_ready) {
    f = cogito_font_bold;
  }
  if (n && n->font_tabular) {
    f = cogito_font_tabular_for(f, size);
  }
  float spacing = (n && n->letter_spacing_set) ? n->letter_spacing : 0.0f;
  if (!s || len <= 0) return 0;
  if (len > 1023) len = 1023;
  char buf[1024];
  memcpy(buf, s, (size_t)len);
  buf[len] = '\0';
  if (n && n->font_tabular) {
    return cogito_text_width_size_font_spaced_tabular(f, buf, size, spacing);
  }
  return cogito_text_width_size_font_spaced(f, buf, size, spacing);
}

static void cogito_text_set_from_buf(CogitoNode* n, const char* buf, size_t len) {
  if (!n) return;
  ErgoStr* s = stdr_str_from_slice(buf, len);
  cogito_node_set_text(n, s);
  if (s) ergo_release_val(EV_STR(s));
}

// Static buffer to reduce heap allocations in text editing hot path
static char cogito_text_edit_buf[4096];

static void cogito_text_delete_range(CogitoNode* n, size_t start, size_t end) {
  if (!n || !n->text) return;
  if (start > end) { size_t tmp = start; start = end; end = tmp; }
  size_t len = n->text->len;
  if (start > len) start = len;
  if (end > len) end = len;
  if (start == end) return;
  size_t new_len = len - (end - start);
  
  // Use stack buffer for small edits to avoid malloc/free overhead
  char* buf;
  bool use_static = new_len < sizeof(cogito_text_edit_buf);
  if (use_static) {
    buf = cogito_text_edit_buf;
  } else {
    buf = (char*)malloc(new_len + 1);
    if (!buf) return;
  }
  
  if (start > 0) memcpy(buf, n->text->data, start);
  if (end < len) memcpy(buf + start, n->text->data + end, len - end);
  buf[new_len] = '\0';
  cogito_text_set_from_buf(n, buf, new_len);
  if (!use_static) free(buf);
}

static void cogito_text_append_codepoint(CogitoNode* n, int cp) {
  if (!n || cp <= 0) return;
  if (n->text_input.sel_start != n->text_input.sel_end) {
    size_t start = (size_t)n->text_input.sel_start;
    size_t end = (size_t)n->text_input.sel_end;
    cogito_text_delete_range(n, start, end);
    n->text_input.caret = (int)((start < end) ? start : end);
    n->text_input.sel_start = n->text_input.caret;
    n->text_input.sel_end = n->text_input.caret;
  }
  char utf8[5] = {0, 0, 0, 0, 0};
  int ulen = 0;
  if (cp <= 0x7F) {
    utf8[0] = (char)cp;
    ulen = 1;
  } else if (cp <= 0x7FF) {
    utf8[0] = (char)(0xC0 | ((cp >> 6) & 0x1F));
    utf8[1] = (char)(0x80 | (cp & 0x3F));
    ulen = 2;
  } else if (cp <= 0xFFFF) {
    utf8[0] = (char)(0xE0 | ((cp >> 12) & 0x0F));
    utf8[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    utf8[2] = (char)(0x80 | (cp & 0x3F));
    ulen = 3;
  } else {
    utf8[0] = (char)(0xF0 | ((cp >> 18) & 0x07));
    utf8[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    utf8[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    utf8[3] = (char)(0x80 | (cp & 0x3F));
    ulen = 4;
  }
  size_t base_len = n->text ? n->text->len : 0;
  size_t insert_at = (size_t)n->text_input.caret;
  if (insert_at > base_len) insert_at = base_len;
  size_t out_len = base_len + (size_t)ulen;
  
  // Use stack buffer for small edits to avoid malloc/free overhead
  char* buf;
  bool use_static = out_len < sizeof(cogito_text_edit_buf);
  if (use_static) {
    buf = cogito_text_edit_buf;
  } else {
    buf = (char*)malloc(out_len + 1);
    if (!buf) return;
  }
  
  if (insert_at > 0) memcpy(buf, n->text->data, insert_at);
  memcpy(buf + insert_at, utf8, (size_t)ulen);
  if (base_len > insert_at) memcpy(buf + insert_at + (size_t)ulen, n->text->data + insert_at, base_len - insert_at);
  buf[out_len] = '\0';
  cogito_text_set_from_buf(n, buf, out_len);
  if (!use_static) free(buf);
  n->text_input.caret = (int)(insert_at + (size_t)ulen);
  n->text_input.sel_start = n->text_input.caret;
  n->text_input.sel_end = n->text_input.caret;
}

static void cogito_text_backspace(CogitoNode* n) {
  if (!n || !n->text || n->text->len == 0) return;
  if (n->text_input.sel_start != n->text_input.sel_end) {
    size_t start = (size_t)n->text_input.sel_start;
    size_t end = (size_t)n->text_input.sel_end;
    cogito_text_delete_range(n, start, end);
    n->text_input.caret = (int)((start < end) ? start : end);
    n->text_input.sel_start = n->text_input.caret;
    n->text_input.sel_end = n->text_input.caret;
    return;
  }
  size_t i = (size_t)n->text_input.caret;
  if (i == 0) return;
  i--;
  while (i > 0 && ((unsigned char)n->text->data[i] & 0xC0) == 0x80) {
    i--;
  }
  size_t start = i;
  size_t end = (size_t)n->text_input.caret;
  cogito_text_delete_range(n, start, end);
  n->text_input.caret = (int)start;
  n->text_input.sel_start = n->text_input.caret;
  n->text_input.sel_end = n->text_input.caret;
}

static int cogito_text_index_from_x(const CogitoNode* n, const char* text, int size, int target_x) {
  if (!text || target_x <= 0) return 0;
  int len = (int)strlen(text);
  if (len == 0) return 0;
  
  // Binary search for the character position
  int lo = 0, hi = len;
  while (lo < hi) {
    int mid = (lo + hi) / 2;
    int w = cogito_text_width_size_n_node(n, text, mid + 1, size);
    if (w < target_x) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  
  // lo is now the first position where width >= target_x, or len
  if (lo < len && lo > 0) {
    // Check if previous position is closer
    int w_lo = cogito_text_width_size_n_node(n, text, lo + 1, size);
    int w_prev = cogito_text_width_size_n_node(n, text, lo, size);
    if (target_x - w_prev < w_lo - target_x) {
      return lo;
    }
  }
  return lo;
}

static int cogito_text_line_index_from_pos(const CogitoNode* n, const char* text, int size, int max_w, int line_y, int line_h, int x, int y) {
  if (!text) return 0;
  if (max_w <= 0) max_w = 1;
  int len = (int)strlen(text);
  int idx = 0;
  int cur_y = line_y;
  while (idx < len) {
    int line_start = idx;
    int line_len = 0;
    int last_space = -1;
    int cumulative_w = 0;  // Track width incrementally for better performance
    for (int i = idx; i < len; i++) {
      char c = text[i];
      if (c == '\n') {
        line_len = i - idx;
        idx = i + 1;
        break;
      }
      // Get width of just this character
      int char_w = cogito_text_width_size_n_node(n, text + i, 1, size);
      if (c == ' ') last_space = i;
      if (cumulative_w + char_w > max_w) {
        if (last_space >= idx) {
          line_len = last_space - idx;
          idx = last_space + 1;
        } else {
          line_len = i - idx;
          idx = i;
        }
        break;
      }
      cumulative_w += char_w;
      if (i == len - 1) {
        line_len = i - idx + 1;
        idx = len;
      }
    }
    if (line_len < 0) line_len = 0;
    if (y >= cur_y && y <= cur_y + line_h) {
      int local_x = x;
      if (local_x < 0) local_x = 0;
      int caret = cogito_text_index_from_x(n, text + line_start, size, local_x);
      if (caret > line_len) caret = line_len;
      return line_start + caret;
    }
    cur_y += line_h;
  }
  return len;
}

__attribute__((unused))
static int cogito_text_width_size_n(const char* s, int len, int size) {
  return cogito_text_width_size_n_font(cogito_font, s, len, size);
}

// Node-aware text drawing

static int cogito_text_wrap_height(const CogitoNode* n, const char* text, int size, int max_w) {
  if (!text || max_w <= 0) return 0;
  if (size <= 0) size = cogito_font_size();
  int line_h = cogito_text_height_size(size);
  int lines = 0;
  const char* p = text;
  while (*p) {
    while (*p == '\n') { lines++; p++; }
    if (!*p) break;
    int line_len = 0;
    int line_w = 0;
    int last_space = -1;
    int last_space_w = 0;
    const char* start = p;
    while (*p && *p != '\n') {
      int w = cogito_text_width_size_n_node(n, start, line_len + 1, size);
      if (*p == ' ' || *p == '\t') {
        last_space = line_len;
        last_space_w = w;
      }
      if (w > max_w && line_len > 0) {
        if (last_space >= 0) {
          line_len = last_space;
          line_w = last_space_w;
          p = start + line_len + 1;
        } else {
          line_w = cogito_text_width_size_n_node(n, start, line_len, size);
        }
        break;
      }
      line_len++;
      line_w = w;
      p++;
    }
    if (*p == '\n') p++;
    (void)line_w;
    lines++;
  }
  if (lines == 0) lines = 1;
  return lines * line_h;
}

static void cogito_draw_text_tabular(CogitoFont* f, const char* text, int x, int y, int size, CogitoColor color);

static void cogito_draw_text_size_node(const CogitoNode* n, int x, int y, const char* text, CogitoColor color, int size, bool bold) {
  if (!text || !n) return;
  if (size <= 0) size = cogito_font_size();
  CogitoFont* f = cogito_font_for_node(n);
  if (bold && f == cogito_font && cogito_font_bold_ready) {
    f = cogito_font_bold;
  }
  if (n && n->font_tabular) {
    f = cogito_font_tabular_for(f, size);
  }
  f = cogito_font_resolve_size(f, size);
  bool faux_bold = bold && (
    (f == cogito_font && !cogito_font_bold_ready) ||
    (f == cogito_font_bold && cogito_font_bold_variable)
  );
  if (cogito_backend && cogito_backend->draw_text) {
    if (n->font_tabular) {
      cogito_draw_text_tabular(f, text, x, y, size, color);
    } else {
      cogito_backend->draw_text(f, text, x, y, size, color);
    }
    if (faux_bold) {
      CogitoColor c2 = color;
      c2.a = (unsigned char)((float)c2.a * 0.72f);
      if (n->font_tabular) {
        cogito_draw_text_tabular(f, text, x + 1, y, size, c2);
      } else {
        cogito_backend->draw_text(f, text, x + 1, y, size, c2);
      }
    }
  }
}

__attribute__((unused))
static void cogito_draw_text_node(const CogitoNode* n, int x, int y, const char* text, CogitoColor color) {
  if (!text || !n) return;
  int size = cogito_node_font_size(n);
  bool bold = cogito_node_bold(n);
  cogito_draw_text_size_node(n, x, y, text, color, size, bold);
}

static void cogito_draw_text_ellipsis(const CogitoNode* n, int x, int y, int w, const char* text, CogitoColor color, int size, bool bold, int align) {
  if (!text || w <= 0) return;
  if (size <= 0) size = cogito_font_size();
  int len = 0;
  while (text[len]) len++;
  int full_w = cogito_text_width_size_node(n, text, size);
  if (full_w <= w) {
    int tx = x;
    if (align == 1) tx = x + (w - full_w) / 2;
    else if (align == 2) tx = x + (w - full_w);
    cogito_draw_text_size_node(n, tx, y, text, color, size, bold);
    return;
  }
  const char* dots = "...";
  int dots_w = cogito_text_width_size_node(n, dots, size);
  int max_w = w - dots_w;
  if (max_w < 0) max_w = 0;
  
  // Binary search for truncation point - O(log n) instead of O(n²)
  int lo = 0, hi = len, cut = 0;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    int ww = cogito_text_width_size_n_node(n, text, mid, size);
    if (ww <= max_w) {
      cut = mid;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  
  char buf[1024];
  if (cut > 1000) cut = 1000;
  memcpy(buf, text, (size_t)cut);
  buf[cut] = '\0';
  int line_w = cogito_text_width_size_node(n, buf, size) + dots_w;
  int tx = x;
  if (align == 1) tx = x + (w - line_w) / 2;
  else if (align == 2) tx = x + (w - line_w);
  cogito_draw_text_size_node(n, tx, y, buf, color, size, bold);
  cogito_draw_text_size_node(n, tx + cogito_text_width_size_node(n, buf, size), y, dots, color, size, bold);
}

static void cogito_draw_text_tabular(CogitoFont* f, const char* text, int x, int y, int size, CogitoColor color) {
  if (!f || !text || !text[0]) return;
  if (!cogito_backend || !cogito_backend->draw_text) return;

  int digit_adv = cogito_tabular_digit_width(f, size);
  int pen_x = x;
  const char* p = text;
  const char* seg = text;
  while (*p) {
    if (cogito_is_ascii_digit_char(*p)) {
      int seg_len = (int)(p - seg);
      if (seg_len > 0) {
        char stack_buf[1024];
        char* buf = stack_buf;
        bool heap = false;
        if (seg_len >= (int)sizeof(stack_buf)) {
          buf = (char*)malloc((size_t)seg_len + 1);
          if (!buf) return;
          heap = true;
        }
        memcpy(buf, seg, (size_t)seg_len);
        buf[seg_len] = '\0';
        cogito_backend->draw_text(f, buf, pen_x, y, size, color);
        pen_x += cogito_text_width_size_font(f, buf, size);
        if (heap) free(buf);
      }
      while (*p && cogito_is_ascii_digit_char(*p)) {
        char d[2] = {*p, '\0'};
        int dw = cogito_text_width_size_font(f, d, size);
        int dx = pen_x;
        if (digit_adv > dw) dx += (digit_adv - dw) / 2;
        cogito_backend->draw_text(f, d, dx, y, size, color);
        pen_x += digit_adv;
        p++;
      }
      seg = p;
      continue;
    }
    p++;
  }

  int tail_len = (int)(p - seg);
  if (tail_len > 0) {
    char stack_buf[1024];
    char* buf = stack_buf;
    bool heap = false;
    if (tail_len >= (int)sizeof(stack_buf)) {
      buf = (char*)malloc((size_t)tail_len + 1);
      if (!buf) return;
      heap = true;
    }
    memcpy(buf, seg, (size_t)tail_len);
    buf[tail_len] = '\0';
    cogito_backend->draw_text(f, buf, pen_x, y, size, color);
    if (heap) free(buf);
  }
}

static int cogito_draw_text_wrapped(const CogitoNode* n, int x, int y, int w, const char* text, CogitoColor color, int size, bool bold, int align, bool draw) {
  if (!text || w <= 0) return 0;
  if (size <= 0) size = cogito_font_size();
  int line_h = cogito_text_height_size(size);
  int lines = 0;
  const char* p = text;
  while (*p) {
    while (*p == '\n') { if (draw) { lines++; } else { lines++; } p++; }
    if (!*p) break;
    int line_len = 0;
    int last_space = -1;
    int last_space_w = 0;
    const char* start = p;
    int line_w = 0;
    while (*p && *p != '\n') {
      int wcur = cogito_text_width_size_n_node(n, start, line_len + 1, size);
      if (*p == ' ' || *p == '\t') {
        last_space = line_len;
        last_space_w = wcur;
      }
      if (wcur > w && line_len > 0) {
        if (last_space >= 0) {
          line_len = last_space;
          line_w = last_space_w;
          p = start + line_len + 1;
        } else {
          line_w = cogito_text_width_size_n_node(n, start, line_len, size);
        }
        break;
      }
      line_len++;
      line_w = wcur;
      p++;
    }
    if (line_len > 0) {
      int tx = x;
      if (align == 1) tx = x + (w - line_w) / 2;
      else if (align == 2) tx = x + (w - line_w);
      if (draw) {
        char buf[1024];
        int len_n = line_len;
        if (len_n > 1000) len_n = 1000;
        memcpy(buf, start, (size_t)len_n);
        buf[len_n] = '\0';
        cogito_draw_text_size_node(n, tx, y + lines * line_h, buf, color, size, bold);
      }
    }
    if (*p == '\n') p++;
    lines++;
  }
  if (lines == 0) lines = 1;
  return lines * line_h;
}
