case COGITO_CHIP: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
  CogitoStyle s_base = cogito_theme_resolve_node_state(n, false, false);

  CogitoColor bg         = s_base.has_bg ? s_base.bg : n->bg;
  CogitoColor text_color = s.has_text   ? s.text   : n->text_color;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int border_w = s.has_border_width ? s.border_width : 0;
  // Cogito: Chip — Full radius (h/2)
  int radius = n->h / 2;

  // Animate colors
  cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);

  // State layer: draw between base background and content.
  CogitoColor state_layer = cogito_rgba(0, 0, 0, 0);
  if (active || hover) {
    if (s.has_bg && !(s.bg.r == bg.r && s.bg.g == bg.g && s.bg.b == bg.b && s.bg.a == bg.a)) {
      state_layer = s.bg;
    } else {
      state_layer = cogito_node_state_layer_seed(n);
      state_layer.a = active ? 28 : 20;
    }
  }

  // Draw pill-shaped background
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
  if (state_layer.a > 0) {
    cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, state_layer, radius);
  }
  
  // Draw ripple effect for chips with Primary/Secondary/Tertiary/Surface backgrounds
  if (cogito_ripple_update(n)) {
    float r_radius, r_alpha;
    cogito_ripple_get(n, &r_radius, &r_alpha);
    if (r_radius > 0 && r_alpha > 0) {
      CogitoColor ripple_color = text_color;
      ripple_color.a = (uint8_t)(255 * r_alpha);
      int cx = n->x + n->ripple_x;
      int cy = n->y + n->ripple_y;
      cogito_draw_circle_clipped_node(n, &s, radius, cx, cy, r_radius, ripple_color);
    }
  }
  
  // Border (outline for unselected chips)
  if (border_w > 0) {
    cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border_color, radius, border_w);
  }
  // ---- Leading check icon for selected chips with spring animation ----
  // Use spring_h for checkmark scale animation
  double dt = cogito_spring_dt;
  double target_scale = n->chip.selected ? 1.0 : 0.0;

  if (!n->spring_h.inited) {
    cogito_spring_snap(&n->spring_h, target_scale);
  }

  // On state change, snap to starting position
  bool was_selected = (n->spring_h.target >= 0.5);
  if (was_selected != n->chip.selected) {
    cogito_spring_snap(&n->spring_h, n->chip.selected ? 0.0 : 1.0);
  }
  n->spring_h.target = target_scale;

  // Tick spring: Fast spatial for checkmark (k=800, z=0.6)
  cogito_spring_tick(&n->spring_h, target_scale, 800.0, 0.6, dt);

  float check_scale = (float)n->spring_h.x;
  if (check_scale < 0.0f) check_scale = 0.0f;
  if (check_scale > 1.5f) check_scale = 1.5f;

  if (check_scale > 0.01f) {
    // Draws a proper checkmark: start→mid→end, proportional to chip h
    int icon_zone  = n->h; // square icon slot at leading edge
    int check_size = (int)(((n->h * 7) / 16) * check_scale);
    if (check_size < 2) check_size = 2;
    // RTL: check on right side; LTR: check on left side
    int check_cx = cogito_is_rtl()
        ? (n->x + n->w - icon_zone / 2)
        : (n->x + icon_zone / 2);
    int check_cy = n->y + n->h / 2;
    int sx  = check_cx - check_size / 2;
    int sy  = check_cy;
    int mx  = check_cx - check_size / 8;
    int my  = check_cy + check_size * 3 / 8;
    int ex  = check_cx + check_size / 2;
    int ey  = check_cy - check_size * 3 / 8;
    cogito_draw_line(sx, sy, mx, my, text_color);
    cogito_draw_line(mx, my, ex, ey, text_color);
  }

  if (cogito_spring_active(&n->spring_h, target_scale)) {
    double end_time = cogito_now() + 0.6;
    if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    cogito_window_relayout(cogito_node_window(n));
  }
  // ---- Label ----
  if (n->text) {
    bool chip_rtl = cogito_is_rtl();
    int font_size = cogito_node_font_size(n);
    int text_h    = cogito_text_height_size(font_size);
    int tw        = cogito_text_width_size_node(n, n->text->data, font_size);
    // Cogito: 12dp leading pad (or h+4 when selected = check icon space), 12dp trailing
    int lead = n->chip.selected ? (n->h + 4) : 12;
    int text_x = chip_rtl ? (n->x + n->w - lead - tw) : (n->x + lead);
    int text_y = n->y + (n->h - text_h) / 2;
    cogito_draw_text_size_node(n, text_x, text_y, n->text->data, text_color, font_size, false, false);

    // ---- Close icon ----
    if (n->chip.has_close) {
      // Cogito: trailing 8dp gap + 18dp close button
      int close_zone = 18;
      int icon_size  = 10;
      // RTL: close icon on left; LTR: close icon on right
      int icon_x = chip_rtl
          ? (n->x + close_zone / 2 - icon_size / 2)
          : (n->x + n->w - close_zone / 2 - icon_size / 2);
      int icon_y = n->y + n->h / 2 - icon_size / 2;
      CogitoColor close_col = text_color;
      close_col.a = (uint8_t)(close_col.a * 0.7f);
      cogito_draw_line(icon_x, icon_y, icon_x + icon_size, icon_y + icon_size, close_col);
      cogito_draw_line(icon_x + icon_size, icon_y, icon_x, icon_y + icon_size, close_col);
    }
  }
  break;
}
