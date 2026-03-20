static void cogito_layout_flow(CogitoNode* n, int x, int y) {
  int hgap = n->gap;
  int vgap = n->gap;
  int avail_w = n->w - n->padding_left - n->padding_right;
  int avail_h = n->h - n->padding_top - n->padding_bottom;
  if (avail_w < 0) avail_w = 0;
  if (avail_h < 0) avail_h = 0;
  size_t count = n->len;
  if (count == 0) return;

  bool rtl = cogito_is_rtl();

  // Measure all children
  int intrinsic_w_stack[64];
  int intrinsic_h_stack[64];
  int* intrinsic_w = intrinsic_w_stack;
  int* intrinsic_h = intrinsic_h_stack;
  void* heap_block = NULL;
  if (count > 64) {
    heap_block = calloc(count, sizeof(int) * 2);
    if (!heap_block) return;
    intrinsic_w = (int*)heap_block;
    intrinsic_h = intrinsic_w + count;
  }
  for (size_t i = 0; i < count; i++) {
    int cw = 0, ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    if (cw < 0) cw = 0;
    if (ch < 0) ch = 0;
    intrinsic_w[i] = cw;
    intrinsic_h[i] = ch;
  }

  // Wrap children into rows
  int cy = y + n->padding_top;
  size_t row_start = 0;
  while (row_start < count) {
    // Determine how many children fit on this row
    int row_w = 0;
    int row_h = 0;
    size_t row_end = row_start;
    for (size_t i = row_start; i < count; i++) {
      CogitoNode* c = n->children[i];
      int cw = intrinsic_w[i] + c->margin_left + c->margin_right;
      int ch = intrinsic_h[i] + c->margin_top + c->margin_bottom;
      int needed = cw;
      if (i > row_start) needed += hgap;
      if (row_w + needed > avail_w && i > row_start) break;
      row_w += needed;
      if (ch > row_h) row_h = ch;
      row_end = i + 1;
    }

    // Position children in this row
    int cx;
    if (rtl) {
      cx = x + n->w - n->padding_right;
    } else {
      cx = x + n->padding_left;
    }
    for (size_t i = row_start; i < row_end; i++) {
      CogitoNode* c = n->children[i];
      int cw = intrinsic_w[i];
      int ch = intrinsic_h[i];

      // Vertical alignment within row
      int child_row = c->align / 3;
      if (child_row < 0 || child_row > 2) child_row = 0;
      int slot_h = row_h - c->margin_top - c->margin_bottom;
      if (slot_h < 0) slot_h = 0;
      if (child_row == 0) ch = slot_h;
      if (ch > slot_h) ch = slot_h;

      int oy = 0;
      if (child_row == 1) {
        oy = (slot_h - ch) / 2;
      } else if (child_row == 2) {
        oy = slot_h - ch;
      }
      if (oy < 0) oy = 0;

      if (rtl) {
        int child_x = cx - cw - c->margin_right;
        cogito_layout_node(c, child_x, cy + c->margin_top + oy, cw, ch);
        cx -= cw + c->margin_left + c->margin_right;
        if (i + 1 < row_end) cx -= hgap;
      } else {
        cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top + oy, cw, ch);
        cx += cw + c->margin_left + c->margin_right;
        if (i + 1 < row_end) cx += hgap;
      }
    }

    cy += row_h;
    if (row_end < count) cy += vgap;
    row_start = row_end;
  }

  if (heap_block) free(heap_block);
}
