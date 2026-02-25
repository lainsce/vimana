case COGITO_TEXTFIELD: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      bool focused = (cogito_focused == n && !n->disabled && n->editable);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, focused);

      CogitoColor bg = n->bg;
      CogitoColor text = n->text_color;
      CogitoColor border = n->border_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_text) text = s.text;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      if (cogito_has_selection_text) text = cogito_selection_text;
      cogito_anim_apply(n, &s, &bg, &text, &border, NULL);

      const int field_pad_y = 8;
      const int field_pad_lr = 16;
      const int field_pad_lr_icon = 12;
      const int icon_size = 24;
      const int icon_gap = 16;
      const int helper_top = 4;
      const int helper_gap = 16;

      const char* t = (n->text && n->text->data) ? n->text->data : "";
      bool has_text = t[0] != '\0';
      int size = cogito_node_font_size(n);
      int th = cogito_text_height_size(size);

      const char* placeholder = (n->subtitle && n->subtitle->data && n->subtitle->data[0]) ? n->subtitle->data : NULL;
      bool has_placeholder = placeholder && placeholder[0];
      const char* hint = (n->hint && n->hint->data && n->hint->data[0]) ? n->hint->data : NULL;
      bool has_hint = hint && hint[0];
      bool has_icon = (n->icon && n->icon->data && n->icon->data[0]);

      int hint_size = cogito_label_class_font_size(COGITO_LABEL_CLASS_CAPTION, 12);
      bool hint_bold = cogito_label_class_bold(COGITO_LABEL_CLASS_CAPTION, true);
      int hint_h = has_hint ? cogito_text_height_size(hint_size) : 0;
      int helper_extra = has_hint ? (helper_top + hint_h) : 0;
      int field_h = n->h - helper_extra;
      bool draw_hint = has_hint;
      if (field_h < 24) {
        field_h = n->h;
        draw_hint = false;
      }

      if (n->bg_set) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, field_h, bg, radius);
      }
      if (border_w > 0) {
        cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, field_h, border, radius, border_w);
      }

      int edge_pad = has_icon ? field_pad_lr_icon : field_pad_lr;
      int text_x = n->x + edge_pad;
      int text_right = n->x + n->w - edge_pad;
      if (has_icon) text_x += icon_size + icon_gap;
      int text_w = text_right - text_x;
      if (text_w < 0) text_w = 0;
      int ty = n->y + (field_h - th) / 2;

      if (has_icon) {
        CogitoColor icon_color = s.has_icon_color ? s.icon_color : text;
        int ix = n->x + field_pad_lr_icon;
        int iy = n->y + (field_h - icon_size) / 2;
        cogito_draw_icon_fallback(n, n->icon->data, ix, iy, icon_size, icon_color);
      }

      bool float_placeholder = has_placeholder && (focused || has_text);
      if (has_placeholder) {
        CogitoColor ph = text;
        ph.a = 140;
        float label_target = float_placeholder ? 1.0f : 0.0f;
        double now = cogito_now();
        const double dur_s = 0.300; // 300ms
        if (!n->tf_label_anim_inited) {
          n->tf_label_anim_inited = true;
          n->tf_label_anim_current = label_target;
          n->tf_label_anim_from = label_target;
          n->tf_label_anim_target = label_target;
          n->tf_label_anim_start = now;
        }
        if (n->tf_label_anim_target != label_target) {
          n->tf_label_anim_from = n->tf_label_anim_current;
          n->tf_label_anim_target = label_target;
          n->tf_label_anim_start = now;
        }
        float label_t = n->tf_label_anim_target;
        if (dur_s > 0.0) {
          double elapsed = (now - n->tf_label_anim_start) / dur_s;
          if (elapsed < 0.0) elapsed = 0.0;
          if (elapsed >= 1.0) {
            label_t = n->tf_label_anim_target;
          } else {
            double eased = cogito_ease_cubic_bezier(elapsed, 0.2, 0.0, 0.0, 1.0);
            label_t = (float)(n->tf_label_anim_from +
                              (n->tf_label_anim_target - n->tf_label_anim_from) * eased);
            double end_time = n->tf_label_anim_start + dur_s;
            if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
          }
        }
        if (label_t < 0.0f) label_t = 0.0f;
        if (label_t > 1.0f) label_t = 1.0f;
        n->tf_label_anim_current = label_t;

        int ph_size = (int)lround((double)size + ((double)hint_size - (double)size) * (double)label_t);
        if (ph_size < hint_size) ph_size = hint_size;
        if (ph_size > size) ph_size = size;
        int py_inline = n->y + (field_h - cogito_text_height_size(size)) / 2;
        int py_float = n->y + field_pad_y;
        int py = (int)lround((double)py_inline + ((double)py_float - (double)py_inline) * (double)label_t);
        cogito_draw_text_ellipsis(n, text_x, py, text_w, placeholder, ph, ph_size, false, 0);
      }

      if (float_placeholder) {
        ty = n->y + field_h - field_pad_y - th;
        if (ty < n->y + field_pad_y) ty = n->y + field_pad_y;
      }

      if (n->text_input.sel_start != n->text_input.sel_end) {
        int a = n->text_input.sel_start;
        int b = n->text_input.sel_end;
        if (a > b) { int tmp = a; a = b; b = tmp; }
        int sx = cogito_text_width_size_n_node(n, t, a, size);
        int ex = cogito_text_width_size_n_node(n, t, b, size);
        int x0 = text_x + sx;
        int x1 = text_x + ex;
        if (x0 < text_x) x0 = text_x;
        if (x1 > text_x + text_w) x1 = text_x + text_w;
        if (x1 > x0) {
          CogitoColor sel = s.has_selection ? s.selection : n->selection_color;
          cogito_draw_rect(x0, ty, x1 - x0, th, sel, 2);
        }
      }

      cogito_draw_text_ellipsis(n, text_x, ty, text_w, t, text, size, cogito_node_bold(n), 0);

      if (n->text_input.sel_start != n->text_input.sel_end) {
        int a2 = n->text_input.sel_start;
        int b2 = n->text_input.sel_end;
        if (a2 > b2) { int tmp = a2; a2 = b2; b2 = tmp; }
        int len2 = (int)strlen(t);
        if (a2 < 0) a2 = 0;
        if (b2 > len2) b2 = len2;
        if (b2 > a2) {
          CogitoColor sel2 = s.has_selection ? s.selection : n->selection_color;
          CogitoColor sel_text = cogito_on_color(sel2);
          int sx2 = cogito_text_width_size_n_node(n, t, a2, size);
          char sbuf[1024];
          int slen = b2 - a2;
          if (slen > 1000) slen = 1000;
          memcpy(sbuf, t + a2, (size_t)slen);
          sbuf[slen] = '\0';
          cogito_draw_text_size_node(n, text_x + sx2, ty, sbuf, sel_text, size, cogito_node_bold(n));
        }
      }

      if (focused) {
        int caret = n->text_input.caret;
        if (caret < 0) caret = 0;
        int tw = cogito_text_width_size_n_node(n, t, caret, size);
        int car_x = text_x + tw;
        if (car_x > text_x + text_w) car_x = text_x + text_w;
        cogito_draw_line(car_x, ty, car_x, ty + th, text);
      }

      if (draw_hint) {
        int hint_y = n->y + field_h + helper_top;
        CogitoColor hint_base = text;
        hint_base.a = (uint8_t)(hint_base.a * 0.78f);
        CogitoColor hint_color = cogito_label_class_color(COGITO_LABEL_CLASS_CAPTION, hint_base);

        const char* helper_left = hint;
        const char* helper_right = NULL;
        char helper_left_buf[512];
        const char* sep = strchr(hint, '\n');
        if (sep && sep[1]) {
          size_t left_len = (size_t)(sep - hint);
          if (left_len >= sizeof(helper_left_buf)) left_len = sizeof(helper_left_buf) - 1;
          memcpy(helper_left_buf, hint, left_len);
          helper_left_buf[left_len] = '\0';
          helper_left = helper_left_buf;
          helper_right = sep + 1;
        }

        int helper_pad = 16;
        int helper_x = n->x + helper_pad;
        int helper_w = n->w - helper_pad * 2;
        if (helper_w < 0) helper_w = 0;
        if (helper_right && helper_right[0]) {
          int right_w = cogito_text_width_size_node(n, helper_right, hint_size);
          if (right_w < 0) right_w = 0;
          int left_w = helper_w - right_w - helper_gap;
          if (left_w < 0) left_w = 0;
          cogito_draw_text_ellipsis(n, helper_x, hint_y, left_w, helper_left, hint_color, hint_size, hint_bold, 0);
          int right_x = n->x + n->w - helper_pad - right_w;
          cogito_draw_text_size_node(n, right_x, hint_y, helper_right, hint_color, hint_size, hint_bold);
        } else {
          cogito_draw_text_ellipsis(n, helper_x, hint_y, helper_w, helper_left, hint_color, hint_size, hint_bold, 0);
        }
      }
      break;
    }
