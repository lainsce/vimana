case COGITO_TOAST: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = n->bg;
      CogitoColor border = n->border_color;
      CogitoColor text = s.has_text ? s.text : n->text_color;
      CogitoColor sel = s.has_selection ? s.selection : n->selection_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      // Cogito Snackbar: Full radius (h/2) as floor when not set by theme
      if (radius < n->h / 2) radius = n->h / 2;
      cogito_anim_apply(n, &s, &bg, &text, &border, &sel);
      if (n->bg_set || s.has_bg) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
    }
  }
      int size = cogito_node_font_size(n);
      CogitoColor text_bg = bg;
      text = s.has_text ? text : cogito_on_color(text_bg);
      int text_l = cogito_luma(text);
      int bg_l = cogito_luma(text_bg);
      if (abs(text_l - bg_l) < 80) {
        text = cogito_on_color(text_bg);
  }
      const char* t = n->text ? n->text->data : "";
      int th = cogito_text_height_size(size);
      int ty = n->y + (n->h - th) / 2;
      int pad_x = 16; // Cogito: Snackbar 16dp horizontal padding
      int cx = 0;
      int cy = 0;
      int cw = 0;
      int ch = 0;
      cogito_toast_close_rect(n, &cx, &cy, &cw, &ch);
      int action_w = 0;
      if (n->action_text && n->action_text->data[
    0
  ]) {
        action_w = cogito_text_width_size_node(n, n->action_text->data, size);
  }
      int text_w = (cw > 0) ? (cx - (n->x + pad_x) - 6) : (n->w - pad_x * 2);
      if (action_w > 0) {
        text_w -= (action_w + 24);
  }
      if (text_w < 0) text_w = 0;
      cogito_draw_text_ellipsis(n, n->x + pad_x, ty, text_w, t, text, size, cogito_node_bold(n),
  0);
      if (action_w > 0) {
        CogitoColor action_color = sel;
        int action_x = n->x + pad_x + text_w + 24;
        int action_y = ty;
        cogito_draw_text_size_node(n, action_x, action_y, n->action_text->data, action_color, size, false);
        n->action_button_x = action_x;
        n->action_button_y = action_y;
        n->action_button_w = action_w;
        n->action_button_h = th;
  }
      if (cw > 0 && ch > 0) {
        int icon_size = s.has_icon_size ? s.icon_size : 12;
        if (icon_size < 8) icon_size = 8;
        int ix = cx + (cw - icon_size) / 2;
        int iy = cy + (ch - icon_size) / 2;
        CogitoColor tint = s.has_icon_color ? s.icon_color : text;
        if (cogito_backend && cogito_backend->draw_texture) {
          CogitoTexture* tex = NULL;
          int tw = 0;
          int th_i = 0;
          if (cogito_icon_load_texture("x", icon_size, &tex, &tw, &th_i) && tex) {
            CogitoRect src = (CogitoRect){
          0,
          0, tw, th_i
        };
            CogitoRect dst = (CogitoRect){ix, iy, icon_size, icon_size
        };
            cogito_backend->draw_texture(tex, src, dst, tint);
      } else {
            cogito_draw_line(ix, iy, ix + icon_size, iy + icon_size, tint);
            cogito_draw_line(ix + icon_size, iy, ix, iy + icon_size, tint);
      }
    } else {
          cogito_draw_line(ix, iy, ix + icon_size, iy + icon_size, tint);
          cogito_draw_line(ix + icon_size, iy, ix, iy + icon_size, tint);
    }
  }
      break;
}