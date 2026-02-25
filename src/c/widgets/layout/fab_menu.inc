case COGITO_FAB_MENU: {
      CogitoNode* p = n->parent;
      int fw = 0, fh = 0;
      cogito_intrinsic_size(n, &fw, &fh);
      if (fw > 0) n->w = fw;
      if (fh > 0) n->h = fh;
      if (!p || n->w <= 0 || n->h <= 0) break;

      // Always position FAB Menu relative to its window, floating at the
      // bottom of the screen regardless of nesting depth.
      CogitoNode* win = cogito_node_window(n);
      if (!win) break;

      int col = n->align % 3;
      if (col < 0 || col > 2) col = 2;  // FAB Menu defaults to right lane.
      if (col == 0) {
        n->x = win->x + 16 + n->margin_left;
      } else if (col == 1) {
        n->x = win->x + (win->w - n->w) / 2 + n->margin_left - n->margin_right;
      } else {
        n->x = win->x + win->w - 16 - n->w - n->margin_right;
      }
      n->y = win->y + win->h - 16 - n->h - n->margin_bottom;
      break;
    }