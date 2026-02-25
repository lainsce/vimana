case COGITO_TOOLBAR: {
      const int max_slots = 6;
      const int docked_gap = 32;
      const int floating_gap = 8;
      const int floating_compact_gap = 4;
      const int outer_pad = 16;
      const int floating_pad = 8;
      const int compact_side_pad = 8;

      const char* cls = (n->class_name && n->class_name->data) ? n->class_name->data : NULL;
      bool floating = cls && strstr(cls, "floating") != NULL;
      bool compact = floating && cls && strstr(cls, "compact") != NULL;
      bool vertical = n->toolbar.vertical;

      // Find FAB child if any
      CogitoNode* fab_child = NULL;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        if (c && c->kind == COGITO_FAB) {
          fab_child = c;
          break;
        }
      }

      int slots = (int)n->len;
      if (slots > max_slots) slots = max_slots;
      if (slots < 0) slots = 0;

      int gap = floating ? floating_gap : docked_gap;
      if (compact) gap = floating_compact_gap;
      int side = compact ? compact_side_pad : (floating ? floating_pad : outer_pad);

      // Vertical toolbar: width based on widest item, height based on sum of heights
      if (vertical) {
        int max_item_w = 0;
        int sum_item_h = 0;
        for (int i = 0; i < slots; i++) {
          CogitoNode* c = n->children[i];
          if (!c) continue;
          int cw = 0;
          int ch = 0;
          cogito_intrinsic_size(c, &cw, &ch);
          if (cw < 1) cw = 1;
          if (ch < 1) ch = 1;
          if (cw > max_item_w) max_item_w = cw;
          sum_item_h += ch;
        }
        if (max_item_w < 48) max_item_w = 48;
        w = side * 2 + max_item_w;
        int total_gap = (slots > 1) ? gap * (slots - 1) : 0;
        h = side * 2 + sum_item_h + total_gap;
        break;
      }

      int max_item_w = 0;
      int sum_item_w = 0;
      int non_fab_count = 0;
      for (int i = 0; i < slots; i++) {
        CogitoNode* c = n->children[i];
        if (!c) continue;
        // Skip FAB in size calculation for floating toolbar
        if (floating && c->kind == COGITO_FAB) continue;
        non_fab_count++;
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        if (cw < 1) cw = 1;
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
        if (cw > max_item_w) max_item_w = cw;
        sum_item_w += cw;
      }

      if (max_item_w < 48) max_item_w = 48;
      h = 64;

      if (compact || (floating && fab_child)) {
        // For floating with FAB, calculate width without FAB slot
        int effective_count = floating && fab_child ? non_fab_count : slots;
        if (effective_count <= 0) {
          w = side * 2;
        } else {
          int total_gap = (effective_count > 1) ? gap * (effective_count - 1) : 0;
          w = side * 2 + sum_item_w + total_gap;
        }
      } else {
        int fixed_slots = max_slots;
        int total_gap = (fixed_slots > 1) ? gap * (fixed_slots - 1) : 0;
        w = side * 2 + (max_item_w * fixed_slots) + total_gap;
      }
      break;
    }
    
