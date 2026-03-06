case COGITO_POPOVER: {
      // Popover positions itself relative to anchor
      CogitoNode* win = cogito_node_window(n);
      if (win && win->popover_anchor) {
        CogitoNode* anchor = win->popover_anchor;
        const int edge_inset = 16;
        const int anchor_gap = 8;

        int min_x = win->x + edge_inset;
        int min_y = win->y + edge_inset;
        int max_x = win->x + win->w - edge_inset;
        int max_y = win->y + win->h - edge_inset;

        // Tiny-window fallback: keep placement valid even when insets overlap.
        if (max_x < min_x) {
          min_x = win->x;
          max_x = win->x + win->w;
        }
        if (max_y < min_y) {
          min_y = win->y;
          max_y = win->y + win->h;
        }
        
        // Default: position popover below anchor
        // RTL: prefer right-edge alignment; LTR: prefer left-edge alignment
        int pop_x = cogito_is_rtl() ? (anchor->x + anchor->w - n->w) : anchor->x;
        int pop_y = anchor->y + anchor->h + anchor_gap;

        // Horizontal placement: clamp to visible bounds
        if (pop_x + n->w > max_x) {
          pop_x = anchor->x + anchor->w - n->w;
        }
        if (pop_x < min_x) {
          pop_x = anchor->x;
        }
        if (pop_x < min_x) pop_x = min_x;
        if (pop_x + n->w > max_x) pop_x = max_x - n->w;

        // Vertical placement: prefer below anchor, then above, then clamp.
        if (pop_y + n->h > max_y) {
          int above = anchor->y - n->h - anchor_gap;
          if (above >= min_y) {
            pop_y = above;
          } else {
            pop_y = max_y - n->h;
          }
        }
        if (pop_y < min_y) pop_y = min_y;

        if (n->x != pop_x || n->y != pop_y) {
          n->x = pop_x;
          n->y = pop_y;
          cogito_layout_mark_changed();
        }
        cogito_layout_vstack(n, pop_x, pop_y);
      } else {
        if (n->x != x || n->y != y) {
          n->x = x;
          n->y = y;
          cogito_layout_mark_changed();
        }
        cogito_layout_vstack(n, x, y);
      }
      break;
    }
