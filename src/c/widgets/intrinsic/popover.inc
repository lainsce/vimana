case COGITO_POPOVER:
      w = 280;
      h = 180;
      if (n->len == 0) break;
      // Popover behaves like VStack for intrinsic sizing
      {
        int cw = 0;
        int ch = 0;
        for (size_t i = 0; i < n->len; i++) {
          int iw = 0;
          int ih = 0;
          cogito_intrinsic_size(n->children[i], &iw, &ih);
          if (iw > cw) cw = iw;
          ch += ih;
          if (i + 1 < n->len) ch += pad;
        }
        w = cw;
        h = ch;
      }
      break;

