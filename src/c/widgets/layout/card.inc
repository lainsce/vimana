case COGITO_CARD: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  bool has_header_image = n->kd->card.header_image != NULL;
  int header_content_gap = has_header_image ? 24 : 0;
  int default_pad = has_header_image ? 0 : 24;
  int pad_l = s.has_padding_left ? s.padding_left : default_pad;
  int pad_t = s.has_padding_top ? s.padding_top : default_pad;
  int pad_r = s.has_padding_right ? s.padding_right : pad_l;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
  int gap = n->gap;
  int overflow_reserve = 0;
  if (n->kd->card.overflow_btn) {
    int ow = 0, oh = 0;
    cogito_intrinsic_size(n->kd->card.overflow_btn, &ow, &oh);
    overflow_reserve = ow + 8;
  }
  int avail_w = n->w - pad_l - pad_r;
  if (avail_w < 0) avail_w = 0;
  int avail_w_title = avail_w - overflow_reserve;
  if (avail_w_title < 0) avail_w_title = 0;

  int cy = y;

  // Header image: spans full card width at top
  if (n->kd->card.header_image) {
    cogito_layout_node(n->kd->card.header_image, x, cy, n->w, 160);
    cy += 160;
  }

  cy += has_header_image ? header_content_gap : pad_t;

  // Overflow button: upper-right of card, 24dp from edges
  int overflow_bottom = 0;
  if (n->kd->card.overflow_btn) {
    int bw = 0, bh = 0;
    cogito_intrinsic_size(n->kd->card.overflow_btn, &bw, &bh);
    cogito_layout_node(n->kd->card.overflow_btn,
      x + n->w - 24 - bw,
      y + 24,
      bw, bh);
    overflow_bottom = y + 24 + bh;
  }

  // Reserve space for title (narrower when overflow btn present)
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_label_class_font_size(COGITO_LABEL_CLASS_CONTENT_TITLE, 16);
    cy += cogito_text_height_size(ts) + gap;
  }

  // Reserve space for subtitle
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    int ss = cogito_label_class_font_size(COGITO_LABEL_CLASS_CONTENT_SUBTITLE, 14);
    cy += cogito_text_height_size(ss) + gap;
  }

  // Precompute action row size so content layout can stop above it.
  bool has_actions = false;
  int btn_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if (c == n->kd->card.overflow_btn) continue;
    bool is_action = false;
    for (size_t j = 0; j < n->kd->card.action_len; j++) {
      if (n->kd->card.actions[j] == c) { is_action = true; break; }
    }
    if (!is_action && c->view_id && c->view_id->data && strcmp(c->view_id->data, "card-action") == 0) {
      is_action = true;
    }
    if (!is_action) continue;
    has_actions = true;
    int aw = 0, ah = 0;
    cogito_intrinsic_size(c, &aw, &ah);
    if (ah > btn_h) btn_h = ah;
  }
  int action_row_h = has_actions ? (btn_h + gap) : 0;
  int content_bottom = y + n->h - pad_b - action_row_h;

  // Content children (skip overflow_btn and action buttons)
  // --- Pass 1: collect content children, intrinsic sizes, expand info ---
  size_t cc_idx[64];
  int cc_iw[64], cc_ih[64];
  bool cc_vexp[64];
  size_t cc_count = 0;
  int total_fixed_h = 0;
  int expand_count = 0;
  for (size_t i = 0; i < n->len && cc_count < 64; i++) {
    CogitoNode* c = n->children[i];
    if (c == n->kd->card.overflow_btn) continue;
    bool is_action = false;
    for (size_t j = 0; j < n->kd->card.action_len; j++) {
      if (n->kd->card.actions[j] == c) { is_action = true; break; }
    }
    if (!is_action && c->view_id && c->view_id->data && strcmp(c->view_id->data, "card-action") == 0) {
      is_action = true;
    }
    if (is_action) continue;
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    bool vexp = cogito_layout_effective_vexpand(c);
    cc_idx[cc_count] = i;
    cc_iw[cc_count] = cw;
    cc_ih[cc_count] = ch;
    cc_vexp[cc_count] = vexp;
    total_fixed_h += ch + c->margin_top + c->margin_bottom;
    if (vexp) expand_count++;
    cc_count++;
  }
  if (cc_count > 1) total_fixed_h += gap * (int)(cc_count - 1);
  int content_avail_h = content_bottom - cy;
  if (content_avail_h < 0) content_avail_h = 0;
  int leftover = content_avail_h - total_fixed_h;
  if (leftover < 0) leftover = 0;
  int expand_share = expand_count > 0 ? leftover / expand_count : 0;

  // --- Pass 2: layout with alignment and expand ---
  for (size_t ci = 0; ci < cc_count; ci++) {
    CogitoNode* c = n->children[cc_idx[ci]];
    int cw = cc_iw[ci];
    int ch = cc_ih[ci];

    // Use narrower width when child overlaps overflow button vertically
    int eff_avail_w = (overflow_bottom > 0 && (cy + c->margin_top) < overflow_bottom) ? avail_w_title : avail_w;
    int slot_w = eff_avail_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;

    int child_col = c->align % 3;
    if (child_col < 0 || child_col > 2) child_col = 0;
    int child_row = c->align / 3;
    if (child_row < 0 || child_row > 2) child_row = 0;

    bool child_hexpand = cogito_layout_effective_hexpand(c);
    if (child_hexpand && child_col == 0) cw = slot_w;
    if (cw > slot_w) cw = slot_w;

    if (ci > 0) cy += gap;

    // Clamp to remaining space
    int remaining_h = content_bottom - cy - c->margin_top - c->margin_bottom;
    if (remaining_h <= 0) break;

    int cell_h = ch;
    if (cc_vexp[ci]) cell_h = ch + expand_share;
    if (cc_count == 1 && cc_vexp[ci]) cell_h = content_avail_h;
    if (cell_h > remaining_h) cell_h = remaining_h;
    if (cell_h < 0) cell_h = 0;

    int final_ch = ch;
    if (cc_vexp[ci] && child_row == 0) final_ch = cell_h;
    if (final_ch > cell_h) final_ch = cell_h;

    int ox = 0;
    if (child_col == 1) ox = (slot_w - cw) / 2;
    else if (child_col == 2) ox = slot_w - cw;
    if (ox < 0) ox = 0;

    int oy = 0;
    if (child_row == 1) oy = (cell_h - final_ch) / 2;
    else if (child_row == 2) oy = cell_h - final_ch;
    if (oy < 0) oy = 0;

    cogito_layout_node(c, x + pad_l + ox + c->margin_left, cy + oy + c->margin_top, cw, final_ch);
    cy += cell_h + c->margin_top + c->margin_bottom;
  }

  // Action buttons: right-aligned at card bottom
  if (has_actions) {
    int bottom_y = y + n->h - pad_b - btn_h;
    int bx = x + n->w - 8;
    for (int i = (int)n->len - 1; i >= 0; i--) {
      CogitoNode* c = n->children[i];
      if (c == n->kd->card.overflow_btn) continue;
      bool is_action = false;
      for (size_t j = 0; j < n->kd->card.action_len; j++) {
        if (n->kd->card.actions[j] == c) { is_action = true; break; }
      }
      if (!is_action && c->view_id && c->view_id->data && strcmp(c->view_id->data, "card-action") == 0) {
        is_action = true;
      }
      if (!is_action) continue;
      int aw = 0, ah = 0;
      cogito_intrinsic_size(c, &aw, &ah);
      bx -= aw + 8;
      cogito_layout_node(c, bx, bottom_y, aw, btn_h);
    }
  }
  break;
}

