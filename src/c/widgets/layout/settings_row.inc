case COGITO_SETTINGS_ROW: {
  // Place child widget on the right side
  if (n->len > 0) {
    CogitoNode* c = n->children[0];
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cx = x + n->w - cw - 16;
    int cy_child = y + (n->h - ch) / 2;
    cogito_layout_node(c, cx, cy_child, cw, ch);
  }
  break;
}
