// Ensor scheme utilities — sanitize, piecewise, rotated hue
// (Legacy build_scheme removed — now in ensor_dynamic.inc)

// Sanitize a degree value to [0, 360)
static double cogito_ensor_sanitize_deg(double deg) {
  deg = fmod(deg, 360.0);
  if (deg < 0.0) deg += 360.0;
  return deg;
}

// Piecewise value lookup: src_h is the source hue,
// huebps are the breakpoints, rotations are the per-segment values.
static double cogito_ensor_piecewise_val(double src_h,
                                         const double* huebps, size_t nhue,
                                         const double* vals,   size_t nval) {
  size_t segments = nhue > 1 ? nhue - 1 : 0;
  if (nval < segments) segments = nval;
  for (size_t i = 0; i < segments; i++) {
    if (src_h >= huebps[i] && src_h < huebps[i + 1]) {
      return vals[i];
    }
  }
  return src_h;
}

// Get rotated hue using piecewise rotation table (adds rotation to source hue)
static double cogito_ensor_rotated_hue(double src_h,
                                        const double* huebps, size_t nhue,
                                        const double* rots,   size_t nrot) {
  double rot = cogito_ensor_piecewise_val(src_h, huebps, nhue, rots, nrot);
  return cogito_ensor_sanitize_deg(src_h + rot);
}
