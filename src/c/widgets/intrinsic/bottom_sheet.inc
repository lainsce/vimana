case COGITO_BOTTOM_SHEET: {
  int handle_h = 28; // drag handle area
  int gap = n->gap > 0 ? n->gap : 8;
  int title_h = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    title_h = cogito_text_height_size(ts) + gap;
  }
  int content_h = 0;
  int action_h = 0;
  int action_count = 0;
  int content_count = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    bool is_action = c->view_id && c->view_id->data && strcmp(c->view_id->data, "bottom-sheet-action") == 0;
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (is_action) {
      action_h += ch + c->margin_top + c->margin_bottom;
      if (action_count > 0) action_h += gap;
      action_count++;
    } else {
      content_h += ch + c->margin_top + c->margin_bottom;
      if (content_count > 0) content_h += gap;
      content_count++;
    }
  }
  w = 300;
  // Generic intrinsic sizing adds node padding after this switch.
  h = handle_h + title_h + content_h;
  if (action_count > 0) {
    h += gap + action_h;
  }
  if (h < 100) h = 100;
  break;
}
