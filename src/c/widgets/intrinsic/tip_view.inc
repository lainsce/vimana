case COGITO_TIP_VIEW: {
  int pad_l = n->padding_left > 0 ? n->padding_left : 16;
  int pad_t = n->padding_top > 0 ? n->padding_top : 16;
  int pad_r = n->padding_right > 0 ? n->padding_right : 16;
  int pad_b = n->padding_bottom > 0 ? n->padding_bottom : 16;
  int title_h = 0;
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    title_h = cogito_text_height() + 8;
  }
  int text_h = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    text_h = cogito_text_height() + 8;
  }
  int content_h = 0;
  int max_cw = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    cw += c->margin_left + c->margin_right;
    ch += c->margin_top + c->margin_bottom;
    if (cw > max_cw) max_cw = cw;
    content_h += ch;
  }
  w = (max_cw > 0 ? max_cw : 280) + pad_l + pad_r;
  h = pad_t + pad_b + title_h + text_h + content_h;
  if (h < 60) h = 60;
  break;
}
