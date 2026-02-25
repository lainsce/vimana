case COGITO_BOTTOM_SHEET: {
  int handle_h = 28; // drag handle area
  int gap = n->gap > 0 ? n->gap : 8;
  int title_h = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    title_h = cogito_text_height_size(ts) + gap;
  }
  int content_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    int cw = 0, ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    content_h += ch + n->children[i]->margin_top + n->children[i]->margin_bottom;
    if (i > 0) content_h += gap;
  }
  w = 324;
  // Generic intrinsic sizing adds node padding after this switch.
  h = handle_h + title_h + content_h;
  if (h < 100) h = 100;
  break;
}
