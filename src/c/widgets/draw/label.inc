case COGITO_LABEL: {
      if (n->text) {
        bool hover = false;
        bool active = false;
        cogito_node_hover_state(n, &hover, &active);
        int size = cogito_label_font_size(n);
        CogitoColor text = n->text_color;
        CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
        if (cogito_has_selection_text) text = cogito_selection_text;
        else if (s.has_text) text = s.text;
        int align = n->text_align;
        int content_x = n->x + n->padding_left;
        int content_y = n->y + n->padding_top;
        int content_w = n->w - n->padding_left - n->padding_right;
        int content_h = n->h - n->padding_top - n->padding_bottom;
        if (content_w < 0) content_w = 0;
        if (content_h < 0) content_h = 0;
        if (n->text_wrap) {
          cogito_draw_text_wrapped(n, content_x, content_y, content_w, n->text->data, text, size, cogito_label_bold(n), align, true);
        } else if (n->text_ellipsis) {
          int th = cogito_text_height_size(size);
          int ty = content_y + (content_h - th) / 2;
          cogito_draw_text_ellipsis(n, content_x, ty, content_w, n->text->data, text, size, cogito_label_bold(n), align);
        } else {
          int tw = cogito_text_width_size_node(n, n->text->data, size);
          int th = cogito_text_height_size(size);
          int ty = content_y + (content_h - th) / 2;
          int tx = content_x;
          if (align == 1) tx = content_x + (content_w - tw) / 2;
          else if (align == 2) tx = content_x + (content_w - tw);
          cogito_draw_text_size_node(n, tx, ty, n->text->data, text, size, cogito_label_bold(n));
        }
      }
      break;
    }
