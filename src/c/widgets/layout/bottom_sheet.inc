case COGITO_BOTTOM_SHEET: {
  // BottomSheet uses VStack-like layout below the drag handle
  int pad_l = n->padding_left;
  int pad_t = n->padding_top;
  int pad_r = n->padding_right;
  int pad_b = n->padding_bottom;
  int handle_h = 28;
  int gap = n->gap > 0 ? n->gap : 8;
  int avail_w = n->w - pad_l - pad_r;
  if (avail_w < 0) avail_w = 0;

  int action_h = 0;
  int action_count = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    bool is_action = c->view_id && c->view_id->data && strcmp(c->view_id->data, "bottom-sheet-action") == 0;
    if (!is_action) continue;
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    action_h += ch + c->margin_top + c->margin_bottom;
    if (action_count > 0) action_h += gap;
    action_count++;
  }

  int cy = y + handle_h + pad_t;
  // Title
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    int th = cogito_text_height_size(ts);
    cy += th + gap;
  }

  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    bool is_action = c->view_id && c->view_id->data && strcmp(c->view_id->data, "bottom-sheet-action") == 0;
    if (is_action) continue;
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_w = avail_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;
    if (c->hexpand) cw = slot_w;
    else if (cw > slot_w) cw = slot_w;

    int top_y = cy + c->margin_top;
    int item_h = ch + c->margin_top + c->margin_bottom;

    cogito_layout_node(c, x + pad_l + c->margin_left, top_y, cw, ch);
    cy += item_h + gap;
  }

  if (action_count > 0) {
    int ay = y + n->h - pad_b - action_h;
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* c = n->children[i];
      bool is_action = c->view_id && c->view_id->data && strcmp(c->view_id->data, "bottom-sheet-action") == 0;
      if (!is_action) continue;
      int cw = 0, ch = 0;
      cogito_intrinsic_size(c, &cw, &ch);
      int slot_w = avail_w - c->margin_left - c->margin_right;
      if (slot_w < 0) slot_w = 0;
      // BottomSheet actions are always full-width blocks.
      cogito_layout_node(c,
        x + pad_l + c->margin_left,
        ay + c->margin_top,
        slot_w,
        ch);
      ay += ch + c->margin_top + c->margin_bottom + gap;
    }
  }
  break;
}
