static void cogito_draw_line(int x1, int y1, int x2, int y2, CogitoColor color);
static void cogito_draw_rect(int x, int y, int w, int h, CogitoColor bg,
                             int radius);
static void cogito_draw_rect_lines(int x, int y, int w, int h,
                                   CogitoColor border, int radius,
                                   int thickness);

static CogitoNode* cogito_drawing_area_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_DRAWING_AREA);
  n->drawing_area.pressed = false;
  n->drawing_area.dragging = false;
  n->drawing_area.pointer_x = 0;
  n->drawing_area.pointer_y = 0;
  n->drawing_area.press_x = 0;
  n->drawing_area.press_y = 0;
  n->drawing_area.point_count = 0;
  n->drawing_area.canvas_color = cogito_color(0, 0, 0, 255);
  n->drawing_area.canvas_line_width = 1;
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
  if (n->drawing_area.point_count >= COGITO_DRAWING_AREA_MAX_POINTS) return;
  n->drawing_area.points_x[n->drawing_area.point_count] = cogito_drawing_area_clamp_x(n, x);
  n->drawing_area.points_y[n->drawing_area.point_count] = cogito_drawing_area_clamp_y(n, y);
  n->drawing_area.point_count++;
}

static void cogito_drawing_area_begin_stroke(CogitoNode* n, int x, int y) {
  if (!n || n->kind != COGITO_DRAWING_AREA) return;
  if (n->drawing_area.point_count > 0 &&
      n->drawing_area.point_count < COGITO_DRAWING_AREA_MAX_POINTS) {
    n->drawing_area.points_x[n->drawing_area.point_count] = -1;
    n->drawing_area.points_y[n->drawing_area.point_count] = -1;
    n->drawing_area.point_count++;
  }
  cogito_drawing_area_add_point(n, x, y);
}

static void cogito_drawing_area_clear_points(CogitoNode* n) {
  if (!n || n->kind != COGITO_DRAWING_AREA) return;
  n->drawing_area.point_count = 0;
}

static ErgoVal cogito_drawing_area_new(void) {
  return EV_OBJ(cogito_drawing_area_new_obj());
}

static ErgoVal cogito_drawing_area_get_x(ErgoVal dav) {
  if (dav.tag != EVT_OBJ) ergo_trap("cogito.drawing_area_get_x expects drawing_area");
  CogitoNode* n = (CogitoNode*)dav.as.p;
  return EV_INT(n->drawing_area.pointer_x);
}

static ErgoVal cogito_drawing_area_get_y(ErgoVal dav) {
  if (dav.tag != EVT_OBJ) ergo_trap("cogito.drawing_area_get_y expects drawing_area");
  CogitoNode* n = (CogitoNode*)dav.as.p;
  return EV_INT(n->drawing_area.pointer_y);
}

static ErgoVal cogito_drawing_area_get_pressed(ErgoVal dav) {
  if (dav.tag != EVT_OBJ) ergo_trap("cogito.drawing_area_get_pressed expects drawing_area");
  CogitoNode* n = (CogitoNode*)dav.as.p;
  return EV_BOOL(n->drawing_area.pressed);
}

static void cogito_drawing_area_clear(ErgoVal dav) {
  if (dav.tag != EVT_OBJ) ergo_trap("cogito.drawing_area_clear expects drawing_area");
  CogitoNode* n = (CogitoNode*)dav.as.p;
  cogito_drawing_area_clear_points(n);
}

__attribute__((unused))
static void cogito_drawing_area_on_draw(ErgoVal dav, ErgoVal handler) {
  if (dav.tag != EVT_OBJ) ergo_trap("cogito.drawing_area_on_draw expects drawing_area");
  CogitoNode* n = (CogitoNode*)dav.as.p;
  if (n->on_draw) {
    ergo_release_val(EV_FN(n->on_draw));
    n->on_draw = NULL;
  }
  if (handler.tag == EVT_FN) {
    n->on_draw = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(n->on_draw));
  }
}

static CogitoNode* cogito_canvas_active_node(ErgoVal cv) {
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

static int cogito_canvas_int(ErgoVal v) {
  return (int)ergo_as_int(v);
}

static bool cogito_canvas_parse_color(ErgoVal cv, CogitoColor* out) {
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
    const char* s = ((ErgoStr*)cv.as.p)->data;
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
    int v = (int)ergo_as_int(cv);
    if (v == 1) { *out = primary; return true; }
    if (v == 2) { *out = secondary; return true; }
    if (v == 3) { *out = tertiary; return true; }
  }
  return false;
}

static void cogito_canvas_set_color(ErgoVal ctxv, ErgoVal colorv) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  CogitoColor c;
  if (cogito_canvas_parse_color(colorv, &c)) {
    n->drawing_area.canvas_color = c;
  }
}

static void cogito_canvas_set_line_width(ErgoVal ctxv, ErgoVal widthv) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  int w = cogito_canvas_int(widthv);
  if (w < 1) w = 1;
  if (w > 64) w = 64;
  n->drawing_area.canvas_line_width = w;
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

static void cogito_canvas_line(ErgoVal ctxv, ErgoVal x1v, ErgoVal y1v, ErgoVal x2v, ErgoVal y2v) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  int x1 = n->x + cogito_canvas_int(x1v);
  int y1 = n->y + cogito_canvas_int(y1v);
  int x2 = n->x + cogito_canvas_int(x2v);
  int y2 = n->y + cogito_canvas_int(y2v);
  bool clipped = false;
  cogito_canvas_scissor_begin(n, &clipped);
  cogito_canvas_draw_line_with_width(n, x1, y1, x2, y2, n->drawing_area.canvas_color,
                                     n->drawing_area.canvas_line_width);
  cogito_canvas_scissor_end(clipped);
}

static void cogito_canvas_rect(ErgoVal ctxv, ErgoVal xv, ErgoVal yv, ErgoVal wv, ErgoVal hv) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  int x = n->x + cogito_canvas_int(xv);
  int y = n->y + cogito_canvas_int(yv);
  int w = cogito_canvas_int(wv);
  int h = cogito_canvas_int(hv);
  if (w <= 0 || h <= 0) return;
  bool clipped = false;
  cogito_canvas_scissor_begin(n, &clipped);
  cogito_draw_rect_lines(x, y, w, h, n->drawing_area.canvas_color, 0,
                         n->drawing_area.canvas_line_width);
  cogito_canvas_scissor_end(clipped);
}

static void cogito_canvas_fill_rect(ErgoVal ctxv, ErgoVal xv, ErgoVal yv, ErgoVal wv, ErgoVal hv) {
  CogitoNode* n = cogito_canvas_active_node(ctxv);
  if (!n) return;
  int x = n->x + cogito_canvas_int(xv);
  int y = n->y + cogito_canvas_int(yv);
  int w = cogito_canvas_int(wv);
  int h = cogito_canvas_int(hv);
  if (w <= 0 || h <= 0) return;
  bool clipped = false;
  cogito_canvas_scissor_begin(n, &clipped);
  cogito_draw_rect(x, y, w, h, n->drawing_area.canvas_color, 0);
  cogito_canvas_scissor_end(clipped);
}
