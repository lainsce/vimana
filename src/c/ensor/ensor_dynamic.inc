// Ensor dynamic scheme — DynamicScheme-based color computation
// Faithfully ported from Ensor's DynamicColor.vala, DynamicScheme.vala,
// Scheme.vala, and all variant files (Default, Vibrant, Muted, Monochromatic, Salad, Content).
// Platform = DESKTOP, contrast_level = 0.0 throughout.

static inline double cogito_ensor_prof_now(void) {
#if defined(CLOCK_MONOTONIC)
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
  }
#endif
#if defined(TIME_UTC)
  struct timespec ts_utc;
  if (timespec_get(&ts_utc, TIME_UTC) == TIME_UTC) {
    return (double)ts_utc.tv_sec + (double)ts_utc.tv_nsec / 1000000000.0;
  }
#endif
  return 0.0;
}

// ---- Hue classification (from HCT.vala) ----
static bool cogito_hue_is_yellow(double h) { int fh = (int)floor(h); return fh >= 105 && fh < 125; }
static bool cogito_hue_is_cyan(double h)   { int fh = (int)floor(h); return fh >= 170 && fh < 207; }
static bool cogito_hue_is_blue(double h)   { int fh = (int)floor(h); return fh >= 250 && fh < 270; }

// ---- Contrast utilities (from Contrast.vala) ----
static double cogito_dyn_ratio_of_ys(double y1, double y2) {
  double lighter = y1 > y2 ? y1 : y2;
  double darker  = y1 > y2 ? y2 : y1;
  return (lighter + 5.0) / fmax(1e-10, darker + 5.0);
}

static double cogito_dyn_ratio_of_tones(double t1, double t2) {
  return cogito_dyn_ratio_of_ys(cogito_y_from_lstar(t1), cogito_y_from_lstar(t2));
}

// Returns lighter tone achieving ratio, or -1.0 if impossible.
static double cogito_dyn_lighter_safe(double tone, double ratio) {
  if (tone < 0.0 || tone > 100.0) return -1.0;
  double dark_y  = cogito_y_from_lstar(tone);
  double light_y = ratio * (dark_y + 5.0) - 5.0;
  if (light_y < 0.0 || light_y > 100.0) return -1.0;
  double real = cogito_dyn_ratio_of_ys(light_y, dark_y);
  if (real < ratio && fabs(real - ratio) > 0.04) return -1.0;
  double rv = cogito_lstar_from_y(light_y) + 0.4;
  return (rv < 0.0 || rv > 100.0) ? -1.0 : rv;
}

static double cogito_dyn_darker_safe(double tone, double ratio) {
  if (tone < 0.0 || tone > 100.0) return -1.0;
  double light_y = cogito_y_from_lstar(tone);
  double dark_y  = (light_y + 5.0) / fmax(1e-10, ratio) - 5.0;
  if (dark_y < 0.0 || dark_y > 100.0) return -1.0;
  double real = cogito_dyn_ratio_of_ys(light_y, dark_y);
  if (real < ratio && fabs(real - ratio) > 0.04) return -1.0;
  double rv = cogito_lstar_from_y(dark_y) - 0.4;
  return (rv < 0.0 || rv > 100.0) ? -1.0 : rv;
}

static double cogito_dyn_lighter_unsafe(double tone, double ratio) {
  double t = cogito_dyn_lighter_safe(tone, ratio);
  return t < 0.0 ? 100.0 : t;
}

static double cogito_dyn_darker_unsafe(double tone, double ratio) {
  double t = cogito_dyn_darker_safe(tone, ratio);
  return t < 0.0 ? 0.0 : t;
}

static bool cogito_dyn_prefers_light_fg(double tone) {
  return round(tone) < 60.0;
}

// From DynamicColor.vala foreground_tone
static double cogito_dyn_foreground_tone(double bg, double ratio) {
  double lighter_t = cogito_dyn_lighter_unsafe(bg, ratio);
  double darker_t  = cogito_dyn_darker_unsafe(bg, ratio);
  double lighter_r = cogito_dyn_ratio_of_tones(lighter_t, bg);
  double darker_r  = cogito_dyn_ratio_of_tones(darker_t, bg);
  bool prefer_light = cogito_dyn_prefers_light_fg(bg);

  if (lighter_t > 100.0) lighter_t = 100.0;
  if (lighter_t < 0.0) lighter_t = 0.0;
  if (darker_t > 100.0) darker_t = 100.0;
  if (darker_t < 0.0) darker_t = 0.0;

  if (ratio < 1.5) {
    return prefer_light ? lighter_t : darker_t;
  }

  if (prefer_light) {
    bool neg = fabs(lighter_r - darker_r) < 0.1 && lighter_r < ratio && darker_r < ratio;
    return (lighter_r >= ratio || lighter_r >= darker_r || neg) ? lighter_t : darker_t;
  } else {
    return (darker_r >= ratio || darker_r >= lighter_r) ? darker_t : lighter_t;
  }
}

static double cogito_dyn_tone_of_rgb(CogitoColor color) {
  double x = 0.0, y = 0.0, z = 0.0;
  cogito_xyz_from_srgb(color, &x, &y, &z);
  return cogito_lstar_from_y(y);
}

// ---- ContrastCurve (from ContrastCurve.vala) ----
static double cogito_lerp(double a, double b, double t) {
  return a + t * (b - a);
}

typedef struct {
  double low;
  double normal;
  double medium;
  double high;
} CogitoContrastCurve;

static double cogito_contrast_curve_get(CogitoContrastCurve c, double contrast) {
  if (contrast <= -1.0) {
    return c.low;
  } else if (contrast < 0.0) {
    return cogito_lerp(c.low, c.normal, contrast - (-1.0));
  } else if (contrast < 0.5) {
    return cogito_lerp(c.normal, c.medium, contrast / 0.5);
  } else if (contrast < 1.0) {
    return cogito_lerp(c.medium, c.high, (contrast - 0.5) / 0.5);
  } else {
    return c.high;
  }
}

// Contrast curves from ColorSpec2025.getContrastCurve(defaultContrast).
static CogitoContrastCurve cogito_dyn_curve(double default_contrast) {
  if (default_contrast == 1.5) return (CogitoContrastCurve){ 1.5, 1.5, 3.0, 5.5 };
  if (default_contrast == 3.0) return (CogitoContrastCurve){ 3.0, 3.0, 4.5, 7.0 };
  if (default_contrast == 4.5) return (CogitoContrastCurve){ 4.5, 4.5, 7.0, 11.0 };
  if (default_contrast == 6.0) return (CogitoContrastCurve){ 6.0, 6.0, 7.0, 11.0 };
  if (default_contrast == 7.0) return (CogitoContrastCurve){ 7.0, 7.0, 11.0, 21.0 };
  if (default_contrast == 9.0) return (CogitoContrastCurve){ 9.0, 9.0, 11.0, 21.0 };
  if (default_contrast == 11.0) return (CogitoContrastCurve){ 11.0, 11.0, 21.0, 21.0 };
  if (default_contrast == 21.0) return (CogitoContrastCurve){ 21.0, 21.0, 21.0, 21.0 };
  return (CogitoContrastCurve){ default_contrast, default_contrast, 7.0, 21.0 };
}

// ---- Tone finders (faithful port of find_best_tone_for_chroma from Scheme.vala) ----
typedef struct {
  bool valid;
  double hue;
  double chroma;
  uint64_t stamp;
  uint8_t has_rgb[101];
  CogitoColor rgb[101];
  uint8_t has_chroma[101];
  double actual_chroma[101];
} CogitoDynToneChromaCache;

#define COGITO_DYN_TONE_CHROMA_CACHE_SLOTS 12
static CogitoDynToneChromaCache cogito_dyn_tone_chroma_cache[COGITO_DYN_TONE_CHROMA_CACHE_SLOTS];
static uint64_t cogito_dyn_tone_chroma_stamp = 1;

static CogitoDynToneChromaCache* cogito_dyn_tone_chroma_cache_get(double hue, double chroma) {
  const double eps = 1e-9;
  int free_idx = -1;
  int lru_idx = 0;
  uint64_t lru_stamp = UINT64_MAX;
  for (int i = 0; i < COGITO_DYN_TONE_CHROMA_CACHE_SLOTS; i++) {
    CogitoDynToneChromaCache* e = &cogito_dyn_tone_chroma_cache[i];
    if (e->valid) {
      if (fabs(e->hue - hue) < eps && fabs(e->chroma - chroma) < eps) {
        e->stamp = cogito_dyn_tone_chroma_stamp++;
        return e;
      }
      if (e->stamp < lru_stamp) {
        lru_stamp = e->stamp;
        lru_idx = i;
      }
    } else if (free_idx < 0) {
      free_idx = i;
    }
  }
  int idx = (free_idx >= 0) ? free_idx : lru_idx;
  CogitoDynToneChromaCache* e = &cogito_dyn_tone_chroma_cache[idx];
  e->valid = true;
  e->hue = hue;
  e->chroma = chroma;
  e->stamp = cogito_dyn_tone_chroma_stamp++;
  memset(e->has_rgb, 0, sizeof(e->has_rgb));
  memset(e->has_chroma, 0, sizeof(e->has_chroma));
  return e;
}

static CogitoColor cogito_dyn_rgb_for_tone(
    CogitoDynToneChromaCache* cache, double hue, double chroma, int tone_i) {
  if (!cache || tone_i < 0 || tone_i > 100) {
    return cogito_hct_to_rgb(hue, chroma, (double)tone_i);
  }
  if (!cache->has_rgb[tone_i]) {
    cache->rgb[tone_i] = cogito_hct_to_rgb(hue, chroma, (double)tone_i);
    cache->has_rgb[tone_i] = 1;
  }
  return cache->rgb[tone_i];
}

static double cogito_dyn_actual_chroma_for_tone(
    CogitoDynToneChromaCache* cache, double hue, double chroma, int tone_i) {
  if (!cache || tone_i < 0 || tone_i > 100) return 0.0;
  if (!cache->has_chroma[tone_i]) {
    CogitoColor cand = cogito_dyn_rgb_for_tone(cache, hue, chroma, tone_i);
    double ch = 0.0, cc = 0.0, ct = 0.0;
    cogito_rgb_to_hct(cand, &ch, &cc, &ct);
    cache->actual_chroma[tone_i] = cc;
    cache->has_chroma[tone_i] = 1;
  }
  return cache->actual_chroma[tone_i];
}

static double cogito_dyn_find_best_tone(double hue, double chroma, double start, bool by_decreasing) {
  int start_i = (int)lround(start);
  if (start_i >= 0 && start_i <= 100 && fabs(start - (double)start_i) < 1e-9) {
    CogitoDynToneChromaCache* cache = cogito_dyn_tone_chroma_cache_get(hue, chroma);
    int answer_i = start_i;
    double best_c = cogito_dyn_actual_chroma_for_tone(cache, hue, chroma, answer_i);
    if (best_c >= chroma) return (double)answer_i;
    if (by_decreasing) {
      for (int tone_i = start_i - 1; tone_i >= 0 && best_c < chroma; tone_i--) {
        double cc = cogito_dyn_actual_chroma_for_tone(cache, hue, chroma, tone_i);
        if (cc > best_c) {
          best_c = cc;
          answer_i = tone_i;
        }
      }
    } else {
      for (int tone_i = start_i + 1; tone_i <= 100 && best_c < chroma; tone_i++) {
        double cc = cogito_dyn_actual_chroma_for_tone(cache, hue, chroma, tone_i);
        if (cc > best_c) {
          best_c = cc;
          answer_i = tone_i;
        }
      }
    }
    return (double)answer_i;
  }

  // Fallback for non-integer starts (keeps original behavior).
  double answer = start;
  CogitoColor rgb = cogito_hct_to_rgb(hue, chroma, answer);
  double rh, rc, rt;
  cogito_rgb_to_hct(rgb, &rh, &rc, &rt);
  double best_c = rc;
  double tone = start;
  while (best_c < chroma) {
    if (tone < 0.0 || tone > 100.0) break;
    tone += by_decreasing ? -1.0 : 1.0;
    if (tone < 0.0 || tone > 100.0) break;
    CogitoColor cand = cogito_hct_to_rgb(hue, chroma, tone);
    double ch, cc, ct;
    cogito_rgb_to_hct(cand, &ch, &cc, &ct);
    if (cc > best_c) {
      best_c = cc;
      answer = tone;
    }
  }
  return answer;
}

// t_max_c: starts at 100, decrements → highest tone achieving target chroma
typedef struct {
  bool valid;
  int mode; // 0=tmax_c_m, 1=tmin_c
  double hue;
  double chroma;
  double lo;
  double hi;
  double cmult;
  double value;
  uint64_t stamp;
} CogitoDynToneBoundCacheEntry;

#define COGITO_DYN_TONE_BOUND_CACHE_SLOTS 128
static CogitoDynToneBoundCacheEntry cogito_dyn_tone_bound_cache[COGITO_DYN_TONE_BOUND_CACHE_SLOTS];
static uint64_t cogito_dyn_tone_bound_stamp = 1;

static bool cogito_dyn_tone_bound_cache_lookup(
    int mode, double hue, double chroma, double lo, double hi, double cmult,
    double* out_value) {
  const double eps = 1e-9;
  for (int i = 0; i < COGITO_DYN_TONE_BOUND_CACHE_SLOTS; i++) {
    CogitoDynToneBoundCacheEntry* e = &cogito_dyn_tone_bound_cache[i];
    if (!e->valid) continue;
    if (e->mode != mode) continue;
    if (fabs(e->hue - hue) >= eps) continue;
    if (fabs(e->chroma - chroma) >= eps) continue;
    if (fabs(e->lo - lo) >= eps) continue;
    if (fabs(e->hi - hi) >= eps) continue;
    if (fabs(e->cmult - cmult) >= eps) continue;
    e->stamp = cogito_dyn_tone_bound_stamp++;
    if (out_value) *out_value = e->value;
    return true;
  }
  return false;
}

static void cogito_dyn_tone_bound_cache_store(
    int mode, double hue, double chroma, double lo, double hi, double cmult,
    double value) {
  int free_idx = -1;
  int lru_idx = 0;
  uint64_t lru_stamp = UINT64_MAX;
  for (int i = 0; i < COGITO_DYN_TONE_BOUND_CACHE_SLOTS; i++) {
    CogitoDynToneBoundCacheEntry* e = &cogito_dyn_tone_bound_cache[i];
    if (!e->valid) {
      free_idx = i;
      break;
    }
    if (e->stamp < lru_stamp) {
      lru_stamp = e->stamp;
      lru_idx = i;
    }
  }
  int idx = (free_idx >= 0) ? free_idx : lru_idx;
  CogitoDynToneBoundCacheEntry* e = &cogito_dyn_tone_bound_cache[idx];
  e->valid = true;
  e->mode = mode;
  e->hue = hue;
  e->chroma = chroma;
  e->lo = lo;
  e->hi = hi;
  e->cmult = cmult;
  e->value = value;
  e->stamp = cogito_dyn_tone_bound_stamp++;
}

static double cogito_dyn_tmax_c_m(double hue, double chroma, double lo, double hi, double cmult) {
  double target = chroma * cmult;
  lo = lo < 0.0 ? 0.0 : (lo > 100.0 ? 100.0 : lo);
  hi = hi < lo ? lo : (hi > 100.0 ? 100.0 : hi);
  double cached = 0.0;
  if (cogito_dyn_tone_bound_cache_lookup(0, hue, target, lo, hi, 1.0, &cached)) {
    return cached;
  }
  double answer = cogito_dyn_find_best_tone(hue, target, 100.0, true);
  if (answer < lo) answer = lo;
  if (answer > hi) answer = hi;
  cogito_dyn_tone_bound_cache_store(0, hue, target, lo, hi, 1.0, answer);
  return answer;
}

static double cogito_dyn_tmax_c(double hue, double chroma, double lo, double hi) {
  return cogito_dyn_tmax_c_m(hue, chroma, lo, hi, 1.0);
}

// t_min_c: starts at 0, increments → lowest tone achieving target chroma
static double cogito_dyn_tmin_c(double hue, double chroma, double lo, double hi) {
  lo = lo < 0.0 ? 0.0 : (lo > 100.0 ? 100.0 : lo);
  hi = hi < lo ? lo : (hi > 100.0 ? 100.0 : hi);
  double cached = 0.0;
  if (cogito_dyn_tone_bound_cache_lookup(1, hue, chroma, lo, hi, 1.0, &cached)) {
    return cached;
  }
  double answer = cogito_dyn_find_best_tone(hue, chroma, 0.0, false);
  if (answer < lo) answer = lo;
  if (answer > hi) answer = hi;
  cogito_dyn_tone_bound_cache_store(1, hue, chroma, lo, hi, 1.0, answer);
  return answer;
}

// ---- fix_disliked (from HCT.vala) ----
// Hues in the yellowish range with moderate chroma at dark tones look "muddy".
// Fix by pushing tone to 70.
static void cogito_dyn_fix_disliked(double* h, double* c, double* t) {
  int rh = (int)round(*h);
  if (rh >= 90 && rh <= 111 && round(*c) > 16.0 && round(*t) < 65.0) {
    CogitoColor fixed = cogito_hct_to_rgb(*h, *c, 70.0);
    cogito_rgb_to_hct(fixed, h, c, t);
  }
}

// ---- Palette struct ----
typedef struct { double hue, chroma; } CogitoEnsorPal;

// ---- Dynamic scheme context ----
typedef struct {
  CogitoEnsorPal primary, secondary, tertiary, neutral, neutral_var, error;
  double src_h, src_c, src_t;
  bool dark;
  double contrast_level;
  CogitoEnsorVariant variant;
} CogitoEnsorDyn;

// ---- Correct rotation tables from Vala sources ----

// Vibrant secondary (Vibrant.vala)
static const double dyn_vib_sec_hues[] = { 0, 21, 51, 121, 158, 191, 272, 306, 360 };
static const double dyn_vib_sec_rots[] = { -160, 155, -100, 96, -96, -156, -165, -160 };
// Vibrant tertiary
static const double dyn_vib_tert_hues[] = { 0, 105, 140, 204, 253, 278, 300, 333, 360 };
static const double dyn_vib_tert_rots[] = { -165, 160, -105, 101, -101, -160, -170, -165 };
// Vibrant neutral
static const double dyn_vib_neut_hues[] = { 0, 71, 124, 253, 278, 300, 360 };
static const double dyn_vib_neut_rots[] = { 10, 0, 10, 0, 10, 0 };

// Muted tertiary (Muted.vala)
static const double dyn_mut_tert_hues[] = { 0, 38, 105, 161, 204, 278, 333, 360 };
static const double dyn_mut_tert_rots[] = { -32, 26, 10, -39, 24, -15, -32 };

// Default tertiary (Default.vala)
static const double dyn_def_tert_hues[] = { 0.0, 20.0, 71.0, 161.0, 333.0, 360.0 };
static const double dyn_def_tert_rots[] = { -40.0, 48.0, -32.0, 29.5, -32.0 };

// Error palette hue table (DynamicScheme.vala)
static const double dyn_err_hue_bps[] = { 0, 3, 13, 23, 33, 43, 153, 273, 360 };
static const double dyn_err_hue_vals[] = { 12, 22, 32, 12, 22, 32, 22, 12 };

// ---- Error palette hue from source hue ----
static double cogito_dyn_error_hue(double src_h) {
  return cogito_ensor_piecewise_val(src_h,
    dyn_err_hue_bps, sizeof(dyn_err_hue_bps)/sizeof(dyn_err_hue_bps[0]),
    dyn_err_hue_vals, sizeof(dyn_err_hue_vals)/sizeof(dyn_err_hue_vals[0]));
}

// ---- Rotated hue helper ----
static double cogito_dyn_rotated(double src_h, const double* hbps, size_t nh,
                                  const double* rots, size_t nr) {
  return cogito_ensor_rotated_hue(src_h, hbps, nh, rots, nr);
}

#define DYN_ROT(src, hbps, rots) cogito_dyn_rotated(src, hbps, sizeof(hbps)/sizeof(hbps[0]), rots, sizeof(rots)/sizeof(rots[0]))

// ---- Build DynScheme per variant (from each variant's .vala file) ----
static CogitoEnsorDyn cogito_dyn_make(double h, double c, double t, bool dark, double contrast_level, CogitoEnsorVariant variant) {
  CogitoEnsorDyn s;
  memset(&s, 0, sizeof(s));
  s.src_h = h; s.src_c = c; s.src_t = t;
  s.dark = dark; s.contrast_level = contrast_level; s.variant = variant;

  // Force monochrome if source chroma ≤ 5 (except monochrome itself, always mono)
  bool force_mono = (c <= 5.0 && variant != COGITO_ENSOR_MONOCHROME && variant != COGITO_ENSOR_CONTENT);
  if (force_mono) variant = COGITO_ENSOR_MONOCHROME;
  s.variant = variant;

  switch (variant) {
    case COGITO_ENSOR_VIBRANT: {
      s.primary   = (CogitoEnsorPal){ h, dark ? 36.0 : 48.0 };
      double sh   = DYN_ROT(h, dyn_vib_sec_hues, dyn_vib_sec_rots);
      s.secondary = (CogitoEnsorPal){ sh, dark ? 16.0 : 24.0 };
      double th2  = DYN_ROT(h, dyn_vib_tert_hues, dyn_vib_tert_rots);
      s.tertiary  = (CogitoEnsorPal){ th2, 48.0 };
      double nh2  = DYN_ROT(h, dyn_vib_neut_hues, dyn_vib_neut_rots);
      s.neutral   = (CogitoEnsorPal){ nh2, dark ? (cogito_hue_is_yellow(h) ? 6.0 : 14.0) : 18.0 };
      s.neutral_var = (CogitoEnsorPal){ h, (h >= 105.0 && h < 125.0) ? 16.0 : 23.0 };
    } break;

    case COGITO_ENSOR_MUTED: {
      bool blue = cogito_hue_is_blue(h);
      s.primary   = (CogitoEnsorPal){ h, blue ? 12.0 : 8.0 };
      s.secondary = (CogitoEnsorPal){ h, blue ? 6.0 : 4.0 };
      double th2  = DYN_ROT(h, dyn_mut_tert_hues, dyn_mut_tert_rots);
      s.tertiary  = (CogitoEnsorPal){ th2, 20.0 };
      s.neutral   = (CogitoEnsorPal){ h, 1.0 };
      s.neutral_var = (CogitoEnsorPal){ h, 2.2 };
    } break;

    case COGITO_ENSOR_MONOCHROME:
      s.primary     = (CogitoEnsorPal){ h, 0.0 };
      s.secondary   = (CogitoEnsorPal){ h, 0.0 };
      s.tertiary    = (CogitoEnsorPal){ h, 0.0 };
      s.neutral     = (CogitoEnsorPal){ h, 0.0 };
      s.neutral_var = (CogitoEnsorPal){ h, 0.0 };
      break;

    case COGITO_ENSOR_SALAD: {
      double sh = cogito_ensor_sanitize_deg(h - 50.0);
      s.primary   = (CogitoEnsorPal){ sh, 48.0 };
      s.secondary = (CogitoEnsorPal){ sh, 24.0 };
      s.tertiary  = (CogitoEnsorPal){ h, 36.0 };
      s.neutral   = (CogitoEnsorPal){ h, 10.0 };
      s.neutral_var = (CogitoEnsorPal){ h, 16.0 };
    } break;

    case COGITO_ENSOR_CONTENT: {
      s.primary   = (CogitoEnsorPal){ h, c };
      s.secondary = (CogitoEnsorPal){ h, fmax(c - 32.0, c * 0.5) };
      // Tertiary: analogous(3, 6)[2] → from_params(h + 120, c, t), then fix_disliked
      double th2 = cogito_ensor_sanitize_deg(h + 120.0);
      double tc2 = c, tt2 = t;
      // Gamut-map at the analogous hue to get actual h/c
      CogitoColor tmp = cogito_hct_to_rgb(th2, tc2, tt2);
      cogito_rgb_to_hct(tmp, &th2, &tc2, &tt2);
      cogito_dyn_fix_disliked(&th2, &tc2, &tt2);
      s.tertiary  = (CogitoEnsorPal){ th2, tc2 };
      s.neutral   = (CogitoEnsorPal){ h, c / 8.0 };
      s.neutral_var = (CogitoEnsorPal){ h, c / 8.0 + 4.0 };
    } break;

    default: { // COGITO_ENSOR_DEFAULT
      s.primary   = (CogitoEnsorPal){ h, dark ? 26.0 : 32.0 };
      s.secondary = (CogitoEnsorPal){ h, 16.0 };
      double th2  = DYN_ROT(h, dyn_def_tert_hues, dyn_def_tert_rots);
      s.tertiary  = (CogitoEnsorPal){ th2, dark ? 28.0 : 32.0 };
      s.neutral   = (CogitoEnsorPal){ h, 5.0 };
      s.neutral_var = (CogitoEnsorPal){ h, 5.0 * 1.7 };
    } break;
  }

  // Error palette (common to all variants)
  double err_h = cogito_dyn_error_hue(h);
  s.error = (CogitoEnsorPal){ err_h, fmax(c, 60.0) };

  return s;
}

// ---- Surface tones (DESKTOP, from Scheme.vala) ----
// All surface roles use the neutral palette.

static double cogito_dyn_surface_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 4.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 99.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 97.0;
  return 98.0;
}

static double cogito_dyn_surface_bright_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 18.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 99.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 97.0;
  return 98.0;
}

static double cogito_dyn_surface_dim_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 4.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 90.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 85.0;
  return 87.0;
}

static double cogito_dyn_sc_lowest_tone(const CogitoEnsorDyn* s) {
  return s->dark ? 0.0 : 100.0;
}

static double cogito_dyn_sc_low_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 6.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 98.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 95.0;
  return 96.0;
}

static double cogito_dyn_sc_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 9.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 96.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 92.0;
  return 94.0;
}

static double cogito_dyn_sc_high_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 12.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 94.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 90.0;
  return 92.0;
}

static double cogito_dyn_sc_highest_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 15.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 92.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 88.0;
  return 90.0;
}

// ---- Neutral chromamult helpers (DESKTOP, from Scheme.vala) ----
// surface_bright: chromamult only in dark mode
static double cogito_dyn_sbright_cmult(const CogitoEnsorDyn* s) {
  if (s->dark) {
    if (s->variant == COGITO_ENSOR_MUTED) return 2.5;
    if (s->variant == COGITO_ENSOR_DEFAULT) return 1.7;
    if (s->variant == COGITO_ENSOR_SALAD)
      return cogito_hue_is_yellow(s->neutral.hue) ? 2.7 : 1.75;
    if (s->variant == COGITO_ENSOR_VIBRANT) return 1.36;
  }
  return 1.0;
}

// surface_dim: chromamult only in light mode
static double cogito_dyn_sdim_cmult(const CogitoEnsorDyn* s) {
  if (!s->dark) {
    if (s->variant == COGITO_ENSOR_MUTED) return 2.5;
    if (s->variant == COGITO_ENSOR_DEFAULT) return 1.7;
    if (s->variant == COGITO_ENSOR_SALAD)
      return cogito_hue_is_yellow(s->neutral.hue) ? 2.7 : 1.75;
    if (s->variant == COGITO_ENSOR_VIBRANT) return 1.36;
  }
  return 1.0;
}

// surface_container_low
static double cogito_dyn_scl_cmult(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MUTED) return 1.3;
  if (s->variant == COGITO_ENSOR_DEFAULT) return 1.25;
  if (s->variant == COGITO_ENSOR_SALAD)
    return cogito_hue_is_yellow(s->neutral.hue) ? 1.3 : 1.15;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 1.08;
  return 1.0;
}

// surface_container
static double cogito_dyn_sc_cmult(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MUTED) return 1.6;
  if (s->variant == COGITO_ENSOR_DEFAULT) return 1.4;
  if (s->variant == COGITO_ENSOR_SALAD)
    return cogito_hue_is_yellow(s->neutral.hue) ? 1.6 : 1.3;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 1.15;
  return 1.0;
}

// surface_container_high
static double cogito_dyn_sch_cmult(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MUTED) return 1.9;
  if (s->variant == COGITO_ENSOR_DEFAULT) return 1.5;
  if (s->variant == COGITO_ENSOR_SALAD)
    return cogito_hue_is_yellow(s->neutral.hue) ? 1.95 : 1.45;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 1.22;
  return 1.0;
}

// surface_container_highest (same as on_surface/outline/on_surface_variant neutral mult)
static double cogito_dyn_schi_cmult(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MUTED) return 2.2;
  if (s->variant == COGITO_ENSOR_DEFAULT) return 1.7;
  if (s->variant == COGITO_ENSOR_SALAD)
    return cogito_hue_is_yellow(s->neutral.hue) ? 2.3 : 1.6;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 1.29;
  return 1.0;
}

// on_surface, on_surface_variant, outline, outline_variant neutral chromamult
static double cogito_dyn_neut_cmult(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MUTED) return 2.2;
  if (s->variant == COGITO_ENSOR_DEFAULT) return 1.7;
  if (s->variant == COGITO_ENSOR_SALAD)
    return cogito_hue_is_yellow(s->neutral.hue) ? (s->dark ? 3.0 : 2.3) : 1.6;
  return 1.0;
}

// ---- Render color from palette with chromamult ----
static CogitoColor cogito_dyn_pal_color(CogitoEnsorPal p, double tone, double cmult) {
  if (fabs(cmult - 1.0) < 1e-9) {
    int tone_i = (int)lround(tone);
    if (tone_i >= 0 && tone_i <= 100 && fabs(tone - (double)tone_i) < 1e-9) {
      CogitoDynToneChromaCache* cache = cogito_dyn_tone_chroma_cache_get(p.hue, p.chroma);
      return cogito_dyn_rgb_for_tone(cache, p.hue, p.chroma, tone_i);
    }
  }
  return cogito_hct_to_rgb(p.hue, p.chroma * cmult, tone);
}

typedef struct {
  double tone;
  double cmult;
} CogitoDynToneChoice;

#define COGITO_DYN_PICK_CMULTS 11
#define COGITO_DYN_PICK_TONES 101
#define COGITO_DYN_PICK_CACHE_SLOTS 12

typedef struct {
  bool valid;
  double hue;
  double chroma;
  uint64_t stamp;
  uint8_t has_tone[COGITO_DYN_PICK_CMULTS][COGITO_DYN_PICK_TONES];
  double fg_tone[COGITO_DYN_PICK_CMULTS][COGITO_DYN_PICK_TONES];
} CogitoDynPickToneCache;

static CogitoDynPickToneCache cogito_dyn_pick_tone_cache[COGITO_DYN_PICK_CACHE_SLOTS];
static uint64_t cogito_dyn_pick_tone_stamp = 1;
static bool cogito_dyn_pick_track_stats = false;
static uint64_t cogito_dyn_pick_eval_count = 0;
static uint64_t cogito_dyn_pick_miss_count = 0;

static CogitoDynPickToneCache* cogito_dyn_pick_tone_cache_get(CogitoEnsorPal p) {
  const double eps = 1e-9;
  int free_idx = -1;
  int lru_idx = 0;
  uint64_t lru_stamp = UINT64_MAX;
  for (int i = 0; i < COGITO_DYN_PICK_CACHE_SLOTS; i++) {
    CogitoDynPickToneCache* e = &cogito_dyn_pick_tone_cache[i];
    if (e->valid) {
      if (fabs(e->hue - p.hue) < eps && fabs(e->chroma - p.chroma) < eps) {
        e->stamp = cogito_dyn_pick_tone_stamp++;
        return e;
      }
      if (e->stamp < lru_stamp) {
        lru_stamp = e->stamp;
        lru_idx = i;
      }
    } else if (free_idx < 0) {
      free_idx = i;
    }
  }

  int idx = (free_idx >= 0) ? free_idx : lru_idx;
  CogitoDynPickToneCache* e = &cogito_dyn_pick_tone_cache[idx];
  e->valid = true;
  e->hue = p.hue;
  e->chroma = p.chroma;
  e->stamp = cogito_dyn_pick_tone_stamp++;
  memset(e->has_tone, 0, sizeof(e->has_tone));
  return e;
}

static double cogito_dyn_pick_fg_tone(
    CogitoDynPickToneCache* cache, CogitoEnsorPal p, int ci, int tone_i) {
  if (cogito_dyn_pick_track_stats) cogito_dyn_pick_eval_count++;
  if (!cache || ci < 0 || ci >= COGITO_DYN_PICK_CMULTS ||
      tone_i < 0 || tone_i >= COGITO_DYN_PICK_TONES) {
    return 0.0;
  }
  if (!cache->has_tone[ci][tone_i]) {
    if (cogito_dyn_pick_track_stats) cogito_dyn_pick_miss_count++;
    double cmult = 1.0 - (0.1 * (double)ci);
    double tone = (double)tone_i;
    CogitoColor fg = cogito_dyn_pal_color(p, tone, cmult);
    cache->fg_tone[ci][tone_i] = cogito_dyn_tone_of_rgb(fg);
    cache->has_tone[ci][tone_i] = 1;
  }
  return cache->fg_tone[ci][tone_i];
}

static inline void cogito_dyn_pick_update_best(
    double ratio, double tone, double cmult, double desired_tone,
    CogitoDynToneChoice* best_choice, double* best_ratio) {
  if (!best_choice || !best_ratio) return;
  if (ratio > *best_ratio ||
      (fabs(ratio - *best_ratio) < 1e-9 &&
       (cmult > best_choice->cmult + 1e-9 ||
        (fabs(cmult - best_choice->cmult) < 1e-9 &&
         fabs(tone - desired_tone) < fabs(best_choice->tone - desired_tone))))) {
    *best_ratio = ratio;
    best_choice->tone = tone;
    best_choice->cmult = cmult;
  }
}

// Find nearest passing tone in one direction from start_i.
// dir: +1 for increasing tones, -1 for decreasing tones.
static bool cogito_dyn_pick_find_nearest_pass_dir(
    CogitoDynPickToneCache* tone_cache, CogitoEnsorPal p, int ci, int start_i, int dir,
    double bg_tone, double min_ratio, double desired_tone,
    CogitoDynToneChoice* best_choice, double* best_ratio, int* out_tone_i) {
  if (!tone_cache || !out_tone_i || (dir != 1 && dir != -1)) return false;
  double cmult = 1.0 - (0.1 * (double)ci);
  int boundary = (dir > 0) ? (COGITO_DYN_PICK_TONES - 1) : 0;
  if (start_i == boundary) return false;

  // If the far boundary cannot satisfy contrast, this direction has no solution.
  double fg_boundary = cogito_dyn_pick_fg_tone(tone_cache, p, ci, boundary);
  double ratio_boundary = cogito_dyn_ratio_of_tones(fg_boundary, bg_tone);
  cogito_dyn_pick_update_best(ratio_boundary, (double)boundary, cmult, desired_tone,
                              best_choice, best_ratio);
  if (ratio_boundary < min_ratio) return false;

  int fail = start_i;
  int pass = boundary;
  while ((dir > 0) ? (pass - fail > 1) : (fail - pass > 1)) {
    int mid = (fail + pass) / 2;
    double fg_mid = cogito_dyn_pick_fg_tone(tone_cache, p, ci, mid);
    double ratio_mid = cogito_dyn_ratio_of_tones(fg_mid, bg_tone);
    cogito_dyn_pick_update_best(ratio_mid, (double)mid, cmult, desired_tone,
                                best_choice, best_ratio);
    if (ratio_mid >= min_ratio) pass = mid;
    else fail = mid;
  }

  // Small local refinement toward the start to guard against minor non-monotonicity.
  if (dir > 0) {
    int min_i = start_i + 1;
    for (int ti = pass - 1; ti >= min_i && ti >= pass - 3; ti--) {
      double fg = cogito_dyn_pick_fg_tone(tone_cache, p, ci, ti);
      double ratio = cogito_dyn_ratio_of_tones(fg, bg_tone);
      cogito_dyn_pick_update_best(ratio, (double)ti, cmult, desired_tone,
                                  best_choice, best_ratio);
      if (ratio >= min_ratio) pass = ti;
      else break;
    }
  } else {
    int max_i = start_i - 1;
    for (int ti = pass + 1; ti <= max_i && ti <= pass + 3; ti++) {
      double fg = cogito_dyn_pick_fg_tone(tone_cache, p, ci, ti);
      double ratio = cogito_dyn_ratio_of_tones(fg, bg_tone);
      cogito_dyn_pick_update_best(ratio, (double)ti, cmult, desired_tone,
                                  best_choice, best_ratio);
      if (ratio >= min_ratio) pass = ti;
      else break;
    }
  }

  *out_tone_i = pass;
  return true;
}

// Resolve tone/chroma that satisfies contrast after HCT -> RGB gamut mapping.
// Prefers higher chroma first; for the same chroma, chooses tone closest to
// desired_tone. If no candidate can meet the ratio, returns the highest-ratio
// candidate seen.
static CogitoDynToneChoice cogito_dyn_pick_contrasting_tone(
    CogitoEnsorPal p, double desired_tone, CogitoColor bg_color, double min_ratio) {
  CogitoDynPickToneCache* tone_cache = cogito_dyn_pick_tone_cache_get(p);
  double bg_tone = cogito_dyn_tone_of_rgb(bg_color);
  CogitoDynToneChoice best_ratio_choice = (CogitoDynToneChoice){ desired_tone, 1.0 };
  double best_ratio = -1.0;
  int desired_i = (int)lround(desired_tone);
  if (desired_i < 0) desired_i = 0;
  if (desired_i >= COGITO_DYN_PICK_TONES) desired_i = COGITO_DYN_PICK_TONES - 1;
  bool global_can_plus = cogito_dyn_ratio_of_tones(100.0, bg_tone) >= min_ratio;
  bool global_can_minus = cogito_dyn_ratio_of_tones(0.0, bg_tone) >= min_ratio;

  // Ordered by cmult (highest first). For each cmult, find nearest pass using
  // directed binary search with exact ratio checks.
  for (int ci = 0; ci < COGITO_DYN_PICK_CMULTS; ci++) {
    double cmult = 1.0 - (0.1 * (double)ci);
    bool plus_possible = false;
    bool minus_possible = false;

    // If an extreme tone cannot satisfy contrast in tone-space, that direction
    // cannot pass for any palette realization at this background.
    if (global_can_plus && desired_i < (COGITO_DYN_PICK_TONES - 1)) {
      double fg_plus = cogito_dyn_pick_fg_tone(
          tone_cache, p, ci, COGITO_DYN_PICK_TONES - 1);
      double ratio_plus = cogito_dyn_ratio_of_tones(fg_plus, bg_tone);
      cogito_dyn_pick_update_best(ratio_plus, (double)(COGITO_DYN_PICK_TONES - 1), cmult,
                                  desired_tone, &best_ratio_choice, &best_ratio);
      plus_possible = ratio_plus >= min_ratio;
    }
    if (global_can_minus && desired_i > 0) {
      double fg_minus = cogito_dyn_pick_fg_tone(tone_cache, p, ci, 0);
      double ratio_minus = cogito_dyn_ratio_of_tones(fg_minus, bg_tone);
      cogito_dyn_pick_update_best(ratio_minus, 0.0, cmult, desired_tone,
                                  &best_ratio_choice, &best_ratio);
      minus_possible = ratio_minus >= min_ratio;
    }

    // This cmult cannot satisfy contrast at any tone; skip desired-tone probe.
    if (!plus_possible && !minus_possible) continue;

    // Desired tone exact hit first.
    double fg_start = cogito_dyn_pick_fg_tone(tone_cache, p, ci, desired_i);
    double ratio_start = cogito_dyn_ratio_of_tones(fg_start, bg_tone);
    cogito_dyn_pick_update_best(ratio_start, (double)desired_i, cmult, desired_tone,
                                &best_ratio_choice, &best_ratio);
    if (ratio_start >= min_ratio) {
      return (CogitoDynToneChoice){ (double)desired_i, cmult };
    }

    int plus_i = -1;
    int minus_i = -1;
    bool have_plus = false;
    bool have_minus = false;

    if (plus_possible) {
      have_plus = cogito_dyn_pick_find_nearest_pass_dir(
          tone_cache, p, ci, desired_i, +1, bg_tone, min_ratio, desired_tone,
          &best_ratio_choice, &best_ratio, &plus_i);
    }
    if (minus_possible) {
      have_minus = cogito_dyn_pick_find_nearest_pass_dir(
          tone_cache, p, ci, desired_i, -1, bg_tone, min_ratio, desired_tone,
          &best_ratio_choice, &best_ratio, &minus_i);
    }

    if (have_plus || have_minus) {
      int chosen_i;
      if (have_plus && !have_minus) chosen_i = plus_i;
      else if (!have_plus && have_minus) chosen_i = minus_i;
      else {
        int d_plus = abs(plus_i - desired_i);
        int d_minus = abs(minus_i - desired_i);
        // Keep same tie-break as scan order (+delta before -delta).
        chosen_i = (d_plus <= d_minus) ? plus_i : minus_i;
      }
      return (CogitoDynToneChoice){ (double)chosen_i, cmult };
    }
  }

  // Safety fallback for edge cases where directional search misses a solution.
  for (int ci = 0; ci < COGITO_DYN_PICK_CMULTS; ci++) {
    double cmult = 1.0 - (0.1 * (double)ci);
    for (int delta = 0; delta <= 100; delta++) {
      int tones[2] = { desired_i + delta, desired_i - delta };
      int tone_count = (delta == 0) ? 1 : 2;
      for (int ti = 0; ti < tone_count; ti++) {
        int tone_i = tones[ti];
        if (tone_i < 0 || tone_i >= COGITO_DYN_PICK_TONES) continue;
        double fg_tone = cogito_dyn_pick_fg_tone(tone_cache, p, ci, tone_i);
        double ratio = cogito_dyn_ratio_of_tones(fg_tone, bg_tone);
        cogito_dyn_pick_update_best(ratio, (double)tone_i, cmult, desired_tone,
                                    &best_ratio_choice, &best_ratio);
        if (ratio >= min_ratio) {
          return (CogitoDynToneChoice){ (double)tone_i, cmult };
        }
      }
    }
  }

  return best_ratio_choice;
}

// ---- Snap background tones (from DynamicColor.vala) ----
// Avoids tones 50-64 for is_background colors.
static double cogito_dyn_snap_bg(double tone) {
  if (tone >= 57.0) {
    return tone < 65.0 ? 65.0 : tone;
  } else {
    return tone > 49.0 ? 49.0 : tone;
  }
}

// ---- Resolve contrast against a background ----
// Mirrors DynamicColor behavior for increasing/decreasing contrast.
static double cogito_dyn_ensure_contrast(double self_tone, double bg_tone, double ratio, double contrast_level) {
  if (cogito_dyn_ratio_of_tones(bg_tone, self_tone) >= ratio && contrast_level >= 0.0) return self_tone;
  return cogito_dyn_foreground_tone(bg_tone, ratio);
}

// ---- finalize_tone_for_light_on_roles (from DynamicColor.vala) ----
// Forces tone=100 for on_primary, on_secondary, on_tertiary, on_error,
// inverse_on_surface in light mode.
// Caller passes is_forced=true for those roles.
static double cogito_dyn_finalize_light_on(double tone, bool dark, bool is_forced) {
  if (!dark && is_forced) return 100.0;
  return tone;
}

// ---- TDP FARTHER RELATIVE_LIGHTER for accent roles ----
// Given accent's initial tone and container's initial tone, apply TDP.
// Returns adjusted accent tone (container tone is NOT adjusted by TDP).
static double cogito_dyn_tdp_accent(double accent_init, double container_init, double delta, bool dark) {
  // absolute_delta = dark ? -delta : +delta (RELATIVE_LIGHTER)
  // For accent (role_b), relative_delta = absolute_delta * -1
  double rel = dark ? delta : -delta;
  if (rel > 0) {
    // FARTHER: self_tone = max(container_init + rel, accent_init)
    double bound = container_init + rel;
    return accent_init > bound ? accent_init : bound;
  } else {
    // FARTHER: self_tone = min(container_init + rel, accent_init)
    double bound = container_init + rel;
    return accent_init < bound ? accent_init : bound;
  }
}

// ---- Primary role tones (from Scheme.vala) ----

static double cogito_dyn_primary_container_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->primary;
  switch (s->variant) {
    case COGITO_ENSOR_CONTENT: return s->src_t;
    case COGITO_ENSOR_MONOCHROME: return s->dark ? 85.0 : 25.0;
    case COGITO_ENSOR_MUTED: return s->dark ? 30.0 : 90.0;
    case COGITO_ENSOR_DEFAULT:
      return s->dark ? 25.0 : cogito_dyn_tmax_c(p->hue, p->chroma, 0, 100);
    case COGITO_ENSOR_SALAD:
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 30, 93)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 78,
                         cogito_hue_is_cyan(p->hue) ? 88 : 90);
    default: // VIBRANT + others
      return s->dark ? cogito_dyn_tmin_c(p->hue, p->chroma, 66, 93)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 66,
                         cogito_hue_is_cyan(p->hue) ? 88 : 93);
  }
}

static double cogito_dyn_primary_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->primary;
  switch (s->variant) {
    case COGITO_ENSOR_MONOCHROME: return s->dark ? 100.0 : 0.0;
    case COGITO_ENSOR_MUTED: return s->dark ? 80.0 : 40.0;
    case COGITO_ENSOR_DEFAULT: return s->dark ? 80.0 : 40.0;
    case COGITO_ENSOR_SALAD:
      return cogito_dyn_tmax_c(p->hue, p->chroma, 0,
        cogito_hue_is_yellow(p->hue) ? 25 : (cogito_hue_is_cyan(p->hue) ? 88 : 98));
    default: // VIBRANT + others
      return cogito_dyn_tmax_c(p->hue, p->chroma, 0,
        cogito_hue_is_cyan(p->hue) ? 88 : 98);
  }
}

static double cogito_dyn_on_primary_init(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MONOCHROME) return s->dark ? 10.0 : 90.0;
  return s->dark ? 10.0 : 100.0;
}

static double cogito_dyn_on_primary_container_init(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MONOCHROME) return s->dark ? 0.0 : 100.0;
  return s->dark ? 90.0 : 30.0;
}

// ---- Secondary role tones ----

static double cogito_dyn_secondary_container_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->secondary;
  switch (s->variant) {
    case COGITO_ENSOR_MONOCHROME: return s->dark ? 30.0 : 85.0;
    case COGITO_ENSOR_CONTENT: {
      double init = s->dark ? 30.0 : 90.0;
      return cogito_dyn_find_best_tone(p->hue, p->chroma, init, !s->dark);
    }
    case COGITO_ENSOR_DEFAULT: return s->dark ? 25.0 : 90.0;
    case COGITO_ENSOR_MUTED:
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 20, 30)
                     : cogito_dyn_tmin_c(p->hue, p->chroma, 85, 95);
    case COGITO_ENSOR_VIBRANT:
      return s->dark ? cogito_dyn_tmin_c(p->hue, p->chroma, 30, 40)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 84, 90);
    case COGITO_ENSOR_SALAD:
      return s->dark ? 15.0 : cogito_dyn_tmax_c(p->hue, p->chroma, 90, 95);
    default: return s->dark ? 25.0 : 90.0;
  }
}

static double cogito_dyn_secondary_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->secondary;
  switch (s->variant) {
    case COGITO_ENSOR_MONOCHROME: return s->dark ? 80.0 : 40.0;
    case COGITO_ENSOR_DEFAULT:
      return s->dark ? 80.0 : cogito_dyn_tmax_c(p->hue, p->chroma, 0, 100);
    case COGITO_ENSOR_MUTED:
      return s->dark ? cogito_dyn_tmin_c(p->hue, p->chroma, 0, 98)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 0, 100);
    case COGITO_ENSOR_SALAD:
      return cogito_dyn_tmax_c(p->hue, p->chroma, 0, s->dark ? 90 : 98);
    default: // VIBRANT + others
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 75, 85)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 35, 45);
  }
}

static double cogito_dyn_on_secondary_init(const CogitoEnsorDyn* s) {
  (void)s;
  return s->dark ? 10.0 : 100.0;
}

static double cogito_dyn_on_secondary_container_init(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MONOCHROME) return s->dark ? 90.0 : 10.0;
  return s->dark ? 90.0 : 30.0;
}

// ---- Tertiary role tones ----

static double cogito_dyn_tertiary_container_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->tertiary;
  switch (s->variant) {
    case COGITO_ENSOR_MONOCHROME: return s->dark ? 60.0 : 49.0;
    case COGITO_ENSOR_CONTENT: {
      // fix_disliked on tertiary palette at source tone
      double th = p->hue, tc = p->chroma, tt = s->src_t;
      CogitoColor tmp = cogito_hct_to_rgb(th, tc, tt);
      cogito_rgb_to_hct(tmp, &th, &tc, &tt);
      cogito_dyn_fix_disliked(&th, &tc, &tt);
      return tt < 1.0 ? (s->dark ? 30.0 : 90.0) : tt;
    }
    case COGITO_ENSOR_DEFAULT: return s->dark ? 25.0 : 81.0;
    case COGITO_ENSOR_MUTED:
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 0, 93)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 0, 96);
    case COGITO_ENSOR_SALAD:
      return cogito_dyn_tmax_c(p->hue, p->chroma, 75,
        cogito_hue_is_cyan(p->hue) ? 88 : (s->dark ? 93 : 100));
    default: // VIBRANT
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 0, 93)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 72, 100);
  }
}

static double cogito_dyn_tertiary_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->tertiary;
  switch (s->variant) {
    case COGITO_ENSOR_MONOCHROME: return s->dark ? 90.0 : 25.0;
    case COGITO_ENSOR_CONTENT: return s->dark ? 80.0 : 40.0;
    case COGITO_ENSOR_DEFAULT:
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 0, 98)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 0, 100);
    case COGITO_ENSOR_MUTED:
    case COGITO_ENSOR_SALAD:
      return cogito_dyn_tmax_c(p->hue, p->chroma, 0,
        cogito_hue_is_cyan(p->hue) ? 88 : (s->dark ? 98 : 100));
    default: // VIBRANT
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 75, 85)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 35, 45);
  }
}

static double cogito_dyn_on_tertiary_init(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MONOCHROME) return s->dark ? 10.0 : 90.0;
  return s->dark ? 20.0 : 100.0;
}

static double cogito_dyn_on_tertiary_container_init(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MONOCHROME) return s->dark ? 0.0 : 100.0;
  return s->dark ? 90.0 : 30.0;
}

// ---- Error role tones ----

static double cogito_dyn_error_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->error;
  return s->dark ? cogito_dyn_tmin_c(p->hue, p->chroma, 0, 98)
                 : cogito_dyn_tmax_c(p->hue, p->chroma, 0, 100);
}

static double cogito_dyn_error_container_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->error;
  if (s->dark) return cogito_dyn_tmin_c(p->hue, p->chroma, 30, 93);
  if (s->variant == COGITO_ENSOR_DEFAULT) return 90.0;
  return cogito_dyn_tmax_c(p->hue, p->chroma, 0, 90);
}

static double cogito_dyn_on_error_init(const CogitoEnsorDyn* s) {
  return s->dark ? 10.0 : 100.0;
}

// ---- Full accent role resolution ----
// Applies TDP + contrast + snap + finalize for primary/secondary/tertiary/error.
// bg_tone = surface_bright (dark) or surface_dim (light).

typedef struct {
  double accent, container, on_accent, on_container;
} CogitoEnsorRoleTones;

static CogitoEnsorRoleTones cogito_dyn_resolve_accent(
    double accent_init, double container_init,
    double on_accent_init, double on_container_init,
    double bg_tone, bool dark,
    double container_ratio, double accent_ratio,
    double on_accent_ratio, double on_container_ratio,
    bool on_accent_forced_white, double contrast_level)
{
  CogitoEnsorRoleTones r;

  // --- Container: snap for is_background, contrast if ratio > 1 ---
  r.container = cogito_dyn_snap_bg(container_init);
  if (container_ratio > 1.0)
    r.container = cogito_dyn_ensure_contrast(r.container, bg_tone, container_ratio, contrast_level);

  // --- Accent: TDP(container, accent, 5, RELATIVE_LIGHTER, FARTHER) ---
  double acc = cogito_dyn_tdp_accent(accent_init, container_init, 5.0, dark);
  acc = cogito_dyn_ensure_contrast(acc, bg_tone, accent_ratio, contrast_level);
  acc = cogito_dyn_snap_bg(acc);
  r.accent = acc;

  // --- on_accent: contrast against accent ---
  double on_acc = on_accent_init;
  on_acc = cogito_dyn_ensure_contrast(on_acc, r.accent, on_accent_ratio, contrast_level);
  on_acc = cogito_dyn_finalize_light_on(on_acc, dark, on_accent_forced_white);
  r.on_accent = on_acc;

  // --- on_container: contrast against container ---
  double on_cont = on_container_init;
  on_cont = cogito_dyn_ensure_contrast(on_cont, r.container, on_container_ratio, contrast_level);
  r.on_container = on_cont;

  return r;
}

// Special version for Content variant where on_container uses foreground_tone
static CogitoEnsorRoleTones cogito_dyn_resolve_accent_content(
    double accent_init, double container_init,
    double on_accent_init,
    double bg_tone, bool dark,
    double container_ratio, double accent_ratio,
    double on_accent_ratio, double on_container_ratio,
    bool on_accent_forced_white, double contrast_level)
{
  CogitoEnsorRoleTones r;

  // Container: snap, contrast if ratio > 1
  r.container = cogito_dyn_snap_bg(container_init);
  if (container_ratio > 1.0)
    r.container = cogito_dyn_ensure_contrast(r.container, bg_tone, container_ratio, contrast_level);

  // Accent with TDP
  double acc = cogito_dyn_tdp_accent(accent_init, container_init, 5.0, dark);
  acc = cogito_dyn_ensure_contrast(acc, bg_tone, accent_ratio, contrast_level);
  acc = cogito_dyn_snap_bg(acc);
  r.accent = acc;

  // on_accent
  double on_acc = on_accent_init;
  on_acc = cogito_dyn_ensure_contrast(on_acc, r.accent, on_accent_ratio, contrast_level);
  on_acc = cogito_dyn_finalize_light_on(on_acc, dark, on_accent_forced_white);
  r.on_accent = on_acc;

  // on_container: foreground_tone against container
  r.on_container = cogito_dyn_foreground_tone(r.container, on_container_ratio);

  return r;
}

// ---- Main build function ----

static CogitoEnsorScheme cogito_ensor_build_dynamic_scheme_impl(
    bool dark, double h, double c, double t,
    CogitoEnsorVariant variant, double contrast_level, bool include_fixed_roles)
{
  const char* prof_env = getenv("COGITO_ENSOR_PROFILE");
  bool prof = prof_env && prof_env[0] && prof_env[0] != '0';
  if (!prof) {
    const char* startup_env = getenv("COGITO_STARTUP_PROFILE");
    prof = startup_env && startup_env[0] && startup_env[0] != '0';
  }
  cogito_dyn_pick_track_stats = prof;
  double pt0 = prof ? cogito_ensor_prof_now() : 0.0;
  double p_make = pt0, p_primary = pt0, p_secondary = pt0, p_tertiary = pt0;
  double p_error = pt0, p_surface = pt0, p_on_surface = pt0, p_outline = pt0;
  double p_inverse = pt0, p_fixed = pt0, p_end = pt0;
  double p_tertiary_init = pt0, p_tertiary_pick = pt0;
  double p_error_init = pt0, p_error_pick = pt0;
  double p_init_pc = 0.0, p_init_p = 0.0, p_init_op = 0.0, p_init_opc = 0.0;
  double p_init_sc = 0.0, p_init_se = 0.0, p_init_os = 0.0, p_init_osc = 0.0;
  double p_init_tc = 0.0, p_init_te = 0.0, p_init_ot = 0.0, p_init_otc = 0.0;
  double p_init_ec = 0.0, p_init_er = 0.0, p_init_oe = 0.0;
  uint64_t tertiary_pick_eval_delta = 0, tertiary_pick_miss_delta = 0;
  uint64_t error_pick_eval_delta = 0, error_pick_miss_delta = 0;

  CogitoEnsorScheme out;
  memset(&out, 0, sizeof(out));
  out.dark = dark;

  CogitoEnsorDyn s = cogito_dyn_make(h, c, t, dark, contrast_level, variant);
  double cl = s.contrast_level;

  // Evaluate role contrast curves (desktop tracks ColorSpec2025 PHONE behavior).
  double r_container   = (cl > 0.0) ? cogito_contrast_curve_get(cogito_dyn_curve(1.5), cl) : 1.0;
  double r_accent      = cogito_contrast_curve_get(cogito_dyn_curve(4.5), cl);
  double r_on_accent   = cogito_contrast_curve_get(cogito_dyn_curve(6.0), cl);
  double r_on_cont     = cogito_contrast_curve_get(cogito_dyn_curve(6.0), cl);
  double r_on_err_cont = cogito_contrast_curve_get(cogito_dyn_curve(4.5), cl);
  double r_on_surface  = cogito_contrast_curve_get(cogito_dyn_curve(dark ? 11.0 : 9.0), cl);
  double r_on_sv       = cogito_contrast_curve_get(cogito_dyn_curve(dark ? 6.0 : 4.5), cl);
  double r_outline     = cogito_contrast_curve_get(cogito_dyn_curve(3.0), cl);
  double r_outline_v   = cogito_contrast_curve_get(cogito_dyn_curve(1.5), cl);
  double r_inv_on_s    = cogito_contrast_curve_get(cogito_dyn_curve(7.0), cl);
  double r_inv_pri     = cogito_contrast_curve_get(cogito_dyn_curve(6.0), cl);
  p_make = prof ? cogito_ensor_prof_now() : p_make;

  // Background tone for contrast enforcement (DESKTOP):
  // accent roles use surface_bright (dark) / surface_dim (light)
  double bg_tone = dark ? cogito_dyn_surface_bright_tone(&s)
                        : cogito_dyn_surface_dim_tone(&s);

  // ---- Primary ----
  {
    double init_t0 = 0.0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double pc_init  = cogito_dyn_primary_container_init(&s);
    if (prof) p_init_pc += cogito_ensor_prof_now() - init_t0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double p_init   = cogito_dyn_primary_init(&s);
    if (prof) p_init_p += cogito_ensor_prof_now() - init_t0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double op_init  = cogito_dyn_on_primary_init(&s);
    if (prof) p_init_op += cogito_ensor_prof_now() - init_t0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double opc_init = cogito_dyn_on_primary_container_init(&s);
    if (prof) p_init_opc += cogito_ensor_prof_now() - init_t0;

    CogitoEnsorRoleTones rt;
    if (s.variant == COGITO_ENSOR_CONTENT) {
      rt = cogito_dyn_resolve_accent_content(p_init, pc_init, op_init,
        bg_tone, dark, r_container, r_accent, r_on_accent, r_on_cont, true, cl);
    } else {
      rt = cogito_dyn_resolve_accent(p_init, pc_init, op_init, opc_init,
        bg_tone, dark, r_container, r_accent, r_on_accent, r_on_cont, true, cl);
    }

    CogitoColor p_container_color = cogito_dyn_pal_color(s.primary, rt.container, 1.0);
    CogitoDynToneChoice op_cont = cogito_dyn_pick_contrasting_tone(
        s.primary, rt.on_container, p_container_color, r_on_cont);

    out.primary              = cogito_dyn_pal_color(s.primary, rt.accent, 1.0);
    out.primary_container    = p_container_color;
    out.on_primary           = cogito_dyn_pal_color(s.primary, rt.on_accent, 1.0);
    out.on_primary_container = cogito_dyn_pal_color(s.primary, op_cont.tone, op_cont.cmult);
  }
  p_primary = prof ? cogito_ensor_prof_now() : p_primary;

  // ---- Secondary ----
  {
    double init_t0 = 0.0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double sc_init  = cogito_dyn_secondary_container_init(&s);
    if (prof) p_init_sc += cogito_ensor_prof_now() - init_t0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double se_init  = cogito_dyn_secondary_init(&s);
    if (prof) p_init_se += cogito_ensor_prof_now() - init_t0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double os_init  = cogito_dyn_on_secondary_init(&s);
    if (prof) p_init_os += cogito_ensor_prof_now() - init_t0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double osc_init = cogito_dyn_on_secondary_container_init(&s);
    if (prof) p_init_osc += cogito_ensor_prof_now() - init_t0;

    CogitoEnsorRoleTones rt;
    if (s.variant == COGITO_ENSOR_CONTENT) {
      rt = cogito_dyn_resolve_accent_content(se_init, sc_init, os_init,
        bg_tone, dark, r_container, r_accent, r_on_accent, r_on_cont, true, cl);
    } else {
      rt = cogito_dyn_resolve_accent(se_init, sc_init, os_init, osc_init,
        bg_tone, dark, r_container, r_accent, r_on_accent, r_on_cont, true, cl);
    }

    CogitoColor s_container_color = cogito_dyn_pal_color(s.secondary, rt.container, 1.0);
    CogitoDynToneChoice os_cont = cogito_dyn_pick_contrasting_tone(
        s.secondary, rt.on_container, s_container_color, r_on_cont);

    out.secondary              = cogito_dyn_pal_color(s.secondary, rt.accent, 1.0);
    out.secondary_container    = s_container_color;
    out.on_secondary           = cogito_dyn_pal_color(s.secondary, rt.on_accent, 1.0);
    out.on_secondary_container = cogito_dyn_pal_color(s.secondary, os_cont.tone, os_cont.cmult);
  }
  p_secondary = prof ? cogito_ensor_prof_now() : p_secondary;

  // ---- Tertiary ----
  {
    double init_t0 = 0.0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double tc_init  = cogito_dyn_tertiary_container_init(&s);
    if (prof) p_init_tc += cogito_ensor_prof_now() - init_t0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double te_init  = cogito_dyn_tertiary_init(&s);
    if (prof) p_init_te += cogito_ensor_prof_now() - init_t0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double ot_init  = cogito_dyn_on_tertiary_init(&s);
    if (prof) p_init_ot += cogito_ensor_prof_now() - init_t0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double otc_init = cogito_dyn_on_tertiary_container_init(&s);
    if (prof) p_init_otc += cogito_ensor_prof_now() - init_t0;

    CogitoEnsorRoleTones rt;
    if (s.variant == COGITO_ENSOR_CONTENT) {
      rt = cogito_dyn_resolve_accent_content(te_init, tc_init, ot_init,
        bg_tone, dark, r_container, r_accent, r_on_accent, r_on_cont, true, cl);
    } else {
      rt = cogito_dyn_resolve_accent(te_init, tc_init, ot_init, otc_init,
        bg_tone, dark, r_container, r_accent, r_on_accent, r_on_cont, true, cl);
    }
    p_tertiary_init = prof ? cogito_ensor_prof_now() : p_tertiary_init;

    CogitoColor t_container_color = cogito_dyn_pal_color(s.tertiary, rt.container, 1.0);
    uint64_t tertiary_pick_eval_before = cogito_dyn_pick_eval_count;
    uint64_t tertiary_pick_miss_before = cogito_dyn_pick_miss_count;
    CogitoDynToneChoice ot_cont = cogito_dyn_pick_contrasting_tone(
        s.tertiary, rt.on_container, t_container_color, r_on_cont);
    tertiary_pick_eval_delta = cogito_dyn_pick_eval_count - tertiary_pick_eval_before;
    tertiary_pick_miss_delta = cogito_dyn_pick_miss_count - tertiary_pick_miss_before;
    p_tertiary_pick = prof ? cogito_ensor_prof_now() : p_tertiary_pick;

    out.tertiary              = cogito_dyn_pal_color(s.tertiary, rt.accent, 1.0);
    out.tertiary_container    = t_container_color;
    out.on_tertiary           = cogito_dyn_pal_color(s.tertiary, rt.on_accent, 1.0);
    out.on_tertiary_container = cogito_dyn_pal_color(s.tertiary, ot_cont.tone, ot_cont.cmult);
  }
  p_tertiary = prof ? cogito_ensor_prof_now() : p_tertiary;

  // ---- Error ----
  {
    double init_t0 = 0.0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double ec_init = cogito_dyn_error_container_init(&s);
    if (prof) p_init_ec += cogito_ensor_prof_now() - init_t0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double er_init = cogito_dyn_error_init(&s);
    if (prof) p_init_er += cogito_ensor_prof_now() - init_t0;
    if (prof) init_t0 = cogito_ensor_prof_now();
    double oe_init = cogito_dyn_on_error_init(&s);
    if (prof) p_init_oe += cogito_ensor_prof_now() - init_t0;

    // error_container: snap + contrast if ratio > 1
    double ec_tone = cogito_dyn_snap_bg(ec_init);
    if (r_container > 1.0)
      ec_tone = cogito_dyn_ensure_contrast(ec_tone, bg_tone, r_container, cl);

    // error: TDP(error_container, error, 5, RELATIVE_LIGHTER, FARTHER)
    double er_tone = cogito_dyn_tdp_accent(er_init, ec_init, 5.0, dark);
    er_tone = cogito_dyn_ensure_contrast(er_tone, bg_tone, r_accent, cl);
    er_tone = cogito_dyn_snap_bg(er_tone);

    // on_error: contrast against error
    double oe_tone = oe_init;
    oe_tone = cogito_dyn_ensure_contrast(oe_tone, er_tone, r_on_accent, cl);
    oe_tone = cogito_dyn_finalize_light_on(oe_tone, dark, true);
    p_error_init = prof ? cogito_ensor_prof_now() : p_error_init;

    // on_error_container: derive from the realized container tone so
    // gamut clipping on error_container does not collapse contrast.
    CogitoColor ec_color = cogito_dyn_pal_color(s.error, ec_tone, 1.0);
    double ec_real_tone = cogito_dyn_tone_of_rgb(ec_color);
    double oec_tone = cogito_dyn_foreground_tone(ec_real_tone, r_on_err_cont);
    oec_tone = cogito_dyn_ensure_contrast(oec_tone, ec_real_tone, r_on_err_cont, cl);
    uint64_t error_pick_eval_before = cogito_dyn_pick_eval_count;
    uint64_t error_pick_miss_before = cogito_dyn_pick_miss_count;
    CogitoDynToneChoice oec_choice = cogito_dyn_pick_contrasting_tone(
        s.error, oec_tone, ec_color, r_on_err_cont);
    error_pick_eval_delta = cogito_dyn_pick_eval_count - error_pick_eval_before;
    error_pick_miss_delta = cogito_dyn_pick_miss_count - error_pick_miss_before;
    p_error_pick = prof ? cogito_ensor_prof_now() : p_error_pick;

    out.error              = cogito_dyn_pal_color(s.error, er_tone, 1.0);
    out.error_container    = ec_color;
    out.on_error           = cogito_dyn_pal_color(s.error, oe_tone, 1.0);
    out.on_error_container = cogito_dyn_pal_color(s.error, oec_choice.tone, oec_choice.cmult);
  }
  p_error = prof ? cogito_ensor_prof_now() : p_error;

  // Light mode contract: on-accent foreground roles are pure white.
  // Keep container "on" roles dynamic; only force non-container roles.
  if (!dark) {
    CogitoColor white = cogito_rgba(255, 255, 255, 255);
    out.on_primary = white;
    out.on_secondary = white;
    out.on_tertiary = white;
    out.on_error = white;
  }

  // ---- Surface roles (all use neutral palette, no contrast curves) ----
  {
    double surf_t     = cogito_dyn_surface_tone(&s);
    double sbright_t  = cogito_dyn_surface_bright_tone(&s);
    double sdim_t     = cogito_dyn_surface_dim_tone(&s);
    double scl_t      = cogito_dyn_sc_lowest_tone(&s);
    double sclo_t     = cogito_dyn_sc_low_tone(&s);
    double sc_t       = cogito_dyn_sc_tone(&s);
    double sch_t      = cogito_dyn_sc_high_tone(&s);
    double schi_t     = cogito_dyn_sc_highest_tone(&s);

    out.surface                  = cogito_dyn_pal_color(s.neutral, surf_t, 1.0);
    out.surface_bright           = cogito_dyn_pal_color(s.neutral, sbright_t, cogito_dyn_sbright_cmult(&s));
    out.surface_dim              = cogito_dyn_pal_color(s.neutral, sdim_t, cogito_dyn_sdim_cmult(&s));
    out.surface_container_lowest = cogito_dyn_pal_color(s.neutral, scl_t, 1.0);
    out.surface_container_low    = cogito_dyn_pal_color(s.neutral, sclo_t, cogito_dyn_scl_cmult(&s));
    out.surface_container        = cogito_dyn_pal_color(s.neutral, sc_t, cogito_dyn_sc_cmult(&s));
    out.surface_container_high   = cogito_dyn_pal_color(s.neutral, sch_t, cogito_dyn_sch_cmult(&s));
    out.surface_container_highest= cogito_dyn_pal_color(s.neutral, schi_t, cogito_dyn_schi_cmult(&s));

    out.background    = out.surface;
    out.surface_variant = out.surface_container_highest;
  }
  p_surface = prof ? cogito_ensor_prof_now() : p_surface;

  // ---- on_surface (neutral palette, contrast against surface_bright/dim) ----
  {
    double neut_cm = cogito_dyn_neut_cmult(&s);
    double init_tone;
    if (s.variant == COGITO_ENSOR_VIBRANT) {
      init_tone = cogito_dyn_tmax_c_m(s.neutral.hue, s.neutral.chroma, 0, 100, 1.1);
    } else {
      init_tone = bg_tone;
    }
    double ons_tone = cogito_dyn_ensure_contrast(init_tone, bg_tone, r_on_surface, cl);
    out.on_surface = cogito_dyn_pal_color(s.neutral, ons_tone, neut_cm);
    out.on_background = out.on_surface;
  }
  p_on_surface = prof ? cogito_ensor_prof_now() : p_on_surface;

  // ---- on_surface_variant ----
  {
    double neut_cm = cogito_dyn_neut_cmult(&s);
    double osv_tone = cogito_dyn_ensure_contrast(bg_tone, bg_tone, r_on_sv, cl);
    out.on_surface_variant = cogito_dyn_pal_color(s.neutral, osv_tone, neut_cm);
  }

  // ---- outline ----
  {
    double neut_cm = cogito_dyn_neut_cmult(&s);
    double out_tone = cogito_dyn_ensure_contrast(bg_tone, bg_tone, r_outline, cl);
    out.outline = cogito_dyn_pal_color(s.neutral, out_tone, neut_cm);
  }

  // ---- outline_variant ----
  {
    double neut_cm = cogito_dyn_neut_cmult(&s);
    double outv_tone = cogito_dyn_ensure_contrast(bg_tone, bg_tone, r_outline_v, cl);
    out.outline_variant = cogito_dyn_pal_color(s.neutral, outv_tone, neut_cm);
  }
  p_outline = prof ? cogito_ensor_prof_now() : p_outline;

  // ---- inverse_surface (neutral, fixed tones, no contrast curve) ----
  {
    double inv_tone = dark ? 98.0 : 4.0;
    out.inverse_surface = cogito_dyn_pal_color(s.neutral, inv_tone, 1.0);
  }

  // ---- inverse_on_surface ----
  {
    double inv_bg = dark ? 98.0 : 4.0;
    double inv_ons = cogito_dyn_ensure_contrast(inv_bg, inv_bg, r_inv_on_s, cl);
    inv_ons = cogito_dyn_finalize_light_on(inv_ons, dark, true);
    out.inverse_on_surface = cogito_dyn_pal_color(s.neutral, inv_ons, 1.0);
  }

  // ---- inverse_primary ----
  {
    double inv_bg = dark ? 98.0 : 4.0;
    double ip_init = cogito_dyn_tmax_c(s.primary.hue, s.primary.chroma, 0, 100);
    double ip_tone = cogito_dyn_ensure_contrast(ip_init, inv_bg, r_inv_pri, cl);
    out.inverse_primary = cogito_dyn_pal_color(s.primary, ip_tone, 1.0);
  }
  p_inverse = prof ? cogito_ensor_prof_now() : p_inverse;

  // ---- Fixed accent roles (primary/secondary/tertiary) ----
  if (include_fixed_roles) {
    CogitoEnsorScheme light_base;
    if (!dark) {
      // In light mode, fixed roles are derived from the current realized
      // container colors, so avoid re-running a full secondary scheme build.
      light_base = out;
    } else {
      light_base = cogito_ensor_build_dynamic_scheme_impl(
          false, h, c, t, variant, 0.0, false);
    }

    double r_on_fixed = cogito_contrast_curve_get(cogito_dyn_curve(7.0), cl);
    double r_on_fixed_variant = cogito_contrast_curve_get(cogito_dyn_curve(4.5), cl);

    double pf_tone = cogito_dyn_tone_of_rgb(light_base.primary_container);
    double pfd_tone = fmax(0.0, pf_tone - 10.0);
    CogitoColor pf_color = cogito_dyn_pal_color(s.primary, pf_tone, 1.0);
    CogitoColor pfd_color = cogito_dyn_pal_color(s.primary, pfd_tone, 1.0);
    double opf_tone = cogito_dyn_ensure_contrast(pf_tone, pf_tone, r_on_fixed, cl);
    double opfv_tone = cogito_dyn_ensure_contrast(pfd_tone, pfd_tone, r_on_fixed_variant, cl);
    CogitoDynToneChoice opf_choice = cogito_dyn_pick_contrasting_tone(
        s.primary, opf_tone, pf_color, r_on_fixed);
    CogitoDynToneChoice opfv_choice = cogito_dyn_pick_contrasting_tone(
        s.primary, opfv_tone, pfd_color, r_on_fixed_variant);
    out.primary_fixed = pf_color;
    out.primary_fixed_dim = pfd_color;
    out.on_primary_fixed = cogito_dyn_pal_color(s.primary, opf_choice.tone, opf_choice.cmult);
    out.on_primary_fixed_variant = cogito_dyn_pal_color(s.primary, opfv_choice.tone, opfv_choice.cmult);

    double sf_tone = cogito_dyn_tone_of_rgb(light_base.secondary_container);
    double sfd_tone = fmax(0.0, sf_tone - 10.0);
    CogitoColor sf_color = cogito_dyn_pal_color(s.secondary, sf_tone, 1.0);
    CogitoColor sfd_color = cogito_dyn_pal_color(s.secondary, sfd_tone, 1.0);
    double osf_tone = cogito_dyn_ensure_contrast(sf_tone, sf_tone, r_on_fixed, cl);
    double osfv_tone = cogito_dyn_ensure_contrast(sfd_tone, sfd_tone, r_on_fixed_variant, cl);
    CogitoDynToneChoice osf_choice = cogito_dyn_pick_contrasting_tone(
        s.secondary, osf_tone, sf_color, r_on_fixed);
    CogitoDynToneChoice osfv_choice = cogito_dyn_pick_contrasting_tone(
        s.secondary, osfv_tone, sfd_color, r_on_fixed_variant);
    out.secondary_fixed = sf_color;
    out.secondary_fixed_dim = sfd_color;
    out.on_secondary_fixed = cogito_dyn_pal_color(s.secondary, osf_choice.tone, osf_choice.cmult);
    out.on_secondary_fixed_variant = cogito_dyn_pal_color(s.secondary, osfv_choice.tone, osfv_choice.cmult);

    double tf_tone = cogito_dyn_tone_of_rgb(light_base.tertiary_container);
    double tfd_tone = fmax(0.0, tf_tone - 10.0);
    CogitoColor tf_color = cogito_dyn_pal_color(s.tertiary, tf_tone, 1.0);
    CogitoColor tfd_color = cogito_dyn_pal_color(s.tertiary, tfd_tone, 1.0);
    double otf_tone = cogito_dyn_ensure_contrast(tf_tone, tf_tone, r_on_fixed, cl);
    double otfv_tone = cogito_dyn_ensure_contrast(tfd_tone, tfd_tone, r_on_fixed_variant, cl);
    CogitoDynToneChoice otf_choice = cogito_dyn_pick_contrasting_tone(
        s.tertiary, otf_tone, tf_color, r_on_fixed);
    CogitoDynToneChoice otfv_choice = cogito_dyn_pick_contrasting_tone(
        s.tertiary, otfv_tone, tfd_color, r_on_fixed_variant);
    out.tertiary_fixed = tf_color;
    out.tertiary_fixed_dim = tfd_color;
    out.on_tertiary_fixed = cogito_dyn_pal_color(s.tertiary, otf_choice.tone, otf_choice.cmult);
    out.on_tertiary_fixed_variant = cogito_dyn_pal_color(s.tertiary, otfv_choice.tone, otfv_choice.cmult);
  }
  p_fixed = prof ? cogito_ensor_prof_now() : p_fixed;

  // ---- surface_tint = primary color (same as primary) ----
  out.surface_tint = out.primary;

  // ---- shadow / scrim ----
  out.shadow = cogito_hct_to_rgb(s.neutral.hue, 0.0, 0.0);
  out.scrim  = cogito_hct_to_rgb(s.neutral.hue, 0.0, 0.0);
  p_end = prof ? cogito_ensor_prof_now() : p_end;

  if (prof) {
    fprintf(stderr,
            "[cogito-ensor] total=%.2fms make=%.2fms primary=%.2fms secondary=%.2fms tertiary=%.2fms tertiary_init=%.2fms tertiary_pick=%.2fms tertiary_pick_evals=%llu tertiary_pick_misses=%llu error=%.2fms error_init=%.2fms error_pick=%.2fms error_pick_evals=%llu error_pick_misses=%llu surface=%.2fms on_surface=%.2fms outline=%.2fms inverse=%.2fms fixed=%.2fms tail=%.2fms\n",
            (p_end - pt0) * 1000.0,
            (p_make - pt0) * 1000.0,
            (p_primary - p_make) * 1000.0,
            (p_secondary - p_primary) * 1000.0,
            (p_tertiary - p_secondary) * 1000.0,
            (p_tertiary_init - p_secondary) * 1000.0,
            (p_tertiary_pick - p_tertiary_init) * 1000.0,
            (unsigned long long)tertiary_pick_eval_delta,
            (unsigned long long)tertiary_pick_miss_delta,
            (p_error - p_tertiary) * 1000.0,
            (p_error_init - p_tertiary) * 1000.0,
            (p_error_pick - p_error_init) * 1000.0,
            (unsigned long long)error_pick_eval_delta,
            (unsigned long long)error_pick_miss_delta,
            (p_surface - p_error) * 1000.0,
            (p_on_surface - p_surface) * 1000.0,
            (p_outline - p_on_surface) * 1000.0,
            (p_inverse - p_outline) * 1000.0,
            (p_fixed - p_inverse) * 1000.0,
            (p_end - p_fixed) * 1000.0);
    fprintf(stderr,
            "[cogito-ensor-init] pc=%.3fms p=%.3fms op=%.3fms opc=%.3fms sc=%.3fms se=%.3fms os=%.3fms osc=%.3fms tc=%.3fms te=%.3fms ot=%.3fms otc=%.3fms ec=%.3fms er=%.3fms oe=%.3fms\n",
            p_init_pc * 1000.0,
            p_init_p * 1000.0,
            p_init_op * 1000.0,
            p_init_opc * 1000.0,
            p_init_sc * 1000.0,
            p_init_se * 1000.0,
            p_init_os * 1000.0,
            p_init_osc * 1000.0,
            p_init_tc * 1000.0,
            p_init_te * 1000.0,
            p_init_ot * 1000.0,
            p_init_otc * 1000.0,
            p_init_ec * 1000.0,
            p_init_er * 1000.0,
            p_init_oe * 1000.0);
    fflush(stderr);
  }

  return out;
}

static CogitoEnsorScheme cogito_ensor_build_dynamic_scheme(
    bool dark, double h, double c, double t,
    CogitoEnsorVariant variant, double contrast_level)
{
  return cogito_ensor_build_dynamic_scheme_impl(dark, h, c, t, variant, contrast_level, true);
}
