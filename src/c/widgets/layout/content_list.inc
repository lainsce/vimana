case COGITO_CONTENT_LIST: {
  // VStack layout, children expand to full width
  int gap = n->gap;
  if (gap < 0) gap = 0;
  int cy = y;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_w = n->w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;
    if (c->hexpand || true) cw = slot_w; // children always expand in ContentList
    cogito_layout_node(c, x + c->margin_left, cy + c->margin_top, cw, ch);
    cy += ch + c->margin_top + c->margin_bottom;
    if (i + 1 < n->len) cy += gap;
  }
  break;
}
