case COGITO_FAB_MENU: {
  // When the popup is open, skip the regular pass and let the overlay handle
  // drawing so the menu items float above all other content.
  if (!cogito_fab_menu_force_draw && n->popover_open && n->menu_len > 0) {
    if (cogito_node_window(n)) break;
  }
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
  bool menu_open = n->popover_open && n->menu_len > 0;

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;

  // FAB Menu is S-size (56x56) with corner radius 16
  // Closed: use 16px radius; Open: use 16px radius (rounded rectangle)
  int radius = 16;

  // Animate colors
  cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);

  // Draw box shadow (elevation)
  if (s.has_box_shadow && s.box_shadow.blur > 0) {
    cogito_draw_shadow_node_box(n, &s, n->x, n->y, n->w, n->h, radius, &s.box_shadow);
  } else if (n->shadow_set && n->shadow_level > 0) {
    cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
  }

  // Draw FAB background
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // State layer for hover/press
  if (hover || active) {
    CogitoColor state_layer = cogito_node_state_layer_seed(n);
    state_layer.a = active ? 31 : 20;
    cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, state_layer, radius);
  }

  // Draw ripple effect
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

  // Draw FAB icon: X (close) when menu open, original icon when closed
  {
    int icon_size = 24;
    int icon_x = n->x + (n->w - icon_size) / 2;
    int icon_y = n->y + (n->h - icon_size) / 2;
    const char* icon = menu_open ? "close"
                     : (n->text && n->text->data[0] ? n->text->data : "add");
    cogito_draw_icon_fallback(n, icon, icon_x, icon_y, icon_size, text_color);
  }

  // Draw menu items as M-size button pills stacked above the FAB
  if (menu_open) {
    int item_h = 56;  // M-size button height
    int item_gap = 4; // 4px vertical gap between items
    int font_size = cogito_font_size();
    int item_w = 0, item_x = 0;
    cogito_fab_menu_geom(n, &item_w, &item_x);
    int item_radius = item_h / 2; // fully rounded pill

    // Get mouse position for item hover highlight
    int mx = 0, my = 0;
    (void)cogito_pointer_query(&mx, &my, NULL, NULL);

    // Draw items from top (index n-1) to bottom (index 0) so closer items
    // are rendered on top when they overlap.
    for (int i = (int)n->menu_len - 1; i >= 0; i--) {
      // Item 0: bottom = n->y - 8 (8px gap above FAB top)
      int iy_top = n->y - 8 - (i + 1) * item_h - i * item_gap;
      bool item_hover = cogito_hit_rect(mx, my, item_x, iy_top, item_w, item_h);

      // Shadow
      cogito_draw_shadow_node_level(n, &s, item_x, iy_top, item_w, item_h, item_radius, 1);

      // Background (same color as FAB)
      cogito_draw_rect(item_x, iy_top, item_w, item_h, bg, item_radius);

      // State layer on hover
      if (item_hover) {
        CogitoColor state = cogito_node_state_layer_seed(n);
        state.a = 20;
        cogito_draw_rect(item_x, iy_top, item_w, item_h, state, item_radius);
      }

      // Label
      const char* text = n->menu_labels[i] ? n->menu_labels[i]->data : "";
      int th = cogito_text_height_size(font_size);
      int tw = cogito_text_width_size_node(n, text, font_size);
      int tx = item_x + (item_w - tw) / 2;
      int ty = iy_top + (item_h - th) / 2;
      cogito_draw_text_size_node(n, tx, ty, text, text_color, font_size, false);
    }
  }
  break;
}
