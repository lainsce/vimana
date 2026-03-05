case COGITO_TIP_VIEW: {
  int pad_l = n->padding_left > 0 ? n->padding_left : 16;
  int pad_t = n->padding_top > 0 ? n->padding_top : 16;
  int pad_r = n->padding_right > 0 ? n->padding_right : 16;
  int pad_b = n->padding_bottom > 0 ? n->padding_bottom : 16;
  int inner_x = x + pad_l;
  int inner_y = y + pad_t;
  int inner_w = n->w - pad_l - pad_r;
  int inner_h = n->h - pad_t - pad_b;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  int cy = inner_y;
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    cy += cogito_text_height() + 8;
  }
  if (n->text && n->text->data && n->text->data[0]) {
    cy += cogito_text_height() + 8;
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
