// Ensor quantizer pipeline: QuantizerMap → QuantizerWu → QuantizerWsmeans → QuantizerCelebi
// Ported from Ensor/Quantize/*.vala (Apache License 2.0, Copyright 2021 Google LLC)

// ---- ARGB helpers ----

static inline int cogito_ensor_argb_r(int argb) { return (argb >> 16) & 0xFF; }
static inline int cogito_ensor_argb_g(int argb) { return (argb >> 8) & 0xFF; }
static inline int cogito_ensor_argb_b(int argb) { return argb & 0xFF; }
static inline int cogito_ensor_pack_argb(int r, int g, int b) {
  return (int)(0xFF000000u | ((unsigned int)r << 16) | ((unsigned int)g << 8) | (unsigned int)b);
}

// ---- LAB color ----

typedef struct { double l, a, b; } CogitoEnsorLAB;

static double cogito_ensor_lab_f(double t) {
  const double d = 6.0 / 29.0;
  return (t > d * d * d) ? cbrt(t) : t / (3.0 * d * d) + 4.0 / 29.0;
}

static double cogito_ensor_lab_dist(CogitoEnsorLAB x, CogitoEnsorLAB y) {
  double dl = x.l - y.l, da = x.a - y.a, db = x.b - y.b;
  return dl * dl + da * da + db * db;
}

static CogitoEnsorLAB cogito_ensor_lab_from_argb(int argb) {
  CogitoColor c = {
    (unsigned char)cogito_ensor_argb_r(argb),
    (unsigned char)cogito_ensor_argb_g(argb),
    (unsigned char)cogito_ensor_argb_b(argb),
    255
  };
  double x = 0.0, y = 0.0, z = 0.0;
  cogito_xyz_from_srgb(c, &x, &y, &z);
  double fx = cogito_ensor_lab_f(x / 95.047);
  double fy = cogito_ensor_lab_f(y / 100.0);
  double fz = cogito_ensor_lab_f(z / 108.883);
  double L = 116.0 * fy - 16.0;
  CogitoEnsorLAB lab = { L < 0.0 ? 0.0 : L, 500.0 * (fx - fy), 200.0 * (fy - fz) };
  return lab;
}

static int cogito_ensor_lab_to_argb(CogitoEnsorLAB lab) {
  double fy = (lab.l + 16.0) / 116.0;
  double fx = lab.a / 500.0 + fy;
  double fz = fy - lab.b / 200.0;
  const double d = 6.0 / 29.0, d3 = d * d * d;
  double x = (fx * fx * fx > d3) ? fx * fx * fx * 95.047  : 3.0 * d * d * (fx - 4.0 / 29.0) * 95.047;
  double y = (fy * fy * fy > d3) ? fy * fy * fy * 100.0   : 3.0 * d * d * (fy - 4.0 / 29.0) * 100.0;
  double z = (fz * fz * fz > d3) ? fz * fz * fz * 108.883 : 3.0 * d * d * (fz - 4.0 / 29.0) * 108.883;
  CogitoColor rgb = {0, 0, 0, 255};
  cogito_srgb_from_xyz(x, y, z, &rgb);
  return cogito_ensor_pack_argb(rgb.r, rgb.g, rgb.b);
}

// ---- Color map (open-addressing hash map, int→int) ----
// Capacity must be a power of 2.

#define COGITO_ENSOR_QMAP_CAP 4096

typedef struct { int color; int count; bool used; } CogitoEnsorQEntry;

typedef struct {
  CogitoEnsorQEntry entries[COGITO_ENSOR_QMAP_CAP];
  int size;
} CogitoEnsorQMap;

static void cogito_ensor_qmap_clear(CogitoEnsorQMap* m) {
  memset(m, 0, sizeof(*m));
}

static void cogito_ensor_qmap_increment(CogitoEnsorQMap* m, int color) {
  unsigned int h = (unsigned int)color * 2654435761u;
  int idx = (int)(h & (unsigned int)(COGITO_ENSOR_QMAP_CAP - 1));
  for (int probe = 0; probe < COGITO_ENSOR_QMAP_CAP; probe++) {
    if (!m->entries[idx].used) {
      m->entries[idx].used = true;
      m->entries[idx].color = color;
      m->entries[idx].count = 1;
      m->size++;
      return;
    }
    if (m->entries[idx].color == color) {
      m->entries[idx].count++;
      return;
    }
    idx = (idx + 1) & (COGITO_ENSOR_QMAP_CAP - 1);
  }
}

static void cogito_ensor_qmap_set(CogitoEnsorQMap* m, int color, int count) {
  unsigned int h = (unsigned int)color * 2654435761u;
  int idx = (int)(h & (unsigned int)(COGITO_ENSOR_QMAP_CAP - 1));
  for (int probe = 0; probe < COGITO_ENSOR_QMAP_CAP; probe++) {
    if (!m->entries[idx].used) {
      m->entries[idx].used = true;
      m->entries[idx].color = color;
      m->entries[idx].count = count;
      m->size++;
      return;
    }
    if (m->entries[idx].color == color) {
      m->entries[idx].count = count;
      return;
    }
    idx = (idx + 1) & (COGITO_ENSOR_QMAP_CAP - 1);
  }
}

static int cogito_ensor_qmap_get(const CogitoEnsorQMap* m, int color) {
  unsigned int h = (unsigned int)color * 2654435761u;
  int idx = (int)(h & (unsigned int)(COGITO_ENSOR_QMAP_CAP - 1));
  for (int probe = 0; probe < COGITO_ENSOR_QMAP_CAP; probe++) {
    if (!m->entries[idx].used) return 0;
    if (m->entries[idx].color == color) return m->entries[idx].count;
    idx = (idx + 1) & (COGITO_ENSOR_QMAP_CAP - 1);
  }
  return 0;
}

// ---- QuantizerMap ----
// Builds a pixel count map from a raw pixel array.

static void cogito_ensor_quantizer_map(const int* pixels, int n,
                                       CogitoEnsorQMap* out) {
  cogito_ensor_qmap_clear(out);
  for (int i = 0; i < n; i++) {
    cogito_ensor_qmap_increment(out, pixels[i]);
  }
}

// ---- QuantizerWu ----
// Wu's histogram-based RGB-cube quantizer.

#define COGITO_WU_INDEX_BITS  5
#define COGITO_WU_INDEX_COUNT 33
#define COGITO_WU_TOTAL_SIZE  35937

static inline int cogito_wu_idx(int r, int g, int b) {
  return (r << (COGITO_WU_INDEX_BITS * 2)) +
         (r << (COGITO_WU_INDEX_BITS + 1)) + r +
         (g << COGITO_WU_INDEX_BITS) + g + b;
}

typedef struct { int r0, r1, g0, g1, b0, b1, vol; } CogitoWuBox;

typedef enum { COGITO_WU_RED, COGITO_WU_GREEN, COGITO_WU_BLUE } CogitoWuDir;

static int cogito_wu_volume_i(const CogitoWuBox* c, const int* m) {
  return  m[cogito_wu_idx(c->r1, c->g1, c->b1)]
         -m[cogito_wu_idx(c->r1, c->g1, c->b0)]
         -m[cogito_wu_idx(c->r1, c->g0, c->b1)]
         +m[cogito_wu_idx(c->r1, c->g0, c->b0)]
         -m[cogito_wu_idx(c->r0, c->g1, c->b1)]
         +m[cogito_wu_idx(c->r0, c->g1, c->b0)]
         +m[cogito_wu_idx(c->r0, c->g0, c->b1)]
         -m[cogito_wu_idx(c->r0, c->g0, c->b0)];
}

static double cogito_wu_volume_d(const CogitoWuBox* c, const double* m) {
  return  m[cogito_wu_idx(c->r1, c->g1, c->b1)]
         -m[cogito_wu_idx(c->r1, c->g1, c->b0)]
         -m[cogito_wu_idx(c->r1, c->g0, c->b1)]
         +m[cogito_wu_idx(c->r1, c->g0, c->b0)]
         -m[cogito_wu_idx(c->r0, c->g1, c->b1)]
         +m[cogito_wu_idx(c->r0, c->g1, c->b0)]
         +m[cogito_wu_idx(c->r0, c->g0, c->b1)]
         -m[cogito_wu_idx(c->r0, c->g0, c->b0)];
}

static int cogito_wu_bottom(const CogitoWuBox* c, CogitoWuDir dir, const int* m) {
  switch (dir) {
    case COGITO_WU_RED:
      return -m[cogito_wu_idx(c->r0, c->g1, c->b1)]
             +m[cogito_wu_idx(c->r0, c->g1, c->b0)]
             +m[cogito_wu_idx(c->r0, c->g0, c->b1)]
             -m[cogito_wu_idx(c->r0, c->g0, c->b0)];
    case COGITO_WU_GREEN:
      return -m[cogito_wu_idx(c->r1, c->g0, c->b1)]
             +m[cogito_wu_idx(c->r1, c->g0, c->b0)]
             +m[cogito_wu_idx(c->r0, c->g0, c->b1)]
             -m[cogito_wu_idx(c->r0, c->g0, c->b0)];
    case COGITO_WU_BLUE:
      return -m[cogito_wu_idx(c->r1, c->g1, c->b0)]
             +m[cogito_wu_idx(c->r1, c->g0, c->b0)]
             +m[cogito_wu_idx(c->r0, c->g1, c->b0)]
             -m[cogito_wu_idx(c->r0, c->g0, c->b0)];
  }
  return 0;
}

static int cogito_wu_top(const CogitoWuBox* c, CogitoWuDir dir, int pos, const int* m) {
  switch (dir) {
    case COGITO_WU_RED:
      return  m[cogito_wu_idx(pos, c->g1, c->b1)]
             -m[cogito_wu_idx(pos, c->g1, c->b0)]
             -m[cogito_wu_idx(pos, c->g0, c->b1)]
             +m[cogito_wu_idx(pos, c->g0, c->b0)];
    case COGITO_WU_GREEN:
      return  m[cogito_wu_idx(c->r1, pos, c->b1)]
             -m[cogito_wu_idx(c->r1, pos, c->b0)]
             -m[cogito_wu_idx(c->r0, pos, c->b1)]
             +m[cogito_wu_idx(c->r0, pos, c->b0)];
    case COGITO_WU_BLUE:
      return  m[cogito_wu_idx(c->r1, c->g1, pos)]
             -m[cogito_wu_idx(c->r1, c->g0, pos)]
             -m[cogito_wu_idx(c->r0, c->g1, pos)]
             +m[cogito_wu_idx(c->r0, c->g0, pos)];
  }
  return 0;
}

typedef struct { int cut_location; double maximum; } CogitoWuMaxResult;

static CogitoWuMaxResult cogito_wu_maximize(
    const CogitoWuBox* cube, CogitoWuDir dir, int first, int last,
    int whole_r, int whole_g, int whole_b, int whole_w,
    const int* mr, const int* mg, const int* mb, const int* weights)
{
  int bot_r = cogito_wu_bottom(cube, dir, mr);
  int bot_g = cogito_wu_bottom(cube, dir, mg);
  int bot_b = cogito_wu_bottom(cube, dir, mb);
  int bot_w = cogito_wu_bottom(cube, dir, weights);
  double max = 0.0;
  int cut = -1;
  for (int i = first; i < last; i++) {
    int hr = bot_r + cogito_wu_top(cube, dir, i, mr);
    int hg = bot_g + cogito_wu_top(cube, dir, i, mg);
    int hb = bot_b + cogito_wu_top(cube, dir, i, mb);
    int hw = bot_w + cogito_wu_top(cube, dir, i, weights);
    if (hw == 0) continue;
    double num = (double)(hr * hr + hg * hg + hb * hb);
    double temp = num / (double)hw;
    hr = whole_r - hr; hg = whole_g - hg; hb = whole_b - hb; hw = whole_w - hw;
    if (hw == 0) continue;
    temp += (double)(hr * hr + hg * hg + hb * hb) / (double)hw;
    if (temp > max) { max = temp; cut = i; }
  }
  CogitoWuMaxResult r = { cut, max };
  return r;
}

static bool cogito_wu_cut(CogitoWuBox* one, CogitoWuBox* two,
                           const int* mr, const int* mg, const int* mb,
                           const int* weights) {
  int wr = cogito_wu_volume_i(one, mr);
  int wg = cogito_wu_volume_i(one, mg);
  int wb = cogito_wu_volume_i(one, mb);
  int ww = cogito_wu_volume_i(one, weights);

  CogitoWuMaxResult rx = cogito_wu_maximize(one, COGITO_WU_RED,
    one->r0+1, one->r1, wr, wg, wb, ww, mr, mg, mb, weights);
  CogitoWuMaxResult gx = cogito_wu_maximize(one, COGITO_WU_GREEN,
    one->g0+1, one->g1, wr, wg, wb, ww, mr, mg, mb, weights);
  CogitoWuMaxResult bx = cogito_wu_maximize(one, COGITO_WU_BLUE,
    one->b0+1, one->b1, wr, wg, wb, ww, mr, mg, mb, weights);

  if (rx.cut_location < 0) return false;

  CogitoWuDir dir;
  if (rx.maximum >= gx.maximum && rx.maximum >= bx.maximum) dir = COGITO_WU_RED;
  else if (gx.maximum >= rx.maximum && gx.maximum >= bx.maximum) dir = COGITO_WU_GREEN;
  else dir = COGITO_WU_BLUE;

  two->r1 = one->r1; two->g1 = one->g1; two->b1 = one->b1;
  switch (dir) {
    case COGITO_WU_RED:
      one->r1 = rx.cut_location;
      two->r0 = one->r1; two->g0 = one->g0; two->b0 = one->b0;
      break;
    case COGITO_WU_GREEN:
      one->g1 = gx.cut_location;
      two->r0 = one->r0; two->g0 = one->g1; two->b0 = one->b0;
      break;
    case COGITO_WU_BLUE:
      one->b1 = bx.cut_location;
      two->r0 = one->r0; two->g0 = one->g0; two->b0 = one->b1;
      break;
  }
  one->vol = (one->r1-one->r0)*(one->g1-one->g0)*(one->b1-one->b0);
  two->vol = (two->r1-two->r0)*(two->g1-two->g0)*(two->b1-two->b0);
  return true;
}

// Run Wu quantizer: takes pixel array + count map → fills out_map with cluster colors.
// Returns number of clusters found (≤ max_colors).
static int cogito_ensor_quantizer_wu(const int* pixels, int n_pixels,
                                     const CogitoEnsorQMap* pixel_to_count,
                                     int max_colors,
                                     CogitoEnsorQMap* out) {
  // Allocate histogram arrays on heap (large: 35937 × 5)
  int*    weights   = (int*)calloc(COGITO_WU_TOTAL_SIZE, sizeof(int));
  int*    moments_r = (int*)calloc(COGITO_WU_TOTAL_SIZE, sizeof(int));
  int*    moments_g = (int*)calloc(COGITO_WU_TOTAL_SIZE, sizeof(int));
  int*    moments_b = (int*)calloc(COGITO_WU_TOTAL_SIZE, sizeof(int));
  double* moments   = (double*)calloc(COGITO_WU_TOTAL_SIZE, sizeof(double));

  if (!weights || !moments_r || !moments_g || !moments_b || !moments) {
    free(weights); free(moments_r); free(moments_g); free(moments_b); free(moments);
    cogito_ensor_qmap_clear(out);
    return 0;
  }

  // Construct histogram from pixel_to_count map
  int bits_to_remove = 8 - COGITO_WU_INDEX_BITS;
  for (int i = 0; i < COGITO_ENSOR_QMAP_CAP; i++) {
    if (!pixel_to_count->entries[i].used) continue;
    int pixel = pixel_to_count->entries[i].color;
    int count = pixel_to_count->entries[i].count;
    int red   = cogito_ensor_argb_r(pixel);
    int green = cogito_ensor_argb_g(pixel);
    int blue  = cogito_ensor_argb_b(pixel);
    int ir = (red   >> bits_to_remove) + 1;
    int ig = (green >> bits_to_remove) + 1;
    int ib = (blue  >> bits_to_remove) + 1;
    int idx = cogito_wu_idx(ir, ig, ib);
    weights[idx]   += count;
    moments_r[idx] += red   * count;
    moments_g[idx] += green * count;
    moments_b[idx] += blue  * count;
    moments[idx]   += (double)count * (red*red + green*green + blue*blue);
  }

  // Create moments (3D prefix sum / integral image)
  for (int r = 1; r < COGITO_WU_INDEX_COUNT; r++) {
    int    area[COGITO_WU_INDEX_COUNT]  = {0};
    int    area_r[COGITO_WU_INDEX_COUNT] = {0};
    int    area_g[COGITO_WU_INDEX_COUNT] = {0};
    int    area_b[COGITO_WU_INDEX_COUNT] = {0};
    double area2[COGITO_WU_INDEX_COUNT] = {0.0};
    for (int g = 1; g < COGITO_WU_INDEX_COUNT; g++) {
      int    line = 0,   line_r = 0, line_g = 0, line_b = 0;
      double line2 = 0.0;
      for (int b = 1; b < COGITO_WU_INDEX_COUNT; b++) {
        int idx  = cogito_wu_idx(r, g, b);
        int pidx = cogito_wu_idx(r-1, g, b);
        line   += weights[idx];
        line_r += moments_r[idx];
        line_g += moments_g[idx];
        line_b += moments_b[idx];
        line2  += moments[idx];
        area[b]  += line;
        area_r[b] += line_r;
        area_g[b] += line_g;
        area_b[b] += line_b;
        area2[b] += line2;
        weights[idx]   = weights[pidx]   + area[b];
        moments_r[idx] = moments_r[pidx] + area_r[b];
        moments_g[idx] = moments_g[pidx] + area_g[b];
        moments_b[idx] = moments_b[pidx] + area_b[b];
        moments[idx]   = moments[pidx]   + area2[b];
      }
    }
  }

  // Create boxes
  CogitoWuBox* cubes = (CogitoWuBox*)calloc(max_colors, sizeof(CogitoWuBox));
  double* vv = (double*)calloc(max_colors, sizeof(double));
  if (!cubes || !vv) {
    free(cubes); free(vv);
    free(weights); free(moments_r); free(moments_g); free(moments_b); free(moments);
    cogito_ensor_qmap_clear(out);
    return 0;
  }

  cubes[0].r1 = COGITO_WU_INDEX_COUNT - 1;
  cubes[0].g1 = COGITO_WU_INDEX_COUNT - 1;
  cubes[0].b1 = COGITO_WU_INDEX_COUNT - 1;

  int generated = max_colors;
  int next = 0;
  for (int i = 1; i < max_colors; i++) {
    if (cogito_wu_cut(&cubes[next], &cubes[i], moments_r, moments_g, moments_b, weights)) {
      // Compute variance
      {
        CogitoWuBox* bc = &cubes[next];
        if (bc->vol > 1) {
          int dr = cogito_wu_volume_i(bc, moments_r);
          int dg = cogito_wu_volume_i(bc, moments_g);
          int db = cogito_wu_volume_i(bc, moments_b);
          double xx = cogito_wu_volume_d(bc, moments);
          int hyp = dr*dr + dg*dg + db*db;
          int vol = cogito_wu_volume_i(bc, weights);
          vv[next] = vol > 0 ? xx - (double)hyp / (double)vol : 0.0;
        } else {
          vv[next] = 0.0;
        }
      }
      {
        CogitoWuBox* bc = &cubes[i];
        if (bc->vol > 1) {
          int dr = cogito_wu_volume_i(bc, moments_r);
          int dg = cogito_wu_volume_i(bc, moments_g);
          int db = cogito_wu_volume_i(bc, moments_b);
          double xx = cogito_wu_volume_d(bc, moments);
          int hyp = dr*dr + dg*dg + db*db;
          int vol = cogito_wu_volume_i(bc, weights);
          vv[i] = vol > 0 ? xx - (double)hyp / (double)vol : 0.0;
        } else {
          vv[i] = 0.0;
        }
      }
    } else {
      vv[next] = 0.0;
      i--;
    }
    next = 0;
    double best = vv[0];
    for (int j = 1; j <= i; j++) {
      if (vv[j] > best) { best = vv[j]; next = j; }
    }
    if (best <= 0.0) { generated = i + 1; break; }
  }

  // Create result colors
  cogito_ensor_qmap_clear(out);
  for (int i = 0; i < generated; i++) {
    CogitoWuBox* bc = &cubes[i];
    int w = cogito_wu_volume_i(bc, weights);
    if (w <= 0) continue;
    int r = cogito_wu_volume_i(bc, moments_r) / w;
    int g = cogito_wu_volume_i(bc, moments_g) / w;
    int b = cogito_wu_volume_i(bc, moments_b) / w;
    int color = cogito_ensor_pack_argb(r & 0xFF, g & 0xFF, b & 0xFF);
    cogito_ensor_qmap_set(out, color, 0);
  }

  free(cubes); free(vv);
  free(weights); free(moments_r); free(moments_g); free(moments_b); free(moments);
  return out->size;
}

// ---- QuantizerWsmeans ----
// Weighted K-means refinement in LAB space (MAX_ITERATIONS=1, MIN_MOVEMENT_DISTANCE=3.0).

#define COGITO_WSMEANS_MAX_ITER 1
#define COGITO_WSMEANS_MIN_MOVE 3.0

// Simple LCG random for fixed seed (matches Vala's Rand.with_seed(42688))
static unsigned int cogito_ensor_lcg_next(unsigned int* state) {
  *state = *state * 1664525u + 1013904223u;
  return *state;
}

static void cogito_ensor_quantizer_wsmeans(
    const int* pixels, int n_pixels,
    const int* start_clusters, int n_start,
    int max_colors,
    CogitoEnsorQMap* out)
{
  if (n_pixels == 0) { cogito_ensor_qmap_clear(out); return; }

  // Build pixel count map
  CogitoEnsorQMap pixel_map;
  cogito_ensor_qmap_clear(&pixel_map);
  for (int i = 0; i < n_pixels; i++) {
    cogito_ensor_qmap_increment(&pixel_map, pixels[i]);
  }

  // Collect unique pixels
  int point_count = pixel_map.size;
  if (point_count == 0) { cogito_ensor_qmap_clear(out); return; }

  int cluster_count = (max_colors < point_count) ? max_colors : point_count;
  if (n_start > 0 && n_start < cluster_count) cluster_count = n_start;

  int* upix   = (int*)malloc(point_count * sizeof(int));
  int* ucnt   = (int*)malloc(point_count * sizeof(int));
  CogitoEnsorLAB* points = (CogitoEnsorLAB*)malloc(point_count * sizeof(CogitoEnsorLAB));
  CogitoEnsorLAB* clusters = (CogitoEnsorLAB*)malloc(cluster_count * sizeof(CogitoEnsorLAB));
  int* cluster_indices = (int*)malloc(point_count * sizeof(int));
  int* pixel_count_sums = (int*)calloc(cluster_count, sizeof(int));
  double* cluster_distances = (double*)malloc((size_t)cluster_count * cluster_count * sizeof(double));

  if (!upix || !ucnt || !points || !clusters || !cluster_indices || !pixel_count_sums || !cluster_distances) {
    free(upix); free(ucnt); free(points); free(clusters);
    free(cluster_indices); free(pixel_count_sums); free(cluster_distances);
    cogito_ensor_qmap_clear(out);
    return;
  }

  // Fill unique pixels
  {
    int idx = 0;
    for (int i = 0; i < COGITO_ENSOR_QMAP_CAP; i++) {
      if (!pixel_map.entries[i].used) continue;
      upix[idx] = pixel_map.entries[i].color;
      ucnt[idx] = pixel_map.entries[i].count;
      points[idx] = cogito_ensor_lab_from_argb(upix[idx]);
      idx++;
    }
  }

  // Initialize clusters from starting clusters
  int clusters_from_start = (n_start < cluster_count) ? n_start : cluster_count;
  for (int i = 0; i < clusters_from_start; i++) {
    clusters[i] = cogito_ensor_lab_from_argb(start_clusters[i]);
  }

  // Fill remaining with pseudo-random LAB values (seed 42688)
  {
    const int RAND_MAX_VAL = 32767;
    unsigned int rng = 42688u;
    for (int i = clusters_from_start; i < cluster_count; i++) {
      double l = (double)(cogito_ensor_lcg_next(&rng) % (RAND_MAX_VAL+1)) / (double)RAND_MAX_VAL * 100.0;
      double a = (double)(cogito_ensor_lcg_next(&rng) % (RAND_MAX_VAL+1)) / (double)RAND_MAX_VAL * 200.0 - 100.0;
      double b = (double)(cogito_ensor_lcg_next(&rng) % (RAND_MAX_VAL+1)) / (double)RAND_MAX_VAL * 200.0 - 100.0;
      clusters[i].l = l; clusters[i].a = a; clusters[i].b = b;
    }
  }

  // Initialize cluster indices randomly (seed 42688)
  {
    unsigned int rng = 42688u;
    for (int i = 0; i < point_count; i++) {
      cluster_indices[i] = (int)(cogito_ensor_lcg_next(&rng) % (unsigned int)cluster_count);
    }
  }

  for (int iteration = 0; iteration < COGITO_WSMEANS_MAX_ITER; iteration++) {
    // Cluster-to-cluster distances
    for (int i = 0; i < cluster_count; i++) {
      cluster_distances[i * cluster_count + i] = 0.0;
      for (int j = i + 1; j < cluster_count; j++) {
        double dist = cogito_ensor_lab_dist(clusters[i], clusters[j]);
        cluster_distances[i * cluster_count + j] = dist;
        cluster_distances[j * cluster_count + i] = dist;
      }
    }

    bool color_moved = false;
    for (int i = 0; i < point_count; i++) {
      CogitoEnsorLAB pt = points[i];
      int prev_idx = cluster_indices[i];
      double prev_dist = cogito_ensor_lab_dist(pt, clusters[prev_idx]);
      double min_dist = prev_dist;
      int new_idx = -1;
      for (int j = 0; j < cluster_count; j++) {
        if (j == prev_idx) continue;
        double cd = cluster_distances[prev_idx * cluster_count + j];
        if (cd >= 4.0 * prev_dist) continue;
        double dist = cogito_ensor_lab_dist(pt, clusters[j]);
        if (dist < min_dist) { min_dist = dist; new_idx = j; }
      }
      if (new_idx != -1) {
        double change = fabs(sqrt(fmax(0.0, min_dist)) - sqrt(fmax(0.0, prev_dist)));
        if (change > COGITO_WSMEANS_MIN_MOVE) {
          color_moved = true;
          cluster_indices[i] = new_idx;
        }
      }
    }
    if (!color_moved && iteration != 0) break;

    // Update cluster centers
    double* lsum = (double*)calloc(cluster_count, sizeof(double));
    double* asum = (double*)calloc(cluster_count, sizeof(double));
    double* bsum = (double*)calloc(cluster_count, sizeof(double));
    memset(pixel_count_sums, 0, (size_t)cluster_count * sizeof(int));

    for (int i = 0; i < point_count; i++) {
      int ci = cluster_indices[i];
      pixel_count_sums[ci] += ucnt[i];
      lsum[ci] += points[i].l * ucnt[i];
      asum[ci] += points[i].a * ucnt[i];
      bsum[ci] += points[i].b * ucnt[i];
    }
    for (int i = 0; i < cluster_count; i++) {
      int cnt = pixel_count_sums[i];
      if (cnt == 0) { clusters[i].l = 0; clusters[i].a = 0; clusters[i].b = 0; }
      else { clusters[i].l = lsum[i]/cnt; clusters[i].a = asum[i]/cnt; clusters[i].b = bsum[i]/cnt; }
    }
    free(lsum); free(asum); free(bsum);
  }

  // Build result
  cogito_ensor_qmap_clear(out);
  int* cluster_argbs = (int*)malloc(cluster_count * sizeof(int));
  if (cluster_argbs) {
    for (int i = 0; i < cluster_count; i++) cluster_argbs[i] = -1;
    for (int i = 0; i < cluster_count; i++) {
      int cnt = pixel_count_sums[i];
      if (cnt == 0) continue;
      int argb = cogito_ensor_lab_to_argb(clusters[i]);
      // Merge duplicates
      bool dup = false;
      for (int j = 0; j < i; j++) {
        if (cluster_argbs[j] == argb) {
          int existing = cogito_ensor_qmap_get(out, argb);
          cogito_ensor_qmap_set(out, argb, existing + cnt);
          dup = true; break;
        }
      }
      if (!dup) {
        cluster_argbs[i] = argb;
        int existing = cogito_ensor_qmap_get(out, argb);
        cogito_ensor_qmap_set(out, argb, existing + cnt);
      }
    }
    free(cluster_argbs);
  }

  free(upix); free(ucnt); free(points); free(clusters);
  free(cluster_indices); free(pixel_count_sums); free(cluster_distances);
}

// ---- QuantizerCelebi ----
// Combines Wu (histogram) + Wsmeans (K-means) for best results.

static void cogito_ensor_quantizer_celebi(
    const int* pixels, int n_pixels,
    int max_colors,
    CogitoEnsorQMap* out)
{
  if (n_pixels == 0 || max_colors <= 0) { cogito_ensor_qmap_clear(out); return; }

  // Step 1: build pixel count map for Wu
  CogitoEnsorQMap pixel_map;
  cogito_ensor_quantizer_map(pixels, n_pixels, &pixel_map);

  // Step 2: Wu quantizer → cluster seeds
  CogitoEnsorQMap wu_result;
  cogito_ensor_qmap_clear(&wu_result);
  cogito_ensor_quantizer_wu(pixels, n_pixels, &pixel_map, max_colors, &wu_result);

  // Extract Wu cluster colors into an array
  int wu_count = wu_result.size;
  int* wu_clusters = (int*)malloc(wu_count * sizeof(int));
  if (wu_clusters) {
    int idx = 0;
    for (int i = 0; i < COGITO_ENSOR_QMAP_CAP && idx < wu_count; i++) {
      if (wu_result.entries[i].used) wu_clusters[idx++] = wu_result.entries[i].color;
    }
  }

  // Step 3: Wsmeans refinement
  cogito_ensor_quantizer_wsmeans(pixels, n_pixels,
                                  wu_clusters, wu_count,
                                  max_colors, out);
  free(wu_clusters);
}
