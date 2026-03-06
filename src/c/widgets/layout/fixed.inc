static void cogito_layout_fixed(CogitoNode* n, int x, int y) {
  int base_x = x + n->padding_left;
  int base_y = y + n->padding_top;
  bool rtl = cogito_is_rtl();
  int inner_w = n->w - n->padding_left - n->padding_right;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cx;
    if (rtl && c->fixed_pos_set) {
      cx = base_x + inner_w - c->fixed_x - cw;
    } else {
      cx = base_x + (c->fixed_pos_set ? c->fixed_x : 0);
    }
    int cy = base_y + (c->fixed_pos_set ? c->fixed_y : 0);
    cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
  }
}
