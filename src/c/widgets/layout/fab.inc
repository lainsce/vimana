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

      // Collect FAB siblings (up to 3) and identify this FAB's index.
      int fab_count = 0;
      int fab_index = -1;
      CogitoNode* fabs[3] = {NULL, NULL, NULL};
      for (size_t i = 0; i < p->len && fab_count < 3; i++) {
        CogitoNode* sibling = p->children[i];
        if (sibling && sibling->kind == COGITO_FAB) {
          if (sibling == n) fab_index = fab_count;
          fabs[fab_count] = sibling;
          fab_count++;
        }
      }

      // Compute intrinsic sizes for all collected FABs.
      int ext_count = 0;
      for (int i = 0; i < fab_count; i++) {
        int iw = 0, ih = 0;
        cogito_intrinsic_size(fabs[i], &iw, &ih);
        if (iw > 0) fabs[i]->w = iw;
        if (ih > 0) fabs[i]->h = ih;
        if (fabs[i]->fab.extended) ext_count++;
      }

      int gap = 16;
      int margin = 16;
      bool vertical = (p->kind == COGITO_VSTACK);

      // ── Single FAB ────────────────────────────────────────────────
      if (fab_count <= 1) {
        int col = n->align % 3;
        if (col < 0 || col > 2) col = 2;  // Default: right lane.
        if (col == 0)
          n->x = win->x + n->w + n->margin_left;
        else if (col == 1)
          n->x = win->x + (win->w - n->w) / 2
                 + n->margin_left - n->margin_right;
        else
          n->x = win->x + win->w - margin - n->w - n->margin_right;
        n->y = win->y + win->h - margin - n->h - n->margin_bottom;
        break;
      }

      // ── Vertical combos (VSTACK parent): all-small only ──────────
      //    Supported: 2 small  |  3 small
      //    Stack bottom-up, right edges aligned.
      if (vertical) {
        int cy = win->y + win->h - margin;
        for (int i = fab_count - 1; i >= 0; i--) {
          cy -= fabs[i]->h + fabs[i]->margin_bottom;
          fabs[i]->x = win->x + win->w - margin
                        - fabs[i]->w - fabs[i]->margin_right;
          fabs[i]->y = cy;
          cy -= gap + fabs[i]->margin_top;
        }
      }

      // ── Horizontal combos (non-VSTACK parent) ────────────────────
      //    Supported: 3 small  |  1 extended + 1 small  |  1 small + 1 extended
      //    Lay out right-to-left, bottom edges aligned.
      else {
        int bottom_y = win->y + win->h - margin;
        int cx = win->x + win->w - margin;

        // extended + small mixed pair: use a smaller inner gap
        int inner_gap = (fab_count == 2 && ext_count == 1) ? 12 : gap;

        for (int i = fab_count - 1; i >= 0; i--) {
          cx -= fabs[i]->w + fabs[i]->margin_right;
          fabs[i]->x = cx;
          fabs[i]->y = bottom_y - fabs[i]->h - fabs[i]->margin_bottom;
          cx -= inner_gap + fabs[i]->margin_left;
        }
      }

      // Write back whichever position belongs to the current FAB.
      if (fab_index >= 0 && fab_index < fab_count) {
        n->x = fabs[fab_index]->x;
        n->y = fabs[fab_index]->y;
      }
      break;
    }
