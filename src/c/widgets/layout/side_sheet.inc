case COGITO_SIDE_SHEET: {
  int r_x = x, r_y = y, r_w = n->w;
  if (n->side_sheet.mode == COGITO_SIDE_SHEET_INSET) {
      r_x += 16;
      r_y += 16;
      r_w -= 32;
  }
  // SideSheet uses VStack-like layout: header row (title + close), then children below.
  int pad_l = n->padding_left;
  int pad_t = n->padding_top;
  int pad_r = n->padding_right;
  int pad_b = n->padding_bottom;
  int handle_w = (n->side_sheet.mode == COGITO_SIDE_SHEET_SIDEBAR)
    ? 0
    : COGITO_SIDE_SHEET_HANDLE_TOUCH_W;
  int gap = n->gap > 0 ? n->gap : 8;
  int content_x = r_x + handle_w + pad_l;
  int avail_w = r_w - handle_w - pad_l - pad_r;
  if (avail_w < 0) avail_w = 0;

  // Reserve header row for the title / close button area
  int header_h = 0;
  if (n->text && n->text->data && n->text->data[
    0
  ]) {
    int ts = cogito_node_font_size(n);
    int th = cogito_text_height_size(ts);
    header_h = th + 24; // title text height + vertical padding within header
  } else {
    header_h = 24 + 24; // close button pad_t + box height
  }

  int inner_h = n->h - pad_t - pad_b;
  if (inner_h < 0) inner_h = 0;
  int cy = r_y + pad_t + header_h + gap;

  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i
    ];
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_w = avail_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;
    if (c->hexpand) cw = slot_w;
    else if (cw > slot_w) cw = slot_w;
    int remaining_h = r_y + pad_t + inner_h - (cy + c->margin_top + c->margin_bottom);
    if (remaining_h < 0) remaining_h = 0;
    if (c->vexpand) ch = remaining_h;
    cogito_layout_node(c, content_x + c->margin_left, cy + c->margin_top, cw, ch);
    cy += ch + c->margin_top + c->margin_bottom;
    if (i < n->len - 1) cy += gap;
  }
  break;
}