case COGITO_WELCOME_SCREEN: {
  int pad = 32;
  int cy = y + pad + 80 + 40 + 24 + 16; // after icon + title + desc
  int avail_w = n->w - pad * 2;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (c->hexpand) cw = avail_w;
    cogito_layout_node(c, x + pad, cy, cw, ch);
    cy += ch + 8;
  }
  break;
}
