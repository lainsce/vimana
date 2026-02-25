case COGITO_SWITCHBAR: {
  // Children are laid out below the bar, VStack-style
  int bar_h = 48;
  int cy = y + bar_h;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_w = n->w - c->margin_left - c->margin_right;
    if (c->hexpand) cw = slot_w;
    cogito_layout_node(c, x + c->margin_left, cy + c->margin_top, cw, ch);
    cy += ch + c->margin_top + c->margin_bottom;
  }
  break;
}
