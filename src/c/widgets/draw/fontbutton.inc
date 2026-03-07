case COGITO_FONTBUTTON: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);

      CogitoNode button_proxy = *n;
      button_proxy.kind = COGITO_BUTTON;
      CogitoStyle s = cogito_theme_resolve_node_state(&button_proxy, hover, active);

      CogitoColor bg = s.has_bg ? s.bg : cogito_rgba(0, 0, 0, 0);
      CogitoColor border = s.has_border ? s.border : cogito_rgba(0, 0, 0, 0);
      CogitoColor text = n->text_color;
      if (s.has_text) {
        text = s.text;
      } else if (s.has_bg && bg.a > 0) {
        text = cogito_on_color(bg);
      }
      if (!n->disabled && (hover || active)) {
        text = cogito_color(0, 0, 0, 255);
      }
      int border_w = s.has_border_width ? s.border_width : 0;
      int radius = s.has_radius ? s.radius : 20;

      cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
      if (border_w > 0) {
        cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
      }

      int pad_x = 16;
      int icon_size = 18;
      int gap = 10;
      int icon_x = n->x + pad_x;
      int icon_y = n->y + (n->h - icon_size) / 2;
      cogito_draw_icon_fallback(n, "sf:gearshape", icon_x, icon_y, icon_size, text);

      const char* label = (n->text && n->text->data && n->text->data[0]) ? n->text->data : "System Default";
      int font_size = cogito_node_font_size(n);
      if (font_size < 14) font_size = 14;
      int text_x = icon_x + icon_size + gap;
      int text_w = n->w - (text_x - n->x) - pad_x;
      if (text_w < 0) text_w = 0;
      int text_h = cogito_text_height_size(font_size);
      int text_y = n->y + (n->h - text_h) / 2;
      cogito_draw_text_ellipsis(n, text_x, text_y, text_w, label, text, font_size, false, false, 0);
      break;
    }