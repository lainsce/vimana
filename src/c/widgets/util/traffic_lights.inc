typedef struct {
  bool over_close;
  bool over_min;
  bool over_max;
  bool over_cluster;
  int state_close;
  int state_min;
  int state_max;
  int close_lx;
  int min_lx;
  int max_lx;
  int lights_y;
  int light_d;
  int light_rad;
} CogitoTrafficLightsState;

static void cogito_traffic_lights_compute(int close_x, int min_x, int max_x, int by, int btn,
                                          int mx, int my, bool down,
                                          CogitoTrafficLightsState* out) {
  if (!out) return;
  memset(out, 0, sizeof(*out));
  if (btn <= 0) return;

  int center_off = btn / 2;
  float light_r = (btn / 14.0f) * 9.0f;
  int light_d = (int)(light_r * 2.0f + 0.5f);
  if (light_d < 2) light_d = 2;

  out->light_d = light_d;
  out->light_rad = light_d / 2;
  out->lights_y = by + center_off - light_d / 2;
  out->close_lx = close_x + center_off - light_d / 2;
  out->min_lx = min_x + center_off - light_d / 2;
  out->max_lx = max_x + center_off - light_d / 2;

  out->over_close = (close_x >= 0) && cogito_hit_rect(mx, my, close_x, by, btn, btn);
  out->over_min = (min_x >= 0) && cogito_hit_rect(mx, my, min_x, by, btn, btn);
  out->over_max = (max_x >= 0) && cogito_hit_rect(mx, my, max_x, by, btn, btn);

  int cluster_left = 0;
  int cluster_right = 0;
  bool cluster_init = false;
  if (close_x >= 0) {
    cluster_left = close_x;
    cluster_right = close_x + btn;
    cluster_init = true;
  }
  if (min_x >= 0) {
    if (!cluster_init || min_x < cluster_left) cluster_left = min_x;
    if (!cluster_init || min_x + btn > cluster_right) cluster_right = min_x + btn;
    cluster_init = true;
  }
  if (max_x >= 0) {
    if (!cluster_init || max_x < cluster_left) cluster_left = max_x;
    if (!cluster_init || max_x + btn > cluster_right) cluster_right = max_x + btn;
    cluster_init = true;
  }
  if (cluster_init) {
    out->over_cluster = cogito_hit_rect(mx, my, cluster_left, by, cluster_right - cluster_left, btn);
  }

  out->state_close = out->over_close ? (down ? 2 : 1) : 0;
  out->state_min = out->over_min ? (down ? 2 : 1) : 0;
  out->state_max = out->over_max ? (down ? 2 : 1) : 0;
}

#if !defined(__APPLE__)
static void cogito_draw_window_button_stroke_icon(int btn_x, int btn_y, int btn, int kind, CogitoColor color) {
  if (btn_x < 0 || btn <= 0) return;
  int inset = btn / 4;
  if (inset < 2) inset = 2;
  if (inset > (btn / 2) - 1) inset = (btn / 2) - 1;
  if (inset < 1) inset = 1;
  int x0 = btn_x + inset;
  int y0 = btn_y + inset;
  int x1 = btn_x + btn - inset - 1;
  int y1 = btn_y + btn - inset - 1;
  if (x1 < x0) x1 = x0;
  if (y1 < y0) y1 = y0;
  int cy = btn_y + (btn - 1) / 2;

  if (kind == 0) {
    cogito_draw_line(x0, y0, x1, y1, color);
    cogito_draw_line(x1, y0, x0, y1, color);
    return;
  }
  if (kind == 1) {
    cogito_draw_line(x0, cy, x1, cy, color);
    return;
  }
  cogito_draw_line(x0, y0, x1, y0, color);
  cogito_draw_line(x1, y0, x1, y1, color);
  cogito_draw_line(x1, y1, x0, y1, color);
  cogito_draw_line(x0, y1, x0, y0, color);
}
#endif

static void cogito_draw_traffic_lights_styled(const CogitoStyle* base_style,
                                              CogitoColor fallback_btn,
                                              CogitoColor fallback_border,
                                              int close_x, int min_x, int max_x,
                                              int by, int btn,
                                              const CogitoTrafficLightsState* st) {
  if (!st) return;
  CogitoStyle empty = {0};
  const CogitoStyle* s = base_style ? base_style : &empty;
#if defined(__APPLE__)
  (void)by;
  (void)btn;

  CogitoStyle close_s = cogito_appbar_win_btn_style(0, st->state_close);
  CogitoStyle min_s = cogito_appbar_win_btn_style(1, st->state_min);
  CogitoStyle max_s = cogito_appbar_win_btn_style(2, st->state_max);

  CogitoColor close_bg = close_s.has_bg ? close_s.bg
    : (s->has_appbar_btn_close_color ? s->appbar_btn_close_color : fallback_btn);
  CogitoColor min_bg = min_s.has_bg ? min_s.bg
    : (s->has_appbar_btn_min_color ? s->appbar_btn_min_color : fallback_btn);
  CogitoColor max_bg = max_s.has_bg ? max_s.bg
    : (s->has_appbar_btn_max_color ? s->appbar_btn_max_color : fallback_btn);
  CogitoColor glyph_fallback = s->has_text ? s->text : cogito_rgba(28, 28, 28, 220);
  CogitoColor close_glyph = close_s.has_text ? close_s.text : glyph_fallback;
  CogitoColor min_glyph = min_s.has_text ? min_s.text : glyph_fallback;
  CogitoColor max_glyph = max_s.has_text ? max_s.text : glyph_fallback;

  if (close_x >= 0) {
    cogito_draw_rect(st->close_lx, st->lights_y, st->light_d, st->light_d, close_bg, st->light_rad);
  }
  if (min_x >= 0) {
    cogito_draw_rect(st->min_lx, st->lights_y, st->light_d, st->light_d, min_bg, st->light_rad);
  }
  if (max_x >= 0) {
    cogito_draw_rect(st->max_lx, st->lights_y, st->light_d, st->light_d, max_bg, st->light_rad);
  }

  if (close_x >= 0 && (close_s.has_border || close_s.has_border_width)) {
    int bw = close_s.has_border_width ? close_s.border_width : 1;
    CogitoColor bc = close_s.has_border ? close_s.border : fallback_border;
    if (bw > 0) {
      cogito_draw_rect_lines(st->close_lx, st->lights_y, st->light_d, st->light_d, bc, st->light_rad, bw);
    }
  }
  if (min_x >= 0 && (min_s.has_border || min_s.has_border_width)) {
    int bw = min_s.has_border_width ? min_s.border_width : 1;
    CogitoColor bc = min_s.has_border ? min_s.border : fallback_border;
    if (bw > 0) {
      cogito_draw_rect_lines(st->min_lx, st->lights_y, st->light_d, st->light_d, bc, st->light_rad, bw);
    }
  }
  if (max_x >= 0 && (max_s.has_border || max_s.has_border_width)) {
    int bw = max_s.has_border_width ? max_s.border_width : 1;
    CogitoColor bc = max_s.has_border ? max_s.border : fallback_border;
    if (bw > 0) {
      cogito_draw_rect_lines(st->max_lx, st->lights_y, st->light_d, st->light_d, bc, st->light_rad, bw);
    }
  }

  if (!st->over_cluster) return;
  int glyph_d = (st->light_d * 5) / 10;
  if (glyph_d < 2) glyph_d = 2;
  if (glyph_d > st->light_d) glyph_d = st->light_d;
  int inset = (st->light_d - glyph_d) / 2;

  if (close_x >= 0) {
    int x0 = st->close_lx + inset;
    int y0 = st->lights_y + inset;
    int x1 = st->close_lx + st->light_d - inset - 1;
    int y1 = st->lights_y + st->light_d - inset - 1;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    cogito_draw_line(x0, y0, x1, y1, close_glyph);
    cogito_draw_line(x1, y0, x0, y1, close_glyph);
  }

  if (min_x >= 0) {
    int x0 = st->min_lx + inset;
    int x1 = st->min_lx + st->light_d - inset - 1;
    int y = st->lights_y + st->light_d / 2;
    if (x1 < x0) x1 = x0;
    cogito_draw_line(x0, y, x1, y, min_glyph);
  }

  if (max_x >= 0) {
    int x0 = st->max_lx + inset;
    int x1 = st->max_lx + st->light_d - inset - 1;
    int y = st->lights_y + st->light_d / 2;
    int x = st->max_lx + st->light_d / 2;
    int y0 = st->lights_y + inset;
    int y1 = st->lights_y + st->light_d - inset - 1;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    cogito_draw_line(x0, y, x1, y, max_glyph);
    cogito_draw_line(x, y0, x, y1, max_glyph);
  }
#else
  (void)fallback_btn;
  (void)fallback_border;

  CogitoStyle close_s = cogito_appbar_win_btn_style(0, st->state_close);
  CogitoStyle min_s = cogito_appbar_win_btn_style(1, st->state_min);
  CogitoStyle max_s = cogito_appbar_win_btn_style(2, st->state_max);

  CogitoColor glyph_fallback = s->has_text ? s->text : cogito_rgba(24, 24, 24, 220);
  CogitoColor close_glyph = close_s.has_text ? close_s.text : glyph_fallback;
  CogitoColor min_glyph = min_s.has_text ? min_s.text : glyph_fallback;
  CogitoColor max_glyph = max_s.has_text ? max_s.text : glyph_fallback;

  cogito_draw_window_button_stroke_icon(close_x, by, btn, 0, close_glyph);
  cogito_draw_window_button_stroke_icon(min_x, by, btn, 1, min_glyph);
  cogito_draw_window_button_stroke_icon(max_x, by, btn, 2, max_glyph);
#endif
}
