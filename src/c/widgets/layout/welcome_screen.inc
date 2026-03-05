case COGITO_WELCOME_SCREEN: {
  int pad_l = n->padding_left > 0 ? n->padding_left : 32;
  int pad_t = n->padding_top > 0 ? n->padding_top : 32;
  int pad_r = n->padding_right > 0 ? n->padding_right : 32;
  int pad_b = n->padding_bottom > 0 ? n->padding_bottom : 32;
  int inner_x = x + pad_l;
  int inner_y = y + pad_t;
  int inner_w = n->w - pad_l - pad_r;
  int inner_h = n->h - pad_t - pad_b;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  int cy = inner_y + 80 + 40 + 24 + 16; // after icon + title + desc
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
    cy += ch + c->margin_top + c->margin_bottom + 8;
  }
  break;
}
