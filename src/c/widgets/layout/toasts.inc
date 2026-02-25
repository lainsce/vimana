static void cogito_layout_toasts(CogitoNode* n, int x, int y) {
  CogitoNode* win = cogito_node_window(n);
  int base_x = win ? 0 : x;
  int base_y = win ? 0 : y;
  int base_w = win ? win->w : n->w;
  int base_h = win ? win->h : n->h;
  if (base_w < 0) base_w = 0;
  if (base_h < 0) base_h = 0;
  if (win) {
    n->x = base_x;
    n->y = base_y;
    n->w = base_w;
    n->h = base_h;
  }
  int inner_x = base_x + n->padding_left;
  int inner_y = base_y + n->padding_top;
  int inner_w = base_w - n->padding_left - n->padding_right;
  int inner_h = base_h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  int gap = n->gap > 0 ? n->gap : 6;
  int cy = inner_y + inner_h;
  for (size_t idx = n->len; idx-- > 0;) {
    CogitoNode* c = n->children[idx];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_w = inner_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;
    if (cw > slot_w) cw = slot_w;
    cy -= c->margin_bottom + ch;
    int cy_child = cy;
    int min_y = inner_y + c->margin_top;
    if (cy_child < min_y) cy_child = min_y;
    int cx = inner_x + c->margin_left + (slot_w - cw) / 2;
    if (cx < inner_x + c->margin_left) cx = inner_x + c->margin_left;
    cogito_layout_node(c, cx, cy_child, cw, ch);
    cy = cy_child - c->margin_top;
    // Add gap only between items, not before the first one (we're iterating backwards)
    if (idx > 0) cy -= gap;
  }
}
