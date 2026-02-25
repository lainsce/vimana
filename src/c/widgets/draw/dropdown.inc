case COGITO_DROPDOWN: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = n->bg;
      CogitoColor border = n->border_color;
      CogitoColor text = n->text_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      if (cogito_has_selection_text) text = cogito_selection_text;
      else if (s.has_text) text = s.text;
      cogito_anim_apply(n, &s, &bg, &text, &border, NULL);
      if (n->bg_set || border_w > 0) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      int pad = 8;
      int size = cogito_node_font_size(n);
      const char* label = "";
      if (n->menu_len > 0 && n->selected >= 0 && (size_t)n->selected < n->menu_len) {
        label = n->menu_labels[n->selected] ? n->menu_labels[n->selected]->data : "";
      }
      int th = cogito_text_height_size(size);
      int ty = n->y + (n->h - th) / 2;
      int icon_size = s.has_icon_size ? s.icon_size : 12;
      if (icon_size < 8) icon_size = 8;
      int icon_box = icon_size + 4;
      int text_w = n->w - pad * 2 - icon_box;
      cogito_draw_text_ellipsis(n, n->x + pad, ty, text_w, label, text, size, cogito_node_bold(n), 0);
      int ix = n->x + n->w - pad - icon_box + (icon_box - icon_size) / 2;
      int iy = n->y + (n->h - icon_size) / 2;
      CogitoColor icon_color = s.has_icon_color ? s.icon_color : text;
      cogito_draw_icon_fallback(n, "chevron.down", ix, iy, icon_size, icon_color);
      break;
    }
