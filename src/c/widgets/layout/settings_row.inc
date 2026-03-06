case COGITO_SETTINGS_ROW: {
  // Place child widget on trailing side (right in LTR, left in RTL)
  if (n->len > 0) {
    CogitoNode* c = n->children[0];
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cx = cogito_is_rtl() ? (x + 16) : (x + n->w - cw - 16);
    int cy_child = y + (n->h - ch) / 2;
    cogito_layout_node(c, cx, cy_child, cw, ch);
  }
  break;
}
