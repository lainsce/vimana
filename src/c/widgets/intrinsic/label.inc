case COGITO_LABEL:
      {
      const char* label_text = n->text ? n->text->data : "";
      int capped_len = -1;
      if (n->label_max_width_chars_set) {
        capped_len = cogito_utf8_prefix_bytes_for_codepoints(label_text, n->label_max_width_chars);
      }
      if (n->text_wrap && n->max_w_set && n->max_w > 0) {
        w = n->max_w;
        const char* wrap_text = label_text;
        char stack_buf[256];
        char* heap_buf = NULL;
        if (capped_len >= 0) {
          size_t len = (size_t)capped_len;
          if (len < sizeof(stack_buf)) {
            memcpy(stack_buf, label_text, len);
            stack_buf[len] = '\0';
            wrap_text = stack_buf;
          } else {
            heap_buf = (char*)calloc(len + 1, 1);
            if (heap_buf) {
              memcpy(heap_buf, label_text, len);
              heap_buf[len] = '\0';
              wrap_text = heap_buf;
            }
          }
        }
        h = cogito_text_wrap_height(n, wrap_text, cogito_node_font_size(n), n->max_w);
        if (heap_buf) free(heap_buf);
      } else {
        if (capped_len >= 0) {
          w = cogito_text_width_size_n_node(n, label_text, capped_len, cogito_node_font_size(n)) + 4;
        } else {
          w = text_w + 4;
        }
        h = text_h;
      }
      break;
      }
    
