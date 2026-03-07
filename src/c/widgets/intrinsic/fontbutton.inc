case COGITO_FONTBUTTON:
      {
        int pad_x = 16;
        int icon_w = 18;
        int gap = 10;
        int min_w = 168;
        int btn_h = 40;
        w = text_w + pad_x * 2 + icon_w + gap;
        if (w < min_w) w = min_w;
        h = btn_h;
      }
      break;