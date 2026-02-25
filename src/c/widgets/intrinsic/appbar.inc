case COGITO_APPBAR: {
      int appbar_h = 0;
      cogito_appbar_row_positions(n, NULL, NULL, NULL, NULL, &appbar_h);
      h = appbar_h > 0 ? appbar_h : 48;
      w = n->w > 0 ? n->w : 360;  // Default to 360 if not yet laid out
      break;
    }
    
