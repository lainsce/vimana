case COGITO_REVEALER: {
      if (!n->revealer_show_children || n->len == 0) break;
      int max_w = 0;
      int max_h = 0;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        int iw = 0;
        int ih = 0;
        cogito_intrinsic_size(c, &iw, &ih);
        int tw = iw + c->margin_left + c->margin_right;
        int th = ih + c->margin_top + c->margin_bottom;
        if (tw > max_w) max_w = tw;
        if (th > max_h) max_h = th;
      }
      w = max_w;
      h = max_h;
      break;
    }
