case COGITO_STEPPER: {
      bool hover = false, active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

      CogitoColor bg = s.has_bg ? s.bg : n->bg;
      CogitoColor text_col = s.has_text ? s.text : n->text_color;
      CogitoColor border_col = s.has_border ? s.border : n->border_color;
      int border_w = s.has_border_width ? s.border_width : 1;
      int radius = s.has_radius ? s.radius : (n->h / 2);
      int size = cogito_node_font_size(n);

      // Animated colors
      cogito_anim_apply(n, &s, &bg, &text_col, &border_col, NULL);

      // Draw outer capsule
      cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
      if (border_w > 0) {
        cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border_col, radius, border_w);
      }

      // Layout: [ - | value | + ]
      int btn_w = n->h;  // Square buttons at each end
      int center_w = n->w - btn_w * 2;

      // Draw dividers
      CogitoColor divider = border_col;
      divider.a = (unsigned char)(divider.a * 0.5f);
      cogito_draw_line(n->x + btn_w, n->y + 6, n->x + btn_w, n->y + n->h - 6, divider);
      cogito_draw_line(n->x + btn_w + center_w, n->y + 6, n->x + btn_w + center_w, n->y + n->h - 6, divider);

      // Draw minus button
      int minus_hover = hover && n->stepper.hit_minus;
      CogitoColor minus_col = text_col;
      if (n->stepper.value <= n->stepper.min) {
        minus_col.a = 80;  // Dimmed when at minimum
      }
      if (minus_hover) {
        CogitoColor hl = text_col;
        hl.a = 20;
        cogito_draw_rect(n->x + border_w, n->y + border_w, btn_w - border_w, n->h - border_w * 2, hl, radius);
      }
      int icon_len = 12;
      int mx = n->x + (btn_w - icon_len) / 2;
      int my = n->y + n->h / 2;
      cogito_draw_line(mx, my, mx + icon_len, my, minus_col);

      // Draw plus button
      int plus_hover = hover && n->stepper.hit_plus;
      CogitoColor plus_col = text_col;
      if (n->stepper.value >= n->stepper.max) {
        plus_col.a = 80;  // Dimmed when at maximum
      }
      if (plus_hover) {
        CogitoColor hl = text_col;
        hl.a = 20;
        cogito_draw_rect(n->x + btn_w + center_w, n->y + border_w, btn_w - border_w, n->h - border_w * 2, hl, radius);
      }
      int px = n->x + btn_w + center_w + (btn_w - icon_len) / 2;
      int py = n->y + n->h / 2;
      cogito_draw_line(px, py, px + icon_len, py, plus_col);
      cogito_draw_line(px + icon_len / 2, py - icon_len / 2, px + icon_len / 2, py + icon_len / 2, plus_col);

      // Draw value text centered
      char val_buf[32];
      if (n->stepper.step >= 1.0 && n->stepper.value == (int)n->stepper.value) {
        snprintf(val_buf, sizeof(val_buf), "%d", (int)n->stepper.value);
      } else {
        snprintf(val_buf, sizeof(val_buf), "%.1f", n->stepper.value);
      }
      int text_w = cogito_text_width_size_node(n, val_buf, size);
      int text_h = cogito_text_height_size(size);
      int tx = n->x + btn_w + (center_w - text_w) / 2;
      int ty = n->y + (n->h - text_h) / 2;
      cogito_draw_text_size_node(n, tx, ty, val_buf, text_col, size, false);
      break;
    }
