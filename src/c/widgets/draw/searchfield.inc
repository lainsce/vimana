case COGITO_SEARCHFIELD: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  bool focused           = (cogito_focused == n && !n->disabled && n->editable);
  bool focus_from_keyboard = focused && !active;
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover || focus_from_keyboard, false);

  // Cogito Search Bar: Full radius (h/2), leading 16dp, 24dp icon, 8dp gap to text
  CogitoColor bg         = s.has_bg   ? s.bg   : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = s.has_radius ? s.radius : (n->h / 2);

  cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);

  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // Search icon — 24dp, 16dp from left edge, vertically centered
  int icon_size = 24;
  int icon_x    = n->x + 16;
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

  // Text area: starts at icon_x + icon_size + 8 gap = 16+24+8 = 48dp from left
  int text_x      = icon_x + icon_size + 8;
  int text_avail  = n->w - (text_x - n->x) - 16;
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
    false,
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
    false,
    0);
  }
  // Caret
  if (focused) {
    int caret = n->text_input.caret;
    if (caret < 0) caret = 0;
    int tw    = cogito_text_width_size_n_node(n, txt, caret, size);
    int car_x = text_x + tw;
    if (car_x > text_x + text_avail) car_x = text_x + text_avail;
    double tm = cogito_now();
    if ((int)(tm * 2.0) % 2 == 0) {
      cogito_draw_line(car_x, text_y, car_x, text_y + text_h, text_color);
    }
  }
  break;
}
