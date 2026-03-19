    case COGITO_WINDOW_BAR: {
      // WindowBar: full-width titlebar with AppMenu button
      n->x = x;
      n->y = y;
      n->w = w;
      n->h = h > 0 ? h : 41;
      // Compute AppMenu button rect (icon + app name + chevron)
      if (n->kd) {
        int bar_h = n->h;
        int pad_left = 8;
        int icon_sz = 20;
        int gap = 6;
        int chevron_sz = 10;
        int pad_right = 8;
        int font_sz = 13;
        const char* name = NULL;
        if (cogito_active_app && cogito_active_app->app_name &&
            cogito_active_app->app_name->data && cogito_active_app->app_name->data[0]) {
          name = cogito_active_app->app_name->data;
        }
        int text_w = name ? cogito_text_width_size(name, font_sz) : 0;
        int content_w = pad_left + icon_sz + gap + text_w + gap + chevron_sz + pad_right;
        // Determine left and right reserved zones from appbar traffic lights
        int left_reserved = 0;
        int right_reserved = 0;
        CogitoNode* win_node = cogito_node_window(n);
        CogitoNode* ab = win_node ? cogito_find_appbar(win_node) : NULL;
        if (ab && ab->kd) {
          int btn_sz = ab->kd->appbar.btn_size;
          if (btn_sz < 1) btn_sz = 14;
          int pad = 12;
          if (ab->kd->appbar.btn_close_x >= 0 || ab->kd->appbar.btn_min_x >= 0 || ab->kd->appbar.btn_max_x >= 0) {
            // Find leftmost and rightmost traffic light positions
            int leftmost = w + x, rightmost = x;
            int positions[3] = { ab->kd->appbar.btn_close_x, ab->kd->appbar.btn_min_x, ab->kd->appbar.btn_max_x };
            for (int i = 0; i < 3; i++) {
              if (positions[i] >= 0) {
                if (positions[i] < leftmost) leftmost = positions[i];
                if (positions[i] > rightmost) rightmost = positions[i];
              }
            }
            // Determine if controls are on left side, right side, or both
            int bar_center = x + w / 2;
            bool has_left = (leftmost < bar_center);
            bool has_right = (rightmost + btn_sz > bar_center);
            if (has_left) {
              int lr = rightmost + btn_sz + pad - x;
              if (lr > left_reserved) left_reserved = lr;
            }
            if (has_right) {
              int rr = (x + w) - leftmost + pad;
              if (rr > right_reserved) right_reserved = rr;
            }
          }
        }
        // Position AppMenu button after left reserved zone
        int btn_x = x + left_reserved;
        int btn_y = y;
        int btn_h = bar_h;
        // Clamp width if it would overlap the right reserved zone
        int avail = w - left_reserved - right_reserved;
        int btn_w = content_w;
        if (btn_w > avail && avail > 0) btn_w = avail;
        n->kd->window_bar.appmenu_x = btn_x;
        n->kd->window_bar.appmenu_y = btn_y;
        n->kd->window_bar.appmenu_w = btn_w;
        n->kd->window_bar.appmenu_h = btn_h;
      }
      break;
    }
