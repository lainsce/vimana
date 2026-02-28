case COGITO_CONTENT_LIST: {
  // VStack layout, children expand to full width
  int gap = n->gap;
  if (gap < 0) gap = 0;
  int item_pad_x = 6;
  int item_pad_y = 6;
  int cy = y;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    // Apply item padding to children that don't already have explicit padding
    if (!c->padding_left) c->padding_left = item_pad_x;
    if (!c->padding_right) c->padding_right = item_pad_x;
    if (!c->padding_top) c->padding_top = item_pad_y;
    if (!c->padding_bottom) c->padding_bottom = item_pad_y;
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_w = n->w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;
    if (c->hexpand || true) cw = slot_w; // children always expand in ContentList
    if (c->kind == COGITO_LABEL && c->text_wrap) {
      ch = cogito_text_wrap_height(c, c->text ? c->text->data : "", cogito_node_font_size(c), slot_w);
      if (ch <= 0) ch = cogito_text_height_size(cogito_node_font_size(c));
    }
    // Enforce 42px minimum height for list items
    if (ch < 42) ch = 42;
    cogito_layout_node(c, x + c->margin_left, cy + c->margin_top, cw, ch);
    cy += ch + c->margin_top + c->margin_bottom;
    if (i + 1 < n->len) cy += gap;
  }
  break;
}
