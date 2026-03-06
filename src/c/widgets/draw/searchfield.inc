case COGITO_SEARCHFIELD: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  bool focused           = (cogito_focused == n && !n->disabled && n->editable);
  bool state_hover = hover && !focused;
  bool state_active = active && !focused;
  CogitoStyle base = cogito_theme_resolve_node(n);
  CogitoStyle s = cogito_theme_resolve_node_state(n, state_hover, state_active);

  // Cogito Search Bar: Full radius (h/2), leading 16dp, 24dp icon, 8dp gap to text
  CogitoColor bg         = base.has_bg ? base.bg : n->bg;
  CogitoColor text_color = base.has_text ? base.text : n->text_color;
  int radius = base.has_radius ? base.radius : (n->h / 2);
  if (s.has_bg) bg = s.bg;
  if (s.has_text) text_color = s.text;
  if (s.has_radius) radius = s.radius;

  cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);

  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // Search icon — 24dp, leading edge, vertically centered
  bool sf_rtl = cogito_is_rtl();
  int icon_size = 24;
  int icon_x    = sf_rtl ? (n->x + n->w - 16 - icon_size) : (n->x + 16);
  int size      = cogito_node_font_size(n);
  int text_h    = cogito_text_height_size(size);
  int text_y    = n->y + (n->h - text_h) / 2;
  int icon_y    = n->y + (n->h - icon_size) / 2;
  CogitoColor icon_color = text_color;
  if (!focused && (!n->text || !n->text->data[
    0
  ])) {
    icon_color.a = 150;
  }
  cogito_draw_icon_fallback(n,
  "magnifyingglass", icon_x, icon_y, icon_size, icon_color);

  // Text area: starts after icon+gap (48dp from leading edge)
  int text_x      = sf_rtl ? (n->x + 16) : (icon_x + icon_size + 8);
  int text_avail  = sf_rtl ? (icon_x - text_x - 8) : (n->w - (text_x - n->x) - 16);
  const char* txt = n->text ? n->text->data : "";
  bool has_text   = txt[
    0
  ] != '\0';
  const char* placeholder = (n->subtitle && n->subtitle->data && n->subtitle->data[
    0
  ])
                            ? n->subtitle->data : "Search...";
  bool has_placeholder   = placeholder && placeholder[
    0
  ];

  // Placeholder (no float for search bars — it simply hides when focused+has_text)
  if (has_placeholder && !has_text && !focused) {
    CogitoColor ph = text_color;
    ph.a = 120;
    cogito_draw_text_ellipsis(n, text_x, text_y, text_avail, placeholder, ph, size,
    false, false,
    0);
  }
  // Selection highlight
  if (focused && n->text_input.sel_start != n->text_input.sel_end) {
    int a = n->text_input.sel_start;
    int b = n->text_input.sel_end;
    if (a > b) { int tmp = a; a = b; b = tmp;
    }
    int sx = cogito_text_width_size_n_node(n, txt, a, size);
    int ex = cogito_text_width_size_n_node(n, txt, b, size);
    CogitoColor sel = s.has_selection ? s.selection : n->selection_color;
    int x0 = text_x + sx, x1 = text_x + ex;
    if (x1 > x0 && x1 <= text_x + text_avail) {
      cogito_draw_rect(x0, text_y, x1 - x0, text_h, sel,
      2);
    }
  }
  if (has_text) {
    cogito_draw_text_ellipsis(n, text_x, text_y, text_avail, txt, text_color, size,
    false, false,
    0);
  }
  // Caret
  if (focused) {
    int caret = n->text_input.caret;
    if (caret < 0) caret = 0;
    int tw    = cogito_text_width_size_n_node(n, txt, caret, size);
    int car_x = text_x + tw;
    if (car_x > text_x + text_avail) car_x = text_x + text_avail;
    double now = cogito_now();
    const double pulse_period = 0.800;
    double phase = fmod(now, pulse_period) / pulse_period;
    double alpha_scale = 0.0;
    if (phase < 0.5) {
      double u = phase * 2.0;
      alpha_scale = cogito_ease_cubic_bezier(u, 0.4, 0.0, 0.6, 1.0);
    } else {
      double u = (phase - 0.5) * 2.0;
      alpha_scale = 1.0 - cogito_ease_cubic_bezier(u, 0.4, 0.0, 0.6, 1.0);
    }
    CogitoColor caret_col = text_color;
    int ca = (int)lround((double)caret_col.a * alpha_scale);
    if (ca < 0) ca = 0;
    if (ca > 255) ca = 255;
    caret_col.a = (uint8_t)ca;
    cogito_draw_line(car_x, text_y, car_x, text_y + text_h, caret_col);
    double end_time = now + 0.12;
    if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
  }
  break;
}
