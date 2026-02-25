case COGITO_ZSTACK: {
      if (n->len == 0) break;
      int max_w = 0;
      int max_h = 0;
      for (size_t i = 0; i < n->len; i++) {
        int iw = 0;
        int ih = 0;
        cogito_intrinsic_size(n->children[i], &iw, &ih);
        int tw = iw + n->children[i]->margin_left + n->children[i]->margin_right;
        int th = ih + n->children[i]->margin_top + n->children[i]->margin_bottom;
        if (tw > max_w) max_w = tw;
        if (th > max_h) max_h = th;
      }
      w = max_w;
      h = max_h;
      break;
    }
    
