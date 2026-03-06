case COGITO_APPBAR: {
      int btn_size = 12;
      int btn_gap = 10;
      int btn_pad_top = 15;
      int btn_pad_right = 15;
  int controls_edge_pad = 15;
  int controls_top_pad = 15;
      cogito_appbar_button_metrics(&btn_size, &btn_gap, &btn_pad_top, &btn_pad_right);
      int buttons_y = y + btn_pad_top;
      n->appbar.btn_size = btn_size;
      n->appbar.btn_y = buttons_y;
      CogitoNode* win = cogito_node_window(n);
      bool allow_max = !win || win->resizable;
      // Only show traffic lights on the first (leftmost) appbar
      if (cogito_is_first_appbar(win, n)) {
        int controls_x = win ? win->x : x;
        int controls_w = win ? win->w : w;
        cogito_appbar_controls_positions(n, controls_x, controls_w, btn_size, btn_gap, controls_edge_pad, allow_max,
                                         &n->appbar.btn_close_x, &n->appbar.btn_min_x, &n->appbar.btn_max_x);
        n->appbar.btn_y = (win ? win->y : y) + controls_top_pad;
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
      if (n->appbar.btn_close_x >= 0 || n->appbar.btn_min_x >= 0 || n->appbar.btn_max_x >= 0) {
        n->appbar.btn_y = (win ? win->y : y) + controls_top_pad;
      } else {
        n->appbar.btn_y = buttons_y;
      }
      n->appbar.title_y = title_y;
      n->appbar.subtitle_y = subtitle_y;
      CogitoNode* title_widget = cogito_appbar_title_widget(n);
      int lead_count = 0;
      int lead_total_w = 0;
      int trail_count = 0;
      int trail_total_w = 0;
      int action_row_h = 0;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        if (title_widget && c == title_widget) continue;
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        if (cw < 1) cw = 1;
        if (ch < 1) ch = btn_size;
        if (cogito_appbar_is_leading_child(c)) {
          lead_total_w += cw;
          lead_count++;
        } else {
          trail_total_w += cw;
          trail_count++;
        }
        if (ch > action_row_h) action_row_h = ch;
      }
      if (lead_count > 1) lead_total_w += (lead_count - 1) * btn_gap;
      if (trail_count > 1) trail_total_w += (trail_count - 1) * btn_gap;
      if (action_row_h < btn_size) action_row_h = btn_size;

      int appbar_type = cogito_appbar_type(n);
      int title_tx = 0;
      int title_ty = 0;
      int title_tw = 0;
      int title_th = 0;
      if (title_widget) {
        cogito_intrinsic_size(title_widget, &title_tw, &title_th);
        if (title_tw < 1) title_tw = 1;
        if (title_th < 1) title_th = btn_size;
        title_tx = x + 16;
        if (lead_count > 0) {
          title_tx += lead_total_w + 12;
        }
        title_ty = title_y ? title_y : (y + btn_pad_top + btn_size + 18);
        int avail_right = x + w - 16;
        if (trail_count > 0) {
          int actions_left = x + w - 12 - trail_total_w;
          if (actions_left < avail_right) avail_right = actions_left - 12;
        }
        int max_w = avail_right - title_tx;
        if (max_w < 1) max_w = 1;
        bool title_expand = title_widget->hexpand;
        if (!title_expand && (title_widget->kind == COGITO_HSTACK || title_widget->kind == COGITO_VSTACK)) {
          for (size_t i = 0; i < title_widget->len; i++) {
            if (title_widget->children[i] && title_widget->children[i]->hexpand) {
              title_expand = true;
              break;
            }
          }
        }
        if (appbar_type == COGITO_APPBAR_TYPE_SEARCH || title_expand) title_tw = max_w;
        else if (title_tw > max_w) title_tw = max_w;
      }

      int icon_row_h = action_row_h;
      if (icon_row_h < btn_size) icon_row_h = btn_size;
      int lx = x + 16;
      if (n->appbar.btn_close_x >= 0 || n->appbar.btn_min_x >= 0 || n->appbar.btn_max_x >= 0) {
        int rightmost_light = n->appbar.btn_close_x;
        if (n->appbar.btn_min_x > rightmost_light) rightmost_light = n->appbar.btn_min_x;
        if (n->appbar.btn_max_x > rightmost_light) rightmost_light = n->appbar.btn_max_x;
        if (rightmost_light >= 0) {
          int min_left = rightmost_light + btn_size + 12;
          if (min_left > lx) lx = min_left;
        }
      }
      int trail_left_limit = lx;
      if (title_widget) {
        int title_right = title_tx + title_tw + 12;
        if (title_right > trail_left_limit) trail_left_limit = title_right;
      }
      int cx = x + w - 12;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        if (title_widget && c == title_widget) continue;
        if (cogito_appbar_is_leading_child(c)) continue;
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        if (cw < 1) cw = 1;
        if (ch < 1) ch = btn_size;
        int max_cw = cx - trail_left_limit;
        if (max_cw < 1) {
          int cy = child_y + (icon_row_h - ch) / 2;
          cogito_layout_node(c, trail_left_limit, cy, 0, ch);
          continue;
        }
        if (cw > max_cw) cw = max_cw;
        cx -= cw;
        int cy = child_y + (icon_row_h - ch) / 2;
        cogito_layout_node(c, cx, cy, cw, ch);
        cx -= btn_gap;
        if (cx < trail_left_limit) cx = trail_left_limit;
      }
      int lead_right_limit = title_widget ? (title_tx - 12) : (cx + btn_gap);
      if (lead_right_limit < lx) lead_right_limit = lx;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        if (title_widget && c == title_widget) continue;
        if (!cogito_appbar_is_leading_child(c)) continue;
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        if (cw < 1) cw = 1;
        if (ch < 1) ch = btn_size;
        int max_cw = lead_right_limit - lx;
        if (max_cw < 1) {
          int cy = child_y + (icon_row_h - ch) / 2;
          cogito_layout_node(c, lx, cy, 0, ch);
          continue;
        }
        if (cw > max_cw) cw = max_cw;
        int cy = child_y + (icon_row_h - ch) / 2;
        cogito_layout_node(c, lx, cy, cw, ch);
        lx += cw + btn_gap;
        if (lx > lead_right_limit) lx = lead_right_limit;
      }
      n->appbar.title_x = title_tx > 0 ? title_tx : lx;
      if (title_widget) {
        cogito_layout_node(title_widget, title_tx, title_ty, title_tw, title_th);
      }
      break;
    }
    
