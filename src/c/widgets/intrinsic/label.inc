case COGITO_LABEL:
      if (n->text_wrap && n->max_w_set && n->max_w > 0) {
        w = n->max_w;
        h = cogito_text_wrap_height(n, n->text ? n->text->data : "", cogito_node_font_size(n), n->max_w);
      } else {
        w = text_w + 4;
        h = text_h;
      }
      break;
    
