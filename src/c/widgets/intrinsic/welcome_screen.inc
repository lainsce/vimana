case COGITO_WELCOME_SCREEN: {
  int pad_l = n->padding_left > 0 ? n->padding_left : 32;
  int pad_t = n->padding_top > 0 ? n->padding_top : 32;
  int pad_r = n->padding_right > 0 ? n->padding_right : 32;
  int pad_b = n->padding_bottom > 0 ? n->padding_bottom : 32;
  int max_cw = 0;
  int icon_h = (n->icon && n->icon->data && n->icon->data[0]) ? 80 : 0;
  int title_h = (n->text && n->text->data && n->text->data[0]) ? 40 : 0;
  int desc_h = (n->subtitle && n->subtitle->data && n->subtitle->data[0]) ? 24 : 0;
  int action_h = (n->action_text && n->action_text->data && n->action_text->data[0]) ? 48 : 0;
  int content_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    cw += c->margin_left + c->margin_right;
    ch += c->margin_top + c->margin_bottom;
    if (cw > max_cw) max_cw = cw;
    content_h += ch + 8;
  }
  w = (max_cw > 0 ? max_cw : 480) + pad_l + pad_r;
  h = pad_t + icon_h + title_h + desc_h + content_h + action_h + pad_b;
  if (h < 300) h = 300;
  break;
}
