// Ensor Score: ranks extracted colors by hue proportion and chroma.
// Ported from Ensor/Score.vala (Apache License 2.0, Copyright 2021 Google LLC)

#define COGITO_SCORE_TARGET_CHROMA         48.0
#define COGITO_SCORE_WEIGHT_PROPORTION      0.7
#define COGITO_SCORE_WEIGHT_CHROMA_ABOVE    0.3
#define COGITO_SCORE_WEIGHT_CHROMA_BELOW    0.1
#define COGITO_SCORE_CUTOFF_CHROMA          5.0
#define COGITO_SCORE_CUTOFF_PROPORTION      0.01
// Fallback color: tau purple 0x8C56BF
#define COGITO_SCORE_TAU_PURPLE             ((int)0xFF8C56BF)

static inline double cogito_ensor_sanitize_hue(double h) {
  h = fmod(h, 360.0);
  if (h < 0.0) h += 360.0;
  return h;
}

static inline double cogito_ensor_hue_diff(double a, double b) {
  double d = fabs(a - b);
  return d > 180.0 ? 360.0 - d : d;
}

// Selects the best accent colors from a pixel count map.
// Returns the number of colors written to out_colors (at most desired, at most 4).
static int cogito_ensor_score(const CogitoEnsorQMap* colors_to_pop,
                               int desired,
                               int* out_colors)
{
  if (desired <= 0) desired = 4;
  if (!colors_to_pop || colors_to_pop->size == 0) {
    out_colors[0] = COGITO_SCORE_TAU_PURPLE;
    return 1;
  }

  int n = colors_to_pop->size;
  // Collect colors into arrays (on heap for safety with larger inputs)
  int* argbs     = (int*)malloc(n * sizeof(int));
  int* pops      = (int*)malloc(n * sizeof(int));
  double* cam_h  = (double*)malloc(n * sizeof(double));
  double* cam_c  = (double*)malloc(n * sizeof(double));
  double* excite = (double*)calloc(n, sizeof(double));
  double* scores = (double*)malloc(n * sizeof(double));

  if (!argbs || !pops || !cam_h || !cam_c || !excite || !scores) {
    free(argbs); free(pops); free(cam_h); free(cam_c); free(excite); free(scores);
    out_colors[0] = COGITO_SCORE_TAU_PURPLE;
    return 1;
  }

  // Fill arrays from map
  {
    int idx = 0;
    for (int i = 0; i < COGITO_ENSOR_QMAP_CAP && idx < n; i++) {
      if (!colors_to_pop->entries[i].used) continue;
      argbs[idx] = colors_to_pop->entries[i].color;
      pops[idx]  = colors_to_pop->entries[i].count;
      idx++;
    }
    n = idx;
  }

  // Total population
  double pop_sum = 0.0;
  for (int i = 0; i < n; i++) pop_sum += pops[i];
  if (pop_sum < 1e-10) pop_sum = 1.0;

  // Hue proportions array [0..360]
  double hue_proportions[361];
  memset(hue_proportions, 0, sizeof(hue_proportions));

  // Convert to CAM16 (h, c) and accumulate hue proportions
  for (int i = 0; i < n; i++) {
    CogitoColor col = {
      (unsigned char)cogito_ensor_argb_r(argbs[i]),
      (unsigned char)cogito_ensor_argb_g(argbs[i]),
      (unsigned char)cogito_ensor_argb_b(argbs[i]),
      255
    };
    double x = 0.0, y = 0.0, z = 0.0;
    cogito_xyz_from_srgb(col, &x, &y, &z);
    CogitoCam16 cam = cogito_cam16_from_xyz(x, y, z);
    cam_h[i] = cam.h;
    cam_c[i] = cam.c;
    double proportion = pops[i] / pop_sum;
    int hue_int = (int)floor(cogito_ensor_sanitize_hue(cam.h));
    if (hue_int >= 0 && hue_int <= 360) {
      hue_proportions[hue_int] += proportion;
    }
  }

  // Excited proportions: sum hue proportions in [hue-14, hue+15]
  for (int i = 0; i < n; i++) {
    int hue_int = (int)floor(cam_h[i]);
    for (int j = hue_int - 14; j < hue_int + 16; j++) {
      int sh = (int)floor(cogito_ensor_sanitize_hue((double)j));
      if (sh >= 0 && sh <= 360) {
        excite[i] += hue_proportions[sh];
      }
    }
  }

  // Score each color
  for (int i = 0; i < n; i++) {
    double prop_score = excite[i] * 100.0 * COGITO_SCORE_WEIGHT_PROPORTION;
    double chroma = cam_c[i];
    double cw = (chroma > COGITO_SCORE_TARGET_CHROMA)
                  ? COGITO_SCORE_WEIGHT_CHROMA_ABOVE
                  : COGITO_SCORE_WEIGHT_CHROMA_BELOW;
    scores[i] = prop_score + (chroma - COGITO_SCORE_TARGET_CHROMA) * cw;
  }

  // Insertion sort by score descending
  int* order = (int*)malloc(n * sizeof(int));
  if (!order) {
    free(argbs); free(pops); free(cam_h); free(cam_c); free(excite); free(scores);
    out_colors[0] = COGITO_SCORE_TAU_PURPLE;
    return 1;
  }
  for (int i = 0; i < n; i++) order[i] = i;
  for (int i = 1; i < n; i++) {
    int ki = order[i];
    double ks = scores[ki];
    int j = i - 1;
    while (j >= 0 && scores[order[j]] < ks) { order[j+1] = order[j]; j--; }
    order[j+1] = ki;
  }

  // Select qualifying colors with hue separation ≥ 15°
  double* sel_hues = (double*)malloc(desired * sizeof(double));
  int sel_count = 0;
  if (sel_hues) {
    for (int i = 0; i < n && sel_count < desired; i++) {
      int ii = order[i];
      if (cam_c[ii] < COGITO_SCORE_CUTOFF_CHROMA) continue;
      if (excite[ii] < COGITO_SCORE_CUTOFF_PROPORTION) continue;
      bool dup = false;
      for (int j = 0; j < sel_count; j++) {
        if (cogito_ensor_hue_diff(sel_hues[j], cam_h[ii]) < 15.0) { dup = true; break; }
      }
      if (dup) continue;
      out_colors[sel_count] = argbs[ii];
      sel_hues[sel_count] = cam_h[ii];
      sel_count++;
    }
    free(sel_hues);
  }

  if (sel_count == 0) {
    out_colors[0] = COGITO_SCORE_TAU_PURPLE;
    sel_count = 1;
  }

  free(argbs); free(pops); free(cam_h); free(cam_c); free(excite); free(scores); free(order);
  return sel_count;
}
