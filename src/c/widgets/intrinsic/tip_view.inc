case COGITO_TIP_VIEW: {
  int pad = 16;
  int title_h = 0;
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    title_h = cogito_text_height() + 8;
  }
  int text_h = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    text_h = cogito_text_height() + 8;
  }
  int content_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    int cw = 0, ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    content_h += ch;
  }
  w = 280;
  h = pad * 2 + title_h + text_h + content_h;
  if (h < 60) h = 60;
  break;
}
