// Cogito Text Rendering
// Font loading, measurement, editing, and variable font support via FreeType

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H

#include "widgets/util/text_spans.inc"
#include "widgets/util/text_paragraphs.inc"

static bool cogito_node_bold(const CogitoNode* n);
static bool cogito_node_medium(const CogitoNode* n);

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

static int cogito_utf8_prefix_bytes_for_codepoints(const char* s, int max_chars) {
  if (!s || max_chars <= 0) return 0;
  int count = 0;
  int idx = 0;
  while (s[idx] && count < max_chars) {
    unsigned char ch = (unsigned char)s[idx];
    int step = 1;
    if ((ch & 0x80) == 0x00) step = 1;
    else if ((ch & 0xE0) == 0xC0) step = 2;
    else if ((ch & 0xF0) == 0xE0) step = 3;
    else if ((ch & 0xF8) == 0xF0) step = 4;
    for (int i = 1; i < step && s[idx + i]; i++) {
      unsigned char cc = (unsigned char)s[idx + i];
      if ((cc & 0xC0) != 0x80) {
        step = i;
        break;
      }
    }
    idx += step;
    count++;
  }
  return idx;
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

// Validate font weight - only 400, 500, 700 are allowed
static int cogito_font_weight_validate(int weight) {
  if (weight == 400 || weight == 500 || weight == 700) return weight;
  // Map to nearest allowed weight
  if (weight < 450) return 400;
  if (weight < 600) return 500;
  return 700;
}

static int cogito_font_weight_normal(void) {
  int w = cogito_font_parse_int_env("COGITO_FONT_WEIGHT", 400);
  return cogito_font_weight_validate(w);
}

static int cogito_font_weight_bold(void) {
  int w = cogito_font_parse_int_env("COGITO_FONT_BOLD_WEIGHT", 700);
  return cogito_font_weight_validate(w);
}

static int cogito_font_weight_medium(void) {
  int w = cogito_font_parse_int_env("COGITO_FONT_MEDIUM_WEIGHT", 500);
  return cogito_font_weight_validate(w);
}

__attribute__((unused))
static int cogito_default_codepoints(int* out, int cap) {
  int count = 0;
  if (!out || cap <= 0) return 0;
  for (int cp = 32; cp <= 126 && count < cap; cp++) out[count++] = cp;
  for (int cp = 160; cp <= 255 && count < cap; cp++) out[count++] = cp;
  return count;
}

static uint32_t cogito_font_variation_hash(const CogitoFontVariation* vars, int count) {
  uint32_t h = 0;
  for (int i = 0; i < count; i++) {
    h ^= vars[i].tag;
    h = (h << 5) | (h >> 27);
    uint32_t fv;
    memcpy(&fv, &vars[i].value, sizeof(fv));
    h ^= fv;
    h = (h << 7) | (h >> 25);
  }
  return h;
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

// Find the best named instance matching weight + user variation axes.
// Returns face_index encoding or -1 if no reasonable match found.
static int cogito_find_named_instance_for_variations(const char* path, int target_weight,
    const CogitoFontVariation* vars, int var_count) {
  if (!vars || var_count <= 0) return cogito_find_named_instance_for_weight(path, target_weight);

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
  if (mm->num_namedstyles == 0) {
    FT_Done_MM_Var(lib, mm);
    FT_Done_Face(face);
    return -1;
  }

  // Map axis indices for weight and user variations
  int wght_axis = -1;
  int var_axis[COGITO_MAX_FONT_VARIATIONS];
  for (int v = 0; v < var_count; v++) var_axis[v] = -1;

  for (FT_UInt i = 0; i < mm->num_axis; i++) {
    if (mm->axis[i].tag == FT_MAKE_TAG('w','g','h','t'))
      wght_axis = (int)i;
    for (int v = 0; v < var_count; v++) {
      if (mm->axis[i].tag == (FT_ULong)vars[v].tag)
        var_axis[v] = (int)i;
    }
  }

  FT_Fixed target_w = (FT_Fixed)((long)target_weight << 16);
  int best = -1;
  double best_score = 1e30;

  for (FT_UInt inst = 0; inst < mm->num_namedstyles; inst++) {
    double score = 0;
    // Weight distance (normalized: 1 unit = 1 weight unit)
    if (wght_axis >= 0 && target_weight > 0) {
      double dw = (double)(mm->namedstyle[inst].coords[wght_axis] - target_w) / 65536.0;
      score += dw * dw;
    }
    // User axis distances (normalized: square of difference in axis units)
    for (int v = 0; v < var_count; v++) {
      if (var_axis[v] < 0) continue;
      FT_Fixed inst_val = mm->namedstyle[inst].coords[var_axis[v]];
      double dv = (double)inst_val / 65536.0 - (double)vars[v].value;
      score += dv * dv * 4.0; // weight user axes more
    }
    if (score < best_score) {
      best_score = score;
      best = (int)inst;
    }
  }

  FT_Done_MM_Var(lib, mm);
  FT_Done_Face(face);

  if (best < 0) return -1;
  return (0 | ((best + 1) << 16));
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
static CogitoFont* cogito_font_cache_get_var(CogitoFontCacheEntry* cache, int cap,
    const char* path, int size, int weight,
    const CogitoFontVariation* vars, int var_count);

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

  if (f == cogito_font_medium) {
    if (!cogito_font_medium_from_env || !cogito_font_medium_path_active || !cogito_font_medium_path_active[0]) return f;
    if (cogito_font_medium_loaded_size == size) return f;
    CogitoFont* cached = cogito_font_cache_get(
      cogito_font_medium_cache,
      (int)(sizeof(cogito_font_medium_cache) / sizeof(cogito_font_medium_cache[0])),
      cogito_font_medium_path_active,
      size,
      cogito_font_weight_medium()
    );
    return cached ? cached : f;
  }

  if (f == cogito_font_mono) {
    if (!cogito_font_mono_from_env || !cogito_font_mono_path_active || !cogito_font_mono_path_active[0]) return f;
    if (cogito_font_mono_loaded_size == size) return f;
    CogitoFont* cached = cogito_font_cache_get(
      cogito_font_mono_cache,
      (int)(sizeof(cogito_font_mono_cache) / sizeof(cogito_font_mono_cache[0])),
      cogito_font_mono_path_active,
      size,
      cogito_font_weight_normal()
    );
    return cached ? cached : f;
  }

  return f;
}

static CogitoFont* cogito_font_cache_get(CogitoFontCacheEntry* cache, int cap, const char* path, int size, int weight) {
  if (!cache || cap <= 0 || !path || !path[0] || size <= 0) return NULL;

  for (int i = 0; i < cap; i++) {
    if (cache[i].ready && cache[i].size == size && cache[i].weight == weight &&
        strcmp(cache[i].path, path) == 0) return cache[i].font;
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
    cache[slot].weight = 0;
  }

  if (!cogito_load_font_file_ex(&cache[slot].font, path, size, weight, NULL)) {
    return NULL;
  }

  cache[slot].size = size;
  cache[slot].weight = weight;
  strncpy(cache[slot].path, path, sizeof(cache[slot].path) - 1);
  cache[slot].path[sizeof(cache[slot].path) - 1] = '\0';
  cache[slot].ready = true;
  cache[slot].variation_hash = 0;
  return cache[slot].font;
}

// Extended cache getter that also applies variable-font variation axes.
static CogitoFont* cogito_font_cache_get_var(CogitoFontCacheEntry* cache, int cap,
    const char* path, int size, int weight,
    const CogitoFontVariation* vars, int var_count) {
  if (var_count <= 0 || !vars) return cogito_font_cache_get(cache, cap, path, size, weight);
  if (!cache || cap <= 0 || !path || !path[0] || size <= 0) return NULL;

  uint32_t vhash = cogito_font_variation_hash(vars, var_count);

  for (int i = 0; i < cap; i++) {
    if (cache[i].ready && cache[i].size == size && cache[i].weight == weight &&
        cache[i].variation_hash == vhash && strcmp(cache[i].path, path) == 0)
      return cache[i].font;
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
  }

  // Find best-matching named instance for weight + user variation axes
  if (cogito_backend && cogito_backend->font_load_face) {
    int face_index = cogito_find_named_instance_for_variations(path, weight, vars, var_count);
    if (face_index >= 0) {
      cache[slot].font = cogito_backend->font_load_face(path, size, face_index);
      if (cache[slot].font) {
        cache[slot].size = size;
        cache[slot].weight = weight;
        cache[slot].variation_hash = vhash;
        strncpy(cache[slot].path, path, sizeof(cache[slot].path) - 1);
        cache[slot].path[sizeof(cache[slot].path) - 1] = '\0';
        cache[slot].ready = true;
        return cache[slot].font;
      }
    }
  }

  // Fallback: load with weight only (no user variations applied)
  if (!cogito_load_font_file_ex(&cache[slot].font, path, size, weight, NULL)) {
    return NULL;
  }

  cache[slot].size = size;
  cache[slot].weight = weight;
  cache[slot].variation_hash = vhash;
  strncpy(cache[slot].path, path, sizeof(cache[slot].path) - 1);
  cache[slot].path[sizeof(cache[slot].path) - 1] = '\0';
  cache[slot].ready = true;
  return cache[slot].font;
}

static CogitoFont* cogito_font_tabular_cache_get(const char* path, int size, int weight) {
  if (!path || !path[0] || size <= 0) return NULL;
  int cap = (int)(sizeof(cogito_font_tabular_cache) / sizeof(cogito_font_tabular_cache[0]));

  for (int i = 0; i < cap; i++) {
    if (cogito_font_tabular_cache[i].ready && cogito_font_tabular_cache[i].size == size && cogito_font_tabular_cache[i].weight == weight) {
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
    cogito_font_tabular_cache[slot].weight = 0;
  }

  if (!cogito_load_font_file_tabular(&cogito_font_tabular_cache[slot].font, path, size, weight)) {
    return NULL;
  }

  cogito_font_tabular_cache[slot].size = size;
  cogito_font_tabular_cache[slot].weight = weight;
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
  if (f == cogito_font_medium) {
    if (!cogito_font_medium_from_env || !cogito_font_medium_path_active) return f;
    CogitoFont* cached = cogito_font_tabular_cache_get(cogito_font_medium_path_active, size, cogito_font_weight_medium());
    return cached ? cached : f;
  }
  if (f == cogito_font_mono) {
    if (!cogito_font_mono_from_env || !cogito_font_mono_path_active) return f;
    CogitoFont* cached = cogito_font_tabular_cache_get(cogito_font_mono_path_active, size, cogito_font_weight_normal());
    return cached ? cached : f;
  }
  return f;
}

static bool cogito_node_uses_app_emphasized_font(const CogitoNode* n) {
  if (!n || !n->font_emphasized) return false;
  if (n->font_mono) return false;
  if (n->font_family_set && n->font_family[0]) return false;
  return cogito_font_emphasized_path_active && cogito_font_emphasized_path_active[0];
}

static CogitoFont* cogito_font_variant_for_node(const CogitoNode* n, int size, bool bold, bool medium) {
  if (size <= 0) size = cogito_font_size();

  const CogitoFontVariation* vars = (n && n->font_variation_set && n->font_variation_count > 0)
    ? n->font_variations : NULL;
  int var_count = vars ? n->font_variation_count : 0;

  if (n && n->font_family_set && n->font_family[0] &&
      !cogito_font_family_eq(n->font_family, "monospace") &&
      !cogito_font_family_eq(n->font_family, "mono") &&
      !cogito_font_family_eq(n->font_family, "serif")) {
    const char* path = cogito_resolve_font_by_name(n->font_family);
    if (path && path[0]) {
      int weight = cogito_font_weight_normal();
      if (bold) weight = cogito_font_weight_bold();
      else if (medium) weight = cogito_font_weight_medium();
      CogitoFont* custom = cogito_font_cache_get_var(
        cogito_font_cache,
        (int)(sizeof(cogito_font_cache) / sizeof(cogito_font_cache[0])),
        path,
        size,
        weight,
        vars, var_count);
      if (custom) return custom;
    }
  }

  if (cogito_node_uses_app_emphasized_font(n)) {
    int weight = cogito_font_weight_normal();
    if (bold) weight = cogito_font_weight_bold();
    else if (medium) weight = cogito_font_weight_medium();

    CogitoFont* emphasized = cogito_font_cache_get_var(
      cogito_font_emphasized_cache,
      (int)(sizeof(cogito_font_emphasized_cache) / sizeof(cogito_font_emphasized_cache[0])),
      cogito_font_emphasized_path_active,
      size,
      weight,
      vars, var_count);
    if (emphasized) return emphasized;
  }

  CogitoFont* f = cogito_font_for_node(n);
  if (bold) {
    if (f == cogito_font && cogito_font_bold_ready) {
      f = cogito_font_bold;
    } else if (f == cogito_font_mono &&
               cogito_font_mono_from_env && cogito_font_mono_path_active && cogito_font_mono_path_active[0]) {
      CogitoFont* c = cogito_font_cache_get(
        cogito_font_mono_cache,
        (int)(sizeof(cogito_font_mono_cache) / sizeof(cogito_font_mono_cache[0])),
        cogito_font_mono_path_active, size, cogito_font_weight_bold());
      if (c) f = c;
    } else if (f == cogito_font_serif &&
               cogito_font_serif_from_env && cogito_font_serif_path_active && cogito_font_serif_path_active[0]) {
      CogitoFont* c = cogito_font_cache_get(
        cogito_font_serif_cache,
        (int)(sizeof(cogito_font_serif_cache) / sizeof(cogito_font_serif_cache[0])),
        cogito_font_serif_path_active, size, cogito_font_weight_bold());
      if (c) f = c;
    }
  } else if (medium) {
    if (f == cogito_font && cogito_font_medium_ready) {
      f = cogito_font_medium;
    } else if (f == cogito_font_mono &&
               cogito_font_mono_from_env && cogito_font_mono_path_active && cogito_font_mono_path_active[0]) {
      CogitoFont* c = cogito_font_cache_get(
        cogito_font_mono_cache,
        (int)(sizeof(cogito_font_mono_cache) / sizeof(cogito_font_mono_cache[0])),
        cogito_font_mono_path_active, size, cogito_font_weight_medium());
      if (c) f = c;
    } else if (f == cogito_font_serif &&
               cogito_font_serif_from_env && cogito_font_serif_path_active && cogito_font_serif_path_active[0]) {
      CogitoFont* c = cogito_font_cache_get(
        cogito_font_serif_cache,
        (int)(sizeof(cogito_font_serif_cache) / sizeof(cogito_font_serif_cache[0])),
        cogito_font_serif_path_active, size, cogito_font_weight_medium());
      if (c) f = c;
    }
  }

  if (n && n->font_tabular) {
    f = cogito_font_tabular_for(f, size);
  }

  // Apply variation axes on global fonts by re-loading through variation cache
  if (vars && var_count > 0) {
    const char* vpath = NULL;
    int vweight = cogito_font_weight_normal();
    if (f == cogito_font || f == cogito_font_bold || f == cogito_font_medium) {
      vpath = cogito_font_path_active;
      if (bold || f == cogito_font_bold) vweight = cogito_font_weight_bold();
      else if (medium || f == cogito_font_medium) vweight = cogito_font_weight_medium();
    } else if (f == cogito_font_serif) {
      vpath = cogito_font_serif_path_active;
    } else if (f == cogito_font_mono) {
      vpath = cogito_font_mono_path_active;
    }
    if (vpath && vpath[0]) {
      CogitoFont* var_f = cogito_font_cache_get_var(
        cogito_font_cache,
        (int)(sizeof(cogito_font_cache) / sizeof(cogito_font_cache[0])),
        vpath, size, vweight, vars, var_count);
      if (var_f) f = var_f;
    }
  }

  return cogito_font_resolve_size(f, size);
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
  for (size_t i = 0; i < sizeof(cogito_font_medium_cache) / sizeof(cogito_font_medium_cache[0]); i++) {
    if (cogito_font_medium_cache[i].ready) {
      if (cogito_backend && cogito_backend->font_unload) {
        cogito_backend->font_unload(cogito_font_medium_cache[i].font);
      }
      cogito_font_medium_cache[i].ready = false;
      cogito_font_medium_cache[i].size = 0;
    }
  }
  for (size_t i = 0; i < sizeof(cogito_font_emphasized_cache) / sizeof(cogito_font_emphasized_cache[0]); i++) {
    if (cogito_font_emphasized_cache[i].ready) {
      if (cogito_backend && cogito_backend->font_unload) {
        cogito_backend->font_unload(cogito_font_emphasized_cache[i].font);
      }
      cogito_font_emphasized_cache[i].ready = false;
      cogito_font_emphasized_cache[i].size = 0;
    }
  }
  for (size_t i = 0; i < sizeof(cogito_font_mono_cache) / sizeof(cogito_font_mono_cache[0]); i++) {
    if (cogito_font_mono_cache[i].ready) {
      if (cogito_backend && cogito_backend->font_unload) {
        cogito_backend->font_unload(cogito_font_mono_cache[i].font);
      }
      cogito_font_mono_cache[i].ready = false;
      cogito_font_mono_cache[i].size = 0;
    }
  }
  cogito_text_measure_cache_clear();
}

static void cogito_font_runtime_unload(CogitoFont** font, bool* ready) {
  if (!font || !ready) return;
  if (*font && cogito_backend && cogito_backend->font_unload) {
    cogito_backend->font_unload(*font);
  }
  *font = NULL;
  *ready = false;
}

static void cogito_app_fonts_refresh(void) {
  if (!cogito_backend_ready || !cogito_backend || !cogito_active_app) return;

  const char* font_path = NULL;
  static char app_baseline_font_buf[PATH_MAX];
  const char* baseline_name = cogito_app_get_baseline_font_path(cogito_active_app);
  if (baseline_name && baseline_name[0]) {
    const char* resolved = cogito_resolve_font_by_name(baseline_name);
    if (resolved && resolved[0]) {
      strncpy(app_baseline_font_buf, resolved, PATH_MAX - 1);
      app_baseline_font_buf[PATH_MAX - 1] = '\0';
      font_path = app_baseline_font_buf;
    }
  }
  if (!font_path || !font_path[0]) {
    font_path = getenv("COGITO_FONT");
  }
  if (!font_path || !font_path[0]) {
    font_path = cogito_default_font_path();
  }

  const char* bold_font_path = NULL;
  const char* medium_font_path = NULL;
  static char app_emph_buf[PATH_MAX];
  const char* emph_name = cogito_app_get_emphasized_font_path(cogito_active_app);

  cogito_font_cache_clear();
  cogito_font_runtime_unload(&cogito_font, &cogito_font_ready);
  cogito_font_runtime_unload(&cogito_font_serif, &cogito_font_serif_ready);
  cogito_font_runtime_unload(&cogito_font_mono, &cogito_font_mono_ready);
  cogito_font_runtime_unload(&cogito_font_bold, &cogito_font_bold_ready);
  cogito_font_runtime_unload(&cogito_font_medium, &cogito_font_medium_ready);

  cogito_font_loaded_size = 0;
  cogito_font_serif_loaded_size = 0;
  cogito_font_mono_loaded_size = 0;
  cogito_font_bold_loaded_size = 0;
  cogito_font_medium_loaded_size = 0;
  cogito_font_path_active = NULL;
  cogito_font_serif_path_active = NULL;
  cogito_font_mono_path_active = NULL;
  cogito_font_bold_path_active = NULL;
  cogito_font_medium_path_active = NULL;
  cogito_font_emphasized_path_active = NULL;
  cogito_font_from_env = false;
  cogito_font_serif_from_env = false;
  cogito_font_mono_from_env = false;
  cogito_font_bold_from_env = false;
  cogito_font_medium_from_env = false;
  cogito_font_bold_variable = false;
  cogito_font_medium_variable = false;

  if (emph_name && emph_name[0]) {
    const char* resolved = cogito_resolve_font_by_name(emph_name);
    if (resolved && resolved[0]) {
      strncpy(app_emph_buf, resolved, PATH_MAX - 1);
      app_emph_buf[PATH_MAX - 1] = '\0';
      cogito_font_emphasized_path_active = app_emph_buf;
    }
  }

  if (font_path && font_path[0] &&
      cogito_load_font_file(&cogito_font, font_path, cogito_font_size(),
                            cogito_font_weight_normal())) {
    cogito_font_ready = true;
    cogito_font_loaded_size = cogito_font_size();
    cogito_font_from_env = true;
    cogito_font_path_active = font_path;
  }

  const char* serif_font_path = getenv("COGITO_FONT_SERIF");
  if (!serif_font_path || !serif_font_path[0]) {
    serif_font_path = cogito_guess_serif_font_path(font_path);
  }
  if (serif_font_path && serif_font_path[0]) {
    cogito_font_serif_from_env = true;
    cogito_font_serif_path_active = serif_font_path;
  }

  const char* mono_font_path = getenv("COGITO_FONT_MONO");
  if (!mono_font_path || !mono_font_path[0]) {
    mono_font_path = cogito_guess_mono_font_path(font_path);
  }
  if (mono_font_path && mono_font_path[0]) {
    cogito_font_mono_from_env = true;
    cogito_font_mono_path_active = mono_font_path;
  }

  bool bold_from_regular = false;
  if (!bold_font_path || !bold_font_path[0]) {
    bold_font_path = getenv("COGITO_FONT_BOLD");
  }
  if (!bold_font_path || !bold_font_path[0]) {
    bold_font_path = cogito_guess_bold_font_path(font_path);
    if ((!bold_font_path || !bold_font_path[0]) && font_path && font_path[0]) {
      bold_font_path = font_path;
      bold_from_regular = true;
    }
  }
  if (bold_font_path && bold_font_path[0]) {
    bool weight_applied = false;
    int bold_weight = bold_from_regular ? cogito_font_weight_bold() : 0;
    if (cogito_load_font_file_ex(&cogito_font_bold, bold_font_path,
                                 cogito_font_size(), bold_weight,
                                 &weight_applied)) {
      cogito_font_bold_ready = true;
      cogito_font_bold_loaded_size = cogito_font_size();
      cogito_font_bold_from_env = true;
      cogito_font_bold_path_active = bold_font_path;
      cogito_font_bold_variable = (bold_from_regular && !weight_applied);
    }
  }

  bool medium_from_regular = false;
  if (!medium_font_path || !medium_font_path[0]) {
    medium_font_path = getenv("COGITO_FONT_MEDIUM");
  }
  if (!medium_font_path || !medium_font_path[0]) {
    medium_font_path = cogito_guess_medium_font_path(font_path);
    if ((!medium_font_path || !medium_font_path[0]) && font_path && font_path[0]) {
      medium_font_path = font_path;
      medium_from_regular = true;
    }
  }
  if (medium_font_path && medium_font_path[0]) {
    bool weight_applied = false;
    int medium_weight = medium_from_regular ? cogito_font_weight_medium() : 0;
    if (cogito_load_font_file_ex(&cogito_font_medium, medium_font_path,
                                 cogito_font_size(), medium_weight,
                                 &weight_applied)) {
      cogito_font_medium_ready = true;
      cogito_font_medium_loaded_size = cogito_font_size();
      cogito_font_medium_from_env = true;
      cogito_font_medium_path_active = medium_font_path;
      cogito_font_medium_variable = (medium_from_regular && !weight_applied);
    }
  }

  if (cogito_active_window) {
    cogito_window_relayout(cogito_active_window);
  }
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

static void cogito_node_set_text(CogitoNode* n, YisStr* s);

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
  uint8_t style; // font style flags (italic, underline, etc.)
  uint8_t len;
  char text[COGITO_TEXT_MEASURE_KEY_MAX];
  int width;
} CogitoTextMeasureCacheEntry;

static CogitoTextMeasureCacheEntry cogito_text_measure_cache[COGITO_TEXT_MEASURE_CACHE_SIZE];

static void cogito_text_measure_cache_clear(void) {
  memset(cogito_text_measure_cache, 0, sizeof(cogito_text_measure_cache));
}

static uint32_t cogito_text_measure_hash(CogitoFont* f, const char* s, int len,
                                         int size, int spacing_q64, uint8_t mode,
                                         uint8_t style) {
  uint32_t h = 2166136261u;
  h ^= (uint32_t)(uintptr_t)f; h *= 16777619u;
  h ^= (uint32_t)size; h *= 16777619u;
  h ^= (uint32_t)(spacing_q64 + 32768); h *= 16777619u;
  h ^= (uint32_t)mode; h *= 16777619u;
  h ^= (uint32_t)style; h *= 16777619u;
  h ^= (uint32_t)len; h *= 16777619u;
  for (int i = 0; i < len; i++) {
    h ^= (uint8_t)s[i];
    h *= 16777619u;
  }
  return h;
}

static bool cogito_text_measure_cache_lookup(CogitoFont* f, const char* s, int size,
                                             float spacing, uint8_t mode,
                                             uint8_t style, int* out_w) {
  if (!s || !out_w) return false;
  int len = (int)strnlen(s, COGITO_TEXT_MEASURE_KEY_MAX);
  if (len <= 0 || len >= COGITO_TEXT_MEASURE_KEY_MAX) return false;
  int spacing_q64 = (int)lroundf(spacing * 64.0f);
  uint32_t h = cogito_text_measure_hash(f, s, len, size, spacing_q64, mode, style);
  CogitoTextMeasureCacheEntry* e = &cogito_text_measure_cache[h % COGITO_TEXT_MEASURE_CACHE_SIZE];
  if (!e->valid || e->hash != h || e->font != f || e->size != size ||
      e->spacing_q64 != spacing_q64 || e->mode != mode || e->style != style ||
      e->len != (uint8_t)len ||
      memcmp(e->text, s, (size_t)len) != 0) {
    return false;
  }
  *out_w = e->width;
  return true;
}

static void cogito_text_measure_cache_store(CogitoFont* f, const char* s, int size,
                                            float spacing, uint8_t mode,
                                            uint8_t style, int width) {
  if (!s) return;
  int len = (int)strnlen(s, COGITO_TEXT_MEASURE_KEY_MAX);
  if (len <= 0 || len >= COGITO_TEXT_MEASURE_KEY_MAX) return;
  int spacing_q64 = (int)lroundf(spacing * 64.0f);
  uint32_t h = cogito_text_measure_hash(f, s, len, size, spacing_q64, mode, style);
  CogitoTextMeasureCacheEntry* e = &cogito_text_measure_cache[h % COGITO_TEXT_MEASURE_CACHE_SIZE];
  e->valid = true;
  e->hash = h;
  e->font = f;
  e->size = size;
  e->spacing_q64 = spacing_q64;
  e->mode = mode;
  e->style = style;
  e->len = (uint8_t)len;
  memcpy(e->text, s, (size_t)len);
  e->text[len] = '\0';
  e->width = width;
}

// Forward declaration — defined below cogito_text_width_size_n
static int cogito_text_width_size_n_font(CogitoFont* f, const char* s, int len, int size);

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

static int cogito_text_width_size_font_spaced(CogitoFont* f, const char* s, int size, float spacing, uint8_t style) {
  if (!s) return 0;
  if (size <= 0) size = cogito_font_size();
  f = cogito_font_resolve_size(f, size);

  int cached_w = 0;
  if (cogito_text_measure_cache_lookup(f, s, size, spacing, 0, style, &cached_w)) {
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

  cogito_text_measure_cache_store(f, s, size, spacing, 0, style, base);
  return base;
}

static int cogito_text_width_size_font_spaced_tabular(CogitoFont* f, const char* s, int size, float spacing) {
  if (!s) return 0;
  if (size <= 0) size = cogito_font_size();
  f = cogito_font_resolve_size(f, size);

  int cached_w = 0;
  if (cogito_text_measure_cache_lookup(f, s, size, spacing, 1, 0, &cached_w)) {
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
        total += cogito_text_width_size_n_font(f, seg, seg_len, size);
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
    total += cogito_text_width_size_n_font(f, seg, tail_len, size);
  }

  if (spacing != 0.0f) {
    int cp_count = cogito_utf8_codepoint_count(s);
    if (cp_count > 1) {
      total += (int)(spacing * (float)(cp_count - 1));
    }
  }

  cogito_text_measure_cache_store(f, s, size, spacing, 1, 0, total);
  return total;
}

static int cogito_text_width_size_font(CogitoFont* f, const char* s, int size) {
  return cogito_text_width_size_font_spaced(f, s, size, 0.0f, 0);
}

static int cogito_text_width_size_node(const CogitoNode* n, const char* s, int size) {
  bool bold = n && cogito_node_bold(n);
  bool medium = n && cogito_node_medium(n);
  CogitoFont* f = cogito_font_variant_for_node(n, size, bold, medium);
  float spacing = (n && n->letter_spacing_set) ? n->letter_spacing : 0.0f;
  int style_flags = COGITO_FONT_STYLE_NORMAL;
  if (cogito_node_italic(n)) style_flags |= COGITO_FONT_STYLE_ITALIC;
  if (cogito_node_underline(n)) style_flags |= COGITO_FONT_STYLE_UNDERLINE;
  if (cogito_node_strikethrough(n)) style_flags |= COGITO_FONT_STYLE_STRIKETHROUGH;
  if (style_flags && cogito_backend && cogito_backend->font_set_style) {
    cogito_backend->font_set_style(f, style_flags);
  }
  int w;
  if (n && n->font_tabular) {
    w = cogito_text_width_size_font_spaced_tabular(f, s, size, spacing);
  } else {
    w = cogito_text_width_size_font_spaced(f, s, size, spacing, (uint8_t)style_flags);
  }
  if (style_flags && cogito_backend && cogito_backend->font_set_style) {
    cogito_backend->font_set_style(f, COGITO_FONT_STYLE_NORMAL);
  }
  return w;
}

static int cogito_text_width_size_n(const char* s, int len, int size) {
  if (!s || len <= 0) return 0;
  if (len > 1023) len = 1023;
  char buf[1024];
  memcpy(buf, s, (size_t)len);
  buf[len] = '\0';
  return cogito_text_width_size(buf, size);
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
  bool bold = n && cogito_node_bold(n);
  bool medium = n && cogito_node_medium(n);
  CogitoFont* f = cogito_font_variant_for_node(n, size, bold, medium);
  float spacing = (n && n->letter_spacing_set) ? n->letter_spacing : 0.0f;
  if (!s || len <= 0) return 0;
  int style_flags = COGITO_FONT_STYLE_NORMAL;
  if (cogito_node_italic(n)) style_flags |= COGITO_FONT_STYLE_ITALIC;
  if (cogito_node_underline(n)) style_flags |= COGITO_FONT_STYLE_UNDERLINE;
  if (cogito_node_strikethrough(n)) style_flags |= COGITO_FONT_STYLE_STRIKETHROUGH;
  if (style_flags && cogito_backend && cogito_backend->font_set_style) {
    cogito_backend->font_set_style(f, style_flags);
  }
  if (len > 1023) len = 1023;
  char buf[1024];
  memcpy(buf, s, (size_t)len);
  buf[len] = '\0';
  int w;
  if (n && n->font_tabular) {
    w = cogito_text_width_size_font_spaced_tabular(f, buf, size, spacing);
  } else {
    w = cogito_text_width_size_font_spaced(f, buf, size, spacing, (uint8_t)style_flags);
  }
  if (style_flags && cogito_backend && cogito_backend->font_set_style) {
    cogito_backend->font_set_style(f, COGITO_FONT_STYLE_NORMAL);
  }
  return w;
}

static void cogito_text_set_from_buf(CogitoNode* n, const char* buf, size_t len) {
  if (!n) return;
  YisStr* s = stdr_str_from_slice(buf, len);
  cogito_node_set_text(n, s);
  if (s) yis_release_val(YV_STR(s));
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
  // Adjust span positions for the deleted range
  cogito_spans_shift_delete(n, (int)start, (int)end);
}

static void cogito_text_append_codepoint(CogitoNode* n, int cp) {
  if (!n || cp <= 0) return;
  int* caret = &n->kd->text_input.caret;
  int* sel_start = &n->kd->text_input.sel_start;
  int* sel_end = &n->kd->text_input.sel_end;
  if (n->kind == COGITO_COLORPICKER) {
    caret = &n->kd->colorpicker.caret;
    sel_start = &n->kd->colorpicker.sel_start;
    sel_end = &n->kd->colorpicker.sel_end;
  }
  if (*sel_start != *sel_end) {
    size_t start = (size_t)*sel_start;
    size_t end = (size_t)*sel_end;
    cogito_text_delete_range(n, start, end);
    *caret = (int)((start < end) ? start : end);
    *sel_start = *caret;
    *sel_end = *caret;
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
  size_t insert_at = (size_t)*caret;
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
  // Adjust span positions for the inserted bytes.
  // If the cursor sits right at the end of a span (not inside it),
  // inherit that span's style by extending it to cover the new bytes.
  if (n->span_count > 0) {
    CogitoTextSpan probe = cogito_spans_at(n, (int)insert_at);
    if (!probe.bold_set && !probe.italic_set && !probe.underline_set &&
        !probe.strikethrough_set && !probe.color_set && !probe.size_set &&
        !probe.family_set) {
      // Not inside any span — check the span ending right at the cursor
      for (int si = 0; si < n->span_count; si++) {
        if (n->spans[si].end == (int)insert_at) {
          n->spans[si].end += ulen;
          break;
        }
      }
    }
  }
  cogito_spans_shift_insert(n, (int)insert_at, ulen);
  *caret = (int)(insert_at + (size_t)ulen);
  *sel_start = *caret;
  *sel_end = *caret;
}

static void cogito_text_backspace(CogitoNode* n) {
  if (!n || !n->text || n->text->len == 0) return;
  int* caret = &n->kd->text_input.caret;
  int* sel_start = &n->kd->text_input.sel_start;
  int* sel_end = &n->kd->text_input.sel_end;
  if (n->kind == COGITO_COLORPICKER) {
    caret = &n->kd->colorpicker.caret;
    sel_start = &n->kd->colorpicker.sel_start;
    sel_end = &n->kd->colorpicker.sel_end;
  }
  if (*sel_start != *sel_end) {
    size_t start = (size_t)*sel_start;
    size_t end = (size_t)*sel_end;
    cogito_text_delete_range(n, start, end);
    *caret = (int)((start < end) ? start : end);
    *sel_start = *caret;
    *sel_end = *caret;
    return;
  }
  size_t i = (size_t)*caret;
  if (i == 0) return;
  i--;
  while (i > 0 && ((unsigned char)n->text->data[i] & 0xC0) == 0x80) {
    i--;
  }
  size_t start = i;
  size_t end = (size_t)*caret;
  cogito_text_delete_range(n, start, end);
  *caret = (int)start;
  *sel_start = *caret;
  *sel_end = *caret;
}

static int cogito_line_width_with_spans(const CogitoNode* n,
                                        const char* full_text,
                                        int line_start, int line_len,
                                        int base_size);

// Compute the line height for a wrapped line, accounting for spans.
// Returns the tallest of the base font height and any span's font height
// that overlaps the given character range [line_start, line_start+line_len).
static int cogito_line_height_with_spans(const CogitoNode* n,
                                          int line_start, int line_len,
                                          int base_size) {
  int base_h = cogito_text_height_size(base_size);
  if (!n || n->span_count <= 0 || line_len <= 0) return base_h;
  int line_end = line_start + line_len;
  int max_h = base_h;
  for (int i = 0; i < n->span_count; i++) {
    const CogitoTextSpan* s = &n->spans[i];
    if (s->start >= line_end) break;
    if (s->end <= line_start) continue;
    if (s->size_set && s->size > 0) {
      int sh = cogito_text_height_size(s->size);
      if (sh > max_h) max_h = sh;
    }
  }
  return max_h;
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

// Span-aware version: converts a pixel x-offset into a character index
// within a line starting at line_start in full_text.
static int cogito_text_index_from_x_spans(const CogitoNode* n,
                                           const char* full_text,
                                           int line_start, int line_len,
                                           int base_size, int target_x) {
  if (!full_text || target_x <= 0 || line_len <= 0) return 0;
  if (n->span_count == 0)
    return cogito_text_index_from_x(n, full_text + line_start, base_size, target_x);

  // Linear scan: measure cumulative width char-by-char using span metrics.
  int prev_w = 0;
  for (int i = 1; i <= line_len; i++) {
    int w = cogito_line_width_with_spans(n, full_text, line_start, i, base_size);
    if (w >= target_x) {
      // Pick whichever side of this character the click is closer to.
      return (target_x - prev_w < w - target_x) ? i - 1 : i;
    }
    prev_w = w;
  }
  return line_len;
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
    int cumulative_w = 0;
    // Resolve per-paragraph line height, fall back to caller-provided line_h
    bool lh_set = false;
    float lh_mul = cogito_get_para_line_height(n, idx, &lh_set);
    int base_h = cogito_text_height_size(size);
    int this_line_h = lh_set && lh_mul > 0.0f ? (int)lroundf(base_h * lh_mul) : line_h;
    for (int i = idx; i < len; i++) {
      char c = text[i];
      if (c == '\n') {
        line_len = i - idx;
        idx = i + 1;
        break;
      }
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
    if (y >= cur_y && y <= cur_y + this_line_h) {
      int line_w = cogito_text_width_size_n_node(n, text + line_start, line_len, size);
      int para_align = cogito_get_para_align(n, line_start);
      int align_off = 0;
      if (para_align == 1) align_off = (max_w - line_w) / 2;
      else if (para_align == 2) align_off = max_w - line_w;
      if (align_off < 0) align_off = 0;
      int local_x = x - align_off;
      if (local_x < 0) local_x = 0;
      int caret = cogito_text_index_from_x(n, text + line_start, size, local_x);
      if (caret > line_len) caret = line_len;
      return line_start + caret;
    }
    cur_y += this_line_h;
  }
  return len;
}

// Span-aware version: converts (x, y) to a character index in multi-line
// wrapped text, using per-span font metrics for accurate hit-testing.
// Word-wrap decisions use the base size to match cogito_draw_text_wrapped_spans.
static int cogito_text_line_index_from_pos_spans(const CogitoNode* n,
                                                  const char* text, int size,
                                                  int max_w, int line_y,
                                                  int line_h, int x, int y) {
  if (!text) return 0;
  (void)line_h;
  if (n->span_count == 0)
    return cogito_text_line_index_from_pos(n, text, size, max_w, line_y, line_h, x, y);
  if (max_w <= 0) max_w = 1;
  int len = (int)strlen(text);
  int idx = 0;
  int cur_y = line_y;
  while (idx < len) {
    int line_start = idx;
    int line_len = 0;
    int last_space = -1;
    for (int i = idx; i < len; i++) {
      char c = text[i];
      if (c == '\n') {
        line_len = i - idx;
        idx = i + 1;
        break;
      }
      int w = cogito_line_width_with_spans(n, text, idx, i - idx + 1, size);
      if (c == ' ') last_space = i;
      if (w > max_w && i > idx) {
        if (last_space >= idx) {
          line_len = last_space - idx;
          idx = last_space + 1;
        } else {
          line_len = i - idx;
          idx = i;
        }
        break;
      }
      if (i == len - 1) {
        line_len = i - idx + 1;
        idx = len;
      }
    }
    if (line_len < 0) line_len = 0;
    // Per-line height from tallest span on this line.
    int this_h = cogito_line_height_with_spans(n, line_start, line_len, size);
    int adv = this_h;
    {
      bool lh_set = false;
      float lh = cogito_get_para_line_height(n, line_start, &lh_set);
      if (lh_set && lh > 0.0f)
        adv = (int)lroundf(this_h * lh);
    }
    if (y >= cur_y && y <= cur_y + adv) {
      int line_w = cogito_line_width_with_spans(n, text, line_start, line_len, size);
      int para_align = cogito_get_para_align(n, line_start);
      int align_off = 0;
      if (para_align == 1) align_off = (max_w - line_w) / 2;
      else if (para_align == 2) align_off = max_w - line_w;
      if (align_off < 0) align_off = 0;
      int local_x = x - align_off;
      if (local_x < 0) local_x = 0;
      int caret = cogito_text_index_from_x_spans(n, text, line_start,
                                                   line_len, size, local_x);
      if (caret > line_len) caret = line_len;
      return line_start + caret;
    }
    cur_y += adv;
  }
  return len;
}

static int cogito_text_wrap_height(const CogitoNode* n, const char* text, int size, int w) {
  if (!text || w <= 0) return 0;
  if (size <= 0) size = cogito_font_size();

  const bool has_spans = (n && n->span_count > 0);
  int base_line_h = cogito_text_height_size(size);
  int base_adv = base_line_h;
  if (n) {
    bool lh_set = false;
    float lh = cogito_get_para_line_height(n, 0, &lh_set);
    if (lh_set && lh > 0.0f)
      base_adv = (int)lroundf(base_line_h * lh);
  }

  int lines = 0;
  int total_h = 0;
  const char* p = text;
  int text_offset = 0;
  while (*p) {
    while (*p == '\n') {
      lines++;
      bool lh_set = false;
      float lh = n ? cogito_get_para_line_height(n, text_offset, &lh_set) : 0.0f;
      int empty_h = base_line_h;
      if (lh_set && lh > 0.0f) empty_h = (int)lroundf(base_line_h * lh);
      total_h += empty_h;
      p++;
      text_offset++;
    }
    if (!*p) break;

    const char* start = p;
    int start_offset = text_offset;
    int line_len = 0;
    int last_space = -1;

    while (*p && *p != '\n') {
      int line_w = has_spans
        ? cogito_line_width_with_spans(n, text, start_offset, line_len + 1, size)
        : (n
            ? cogito_text_width_size_n_node(n, start, line_len + 1, size)
            : cogito_text_width_size_n(start, line_len + 1, size));

      if (*p == ' ' || *p == '\t') last_space = line_len;
      if (line_w > w && line_len > 0) {
        if (last_space >= 0) {
          line_len = last_space;
          p = start + line_len + 1;
          text_offset = start_offset + line_len + 1;
        } else {
          p = start + line_len;
          text_offset = start_offset + line_len;
        }
        break;
      }
      line_len++;
      p++;
      text_offset++;
    }

    int line_h = has_spans
      ? cogito_line_height_with_spans(n, start_offset, line_len, size)
      : base_line_h;
    {
      bool lh_set = false;
      float lh = n ? cogito_get_para_line_height(n, start_offset, &lh_set) : 0.0f;
      if (lh_set && lh > 0.0f)
        line_h = (int)lroundf(line_h * lh);
    }
    if (line_h <= 0) line_h = base_adv;
    total_h += line_h;

    if (*p == '\n') {
      p++;
      text_offset++;
    }
    lines++;
  }

  if (lines == 0) return base_adv;
  return total_h;
}

static void cogito_draw_text_tabular(CogitoFont* f, const char* text, int x, int y, int size, CogitoColor color);

// Draw a single text run with explicit direction (used by bidi pipeline)
static void cogito_draw_font_run(CogitoFont* f, const char* text, int len, int x, int y, int size, CogitoColor color, bool rtl) {
  if (!f || !text || len <= 0) return;
  if (!cogito_backend || !cogito_backend->draw_text_dir) return;
  char buf[1024];
  char* s = buf;
  bool heap = false;
  if (len >= 1024) {
    s = (char*)malloc((size_t)len + 1);
    if (!s) return;
    heap = true;
  }
  memcpy(s, text, (size_t)len);
  s[len] = '\0';
  cogito_backend->draw_text_dir(f, s, x, y, size, color, rtl);
  if (heap) free(s);
}

// Measure a single text run with explicit direction
static int cogito_measure_font_run(CogitoFont* f, const char* text, int len, int size, bool rtl) {
  if (!f || !text || len <= 0) return 0;
  if (!cogito_backend || !cogito_backend->text_measure_width_dir) return 0;
  char buf[1024];
  char* s = buf;
  bool heap = false;
  if (len >= 1024) {
    s = (char*)malloc((size_t)len + 1);
    if (!s) return 0;
    heap = true;
  }
  memcpy(s, text, (size_t)len);
  s[len] = '\0';
  int w = cogito_backend->text_measure_width_dir(f, s, size, rtl);
  if (heap) free(s);
  return w;
}

static void cogito_draw_text_size_node(const CogitoNode* n, int x, int y, const char* text, CogitoColor color, int size, bool bold, bool medium) {
  if (!text || !n) return;
  if (size <= 0) size = cogito_font_size();
  CogitoFont* f = cogito_font_variant_for_node(n, size, bold, medium);
  bool faux_bold = bold && (
    (f == cogito_font && !cogito_font_bold_ready) ||
    (f == cogito_font_bold && cogito_font_bold_variable)
  );
  bool faux_medium = medium && (
    (f == cogito_font && !cogito_font_medium_ready) ||
    (f == cogito_font_medium && cogito_font_medium_variable)
  );
  // Apply italic / underline / strikethrough font style flags
  int style_flags = COGITO_FONT_STYLE_NORMAL;
  if (cogito_node_italic(n)) style_flags |= COGITO_FONT_STYLE_ITALIC;
  if (cogito_node_underline(n)) style_flags |= COGITO_FONT_STYLE_UNDERLINE;
  if (cogito_node_strikethrough(n)) style_flags |= COGITO_FONT_STYLE_STRIKETHROUGH;
  if (style_flags && cogito_backend->font_set_style) {
    cogito_backend->font_set_style(f, style_flags);
  }
  if (cogito_backend && cogito_backend->draw_text) {
    // Check if text needs bidi processing (mixed directional text)
    bool needs_bidi = cogito_text_needs_bidi(text);

    if (needs_bidi && !n->font_tabular && cogito_backend->draw_text_dir) {
      int text_len = 0;
      while (text[text_len]) text_len++;
      CogitoBidiRun runs[COGITO_BIDI_MAX_RUNS];
      int nruns = cogito_bidi_get_runs(text, text_len, runs, COGITO_BIDI_MAX_RUNS);
      if (nruns > 1) {
        int pen_x = x;
        for (int r = 0; r < nruns; r++) {
          cogito_draw_font_run(f, text + runs[r].byte_offset, runs[r].byte_len,
                               pen_x, y, size, color, runs[r].rtl);
          int run_w = cogito_measure_font_run(f, text + runs[r].byte_offset,
                                              runs[r].byte_len, size, runs[r].rtl);
          if (faux_bold || faux_medium) {
            CogitoColor c2 = color;
            c2.a = (unsigned char)((float)c2.a * 0.72f);
            cogito_draw_font_run(f, text + runs[r].byte_offset, runs[r].byte_len,
                                 pen_x + 1, y, size, c2, runs[r].rtl);
          }
          pen_x += run_w;
        }
        if (style_flags && cogito_backend->font_set_style) {
          cogito_backend->font_set_style(f, COGITO_FONT_STYLE_NORMAL);
        }
        return;
      }
    }

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
    if (faux_medium) {
      CogitoColor c2 = color;
      c2.a = (unsigned char)((float)c2.a * 0.72f);
      if (n->font_tabular) {
        cogito_draw_text_tabular(f, text, x + 1, y, size, c2);
      } else {
        cogito_backend->draw_text(f, text, x + 1, y, size, c2);
      }
    }
  }
  // Restore normal font style
  if (style_flags && cogito_backend->font_set_style) {
    cogito_backend->font_set_style(f, COGITO_FONT_STYLE_NORMAL);
  }
}

__attribute__((unused))
static void cogito_draw_text_node(const CogitoNode* n, int x, int y, const char* text, CogitoColor color) {
  if (!text || !n) return;
  int size = cogito_node_font_size(n);
  bool bold = cogito_node_bold(n);
  cogito_draw_text_size_node(n, x, y, text, color, size, bold, false);
}

static void cogito_draw_text_ellipsis(const CogitoNode* n, int x, int y, int w, const char* text, CogitoColor color, int size, bool bold, bool medium, int align) {
  if (!text || w <= 0) return;
  if (size <= 0) size = cogito_font_size();
  int len = 0;
  while (text[len]) len++;
  int full_w = cogito_text_width_size_node(n, text, size);
  if (full_w <= w) {
    int tx = x;
    if (align == 1) tx = x + (w - full_w) / 2;
    else if (align == 2) tx = x + (w - full_w);
    cogito_draw_text_size_node(n, tx, y, text, color, size, bold, medium);
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
  if (cogito_is_rtl()) {
    // RTL: ellipsis on left (visual start), text on right
    cogito_draw_text_size_node(n, tx, y, dots, color, size, bold, medium);
    cogito_draw_text_size_node(n, tx + dots_w, y, buf, color, size, bold, medium);
  } else {
    cogito_draw_text_size_node(n, tx, y, buf, color, size, bold, medium);
    cogito_draw_text_size_node(n, tx + cogito_text_width_size_node(n, buf, size), y, dots, color, size, bold, medium);
  }
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

static int cogito_draw_text_wrapped(const CogitoNode* n, int x, int y, int w, const char* text, CogitoColor color, int size, bool bold, bool medium, int align, bool draw, int max_lines) {
  if (!text || w <= 0) return 0;
  if (size <= 0) size = cogito_font_size();
  int base_line_h = cogito_text_height_size(size);
  int lines = 0;
  int total_h = 0;
  const char* p = text;
  int text_offset = 0;
  while (*p) {
    if (max_lines > 0 && lines >= max_lines) break;
    while (*p == '\n') {
      bool lh_set = false;
      float lh = n ? cogito_get_para_line_height(n, text_offset, &lh_set) : 0.0f;
      int empty_h = base_line_h;
      if (lh_set && lh > 0.0f) empty_h = (int)lroundf(base_line_h * lh);
      total_h += empty_h;
      lines++; p++; text_offset++;
      if (max_lines > 0 && lines >= max_lines) goto wrapped_done;
    }
    if (!*p) break;

    // Resolve per-paragraph properties for this line
    int para_align = n ? cogito_get_para_align(n, text_offset) : align;
    bool lh_set = false;
    float lh_mul = n ? cogito_get_para_line_height(n, text_offset, &lh_set) : 0.0f;
    int line_h = base_line_h;
    if (lh_set && lh_mul > 0.0f) line_h = (int)lroundf(base_line_h * lh_mul);

    bool is_last_line = (max_lines > 0 && lines == max_lines - 1);
    const char* start = p;
    int start_offset = text_offset;
    if (is_last_line) {
      int para_len = 0;
      while (start[para_len] && start[para_len] != '\n') para_len++;
      int para_w = (para_len > 0) ? cogito_text_width_size_n_node(n, start, para_len, size) : 0;
      bool has_more = (start[para_len] != '\0');
      if (para_w > w || has_more) {
        if (draw && para_len > 0) {
          char buf[1024];
          int len_n = para_len > 1000 ? 1000 : para_len;
          memcpy(buf, start, (size_t)len_n);
          buf[len_n] = '\0';
          cogito_draw_text_ellipsis(n, x, y + total_h, w, buf, color, size, bold, medium, para_align);
        }
      } else if (para_len > 0) {
        int tx = x;
        if (para_align == 1) tx = x + (w - para_w) / 2;
        else if (para_align == 2) tx = x + (w - para_w);
        if (draw) {
          char buf[1024];
          int len_n = para_len > 1000 ? 1000 : para_len;
          memcpy(buf, start, (size_t)len_n);
          buf[len_n] = '\0';
          cogito_draw_text_size_node(n, tx, y + total_h, buf, color, size, bold, medium);
        }
      }
      total_h += line_h;
      lines++;
      break;
    }
    int line_len = 0;
    int last_space = -1;
    int last_space_w = 0;
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
          text_offset = start_offset + line_len + 1;
        } else {
          line_w = cogito_text_width_size_n_node(n, start, line_len, size);
        }
        break;
      }
      line_len++;
      line_w = wcur;
      p++;
      text_offset++;
    }
    if (line_len > 0) {
      int tx = x;
      if (para_align == 1) tx = x + (w - line_w) / 2;
      else if (para_align == 2) tx = x + (w - line_w);
      if (draw) {
        char buf[1024];
        int len_n = line_len;
        if (len_n > 1000) len_n = 1000;
        memcpy(buf, start, (size_t)len_n);
        buf[len_n] = '\0';
        cogito_draw_text_size_node(n, tx, y + total_h, buf, color, size, bold, medium);
      }
    }
    total_h += line_h;
    if (*p == '\n') { p++; text_offset++; }
    lines++;
  }
  wrapped_done:
  if (lines == 0) { lines = 1; total_h = base_line_h; }
  return total_h;
}

// ── Span-aware text drawing ─────────────────────────────────────
// Draws a single text segment with style resolved from a span + node defaults.
static void cogito_draw_span_segment(const CogitoNode* n, int x, int y,
                                     const char* text, int text_len,
                                     CogitoColor color, int size,
                                     const CogitoTextSpan* span) {
  if (!text || text_len <= 0 || !n) return;
  if (size <= 0) size = cogito_font_size();

  // Resolve bold/medium from span or node defaults
  bool bold = span && span->bold_set ? span->bold : cogito_node_bold(n);
  bool medium = cogito_node_medium(n);

  // Resolve font family: if span overrides family, we need a custom font lookup
  CogitoFont* f;
  if (span && span->family_set && span->family[0]) {
    const char* path = cogito_resolve_font_by_name(span->family);
    if (path && path[0]) {
      int weight = cogito_font_weight_normal();
      if (bold) weight = cogito_font_weight_bold();
      else if (medium) weight = cogito_font_weight_medium();
      f = cogito_font_cache_get_var(
        cogito_font_cache,
        (int)(sizeof(cogito_font_cache) / sizeof(cogito_font_cache[0])),
        path, size, weight, NULL, 0);
      if (!f) f = cogito_font_variant_for_node(n, size, bold, medium);
    } else {
      f = cogito_font_variant_for_node(n, size, bold, medium);
    }
  } else {
    f = cogito_font_variant_for_node(n, size, bold, medium);
  }

  bool faux_bold = bold && (
    (f == cogito_font && !cogito_font_bold_ready) ||
    (f == cogito_font_bold && cogito_font_bold_variable)
  );

  // Resolve style flags from span or node defaults
  int style_flags = COGITO_FONT_STYLE_NORMAL;
  bool is_italic = span && span->italic_set ? span->italic : cogito_node_italic(n);
  bool is_underline = span && span->underline_set ? span->underline : cogito_node_underline(n);
  bool is_strike = span && span->strikethrough_set ? span->strikethrough : cogito_node_strikethrough(n);
  if (is_italic) style_flags |= COGITO_FONT_STYLE_ITALIC;
  if (is_underline) style_flags |= COGITO_FONT_STYLE_UNDERLINE;
  if (is_strike) style_flags |= COGITO_FONT_STYLE_STRIKETHROUGH;

  if (style_flags && cogito_backend && cogito_backend->font_set_style)
    cogito_backend->font_set_style(f, style_flags);

  // Resolve color from span or use default
  CogitoColor draw_color = (span && span->color_set) ? span->color : color;

  // Copy to null-terminated buffer
  char buf[1024];
  int len = text_len > 1000 ? 1000 : text_len;
  memcpy(buf, text, (size_t)len);
  buf[len] = '\0';

  if (cogito_backend && cogito_backend->draw_text) {
    cogito_backend->draw_text(f, buf, x, y, size, draw_color);
    if (faux_bold) {
      CogitoColor c2 = draw_color;
      c2.a = (unsigned char)((float)c2.a * 0.72f);
      cogito_backend->draw_text(f, buf, x + 1, y, size, c2);
    }
  }

  if (style_flags && cogito_backend && cogito_backend->font_set_style)
    cogito_backend->font_set_style(f, COGITO_FONT_STYLE_NORMAL);
}

// Measure width of a text segment using span-resolved styles
static int cogito_span_segment_width(const CogitoNode* n, const char* text, int text_len, int size, const CogitoTextSpan* span) {
  if (!text || text_len <= 0) return 0;
  if (size <= 0) size = cogito_font_size();

  bool bold = span && span->bold_set ? span->bold : cogito_node_bold(n);
  bool medium = cogito_node_medium(n);

  CogitoFont* f;
  if (span && span->family_set && span->family[0]) {
    const char* path = cogito_resolve_font_by_name(span->family);
    if (path && path[0]) {
      int weight = cogito_font_weight_normal();
      if (bold) weight = cogito_font_weight_bold();
      else if (medium) weight = cogito_font_weight_medium();
      f = cogito_font_cache_get_var(
        cogito_font_cache,
        (int)(sizeof(cogito_font_cache) / sizeof(cogito_font_cache[0])),
        path, size, weight, NULL, 0);
      if (!f) f = cogito_font_variant_for_node(n, size, bold, medium);
    } else {
      f = cogito_font_variant_for_node(n, size, bold, medium);
    }
  } else {
    f = cogito_font_variant_for_node(n, size, bold, medium);
  }

  bool is_italic = span && span->italic_set ? span->italic : cogito_node_italic(n);
  bool is_underline = span && span->underline_set ? span->underline : cogito_node_underline(n);
  bool is_strike = span && span->strikethrough_set ? span->strikethrough : cogito_node_strikethrough(n);
  int style_flags = COGITO_FONT_STYLE_NORMAL;
  if (is_italic) style_flags |= COGITO_FONT_STYLE_ITALIC;
  if (is_underline) style_flags |= COGITO_FONT_STYLE_UNDERLINE;
  if (is_strike) style_flags |= COGITO_FONT_STYLE_STRIKETHROUGH;
  if (style_flags && cogito_backend && cogito_backend->font_set_style)
    cogito_backend->font_set_style(f, style_flags);

  float spacing = (n && n->letter_spacing_set) ? n->letter_spacing : 0.0f;
  char buf[1024];
  int len = text_len > 1000 ? 1000 : text_len;
  memcpy(buf, text, (size_t)len);
  buf[len] = '\0';
  int w = cogito_text_width_size_font_spaced(f, buf, size, spacing, (uint8_t)style_flags);

  if (style_flags && cogito_backend && cogito_backend->font_set_style)
    cogito_backend->font_set_style(f, COGITO_FONT_STYLE_NORMAL);
  return w;
}

// Draw a single pre-wrapped line using spans.
// line_text points into the original text at byte offset line_start.
// line_len is the byte length of this line.
static void cogito_draw_line_with_spans(const CogitoNode* n, int x, int y,
                                        const char* full_text,
                                        int line_start, int line_len,
                                        CogitoColor base_color, int base_size,
                                        bool draw) {
  if (!draw || line_len <= 0) return;
  int line_end = line_start + line_len;
  int pen_x = x;
  int pos = line_start;

  while (pos < line_end) {
    // Find which span (if any) covers 'pos'
    const CogitoTextSpan* span = NULL;
    int seg_end = line_end; // default: rest of line
    for (int i = 0; i < n->span_count; i++) {
      const CogitoTextSpan* s = &n->spans[i];
      if (s->end <= pos) continue;
      if (s->start > pos) {
        // Gap before this span — draw unstyled up to span start
        seg_end = s->start < line_end ? s->start : line_end;
        break;
      }
      // This span covers pos
      span = s;
      seg_end = s->end < line_end ? s->end : line_end;
      break;
    }

    int seg_len = seg_end - pos;
    if (seg_len <= 0) break;

    int size_to_use = (span && span->size_set) ? span->size : base_size;
    cogito_draw_span_segment(n, pen_x, y, full_text + pos, seg_len,
                             base_color, size_to_use, span);
    pen_x += cogito_span_segment_width(n, full_text + pos, seg_len, size_to_use, span);
    pos = seg_end;
  }
}

// Measure total width of a line accounting for per-span font metrics
static int cogito_line_width_with_spans(const CogitoNode* n,
                                        const char* full_text,
                                        int line_start, int line_len,
                                        int base_size) {
  if (line_len <= 0) return 0;
  if (n->span_count == 0)
    return cogito_text_width_size_n_node(n, full_text + line_start, line_len, base_size);

  int line_end = line_start + line_len;
  int total = 0;
  int pos = line_start;
  while (pos < line_end) {
    const CogitoTextSpan* span = NULL;
    int seg_end = line_end;
    for (int i = 0; i < n->span_count; i++) {
      const CogitoTextSpan* s = &n->spans[i];
      if (s->end <= pos) continue;
      if (s->start > pos) {
        seg_end = s->start < line_end ? s->start : line_end;
        break;
      }
      span = s;
      seg_end = s->end < line_end ? s->end : line_end;
      break;
    }
    int seg_len = seg_end - pos;
    if (seg_len <= 0) break;
    int size_to_use = (span && span->size_set) ? span->size : base_size;
    total += cogito_span_segment_width(n, full_text + pos, seg_len, size_to_use, span);
    pos = seg_end;
  }
  return total;
}

// Span-aware wrapped text drawing.  Falls back to cogito_draw_text_wrapped
// when span_count == 0 for zero overhead on plain text.
static int cogito_draw_text_wrapped_spans(const CogitoNode* n, int x, int y, int w,
                                          const char* text, CogitoColor color,
                                          int size, bool bold, bool medium,
                                          int align, bool draw, int max_lines) {
  if (!n || n->span_count == 0)
    return cogito_draw_text_wrapped(n, x, y, w, text, color, size, bold, medium, align, draw, max_lines);

  if (!text || w <= 0) return 0;
  if (size <= 0) size = cogito_font_size();
  int base_line_h = cogito_text_height_size(size);
  int base_adv = base_line_h;
  {
    bool lh_set = false;
    float lh = cogito_get_para_line_height(n, 0, &lh_set);
    if (lh_set && lh > 0.0f)
      base_adv = (int)lroundf(base_line_h * lh);
  }

  int lines = 0;
  int total_h = 0;
  const char* p = text;
  int text_offset = 0; // byte offset from start of text

  while (*p) {
    if (max_lines > 0 && lines >= max_lines) break;
    while (*p == '\n') {
      // Empty paragraph line — use its paragraph's line height
      bool lh_set = false;
      float lh = cogito_get_para_line_height(n, text_offset, &lh_set);
      int empty_adv = base_line_h;
      if (lh_set && lh > 0.0f) empty_adv = (int)lroundf(base_line_h * lh);
      lines++; total_h += empty_adv; p++; text_offset++;
      if (max_lines > 0 && lines >= max_lines) goto spans_done;
    }
    if (!*p) break;

    const char* start = p;
    int start_offset = text_offset;
    int line_len = 0;
    int last_space = -1;

    // Word-wrap loop (span-aware measurement for correct line breaks)
    while (*p && *p != '\n') {
      int wcur = cogito_line_width_with_spans(n, text, start_offset, line_len + 1, size);
      if (*p == ' ' || *p == '\t') last_space = line_len;
      if (wcur > w && line_len > 0) {
        if (last_space >= 0) {
          line_len = last_space;
          p = start + line_len + 1;
          text_offset = start_offset + line_len + 1;
        } else {
          p = start + line_len;
          text_offset = start_offset + line_len;
        }
        break;
      }
      line_len++;
      p++;
      text_offset++;
    }

    if (line_len > 0) {
      int line_w = cogito_line_width_with_spans(n, text, start_offset, line_len, size);
      int para_align = cogito_get_para_align(n, start_offset);
      int tx = x;
      if (para_align == 1) tx = x + (w - line_w) / 2;
      else if (para_align == 2) tx = x + (w - line_w);
      if (draw)
        cogito_draw_line_with_spans(n, tx, y + total_h, text,
                                    start_offset, line_len, color, size, true);
    }
    // Compute per-line height from tallest span on this line.
    int this_h = cogito_line_height_with_spans(n, start_offset, line_len, size);
    {
      bool lh_set = false;
      float lh = cogito_get_para_line_height(n, start_offset, &lh_set);
      if (lh_set && lh > 0.0f)
        this_h = (int)lroundf(this_h * lh);
    }
    total_h += this_h;
    if (*p == '\n') { p++; text_offset++; }
    lines++;
  }
  spans_done:
  if (lines == 0) { lines = 1; total_h = base_adv; }
  return total_h;
}
