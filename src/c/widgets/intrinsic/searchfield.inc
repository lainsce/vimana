case COGITO_SEARCHFIELD: {
      // Keep intrinsic size as outer size (including style padding).
      int target_w = 180;
      int target_h = 56;
      int pad_w = n->padding_left + n->padding_right;
      int pad_h = n->padding_top + n->padding_bottom;
      w = target_w - pad_w;
      h = target_h - pad_h;
      if (w < 0) w = 0;
      if (h < 0) h = 0;
      break;
    }
    
