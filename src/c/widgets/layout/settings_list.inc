case COGITO_SETTINGS_LIST: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = s.has_padding_left ? s.padding_left : 0;
  int pad_r = s.has_padding_right ? s.padding_right : 0;
  int pad_t = s.has_padding_top ? s.padding_top : 8;
  int avail_w = n->w - pad_l - pad_r;
  int cy = y + pad_t;
  // Skip title
  if (n->text && n->text->data && n->text->data[0]) {
    cy += cogito_text_height() + 12;
  }
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (c->hexpand) cw = avail_w;
    cogito_layout_node(c, x + pad_l, cy, cw, ch);
    cy += ch;
  }
  break;
}
