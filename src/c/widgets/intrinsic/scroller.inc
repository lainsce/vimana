case COGITO_SCROLLER:
      if (n->len > 0) {
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(n->children[0], &cw, &ch);
        w = cw;
        h = ch;
      }
      break;
    
