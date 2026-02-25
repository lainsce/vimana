case COGITO_TEXTVIEW: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      // Only apply focus styling when focused via keyboard (tab), not mouse click
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, false);
      CogitoColor bg = n->bg;
      CogitoColor text = n->text_color;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_radius) radius = s.radius;
      if (cogito_has_selection_text) text = cogito_selection_text;
      else if (s.has_text) text = s.text;
      cogito_anim_apply(n, &s, &bg, &text, NULL, NULL);
      if (n->bg_set) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
      }
      int pad = 8;
      int size = cogito_node_font_size(n);
      const char* t = n->text ? n->text->data : "";
      if (n->text_input.sel_start != n->text_input.sel_end) {
        int a = n->text_input.sel_start;
        int b = n->text_input.sel_end;
        if (a > b) { int tmp = a; a = b; b = tmp; }
        int max_w = n->w - pad * 2;
        if (max_w < 1) max_w = 1;
        int len = (int)strlen(t);
        int idx = 0;
        int line_h = cogito_text_height_size(size);
        int cy = n->y + pad;
        CogitoColor sel = s.has_selection ? s.selection : n->selection_color;
        while (idx < len) {
          int line_start = idx;
          int line_len = 0;
          int last_space = -1;
          for (int i = idx; i < len; i++) {
            char c = t[i];
            if (c == '\n') {
              line_len = i - idx;
              idx = i + 1;
              break;
            }
            int w = cogito_text_width_size_n_node(n, t + idx, i - idx + 1, size);
            if (c == ' ') last_space = i;
            if (w > max_w) {
              if (last_space >= idx) {
                line_len = last_space - idx;
                idx = last_space + 1;
              } else {
                line_len = i - idx;
                idx = i;
              }
              break;
            }
            if (i == len - 1) {
              line_len = i - idx + 1;
              idx = len;
            }
          }
          int line_end = line_start + line_len;
          if (b > line_start && a < line_end) {
            int sa = a > line_start ? a : line_start;
            int sb = b < line_end ? b : line_end;
            int sx = cogito_text_width_size_n_node(n, t + line_start, sa - line_start, size);
            int ex = cogito_text_width_size_n_node(n, t + line_start, sb - line_start, size);
            int x0 = n->x + pad + sx;
            int x1 = n->x + pad + ex;
            if (x1 > x0) {
              cogito_draw_rect(x0, cy, x1 - x0, line_h, sel, 2);
            }
          }
          cy += line_h;
        }
      }
      cogito_draw_text_wrapped(n, n->x + pad, n->y + pad, n->w - pad * 2, t, text, size, cogito_node_bold(n), 0, true);
      if (n->text_input.sel_start != n->text_input.sel_end) {
        int a2 = n->text_input.sel_start;
        int b2 = n->text_input.sel_end;
        if (a2 > b2) { int tmp = a2; a2 = b2; b2 = tmp; }
        int max_w2 = n->w - pad * 2;
        if (max_w2 < 1) max_w2 = 1;
        int len2 = (int)strlen(t);
        int idx2 = 0;
        int line_h2 = cogito_text_height_size(size);
        int cy2 = n->y + pad;
        CogitoColor sel2 = s.has_selection ? s.selection : n->selection_color;
        CogitoColor sel_text = cogito_on_color(sel2);
        while (idx2 < len2) {
          int line_start2 = idx2;
          int line_len2 = 0;
          int last_space2 = -1;
          for (int i = idx2; i < len2; i++) {
            char c = t[i];
            if (c == '\n') {
              line_len2 = i - idx2;
              idx2 = i + 1;
              break;
            }
            int w2 = cogito_text_width_size_n_node(n, t + idx2, i - idx2 + 1, size);
            if (c == ' ') last_space2 = i;
            if (w2 > max_w2) {
              if (last_space2 >= idx2) {
                line_len2 = last_space2 - idx2;
                idx2 = last_space2 + 1;
              } else {
                line_len2 = i - idx2;
                idx2 = i;
              }
              break;
            }
            if (i == len2 - 1) {
              line_len2 = i - idx2 + 1;
              idx2 = len2;
            }
          }
          int line_end2 = line_start2 + line_len2;
          if (b2 > line_start2 && a2 < line_end2) {
            int sa = a2 > line_start2 ? a2 : line_start2;
            int sb = b2 < line_end2 ? b2 : line_end2;
            int sx2 = cogito_text_width_size_n_node(n, t + line_start2, sa - line_start2, size);
            char sbuf[1024];
            int slen = sb - sa;
            if (slen > 1000) slen = 1000;
            memcpy(sbuf, t + sa, (size_t)slen);
            sbuf[slen] = '\0';
            cogito_draw_text_size_node(n, n->x + pad + sx2, cy2, sbuf, sel_text, size, cogito_node_bold(n));
          }
          cy2 += line_h2;
        }
      }
      if (cogito_focused == n && !n->disabled && n->editable) {
        int max_w = n->w - pad * 2;
        if (max_w < 1) max_w = 1;
        int len = (int)strlen(t);
        int idx = 0;
        int line_h = cogito_text_height_size(size);
        int cy = n->y + pad;
        int caret_idx = n->text_input.caret;
        if (caret_idx < 0) caret_idx = 0;
        if (caret_idx > len) caret_idx = len;
        while (idx <= len) {
          int line_start = idx;
          int line_len = 0;
          int last_space = -1;
          for (int i = idx; i < len; i++) {
            char c = t[i];
            if (c == '\n') {
              line_len = i - idx;
              idx = i + 1;
              break;
            }
            int w = cogito_text_width_size_n_node(n, t + idx, i - idx + 1, size);
            if (c == ' ') last_space = i;
            if (w > max_w) {
              if (last_space >= idx) {
                line_len = last_space - idx;
                idx = last_space + 1;
              } else {
                line_len = i - idx;
                idx = i;
              }
              break;
            }
            if (i == len - 1) {
              line_len = i - idx + 1;
              idx = len;
            }
          }
          int line_end = line_start + line_len;
          if (caret_idx >= line_start && caret_idx <= line_end) {
            int local = caret_idx - line_start;
            int cx = n->x + pad + cogito_text_width_size_n_node(n, t + line_start, local, size);
            int max_x = n->x + n->w - pad;
            if (cx > max_x) cx = max_x;
            cogito_draw_line(cx, cy, cx, cy + line_h, text);
            break;
          }
          cy += line_h;
          if (idx >= len) {
            // If text ends with '\n', caret can be on a trailing empty line.
            if (caret_idx == len && len > 0 && t[len - 1] == '\n') {
              int cx = n->x + pad;
              int max_x = n->x + n->w - pad;
              if (cx > max_x) cx = max_x;
              cogito_draw_line(cx, cy, cx, cy + line_h, text);
            }
            break;
          }
        }
      }
      break;
    }
