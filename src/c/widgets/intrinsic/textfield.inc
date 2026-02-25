case COGITO_TEXTFIELD: {
      // Keep intrinsic size as outer size (including style padding).
      int target_w = 160;
      int target_h = 56;
      const char* hint = (n->hint && n->hint->data && n->hint->data[0]) ? n->hint->data : NULL;
      if (hint) {
        int hint_size = cogito_label_class_font_size(COGITO_LABEL_CLASS_CAPTION, 12);
        target_h += 4 + cogito_text_height_size(hint_size);
      }
      int pad_w = n->padding_left + n->padding_right;
      int pad_h = n->padding_top + n->padding_bottom;
      w = target_w - pad_w;
      h = target_h - pad_h;
      if (w < 0) w = 0;
      if (h < 0) h = 0;
      break;
    }
    
