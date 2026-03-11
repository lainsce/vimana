case COGITO_TEXTVIEW: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      // Resolve base style first, then state overrides (hover only for now).
      CogitoStyle base = cogito_theme_resolve_node(n);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, false);
      CogitoColor bg = base.has_bg ? base.bg : n->bg;
      CogitoColor text = base.has_text ? base.text : n->text_color;
      int radius = base.has_radius ? base.radius : n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_radius) radius = s.radius;
      if (s.has_text) text = s.text;
      cogito_anim_apply(n, &s, &bg, &text, NULL, NULL);
      if (text.a < 8) {
        if (cogito_cached_on_surface.a) text = cogito_cached_on_surface;
        else text = cogito_rgba(30, 30, 30, 255);
      }
      /* Compute visible rect: when inside a scroller, clamp to the
       * scroller's viewport so the background corners stay at the
       * visible edges and text outside the viewport is clipped. */
      int vis_x = n->x;
      int vis_y = n->y;
      int vis_w = n->w;
      int vis_h = n->h;
      bool tv_in_scroller = false;
      {
        CogitoNode* p = n->parent;
        while (p && p->kind != COGITO_SCROLLER) p = p->parent;
        if (p) {
          tv_in_scroller = true;
          int sv_l = p->x + p->padding_left;
          int sv_t = p->y + p->padding_top;
          int sv_r = p->x + p->w - p->padding_right;
          int sv_b = p->y + p->h - p->padding_bottom;
          if (vis_x < sv_l) { vis_w -= (sv_l - vis_x); vis_x = sv_l; }
          if (vis_y < sv_t) { vis_h -= (sv_t - vis_y); vis_y = sv_t; }
          if (vis_x + vis_w > sv_r) vis_w = sv_r - vis_x;
          if (vis_y + vis_h > sv_b) vis_h = sv_b - vis_y;
          if (vis_w < 0) vis_w = 0;
          if (vis_h < 0) vis_h = 0;
        }
      }
      if (n->bg_set) {
        int bg_tl = radius, bg_tr = radius, bg_br = radius, bg_bl = radius;
        cogito_resolve_node_radii(n, &s, radius, vis_w, vis_h, &bg_tl, &bg_tr, &bg_br, &bg_bl);
        if (tv_in_scroller) {
          /* Square the corners at clipped edges. */
          if (vis_y > n->y) { bg_tl = 0; bg_tr = 0; }
          if (vis_y + vis_h < n->y + n->h) { bg_br = 0; bg_bl = 0; }
        }
        cogito_draw_rect_radii_aa(vis_x, vis_y, vis_w, vis_h, bg, bg_tl, bg_tr, bg_br, bg_bl);
      }
      /* Scissor: clip text/selection/caret to the visible area. */
      if (tv_in_scroller && cogito_backend && cogito_backend->begin_scissor) {
        cogito_backend->begin_scissor(vis_x, vis_y, vis_w, vis_h);
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
      cogito_draw_text_wrapped(n, n->x + pad, n->y + pad, n->w - pad * 2, t, text, size, cogito_node_bold(n), cogito_node_medium(n), 0, true, 0);
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
            cogito_draw_text_size_node(n, n->x + pad + sx2, cy2, sbuf, sel_text, size, cogito_node_bold(n), cogito_node_medium(n));
          }
          cy2 += line_h2;
        }
      }
      if (cogito_focused == n && !n->disabled && n->editable) {
        double now = cogito_now();
        const double pulse_period = 0.800;
        double phase = fmod(now, pulse_period) / pulse_period;
        double alpha_scale = 0.0;
        if (phase < 0.5) {
          double u = phase * 2.0;
          alpha_scale = cogito_ease_cubic_bezier(u, 0.4, 0.0, 0.6, 1.0);
        } else {
          double u = (phase - 0.5) * 2.0;
          alpha_scale = 1.0 - cogito_ease_cubic_bezier(u, 0.4, 0.0, 0.6, 1.0);
        }
        CogitoColor caret_col = text;
        int ca = (int)lround((double)caret_col.a * alpha_scale);
        if (ca < 0) ca = 0;
        if (ca > 255) ca = 255;
        caret_col.a = (uint8_t)ca;
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
            cogito_draw_line(cx, cy, cx, cy + line_h, caret_col);
            break;
          }
          cy += line_h;
          if (idx >= len) {
            // If text ends with '\n', caret can be on a trailing empty line.
            if (caret_idx == len && len > 0 && t[len - 1] == '\n') {
              int cx = n->x + pad;
              int max_x = n->x + n->w - pad;
              if (cx > max_x) cx = max_x;
              cogito_draw_line(cx, cy, cx, cy + line_h, caret_col);
            }
            break;
          }
        }
        double end_time = now + 0.12;
        if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
      }
      if (tv_in_scroller && cogito_backend && cogito_backend->end_scissor) {
        cogito_backend->end_scissor();
      }
      break;
    }
