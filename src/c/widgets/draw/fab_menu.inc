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

  // Determine color variant based on fab_menu.variant
  // variant: 0=Primary, 1=Secondary, 2=Tertiary
  // Main FAB uses regular colors (not container colors)
  // Menu items use container colors
  CogitoColor bg, text_color, item_bg, item_text_color;
  switch (n->fab_menu.variant) {
    case 0: // Primary
      bg = cogito_cached_primary;
      text_color = cogito_cached_on_primary;
      item_bg = cogito_cached_primary_container;
      item_text_color = cogito_cached_on_primary_container;
      break;
    case 2: // Tertiary
      bg = cogito_cached_tertiary;
      text_color = cogito_cached_on_tertiary;
      item_bg = cogito_cached_tertiary_container;
      item_text_color = cogito_cached_on_tertiary_container;
      break;
    case 1: // Secondary (default)
    default:
      bg = cogito_cached_secondary;
      text_color = cogito_cached_on_secondary;
      item_bg = cogito_cached_secondary_container;
      item_text_color = cogito_cached_on_secondary_container;
      break;
  }

  // FAB Menu size metrics: S(0), M(1), L(2)
  // Size: S=56, M=80, L=96
  // Corner radius: S=16, M=20, L=28
  // Icon size: S=24, M=28, L=36
  int fab_radius, fab_icon;
  int size = n->fab_menu.size;
  if (size == 0) {
    fab_radius = 16; fab_icon = 24;
  } else if (size == 2) {
    fab_radius = 28; fab_icon = 36;
  } else {
    fab_radius = 20; fab_icon = 28;
  }

  // When menu is open, the button becomes 56x56 and positions at top-right of the original area
  // The close button (X) is 24px and positioned at top-right of the 56x56 button
  int draw_x, draw_y, draw_w, draw_h, draw_radius;
  if (menu_open) {
    // Fixed 56x56 size at top-right of the original FAB area
    draw_w = 56;
    draw_h = 56;
    draw_x = n->x + n->w - draw_w;  // Right-align
    draw_y = n->y;                   // Top-align
    draw_radius = draw_w / 2;       // Circle when open
  } else {
    // Normal closed state uses the configured size
    draw_x = n->x;
    draw_y = n->y;
    draw_w = n->w;
    draw_h = n->h;
    draw_radius = fab_radius;
  }

  // Animate colors
  cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);

  // Draw box shadow (elevation)
  cogito_draw_shadow_node_level(n, &s, draw_x, draw_y, draw_w, draw_h, draw_radius, 3);

  // Draw FAB background
  cogito_draw_rect_node(n, &s, draw_x, draw_y, draw_w, draw_h, bg, draw_radius);

  // State layer for hover/press
  if (hover || active) {
    CogitoColor state_layer = cogito_node_state_layer_seed(n);
    state_layer.a = active ? 31 : 20;
    cogito_draw_rect_node(n, &s, draw_x, draw_y, draw_w, draw_h, state_layer, draw_radius);
  }

  // Draw ripple effect
  if (cogito_ripple_update(n)) {
    float r_radius, r_alpha;
    cogito_ripple_get(n, &r_radius, &r_alpha);
    if (r_radius > 0 && r_alpha > 0) {
      CogitoColor ripple_color = text_color;
      ripple_color.a = (uint8_t)(255 * r_alpha);
      // Adjust ripple center for the new position when menu is open
      int cx = menu_open ? draw_x + n->ripple_x - (n->x + n->w - draw_w) : n->x + n->ripple_x;
      int cy = menu_open ? draw_y + n->ripple_y : n->y + n->ripple_y;
      cogito_draw_circle_clipped_node(n, &s, draw_radius, cx, cy, r_radius, ripple_color);
    }
  }

  // Draw FAB icon: X (close) when menu open, original icon when closed
  // When menu is open, the close button is 24px and aligned to top-right of the 56x56 button
  {
    int icon_size = menu_open ? 24 : fab_icon;
    int icon_x, icon_y;
    if (menu_open) {
      // Close button: fixed size 24px, aligned to top-right corner of the 56x56 button with 16px padding
      icon_x = draw_x + draw_w - 16 - icon_size;
      icon_y = draw_y + 16;
    } else {
      // Normal state: centered
      icon_x = draw_x + (draw_w - icon_size) / 2;
      icon_y = draw_y + (draw_h - icon_size) / 2;
    }
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
    int icon_size = 24; // Leading icon size
    int icon_text_gap = 8; // Gap between icon and text

    // Get mouse position for item hover highlight
    int mx = 0, my = 0;
    (void)cogito_pointer_query(&mx, &my, NULL, NULL);

    // Draw items from top (index n-1) to bottom (index 0) so closer items
    // are rendered on top when they overlap.
    // Items stack above the 56x56 button at top-right
    for (int i = (int)n->menu_len - 1; i >= 0; i--) {
      // Item 0: bottom = draw_y - 8 (8px gap above FAB top)
      int iy_top = draw_y - 8 - (i + 1) * item_h - i * item_gap;
      bool item_hover = cogito_hit_rect(mx, my, item_x, iy_top, item_w, item_h);

      // Background (same color as FAB)
      cogito_draw_rect(item_x, iy_top, item_w, item_h, item_bg, item_radius);

      // State layer on hover
      if (item_hover) {
        CogitoColor state = cogito_node_state_layer_seed(n);
        state.a = 20;
        cogito_draw_rect(item_x, iy_top, item_w, item_h, state, item_radius);
      }

      // Label and icon
      const char* text = n->menu_labels[i] ? n->menu_labels[i]->data : "";
      const char* icon = (n->menu_icons && n->menu_icons[i] && n->menu_icons[i]->data[0]) ? n->menu_icons[i]->data : NULL;
      int th = cogito_text_height_size(font_size);
      int ty = iy_top + (item_h - th) / 2;

      // Layout: icon at 24dp from left (if present), 8dp gap, text, 24dp from right
      int pad_left_no_icon = 24;
      int pad_left_with_icon = 24;

      // Draw leading icon if present
      if (icon) {
        int icon_x = item_x + pad_left_with_icon;
        int icon_y = iy_top + (item_h - icon_size) / 2;
        cogito_draw_icon_fallback(n, icon, icon_x, icon_y, icon_size, item_text_color);
      }

      // Draw label text
      int tx = icon ? item_x + pad_left_with_icon + icon_size + icon_text_gap : item_x + pad_left_no_icon;
      cogito_draw_text_size_node(n, tx, ty, text, item_text_color, font_size, false, true);
    }
  }
  break;
}
