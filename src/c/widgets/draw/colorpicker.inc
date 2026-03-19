case COGITO_COLORPICKER: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = n->kd->colorpicker.popover_panel
        ? cogito_theme_resolve_node(n)
        : cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = s.has_bg ? s.bg : n->bg;
      CogitoColor border = s.has_border ? s.border : n->border_color;
      int border_w = s.has_border_width ? s.border_width : n->border_width;
      int radius = s.has_radius ? s.radius : (n->kd->colorpicker.popover_panel ? 20 : 8);

      int mx = 0, my = 0;
      bool down = false;
      (void)cogito_pointer_query(&mx, &my, &down, NULL);

      CogitoColor text_col = s.has_text ? s.text : n->text_color;
      CogitoColor accent = s.has_selection ? s.selection : n->selection_color;

      if (n->kd->colorpicker.popover_panel) {
        // ---- Panel mode: DatePicker-like header, rows, and footer actions ----
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
        }

        int base_size = cogito_node_font_size(n);
        int header_size = base_size;
        if (header_size < 14) header_size = 14;
        int label_size = base_size - 1;
        if (label_size < 13) label_size = 13;
        int action_size = base_size;
        if (action_size < 14) action_size = 14;

        CogitoColor text_muted = text_col;
        text_muted.a = 170;
        CogitoColor outline = border;
        if (outline.a > 110) outline.a = 110;

        int header_x = n->x + COGITO_COLORPICKER_PANEL_PAD;
        int header_y = n->y + COGITO_COLORPICKER_PANEL_TOP;
        int header_w = n->w - COGITO_COLORPICKER_PANEL_PAD * 2;
        int header_h = COGITO_COLORPICKER_PANEL_HEADER_H;
        int header_th = cogito_text_height_size(header_size + 1);
        int header_ty = header_y + (header_h - header_th) / 2;
        cogito_draw_text_size_node(n, header_x, header_ty, "Select Color", text_col, header_size + 1, true, false);

        if (!n->text || !n->text->data || !n->text->data[0]) {
          cogito_colorpicker_sync_hex(n);
        }
        const char* header_hex = (n->text && n->text->data) ? n->text->data : "";
        int hex_tw = cogito_text_width_size_node(n, header_hex, header_size);
        int hex_tx = header_x + header_w - hex_tw;
        if (hex_tx < header_x + 56) hex_tx = header_x + 56;
        cogito_draw_text_size_node(n, hex_tx, header_ty, header_hex, text_muted, header_size, false, false);

        int rows_top = cogito_colorpicker_panel_rows_top(n);
        int rows_sep_y = rows_top - (COGITO_COLORPICKER_PANEL_HEADER_TO_ROWS_GAP / 2);
        cogito_draw_line(header_x, rows_sep_y, header_x + header_w, rows_sep_y, outline);

        const char* labels[3] = {"Hue", "Chroma", "Tone"};
        int lx = n->x + COGITO_COLORPICKER_PANEL_PAD;
        CogitoNode* rows[3] = {NULL, NULL, NULL};
        int slider_count = cogito_colorpicker_panel_collect_sliders(n, rows);
        for (int si = 0; si < slider_count && si < 3; si++) {
          CogitoNode* c = rows[si];
          if (!c) continue;
          int lh = cogito_text_height_size(label_size);
          int ly = c->y + (c->h - lh) / 2;
          cogito_draw_text_size_node(n, lx, ly, labels[si], text_muted, label_size, false, false);

          int value = (int)lround(c->kd->slider.value);
          char value_buf[8];
          snprintf(value_buf, sizeof(value_buf), "%d", value);
          int vx = n->x + n->w - COGITO_COLORPICKER_PANEL_PAD - COGITO_COLORPICKER_PANEL_VALUE_W;
          int vy = c->y + (c->h - lh) / 2;
          cogito_draw_text_ellipsis(n, vx, vy, COGITO_COLORPICKER_PANEL_VALUE_W,
                                    value_buf, text_muted, label_size, false, false, 2);
        }

        // Draw slider children (positioned by layout phase)
        for (size_t i = 0; i < n->len; i++) {
          cogito_draw_node(n->children[i]);
        }

        // Footer: separator + Cancel + OK buttons
        {
          int fcx = 0, fcy = 0, fcw = 0, fch = 0;
          int fox = 0, foy = 0, fow = 0, foh = 0;
          cogito_colorpicker_panel_footer_layout(n, &fcx, &fcy, &fcw, &fch, &fox, &foy, &fow, &foh);

          int footer_y = n->y + n->h - COGITO_COLORPICKER_PANEL_BOTTOM - COGITO_COLORPICKER_PANEL_FOOTER_H;
          CogitoColor sep_col = outline;
          cogito_draw_line(n->x + COGITO_COLORPICKER_PANEL_PAD, footer_y - COGITO_COLORPICKER_PANEL_FOOTER_GAP,
                           n->x + n->w - COGITO_COLORPICKER_PANEL_PAD, footer_y - COGITO_COLORPICKER_PANEL_FOOTER_GAP, sep_col);

          bool over_cancel = cogito_hit_rect(mx, my, fcx, fcy, fcw, fch);
          bool over_ok = cogito_hit_rect(mx, my, fox, foy, fow, foh);
          CogitoColor action_hl = accent;
          action_hl.a = down ? 74 : 42;
          if (over_cancel) cogito_draw_rect(fcx, fcy, fcw, fch, action_hl, 16);
          if (over_ok) cogito_draw_rect(fox, foy, fow, foh, action_hl, 16);

          int act_th = cogito_text_height_size(action_size + 1);
          int cancel_tw = cogito_text_width_size_node(n, "Cancel", action_size + 1);
          int ok_tw = cogito_text_width_size_node(n, "OK", action_size + 1);
          cogito_draw_text_size_node(n, fcx + (fcw - cancel_tw) / 2, fcy + (fch - act_th) / 2,
                                     "Cancel", accent, action_size + 1, true, false);
          cogito_draw_text_size_node(n, fox + (fow - ok_tw) / 2, foy + (foh - act_th) / 2,
                                     "OK", accent, action_size + 1, true, false);
        }
      } else {
        // ---- Trigger mode: compact field with swatch + editable hex + palette icon ----
        CogitoNode* win = cogito_node_window(n);
        bool open = (win && win->popover_open && win->popover_anchor == n);
        CogitoNode* live = n;
        if (open && !n->kd->colorpicker.hex_editing && win && win->popover) {
          for (size_t i = 0; i < win->popover->len; i++) {
            CogitoNode* c = win->popover->children[i];
            if (c && c->kind == COGITO_COLORPICKER &&
                c->kd->colorpicker.popover_panel &&
                c->kd->colorpicker.owner == n) {
              live = c;
              break;
            }
          }
        }

        if (open) {
          border = accent;
          border_w = border_w < 2 ? 2 : border_w;
        } else if (border_w <= 0) {
          border_w = 1;
          border.a = 120;
        }

        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
        }

        if (!live->text || !live->text->data || !live->text->data[0] || !live->kd->colorpicker.hex_editing) {
          cogito_colorpicker_sync_hex(live);
        }

        int sw_x = 0, sw_y = 0, sw_w = 0, sw_h = 0;
        int hx = 0, hy = 0, hw = 0, hh = 0;
        int ix = 0, iy = 0, iw = 0, ih = 0;
        cogito_colorpicker_field_layout(n,
          &sw_x, &sw_y, &sw_w, &sw_h,
          &hx, &hy, &hw, &hh,
          &ix, &iy, &iw, &ih);

        // Color swatch
        CogitoColor swatch_color = cogito_hct_to_rgb(live->kd->colorpicker.h, live->kd->colorpicker.c, live->kd->colorpicker.t);
        cogito_draw_rect(sw_x, sw_y, sw_w, sw_h, swatch_color, 6);
        CogitoColor swatch_border = text_col;
        swatch_border.a = 60;
        cogito_draw_rect_lines(sw_x, sw_y, sw_w, sw_h, swatch_border, 6, 1);

        // Hex code text (selectable, editable)
        int size = cogito_node_font_size(n);
        int pad = 8;
        char hex_buf[8];
        const char* hex = live->text ? live->text->data : NULL;
        if (!hex || !hex[0]) {
          cogito_color_to_hex(swatch_color, hex_buf);
          hex = hex_buf;
        }
        int th = cogito_text_height_size(size);
        int ty = hy + (hh - th) / 2;

        if (n->kd->colorpicker.sel_start != n->kd->colorpicker.sel_end) {
          int a = n->kd->colorpicker.sel_start, b = n->kd->colorpicker.sel_end;
          if (a > b) { int tmp = a; a = b; b = tmp; }
          int sx_sel = cogito_text_width_size_n_node(n, hex, a, size);
          int ex = cogito_text_width_size_n_node(n, hex, b, size);
          int pill_pad = 4;
          int pill_r = th / 2;
          int x0 = hx + pad + sx_sel - pill_pad;
          int x1 = hx + pad + ex + pill_pad;
          if (x1 > x0) cogito_draw_rect(x0, ty, x1 - x0, th, accent, pill_r);
        }

        cogito_draw_text_ellipsis(n, hx + pad, ty, hw - pad * 2, hex, text_col, size, false, false, 0);

        if (cogito_focused == n && n->kd->colorpicker.hex_editing) {
          int caret = n->kd->colorpicker.caret;
          if (caret < 0) caret = 0;
          int tw = cogito_text_width_size_n_node(n, hex, caret, size);
          int cx = hx + pad + tw;
          int max_x = hx + hw - pad;
          if (cx > max_x) cx = max_x;
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
          CogitoColor caret_col = text_col;
          int ca = (int)lround((double)caret_col.a * alpha_scale);
          if (ca < 0) ca = 0;
          if (ca > 255) ca = 255;
          caret_col.a = (uint8_t)ca;
          cogito_draw_line(cx, ty, cx, ty + th, caret_col);
          double end_time = now + 0.12;
          if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
        }

        // Palette icon button (32x32 rounded highlight, centered in the 44xh icon area)
        int icon_box = 32;
        int icon_box_x = ix + (iw - icon_box) / 2;
        int icon_box_y = iy + (ih - icon_box) / 2;
        bool over_icon = cogito_hit_rect(mx, my, ix, iy, iw, ih);
        if (open || over_icon) {
          CogitoColor icon_hl = accent;
          icon_hl.a = (open || down) ? 62 : 40;
          cogito_draw_rect(icon_box_x, icon_box_y, icon_box, icon_box, icon_hl, icon_box / 2);
        }

        int icon_size = 18;
        CogitoColor icon_col = open ? accent : text_col;
        if (!open) icon_col.a = 190;
        cogito_draw_icon_fallback(n, "sf:paintpalette",
                                  icon_box_x + (icon_box - icon_size) / 2,
                                  icon_box_y + (icon_box - icon_size) / 2,
                                  icon_size, icon_col);
      }
      break;
    }
