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
  int light_d = btn;
  if (light_d < 2) light_d = 2;

  out->light_d = light_d;
  out->light_rad = light_d / 2;
  out->lights_y = by + center_off - light_d / 2;
  out->close_lx = close_x + center_off - light_d / 2;
  out->min_lx = min_x + center_off - light_d / 2;
  out->max_lx = max_x + center_off - light_d / 2;

  int hit_pad = 3;
  int hit_size = btn + hit_pad * 2;
  out->over_close = (close_x >= 0) && cogito_hit_rect(mx, my, close_x - hit_pad, by - hit_pad, hit_size, hit_size);
  out->over_min = (min_x >= 0) && cogito_hit_rect(mx, my, min_x - hit_pad, by - hit_pad, hit_size, hit_size);
  out->over_max = (max_x >= 0) && cogito_hit_rect(mx, my, max_x - hit_pad, by - hit_pad, hit_size, hit_size);

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
    out->over_cluster = cogito_hit_rect(mx, my, cluster_left - hit_pad, by - hit_pad, cluster_right - cluster_left + hit_pad * 2, hit_size);
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

static void cogito_draw_traffic_light_glyph(int lx, int ly, int d, const char* sf_name, CogitoColor color) {
  if (d <= 0 || !sf_name) return;
  int glyph_size = (d * 6) / 10;
  if (glyph_size < 2) glyph_size = 2;
  int gx = lx + (d - glyph_size) / 2;
  int gy = ly + (d - glyph_size) / 2;
  CogitoTexture* tex = NULL;
  int tw = 0, th = 0;
  if (cogito_icon_load_texture(sf_name, glyph_size, &tex, &tw, &th) && tex && cogito_backend && cogito_backend->draw_texture) {
    int dw = glyph_size, dh = glyph_size;
    int dx = gx, dy = gy;
    if (tw > 0 && th > 0) {
      if (tw > th) { dh = (glyph_size * th + tw / 2) / tw; if (dh < 1) dh = 1; dy = gy + (glyph_size - dh) / 2; }
      else if (th > tw) { dw = (glyph_size * tw + th / 2) / th; if (dw < 1) dw = 1; dx = gx + (glyph_size - dw) / 2; }
    }
    CogitoRect src = {0, 0, tw, th};
    CogitoRect dst = {dx, dy, dw, dh};
    cogito_backend->draw_texture(tex, src, dst, color);
  }
}

static void cogito_draw_traffic_lights_styled(const CogitoStyle* base_style,
                                              CogitoColor fallback_btn,
                                              CogitoColor fallback_border,
                                              int close_x, int min_x, int max_x,
                                              int by, int btn,
                                              const CogitoTrafficLightsState* st,
                                              bool monochrome) {
  if (!st) return;
  CogitoStyle empty = {0};
  const CogitoStyle* s = base_style ? base_style : &empty;
#if defined(__APPLE__)
  (void)by;
  (void)btn;

  if (monochrome) {
    CogitoColor mono_border = fallback_border;
    if (mono_border.a == 0) mono_border = cogito_rgba(0, 0, 0, 70);

    CogitoColor close_bg = fallback_btn;
    CogitoColor min_bg = fallback_btn;
    CogitoColor max_bg = fallback_btn;

    // Keep hover/press affordance while remaining grayscale.
    if (st->state_close == 1) close_bg = cogito_mix(close_bg, cogito_rgba(255, 255, 255, 255), 0.10f);
    else if (st->state_close == 2) close_bg = cogito_mix(close_bg, cogito_rgba(0, 0, 0, 255), 0.12f);
    if (st->state_min == 1) min_bg = cogito_mix(min_bg, cogito_rgba(255, 255, 255, 255), 0.10f);
    else if (st->state_min == 2) min_bg = cogito_mix(min_bg, cogito_rgba(0, 0, 0, 255), 0.12f);
    if (st->state_max == 1) max_bg = cogito_mix(max_bg, cogito_rgba(255, 255, 255, 255), 0.10f);
    else if (st->state_max == 2) max_bg = cogito_mix(max_bg, cogito_rgba(0, 0, 0, 255), 0.12f);

    if (close_x >= 0) {
      cogito_draw_rect(st->close_lx, st->lights_y, st->light_d, st->light_d, close_bg, st->light_rad);
      cogito_draw_rect_lines(st->close_lx, st->lights_y, st->light_d, st->light_d, mono_border, st->light_rad, 1);
    }
    if (min_x >= 0) {
      cogito_draw_rect(st->min_lx, st->lights_y, st->light_d, st->light_d, min_bg, st->light_rad);
      cogito_draw_rect_lines(st->min_lx, st->lights_y, st->light_d, st->light_d, mono_border, st->light_rad, 1);
    }
    if (max_x >= 0) {
      cogito_draw_rect(st->max_lx, st->lights_y, st->light_d, st->light_d, max_bg, st->light_rad);
      cogito_draw_rect_lines(st->max_lx, st->lights_y, st->light_d, st->light_d, mono_border, st->light_rad, 1);
    }

    if (!st->over_cluster) return;

    if (close_x >= 0) {
      CogitoColor g = s->has_text ? s->text : cogito_on_color(close_bg);
      cogito_draw_traffic_light_glyph(st->close_lx, st->lights_y, st->light_d, "sf:xmark", g);
    }
    if (min_x >= 0) {
      CogitoColor g = s->has_text ? s->text : cogito_on_color(min_bg);
      cogito_draw_traffic_light_glyph(st->min_lx, st->lights_y, st->light_d, "sf:minus", g);
    }
    if (max_x >= 0) {
      CogitoColor g = s->has_text ? s->text : cogito_on_color(max_bg);
      cogito_draw_traffic_light_glyph(st->max_lx, st->lights_y, st->light_d, "sf:plus", g);
    }
    return;
  }

  CogitoStyle close_s = cogito_appbar_win_btn_style(0, st->state_close);
  CogitoStyle min_s = cogito_appbar_win_btn_style(1, st->state_min);
  CogitoStyle max_s = cogito_appbar_win_btn_style(2, st->state_max);

  CogitoColor close_bg = close_s.has_bg ? close_s.bg
    : (s->has_appbar_btn_close_color ? s->appbar_btn_close_color : fallback_btn);
  CogitoColor min_bg = min_s.has_bg ? min_s.bg
    : (s->has_appbar_btn_min_color ? s->appbar_btn_min_color : fallback_btn);
  CogitoColor max_bg = max_s.has_bg ? max_s.bg
    : (s->has_appbar_btn_max_color ? s->appbar_btn_max_color : fallback_btn);
  CogitoColor close_glyph = close_s.has_text ? close_s.text : cogito_on_color(close_bg);
  CogitoColor min_glyph = min_s.has_text ? min_s.text : cogito_on_color(min_bg);
  CogitoColor max_glyph = max_s.has_text ? max_s.text : cogito_on_color(max_bg);

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

  if (close_x >= 0) {
    cogito_draw_traffic_light_glyph(st->close_lx, st->lights_y, st->light_d, "sf:xmark", close_glyph);
  }

  if (min_x >= 0) {
    cogito_draw_traffic_light_glyph(st->min_lx, st->lights_y, st->light_d, "sf:minus", min_glyph);
  }

  if (max_x >= 0) {
    cogito_draw_traffic_light_glyph(st->max_lx, st->lights_y, st->light_d, "sf:plus", max_glyph);
  }
#else
  (void)fallback_btn;

  CogitoColor glyph_fallback = s->has_text ? s->text : cogito_rgba(24, 24, 24, 220);
  CogitoColor close_glyph = glyph_fallback;
  CogitoColor min_glyph = glyph_fallback;
  CogitoColor max_glyph = glyph_fallback;

  if (!monochrome) {
    CogitoStyle close_s = cogito_appbar_win_btn_style(0, st->state_close);
    CogitoStyle min_s = cogito_appbar_win_btn_style(1, st->state_min);
    CogitoStyle max_s = cogito_appbar_win_btn_style(2, st->state_max);
    close_glyph = close_s.has_text ? close_s.text : glyph_fallback;
    min_glyph = min_s.has_text ? min_s.text : glyph_fallback;
    max_glyph = max_s.has_text ? max_s.text : glyph_fallback;
  } else if (fallback_border.a > 0) {
    close_glyph = fallback_border;
    min_glyph = fallback_border;
    max_glyph = fallback_border;
  }

  cogito_draw_window_button_stroke_icon(close_x, by, btn, 0, close_glyph);
  cogito_draw_window_button_stroke_icon(min_x, by, btn, 1, min_glyph);
  cogito_draw_window_button_stroke_icon(max_x, by, btn, 2, max_glyph);
#endif
}
