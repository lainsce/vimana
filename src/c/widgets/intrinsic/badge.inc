case COGITO_BADGE: {
  if (n->text && n->text->data && n->text->data[0]) {
    int size = cogito_node_font_size(n);
    int tw = cogito_text_width_size_node(n, n->text->data, size);
    w = tw + 24;
    if (w < 24) w = 24;
    h = 24;
  } else {
    w = 8;
    h = 8;
  }
  break;
}
