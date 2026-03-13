// Centralized theme color utilities
// Provides consistent access to Ensor scheme colors for widgets that need
// colors not directly available through the SUM stylesheet system.

// Cached tertiary colors - populated when theme is applied via SUM path
// This avoids building Ensor schemes at draw time which can cause timing issues
static CogitoColor cogito_cached_primary = {0};
static CogitoColor cogito_cached_on_primary = {0};
static CogitoColor cogito_cached_primary_container = {0};
static CogitoColor cogito_cached_on_primary_container = {0};
static CogitoColor cogito_cached_secondary = {0};
static CogitoColor cogito_cached_on_secondary = {0};
static CogitoColor cogito_cached_secondary_container = {0};
static CogitoColor cogito_cached_on_secondary_container = {0};
static CogitoColor cogito_cached_tertiary = {0};
static CogitoColor cogito_cached_on_tertiary = {0};
static CogitoColor cogito_cached_tertiary_container = {0};
static CogitoColor cogito_cached_on_tertiary_container = {0};
static CogitoColor cogito_cached_background = {0};
static CogitoColor cogito_cached_on_background = {0};
static CogitoColor cogito_cached_surface = {0};
static CogitoColor cogito_cached_on_surface = {0};
static CogitoColor cogito_cached_surface_tint = {0};
static CogitoColor cogito_cached_surface_container_lowest = {0};
static CogitoColor cogito_cached_surface_container_low = {0};
static CogitoColor cogito_cached_surface_container = {0};
static CogitoColor cogito_cached_surface_container_high = {0};
static CogitoColor cogito_cached_surface_container_highest = {0};
static CogitoColor cogito_cached_surface_variant = {0};
static CogitoColor cogito_cached_on_surface_variant = {0};
static CogitoColor cogito_cached_outline = {0};
static CogitoColor cogito_cached_error = {0};
static CogitoColor cogito_cached_on_error = {0};
static CogitoColor cogito_cached_error_container = {0};
static CogitoColor cogito_cached_on_error_container = {0};
static CogitoColor cogito_cached_outline_variant = {0};
static CogitoColor cogito_cached_inverse_surface = {0};
static CogitoColor cogito_cached_inverse_on_surface = {0};
static CogitoColor cogito_cached_inverse_primary = {0};
static CogitoColor cogito_cached_close = {0};
static CogitoColor cogito_cached_minimize = {0};
static CogitoColor cogito_cached_maximize = {0};
static CogitoColor cogito_cached_on_close = {0};
static CogitoColor cogito_cached_on_minimize = {0};
static CogitoColor cogito_cached_on_maximize = {0};

typedef struct CogitoThemePaletteSnapshot {
  CogitoColor primary;
  CogitoColor on_primary;
  CogitoColor primary_container;
  CogitoColor on_primary_container;
  CogitoColor secondary;
  CogitoColor on_secondary;
  CogitoColor secondary_container;
  CogitoColor on_secondary_container;
  CogitoColor tertiary;
  CogitoColor on_tertiary;
  CogitoColor tertiary_container;
  CogitoColor on_tertiary_container;
  CogitoColor background;
  CogitoColor on_background;
  CogitoColor surface;
  CogitoColor on_surface;
  CogitoColor surface_tint;
  CogitoColor surface_container_lowest;
  CogitoColor surface_container_low;
  CogitoColor surface_container;
  CogitoColor surface_container_high;
  CogitoColor surface_container_highest;
  CogitoColor surface_variant;
  CogitoColor on_surface_variant;
  CogitoColor outline;
  CogitoColor error;
  CogitoColor on_error;
  CogitoColor error_container;
  CogitoColor on_error_container;
  CogitoColor outline_variant;
  CogitoColor inverse_surface;
  CogitoColor inverse_on_surface;
  CogitoColor inverse_primary;
  CogitoColor close;
  CogitoColor minimize;
  CogitoColor maximize;
  CogitoColor on_close;
  CogitoColor on_minimize;
  CogitoColor on_maximize;
} CogitoThemePaletteSnapshot;

#define COGITO_THEME_PALETTE_SCOPE_MAX 16
static CogitoThemePaletteSnapshot cogito_theme_palette_scope_stack[COGITO_THEME_PALETTE_SCOPE_MAX];
static int cogito_theme_palette_scope_depth = 0;

static void cogito_theme_palette_snapshot_capture(CogitoThemePaletteSnapshot* out) {
  if (!out) return;
  out->primary = cogito_cached_primary;
  out->on_primary = cogito_cached_on_primary;
  out->primary_container = cogito_cached_primary_container;
  out->on_primary_container = cogito_cached_on_primary_container;
  out->secondary = cogito_cached_secondary;
  out->on_secondary = cogito_cached_on_secondary;
  out->secondary_container = cogito_cached_secondary_container;
  out->on_secondary_container = cogito_cached_on_secondary_container;
  out->tertiary = cogito_cached_tertiary;
  out->on_tertiary = cogito_cached_on_tertiary;
  out->tertiary_container = cogito_cached_tertiary_container;
  out->on_tertiary_container = cogito_cached_on_tertiary_container;
  out->background = cogito_cached_background;
  out->on_background = cogito_cached_on_background;
  out->surface = cogito_cached_surface;
  out->on_surface = cogito_cached_on_surface;
  out->surface_tint = cogito_cached_surface_tint;
  out->surface_container_lowest = cogito_cached_surface_container_lowest;
  out->surface_container_low = cogito_cached_surface_container_low;
  out->surface_container = cogito_cached_surface_container;
  out->surface_container_high = cogito_cached_surface_container_high;
  out->surface_container_highest = cogito_cached_surface_container_highest;
  out->surface_variant = cogito_cached_surface_variant;
  out->on_surface_variant = cogito_cached_on_surface_variant;
  out->outline = cogito_cached_outline;
  out->error = cogito_cached_error;
  out->on_error = cogito_cached_on_error;
  out->error_container = cogito_cached_error_container;
  out->on_error_container = cogito_cached_on_error_container;
  out->outline_variant = cogito_cached_outline_variant;
  out->inverse_surface = cogito_cached_inverse_surface;
  out->inverse_on_surface = cogito_cached_inverse_on_surface;
  out->inverse_primary = cogito_cached_inverse_primary;
  out->close = cogito_cached_close;
  out->minimize = cogito_cached_minimize;
  out->maximize = cogito_cached_maximize;
  out->on_close = cogito_cached_on_close;
  out->on_minimize = cogito_cached_on_minimize;
  out->on_maximize = cogito_cached_on_maximize;
}

static void cogito_theme_palette_snapshot_apply(const CogitoThemePaletteSnapshot* snapshot) {
  if (!snapshot) return;
  cogito_cached_primary = snapshot->primary;
  cogito_cached_on_primary = snapshot->on_primary;
  cogito_cached_primary_container = snapshot->primary_container;
  cogito_cached_on_primary_container = snapshot->on_primary_container;
  cogito_cached_secondary = snapshot->secondary;
  cogito_cached_on_secondary = snapshot->on_secondary;
  cogito_cached_secondary_container = snapshot->secondary_container;
  cogito_cached_on_secondary_container = snapshot->on_secondary_container;
  cogito_cached_tertiary = snapshot->tertiary;
  cogito_cached_on_tertiary = snapshot->on_tertiary;
  cogito_cached_tertiary_container = snapshot->tertiary_container;
  cogito_cached_on_tertiary_container = snapshot->on_tertiary_container;
  cogito_cached_background = snapshot->background;
  cogito_cached_on_background = snapshot->on_background;
  cogito_cached_surface = snapshot->surface;
  cogito_cached_on_surface = snapshot->on_surface;
  cogito_cached_surface_tint = snapshot->surface_tint;
  cogito_cached_surface_container_lowest = snapshot->surface_container_lowest;
  cogito_cached_surface_container_low = snapshot->surface_container_low;
  cogito_cached_surface_container = snapshot->surface_container;
  cogito_cached_surface_container_high = snapshot->surface_container_high;
  cogito_cached_surface_container_highest = snapshot->surface_container_highest;
  cogito_cached_surface_variant = snapshot->surface_variant;
  cogito_cached_on_surface_variant = snapshot->on_surface_variant;
  cogito_cached_outline = snapshot->outline;
  cogito_cached_error = snapshot->error;
  cogito_cached_on_error = snapshot->on_error;
  cogito_cached_error_container = snapshot->error_container;
  cogito_cached_on_error_container = snapshot->on_error_container;
  cogito_cached_outline_variant = snapshot->outline_variant;
  cogito_cached_inverse_surface = snapshot->inverse_surface;
  cogito_cached_inverse_on_surface = snapshot->inverse_on_surface;
  cogito_cached_inverse_primary = snapshot->inverse_primary;
  cogito_cached_close = snapshot->close;
  cogito_cached_minimize = snapshot->minimize;
  cogito_cached_maximize = snapshot->maximize;
  cogito_cached_on_close = snapshot->on_close;
  cogito_cached_on_minimize = snapshot->on_minimize;
  cogito_cached_on_maximize = snapshot->on_maximize;
}

static void cogito_theme_palette_apply_scheme(const CogitoEnsorScheme* scheme) {
  if (!scheme) return;
  cogito_cached_primary = scheme->primary;
  cogito_cached_on_primary = scheme->on_primary;
  cogito_cached_primary_container = scheme->primary_container;
  cogito_cached_on_primary_container = scheme->on_primary_container;
  cogito_cached_secondary = scheme->secondary;
  cogito_cached_on_secondary = scheme->on_secondary;
  cogito_cached_secondary_container = scheme->secondary_container;
  cogito_cached_on_secondary_container = scheme->on_secondary_container;
  cogito_cached_tertiary = scheme->tertiary;
  cogito_cached_on_tertiary = scheme->on_tertiary;
  cogito_cached_tertiary_container = scheme->tertiary_container;
  cogito_cached_on_tertiary_container = scheme->on_tertiary_container;
  cogito_cached_background = scheme->background;
  cogito_cached_on_background = scheme->on_background;
  cogito_cached_surface = scheme->surface;
  cogito_cached_on_surface = scheme->on_surface;
  cogito_cached_surface_tint = scheme->surface_tint;
  cogito_cached_surface_container_lowest = scheme->surface_container_lowest;
  cogito_cached_surface_container_low = scheme->surface_container_low;
  cogito_cached_surface_container = scheme->surface_container;
  cogito_cached_surface_container_high = scheme->surface_container_high;
  cogito_cached_surface_container_highest = scheme->surface_container_highest;
  cogito_cached_surface_variant = scheme->surface_variant;
  cogito_cached_on_surface_variant = scheme->on_surface_variant;
  cogito_cached_outline = scheme->outline;
  cogito_cached_error = scheme->error;
  cogito_cached_on_error = scheme->on_error;
  cogito_cached_error_container = scheme->error_container;
  cogito_cached_on_error_container = scheme->on_error_container;
  cogito_cached_outline_variant = scheme->outline_variant;
  cogito_cached_inverse_surface = scheme->inverse_surface;
  cogito_cached_inverse_on_surface = scheme->inverse_on_surface;
  cogito_cached_inverse_primary = scheme->inverse_primary;
  cogito_cached_close = scheme->close;
  cogito_cached_minimize = scheme->minimize;
  cogito_cached_maximize = scheme->maximize;
  cogito_cached_on_close = scheme->on_close;
  cogito_cached_on_minimize = scheme->on_minimize;
  cogito_cached_on_maximize = scheme->on_maximize;
}

static bool cogito_theme_palette_scope_push_scheme(const CogitoEnsorScheme* scheme) {
  if (!scheme) return false;
  if (cogito_theme_palette_scope_depth >= COGITO_THEME_PALETTE_SCOPE_MAX) return false;
  cogito_theme_palette_snapshot_capture(&cogito_theme_palette_scope_stack[cogito_theme_palette_scope_depth]);
  cogito_theme_palette_scope_depth++;
  cogito_theme_palette_apply_scheme(scheme);
  return true;
}

static void cogito_theme_palette_scope_pop(void) {
  if (cogito_theme_palette_scope_depth <= 0) return;
  cogito_theme_palette_scope_depth--;
  cogito_theme_palette_snapshot_apply(&cogito_theme_palette_scope_stack[cogito_theme_palette_scope_depth]);
}

// Called from sum_build_dynamic when theme is applied
// Stores tertiary colors for later use by widgets
static void cogito_cache_tertiary_colors(CogitoEnsorScheme* scheme) {
  if (!scheme) return;
  cogito_cached_primary = scheme->primary;
  cogito_cached_on_primary = scheme->on_primary;
  cogito_cached_primary_container = scheme->primary_container;
  cogito_cached_on_primary_container = scheme->on_primary_container;
  cogito_cached_secondary = scheme->secondary;
  cogito_cached_on_secondary = scheme->on_secondary;
  cogito_cached_secondary_container = scheme->secondary_container;
  cogito_cached_on_secondary_container = scheme->on_secondary_container;
  cogito_cached_tertiary = scheme->tertiary;
  cogito_cached_on_tertiary = scheme->on_tertiary;
  cogito_cached_tertiary_container = scheme->tertiary_container;
  cogito_cached_on_tertiary_container = scheme->on_tertiary_container;
  cogito_cached_background = scheme->background;
  cogito_cached_on_background = scheme->on_background;
  cogito_cached_surface = scheme->surface;
  cogito_cached_on_surface = scheme->on_surface;
  cogito_cached_surface_tint = scheme->surface_tint;
  cogito_cached_surface_container_lowest = scheme->surface_container_lowest;
  cogito_cached_surface_container_low = scheme->surface_container_low;
  cogito_cached_surface_container = scheme->surface_container;
  cogito_cached_surface_container_high = scheme->surface_container_high;
  cogito_cached_surface_container_highest = scheme->surface_container_highest;
  cogito_cached_surface_variant = scheme->surface_variant;
  cogito_cached_on_surface_variant = scheme->on_surface_variant;
  cogito_cached_outline = scheme->outline;
  cogito_cached_error = scheme->error;
  cogito_cached_on_error = scheme->on_error;
  cogito_cached_error_container = scheme->error_container;
  cogito_cached_on_error_container = scheme->on_error_container;
  cogito_cached_outline_variant = scheme->outline_variant;
  cogito_cached_inverse_surface = scheme->inverse_surface;
  cogito_cached_inverse_on_surface = scheme->inverse_on_surface;
  cogito_cached_inverse_primary = scheme->inverse_primary;
  cogito_cached_close = scheme->close;
  cogito_cached_minimize = scheme->minimize;
  cogito_cached_maximize = scheme->maximize;
  cogito_cached_on_close = scheme->on_close;
  cogito_cached_on_minimize = scheme->on_minimize;
  cogito_cached_on_maximize = scheme->on_maximize;
}

// Get tertiary colors from cached values (set when theme was applied).
// Uses the SUM path - colors are pre-computed, not built at draw time.
// Any output parameter can be NULL if not needed.
static void cogito_theme_get_tertiary(
  CogitoColor* out_tertiary,
  CogitoColor* out_on_tertiary,
  CogitoColor* out_tertiary_container,
  CogitoColor* out_on_tertiary_container
) {
  if (out_tertiary) *out_tertiary = cogito_cached_tertiary;
  if (out_on_tertiary) *out_on_tertiary = cogito_cached_on_tertiary;
  if (out_tertiary_container) *out_tertiary_container = cogito_cached_tertiary_container;
  if (out_on_tertiary_container) *out_on_tertiary_container = cogito_cached_on_tertiary_container;
}

// Get state-layer source colors from the cached Ensor scheme.
static void cogito_theme_get_state_layer_palette(
  CogitoColor* out_primary,
  CogitoColor* out_primary_container,
  CogitoColor* out_on_primary_container,
  CogitoColor* out_secondary,
  CogitoColor* out_secondary_container,
  CogitoColor* out_on_secondary_container,
  CogitoColor* out_tertiary,
  CogitoColor* out_tertiary_container,
  CogitoColor* out_on_tertiary_container,
  CogitoColor* out_surface,
  CogitoColor* out_surface_container,
  CogitoColor* out_on_surface,
  CogitoColor* out_on_surface_variant
) {
  if (out_primary) *out_primary = cogito_cached_primary;
  if (out_primary_container) *out_primary_container = cogito_cached_primary_container;
  if (out_on_primary_container) *out_on_primary_container = cogito_cached_on_primary_container;
  if (out_secondary) *out_secondary = cogito_cached_secondary;
  if (out_secondary_container) *out_secondary_container = cogito_cached_secondary_container;
  if (out_on_secondary_container) *out_on_secondary_container = cogito_cached_on_secondary_container;
  if (out_tertiary) *out_tertiary = cogito_cached_tertiary;
  if (out_tertiary_container) *out_tertiary_container = cogito_cached_tertiary_container;
  if (out_on_tertiary_container) *out_on_tertiary_container = cogito_cached_on_tertiary_container;
  if (out_surface) *out_surface = cogito_cached_surface;
  if (out_surface_container) *out_surface_container = cogito_cached_surface_container;
  if (out_on_surface) *out_on_surface = cogito_cached_on_surface;
  if (out_on_surface_variant) *out_on_surface_variant = cogito_cached_on_surface_variant;
}
