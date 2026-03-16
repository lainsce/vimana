case COGITO_VIEW_CHOOSER: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int radius = s.has_radius ? s.radius : 8;
  int border_w = s.has_border_width ? s.border_width : 1;

  cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);

  // Draw dropdown background with subtle shadow from border color
  CogitoColor shadow_col = border_color;
  shadow_col.a = (uint8_t)(border_color.a * 0.15f);
  cogito_draw_rect(n->x, n->y + 2, n->w, n->h, shadow_col, radius);
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border_color, radius, border_w);
  }

  // Draw selected item text using node's style (font-size, font-weight, font-variation)
  int sel = n->selected >= 0 ? n->selected : 0;
  int text_end_x = n->x + 16;
  if (n->tab_labels && sel >= 0 && sel < (int)n->tab_len && n->tab_labels[sel]) {
    int ts = s.has_font_size ? s.font_size : cogito_node_font_size(n);
    bool lbold = s.has_font_weight ? (s.font_weight >= 600) : cogito_node_bold(n);
    bool lmedium = (!lbold && s.has_font_weight) ? (s.font_weight >= 500 && s.font_weight < 600) : cogito_node_medium(n);
    int th = cogito_text_height_size(ts);
    int tx = n->x + 16;
    int ty = n->y + (n->h - th) / 2;
    cogito_draw_text_size_node(n, tx, ty, n->tab_labels[sel]->data, text_color, ts, lbold, lmedium);
    text_end_x = tx + cogito_text_width_size_node(n, n->tab_labels[sel]->data, ts);
  }

  int icon_size = s.has_icon_size ? s.icon_size : 22;
  if (icon_size < 8) icon_size = 8;
  if (icon_size > n->h - 4) icon_size = n->h - 4;
  if (icon_size < 8) icon_size = 8;
  int icon_x = text_end_x + 12;
  int icon_y = n->y + (n->h - icon_size) / 2;
  CogitoColor icon_color = s.has_icon_color ? s.icon_color : text_color;
  icon_color.a = (uint8_t)(icon_color.a * 0.65f);
  cogito_draw_icon_fallback(n, "chevron.down", icon_x, icon_y, icon_size, icon_color);
  break;
}
