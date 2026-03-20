    case COGITO_FLOW: {
      // Intrinsic: report single-row width, max child height (like hstack).
      // Actual wrapping happens during layout when container width is known.
      int cw = 0;
      int ch = 0;
      int gap = n->gap;
      for (size_t i = 0; i < n->len; i++) {
        int iw = 0, ih = 0;
        cogito_intrinsic_size(n->children[i], &iw, &ih);
        cw += iw;
        if (i + 1 < n->len) cw += gap;
        if (ih > ch) ch = ih;
      }
      w = cw + n->margin_left + n->margin_right;
      h = ch + n->margin_top + n->margin_bottom;
      break;
    }
