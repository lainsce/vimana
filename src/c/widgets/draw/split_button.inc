case COGITO_SPLIT_BUTTON: {
  // Resolve base style (no hover/active — we apply those per-part)
  CogitoStyle s = cogito_theme_resolve_node(n);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int border_w = s.has_border_width ? s.border_width : 0;
  if (n->split_btn.outlined && border_w == 0) border_w = 1;
  if (n->split_btn.text_style) { bg = cogito_rgba(0,0,0,0); border_w = 0; }

  cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);

  // Split button dimensions
  int menu_w = n->h;
  bool menu_active = cogito_menu.open && cogito_menu.owner == n;
  int gap = 2;
  if (menu_w > n->w - 1) menu_w = n->w - 1;
  if (menu_w < 1) menu_w = 1;
  if (gap > n->w - menu_w - 1) gap = n->w - menu_w - 1;
  if (gap < 0) gap = 0;
  int main_w = n->w - menu_w - gap;
  if (main_w < 1) main_w = 1;
  int menu_x = n->x + main_w + gap;
  n->split_btn.arrow_x = menu_x;

  // Per-part hover/active detection
  int mx = 0, my = 0;
  bool btn_down = false;
  bool have_mouse = cogito_pointer_query(&mx, &my, &btn_down, NULL);
  bool over_whole = have_mouse && cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h) && !n->disabled;
  bool over_main = over_whole && mx < menu_x;
  bool over_menu = over_whole && mx >= menu_x;
  bool main_hover = over_main && !btn_down;
  bool main_active = over_main && btn_down;
  bool menu_hover = (over_menu && !btn_down) || menu_active;
  bool menu_active_press = over_menu && btn_down;

  // State layer colors for each part
  CogitoColor main_bg = bg;
  CogitoColor menu_bg = bg;
  CogitoColor state_seed = cogito_node_state_layer_seed(n);
  if (main_hover || main_active) {
    CogitoColor layer = state_seed;
    layer.a = main_active ? 31 : 20;
    main_bg = cogito_color_blend(main_bg, layer);
  }
  if (menu_hover || menu_active_press) {
    CogitoColor layer = state_seed;
    layer.a = menu_active_press ? 31 : 20;
    menu_bg = cogito_color_blend(menu_bg, layer);
  }

  int outer_radius = n->h / 2;
  if (outer_radius < 0) outer_radius = 0;
  
  // When pressed, use size-based pressed radius
  int pressed_radius = cogito_button_pressed_radius(n->split_btn.size);
  int main_outer_radius = main_active ? pressed_radius : outer_radius;
  
  // Menu button acts like a latched button:
  // - When latched (dropdown open): stays fully rounded, only pressed radius when actually pressed
  // - When not latched: normal behavior, pressed radius when pressed
  int menu_target_outer, menu_target_inner;
  if (menu_active) {
    // Latched state: fully rounded, use pressed radius only when being pressed
    menu_target_outer = menu_active_press ? pressed_radius : outer_radius;
    menu_target_inner = menu_target_outer;
  } else if (menu_active_press) {
    // Not latched but being pressed: use pressed radius for all corners
    menu_target_outer = pressed_radius;
    menu_target_inner = pressed_radius;
  } else {
    // Normal state: circular outer corners, 4px inner corners
    menu_target_outer = outer_radius;
    menu_target_inner = 4;
  }
  
  // Animate the inner radius
  double now = cogito_now();
  if (!n->anim_layout_active) {
    n->anim_layout_active = true;
    n->anim_h = n->anim_h_from = n->anim_h_target = menu_target_inner;
    n->anim_h_start = now - 1000; // Start in past to complete immediately
  } else if (n->anim_h_target != menu_target_inner) {
    n->anim_h_from = n->anim_h;
    n->anim_h_target = menu_target_inner;
    n->anim_h_start = now;
  }
  int prev_anim_ms = n->anim_ms;
  int prev_anim_ease = n->anim_ease;
  if (n->anim_ms <= 0) n->anim_ms = 180;
  if (n->anim_ease <= 0) n->anim_ease = 4; // ease-in-out fallback
  int menu_inner_radius = cogito_anim_int(n, menu_target_inner,
                                          &n->anim_h, &n->anim_h_from, &n->anim_h_target,
                                          &n->anim_h_start, now);
  n->anim_ms = prev_anim_ms;
  n->anim_ease = prev_anim_ease;
  
  int menu_outer_radius = menu_target_outer;
  if (menu_inner_radius > menu_outer_radius) menu_inner_radius = menu_outer_radius;
  
  int main_inner_radius = 4;
  if (main_inner_radius > main_outer_radius) main_inner_radius = main_outer_radius;

  // Draw main button with circular outer corners and 4px inner corners
  cogito_draw_rect_radii_aa(n->x, n->y, main_w, n->h, main_bg,
                            main_outer_radius, main_inner_radius, main_inner_radius, main_outer_radius);
  
  // Draw ripple effect for split buttons with Primary/Secondary/Tertiary/Surface backgrounds
  if (cogito_ripple_update(n)) {
    float r_radius, r_alpha;
    cogito_ripple_get(n, &r_radius, &r_alpha);
    if (r_radius > 0 && r_alpha > 0) {
      CogitoColor ripple_color = text_color;
      ripple_color.a = (uint8_t)(255 * r_alpha);
      int cx = n->x + n->ripple_x;
      int cy = n->y + n->ripple_y;
      // Clip ripple to the overall button shape
      cogito_draw_circle_clipped_node(n, &s, outer_radius, cx, cy, r_radius, ripple_color);
    }
  }
  
  if (border_w > 0) {
    cogito_draw_rect_lines_radii_aa(n->x, n->y, main_w, n->h, border_color,
                                    main_outer_radius, main_inner_radius, main_inner_radius, main_outer_radius, border_w);
  }

  // Draw menu button with reversed corner radii; when menu is active make it fully circular.
  cogito_draw_rect_radii_aa(menu_x, n->y, menu_w, n->h, menu_bg,
                            menu_inner_radius, menu_outer_radius, menu_outer_radius, menu_inner_radius);
  if (border_w > 0) {
    cogito_draw_rect_lines_radii_aa(menu_x, n->y, menu_w, n->h, border_color,
                                    menu_inner_radius, menu_outer_radius, menu_outer_radius, menu_inner_radius, border_w);
  }

  // Draw main button text
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    int tw = cogito_text_width_size_node(n, n->text->data, ts);
    int th = cogito_text_height_size(ts);
    int tx = n->x + (main_w - tw) / 2;
    int ty = n->y + (n->h - th) / 2;
    cogito_draw_text_size_node(n, tx, ty, n->text->data, text_color, ts, true);
  }

  // Draw dropdown chevron via platform icon theme (SF symbol on macOS)
  int icon_size = s.has_icon_size ? s.icon_size : 22;
  if (icon_size < 8) icon_size = 8;
  int max_icon = n->h - 8;
  if (max_icon < 8) max_icon = 8;
  if (icon_size > max_icon) icon_size = max_icon;
  int ix = menu_x + (menu_w - icon_size) / 2;
  int iy = n->y + (n->h - icon_size) / 2;
  CogitoColor arrow_col = s.has_icon_color ? s.icon_color : text_color;
  cogito_draw_icon_fallback(n, menu_active ? "chevron.up" : "chevron.down", ix, iy, icon_size, arrow_col);
  break;
}
