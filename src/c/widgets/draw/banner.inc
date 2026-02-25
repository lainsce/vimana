case COGITO_BANNER: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int border_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 0);
  int radius = s.has_radius ? s.radius : (n->radius_set ? n->border_radius : 0);

  cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);

  // Draw background
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border_color, radius, border_w);
  }

  int pad_l = s.has_padding_left ? s.padding_left : 16;
  int pad_r = s.has_padding_right ? s.padding_right : 16;
  int size = cogito_node_font_size(n);
  int th = cogito_text_height_size(size);
  int content_x = n->x + pad_l;
  int content_y = n->y + (n->h - th) / 2;

  // Draw icon if present
  if (n->icon && n->icon->data && n->icon->data[0]) {
    int icon_size = 20;
    int iy = n->y + (n->h - icon_size) / 2;
    cogito_draw_icon_fallback(n, n->icon->data, content_x, iy, icon_size, text_color);
    content_x += icon_size + 12;
  }

  // Draw message text
  if (n->text && n->text->data && n->text->data[0]) {
    int avail = n->x + n->w - pad_r - content_x;
    // Reserve space for action button
    if (n->action_text && n->action_text->data && n->action_text->data[0]) {
      int aw = cogito_text_width_size_node(n, n->action_text->data, size) + 24;
      avail -= aw;
    }
    cogito_draw_text_size_node(n, content_x, content_y, n->text->data, text_color, size, false);
    (void)avail;
  }

  // Draw action button
  if (n->action_text && n->action_text->data && n->action_text->data[0]) {
    CogitoColor action_color = s.has_selection ? s.selection : text_color;
    int aw = cogito_text_width_size_node(n, n->action_text->data, size);
    int ax = n->x + n->w - pad_r - aw;
    int ay = n->y + (n->h - th) / 2;

    // Store action button rect for hit testing
    n->action_button_x = ax - 8;
    n->action_button_y = n->y + 4;
    n->action_button_w = aw + 16;
    n->action_button_h = n->h - 8;

    // Hover effect on action
    int mx = 0, my = 0;
    (void)cogito_pointer_query(&mx, &my, NULL, NULL);
    if (cogito_hit_rect(mx, my, n->action_button_x, n->action_button_y,
                        n->action_button_w, n->action_button_h)) {
      CogitoColor hov = action_color;
      hov.a = 30;
      cogito_draw_rect(n->action_button_x, n->action_button_y,
                       n->action_button_w, n->action_button_h, hov, 4);
    }

    cogito_draw_text_size_node(n, ax, ay, n->action_text->data, action_color, size, true);
  }
  break;
}
