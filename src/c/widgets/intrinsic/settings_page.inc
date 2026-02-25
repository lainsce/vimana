case COGITO_SETTINGS_PAGE: {
  // VStack-like
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_t = s.has_padding_top ? s.padding_top : 16;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : 16;
  int gap = n->gap > 0 ? n->gap : 12;
  int max_cw = 0;
  int total_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    int cw = 0, ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    if (cw > max_cw) max_cw = cw;
    total_h += ch;
    if (i > 0) total_h += gap;
  }
  w = max_cw > 0 ? max_cw : 400;
  h = pad_t + total_h + pad_b;
  break;
}
