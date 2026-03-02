case COGITO_WEBVIEW: {
      bool hover = false, active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

      CogitoColor bg = s.has_bg ? s.bg : n->bg;
      CogitoColor text_color = s.has_text ? s.text : n->text_color;
      CogitoColor border_color = s.has_border ? s.border : n->border_color;
      int border_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 0);
      int radius = n->radius_set ? n->border_radius : (s.has_radius ? s.radius : 0);
      if (radius < 0) radius = 0;

      cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);

      // Background
      if (bg.a > 0) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
      }
      if (border_w > 0) {
        cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border_color, radius, border_w);
      }

      // Content area
      const char* html = (n->icon && n->icon->data) ? n->icon->data : "";
      if (!html[0]) break;

      int pad_l = n->padding_left;
      int pad_r = n->padding_right;
      int pad_t = n->padding_top;
      int pad_b = n->padding_bottom;
      int cx = n->x + pad_l;
      int cy = n->y + pad_t;
      int cw = n->w - pad_l - pad_r;
      int ch = n->h - pad_t - pad_b;
      if (cw <= 0 || ch <= 0) break;

      // Clip to content area
      if (cogito_backend && cogito_backend->begin_scissor) {
        cogito_backend->begin_scissor(cx, cy, cw, ch);
      }

      cogito_webview_draw_content(n, html, cx, cy, cw, text_color);

      if (cogito_backend && cogito_backend->end_scissor) {
        cogito_backend->end_scissor();
      }
      break;
    }
