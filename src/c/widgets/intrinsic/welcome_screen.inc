case COGITO_WELCOME_SCREEN: {
  w = 480;
  int icon_h = (n->icon && n->icon->data && n->icon->data[0]) ? 80 : 0;
  int title_h = (n->text && n->text->data && n->text->data[0]) ? 40 : 0;
  int desc_h = (n->subtitle && n->subtitle->data && n->subtitle->data[0]) ? 24 : 0;
  int action_h = (n->action_text && n->action_text->data && n->action_text->data[0]) ? 48 : 0;
  int content_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    int cw = 0, ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    content_h += ch + 8;
  }
  h = 40 + icon_h + title_h + desc_h + content_h + action_h + 40;
  if (h < 300) h = 300;
  break;
}
