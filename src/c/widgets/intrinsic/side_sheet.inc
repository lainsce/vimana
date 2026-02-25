case COGITO_SIDE_SHEET: {
  int gap = n->gap > 0 ? n->gap : 8;
  int title_w = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    title_w = cogito_text_width_size_node(n, n->text->data, ts) + gap;
  }
  int content_w = 0;
  for (size_t i = 0; i < n->len; i++) {
    int cw = 0, ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    content_w += cw + n->children[i]->margin_left + n->children[i]->margin_right;
    if (i > 0) content_w += gap;
  }
  h = 324;
  // Generic intrinsic sizing adds node padding after this switch.
  w = title_w + content_w;
  if (w < 200) w = 200;
  if (n->side_sheet.mode == COGITO_SIDE_SHEET_INSET) {
      w += 32;
      h += 32;
  }
  break;
}
