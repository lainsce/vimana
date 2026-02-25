case COGITO_HSTACK: {
      int cw = 0;
      int ch = 0;
      if (n->len == 0) { w = n->margin_left + n->margin_right; h = n->margin_top + n->margin_bottom; break; }
      for (size_t i = 0; i < n->len; i++) {
        int iw = 0;
        int ih = 0;
        cogito_intrinsic_size(n->children[i], &iw, &ih);
        if (n->kind == COGITO_VSTACK) {
          if (iw > cw) cw = iw;
          ch += ih;
          if (i + 1 < n->len) ch += pad;
        } else {
          cw += iw;
          if (i + 1 < n->len) cw += pad;
          if (ih > ch) ch = ih;
        }
      }
      w = cw + n->margin_left + n->margin_right;
      h = ch + n->margin_top + n->margin_bottom;
      break;
    }
    
