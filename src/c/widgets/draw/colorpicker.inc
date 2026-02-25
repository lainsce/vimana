case COGITO_COLORPICKER: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = n->colorpicker.popover_panel
        ? cogito_theme_resolve_node(n)
        : cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = s.has_bg ? s.bg : n->bg;
      CogitoColor border = s.has_border ? s.border : n->border_color;
      int border_w = s.has_border_width ? s.border_width : n->border_width;
      int radius = s.has_radius ? s.radius : (n->colorpicker.popover_panel ? 20 : 8);

      int mx = 0, my = 0;
      bool down = false;
      (void)cogito_pointer_query(&mx, &my, &down, NULL);

      CogitoColor text_col = s.has_text ? s.text : n->text_color;
      CogitoColor accent = s.has_selection ? s.selection : n->selection_color;

      if (n->colorpicker.popover_panel) {
        // ---- Panel mode: background + H/C/T labels + slider children + footer ----
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
        }

        // Draw "H", "C", "T" labels left of each positioned slider child
        int size = cogito_node_font_size(n);
        const char* labels[3] = {"H", "C", "T"};
        int lx = n->x + 16;
        CogitoNode* rows[3] = {NULL, NULL, NULL};
        int slider_count = cogito_colorpicker_panel_collect_sliders(n, rows);
        for (int si = 0; si < slider_count && si < 3; si++) {
          CogitoNode* c = rows[si];
          if (!c) continue;
          int lh = cogito_text_height_size(size);
          int ly = c->y + (c->h - lh) / 2;
          cogito_draw_text_size_node(n, lx, ly, labels[si], text_col, size, false);
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

          int footer_y = n->y + n->h - COGITO_COLORPICKER_PANEL_FOOTER_H;
          CogitoColor sep_col = text_col;
          sep_col.a = 40;
          cogito_draw_line(n->x + 16, footer_y, n->x + n->w - 16, footer_y, sep_col);

          bool over_cancel = cogito_hit_rect(mx, my, fcx, fcy, fcw, fch);
          bool over_ok = cogito_hit_rect(mx, my, fox, foy, fow, foh);
          CogitoColor action_hl = accent;
          action_hl.a = down ? 74 : 42;
          if (over_cancel) cogito_draw_rect(fcx, fcy, fcw, fch, action_hl, 16);
          if (over_ok) cogito_draw_rect(fox, foy, fow, foh, action_hl, 16);

          int action_size = size;
          int act_th = cogito_text_height_size(action_size + 1);
          int cancel_tw = cogito_text_width_size_node(n, "Cancel", action_size + 1);
          int ok_tw = cogito_text_width_size_node(n, "OK", action_size + 1);
          cogito_draw_text_size_node(n, fcx + (fcw - cancel_tw) / 2, fcy + (fch - act_th) / 2,
                                     "Cancel", accent, action_size + 1, true);
          cogito_draw_text_size_node(n, fox + (fow - ok_tw) / 2, foy + (foh - act_th) / 2,
                                     "OK", accent, action_size + 1, true);
        }
      } else {
        // ---- Trigger mode: compact field with swatch + editable hex + palette icon ----
        CogitoNode* win = cogito_node_window(n);
        bool open = (win && win->popover_open && win->popover_anchor == n);
        CogitoNode* live = n;
        if (open && !n->colorpicker.hex_editing && win && win->popover) {
          for (size_t i = 0; i < win->popover->len; i++) {
            CogitoNode* c = win->popover->children[i];
            if (c && c->kind == COGITO_COLORPICKER &&
                c->colorpicker.popover_panel &&
                c->colorpicker.owner == n) {
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

        if (!live->text || !live->text->data || !live->text->data[0] || !live->colorpicker.hex_editing) {
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
        CogitoColor swatch_color = cogito_hct_to_rgb(live->colorpicker.h, live->colorpicker.c, live->colorpicker.t);
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

        if (n->text_input.sel_start != n->text_input.sel_end) {
          int a = n->text_input.sel_start, b = n->text_input.sel_end;
          if (a > b) { int tmp = a; a = b; b = tmp; }
          int sx_sel = cogito_text_width_size_n_node(n, hex, a, size);
          int ex = cogito_text_width_size_n_node(n, hex, b, size);
          int x0 = hx + pad + sx_sel;
          int x1 = hx + pad + ex;
          if (x1 > x0) cogito_draw_rect(x0, ty, x1 - x0, th, accent, 2);
        }

        cogito_draw_text_ellipsis(n, hx + pad, ty, hw - pad * 2, hex, text_col, size, false, 0);

        if (cogito_focused == n && n->colorpicker.hex_editing) {
          int caret = n->text_input.caret;
          if (caret < 0) caret = 0;
          int tw = cogito_text_width_size_n_node(n, hex, caret, size);
          int cx = hx + pad + tw;
          int max_x = hx + hw - pad;
          if (cx > max_x) cx = max_x;
          double time = cogito_now();
          if ((int)(time * 2.0) % 2 == 0) cogito_draw_line(cx, ty, cx, ty + th, text_col);
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
