case COGITO_SWITCHBAR: {
  int bar_h = 48;
  int max_cw = 320;
  int content_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    cw += c->margin_left + c->margin_right;
    ch += c->margin_top + c->margin_bottom;
    if (cw > max_cw) max_cw = cw;
    content_h += ch;
  }
  w = max_cw + n->padding_left + n->padding_right;
  h = bar_h + content_h + n->padding_top + n->padding_bottom;
  if (h < 48 + n->padding_top + n->padding_bottom) h = 48 + n->padding_top + n->padding_bottom;
  break;
}
