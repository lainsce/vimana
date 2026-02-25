case COGITO_SETTINGS_LIST: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_t = s.has_padding_top ? s.padding_top : 8;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : 8;
  int title_h = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    title_h = cogito_text_height() + 12;
  }
  int content_h = 0;
  int max_cw = 0;
  for (size_t i = 0; i < n->len; i++) {
    int cw = 0, ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    if (cw > max_cw) max_cw = cw;
    content_h += ch;
  }
  w = max_cw > 0 ? max_cw : 400;
  h = pad_t + pad_b + title_h + content_h;
  if (h < 48) h = 48;
  break;
}
