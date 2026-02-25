case COGITO_TOOLBAR: {
      bool floating = cogito_toolbar_is_floating(n);
      bool compact = floating && cogito_toolbar_is_compact(n);
      bool vertical = n->toolbar.vertical;
      int gap = floating ? COGITO_TOOLBAR_FLOATING_GAP : COGITO_TOOLBAR_DOCKED_GAP;
      if (compact) gap = COGITO_TOOLBAR_FLOATING_COMPACT_GAP;
      int slots = COGITO_TOOLBAR_MAX_SLOTS;
      if (slots < 1) slots = 1;

      // Find FAB child if any
      CogitoNode* fab_child = NULL;
      size_t fab_index = (size_t)-1;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        if (c && c->kind == COGITO_FAB) {
          fab_child = c;
          fab_index = i;
          break;
        }
      }

      int pad_l = n->padding_left;
      int pad_r = n->padding_right;
      int pad_t = n->padding_top;
      int pad_b = n->padding_bottom;
      if (pad_l < 0) pad_l = 0;
      if (pad_r < 0) pad_r = 0;
      if (pad_t < 0) pad_t = 0;
      if (pad_b < 0) pad_b = 0;
      if (floating) {
        if (pad_l <= 0) pad_l = COGITO_TOOLBAR_FLOATING_SIDE_PADDING;
        if (pad_r <= 0) pad_r = COGITO_TOOLBAR_FLOATING_SIDE_PADDING;
        if (pad_t <= 0) pad_t = COGITO_TOOLBAR_FLOATING_SIDE_PADDING;
        if (pad_b <= 0) pad_b = COGITO_TOOLBAR_FLOATING_SIDE_PADDING;
      } else {
        if (pad_l < COGITO_TOOLBAR_OUTER_PADDING) pad_l = COGITO_TOOLBAR_OUTER_PADDING;
        if (pad_r < COGITO_TOOLBAR_OUTER_PADDING) pad_r = COGITO_TOOLBAR_OUTER_PADDING;
      }

      int content_w = n->w - pad_l - pad_r;
      if (content_w < 0) content_w = 0;
      int content_h = n->h - pad_t - pad_b;
      if (content_h < 0) content_h = 0;
      int content_x = n->x + pad_l;
      int content_y = n->y + pad_t;

      // Vertical toolbar: stack items from top to bottom
      if (vertical) {
        int item_count = 0;
        int widths[COGITO_TOOLBAR_MAX_SLOTS];
        int heights[COGITO_TOOLBAR_MAX_SLOTS];
        int total_h = 0;
        
        for (size_t i = 0; i < n->len && item_count < COGITO_TOOLBAR_MAX_SLOTS; i++) {
          CogitoNode* c = n->children[i];
          int cw = 0;
          int ch = 0;
          if (c) cogito_intrinsic_size(c, &cw, &ch);
          if (cw < 1) cw = 1;
          if (ch < 1) ch = 1;
          if (cw > content_w) cw = content_w;
          if (ch > content_h) ch = content_h;
          widths[item_count] = cw;
          heights[item_count] = ch;
          total_h += ch;
          item_count++;
        }

        if (item_count > 0) {
          int gap_used = gap;
          if (item_count > 1) {
            int gap_total = gap_used * (item_count - 1);
            int required = total_h + gap_total;
            if (required > content_h) {
              int fit_gap = (content_h - total_h) / (item_count - 1);
              if (fit_gap < 0) fit_gap = 0;
              gap_used = fit_gap;
              gap_total = gap_used * (item_count - 1);
              required = total_h + gap_total;
            }
            int start_y = content_y + (content_h - required) / 2;
            if (start_y < content_y) start_y = content_y;
            int cy = start_y;
            for (int i = 0; i < item_count; i++) {
              CogitoNode* c = n->children[i];
              if (!c) continue;
              int cw = widths[i];
              int ch = heights[i];
              int cx = content_x + (content_w - cw) / 2;
              cogito_layout_node(c, cx, cy, cw, ch);
              cy += ch;
              if (i + 1 < item_count) cy += gap_used;
            }
          } else {
            CogitoNode* c = n->children[0];
            if (c) {
              int cw = widths[0];
              int ch = heights[0];
              int cx = content_x + (content_w - cw) / 2;
              int cy = content_y + (content_h - ch) / 2;
              cogito_layout_node(c, cx, cy, cw, ch);
            }
          }
        }
        // Hide any children beyond max slots
        for (size_t i = (size_t)item_count; i < n->len; i++) {
          CogitoNode* c = n->children[i];
          if (!c) continue;
          cogito_layout_node(c, n->x, n->y, 0, 0);
        }
        break;
      }

      // For floating toolbars with FAB: FAB goes to the right with 16px gap
      if (floating && fab_child) {
        int fab_w = 0;
        int fab_h = 0;
        cogito_intrinsic_size(fab_child, &fab_w, &fab_h);
        if (fab_w < 1) fab_w = 56;
        if (fab_h < 1) fab_h = 56;

        // Layout FAB to the right of toolbar with 8px gap
        int fab_gap = 8;
        int fab_x = n->x + n->w + fab_gap;
        int fab_y = n->y + (n->h - fab_h) / 2;
        cogito_layout_node(fab_child, fab_x, fab_y, fab_w, fab_h);

        // Layout remaining children in compact mode
        int item_count = 0;
        int widths[COGITO_TOOLBAR_MAX_SLOTS];
        int heights[COGITO_TOOLBAR_MAX_SLOTS];
        int total_w = 0;
        
        for (size_t i = 0; i < n->len && item_count < COGITO_TOOLBAR_MAX_SLOTS; i++) {
          if (i == fab_index) continue;
          CogitoNode* c = n->children[i];
          int cw = 0;
          int ch = 0;
          if (c) cogito_intrinsic_size(c, &cw, &ch);
          if (cw < 1) cw = 1;
          if (ch < 1) ch = 1;
          if (c && c->kind == COGITO_BUTTON && !c->hexpand) {
            int font_px = cogito_node_font_size(c);
            int text_w = 0;
            if (c->text && c->text->data && c->text->data[0]) {
              text_w = cogito_text_width_size_node(c, c->text->data, font_px);
            }
            int pad_x = c->padding_left + c->padding_right;
            if (pad_x <= 0) pad_x = 24;
            int compact_w = text_w + pad_x;
            int min_tap = ch > 0 ? ch : 40;
            if (min_tap < 32) min_tap = 32;
            if (compact_w < min_tap) compact_w = min_tap;
            cw = compact_w;
          }
          if (cw > content_w) cw = content_w;
          if (ch > content_h) ch = content_h;
          widths[item_count] = cw;
          heights[item_count] = ch;
          total_w += cw;
          item_count++;
        }

        if (item_count > 0) {
          int gap_used = gap;
          if (item_count > 1) {
            int gap_total = gap_used * (item_count - 1);
            int required = total_w + gap_total;
            if (required > content_w) {
              int fit_gap = (content_w - total_w) / (item_count - 1);
              if (fit_gap < 0) fit_gap = 0;
              gap_used = fit_gap;
              gap_total = gap_used * (item_count - 1);
              required = total_w + gap_total;
            }
            int start_x = content_x + (content_w - required) / 2;
            if (start_x < content_x) start_x = content_x;
            int cx = start_x;
            for (int i = 0; i < item_count; i++) {
              int cw = widths[i];
              int ch = heights[i];
              int cy = content_y + (content_h - ch) / 2;
              // Find the actual child node (skipping FAB)
              CogitoNode* c = NULL;
              int slot_idx = 0;
              for (size_t j = 0; j < n->len && slot_idx <= i; j++) {
                if (j == fab_index) continue;
                if (slot_idx == i) { c = n->children[j]; break; }
                slot_idx++;
              }
              if (!c) continue;
              bool override_btn_min = (c->kind == COGITO_BUTTON && !c->hexpand);
              bool saved_min_w_set = c->min_w_set;
              int saved_min_w = c->min_w;
              if (override_btn_min) {
                c->min_w_set = true;
                c->min_w = cw;
              }
              cogito_layout_node(c, cx, cy, cw, ch);
              if (override_btn_min) {
                c->min_w_set = saved_min_w_set;
                c->min_w = saved_min_w;
              }
              cx += cw;
              if (i + 1 < item_count) cx += gap_used;
            }
          } else {
            int cw = widths[0];
            int ch = heights[0];
            int cx = content_x + (content_w - cw) / 2;
            int cy = content_y + (content_h - ch) / 2;
            // Find the actual child node (skipping FAB)
            CogitoNode* c = NULL;
            for (size_t j = 0; j < n->len; j++) {
              if (j == fab_index) continue;
              c = n->children[j];
              break;
            }
            if (c) {
              bool override_btn_min = (c->kind == COGITO_BUTTON && !c->hexpand);
              bool saved_min_w_set = c->min_w_set;
              int saved_min_w = c->min_w;
              if (override_btn_min) {
                c->min_w_set = true;
                c->min_w = cw;
              }
              cogito_layout_node(c, cx, cy, cw, ch);
              if (override_btn_min) {
                c->min_w_set = saved_min_w_set;
                c->min_w = saved_min_w;
              }
            }
          }
        }
        break;
      }

      if (compact) {
        int item_count = (int)n->len;
        if (item_count > COGITO_TOOLBAR_MAX_SLOTS) item_count = COGITO_TOOLBAR_MAX_SLOTS;
        if (item_count < 0) item_count = 0;
        if (item_count == 0) break;

        int widths[COGITO_TOOLBAR_MAX_SLOTS];
        int heights[COGITO_TOOLBAR_MAX_SLOTS];
        int total_w = 0;
        for (int i = 0; i < item_count; i++) {
          CogitoNode* c = n->children[i];
          int cw = 0;
          int ch = 0;
          if (c) cogito_intrinsic_size(c, &cw, &ch);
          if (cw < 1) cw = 1;
          if (ch < 1) ch = 1;
          if (c && c->kind == COGITO_BUTTON && !c->hexpand) {
            int font_px = cogito_node_font_size(c);
            int text_w = 0;
            if (c->text && c->text->data && c->text->data[0]) {
              text_w = cogito_text_width_size_node(c, c->text->data, font_px);
            }
            int pad_x = c->padding_left + c->padding_right;
            if (pad_x <= 0) pad_x = 24;
            int compact_w = text_w + pad_x;
            int min_tap = ch > 0 ? ch : 40;
            if (min_tap < 32) min_tap = 32;
            if (compact_w < min_tap) compact_w = min_tap;
            cw = compact_w;
          }
          if (cw > content_w) cw = content_w;
          if (ch > content_h) ch = content_h;
          widths[i] = cw;
          heights[i] = ch;
          total_w += cw;
        }
        int gap_used = gap;
        if (item_count > 1) {
          int gap_total = gap_used * (item_count - 1);
          int required = total_w + gap_total;
          if (required > content_w) {
            int fit_gap = (content_w - total_w) / (item_count - 1);
            if (fit_gap < 0) fit_gap = 0;
            gap_used = fit_gap;
            gap_total = gap_used * (item_count - 1);
            required = total_w + gap_total;
          }
          int start_x = content_x + (content_w - required) / 2;
          if (start_x < content_x) start_x = content_x;
          int cx = start_x;
          for (int i = 0; i < item_count; i++) {
            CogitoNode* c = n->children[i];
            if (!c) continue;
            int cw = widths[i];
            int ch = heights[i];
            int cy = content_y + (content_h - ch) / 2;
            bool override_btn_min = (c->kind == COGITO_BUTTON && !c->hexpand);
            bool saved_min_w_set = c->min_w_set;
            int saved_min_w = c->min_w;
            if (override_btn_min) {
              c->min_w_set = true;
              c->min_w = cw;
            }
            cogito_layout_node(c, cx, cy, cw, ch);
            if (override_btn_min) {
              c->min_w_set = saved_min_w_set;
              c->min_w = saved_min_w;
            }
            cx += cw;
            if (i + 1 < item_count) cx += gap_used;
          }
        } else {
          CogitoNode* c = n->children[0];
          if (c) {
            int cw = widths[0];
            int ch = heights[0];
            int cx = content_x + (content_w - cw) / 2;
            int cy = content_y + (content_h - ch) / 2;
            bool override_btn_min = (c->kind == COGITO_BUTTON && !c->hexpand);
            bool saved_min_w_set = c->min_w_set;
            int saved_min_w = c->min_w;
            if (override_btn_min) {
              c->min_w_set = true;
              c->min_w = cw;
            }
            cogito_layout_node(c, cx, cy, cw, ch);
            if (override_btn_min) {
              c->min_w_set = saved_min_w_set;
              c->min_w = saved_min_w;
            }
          }
        }
        for (size_t i = (size_t)item_count; i < n->len; i++) {
          CogitoNode* c = n->children[i];
          if (!c) continue;
          cogito_layout_node(c, n->x, n->y, 0, 0);
        }
        break;
      }

      // Non-floating toolbar: FAB occupies the 6th slot
      int total_gap = (slots > 1) ? gap * (slots - 1) : 0;
      if (total_gap > content_w) total_gap = content_w;
      int slot_w = (content_w - total_gap) / slots;
      if (slot_w < 1) slot_w = 1;

      int used_w = slot_w * slots + total_gap;
      int slot_x0 = content_x + (content_w - used_w) / 2;
      if (slot_x0 < content_x) slot_x0 = content_x;

      int slot_idx = 0;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        if (!c) continue;

        // Check if this is a FAB in non-floating toolbar
        bool is_fab = (c->kind == COGITO_FAB);
        
        // For non-floating with FAB, FAB goes to 6th slot (index 5)
        if (fab_child && !floating && is_fab) {
          // Place FAB in the 6th slot
          int fab_slot = 5; // 6th slot (0-indexed)
          int fab_w = 0;
          int fab_h = 0;
          cogito_intrinsic_size(c, &fab_w, &fab_h);
          if (fab_w < 1) fab_w = 56;
          if (fab_h < 1) fab_h = 56;
          
          int fab_slot_x = slot_x0 + fab_slot * (slot_w + gap);
          int fab_x = fab_slot_x + (slot_w - fab_w) / 2;
          int fab_y = content_y + (content_h - fab_h) / 2;
          cogito_layout_node(c, fab_x, fab_y, fab_w, fab_h);
          continue;
        }

        // Skip if we've filled all slots
        if (slot_idx >= COGITO_TOOLBAR_MAX_SLOTS) {
          cogito_layout_node(c, n->x, n->y, 0, 0);
          continue;
        }

        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        if (cw < 0) cw = 0;
        if (ch < 0) ch = 0;

        bool fill_slot_w = c->hexpand ||
          c->kind == COGITO_TEXTFIELD ||
          c->kind == COGITO_SEARCHFIELD ||
          c->kind == COGITO_DROPDOWN;

        if (c->kind == COGITO_BUTTON && !c->hexpand) {
          int font_px = cogito_node_font_size(c);
          int text_w = 0;
          if (c->text && c->text->data && c->text->data[0]) {
            text_w = cogito_text_width_size_node(c, c->text->data, font_px);
          }
          int pad_x = c->padding_left + c->padding_right;
          if (pad_x <= 0) pad_x = 24;
          int compact_w = text_w + pad_x;
          int min_tap = ch > 0 ? ch : 40;
          if (min_tap < 32) min_tap = 32;
          if (compact_w < min_tap) compact_w = min_tap;
          cw = compact_w;
        }

        if (fill_slot_w) cw = slot_w;
        if (cw > slot_w) cw = slot_w;
        if (ch > content_h) ch = content_h;

        int current_slot_x = slot_x0 + slot_idx * (slot_w + gap);
        int child_x = current_slot_x + (slot_w - cw) / 2;
        int child_y = content_y + (content_h - ch) / 2;
        bool override_btn_min = (c->kind == COGITO_BUTTON && !c->hexpand);
        bool saved_min_w_set = c->min_w_set;
        int saved_min_w = c->min_w;
        if (override_btn_min) {
          c->min_w_set = true;
          c->min_w = cw;
        }
        cogito_layout_node(c, child_x, child_y, cw, ch);
        if (override_btn_min) {
          c->min_w_set = saved_min_w_set;
          c->min_w = saved_min_w;
        }
        slot_idx++;
      }
      break;
    }
    
