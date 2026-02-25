case COGITO_DIALOG_SLOT:
      if (n->dialog && n->dialog_open) {
        int dw = 0;
        int dh = 0;
        cogito_intrinsic_size(n->dialog, &dw, &dh);
        if (dw <= 0) dw = 320;
        if (dh <= 0) dh = 200;
        CogitoNode* win = cogito_node_window(n);
        if (win) {
          n->x = 0;
          n->y = 0;
          n->w = win->w;
          n->h = win->h;
        }
        int base_x = win ? 0 : x;
        int base_y = win ? 0 : y;
        int base_w = win ? win->w : w;
        int base_h = win ? win->h : h;
        int dx = base_x + (base_w - dw) / 2;
        int dy = base_y + (base_h - dh) / 2;
        cogito_layout_node(n->dialog, dx, dy, dw, dh);
      }
      break;
    
