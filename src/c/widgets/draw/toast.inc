case COGITO_TOAST: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);

      // ---- Auto-dismiss for toasts without action ----
      bool has_action = (n->action_text && n->action_text->data[0]);
      if (!has_action && !hover) {
        // Initialize show time on first draw
        if (n->toast_show_time == 0.0) {
          n->toast_show_time = cogito_now();
        }

        // Check if 3500ms has passed
        double elapsed = cogito_now() - n->toast_show_time;
        if (elapsed >= 3.5 && !n->toast_dismissing) {
          n->toast_dismissing = true;
          cogito_spring_snap(&n->spring_opacity, 1.0);
        }
      } else {
        // Reset timer if hovered or has action
        n->toast_show_time = cogito_now();
        n->toast_dismissing = false;
      }

      // Handle dismiss animation
      float opacity_mult = 1.0f;
      if (n->toast_dismissing) {
        double dt = cogito_spring_dt;
        cogito_spring_tick(&n->spring_opacity, 0.0, 800.0, 0.75, dt);
        opacity_mult = (float)n->spring_opacity.x;
        if (opacity_mult < 0.0f) opacity_mult = 0.0f;

        if (cogito_spring_active(&n->spring_opacity, 0.0)) {
          double end_time = cogito_now() + 0.6;
          if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
          cogito_window_relayout(cogito_node_window(n));
        } else if (opacity_mult <= 0.01f) {
          // Animation complete, hide the toast
          n->hidden = true;
        }
      }

      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = n->bg;
      CogitoColor border = n->border_color;
      CogitoColor text = s.has_text ? s.text : n->text_color;
      CogitoColor sel = s.has_selection ? s.selection : n->selection_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      // Cogito Snackbar: Full radius (h/2) as floor when not set by theme
      if (radius < n->h / 2) radius = n->h / 2;

      // Apply opacity multiplier for dismiss animation
      bg.a = (uint8_t)(bg.a * opacity_mult);
      border.a = (uint8_t)(border.a * opacity_mult);
      text.a = (uint8_t)(text.a * opacity_mult);

      cogito_anim_apply(n, &s, &bg, &text, &border, &sel);
      if (n->bg_set || s.has_bg) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      int size = cogito_node_font_size(n);
      CogitoColor text_bg = bg;
      text = s.has_text ? text : cogito_on_color(text_bg);
      int text_l = cogito_luma(text);
      int bg_l = cogito_luma(text_bg);
      if (abs(text_l - bg_l) < 80) {
        text = cogito_on_color(text_bg);
      }
      const char* t = n->text ? n->text->data : "";
      int th = cogito_text_height_size(size);
      int ty = n->y + (n->h - th) / 2;
      int pad_x = 16; // Cogito: Snackbar 16dp horizontal padding
      int action_w = 0;
      if (n->action_text && n->action_text->data[0]) {
        action_w = cogito_text_width_size_node(n, n->action_text->data, size);
      }
      int text_w = n->w - pad_x * 2;
      if (action_w > 0) {
        text_w -= (action_w + 24);
      }
      if (text_w < 0) text_w = 0;
      cogito_draw_text_ellipsis(n, n->x + pad_x, ty, text_w, t, text, size, cogito_node_bold(n), cogito_node_medium(n), 0);
      if (action_w > 0) {
        CogitoColor action_color = sel;
        int action_x = n->x + pad_x + text_w + 24;
        int action_y = ty;
        cogito_draw_text_size_node(n, action_x, action_y, n->action_text->data, action_color, size, false, false);
        n->action_button_x = action_x;
        n->action_button_y = action_y;
        n->action_button_w = action_w;
        n->action_button_h = th;
      }
      break;
    }
