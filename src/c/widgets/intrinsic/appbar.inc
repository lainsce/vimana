case COGITO_APPBAR: {
      int appbar_h = 0;
      cogito_appbar_row_positions(n, NULL, NULL, NULL, NULL, &appbar_h);
      h = appbar_h > 0 ? appbar_h : 40;
      w = n->w > 0 ? n->w : 0;  // Default to 0 if not yet laid out
      break;
    }
    
