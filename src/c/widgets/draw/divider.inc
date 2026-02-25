case COGITO_DIVIDER: {
      CogitoStyle s = cogito_theme_resolve_node(n);
      if (s.has_opacity) {
        n->opacity_set = true;
        n->opacity = s.opacity;
      }
      CogitoColor line = s.has_border ? s.border : (s.has_text ? s.text : n->text_color);
      int thickness = s.has_border_width ? s.border_width : 1;
      if (thickness < 1) thickness = 1;
      int inset = n->divider.inset ? 18 : 0;
      if (n->divider.vertical) {
        int max_inset = (n->h - 1) / 2;
        if (inset > max_inset) inset = max_inset;
        int y0 = n->y + inset;
        int y1 = n->y + n->h - 1 - inset;
        if (y1 < y0) break;
        int x = n->x + n->w / 2;
        int start_x = x - (thickness - 1) / 2;
        for (int i = 0; i < thickness; i++) {
          cogito_draw_line(start_x + i, y0, start_x + i, y1, line);
        }
      } else {
        int max_inset = (n->w - 1) / 2;
        if (inset > max_inset) inset = max_inset;
        int x0 = n->x + inset;
        int x1 = n->x + n->w - 1 - inset;
        if (x1 < x0) break;
        int y = n->y + n->h / 2;
        int start_y = y - (thickness - 1) / 2;
        for (int i = 0; i < thickness; i++) {
          cogito_draw_line(x0, start_y + i, x1, start_y + i, line);
        }
      }
      break;
    }
