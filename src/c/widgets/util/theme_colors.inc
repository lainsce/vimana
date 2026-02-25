// Centralized theme color utilities
// Provides consistent access to Ensor scheme colors for widgets that need
// colors not directly available through the SUM stylesheet system.

// Cached tertiary colors - populated when theme is applied via SUM path
// This avoids building Ensor schemes at draw time which can cause timing issues
static CogitoColor cogito_cached_primary = {0};
static CogitoColor cogito_cached_secondary = {0};
static CogitoColor cogito_cached_secondary_container = {0};
static CogitoColor cogito_cached_tertiary = {0};
static CogitoColor cogito_cached_surface = {0};
static CogitoColor cogito_cached_on_surface = {0};
static CogitoColor cogito_cached_on_primary_container = {0};
static CogitoColor cogito_cached_on_secondary_container = {0};
static CogitoColor cogito_cached_on_tertiary = {0};
static CogitoColor cogito_cached_tertiary_container = {0};
static CogitoColor cogito_cached_on_tertiary_container = {0};
static CogitoColor cogito_cached_on_surface_variant = {0};

// Called from sum_build_dynamic when theme is applied
// Stores tertiary colors for later use by widgets
static void cogito_cache_tertiary_colors(CogitoEnsorScheme* scheme) {
  if (!scheme) return;
  cogito_cached_primary = scheme->primary;
  cogito_cached_secondary = scheme->secondary;
  cogito_cached_secondary_container = scheme->secondary_container;
  cogito_cached_tertiary = scheme->tertiary;
  cogito_cached_surface = scheme->surface;
  cogito_cached_on_surface = scheme->on_surface;
  cogito_cached_on_primary_container = scheme->on_primary_container;
  cogito_cached_on_secondary_container = scheme->on_secondary_container;
  cogito_cached_on_tertiary = scheme->on_tertiary;
  cogito_cached_tertiary_container = scheme->tertiary_container;
  cogito_cached_on_tertiary_container = scheme->on_tertiary_container;
  cogito_cached_on_surface_variant = scheme->on_surface_variant;
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
  CogitoColor* out_secondary,
  CogitoColor* out_secondary_container,
  CogitoColor* out_tertiary,
  CogitoColor* out_surface,
  CogitoColor* out_on_surface,
  CogitoColor* out_on_primary_container,
  CogitoColor* out_on_secondary_container,
  CogitoColor* out_on_tertiary_container,
  CogitoColor* out_on_surface_variant
) {
  if (out_primary) *out_primary = cogito_cached_primary;
  if (out_secondary) *out_secondary = cogito_cached_secondary;
  if (out_secondary_container) *out_secondary_container = cogito_cached_secondary_container;
  if (out_tertiary) *out_tertiary = cogito_cached_tertiary;
  if (out_surface) *out_surface = cogito_cached_surface;
  if (out_on_surface) *out_on_surface = cogito_cached_on_surface;
  if (out_on_primary_container) *out_on_primary_container = cogito_cached_on_primary_container;
  if (out_on_secondary_container) *out_on_secondary_container = cogito_cached_on_secondary_container;
  if (out_on_tertiary_container) *out_on_tertiary_container = cogito_cached_on_tertiary_container;
  if (out_on_surface_variant) *out_on_surface_variant = cogito_cached_on_surface_variant;
}
