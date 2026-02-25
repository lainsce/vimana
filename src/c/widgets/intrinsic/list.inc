case COGITO_LIST: {
      if (n->len == 0) {
        w = 200;
        h = 120;
        break;
      }
      int max_w = 0;
      int sum_h = 0;
      int gap = n->gap;
      for (size_t i = 0; i < n->len; i++) {
        int iw = 0;
        int ih = 0;
        cogito_intrinsic_size(n->children[i], &iw, &ih);
        if (ih < 42) ih = 42;
        int tw = iw + n->children[i]->margin_left + n->children[i]->margin_right;
        int th = ih + n->children[i]->margin_top + n->children[i]->margin_bottom;
        if (tw > max_w) max_w = tw;
        sum_h += th;
        if (i + 1 < n->len) sum_h += gap;
      }
      if (max_w < 200) max_w = 200;
      w = max_w;
      h = sum_h;
      break;
    }
