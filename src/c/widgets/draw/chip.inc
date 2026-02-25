case COGITO_CHIP: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg         = s.has_bg     ? s.bg     : n->bg;
  CogitoColor text_color = s.has_text   ? s.text   : n->text_color;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int border_w = s.has_border_width ? s.border_width : 0;
  // Cogito: Chip — Full radius (h/2)
  int radius = n->h / 2;

  // Animate colors
  cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);

  // State layer (hover = 8%, active/pressed = 12%)
  CogitoColor state_layer = cogito_node_state_layer_seed(n);
  if (active) state_layer.a = (uint8_t)(bg.a > 0 ? 31 : 0);
  else if (hover) state_layer.a = (uint8_t)(bg.a > 0 ? 20 : 0);
  else state_layer.a = 0;

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
  // ---- Leading check icon for selected chips ----
  // Cogito: 18dp icon zone at leading edge (8dp gap + check icon)
  if (n->chip.selected) {
    // Draws a proper checkmark: start→mid→end, proportional to chip h
    int icon_zone  = n->h; // square icon slot at left edge
    int check_size = (n->h * 7) / 16; // ~7dp for 16dp tall chip (scales)
    if (check_size < 6) check_size = 6;
    int check_cx = n->x + icon_zone / 2;
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
  // ---- Label ----
  if (n->text) {
    int font_size = cogito_node_font_size(n);
    int text_h    = cogito_text_height_size(font_size);
    // Cogito: 12dp leading pad (or 8dp when leading icon present), 12dp trailing
    int lead = n->chip.selected ? (n->h + 4) : 12;
    int text_x = n->x + lead;
    int text_y = n->y + (n->h - text_h) / 2;
    cogito_draw_text_size_node(n, text_x, text_y, n->text->data, text_color, font_size, false);

    // ---- Close icon ----
    if (n->chip.has_close) {
      // Cogito: trailing 8dp gap + 18dp close button
      int close_zone = 18;
      int icon_size  = 10;
      int icon_x = n->x + n->w - close_zone / 2 - icon_size / 2;
      int icon_y = n->y + n->h / 2 - icon_size / 2;
      CogitoColor close_col = text_color;
      close_col.a = (uint8_t)(close_col.a * 0.7f);
      cogito_draw_line(icon_x, icon_y, icon_x + icon_size, icon_y + icon_size, close_col);
      cogito_draw_line(icon_x + icon_size, icon_y, icon_x, icon_y + icon_size, close_col);
    }
  }
  break;
}
