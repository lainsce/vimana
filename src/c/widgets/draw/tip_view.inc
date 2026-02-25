case COGITO_TIP_VIEW: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = s.has_radius ? s.radius : 12;
  int shadow = s.has_shadow ? s.shadow_level : 2;
  int pad = 16;

  // Draw shadow
  if (shadow > 0) {
    cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, shadow);
  }

  // Draw background
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);

  // Draw border
  if (s.has_border) {
    int bw = s.has_border_width ? s.border_width : 1;
    cogito_draw_rect_lines(n->x, n->y, n->w, n->h, s.border, radius, bw);
  }

  int cy = n->y + pad;

  // Draw title
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    int ts = cogito_node_font_size(n);
    cogito_draw_text_size_node(n, n->x + pad, cy, n->subtitle->data, text_color, ts, true);
    cy += cogito_text_height_size(ts) + 8;
  }

  // Draw body text
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    CogitoColor body_col = text_color;
    body_col.a = (uint8_t)(body_col.a * 0.8f);
    cogito_draw_text_size_node(n, n->x + pad, cy, n->text->data, body_col, ts, false);
    cy += cogito_text_height_size(ts) + 8;
  }

  // Draw children
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i]);
  }
  break;
}
