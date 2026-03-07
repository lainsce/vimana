case COGITO_LABEL: {
      if (n->text) {
        const char* full_text = n->text->data ? n->text->data : "";
        const char* draw_text = full_text;
        int capped_len = -1;
        char trunc_stack[512];
        char* trunc_heap = NULL;
        if (n->label_max_width_chars_set) {
          capped_len = cogito_utf8_prefix_bytes_for_codepoints(full_text, n->label_max_width_chars);
          size_t len = (size_t)capped_len;
          if (len < sizeof(trunc_stack)) {
            memcpy(trunc_stack, full_text, len);
            trunc_stack[len] = '\0';
            draw_text = trunc_stack;
          } else {
            trunc_heap = (char*)calloc(len + 1, 1);
            if (trunc_heap) {
              memcpy(trunc_heap, full_text, len);
              trunc_heap[len] = '\0';
              draw_text = trunc_heap;
            }
          }
        }

        bool hover = false;
        bool active = false;
        cogito_node_hover_state(n, &hover, &active);
        int size = cogito_label_font_size(n);
        CogitoColor text = n->text_color;
        CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
        if (cogito_has_selection_text) {
          text = cogito_selection_text;
          if (text.a < 8) text = s.has_text ? s.text : n->text_color;
        }
        else if (s.has_text) text = s.text;
        int align = n->text_align;
        int content_x = n->x + n->padding_left;
        int content_y = n->y + n->padding_top;
        int content_w = n->w - n->padding_left - n->padding_right;
        int content_h = n->h - n->padding_top - n->padding_bottom;
        if (content_w < 0) content_w = 0;
        if (content_h < 0) content_h = 0;
        bool lbold = cogito_label_bold(n);
        bool lmedium = cogito_label_medium(n);
        if (n->text_wrap) {
          cogito_draw_text_wrapped(n, content_x, content_y, content_w, draw_text, text, size, lbold, lmedium, align, true);
        } else if (n->text_ellipsis) {
          int th = cogito_text_height_size(size);
          int ty = content_y + (content_h - th) / 2;
          cogito_draw_text_ellipsis(n, content_x, ty, content_w, draw_text, text, size, lbold, lmedium, align);
        } else {
          int tw = cogito_text_width_size_node(n, draw_text, size);
          int th = cogito_text_height_size(size);
          int ty = content_y + (content_h - th) / 2;
          int tx = content_x;
          if (align == 1) tx = content_x + (content_w - tw) / 2;
          else if (align == 2) tx = content_x + (content_w - tw);
          cogito_draw_text_size_node(n, tx, ty, draw_text, text, size, lbold, lmedium);
        }
        if (trunc_heap) free(trunc_heap);
      }
      break;
    }
