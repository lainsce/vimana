case COGITO_TEXTVIEW: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      // Resolve base style first, then state overrides (hover only for now).
      CogitoStyle base = cogito_theme_resolve_node(n);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, false);
      CogitoColor bg = base.has_bg ? base.bg : n->bg;
      CogitoColor text = base.has_text ? base.text : n->text_color;
  CogitoColor border = base.has_border ? base.border : n->border_color;
  int border_w = base.has_border_width ? base.border_width : n->border_width;
      int radius = base.has_radius ? base.radius : n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_radius) radius = s.radius;
      if (s.has_text) text = s.text;
  if (s.has_border) border = s.border;
  if (s.has_border_width) border_w = s.border_width;
  cogito_anim_apply(n, &s, &bg, &text, &border, NULL);
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
      if (!tv_in_scroller && vis_w > 0 && vis_h > 0) {
        if (s.has_box_shadow && s.box_shadow.blur > 0) {
          cogito_draw_shadow_node_box(n, &s, n->x, n->y, n->w, n->h, radius, &s.box_shadow);
        } else if (s.has_shadow && s.shadow_level >= 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, s.shadow_level);
        } else if (n->shadow_set && n->shadow_level >= 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
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
      if (n->kd->text_input.sel_start != n->kd->text_input.sel_end) {
        int a = n->kd->text_input.sel_start;
        int b = n->kd->text_input.sel_end;
        if (a > b) { int tmp = a; a = b; b = tmp; }
        int max_w = n->w - pad * 2;
        if (max_w < 1) max_w = 1;
        int len = (int)strlen(t);
        int idx = 0;
        int base_line_h = cogito_text_height_size(size);
        int cy = n->y + pad;
        int pill_r = base_line_h / 2;
        int pill_pad = 4; // horizontal padding around text in pill
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
            int w = (n->span_count > 0)
              ? cogito_line_width_with_spans(n, t, idx, i - idx + 1, size)
              : cogito_text_width_size_n_node(n, t + idx, i - idx + 1, size);
            if (c == ' ') last_space = i;
            if (w > max_w && i > idx) {
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
          int pill_h = cogito_line_height_with_spans(n, line_start, line_len, size);
          int adv_h = pill_h;
          {
            bool lh_set = false;
            float lh = cogito_get_para_line_height(n, line_start, &lh_set);
            if (lh_set && lh > 0.0f)
              adv_h = (int)lroundf(pill_h * lh);
          }
          int line_end = line_start + line_len;
          // Compute alignment offset for this line.
          int full_lw = (n->span_count > 0)
            ? cogito_line_width_with_spans(n, t, line_start, line_len, size)
            : cogito_text_width_size_n_node(n, t + line_start, line_len, size);
          int align_off = 0;
          {
            int pa = cogito_get_para_align(n, line_start);
            if (pa == 1) align_off = (max_w - full_lw) / 2;
            else if (pa == 2) align_off = max_w - full_lw;
          }
          if (align_off < 0) align_off = 0;
          if (b > line_start && a < line_end) {
            int sa = a > line_start ? a : line_start;
            int sb = b < line_end ? b : line_end;
            int sx = (n->span_count > 0)
              ? cogito_line_width_with_spans(n, t, line_start, sa - line_start, size)
              : cogito_text_width_size_n_node(n, t + line_start, sa - line_start, size);
            int ex = (n->span_count > 0)
              ? cogito_line_width_with_spans(n, t, line_start, sb - line_start, size)
              : cogito_text_width_size_n_node(n, t + line_start, sb - line_start, size);
            int x0 = n->x + pad + align_off + sx - pill_pad;
            int x1 = n->x + pad + align_off + ex + pill_pad;
            if (x0 < n->x) x0 = n->x;
            if (x1 > n->x + n->w) x1 = n->x + n->w;
            if (x1 > x0) {
              cogito_draw_rect(x0, cy, x1 - x0, pill_h, sel, pill_r);
            }
          }
          cy += adv_h;
        }
      }
      cogito_draw_text_wrapped_spans(n, n->x + pad, n->y + pad, n->w - pad * 2, t, text, size, cogito_node_bold(n), cogito_node_medium(n), cogito_get_para_align(n, 0), true, 0);
      if (n->kd->text_input.sel_start != n->kd->text_input.sel_end) {
        int a2 = n->kd->text_input.sel_start;
        int b2 = n->kd->text_input.sel_end;
        if (a2 > b2) { int tmp = a2; a2 = b2; b2 = tmp; }
        int max_w2 = n->w - pad * 2;
        if (max_w2 < 1) max_w2 = 1;
        int len2 = (int)strlen(t);
        int idx2 = 0;
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
            int w2 = (n->span_count > 0)
              ? cogito_line_width_with_spans(n, t, idx2, i - idx2 + 1, size)
              : cogito_text_width_size_n_node(n, t + idx2, i - idx2 + 1, size);
            if (c == ' ') last_space2 = i;
            if (w2 > max_w2 && i > idx2) {
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
          int lh2 = cogito_line_height_with_spans(n, line_start2, line_len2, size);
          int adv2 = lh2;
          {
            bool lh_set = false;
            float lh = cogito_get_para_line_height(n, line_start2, &lh_set);
            if (lh_set && lh > 0.0f)
              adv2 = (int)lroundf(lh2 * lh);
          }
          int line_end2 = line_start2 + line_len2;
          int full_lw2 = (n->span_count > 0)
            ? cogito_line_width_with_spans(n, t, line_start2, line_len2, size)
            : cogito_text_width_size_n_node(n, t + line_start2, line_len2, size);
          int ao2 = 0;
          {
            int pa2 = cogito_get_para_align(n, line_start2);
            if (pa2 == 1) ao2 = (max_w2 - full_lw2) / 2;
            else if (pa2 == 2) ao2 = max_w2 - full_lw2;
          }
          if (ao2 < 0) ao2 = 0;
          if (b2 > line_start2 && a2 < line_end2) {
            int sa = a2 > line_start2 ? a2 : line_start2;
            int sb = b2 < line_end2 ? b2 : line_end2;
            int sx2 = (n->span_count > 0)
              ? cogito_line_width_with_spans(n, t, line_start2, sa - line_start2, size)
              : cogito_text_width_size_n_node(n, t + line_start2, sa - line_start2, size);
            if (n->span_count > 0) {
              cogito_draw_line_with_spans(n, n->x + pad + ao2 + sx2, cy2, t,
                                          sa, sb - sa, sel_text, size, true);
            } else {
              char sbuf[1024];
              int slen = sb - sa;
              if (slen > 1000) slen = 1000;
              memcpy(sbuf, t + sa, (size_t)slen);
              sbuf[slen] = '\0';
              cogito_draw_text_size_node(n, n->x + pad + ao2 + sx2, cy2, sbuf, sel_text, size, cogito_node_bold(n), cogito_node_medium(n));
            }
          }
          cy2 += adv2;
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
        /* Compute caret height from the span at the caret position. */
        int caret_h;
        {
          CogitoTextSpan caret_span = cogito_spans_at(n, n->kd->text_input.caret);
          int ch = (caret_span.size_set && caret_span.size > 0)
            ? cogito_text_height_size(caret_span.size)
            : cogito_text_height_size(size);
          bool lh_set = false;
          float lh = cogito_get_para_line_height(n, n->kd->text_input.caret, &lh_set);
          caret_h = (lh_set && lh > 0.0f)
            ? (int)lroundf(ch * lh) : ch;
        }
        int cy = n->y + pad;
        int caret_idx = n->kd->text_input.caret;
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
            int w = (n->span_count > 0)
              ? cogito_line_width_with_spans(n, t, idx, i - idx + 1, size)
              : cogito_text_width_size_n_node(n, t + idx, i - idx + 1, size);
            if (c == ' ') last_space = i;
            if (w > max_w && i > idx) {
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
            int caret_lw = (n->span_count > 0)
              ? cogito_line_width_with_spans(n, t, line_start, line_len, size)
              : cogito_text_width_size_n_node(n, t + line_start, line_len, size);
            int caret_ao = 0;
            {
              int pa = cogito_get_para_align(n, line_start);
              if (pa == 1) caret_ao = (max_w - caret_lw) / 2;
              else if (pa == 2) caret_ao = max_w - caret_lw;
            }
            if (caret_ao < 0) caret_ao = 0;
            int cx = n->x + pad + caret_ao + ((n->span_count > 0)
              ? cogito_line_width_with_spans(n, t, line_start, local, size)
              : cogito_text_width_size_n_node(n, t + line_start, local, size));
            int max_x = n->x + n->w - pad;
            if (cx > max_x) cx = max_x;
            cogito_draw_line(cx, cy, cx, cy + caret_h, caret_col);
            break;
          }
          int adv_c = cogito_line_height_with_spans(n, line_start, line_len, size);
          {
            bool lh_set = false;
            float lh = cogito_get_para_line_height(n, line_start, &lh_set);
            if (lh_set && lh > 0.0f)
              adv_c = (int)lroundf(adv_c * lh);
          }
          cy += adv_c;
          if (idx >= len) {
            // If text ends with '\n', caret can be on a trailing empty line.
            if (caret_idx == len && len > 0 && t[len - 1] == '\n') {
              int empty_ao = 0;
              {
                int pa = cogito_get_para_align(n, len);
                if (pa == 1) empty_ao = max_w / 2;
                else if (pa == 2) empty_ao = max_w;
              }
              int cx = n->x + pad + empty_ao;
              int max_x = n->x + n->w - pad;
              if (cx > max_x) cx = max_x;
              cogito_draw_line(cx, cy, cx, cy + caret_h, caret_col);
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
      if (border_w > 0 && border.a > 0 && vis_w > 0 && vis_h > 0) {
        int b_tl = radius, b_tr = radius, b_br = radius, b_bl = radius;
        cogito_resolve_node_radii(n, &s, radius, vis_w, vis_h, &b_tl, &b_tr, &b_br, &b_bl);
        if (tv_in_scroller) {
          if (vis_y > n->y) { b_tl = 0; b_tr = 0; }
          if (vis_y + vis_h < n->y + n->h) { b_br = 0; b_bl = 0; }
        }
        cogito_draw_rect_lines_radii_aa(vis_x, vis_y, vis_w, vis_h, border, b_tl, b_tr, b_br, b_bl, border_w);
      }
      break;
    }
