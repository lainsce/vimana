case COGITO_DIALOG: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg)           { n->bg = s.bg;                       n->bg_set = true;
  }
      if (s.has_border)       { n->border_color = s.border;          n->border_color_set = true;
  }
      if (s.has_border_width) { n->border_width = s.border_width;    n->border_width_set = true;
  }
  // Cogito: Dialog uses XL=28dp corner radius as a hard floor
      if (s.has_radius)       { n->border_radius = s.radius;
  }
      else if (n->border_radius < 28) { n->border_radius = 28;
  }
      if (s.has_shadow)       { n->shadow_level = s.shadow_level;   n->shadow_set = true;
  }
  // Draw box-shadow from SUM
      if (s.has_box_shadow && s.box_shadow.blur > 0) {
        cogito_draw_shadow_node_box(n, &s, n->x, n->y, n->w, n->h, n->border_radius, &s.box_shadow);
  } else if (n->shadow_set && n->shadow_level > 0) {
        cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, n->border_radius, n->shadow_level);
  }

      if (n->bg_set) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, n->bg, n->border_radius);
  }
      if (n->border_width > 0) {
        cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
  }

      int pad_l = COGITO_DIALOG_HEADER_INSET;
      int pad_t = COGITO_DIALOG_HEADER_INSET;
      int title_size = 20;
      int title_h = cogito_text_height_size(title_size);
      int cx = 0, cy = 0, cw = 0, ch = 0;
      cogito_dialog_close_rect(n, &cx, &cy, &cw, &ch);
      int header_h = title_h > ch ? title_h : ch;
      int section_pad_l = 24;
      int section_pad_r = 24;
      CogitoColor title_color = s.has_text ? s.text : n->text_color;

      // Draw title in header row.
      if (n->text && n->text->data[
    0
  ]) {
        int ty = n->y + pad_t + (header_h - title_h) / 2;
        cogito_draw_text_size_node(n, n->x + pad_l, ty, n->text->data, title_color, title_size, false);
  }
  // Draw close button in top-right.
      if (cw > 0 && ch > 0) {
        int mx = 0, my = 0;
        bool down = false;
        (void)cogito_pointer_query(&mx, &my, &down, NULL);
        bool over = cogito_hit_rect(mx, my, cx, cy, cw, ch);
        CogitoColor cbg = title_color;
        cbg.a = 36;
        if (over) cbg.a = down ? 72 : 48;
        int cr = cw / 2;
        if (cr < 4) cr = 4;
        cogito_draw_rect(cx, cy, cw, ch, cbg, cr);
        int icon_size = cw - 10;
        if (icon_size < 10) icon_size = 10;
        int ix = cx + (cw - icon_size) / 2;
        int iy = cy + (ch - icon_size) / 2;
        CogitoColor icon_color = s.has_icon_color ? s.icon_color : title_color;
        cogito_draw_icon_fallback(n,
    "xmark", ix, iy, icon_size, icon_color);
  }
      // Optional custom middle content: draw section dividers around it.
      if (n->len >= 3) {
        CogitoColor divider = s.has_border ? s.border : title_color;
        if (divider.a > 96) divider.a = 96;
        int x0 = n->x + section_pad_l;
        int x1 = n->x + n->w - section_pad_r - 1;
        if (x1 > x0) {
          CogitoNode* first = n->children[0];
          CogitoNode* second = n->children[1];
          int first_end = first->y + first->h + first->margin_bottom;
          int second_start = second->y + second->margin_top;
          if (second_start >= first_end) {
            int y1 = first_end + (second_start - first_end) / 2;
            cogito_draw_line(x0, y1, x1, y1, divider);
          }
          CogitoNode* before_actions = n->children[n->len - 2];
          CogitoNode* actions = n->children[n->len - 1];
          int custom_end = before_actions->y + before_actions->h + before_actions->margin_bottom;
          int actions_start = actions->y + actions->margin_top;
          if (actions_start >= custom_end) {
            int y2 = custom_end + (actions_start - custom_end) / 2;
            cogito_draw_line(x0, y2, x1, y2, divider);
          }
        }
      }
  // Draw children
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i
    ]);
  }
      break;
}
