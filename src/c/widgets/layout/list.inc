static void cogito_layout_list(CogitoNode* n, int x, int y) {
  int pad = n->gap;
  int cy = y + n->padding_top;
  int item_pad_x = 6;
  int item_pad_y = 6;
  int avail_w = n->w - n->padding_left - n->padding_right;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    // Apply item padding to children that don't already have explicit padding
    if (!c->padding_left) c->padding_left = item_pad_x;
    if (!c->padding_right) c->padding_right = item_pad_x;
    if (!c->padding_top) c->padding_top = item_pad_y;
    if (!c->padding_bottom) c->padding_bottom = item_pad_y;
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_w = avail_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;
    // List items always fill the available width
    cw = slot_w;
    if (c->kind == COGITO_LABEL && c->text_wrap) {
      ch = cogito_text_wrap_height(c, c->text ? c->text->data : "", cogito_node_font_size(c), slot_w);
      if (ch <= 0) ch = cogito_text_height_size(cogito_node_font_size(c));
    }
    // Enforce 42px minimum height for list items
    if (ch < 42) ch = 42;
    cy += c->margin_top;
    cogito_layout_node(c, x + n->padding_left + c->margin_left, cy, cw, ch);
    cy += ch + c->margin_bottom;
    if (i < n->len - 1) cy += pad;
  }
}
