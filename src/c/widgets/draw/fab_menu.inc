case COGITO_FAB_MENU: {
  // When the popup is open, skip the regular pass and let the overlay handle
  // drawing so the menu items float above all other content.
  // FAB Menu requires 2-7 items to be considered open
  if (!cogito_fab_menu_force_draw && n->popover_open && n->menu_len >= 2) {
    if (cogito_node_window(n)) break;
  }
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
  // FAB Menu requires 2-7 items
  bool menu_open = n->popover_open && n->menu_len >= 2 && n->menu_len <= 7;

  // Determine color variant based on fab_menu.variant
  // variant: 0=Primary, 1=Secondary, 2=Tertiary
  // Main FAB uses regular colors (not container colors)
  // Menu items use container colors
  CogitoColor bg, text_color, item_bg, item_text_color;
  switch (n->kd->fab_menu.variant) {
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
  int size = n->kd->fab_menu.size;
  if (size == 0) {
    fab_radius = 16; fab_icon = 24;
  } else if (size == 2) {
    fab_radius = 28; fab_icon = 36;
  } else {
    fab_radius = 20; fab_icon = 28;
  }

  // When menu is open, the button becomes 56x56 and positions at top-right of the original area
  // The close button (X) is 24px and positioned at top-right of the 56x56 button
  // Use spring-based progress animation (stores 0.0-1.0 progress, like time-based)
  // Closed size based on fab_menu.size (not n->w which may include menu items)
  int closed_w, closed_h;
  if (size == 0) {
    closed_w = 56; closed_h = 56;
  } else if (size == 2) {
    closed_w = 96; closed_h = 96;
  } else {
    closed_w = 80; closed_h = 80;
  }

  // Spring for progress value: 0.0 = closed, 1.0 = open
  // Use spring_x to store the progress (semantic value like time-based)
  double target_progress = menu_open ? 1.0 : 0.0;
  double dt = cogito_spring_dt;

  // On state change, snap spring to current visual state, then animate to target
  bool state_changed = (n->kd->fab_menu.geom_anim_target != (float)target_progress);
  if (state_changed) {
    // Snap to opposite of target (current visual state)
    cogito_spring_snap(&n->spring_x, menu_open ? 0.0 : 1.0);
    n->kd->fab_menu.geom_anim_target = (float)target_progress;
  }

  // Tick spring: Spatial Fast for FAB menu (k=800, z=0.6)
  cogito_spring_tick(&n->spring_x, target_progress, 800.0, 0.6, dt);

  // Use spring position as progress t (clamped 0-1)
  float t = (float)n->spring_x.x;
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  // Schedule more frames if spring is still moving
  if (cogito_spring_active(&n->spring_x, target_progress)) {
    double end_time = cogito_now() + 0.6;
    if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
  }

  // Interpolate geometry: t=0 (closed), t=1 (open)
  // Open state: 56x56 at top-right of closed position
  int open_x = n->x + closed_w - 56;
  int open_y = n->y;
  int open_w = 56, open_h = 56;

  int draw_x = (int)(n->x + t * (open_x - n->x) + 0.5f);
  int draw_y = (int)(n->y + t * (open_y - n->y) + 0.5f);
  int draw_w = (int)(closed_w + t * (open_w - closed_w) + 0.5f);
  int draw_h = (int)(closed_h + t * (open_h - closed_h) + 0.5f);

  // Derive corner radius from animated width: t=0 -> fab_radius, t=1 -> 28 (circle)
  int draw_radius = (int)(fab_radius + t * (28.0f - fab_radius) + 0.5f);

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
      // Ripple center is relative to the node's current animated position
      int cx = draw_x + n->ripple_x;
      int cy = draw_y + n->ripple_y;
      cogito_draw_circle_clipped_node(n, &s, draw_radius, cx, cy, r_radius, ripple_color);
    }
  }

  // Draw FAB icon: X (close) when menu open, original icon when closed
  // Use crossfade with alpha blending for smooth icon transition
  {
    // Icon size: interpolate between fab_icon (closed) and 24 (open)
    int icon_size_closed = fab_icon;
    int icon_size_open = 24;
    int icon_size = (int)(icon_size_closed + t * (icon_size_open - icon_size_closed) + 0.5f);

    // Position: interpolate between centered (closed) and top-right (open)
    int icon_x_closed = draw_x + (draw_w - icon_size) / 2;
    int icon_y_closed = draw_y + (draw_h - icon_size) / 2;
    int icon_x_open = draw_x + draw_w - 16 - icon_size;
    int icon_y_open = draw_y + 16;

    int icon_x = (int)(icon_x_closed + t * (icon_x_open - icon_x_closed) + 0.5f);
    int icon_y = (int)(icon_y_closed + t * (icon_y_open - icon_y_closed) + 0.5f);

    const char* icon_open = "close";
    const char* icon_closed = (n->text && n->text->data[0]) ? n->text->data : "add";

    // Crossfade: blend between icons during transition (t=0.35 to t=0.65)
    float fade_start = 0.35f;
    float fade_end = 0.65f;
    float open_alpha = 0.0f;
    if (t <= fade_start) {
      open_alpha = 0.0f;
    } else if (t >= fade_end) {
      open_alpha = 1.0f;
    } else {
      open_alpha = (t - fade_start) / (fade_end - fade_start);
    }
    float closed_alpha = 1.0f - open_alpha;

    // Draw closed icon (fading out)
    if (closed_alpha > 0.01f) {
      CogitoColor closed_color = text_color;
      closed_color.a = (uint8_t)(text_color.a * closed_alpha);
      cogito_draw_icon_fallback(n, icon_closed, icon_x, icon_y, icon_size, closed_color);
    }

    // Draw open icon (fading in)
    if (open_alpha > 0.01f) {
      CogitoColor open_color = text_color;
      open_color.a = (uint8_t)(text_color.a * open_alpha);
      cogito_draw_icon_fallback(n, icon_open, icon_x, icon_y, icon_size, open_color);
    }
  }

  // Draw menu items as M-size button pills stacked above the FAB
  // Animate items fanning out from FAB's top-right position when opening
  if (n->menu_len > 0) {
    int item_h = 56;  // M-size button height
    int item_gap = 4; // 4px vertical gap between items
    int font_size = cogito_font_size();
    int final_item_w = 0, final_item_x = 0;
    cogito_fab_menu_geom(n, &final_item_w, &final_item_x);
    int item_radius = item_h / 2; // fully rounded pill
    int icon_size = 24; // Leading icon size
    int icon_text_gap = 8; // Gap between icon and text
    int pad_left = 24;

    // Get mouse position for item hover highlight
    int mx = 0, my = 0;
    (void)cogito_pointer_query(&mx, &my, NULL, NULL);

    // Items animation: time-based progress triggered when FAB is nearly open (t > 0.6)
    float items_target_t = (t > 0.6f) ? 1.0f : 0.0f;
    const double items_dur_s = 0.400; // 400ms to match FAB spring timing
    double now = cogito_now();

    if (n->kd->fab_menu.items_anim_target != items_target_t) {
      n->kd->fab_menu.items_anim_from = n->kd->fab_menu.items_t;
      n->kd->fab_menu.items_anim_target = items_target_t;
      n->kd->fab_menu.items_anim_start = now;
    }

    float items_t = n->kd->fab_menu.items_anim_target;
    if (items_dur_s > 0.0) {
      double elapsed = (now - n->kd->fab_menu.items_anim_start) / items_dur_s;
      if (elapsed < 0.0) elapsed = 0.0;
      if (elapsed >= 1.0) {
        items_t = n->kd->fab_menu.items_anim_target;
      } else {
        double eased = 1.0 - pow(1.0 - elapsed, 3.0); // ease-out cubic
        items_t = (float)(n->kd->fab_menu.items_anim_from +
                          (n->kd->fab_menu.items_anim_target - n->kd->fab_menu.items_anim_from) * eased);
        double end_time = n->kd->fab_menu.items_anim_start + items_dur_s;
        if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
      }
    }
    if (items_t < 0.0f) items_t = 0.0f;
    if (items_t > 1.0f) items_t = 1.0f;
    n->kd->fab_menu.items_t = items_t;

    // All items start from the same right-edge position (FAB's right edge)
    int start_x = draw_x + draw_w;

    // Draw items from top (index n-1) to bottom (index 0)
    for (int i = (int)n->menu_len - 1; i >= 0; i--) {
      // Final position for this item (stacked vertically above FAB)
      int final_y = draw_y - 8 - (i + 1) * item_h - i * item_gap;

      // Per-item stagger based on vertical position
      // Opening: bottom items (i=0) animate first, top items animate last
      // Closing: reverse - top items hide first
      float item_index = (float)i; // 0 = bottom, menu_len-1 = top
      float stagger_amount = 0.15f; // 15% overlap - snappier feel
      float item_t;

      if (items_target_t > 0.5f) {
        // Opening: stagger from bottom to top
        float stagger_offset = (item_index / (float)n->menu_len) * stagger_amount;
        item_t = (items_t - stagger_offset) / (1.0f - stagger_amount);
      } else {
        // Closing: stagger from top to bottom (reverse)
        float reverse_index = (float)(n->menu_len - 1 - i);
        float stagger_offset = (reverse_index / (float)n->menu_len) * stagger_amount;
        item_t = (items_t - stagger_offset) / (1.0f - stagger_amount);
      }
      if (item_t < 0.0f) item_t = 0.0f;
      if (item_t > 1.0f) item_t = 1.0f;

      // Ease-out for smooth motion
      float item_eased = 1.0f - powf(1.0f - item_t, 3.0f);

      // Each item animates from the right (FAB's right edge) to its final x position
      int item_x = (int)(start_x + item_eased * (final_item_x - start_x));
      int item_y = final_y; // Y is always at final position (no vertical animation)

      // Width animates from 0 (hidden) to full width
      int item_w = (int)(item_eased * final_item_w);
      if (item_w < 0) item_w = 0;

      // Only draw if visible
      if (item_w > 4 && item_t > 0.01f) {
        bool item_hover = cogito_hit_rect(mx, my, item_x, item_y, item_w, item_h);

        // Background
        cogito_draw_rect(item_x, item_y, item_w, item_h, item_bg, item_radius);

        // State layer on hover
        if (item_hover) {
          CogitoColor state = cogito_node_state_layer_seed(n);
          state.a = 20;
          cogito_draw_rect(item_x, item_y, item_w, item_h, state, item_radius);
        }

        // Label and icon (draw text when item is wide enough)
        if (item_w > 40) {
          const char* text = n->menu_labels[i] ? n->menu_labels[i]->data : "";
          const char* icon = (n->menu_icons && n->menu_icons[i] && n->menu_icons[i]->data[0]) ? n->menu_icons[i]->data : NULL;
          int th = cogito_text_height_size(font_size);
          int ty = item_y + (item_h - th) / 2;

          // Calculate content positions relative to item
          int content_x = item_x + pad_left;

          // Draw leading icon if present
          if (icon) {
            int icon_x = content_x;
            int icon_y = item_y + (item_h - icon_size) / 2;
            cogito_draw_icon_fallback(n, icon, icon_x, icon_y, icon_size, item_text_color);
            content_x += icon_size + icon_text_gap;
          }

          // Draw label text (when there's enough space)
          int max_text_w = item_w - (content_x - item_x) - 24; // 24dp right padding
          if (max_text_w > 20) {
            cogito_draw_text_size_node(n, content_x, ty, text, item_text_color, font_size, false, true);
          }
        }
      }
    }
  }
  break;
}
