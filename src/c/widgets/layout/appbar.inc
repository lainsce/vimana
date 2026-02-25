case COGITO_APPBAR: {
      int btn_size = 12;
      int btn_gap = 6;
      int btn_pad_top = 9;
      int btn_pad_right = 9;
      cogito_appbar_button_metrics(&btn_size, &btn_gap, &btn_pad_top, &btn_pad_right);
      int buttons_y = y + btn_pad_top;
      n->appbar.btn_size = btn_size;
      n->appbar.btn_y = buttons_y;
      CogitoNode* win = cogito_node_window(n);
      bool allow_max = !win || win->resizable;
      // Only show traffic lights on the first (leftmost) appbar
      if (cogito_is_first_appbar(win, n)) {
        cogito_appbar_controls_positions(n, x, w, btn_size, btn_gap, btn_pad_right, allow_max,
                                         &n->appbar.btn_close_x, &n->appbar.btn_min_x, &n->appbar.btn_max_x);
      } else {
        // Other appbars don't show traffic lights
        n->appbar.btn_close_x = -1;
        n->appbar.btn_min_x = -1;
        n->appbar.btn_max_x = -1;
      }
      int title_y = 0;
      int subtitle_y = 0;
      int child_y = 0;
      int appbar_h = 0;
      cogito_appbar_row_positions(n, &buttons_y, &title_y, &subtitle_y, &child_y, &appbar_h);
      n->appbar.btn_y = buttons_y;
      n->appbar.title_y = title_y;
      n->appbar.subtitle_y = subtitle_y;
      CogitoNode* title_widget = cogito_appbar_title_widget(n);
      int action_count = 0;
      int action_total_w = 0;
      int action_row_h = 0;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        if (title_widget && c == title_widget) continue;
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        if (cw < 1) cw = 1;
        if (ch < 1) ch = btn_size;
        action_total_w += cw;
        if (ch > action_row_h) action_row_h = ch;
        action_count++;
      }
      if (action_count > 1) action_total_w += (action_count - 1) * btn_gap;
      if (action_row_h < btn_size) action_row_h = btn_size;

      int icon_row_h = appbar_h - (child_y - y) - 6;
      if (icon_row_h < action_row_h) icon_row_h = action_row_h;
      if (icon_row_h < btn_size) icon_row_h = btn_size;
      int cx = x + w - 12;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        if (title_widget && c == title_widget) continue;
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        if (cw < 1) cw = 1;
        if (ch < 1) ch = btn_size;
        cx -= cw;
        int cy = child_y + (icon_row_h - ch) / 2;
        cogito_layout_node(c, cx, cy, cw, ch);
        cx -= btn_gap;
      }
      if (title_widget) {
        int tw = 0;
        int th = 0;
        cogito_intrinsic_size(title_widget, &tw, &th);
        if (tw < 1) tw = 1;
        if (th < 1) th = btn_size;
        int tx = x + 16;
        int ty = title_y ? title_y : (y + btn_pad_top + btn_size + 18);
        int avail_right = x + w - 16;
        if (action_count > 0) {
          int actions_left = x + w - 12 - action_total_w;
          if (actions_left < avail_right) avail_right = actions_left - 12;
        }
        int max_w = avail_right - tx;
        if (max_w < 1) max_w = 1;
        if (tw > max_w) tw = max_w;
        cogito_layout_node(title_widget, tx, ty, tw, th);
      }
      break;
    }
    
