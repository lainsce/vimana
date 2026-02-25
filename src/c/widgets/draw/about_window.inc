case COGITO_ABOUT_WINDOW: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int border_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 0);
  int radius = s.has_radius ? s.radius : (n->radius_set ? n->border_radius : 26);
  int shadow = s.has_shadow ? s.shadow_level : n->shadow_level;

  cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);
  if (shadow > 0) {
    cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, shadow);
  }
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border_color, radius, border_w);
  }

  cogito_about_window_sync_title(n);
  cogito_about_window_sync_icon(n);
  cogito_about_window_sync_meta(n);
  cogito_about_window_sync_actions(n);

  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i
    ]);
  }
  break;
}