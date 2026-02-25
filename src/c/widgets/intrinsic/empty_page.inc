case COGITO_EMPTY_PAGE: {
  int icon_h = (n->icon && n->icon->data && n->icon->data[0]) ? 60 : 0;
  int title_h = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    if (ts < 18) ts = 18;
    title_h = cogito_text_height_size(ts) + 12;
  }
  int desc_h = 0;
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    desc_h = cogito_text_height() + 8;
  }
  int action_h = (n->action_text && n->action_text->data && n->action_text->data[0]) ? 44 : 0;
  w = 300;
  h = 40 + icon_h + title_h + desc_h + action_h + 40;
  if (h < 200) h = 200;
  break;
}
