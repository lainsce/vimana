case COGITO_TIP_VIEW: {
  int pad = 16;
  int cy = y + pad;
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    cy += cogito_text_height() + 8;
  }
  if (n->text && n->text->data && n->text->data[0]) {
    cy += cogito_text_height() + 8;
  }
  int avail_w = n->w - pad * 2;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (c->hexpand) cw = avail_w;
    cogito_layout_node(c, x + pad, cy, cw, ch);
    cy += ch;
  }
  break;
}
