static void cogito_draw_line(int x1, int y1, int x2, int y2, CogitoColor color);
static void cogito_draw_rect(int x, int y, int w, int h, CogitoColor bg,
                             int radius);
static void cogito_draw_rect_linear_gradient(int x, int y, int w, int h,
                                             CogitoColor start,
                                             CogitoColor end,
                                             float angle_deg);
static void cogito_draw_rect_radial_gradient(int x, int y, int w, int h,
                                             CogitoColor inner,
                                             CogitoColor outer,
                                             float center_x, float center_y,
                                             float radius);
static void cogito_draw_rect_lines(int x, int y, int w, int h,
                                   CogitoColor border, int radius,
                                   int thickness);

static CogitoNode* cogito_drawing_area_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_DRAWING_AREA);
  n->kd->drawing_area.pressed = false;
  n->kd->drawing_area.dragging = false;
  n->kd->drawing_area.pointer_x = 0;
  n->kd->drawing_area.pointer_y = 0;
  n->kd->drawing_area.press_x = 0;
  n->kd->drawing_area.press_y = 0;
  n->kd->drawing_area.point_count = 0;
  n->kd->drawing_area.canvas_color = cogito_color(0, 0, 0, 255);
  n->kd->drawing_area.canvas_line_width = 1;
  return n;
}

static int cogito_drawing_area_clamp_x(CogitoNode* n, int x) {
  if (!n) return 0;
  if (x < 0) return 0;
  int max_x = n->w > 0 ? (n->w - 1) : 0;
  if (x > max_x) return max_x;
  return x;
}

static int cogito_drawing_area_clamp_y(CogitoNode* n, int y) {
  if (!n) return 0;
  if (y < 0) return 0;
  int max_y = n->h > 0 ? (n->h - 1) : 0;
  if (y > max_y) return max_y;
  return y;
}

static void cogito_drawing_area_add_point(CogitoNode* n, int x, int y) {
  if (!n || n->kind != COGITO_DRAWING_AREA) return;
  if (n->kd->drawing_area.point_count >= COGITO_DRAWING_AREA_MAX_POINTS) return;
  n->kd->drawing_area.points_x[n->kd->drawing_area.point_count] = cogito_drawing_area_clamp_x(n, x);
  n->kd->drawing_area.points_y[n->kd->drawing_area.point_count] = cogito_drawing_area_clamp_y(n, y);
  n->kd->drawing_area.point_count++;
}

static void cogito_drawing_area_begin_stroke(CogitoNode* n, int x, int y) {
  if (!n || n->kind != COGITO_DRAWING_AREA) return;
  if (n->kd->drawing_area.point_count > 0 &&
      n->kd->drawing_area.point_count < COGITO_DRAWING_AREA_MAX_POINTS) {
    n->kd->drawing_area.points_x[n->kd->drawing_area.point_count] = -1;
    n->kd->drawing_area.points_y[n->kd->drawing_area.point_count] = -1;
    n->kd->drawing_area.point_count++;
  }
  cogito_drawing_area_add_point(n, x, y);
}

static void cogito_drawing_area_clear_points(CogitoNode* n) {
  if (!n || n->kind != COGITO_DRAWING_AREA) return;
  n->kd->drawing_area.point_count = 0;
}

static YisVal cogito_drawing_area_new(void) {
  return YV_OBJ(cogito_drawing_area_new_obj());
}

static YisVal cogito_drawing_area_get_x(YisVal dav) {
  if (dav.tag != EVT_OBJ) yis_trap("cogito.drawing_area_get_x expects drawing_area");
  CogitoNode* n = (CogitoNode*)dav.as.p;
  return YV_INT(n->kd->drawing_area.pointer_x);
}

static YisVal cogito_drawing_area_get_y(YisVal dav) {
  if (dav.tag != EVT_OBJ) yis_trap("cogito.drawing_area_get_y expects drawing_area");
  CogitoNode* n = (CogitoNode*)dav.as.p;
  return YV_INT(n->kd->drawing_area.pointer_y);
}

static YisVal cogito_drawing_area_get_pressed(YisVal dav) {
  if (dav.tag != EVT_OBJ) yis_trap("cogito.drawing_area_get_pressed expects drawing_area");
  CogitoNode* n = (CogitoNode*)dav.as.p;
  return YV_BOOL(n->kd->drawing_area.pressed);
}

static void cogito_drawing_area_clear(YisVal dav) {
  if (dav.tag != EVT_OBJ) yis_trap("cogito.drawing_area_clear expects drawing_area");
  CogitoNode* n = (CogitoNode*)dav.as.p;
  cogito_drawing_area_clear_points(n);
}

__attribute__((unused))
static void cogito_drawing_area_on_draw(YisVal dav, YisVal handler) {
  if (dav.tag != EVT_OBJ) yis_trap("cogito.drawing_area_on_draw expects drawing_area");
  CogitoNode* n = (CogitoNode*)dav.as.p;
  if (n->on_draw) {
    yis_release_val(YV_FN(n->on_draw));
    n->on_draw = NULL;
  }
  if (handler.tag == EVT_FN) {
    n->on_draw = (YisFn*)handler.as.p;
    yis_retain_val(YV_FN(n->on_draw));
  }
}

static CogitoNode* cogito_canvas_active_node(YisVal cv) {
  if (cv.tag != EVT_OBJ) return NULL;
  CogitoNode* n = (CogitoNode*)cv.as.p;
  if (!n || n->kind != COGITO_DRAWING_AREA) return NULL;
  if (cogito_canvas_active != n) return NULL;
  return n;
}

static void cogito_canvas_scissor_begin(const CogitoNode* n, bool* used) {
  bool on = false;
  if (n && cogito_backend && cogito_backend->begin_scissor && cogito_backend->end_scissor) {
    cogito_backend->begin_scissor(n->x, n->y, n->w, n->h);
    on = true;
  }
  if (used) *used = on;
}

static void cogito_canvas_scissor_end(bool used) {
  if (used && cogito_backend && cogito_backend->end_scissor) {
    cogito_backend->end_scissor();
  }
}

static int cogito_canvas_int(YisVal v) {
  return (int)yis_as_int(v);
}

static bool cogito_canvas_parse_color(YisVal cv, CogitoColor* out) {
  if (!out) return false;
  CogitoColor primary = cogito_theme.iconbtn_filled.bg.a
                            ? cogito_theme.iconbtn_filled.bg
                            : cogito_theme.base.text;
  CogitoColor secondary = cogito_theme.iconbtn_tonal.bg.a
                              ? cogito_theme.iconbtn_tonal.bg
                              : cogito_theme.base.text;
  CogitoColor tertiary = cogito_theme.iconbtn_iconic.text.a
                             ? cogito_theme.iconbtn_iconic.text
                             : cogito_theme.base.text;
  if (cv.tag == EVT_STR) {
    const char* s = ((YisStr*)cv.as.p)->data;
    if (!s) return false;
    if (strcmp(s, "Primary") == 0 || strcmp(s, "primary") == 0) {
      *out = primary;
      return true;
    }
    if (strcmp(s, "Secondary") == 0 || strcmp(s, "secondary") == 0) {
      *out = secondary;
      return true;
    }
    if (strcmp(s, "Tertiary") == 0 || strcmp(s, "tertiary") == 0) {
      *out = tertiary;
      return true;
    }
    const char* p = s;
    CogitoColor c;
    if (cogito_css_parse_color(&p, &c)) {
      *out = c;
      return true;
    }
    return false;
  }
  if (cv.tag == EVT_INT || cv.tag == EVT_FLOAT) {
    int v = (int)yis_as_int(cv);
    if (v == 1) { *out = primary; return true; }
    if (v == 2) { *out = secondary; return true; }
    if (v == 3) { *out = tertiary; return true; }
  }
  return false;
}

static void cogito_canvas_set_color(YisVal ctxv, YisVal colorv) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  CogitoColor c;
  if (cogito_canvas_parse_color(colorv, &c)) {
    n->kd->drawing_area.canvas_color = c;
  }
}

static void cogito_canvas_set_line_width(YisVal ctxv, YisVal widthv) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  int w = cogito_canvas_int(widthv);
  if (w < 1) w = 1;
  if (w > 64) w = 64;
  n->kd->drawing_area.canvas_line_width = w;
}

static void cogito_canvas_draw_line_with_width(const CogitoNode* n, int x1, int y1, int x2, int y2, CogitoColor color, int width) {
  if (!n || width <= 1) {
    cogito_draw_line(x1, y1, x2, y2, color);
    return;
  }
  int half = width / 2;
  int dx = abs(x2 - x1);
  int dy = abs(y2 - y1);
  if (dx >= dy) {
    for (int o = -half; o <= half; o++) {
      cogito_draw_line(x1, y1 + o, x2, y2 + o, color);
    }
  } else {
    for (int o = -half; o <= half; o++) {
      cogito_draw_line(x1 + o, y1, x2 + o, y2, color);
    }
  }
}

static void cogito_canvas_line(YisVal ctxv, YisVal x1v, YisVal y1v, YisVal x2v, YisVal y2v) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  int x1 = n->x + cogito_canvas_int(x1v);
  int y1 = n->y + cogito_canvas_int(y1v);
  int x2 = n->x + cogito_canvas_int(x2v);
  int y2 = n->y + cogito_canvas_int(y2v);
  bool clipped = false;
  cogito_canvas_scissor_begin(n, &clipped);
  cogito_canvas_draw_line_with_width(n, x1, y1, x2, y2, n->kd->drawing_area.canvas_color,
                                     n->kd->drawing_area.canvas_line_width);
  cogito_canvas_scissor_end(clipped);
}

static void cogito_canvas_rect(YisVal ctxv, YisVal xv, YisVal yv, YisVal wv, YisVal hv) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  int x = n->x + cogito_canvas_int(xv);
  int y = n->y + cogito_canvas_int(yv);
  int w = cogito_canvas_int(wv);
  int h = cogito_canvas_int(hv);
  if (w <= 0 || h <= 0) return;
  bool clipped = false;
  cogito_canvas_scissor_begin(n, &clipped);
  cogito_draw_rect_lines(x, y, w, h, n->kd->drawing_area.canvas_color, 0,
                         n->kd->drawing_area.canvas_line_width);
  cogito_canvas_scissor_end(clipped);
}

static void cogito_canvas_fill_rect(YisVal ctxv, YisVal xv, YisVal yv, YisVal wv, YisVal hv) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  int x = n->x + cogito_canvas_int(xv);
  int y = n->y + cogito_canvas_int(yv);
  int w = cogito_canvas_int(wv);
  int h = cogito_canvas_int(hv);
  if (w <= 0 || h <= 0) return;
  bool clipped = false;
  cogito_canvas_scissor_begin(n, &clipped);
  cogito_draw_rect(x, y, w, h, n->kd->drawing_area.canvas_color, 0);
  cogito_canvas_scissor_end(clipped);
}

static void cogito_canvas_fill_linear_gradient(YisVal ctxv, YisVal xv, YisVal yv,
                                               YisVal wv, YisVal hv,
                                               YisVal startv, YisVal endv,
                                               YisVal verticalv) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  int x = n->x + cogito_canvas_int(xv);
  int y = n->y + cogito_canvas_int(yv);
  int w = cogito_canvas_int(wv);
  int h = cogito_canvas_int(hv);
  if (w <= 0 || h <= 0) return;

  CogitoColor start = n->kd->drawing_area.canvas_color;
  CogitoColor end = n->kd->drawing_area.canvas_color;
  bool has_start = cogito_canvas_parse_color(startv, &start);
  bool has_end = cogito_canvas_parse_color(endv, &end);

  if (!has_start && !has_end) {
    bool clipped = false;
    cogito_canvas_scissor_begin(n, &clipped);
    cogito_draw_rect(x, y, w, h, n->kd->drawing_area.canvas_color, 0);
    cogito_canvas_scissor_end(clipped);
    return;
  }
  if (!has_start) start = end;
  if (!has_end) end = start;

  bool vertical = yis_as_bool(verticalv);
  float angle_deg = vertical ? 90.0f : 0.0f;
  bool clipped = false;
  cogito_canvas_scissor_begin(n, &clipped);
  cogito_draw_rect_linear_gradient(x, y, w, h, start, end, angle_deg);
  cogito_canvas_scissor_end(clipped);
}

static void cogito_canvas_fill_linear_gradient_angle(YisVal ctxv, YisVal xv,
                                                     YisVal yv, YisVal wv,
                                                     YisVal hv, YisVal startv,
                                                     YisVal endv,
                                                     YisVal anglev) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  int x = n->x + cogito_canvas_int(xv);
  int y = n->y + cogito_canvas_int(yv);
  int w = cogito_canvas_int(wv);
  int h = cogito_canvas_int(hv);
  if (w <= 0 || h <= 0) return;

  CogitoColor start = n->kd->drawing_area.canvas_color;
  CogitoColor end = n->kd->drawing_area.canvas_color;
  bool has_start = cogito_canvas_parse_color(startv, &start);
  bool has_end = cogito_canvas_parse_color(endv, &end);

  if (!has_start && !has_end) {
    bool clipped = false;
    cogito_canvas_scissor_begin(n, &clipped);
    cogito_draw_rect(x, y, w, h, n->kd->drawing_area.canvas_color, 0);
    cogito_canvas_scissor_end(clipped);
    return;
  }
  if (!has_start) start = end;
  if (!has_end) end = start;

  float angle_deg = (float)yis_as_float(anglev);
  bool clipped = false;
  cogito_canvas_scissor_begin(n, &clipped);
  cogito_draw_rect_linear_gradient(x, y, w, h, start, end, angle_deg);
  cogito_canvas_scissor_end(clipped);
}

static void cogito_canvas_fill_radial_gradient(YisVal ctxv, YisVal xv,
                                               YisVal yv, YisVal wv,
                                               YisVal hv, YisVal innerv,
                                               YisVal outerv, YisVal centerxv,
                                               YisVal centeryv,
                                               YisVal radiusv) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  int x = n->x + cogito_canvas_int(xv);
  int y = n->y + cogito_canvas_int(yv);
  int w = cogito_canvas_int(wv);
  int h = cogito_canvas_int(hv);
  if (w <= 0 || h <= 0) return;

  CogitoColor inner = n->kd->drawing_area.canvas_color;
  CogitoColor outer = n->kd->drawing_area.canvas_color;
  bool has_inner = cogito_canvas_parse_color(innerv, &inner);
  bool has_outer = cogito_canvas_parse_color(outerv, &outer);

  if (!has_inner && !has_outer) {
    bool clipped = false;
    cogito_canvas_scissor_begin(n, &clipped);
    cogito_draw_rect(x, y, w, h, n->kd->drawing_area.canvas_color, 0);
    cogito_canvas_scissor_end(clipped);
    return;
  }
  if (!has_inner) inner = outer;
  if (!has_outer) outer = inner;

  float center_x = (float)n->x + (float)yis_as_float(centerxv);
  float center_y = (float)n->y + (float)yis_as_float(centeryv);
  float radius = (float)yis_as_float(radiusv);
  if (radius <= 0.0f) {
    float left = (float)x;
    float right = (float)(x + w);
    float top = (float)y;
    float bottom = (float)(y + h);
    float dx0 = fabsf(center_x - left);
    float dx1 = fabsf(center_x - right);
    float dy0 = fabsf(center_y - top);
    float dy1 = fabsf(center_y - bottom);
    float max_dx = dx0 > dx1 ? dx0 : dx1;
    float max_dy = dy0 > dy1 ? dy0 : dy1;
    radius = sqrtf(max_dx * max_dx + max_dy * max_dy);
  }

  bool clipped = false;
  cogito_canvas_scissor_begin(n, &clipped);
  cogito_draw_rect_radial_gradient(x, y, w, h, inner, outer,
                                   center_x, center_y, radius);
  cogito_canvas_scissor_end(clipped);
}

static void cogito_canvas_circle(YisVal ctxv, YisVal cxv, YisVal cyv, YisVal rv) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  int cx = n->x + cogito_canvas_int(cxv);
  int cy = n->y + cogito_canvas_int(cyv);
  int r = cogito_canvas_int(rv);
  if (r <= 0) return;
  bool clipped = false;
  cogito_canvas_scissor_begin(n, &clipped);
  if (cogito_backend && cogito_backend->draw_circle_lines) {
    cogito_backend->draw_circle_lines(cx, cy, (float)r,
      n->kd->drawing_area.canvas_color, n->kd->drawing_area.canvas_line_width);
  }
  cogito_canvas_scissor_end(clipped);
}

static void cogito_canvas_fill_circle(YisVal ctxv, YisVal cxv, YisVal cyv, YisVal rv) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  int cx = n->x + cogito_canvas_int(cxv);
  int cy = n->y + cogito_canvas_int(cyv);
  int r = cogito_canvas_int(rv);
  if (r <= 0) return;
  bool clipped = false;
  cogito_canvas_scissor_begin(n, &clipped);
  if (cogito_backend && cogito_backend->draw_circle) {
    cogito_backend->draw_circle(cx, cy, (float)r, n->kd->drawing_area.canvas_color);
  }
  cogito_canvas_scissor_end(clipped);
}

// ---------------------------------------------------------------------------
// Cached monochrome bitmap drawing
// Converts a Yis [string] rows array (each string = row of '0'/'1' chars) to
// a CogitoTexture on first use, caches by array pointer + dst size, then
// draws it in one draw_texture call using the current canvas color as a tint.
// ---------------------------------------------------------------------------

#define COGITO_BITMAP_CACHE_MAX 256
typedef struct {
  void*           key;    // YisArr* pointer — stable for global const arrays
  int             dst_w;
  int             dst_h;
  CogitoTexture*  tex;
} CogitoBitmapEntry;

static CogitoBitmapEntry cogito_bitmap_cache[COGITO_BITMAP_CACHE_MAX];
static int cogito_bitmap_cache_count = 0;

static CogitoTexture* cogito_bitmap_cache_lookup(void* key, int dst_w, int dst_h) {
  for (int i = 0; i < cogito_bitmap_cache_count; i++) {
    if (cogito_bitmap_cache[i].key == key &&
        cogito_bitmap_cache[i].dst_w == dst_w &&
        cogito_bitmap_cache[i].dst_h == dst_h) {
      return cogito_bitmap_cache[i].tex;
    }
  }
  return NULL;
}

static void cogito_bitmap_cache_store(void* key, int dst_w, int dst_h, CogitoTexture* tex) {
  if (cogito_bitmap_cache_count >= COGITO_BITMAP_CACHE_MAX) return;
  cogito_bitmap_cache[cogito_bitmap_cache_count++] =
      (CogitoBitmapEntry){key, dst_w, dst_h, tex};
}

__attribute__((unused))
static void cogito_canvas_draw_bitmap(YisVal ctxv, YisVal xv, YisVal yv,
                                      YisVal src_wv, YisVal src_hv,
                                      YisVal dst_wv, YisVal dst_hv,
                                      YisVal rowsv) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n || !cogito_backend || !cogito_backend->texture_create ||
      !cogito_backend->draw_texture) return;
  if (rowsv.tag != EVT_ARR) return;

  int src_w = cogito_canvas_int(src_wv);
  int src_h = cogito_canvas_int(src_hv);
  int dst_w = cogito_canvas_int(dst_wv);
  int dst_h = cogito_canvas_int(dst_hv);
  if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;

  void* cache_key = rowsv.as.p;
  CogitoTexture* tex = cogito_bitmap_cache_lookup(cache_key, dst_w, dst_h);

  if (!tex) {
    uint8_t* pixels = (uint8_t*)calloc((size_t)dst_w * (size_t)dst_h * 4, 1);
    if (!pixels) return;

    YisArr* rows = (YisArr*)rowsv.as.p;
    int rise_x = src_w, rise_y = src_h;
    int run_x = dst_w,  run_y = dst_h;

    int src_y = 0, err_y = 0;
    for (int dy = 0; dy < dst_h; dy++) {
      const char* bits = NULL;
      int bits_len = 0;
      if (src_y < (int)rows->len) {
        YisVal rv = rows->items[src_y];
        if (rv.tag == EVT_STR) {
          YisStr* s = (YisStr*)rv.as.p;
          bits = s->data;
          bits_len = (int)s->len;
        }
      }

      int src_x = 0, err_x = 0;
      for (int dx = 0; dx < dst_w; dx++) {
        if (bits && src_x < bits_len && bits[src_x] == '1') {
          int pi = (dy * dst_w + dx) * 4;
          pixels[pi]     = 255;
          pixels[pi + 1] = 255;
          pixels[pi + 2] = 255;
          pixels[pi + 3] = 255;
        }
        err_x += rise_x;
        while (err_x >= run_x) { err_x -= run_x; src_x++; }
      }

      err_y += rise_y;
      while (err_y >= run_y) { err_y -= run_y; src_y++; }
    }

    tex = cogito_backend->texture_create(dst_w, dst_h, pixels, 4);
    free(pixels);
    cogito_bitmap_cache_store(cache_key, dst_w, dst_h, tex);
  }

  if (!tex) return;

  int ax = n->x + cogito_canvas_int(xv);
  int ay = n->y + cogito_canvas_int(yv);
  CogitoColor color = n->kd->drawing_area.canvas_color;
  CogitoRect src = {0, 0, dst_w, dst_h};
  CogitoRect dst = {ax, ay, dst_w, dst_h};

  bool clipped = false;
  cogito_canvas_scissor_begin(n, &clipped);
  cogito_backend->draw_texture(tex, src, dst, color);
  cogito_canvas_scissor_end(clipped);
}
