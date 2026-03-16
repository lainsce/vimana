case COGITO_BUTTON_GROUP: {
      // ButtonGroup intrinsic size is based on children (Button/IconButton)
      // Sum of child widths + gaps (must match layout gap values)
      
      int btn_size_i = (n->buttongroup.size >= 0 && n->buttongroup.size <= 4) ? n->buttongroup.size : 2;
      int buttongroup_gaps_i[] = {12, 12, 8, 8, 8}; // XS, S, M, L, XL
      bool is_connected = (n->buttongroup.connected == 1);
      int actual_gap = is_connected ? 2 : buttongroup_gaps_i[btn_size_i];
      
      int total_w = 0;
      int max_h = 0;
      
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* child = n->children[i];
        int cw = 0, ch = 0;
        cogito_intrinsic_size(child, &cw, &ch);
        if (cw < 0) cw = 0;
        if (ch < 0) ch = 0;
        total_w += cw + child->margin_left + child->margin_right;
        if (ch + child->margin_top + child->margin_bottom > max_h) {
          max_h = ch + child->margin_top + child->margin_bottom;
        }
        if (i + 1 < n->len) total_w += actual_gap;
      }
      
      total_w += n->margin_left + n->margin_right;
      max_h += n->margin_top + n->margin_bottom;
      
      w = total_w;
      h = max_h;
      
      // Minimum height based on button size tier (padding added by outer cogito_intrinsic_size)
      int button_heights[] = {32, 40, 56, 96, 136};
      int min_h = button_heights[btn_size_i];
      if (h < min_h) h = min_h;
      break;
    }