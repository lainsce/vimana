case COGITO_FIXED: {
      int max_w = 0;
      int max_h = 0;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        int cx = c->fixed_pos_set ? c->fixed_x : 0;
        int cy = c->fixed_pos_set ? c->fixed_y : 0;
        int right = cx + cw + c->margin_left + c->margin_right;
        int bottom = cy + ch + c->margin_top + c->margin_bottom;
        if (right > max_w) max_w = right;
        if (bottom > max_h) max_h = bottom;
      }
      w = max_w + n->padding_left + n->padding_right;
      h = max_h + n->padding_top + n->padding_bottom;
      break;
    }
    
