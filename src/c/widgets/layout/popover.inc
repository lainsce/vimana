case COGITO_POPOVER: {
      // Popover positions itself relative to anchor
      CogitoNode* win = cogito_node_window(n);
      if (win && win->popover_anchor) {
        CogitoNode* anchor = win->popover_anchor;
        int win_x = win->x + 8;  // Minimum padding from window edge
        int win_y = win->y + 8;
        int win_w = win->w - 16;   // Account for padding
        int win_h = win->h - 16;
        
        // Default: position popover below anchor
        int pop_x = anchor->x;
        int pop_y = anchor->y + anchor->h + 8;
        
        // Check if popover fits to the right of anchor
        bool fits_right = (pop_x + n->w <= win_x + win_w);
        
        // Check if popover fits to the left of anchor
        bool fits_left = (anchor->x - n->w >= win_x);
        
        // Check if popover fits below anchor
        bool fits_below = (pop_y + n->h <= win_y + win_h);
        
        // Check if popover fits above anchor
        bool fits_above = (anchor->y - n->h - 8 >= win_y);
        
        // Determine best horizontal position
        if (fits_right) {
          // Default position - right side, no change needed
        } else if (fits_left) {
          // Move to left side of anchor
          pop_x = anchor->x - n->w;
        } else {
          // Can't fit on either side, clamp to window
          pop_x = win_x + win_w - n->w;
          if (pop_x < win_x) pop_x = win_x;
        }
        
        // Determine best vertical position
        if (fits_below) {
          // Default position - below anchor, no change needed
        } else if (fits_above) {
          // Position above anchor
          pop_y = anchor->y - n->h - 8;
        } else if (pop_y + n->h > win_y + win_h) {
          // Can't fit below, clamp to window bottom
          pop_y = win_y + win_h - n->h;
          if (pop_y < win_y) pop_y = win_y;
        }

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
