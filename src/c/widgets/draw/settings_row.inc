case COGITO_SETTINGS_ROW: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  CogitoColor text_color = s.has_text ? s.text : n->text_color;

  // Draw label: left side in LTR, right side in RTL
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    int th = cogito_text_height_size(ts);
    int tw = cogito_text_width_size_node(n, n->text->data, ts);
    int tx = cogito_is_rtl() ? (n->x + n->w - 16 - tw) : (n->x + 16);
    int ty = n->y + (n->h - th) / 2;
    cogito_draw_text_size_node(n, tx, ty, n->text->data, text_color, ts, false, false);
  }

  // Draw child widget (on right, positioned by layout)
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i]);
  }
  break;
}
