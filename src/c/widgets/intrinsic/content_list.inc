case COGITO_CONTENT_LIST: {
  if (n->len == 0) {
    w = 280;
    h = 48;
    break;
  }
  int gap = n->gap;
  if (gap < 0) gap = 0;
  int max_cw = 0;
  int total_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    int cw = 0, ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    if (ch < 42) ch = 42;
    int tw = cw + n->children[i]->margin_left + n->children[i]->margin_right;
    int th = ch + n->children[i]->margin_top + n->children[i]->margin_bottom;
    if (tw > max_cw) max_cw = tw;
    total_h += th;
    if (i + 1 < n->len) total_h += gap;
  }
  if (max_cw < 200) max_cw = 200;
  w = max_cw;
  h = total_h;
  break;
}
