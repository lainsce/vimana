case COGITO_ICONBTN:
      {
        // icon_width: 0 = narrow, 1 = default (square), 2 = wide
        // Heights per size: XS=32, S=40, M=56, L=96, XL=136
        // Default widths (square): XS=32, S=40, M=56, L=96, XL=136
        // Narrow widths: XS=28, S=32, M=48, L=64, XL=104
        // Wide widths: XS=40, S=52, M=72, L=128, XL=184
        // Directly set dimensions to ensure they're correct
        if (n->button.size == COGITO_BUTTON_SIZE_XS) {
          h = 32;
        } else if (n->button.size == COGITO_BUTTON_SIZE_S) {
          h = 40;
        } else if (n->button.size == COGITO_BUTTON_SIZE_M) {
          h = 56;
        } else if (n->button.size == COGITO_BUTTON_SIZE_L) {
          h = 96;
        } else if (n->button.size == COGITO_BUTTON_SIZE_XL) {
          h = 136;
        } else {
          h = 56; // Default to M size
        }
        
        // Ensure icon buttons have a minimum height constraint to prevent them from being shrunk
        if (!n->min_h_set) {
          n->min_h = h;
          n->min_h_set = true;
        }
        
        if (n->button.icon_width == 0) {
          // Narrow: width is narrower than height
          if (h == 32) w = 28;
          else if (h == 40) w = 32;
          else if (h == 56) w = 48;
          else if (h == 96) w = 64;
          else if (h == 136) w = 104;
          else w = h;
        } else if (n->button.icon_width == 2) {
          // Wide: width is wider than height
          if (h == 32) w = 40;
          else if (h == 40) w = 52;
          else if (h == 56) w = 72;
          else if (h == 96) w = 128;
          else if (h == 136) w = 184;
          else w = h;
        } else if (n->button.icon_width == 1) {
          // Default (1): square
          w = h;
        } else {
          w = h;
        }

        // Ensure icon buttons have a minimum width constraint to prevent them from being shrunk
        if (!n->min_w_set) {
          n->min_w = w;
          n->min_w_set = true;
        }
      }
      break;
    
