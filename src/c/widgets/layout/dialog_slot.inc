case COGITO_DIALOG_SLOT:
      if (n->dialog && (n->dialog_open || n->dialog->overlay_anim_active)) {
        if (n->dialog->kind == COGITO_WELCOME_SCREEN) {
          CogitoNode* win = cogito_node_window(n);
          if (win) {
            n->x = 0;
            n->y = 0;
            n->w = win->w;
            n->h = win->h;
            cogito_layout_node(n->dialog, 0, 0, win->w, win->h);
          } else {
            cogito_layout_node(n->dialog, x, y, w, h);
          }
          break;
        }
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
        bool compact = win ? cogito_window_node_is_compact(win) : false;
        int dx = base_x;
        int dy = base_y;
        if (compact && n->dialog->kind == COGITO_BOTTOM_SHEET) {
          dw = base_w;
          int desired_h = n->dialog->kd->bottom_sheet.desired_h;
          if (desired_h <= 0) {
            desired_h = (int)lround((double)base_h * COGITO_BOTTOM_SHEET_DEFAULT_RATIO);
          }
          if (desired_h < 200) desired_h = 200;
          if (desired_h > base_h) desired_h = base_h;
          dh = desired_h;
          dy = base_y + base_h - desired_h;
        } else if (compact) {
          dw = base_w;
          dh = base_h;
        } else {
          float p = cogito_overlay_anim_value(n->dialog, n->dialog_open);
          dx = base_x + (base_w - dw) / 2;
          dy = base_y + (base_h - dh) / 2 - (int)lroundf((1.0f - p) * 24.0f);
        }
        cogito_layout_node(n->dialog, dx, dy, dw, dh);
      }
      break;
    
