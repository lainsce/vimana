// Cogito Ensor — color system
// Ported from Ensor/Vala for Cogito.
// Provides 6 scheme variants and correct HCT tonal palette generation.

// ---- Scheme variant enum ----
typedef enum {
  COGITO_ENSOR_DEFAULT    = 0,
  COGITO_ENSOR_VIBRANT    = 1,
  COGITO_ENSOR_MUTED      = 2,
  COGITO_ENSOR_MONOCHROME = 3,
  COGITO_ENSOR_SALAD      = 4,
  COGITO_ENSOR_CONTENT    = 5,
} CogitoEnsorVariant;

// ---- Full color scheme (all role colors) ----
typedef struct {
  bool dark;
  CogitoColor primary, on_primary, primary_container, on_primary_container;
  CogitoColor primary_fixed, primary_fixed_dim, on_primary_fixed, on_primary_fixed_variant;
  CogitoColor secondary, on_secondary, secondary_container, on_secondary_container;
  CogitoColor secondary_fixed, secondary_fixed_dim, on_secondary_fixed, on_secondary_fixed_variant;
  CogitoColor tertiary, on_tertiary, tertiary_container, on_tertiary_container;
  CogitoColor tertiary_fixed, tertiary_fixed_dim, on_tertiary_fixed, on_tertiary_fixed_variant;
  CogitoColor error, on_error, error_container, on_error_container;
  CogitoColor background, on_background;
  CogitoColor surface, on_surface, surface_tint;
  CogitoColor surface_dim, surface_bright;
  CogitoColor surface_container_lowest, surface_container_low, surface_container, surface_container_high, surface_container_highest;
  CogitoColor inverse_surface, inverse_on_surface, inverse_primary;
  CogitoColor outline, outline_variant;
  CogitoColor surface_variant, on_surface_variant;
  CogitoColor shadow, scrim;
} CogitoEnsorScheme;

#include "ensor_palette.inc"
#include "ensor_schemes.inc"
#include "ensor_quantize.inc"
#include "ensor_score.inc"
#include "ensor_keycolor.inc"
#include "ensor_temperature.inc"
#include "ensor_accent.inc"
#include "ensor_dynamic.inc"
