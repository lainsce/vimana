// Returns true when the child rect (cx,cy,cw,ch) overlaps the parent
// viewport (px,py,pw,ph).  Used to skip drawing fully-offscreen items.
static bool cogito_rect_visible(int cx, int cy, int cw, int ch,
                                int px, int py, int pw, int ph) {
  return !(cx + cw <= px || cx >= px + pw ||
           cy + ch <= py || cy >= py + ph);
}

static float cogito_roundness_for(int w, int h, int radius) {
  if (radius <= 0) return 0.0f;
  float min = (float)(w < h ? w : h);
  if (min <= 0.0f) return 0.0f;
  float roundness = (float)radius / (min * 0.5f);
  if (roundness > 1.0f) roundness = 1.0f;
  if (roundness < 0.0f) roundness = 0.0f;
  return roundness;
}

static void cogito_draw_rect(int x, int y, int w, int h, CogitoColor bg, int radius) {
  if (w <= 0 || h <= 0) return;
  if (cogito_draw_clip_active &&
      !cogito_rect_visible(x, y, w, h,
                           cogito_draw_clip_x, cogito_draw_clip_y,
                           cogito_draw_clip_w, cogito_draw_clip_h)) {
    return;
  }
  if (cogito_backend && cogito_backend->draw_rect_rounded) {
    float roundness = cogito_roundness_for(w, h, radius);
    if (roundness > 0.0f) {
      cogito_backend->draw_rect_rounded(x, y, w, h, bg, roundness);
    } else {
      cogito_backend->draw_rect(x, y, w, h, bg);
    }
  }
}

static void cogito_draw_rect_lines(int x, int y, int w, int h, CogitoColor border, int radius, int thickness) {
  if (w <= 0 || h <= 0 || thickness <= 0) return;
  if (cogito_draw_clip_active &&
      !cogito_rect_visible(x, y, w, h,
                           cogito_draw_clip_x, cogito_draw_clip_y,
                           cogito_draw_clip_w, cogito_draw_clip_h)) {
    return;
  }
  if (cogito_backend && cogito_backend->draw_rect_rounded_lines) {
    float roundness = cogito_roundness_for(w, h, radius);
    if (roundness > 0.0f) {
      cogito_backend->draw_rect_rounded_lines(x, y, w, h, border, roundness, thickness);
    } else {
      cogito_backend->draw_rect_lines(x, y, w, h, border, thickness);
    }
  }
}

static int cogito_int_max(int a, int b) {
  return a > b ? a : b;
}

static void cogito_clamp_corner_radii(int w, int h, int* tl, int* tr, int* br, int* bl) {
  if (!tl || !tr || !br || !bl) return;
  int max_r = (w < h ? w : h) / 2;
  if (max_r < 0) max_r = 0;
  if (*tl < 0) *tl = 0;
  if (*tr < 0) *tr = 0;
  if (*br < 0) *br = 0;
  if (*bl < 0) *bl = 0;
  if (*tl > max_r) *tl = max_r;
  if (*tr > max_r) *tr = max_r;
  if (*br > max_r) *br = max_r;
  if (*bl > max_r) *bl = max_r;
}

static bool cogito_corner_radii_uniform(int tl, int tr, int br, int bl) {
  return tl == tr && tr == br && br == bl;
}

static void cogito_draw_rect_plain(int x, int y, int w, int h, CogitoColor color) {
  if (w <= 0 || h <= 0) return;
  if (cogito_draw_clip_active &&
      !cogito_rect_visible(x, y, w, h,
                           cogito_draw_clip_x, cogito_draw_clip_y,
                           cogito_draw_clip_w, cogito_draw_clip_h)) {
    return;
  }
  if (cogito_backend && cogito_backend->draw_rect) {
    cogito_backend->draw_rect(x, y, w, h, color);
  }
}

static void cogito_draw_corner_fill(int cx, int cy, int r, int sx, int sy, int sw, int sh, CogitoColor color) {
  if (r <= 0 || sw <= 0 || sh <= 0) return;
  if (!cogito_backend) return;
  bool can_clip = (cogito_backend->begin_scissor && cogito_backend->end_scissor);
  if (!cogito_backend->draw_rect_rounded && !cogito_backend->draw_circle) return;
  if (can_clip) {
    cogito_backend->begin_scissor(sx, sy, sw, sh);
  }
  if (cogito_backend->draw_rect_rounded) {
    int d = r * 2;
    float roundness = cogito_roundness_for(d, d, r);
    cogito_backend->draw_rect_rounded(cx - r, cy - r, d, d, color, roundness);
  } else {
    cogito_backend->draw_circle(cx, cy, (float)r, color);
  }
  if (can_clip) {
    cogito_backend->end_scissor();
  }
}
// Draw a circle clipped to a rectangle (for ripple effects)
static void cogito_draw_circle_clipped(int cx, int cy, float r, int clip_x, int clip_y, int clip_w, int clip_h, CogitoColor color) {
  if (r <= 0 || clip_w <= 0 || clip_h <= 0) return;
  if (!cogito_backend || !cogito_backend->draw_circle) return;
  if (cogito_backend->begin_scissor && cogito_backend->end_scissor) {
    cogito_backend->begin_scissor(clip_x, clip_y, clip_w, clip_h);
    cogito_backend->draw_circle(cx, cy, r, color);
    cogito_backend->end_scissor();
  } else {
    cogito_backend->draw_circle(cx, cy, r, color);
  }
}
// Draw a circle clipped to a rounded-rect region (for ripple effects on rounded widgets).
static void cogito_draw_circle_clipped_radii(int cx, int cy, float r,
                                             int clip_x, int clip_y, int clip_w, int clip_h,
                                             int tl, int tr, int br, int bl,
                                             CogitoColor color) {
  if (r <= 0 || clip_w <= 0 || clip_h <= 0) return;
  if (!cogito_backend) return;
  cogito_clamp_corner_radii(clip_w, clip_h, &tl, &tr, &br, &bl);
  if (tl == 0 && tr == 0 && br == 0 && bl == 0) {
    cogito_draw_circle_clipped(cx, cy, r, clip_x, clip_y, clip_w, clip_h, color);
    return;
  }
  if (!cogito_backend->draw_rect) {
    cogito_draw_circle_clipped(cx, cy, r, clip_x, clip_y, clip_w, clip_h, color);
    return;
  }

  int clip_right = clip_x + clip_w - 1;
  int clip_bottom = clip_y + clip_h - 1;
  int y0 = (int)floorf((float)cy - r);
  int y1 = (int)ceilf((float)cy + r);
  if (y0 < clip_y) y0 = clip_y;
  if (y1 > clip_bottom) y1 = clip_bottom;

  float rr = r * r;
  int tl_cx = clip_x + tl;
  int tl_cy = clip_y + tl;
  int tr_cx = clip_x + clip_w - tr - 1;
  int tr_cy = clip_y + tr;
  int br_cx = clip_x + clip_w - br - 1;
  int br_cy = clip_y + clip_h - br - 1;
  int bl_cx = clip_x + bl;
  int bl_cy = clip_y + clip_h - bl - 1;

  for (int y = y0; y <= y1; y++) {
    float py = (float)y + 0.5f;
    float dy = py - (float)cy;
    float dx_sq = rr - (dy * dy);
    if (dx_sq < 0.0f) continue;
    float dx = sqrtf(dx_sq);
    int c_left = (int)ceilf((float)cx - dx);
    int c_right = (int)floorf((float)cx + dx);

    int s_left = clip_x;
    int s_right = clip_right;

    if (tl > 0 && y < clip_y + tl) {
      float qy = py - (float)tl_cy;
      float qx_sq = (float)(tl * tl) - (qy * qy);
      int left = (qx_sq <= 0.0f) ? tl_cx : (int)ceilf((float)tl_cx - sqrtf(qx_sq));
      if (left > s_left) s_left = left;
    }
    if (bl > 0 && y > clip_y + clip_h - bl - 1) {
      float qy = py - (float)bl_cy;
      float qx_sq = (float)(bl * bl) - (qy * qy);
      int left = (qx_sq <= 0.0f) ? bl_cx : (int)ceilf((float)bl_cx - sqrtf(qx_sq));
      if (left > s_left) s_left = left;
    }
    if (tr > 0 && y < clip_y + tr) {
      float qy = py - (float)tr_cy;
      float qx_sq = (float)(tr * tr) - (qy * qy);
      int right = (qx_sq <= 0.0f) ? tr_cx : (int)floorf((float)tr_cx + sqrtf(qx_sq));
      if (right < s_right) s_right = right;
    }
    if (br > 0 && y > clip_y + clip_h - br - 1) {
      float qy = py - (float)br_cy;
      float qx_sq = (float)(br * br) - (qy * qy);
      int right = (qx_sq <= 0.0f) ? br_cx : (int)floorf((float)br_cx + sqrtf(qx_sq));
      if (right < s_right) s_right = right;
    }

    int left = c_left > s_left ? c_left : s_left;
    int right = c_right < s_right ? c_right : s_right;
    if (right >= left) {
      cogito_backend->draw_rect(left, y, right - left + 1,
      1, color);
    }
  }
}

static void cogito_draw_corner_outline(int cx, int cy, int r, int sx, int sy, int sw, int sh, CogitoColor color, int thickness) {
  if (r <= 0 || sw <= 0 || sh <= 0 || thickness <= 0) return;
  if (!cogito_backend) return;
  bool can_clip = (cogito_backend->begin_scissor && cogito_backend->end_scissor);
  if (!cogito_backend->draw_rect_rounded_lines && !cogito_backend->draw_circle_lines) return;
  if (can_clip) {
    cogito_backend->begin_scissor(sx, sy, sw, sh);
  }
  if (cogito_backend->draw_rect_rounded_lines) {
    int d = r * 2;
    float roundness = cogito_roundness_for(d, d, r);
    cogito_backend->draw_rect_rounded_lines(cx - r, cy - r, d, d, color, roundness, thickness);
  } else {
    cogito_backend->draw_circle_lines(cx, cy, (float)r, color, thickness);
  }
  if (can_clip) {
    cogito_backend->end_scissor();
  }
}

static void cogito_draw_rect_radii(int x, int y, int w, int h, CogitoColor bg, int tl, int tr, int br, int bl) {
  if (w <= 0 || h <= 0) return;
  cogito_clamp_corner_radii(w, h, &tl, &tr, &br, &bl);
  if (cogito_corner_radii_uniform(tl, tr, br, bl)) {
    cogito_draw_rect(x, y, w, h, bg, tl);
    return;
  }

  int top = cogito_int_max(tl, tr);
  int bottom = cogito_int_max(bl, br);
  int left = cogito_int_max(tl, bl);
  int right = cogito_int_max(tr, br);

  cogito_draw_rect_plain(x + left, y, w - left - right, h, bg);
  cogito_draw_rect_plain(x, y + tl, left, h - tl - bl, bg);
  cogito_draw_rect_plain(x + w - right, y + tr, right, h - tr - br, bg);
  cogito_draw_rect_plain(x + tl, y, w - tl - tr, top, bg);
  cogito_draw_rect_plain(x + bl, y + h - bottom, w - bl - br, bottom, bg);

  cogito_draw_corner_fill(x + tl, y + tl, tl, x, y, tl + 1, tl + 1, bg);
  cogito_draw_corner_fill(x + w - tr - 1, y + tr, tr, x + w - tr - 1, y, tr + 1, tr + 1, bg);
  cogito_draw_corner_fill(x + w - br - 1, y + h - br - 1, br, x + w - br - 1, y + h - br - 1, br + 1, br + 1, bg);
  cogito_draw_corner_fill(x + bl, y + h - bl - 1, bl, x, y + h - bl - 1, bl + 1, bl + 1, bg);
}

static void cogito_draw_rect_lines_radii(int x, int y, int w, int h, CogitoColor border, int tl, int tr, int br, int bl, int thickness) {
  if (w <= 0 || h <= 0 || thickness <= 0) return;
  cogito_clamp_corner_radii(w, h, &tl, &tr, &br, &bl);
  if (cogito_corner_radii_uniform(tl, tr, br, bl)) {
    cogito_draw_rect_lines(x, y, w, h, border, tl, thickness);
    return;
  }

  cogito_draw_rect_plain(x + tl, y, w - tl - tr, thickness, border);
  cogito_draw_rect_plain(x + bl, y + h - thickness, w - bl - br, thickness, border);
  cogito_draw_rect_plain(x, y + tl, thickness, h - tl - bl, border);
  cogito_draw_rect_plain(x + w - thickness, y + tr, thickness, h - tr - br, border);

  cogito_draw_corner_outline(x + tl, y + tl, tl, x, y, tl + thickness, tl + thickness, border, thickness);
  cogito_draw_corner_outline(x + w - tr - 1, y + tr, tr, x + w - tr - thickness, y, tr + thickness, tr + thickness, border, thickness);
  cogito_draw_corner_outline(x + w - br - 1, y + h - br - 1, br, x + w - br - thickness, y + h - br - thickness, br + thickness, br + thickness, border, thickness);
  cogito_draw_corner_outline(x + bl, y + h - bl - 1, bl, x, y + h - bl - thickness, bl + thickness, bl + thickness, border, thickness);
}

static void cogito_draw_rect_radii_aa(int x, int y, int w, int h, CogitoColor bg, int tl, int tr, int br, int bl);
static void cogito_draw_rect_lines_radii_aa(int x, int y, int w, int h, CogitoColor border, int tl, int tr, int br, int bl, int thickness);

static void cogito_resolve_node_radii(const CogitoNode* n, const CogitoStyle* s, int fallback_radius,
                                      int w, int h, int* out_tl, int* out_tr, int* out_br, int* out_bl) {
  int tl = fallback_radius;
  int tr = fallback_radius;
  int br = fallback_radius;
  int bl = fallback_radius;
  if (n) {
    if (n->radius_tl_set) tl = n->radius_tl;
    if (n->radius_tr_set) tr = n->radius_tr;
    if (n->radius_br_set) br = n->radius_br;
    if (n->radius_bl_set) bl = n->radius_bl;
  }
  if (s) {
    if (s->has_radius) {
      tl = s->radius;
      tr = s->radius;
      br = s->radius;
      bl = s->radius;
    }
    if (s->has_radius_tl) tl = s->radius_tl;
    if (s->has_radius_tr) tr = s->radius_tr;
    if (s->has_radius_br) br = s->radius_br;
    if (s->has_radius_bl) bl = s->radius_bl;
  }
  cogito_clamp_corner_radii(w, h, &tl, &tr, &br, &bl);
  if (out_tl) *out_tl = tl;
  if (out_tr) *out_tr = tr;
  if (out_br) *out_br = br;
  if (out_bl) *out_bl = bl;
}

static void cogito_draw_circle_clipped_node(const CogitoNode* n, const CogitoStyle* s,
                                            int fallback_radius, int cx, int cy, float r,
                                            CogitoColor color) {
  if (!n) return;
  int tl = fallback_radius;
  int tr = fallback_radius;
  int br = fallback_radius;
  int bl = fallback_radius;
  cogito_resolve_node_radii(n, s, fallback_radius, n->w, n->h, &tl, &tr, &br, &bl);
  cogito_draw_circle_clipped_radii(cx, cy, r, n->x, n->y, n->w, n->h, tl, tr, br, bl, color);
}

static void cogito_draw_rect_node(const CogitoNode* n, const CogitoStyle* s,
                                  int x, int y, int w, int h, CogitoColor bg, int fallback_radius) {
  int tl = fallback_radius, tr = fallback_radius, br = fallback_radius, bl = fallback_radius;
  cogito_resolve_node_radii(n, s, fallback_radius, w, h, &tl, &tr, &br, &bl);
  cogito_draw_rect_radii_aa(x, y, w, h, bg, tl, tr, br, bl);
}

static void cogito_draw_rect_lines_node(const CogitoNode* n, const CogitoStyle* s,
                                        int x, int y, int w, int h, CogitoColor border, int fallback_radius, int thickness) {
  int tl = fallback_radius, tr = fallback_radius, br = fallback_radius, bl = fallback_radius;
  cogito_resolve_node_radii(n, s, fallback_radius, w, h, &tl, &tr, &br, &bl);
  cogito_draw_rect_lines_radii_aa(x, y, w, h, border, tl, tr, br, bl, thickness);
}

static float cogito_shadow_gauss(float x, float sigma) {
  if (sigma <= 0.0001f) return 0.0f;
  return expf(-(x * x) / (2.0f * sigma * sigma));
}

static void cogito_draw_shadow_blur_radii(int x, int y, int w, int h,
                                           int tl, int tr, int br, int bl,
                                           int spread, int blur, CogitoColor color) {
  if (w <= 0 || h <= 0 || color.a == 0) return;
  if (blur < 0) blur = 0;
  int steps = blur > 0 ? blur : 1;
  if (steps > 48) steps = 48;
  float sigma = (float)(blur > 0 ? blur : 1) * 0.62f;

  for (int d = steps; d >= 0; d--) {
    float dist = (float)d;
    float weight = cogito_shadow_gauss(dist, sigma);
    int a = (int)lroundf((float)color.a * 2.2f * weight / (float)(steps + 1));
    if (a < 1) continue;
    if (a > 255) a = 255;
    CogitoColor c = color;
    c.a = (uint8_t)a;
    int s = spread + d;
    // Calculate shadow dimensions and radii
    int shadow_x = x - s;
    int shadow_y = y - s;
    int shadow_w = w + s * 2;
    int shadow_h = h + s * 2;
    int shadow_tl = tl + s;
    int shadow_tr = tr + s;
    int shadow_br = br + s;
    int shadow_bl = bl + s;
    // Ensure we don't have negative dimensions or radii
    if (shadow_w <= 0 || shadow_h <= 0) continue;
    if (shadow_tl < 0) shadow_tl = 0;
    if (shadow_tr < 0) shadow_tr = 0;
    if (shadow_br < 0) shadow_br = 0;
    if (shadow_bl < 0) shadow_bl = 0;
    cogito_draw_rect_radii(shadow_x, shadow_y, shadow_w, shadow_h, c, shadow_tl, shadow_tr, shadow_br, shadow_bl);
  }
}

static void cogito_draw_shadow_radii_level(int x, int y, int w, int h,
                                            int tl, int tr, int br, int bl,
                                            int level) {
  if (level <= 0 || w <= 0 || h <= 0) return;
  if (level > 5) level = 5;

  // Elevation shadow system
  // Each elevation uses two shadow layers: umbra + penumbra
  // Umbra: darker, closer shadow | Penumbra: lighter, softer shadow
  // CSS Elevation 3: box-shadow: 0 4px 8px 3px rgba(0,0,0,0.15), 0 1px 3px rgba(0,0,0,0.3);
  
  int umbra_alpha = 15;     // ~6% - darker, closer shadow
  int penumbra_alpha = 10;   // ~4% - lighter, softer outer shadow
  
  int umbra_y = 0, umbra_blur = 0, umbra_spread = 0;
  int penumbra_y = 0, penumbra_blur = 0, penumbra_spread = 0;
  
  switch(level) {
    case 1: // Elevation 1: subtle, flat surfaces
      umbra_y = 1; umbra_blur = 2; umbra_spread = 0;
      penumbra_y = 1; penumbra_blur = 3; penumbra_spread = -1;
      break;
    case 2: // Elevation 2: cards, buttons
      umbra_y = 1; umbra_blur = 3; umbra_spread = 0;
      penumbra_y = 2; penumbra_blur = 6; penumbra_spread = -2;
      break;
    case 3: // Elevation 3: FABs, dialogs
      umbra_y = 1; umbra_blur = 3; umbra_spread = 0;
      penumbra_y = 4; penumbra_blur = 8; penumbra_spread = -3;
      break;
    case 4: // Elevation 4: bottom sheets
      umbra_y = 2; umbra_blur = 4; umbra_spread = 0;
      penumbra_y = 6; penumbra_blur = 12; penumbra_spread = -4;
      break;
    case 5: // Elevation 5: dialogs, modals
      umbra_y = 3; umbra_blur = 5; umbra_spread = 0;
      penumbra_y = 8; penumbra_blur = 16; penumbra_spread = -5;
      break;
  }
  
  // Draw penumbra (outer, softer shadow - drawn first, behind umbra)
  CogitoColor penumbra_col = cogito_rgba(0, 0, 0, (uint8_t)penumbra_alpha);
  cogito_draw_shadow_blur_radii(x, y + penumbra_y, w, h, tl, tr, br, bl,
                                 penumbra_spread, penumbra_blur, penumbra_col);
  
  // Draw umbra (inner, darker shadow - drawn second, on top)
  CogitoColor umbra_col = cogito_rgba(0, 0, 0, (uint8_t)umbra_alpha);
  cogito_draw_shadow_blur_radii(x, y + umbra_y, w, h, tl, tr, br, bl,
                                 umbra_spread, umbra_blur, umbra_col);
}

static void cogito_draw_shadow_radii_box(int x, int y, int w, int h,
                                         int tl, int tr, int br, int bl,
                                         const CogitoBoxShadow* bs) {
  if (!bs || bs->inset || w <= 0 || h <= 0 || bs->color.a == 0) return;
  int dx = (int)lroundf(bs->dx);
  int dy = (int)lroundf(bs->dy);
  int spread = (int)lroundf(bs->spread);
  int blur = (int)lroundf(bs->blur);
  if (blur < 0) blur = 0;

  int bx = x - spread;
  int by = y - spread;
  int bw = w + spread * 2;
  int bh = h + spread * 2;
  int btl = tl + spread;
  int btr = tr + spread;
  int bbr = br + spread;
  int bbl = bl + spread;
  if (btl < 0) btl = 0;
  if (btr < 0) btr = 0;
  if (bbr < 0) bbr = 0;
  if (bbl < 0) bbl = 0;
  if (bw <= 0 || bh <= 0) return;

  cogito_draw_shadow_blur_radii(bx + dx, by + dy, bw, bh, btl, btr, bbr, bbl,
  0, blur, bs->color);
}

static void cogito_draw_shadow_node_level(const CogitoNode* n, const CogitoStyle* s,
                                          int x, int y, int w, int h, int fallback_radius, int level) {
  int tl = fallback_radius, tr = fallback_radius, br = fallback_radius, bl = fallback_radius;
  cogito_resolve_node_radii(n, s, fallback_radius, w, h, &tl, &tr, &br, &bl);
  cogito_draw_shadow_radii_level(x, y, w, h, tl, tr, br, bl, level);
}

static void cogito_draw_shadow_node_box(const CogitoNode* n, const CogitoStyle* s,
                                        int x, int y, int w, int h, int fallback_radius, const CogitoBoxShadow* bs) {
  int tl = fallback_radius, tr = fallback_radius, br = fallback_radius, bl = fallback_radius;
  cogito_resolve_node_radii(n, s, fallback_radius, w, h, &tl, &tr, &br, &bl);
  cogito_draw_shadow_radii_box(x, y, w, h, tl, tr, br, bl, bs);
}

static void cogito_draw_line(int x1, int y1, int x2, int y2, CogitoColor color) {
  if (cogito_draw_clip_active) {
    int min_x = (x1 < x2) ? x1 : x2;
    int min_y = (y1 < y2) ? y1 : y2;
    int max_x = (x1 > x2) ? x1 : x2;
    int max_y = (y1 > y2) ? y1 : y2;
    if (!cogito_rect_visible(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1,
                             cogito_draw_clip_x, cogito_draw_clip_y,
                             cogito_draw_clip_w, cogito_draw_clip_h)) {
      return;
    }
  }
  if (cogito_backend && cogito_backend->draw_line) {
    cogito_backend->draw_line(x1, y1, x2, y2, color,
    1);
  }
}

typedef bool (*CogitoAASampleFn)(double px, double py, void* user);

static int cogito_aa_coverage_4x4(int x, int y, CogitoAASampleFn sample_fn, void* user) {
  if (!sample_fn) return 0;
  static const double sub[
    4
  ] = {
    0.125,
    0.375,
    0.625,
    0.875
  };
  int hit = 0;
  for (int yi = 0; yi < 4; yi++) {
    for (int xi = 0; xi < 4; xi++) {
      double px = (double)x + sub[xi
      ];
      double py = (double)y + sub[yi
      ];
      if (sample_fn(px, py, user)) hit++;
    }
  }
  return hit;
}

static void cogito_draw_aa_bbox(int min_x, int min_y, int max_x, int max_y,
                                CogitoColor color,
                                CogitoAASampleFn sample_fn,
                                void* user) {
  if (!cogito_backend || !cogito_backend->draw_rect || color.a == 0) return;
  if (!sample_fn) return;
  if (min_x > max_x || min_y > max_y) return;
  const int samples = 16;
  for (int y = min_y; y <= max_y; y++) {
    for (int x = min_x; x <= max_x; x++) {
      int hit = cogito_aa_coverage_4x4(x, y, sample_fn, user);
      if (hit <= 0) continue;
      CogitoColor c = color;
      int a = (int)lround((double)color.a * ((double)hit / (double)samples));
      if (a < 1) continue;
      if (a > 255) a = 255;
      c.a = (uint8_t)a;
      cogito_backend->draw_rect(x, y,
      1,
      1, c);
    }
  }
}

typedef struct {
  double x0;
  double y0;
  double x1;
  double y1;
  double tl;
  double tr;
  double br;
  double bl;
} CogitoRectRadiiAACtx;

static bool cogito_rect_radii_contains(const CogitoRectRadiiAACtx* ctx, double px, double py) {
  if (!ctx) return false;
  if (px < ctx->x0 || py < ctx->y0 || px > ctx->x1 || py > ctx->y1) return false;

  if (ctx->tl > 0.0 && px < ctx->x0 + ctx->tl && py < ctx->y0 + ctx->tl) {
    double cx = ctx->x0 + ctx->tl;
    double cy = ctx->y0 + ctx->tl;
    double dx = px - cx;
    double dy = py - cy;
    if (dx * dx + dy * dy > ctx->tl * ctx->tl) return false;
  }

  if (ctx->tr > 0.0 && px > ctx->x1 - ctx->tr && py < ctx->y0 + ctx->tr) {
    double cx = ctx->x1 - ctx->tr;
    double cy = ctx->y0 + ctx->tr;
    double dx = px - cx;
    double dy = py - cy;
    if (dx * dx + dy * dy > ctx->tr * ctx->tr) return false;
  }

  if (ctx->br > 0.0 && px > ctx->x1 - ctx->br && py > ctx->y1 - ctx->br) {
    double cx = ctx->x1 - ctx->br;
    double cy = ctx->y1 - ctx->br;
    double dx = px - cx;
    double dy = py - cy;
    if (dx * dx + dy * dy > ctx->br * ctx->br) return false;
  }

  if (ctx->bl > 0.0 && px < ctx->x0 + ctx->bl && py > ctx->y1 - ctx->bl) {
    double cx = ctx->x0 + ctx->bl;
    double cy = ctx->y1 - ctx->bl;
    double dx = px - cx;
    double dy = py - cy;
    if (dx * dx + dy * dy > ctx->bl * ctx->bl) return false;
  }

  return true;
}

static bool cogito_rect_radii_sample(double px, double py, void* user) {
  CogitoRectRadiiAACtx* ctx = (CogitoRectRadiiAACtx*)user;
  return cogito_rect_radii_contains(ctx, px, py);
}

typedef struct {
  CogitoRectRadiiAACtx outer;
  CogitoRectRadiiAACtx inner;
  bool has_inner;
} CogitoRectRadiiOutlineAACtx;

static bool cogito_rect_radii_outline_sample(double px, double py, void* user) {
  CogitoRectRadiiOutlineAACtx* ctx = (CogitoRectRadiiOutlineAACtx*)user;
  if (!ctx) return false;
  if (!cogito_rect_radii_contains(&ctx->outer, px, py)) return false;
  if (ctx->has_inner && cogito_rect_radii_contains(&ctx->inner, px, py)) return false;
  return true;
}

static void cogito_draw_rect_radii_aa(int x, int y, int w, int h, CogitoColor bg, int tl, int tr, int br, int bl) {
  if (w <= 0 || h <= 0 || bg.a == 0) return;
  cogito_clamp_corner_radii(w, h, &tl, &tr, &br, &bl);
  if (!cogito_backend || !cogito_backend->draw_rect) {
    cogito_draw_rect_radii(x, y, w, h, bg, tl, tr, br, bl);
    return;
  }
  if (tl == 0 && tr == 0 && br == 0 && bl == 0) {
    cogito_draw_rect_plain(x, y, w, h, bg);
    return;
  }
  if (cogito_corner_radii_uniform(tl, tr, br, bl)) {
    cogito_draw_rect(x, y, w, h, bg, tl);
    return;
  }

  CogitoRectRadiiAACtx ctx;
  ctx.x0 = (double)x;
  ctx.y0 = (double)y;
  ctx.x1 = (double)(x + w);
  ctx.y1 = (double)(y + h);
  ctx.tl = (double)tl;
  ctx.tr = (double)tr;
  ctx.br = (double)br;
  ctx.bl = (double)bl;

  cogito_draw_aa_bbox(x - 1, y - 1, x + w, y + h, bg, cogito_rect_radii_sample, &ctx);
}

static void cogito_draw_rect_lines_radii_aa(int x, int y, int w, int h, CogitoColor border, int tl, int tr, int br, int bl, int thickness) {
  if (w <= 0 || h <= 0 || thickness <= 0 || border.a == 0) return;
  cogito_clamp_corner_radii(w, h, &tl, &tr, &br, &bl);
  if (!cogito_backend || !cogito_backend->draw_rect) {
    cogito_draw_rect_lines_radii(x, y, w, h, border, tl, tr, br, bl, thickness);
    return;
  }
  if (tl == 0 && tr == 0 && br == 0 && bl == 0) {
    cogito_draw_rect_lines(x, y, w, h, border,
    0, thickness);
    return;
  }
  if (cogito_corner_radii_uniform(tl, tr, br, bl)) {
    cogito_draw_rect_lines(x, y, w, h, border, tl, thickness);
    return;
  }

  CogitoRectRadiiOutlineAACtx ctx;
  ctx.outer.x0 = (double)x;
  ctx.outer.y0 = (double)y;
  ctx.outer.x1 = (double)(x + w);
  ctx.outer.y1 = (double)(y + h);
  ctx.outer.tl = (double)tl;
  ctx.outer.tr = (double)tr;
  ctx.outer.br = (double)br;
  ctx.outer.bl = (double)bl;

  int ix = x + thickness;
  int iy = y + thickness;
  int iw = w - thickness * 2;
  int ih = h - thickness * 2;
  ctx.has_inner = iw > 0 && ih > 0;
  if (ctx.has_inner) {
    int itl = tl - thickness;
    int itr = tr - thickness;
    int ibr = br - thickness;
    int ibl = bl - thickness;
    cogito_clamp_corner_radii(iw, ih, &itl, &itr, &ibr, &ibl);
    ctx.inner.x0 = (double)ix;
    ctx.inner.y0 = (double)iy;
    ctx.inner.x1 = (double)(ix + iw);
    ctx.inner.y1 = (double)(iy + ih);
    ctx.inner.tl = (double)itl;
    ctx.inner.tr = (double)itr;
    ctx.inner.br = (double)ibr;
    ctx.inner.bl = (double)ibl;
  }

  cogito_draw_aa_bbox(x - 1, y - 1, x + w, y + h, border, cogito_rect_radii_outline_sample, &ctx);
}

static double cogito_dist2_point_segment(double px, double py,
                                         double x0, double y0,
                                         double x1, double y1) {
  double vx = x1 - x0;
  double vy = y1 - y0;
  double wx = px - x0;
  double wy = py - y0;
  double c1 = wx * vx + wy * vy;
  if (c1 <= 0.0) return wx * wx + wy * wy;
  double c2 = vx * vx + vy * vy;
  if (c2 <= 1e-12) return wx * wx + wy * wy;
  if (c1 >= c2) {
    double dx = px - x1;
    double dy = py - y1;
    return dx * dx + dy * dy;
  }
  double t = c1 / c2;
  double projx = x0 + t * vx;
  double projy = y0 + t * vy;
  double dx = px - projx;
  double dy = py - projy;
  return dx * dx + dy * dy;
}

static double cogito_wave_y(double x, double x0, double cy,
                            double amp, double period, double phase) {
  return cy + sin(((x - x0) / period) * 6.28318530717958647692 + phase) * amp;
}

typedef struct {
  int x0;
  int x1;
  int seg_count;
  double cy;
  double amp;
  double period;
  double phase;
  double cover_r2;
  double start_y;
  double end_y;
} CogitoWavyLineAACtx;

static bool cogito_wavy_line_sample(double px, double py, void* user) {
  CogitoWavyLineAACtx* ctx = (CogitoWavyLineAACtx*)user;
  if (!ctx) return false;

  int k = (int)floor(px) - ctx->x0;
  int i0 = k - 3;
  int i1 = k + 3;
  if (i0 < 0) i0 = 0;
  if (i1 > ctx->seg_count - 1) i1 = ctx->seg_count - 1;

  for (int i = i0; i <= i1; i++) {
    double sx0 = (double)(ctx->x0 + i);
    double sx1 = sx0 + 1.0;
    double sy0 = cogito_wave_y(sx0, (double)ctx->x0, ctx->cy, ctx->amp, ctx->period, ctx->phase);
    double sy1 = cogito_wave_y(sx1, (double)ctx->x0, ctx->cy, ctx->amp, ctx->period, ctx->phase);
    if (cogito_dist2_point_segment(px, py, sx0, sy0, sx1, sy1) <= ctx->cover_r2) {
      return true;
    }
  }

  double dsx = px - (double)ctx->x0;
  double dsy = py - ctx->start_y;
  double dex = px - (double)ctx->x1;
  double dey = py - ctx->end_y;
  return (dsx * dsx + dsy * dsy <= ctx->cover_r2) ||
         (dex * dex + dey * dey <= ctx->cover_r2);
}

static void cogito_draw_wavy_line_aa(int x0, int x1, int cy,
                                     double thickness,
                                     double amp, double period, double phase,
                                     CogitoColor color) {
  if (!cogito_backend || !cogito_backend->draw_rect || color.a == 0) return;
  if (x1 <= x0 || thickness <= 0.0) return;
  if (period < 1.0) period = 1.0;

  const double half_t = thickness * 0.5;
  const double edge_pad = 0.35;
  const double cover_r = half_t + edge_pad;
  const double cover_r2 = cover_r * cover_r;
  double abs_amp = fabs(amp);

  int min_x = (int)floor((double)x0 - cover_r) - 1;
  int max_x = (int)ceil((double)x1 + cover_r) + 1;
  int min_y = (int)floor((double)cy - abs_amp - cover_r) - 1;
  int max_y = (int)ceil((double)cy + abs_amp + cover_r) + 1;

  int seg_count = x1 - x0;
  if (seg_count < 1) return;

  CogitoWavyLineAACtx ctx;
  ctx.x0 = x0;
  ctx.x1 = x1;
  ctx.seg_count = seg_count;
  ctx.cy = (double)cy;
  ctx.amp = amp;
  ctx.period = period;
  ctx.phase = phase;
  ctx.cover_r2 = cover_r2;
  ctx.start_y = cogito_wave_y((double)x0, (double)x0, (double)cy, amp, period, phase);
  ctx.end_y = cogito_wave_y((double)x1, (double)x0, (double)cy, amp, period, phase);
  cogito_draw_aa_bbox(min_x, min_y, max_x, max_y, color, cogito_wavy_line_sample, &ctx);
}

static double cogito_norm_angle_rad(double a) {
  const double tau = 6.28318530717958647692;
  while (a < 0.0) a += tau;
  while (a >= tau) a -= tau;
  return a;
}

static bool cogito_angle_in_arc_rad(double a, double start, double end) {
  a = cogito_norm_angle_rad(a);
  start = cogito_norm_angle_rad(start);
  end = cogito_norm_angle_rad(end);
  if (start <= end) return a >= start && a <= end;
  return a >= start || a <= end;
}

static double cogito_ring_wave_radius(double base_r, bool wavy, double wave_amp, double wave_phase, double angle, double start_angle) {
  if (!wavy) return base_r;
  double local = angle - start_angle;
  while (local < 0.0) local += 6.28318530717958647692;
  return base_r + sin(local * 6.0 + wave_phase) * wave_amp;
}

typedef struct {
  int cx;
  int cy;
  double base_r;
  double half_t;
  double start;
  double end;
  double start_norm;
  bool full_arc;
  bool wavy;
  double wave_phase;
  double wave_amp;
  double edge_pad;
  double sx;
  double sy;
  double ex;
  double ey;
  double cap_r2;
} CogitoRingArcAACtx;

static bool cogito_ring_arc_sample(double px, double py, void* user) {
  CogitoRingArcAACtx* ctx = (CogitoRingArcAACtx*)user;
  if (!ctx) return false;

  double dx = px - (double)ctx->cx;
  double dy = py - (double)ctx->cy;
  double dist = sqrt(dx * dx + dy * dy);
  double angle = cogito_norm_angle_rad(atan2(dy, dx));

  bool covered = false;
  if (ctx->full_arc || cogito_angle_in_arc_rad(angle, ctx->start, ctx->end)) {
    double rr = cogito_ring_wave_radius(ctx->base_r, ctx->wavy, ctx->wave_amp, ctx->wave_phase, angle, ctx->start_norm);
    double inner = rr - ctx->half_t;
    double outer = rr + ctx->half_t;
    covered = (dist >= inner - ctx->edge_pad && dist <= outer + ctx->edge_pad);
  }
  if (!covered && !ctx->full_arc) {
    double dsx = px - ctx->sx;
    double dsy = py - ctx->sy;
    double dex = px - ctx->ex;
    double dey = py - ctx->ey;
    covered = (dsx * dsx + dsy * dsy <= ctx->cap_r2) || (dex * dex + dey * dey <= ctx->cap_r2);
  }
  return covered;
}

static void cogito_draw_ring_arc_aa(int cx, int cy,
                                    double base_r, double thickness,
                                    double start_deg, double end_deg,
                                    CogitoColor color,
                                    bool wavy, double wave_phase, double wave_amp) {
  if (!cogito_backend || !cogito_backend->draw_rect || color.a == 0) return;
  if (thickness <= 0.0 || base_r <= 0.0) return;

  const double deg_to_rad = 0.01745329251994329577;
  const double tau = 6.28318530717958647692;
  const double start = start_deg * deg_to_rad;
  const double end = end_deg * deg_to_rad;
  const double start_norm = cogito_norm_angle_rad(start);
  double span = fabs(end - start);
  while (span > tau) span -= tau;
  bool full_arc = span >= (tau - 1e-4);
  const double half_t = thickness * 0.5;

  if (half_t <= 0.0) return;

  double wave_extra = wavy ? (fabs(wave_amp) + 1.5) : 1.0;
  double outer_bound = base_r + half_t + wave_extra;
  if (outer_bound < 1.0) outer_bound = 1.0;

  int min_x = (int)floor((double)cx - outer_bound) - 1;
  int max_x = (int)ceil((double)cx + outer_bound) + 1;
  int min_y = (int)floor((double)cy - outer_bound) - 1;
  int max_y = (int)ceil((double)cy + outer_bound) + 1;

  double sx = 0.0;
  double sy = 0.0;
  double ex = 0.0;
  double ey = 0.0;
  const double edge_pad = 0.35;
  double cap_r2 = (half_t + edge_pad) * (half_t + edge_pad);
  if (!full_arc) {
    double start_rr = cogito_ring_wave_radius(base_r, wavy, wave_amp, wave_phase, cogito_norm_angle_rad(start), start_norm);
    double end_rr = cogito_ring_wave_radius(base_r, wavy, wave_amp, wave_phase, cogito_norm_angle_rad(end), start_norm);
    sx = (double)cx + cos(start) * start_rr;
    sy = (double)cy + sin(start) * start_rr;
    ex = (double)cx + cos(end) * end_rr;
    ey = (double)cy + sin(end) * end_rr;
  }

  CogitoRingArcAACtx ctx;
  ctx.cx = cx;
  ctx.cy = cy;
  ctx.base_r = base_r;
  ctx.half_t = half_t;
  ctx.start = start;
  ctx.end = end;
  ctx.start_norm = start_norm;
  ctx.full_arc = full_arc;
  ctx.wavy = wavy;
  ctx.wave_phase = wave_phase;
  ctx.wave_amp = wave_amp;
  ctx.edge_pad = edge_pad;
  ctx.sx = sx;
  ctx.sy = sy;
  ctx.ex = ex;
  ctx.ey = ey;
  ctx.cap_r2 = cap_r2;
  cogito_draw_aa_bbox(min_x, min_y, max_x, max_y, color, cogito_ring_arc_sample, &ctx);
}

static const char* cogito_icon_text_fallback(const char* icon) {
  if (!icon || !icon[
    0
  ]) return "?";
  if (strncmp(icon,
  "sf:",
  3) == 0) return cogito_icon_text_fallback(icon + 3);
  if (strstr(icon,
  "textformat.bold") || strstr(icon,
  "format-text-bold") || strcmp(icon,
  "bold") == 0) return "B";
  if (strstr(icon,
  "textformat.italic") || strstr(icon,
  "format-text-italic") || strcmp(icon,
  "italic") == 0) return "I";
  if (strstr(icon,
  "textformat.underline") || strstr(icon,
  "format-text-underline") || strcmp(icon,
  "underline") == 0) return "U";
  if (strstr(icon,
  "textformat") || strstr(icon,
  "format-text-size") || strstr(icon,
  "character")) return "Aa";
  if (strstr(icon,
  "window-close")) return "x";
  if (strstr(icon,
  "window-minimize")) return "-";
  if (strstr(icon,
  "window-maximize")) return "+";
  if (strcmp(icon,
  "x") == 0 || strstr(icon,
  "xmark")) return "x";
  if (strstr(icon,
  "plus")) return "+";
  if (strstr(icon,
  "minus")) return "-";
  if (strstr(icon,
  "add")) return "+";
  if (strstr(icon,
  "remove")) return "-";
  if (strstr(icon,
  "check")) return "v";
  if (strstr(icon,
  "star")) return "*";
  if (strstr(icon,
  "photo") || strstr(icon,
  "image")) return "[]";
  if (strstr(icon,
  "search") || strstr(icon,
  "magnifyingglass")) return "o";
  if (strstr(icon,
  "chevron") || strstr(icon,
  "pan-")) return "v";
  if (strstr(icon,
  "arrow")) return ">";
  if (strstr(icon,
  "question") || strstr(icon,
  "help")) return "?";
  if (strstr(icon,
  "equal")) return "=";
  return icon;
}

static void cogito_draw_icon_fallback(CogitoNode* n, const char* icon, int x, int y, int size, CogitoColor color) {
  if (!n || !icon || !icon[
    0
  ] || size <= 0) return;
  if (cogito_backend && cogito_backend->draw_texture) {
    CogitoTexture* tex = NULL;
    int tw = 0;
    int th = 0;
    if (cogito_icon_load_texture(icon, size, &tex, &tw, &th) && tex) {
      CogitoRect src = {
        0,
        0, tw, th
      };
      int dx = x;
      int dy = y;
      int dw = size;
      int dh = size;
      if (tw > 0 && th > 0) {
        if (tw > th) {
          dh = (size * th + tw / 2) / tw;
          if (dh < 1) dh = 1;
          dy = y + (size - dh) / 2;
        } else if (th > tw) {
          dw = (size * tw + th / 2) / th;
          if (dw < 1) dw = 1;
          dx = x + (size - dw) / 2;
        }
      }
      CogitoRect dst = {dx, dy, dw, dh
      };
      if (cogito_draw_clip_active &&
          !cogito_rect_visible(dst.x, dst.y, dst.w, dst.h,
                               cogito_draw_clip_x, cogito_draw_clip_y,
                               cogito_draw_clip_w, cogito_draw_clip_h)) {
        return;
      }
      cogito_backend->draw_texture(tex, src, dst, color);
      return;
    }
  }
  const char* icon_name = (strncmp(icon,
  "sf:",
  3) == 0) ? icon + 3 : icon;

  if (strstr(icon_name,
  "magnifyingglass") || strstr(icon_name,
  "search")) {
    int cx = x + size / 2 - 1;
    int cy = y + size / 2 - 1;
    int r = size / 2 - 3;
    if (r < 2) r = 2;
    cogito_draw_rect(cx - r, cy - r, r * 2, r * 2, color, r);
    cogito_draw_line(x + size - 4, y + size - 4, x + size, y + size, color);
    return;
  }
  if (strstr(icon_name,
  "plus") || strstr(icon_name,
  "add")) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    int r = size / 2 - 2;
    if (r < 2) r = 2;
    cogito_draw_line(cx - r, cy, cx + r, cy, color);
    cogito_draw_line(cx, cy - r, cx, cy + r, color);
    return;
  }
  if (strstr(icon_name,
  "minus") || strstr(icon_name,
  "remove")) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    int r = size / 2 - 2;
    if (r < 2) r = 2;
    cogito_draw_line(cx - r, cy, cx + r, cy, color);
    return;
  }
  if (strstr(icon_name,
  "xmark") || strcmp(icon_name,
  "x") == 0 || strstr(icon_name,
  "window-close")) {
    cogito_draw_line(x, y, x + size, y + size, color);
    cogito_draw_line(x + size, y, x, y + size, color);
    return;
  }
  if (strstr(icon_name,
  "photo") || strstr(icon_name,
  "image")) {
    int r = size / 8;
    if (r < 2) r = 2;
    cogito_draw_rect_lines(x, y, size, size, color, r,
    1);
    cogito_draw_line(x + 3, y + size - 4, x + size / 2, y + size / 2, color);
    cogito_draw_line(x + size / 2, y + size / 2, x + size - 3, y + size - 6, color);
    return;
  }
  if (strstr(icon_name,
  "chevron") || strstr(icon_name,
  "pan-")) {
    int mid = x + size / 2;
    int top = y + size / 3;
    int bot = y + (size * 2) / 3;
    cogito_draw_line(mid - 4, top, mid, bot, color);
    cogito_draw_line(mid, bot, mid + 4, top, color);
    return;
  }

  int fs = size;
  if (fs < 10) fs = 10;
  const char* txt = cogito_icon_text_fallback(icon);
  int tw = cogito_text_width_size_node(n, txt, fs);
  int th = cogito_text_height_size(fs);
  int tx = x + (size - tw) / 2;
  int ty = y + (size - th) / 2;
  cogito_draw_text_size_node(n, tx, ty, txt, color, fs, false);
}
// Window decoration (traffic lights / CSD buttons)
#include "widgets/util/traffic_lights.inc"
// Inspector panel (debug overlay)
#include "widgets/util/inspector.inc"
// Scrollbar geometry and hit-test (overlay scrollbars)
#include "widgets/util/scrollbar.inc"

static void cogito_draw_node(CogitoNode* n);

#include "widgets/draw/toasts.inc"

static int cogito_shape_intersections(const float* px, const float* py, int point_count, int y, float* out_xs, int max_xs) {
  if (!px || !py || point_count < 3 || !out_xs || max_xs <= 0) return 0;
  int count = 0;
  float fy = (float)y + 0.5f;
  for (int i = 0; i < point_count; i++) {
    int j = (i + 1) % point_count;
    float y1 = py[i];
    float y2 = py[j];
    if ((fy >= y1 && fy < y2) || (fy >= y2 && fy < y1)) {
      float t = (fy - y1) / (y2 - y1);
      float x = px[i] + t * (px[j] - px[i]);
      if (count < max_xs) out_xs[count++] = x;
    }
  }
  for (int i = 0; i < count; i++) {
    for (int j = i + 1; j < count; j++) {
      if (out_xs[j] < out_xs[i]) {
        float tmp = out_xs[i];
        out_xs[i] = out_xs[j];
        out_xs[j] = tmp;
      }
    }
  }
  // Collapse near-duplicate intersections (vertex-touch artifacts) so they do
  // not create accidental 1px spans when paired.
  const float dedup_eps = 1e-3f;
  int uniq = 0;
  for (int i = 0; i < count; i++) {
    if (uniq == 0 || fabsf(out_xs[i] - out_xs[uniq - 1]) > dedup_eps) {
      out_xs[uniq++] = out_xs[i];
    }
  }
  // Keep an even number for interval pairing.
  if (uniq & 1) uniq--;
  return uniq;
}

static bool cogito_shape_point_inside_evenodd(const float* px, const float* py, int point_count, float x, float y) {
  bool inside = false;
  for (int i = 0, j = point_count - 1; i < point_count; j = i++) {
    float xi = px[i], yi = py[i];
    float xj = px[j], yj = py[j];
    bool crosses = ((yi > y) != (yj > y));
    if (!crosses) continue;
    float xint = (xj - xi) * (y - yi) / (yj - yi) + xi;
    if (x < xint) inside = !inside;
  }
  return inside;
}

static int cogito_shape_pixel_coverage_4x4(const float* px, const float* py, int point_count, int x, int y) {
  const float offs[4] = {0.125f, 0.375f, 0.625f, 0.875f};
  int hit = 0;
  for (int oy = 0; oy < 4; oy++) {
    for (int ox = 0; ox < 4; ox++) {
      float sx = (float)x + offs[ox];
      float sy = (float)y + offs[oy];
      if (cogito_shape_point_inside_evenodd(px, py, point_count, sx, sy)) hit++;
    }
  }
  return hit;
}

#define COGITO_SHAPE_MAX_POINTS COGITO_SHAPE_SVG_POLY_MAX

static int cogito_shape_build_smooth_points(const float* px, const float* py, int point_count, float* out_x, float* out_y, int max_out, bool smooth_points) {
  if (!px || !py || !out_x || !out_y || point_count < 3 || max_out < point_count) return 0;
  if (point_count > COGITO_SHAPE_MAX_POINTS) point_count = COGITO_SHAPE_MAX_POINTS;
  if (max_out > COGITO_SHAPE_MAX_POINTS) max_out = COGITO_SHAPE_MAX_POINTS;

  float work_x[COGITO_SHAPE_MAX_POINTS];
  float work_y[COGITO_SHAPE_MAX_POINTS];
  int work_n = point_count;
  for (int i = 0; i < work_n; i++) {
    work_x[i] = px[i];
    work_y[i] = py[i];
  }

  if (smooth_points) {
    // Chaikin corner cutting for closed paths: smooths without spline overshoot.
    while ((work_n * 2) <= max_out) {
      float next_x[COGITO_SHAPE_MAX_POINTS];
      float next_y[COGITO_SHAPE_MAX_POINTS];
      int next_n = 0;
      for (int i = 0; i < work_n; i++) {
        int j = (i + 1) % work_n;
        float x0 = work_x[i];
        float y0 = work_y[i];
        float x1 = work_x[j];
        float y1 = work_y[j];
        if (next_n + 1 >= COGITO_SHAPE_MAX_POINTS) break;
        next_x[next_n] = 0.75f * x0 + 0.25f * x1;
        next_y[next_n] = 0.75f * y0 + 0.25f * y1;
        next_n++;
        next_x[next_n] = 0.25f * x0 + 0.75f * x1;
        next_y[next_n] = 0.25f * y0 + 0.75f * y1;
        next_n++;
      }
      if (next_n < 3) break;
      work_n = next_n;
      for (int i = 0; i < work_n; i++) {
        work_x[i] = next_x[i];
        work_y[i] = next_y[i];
      }
    }
  }

  for (int i = 0; i < work_n; i++) {
    out_x[i] = work_x[i];
    out_y[i] = work_y[i];
  }
  if (smooth_points) {
    const float k = 0.06f;
    float tmp_x[COGITO_SHAPE_MAX_POINTS];
    float tmp_y[COGITO_SHAPE_MAX_POINTS];
    for (int i = 0; i < work_n; i++) {
      int ip = (i - 1 + work_n) % work_n;
      int in = (i + 1) % work_n;
      float avg_x = (out_x[ip] + out_x[in]) * 0.5f;
      float avg_y = (out_y[ip] + out_y[in]) * 0.5f;
      tmp_x[i] = out_x[i] * (1.0f - k) + avg_x * k;
      tmp_y[i] = out_y[i] * (1.0f - k) + avg_y * k;
    }
    for (int i = 0; i < work_n; i++) {
      out_x[i] = tmp_x[i];
      out_y[i] = tmp_y[i];
    }
  }
  return work_n;
}

static void cogito_draw_shape_polygon(const float* px, const float* py, int in_point_count, CogitoColor fill, CogitoColor stroke, int stroke_w, bool smooth_points, bool antialias) {
  if (!px || !py) return;
  int ctrl_count = in_point_count;
  if (ctrl_count < 3) return;
  if (ctrl_count > COGITO_SHAPE_MAX_POINTS) ctrl_count = COGITO_SHAPE_MAX_POINTS;
  float smooth_x[COGITO_SHAPE_MAX_POINTS];
  float smooth_y[COGITO_SHAPE_MAX_POINTS];
  int point_count = cogito_shape_build_smooth_points(px, py, ctrl_count, smooth_x, smooth_y, COGITO_SHAPE_MAX_POINTS, smooth_points);
  const float* draw_x = px;
  const float* draw_y = py;
  if (point_count >= 3) {
    draw_x = smooth_x;
    draw_y = smooth_y;
  } else {
    point_count = ctrl_count;
  }

  float min_xf = draw_x[0], max_xf = draw_x[0];
  float min_yf = draw_y[0], max_yf = draw_y[0];
  for (int i = 1; i < point_count; i++) {
    if (draw_x[i] < min_xf) min_xf = draw_x[i];
    if (draw_x[i] > max_xf) max_xf = draw_x[i];
    if (draw_y[i] < min_yf) min_yf = draw_y[i];
    if (draw_y[i] > max_yf) max_yf = draw_y[i];
  }
  float w = max_xf - min_xf;
  float h = max_yf - min_yf;
  float s = (w < h) ? w : h;
  if (s < 1.0f) s = 1.0f;
  float tol = 32.0f / s;
  if (tol < 0.25f) tol = 0.25f;
  if (tol > 1.0f) tol = 1.0f;

  int min_x = (int)floorf(min_xf);
  int max_x = (int)floorf(max_xf);

  int min_y = INT_MAX;
  int max_y = INT_MIN;
  for (int i = 0; i < point_count; i++) {
    int iy = (int)floorf(draw_y[i]);
    if (iy < min_y) min_y = iy;
    if (iy > max_y) max_y = iy;
  }
  if (min_y > max_y) return;
  if (min_x > max_x) return;

  int bbox_w = (max_x - min_x + 1);
  int bbox_h = (max_y - min_y + 1);
  if (bbox_w < 1) bbox_w = 1;
  if (bbox_h < 1) bbox_h = 1;
  int bbox_area = bbox_w * bbox_h;

  // 4x4 supersampling is accurate but very expensive for large polygons.
  // Keep it for small shapes and fall back to the cheaper AA scanline path
  // for larger ones.
  const int ssaa_area_limit = 4096;
  if (antialias && !smooth_points) {
    const int samples = 16;
    if (bbox_area <= ssaa_area_limit) {
      // Small literal SVG-like shapes: full 4x4 SSAA for max fidelity.
      for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
          int hit = cogito_shape_pixel_coverage_4x4(draw_x, draw_y, point_count, x, y);
          if (hit <= 0) continue;
          CogitoColor c = fill;
          int a = (int)lroundf((float)fill.a * ((float)hit / (float)samples));
          if (a <= 0) continue;
          if (a > 255) a = 255;
          c.a = (uint8_t)a;
          cogito_backend->draw_rect(x, y, 1, 1, c);
        }
      }
      goto shape_stroke;
    }

    // Large shapes: adaptive SSAA.
    // Fill interior spans directly and supersample only the edge pixels.
    float xs_adapt[64];
    for (int y = min_y; y <= max_y; y++) {
      int n = cogito_shape_intersections(draw_x, draw_y, point_count, y, xs_adapt, 64);
      for (int i = 0; i + 1 < n; i += 2) {
        float x0 = xs_adapt[i];
        float x1 = xs_adapt[i + 1];
        if (x1 < x0) {
          float t = x0;
          x0 = x1;
          x1 = t;
        }
        if (x1 <= x0) continue;

        int left = (int)floorf(x0);
        int right = (int)floorf(x1 - 1e-4f);
        if (right < left) continue;

        int span_w = right - left + 1;
        if (span_w <= 2) {
          // Very thin spans: sample every pixel.
          for (int x = left; x <= right; x++) {
            int hit = cogito_shape_pixel_coverage_4x4(draw_x, draw_y, point_count, x, y);
            if (hit <= 0) continue;
            CogitoColor c = fill;
            int a = (int)lroundf((float)fill.a * ((float)hit / (float)samples));
            if (a <= 0) continue;
            if (a > 255) a = 255;
            c.a = (uint8_t)a;
            cogito_backend->draw_rect(x, y, 1, 1, c);
          }
          continue;
        }

        // Fill interior in one call.
        int interior_x = left + 1;
        int interior_w = span_w - 2;
        if (interior_w > 0) {
          cogito_backend->draw_rect(interior_x, y, interior_w, 1, fill);
        }

        // Sample only left and right edge pixels.
        int hit_l = cogito_shape_pixel_coverage_4x4(draw_x, draw_y, point_count, left, y);
        if (hit_l > 0) {
          CogitoColor c = fill;
          int a = (int)lroundf((float)fill.a * ((float)hit_l / (float)samples));
          if (a > 255) a = 255;
          if (a > 0) {
            c.a = (uint8_t)a;
            cogito_backend->draw_rect(left, y, 1, 1, c);
          }
        }

        int hit_r = cogito_shape_pixel_coverage_4x4(draw_x, draw_y, point_count, right, y);
        if (hit_r > 0) {
          CogitoColor c = fill;
          int a = (int)lroundf((float)fill.a * ((float)hit_r / (float)samples));
          if (a > 255) a = 255;
          if (a > 0) {
            c.a = (uint8_t)a;
            cogito_backend->draw_rect(right, y, 1, 1, c);
          }
        }
      }
    }
    goto shape_stroke;
  }

  float xs[64];
  for (int y = min_y; y <= max_y; y++) {
    int n = cogito_shape_intersections(draw_x, draw_y, point_count, y, xs, 64);
    for (int i = 0; i + 1 < n; i += 2) {
      float x0 = xs[i];
      float x1 = xs[i + 1];
      if (x1 < x0) {
        float t = x0;
        x0 = x1;
        x1 = t;
      }
      if (!cogito_backend || !cogito_backend->draw_rect || x1 <= x0) continue;
      if (!antialias) {
        int ix0 = (int)lroundf(x0);
        int ix1 = (int)lroundf(x1);
        if (ix1 >= ix0) {
          cogito_backend->draw_rect(ix0, y, ix1 - ix0 + 1, 1, fill);
        }
        continue;
      }
      // Half-open interval fill [x0, x1): avoid right-edge overdraw that can
      // manifest as occasional stray single pixels at sharp corners.
      const float edge_eps = tol * 0.5f;
      x0 += edge_eps;
      x1 -= edge_eps;
      if (x1 <= x0) continue;
      int ix0 = (int)floorf(x0);
      int ix1 = (int)floorf(x1 - edge_eps);
      for (int x = ix0; x <= ix1; x++) {
        float cover = fminf(x1, (float)x + 1.0f) - fmaxf(x0, (float)x);
        if (cover <= 0.0f) continue;
        if (cover > 1.0f) cover = 1.0f;
        // Guard against tiny sliver coverage that can show up as full-pixel
        // spurs on sharp corners depending on backend blending behavior.
        float min_cover = tol;
        if (min_cover > 1.0f) min_cover = 1.0f;
        if (cover < min_cover) continue;
        CogitoColor c = fill;
        int a = (int)lroundf((float)fill.a * cover);
        if (a <= 0) continue;
        if (a > 255) a = 255;
        c.a = (uint8_t)a;
        cogito_backend->draw_rect(x, y, 1, 1, c);
      }
    }
  }
shape_stroke:
  if (stroke_w > 0) {
    for (int i = 0; i < point_count; i++) {
      int j = (i + 1) % point_count;
      cogito_draw_line((int)lroundf(draw_x[i]), (int)lroundf(draw_y[i]),
                       (int)lroundf(draw_x[j]), (int)lroundf(draw_y[j]),
                       stroke);
      if (stroke_w > 1) {
        cogito_draw_line((int)lroundf(draw_x[i]) + 1, (int)lroundf(draw_y[i]),
                         (int)lroundf(draw_x[j]) + 1, (int)lroundf(draw_y[j]),
                         stroke);
      }
    }
  }
}

static void cogito_draw_node(CogitoNode* n) {
  if (!n) return;
  if (cogito_draw_count_enabled) cogito_draw_count_nodes++;
  // Use the cached main pointer position (set once per frame in the event loop)
  // instead of calling get_mouse_position per-node which is a backend API call.
  if (n->tooltip && n->tooltip->data && n->tooltip->data[
    0
  ] && !n->disabled) {
    if (cogito_main_pointer_valid &&
        cogito_hit_node(n, cogito_main_pointer_x, cogito_main_pointer_y)) {
      cogito_tooltip_hovered = n;
    }
  }
  switch (n->kind) {
    #include "widgets/draw/button.inc"
    #include "widgets/draw/carousel.inc"
    #include "widgets/draw/carousel_item.inc"
    #include "widgets/draw/iconbtn.inc"
    #include "widgets/draw/image.inc"
    #include "widgets/draw/label.inc"
    #include "widgets/draw/textfield.inc"
    #include "widgets/draw/textview.inc"
    #include "widgets/draw/dropdown.inc"
    #include "widgets/draw/datepicker.inc"
    #include "widgets/draw/fab.inc"
    #include "widgets/draw/fab_menu.inc"
    #include "widgets/draw/nav_rail.inc"
    #include "widgets/draw/bottom_nav.inc"
    #include "widgets/draw/slider.inc"
    #include "widgets/draw/colorpicker.inc"
    #include "widgets/draw/tabs.inc"
    #include "widgets/draw/progress.inc"
    #include "widgets/draw/toast.inc"
    #include "widgets/draw/checkbox.inc"
    #include "widgets/draw/chip.inc"
    #include "widgets/draw/switch.inc"
    #include "widgets/draw/list.inc"
    #include "widgets/draw/grid.inc"
    #include "widgets/draw/appbar.inc"
    #include "widgets/draw/dialog.inc"
    #include "widgets/draw/popover.inc"
    #include "widgets/draw/toolbar.inc"
    #include "widgets/draw/dialog_slot.inc"
    #include "widgets/draw/view_switcher.inc"
    #include "widgets/draw/drawing_area.inc"
    #include "widgets/draw/shape.inc"

case COGITO_TOASTS:
      if (!cogito_toasts_force_draw) {
          CogitoNode* win = cogito_node_window(n);
          if (win) break;
      }
      for (size_t i = 0; i < n->len; i++) {
          cogito_draw_node(n->children[i]);
      }
      break;
    #include "widgets/draw/vstack.inc"
    #include "widgets/draw/hstack.inc"
    #include "widgets/draw/zstack.inc"
    #include "widgets/draw/fixed.inc"
    #include "widgets/draw/window.inc"
    #include "widgets/draw/scroller.inc"
    #include "widgets/draw/searchfield.inc"
    #include "widgets/draw/stepper.inc"
    #include "widgets/draw/treeview.inc"
    #include "widgets/draw/tooltip.inc"
    #include "widgets/draw/divider.inc"
    #include "widgets/draw/menu_section.inc"
    #include "widgets/draw/card.inc"
    #include "widgets/draw/avatar.inc"
    #include "widgets/draw/badge.inc"
    #include "widgets/draw/banner.inc"
    #include "widgets/draw/bottom_sheet.inc"
    #include "widgets/draw/side_sheet.inc"
    #include "widgets/draw/timepicker.inc"
    #include "widgets/draw/active_indicator.inc"
    #include "widgets/draw/switchbar.inc"
    #include "widgets/draw/content_list.inc"
    #include "widgets/draw/empty_page.inc"
    #include "widgets/draw/tip_view.inc"
    #include "widgets/draw/settings_window.inc"
    #include "widgets/draw/settings_page.inc"
    #include "widgets/draw/settings_list.inc"
    #include "widgets/draw/settings_row.inc"
    #include "widgets/draw/welcome_screen.inc"
    #include "widgets/draw/view_dual.inc"
    #include "widgets/draw/view_chooser.inc"
    #include "widgets/draw/about_window.inc"
    #include "widgets/draw/split_button.inc"
    #include "widgets/draw/buttongroup.inc"
    case COGITO_KIND_COUNT:
      break;
  }
}
// Overlay drawing (menu, tooltip)
#include "widgets/draw/menu.inc"
#include "widgets/draw/tooltip_overlay.inc"
