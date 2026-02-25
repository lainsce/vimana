case COGITO_CONTENT_LIST: {
  // VStack-like sizing
  int gap = n->gap;
  if (gap < 0) gap = 0;
  int max_cw = 0;
  int total_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    int cw = 0, ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    if (cw > max_cw) max_cw = cw;
    total_h += ch + n->children[i]->margin_top + n->children[i]->margin_bottom;
    if (i + 1 < n->len) total_h += gap;
  }
  w = max_cw > 0 ? max_cw : 280;
  h = total_h > 0 ? total_h : 48;
  break;
}
