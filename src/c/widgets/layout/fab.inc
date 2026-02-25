case COGITO_FAB: {
      CogitoNode* p = n->parent;
      int fw = 0, fh = 0;
      cogito_intrinsic_size(n, &fw, &fh);
      if (fw > 0) n->w = fw;
      if (fh > 0) n->h = fh;
      if (!p || n->w <= 0 || n->h <= 0) break;

      // Skip layout if parent is Toolbar - Toolbar handles FAB positioning
      if (p->kind == COGITO_TOOLBAR) break;

      if (p->kind == COGITO_NAV_RAIL) {
        int rail_w = p->w > 0 ? p->w : COGITO_NAV_RAIL_COLLAPSED_WIDTH;
        int top_offset = p->nav_rail.expanded
          ? COGITO_NAV_RAIL_EXPANDED_TOP_OFFSET
          : COGITO_NAV_RAIL_COLLAPSED_TOP_OFFSET;
        int section_gap = p->nav_rail.expanded
          ? COGITO_NAV_RAIL_EXPANDED_SECTION_GAP
          : COGITO_NAV_RAIL_COLLAPSED_SECTION_GAP;
        if (p->nav_rail.expanded) {
          int indicator_w = rail_w - (COGITO_NAV_RAIL_EXPANDED_SIDE_PADDING * 2);
          if (indicator_w < COGITO_NAV_RAIL_EXPANDED_INDICATOR_MIN_WIDTH) {
            indicator_w = COGITO_NAV_RAIL_EXPANDED_INDICATOR_MIN_WIDTH;
          }
          if (n->w > indicator_w) n->w = indicator_w;
          int bg_x = p->x + (rail_w - indicator_w) / 2;
          // Keep FAB on the same lane start as destination indicators.
          // FAB's internal left padding then aligns icon with destination icons.
          n->x = bg_x + n->margin_left - n->margin_right;
        } else {
          n->x = p->x + (rail_w - n->w) / 2 + n->margin_left - n->margin_right;
        }
        int y = p->y + p->padding_top + top_offset + n->margin_top;
        if (p->nav_rail.show_toggle) {
          y += COGITO_NAV_RAIL_SHARED_TOGGLE_BUTTON_SIZE + section_gap;
        }
        n->y = y;
        break;
      }
      if (p->kind == COGITO_BOTTOM_NAV) {
        n->x = p->x + p->w - 16 - n->w - n->margin_right;
        n->y = p->y + (p->h - n->h) / 2 + n->margin_top - n->margin_bottom;
        break;
      }

      // Position FAB relative to its window, floating at the bottom.
      CogitoNode* win = cogito_node_window(n);
      if (!win) break;

      // Count FAB siblings and find this FAB's index among them
      int fab_count = 0;
      int fab_index = -1;
      CogitoNode* fabs[2] = {NULL, NULL};
      for (size_t i = 0; i < p->len && fab_count < 2; i++) {
        CogitoNode* sibling = p->children[i];
        if (sibling && sibling->kind == COGITO_FAB) {
          if (sibling == n) fab_index = fab_count;
          fabs[fab_count] = sibling;
          fab_count++;
        }
      }

      // Single FAB: use original positioning logic
      if (fab_count <= 1) {
        int col = n->align % 3;
        if (col < 0 || col > 2) col = 2;  // FAB defaults to right lane.
        if (col == 0) {
          n->x = win->x + n->w + n->margin_left;
        } else if (col == 1) {
          n->x = win->x + (win->w - n->w) / 2 + n->margin_left - n->margin_right;
        } else {
          n->x = win->x + win->w - 16 - n->w - n->margin_right;
        }
        n->y = win->y + win->h - 16 - n->h - n->margin_bottom;
        break;
      }

      // Two FABs: position them side by side at bottom-right
      // Layout: [extended FAB] [gap] [normal FAB] or [normal FAB] [gap] [extended FAB]
      // Extended FAB takes more space, normal FAB is compact
      CogitoNode* left_fab = fabs[0];
      CogitoNode* right_fab = fabs[1];

      // Get intrinsic sizes for both FABs
      int left_w = 0, left_h = 0;
      int right_w = 0, right_h = 0;
      cogito_intrinsic_size(left_fab, &left_w, &left_h);
      cogito_intrinsic_size(right_fab, &right_w, &right_h);

      // Apply intrinsic sizes
      if (left_w > 0) left_fab->w = left_w;
      if (left_h > 0) left_fab->h = left_h;
      if (right_w > 0) right_fab->w = right_w;
      if (right_h > 0) right_fab->h = right_h;

      // Calculate positions
      int gap = 16;  // Gap between FABs
      int margin = 16;  // Margin from screen edges

      // Right FAB is always at the right edge
      int right_x = win->x + win->w - margin - right_fab->w - right_fab->margin_right;
      int right_y = win->y + win->h - margin - right_fab->h - right_fab->margin_bottom;

      // Left FAB is positioned with gap from right FAB
      int left_x = right_x - gap - left_fab->w;
      int left_y = win->y + win->h - margin - left_fab->h - left_fab->margin_bottom;

      // Apply positions based on which FAB this is
      if (fab_index == 0) {
        n->x = left_x;
        n->y = left_y;
      } else {
        n->x = right_x;
        n->y = right_y;
      }
      break;
    }
