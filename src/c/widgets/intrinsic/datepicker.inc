case COGITO_DATEPICKER: {
      if (n->datepicker.popover_panel) {
        w = COGITO_DATEPICKER_POPOVER_W;
        h = COGITO_DATEPICKER_POPOVER_H;
      } else {
        int pad_w = n->padding_left + n->padding_right;
        int pad_h = n->padding_top + n->padding_bottom;
        w = COGITO_DATEPICKER_FIELD_MIN_WIDTH - pad_w;
        h = COGITO_DATEPICKER_FIELD_HEIGHT - pad_h;
        if (w < 0) w = 0;
        if (h < 0) h = 0;
      }
      break;
    }
