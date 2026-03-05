case COGITO_SETTINGS_LIST: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = s.has_padding_left ? s.padding_left : 0;
  int pad_r = s.has_padding_right ? s.padding_right : 0;
  int pad_t = s.has_padding_top ? s.padding_top : 8;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : 8;
  int inner_x = x + pad_l;
  int inner_y = y + pad_t;
  int inner_w = n->w - pad_l - pad_r;
  int inner_h = n->h - pad_t - pad_b;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  int cy = inner_y;
  // Skip title
  if (n->text && n->text->data && n->text->data[0]) {
    cy += cogito_text_height() + 12;
  }
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_w = inner_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;
    if (c->hexpand) cw = slot_w;
    else if (cw > slot_w) cw = slot_w;
    int remaining_h = inner_y + inner_h - (cy + c->margin_top + c->margin_bottom);
    if (remaining_h < 0) remaining_h = 0;
    if (c->vexpand) ch = remaining_h;
    cogito_layout_node(c, inner_x + c->margin_left, cy + c->margin_top, cw, ch);
    cy += ch + c->margin_top + c->margin_bottom;
  }
  break;
}
