case COGITO_CARD: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = s.has_padding_left ? s.padding_left : 16;
  int pad_t = s.has_padding_top ? s.padding_top : 16;
  int pad_r = s.has_padding_right ? s.padding_right : pad_l;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
  int gap = n->gap;
  int avail_w = n->w - pad_l - pad_r;
  if (avail_w < 0) avail_w = 0;

  int cy = y;

  // Header image: spans full card width at top
  if (n->card.header_image) {
    cogito_layout_node(n->card.header_image, x, cy, n->w, 160);
    cy += 160;
  }

  cy += pad_t;

  // Overflow button: upper-right of content area
  if (n->card.overflow_btn) {
    int bw = 0, bh = 0;
    cogito_intrinsic_size(n->card.overflow_btn, &bw, &bh);
    cogito_layout_node(n->card.overflow_btn,
      x + n->w - pad_r - bw,
      cy,
      bw, bh);
  }

  // Reserve space for title
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_label_class_font_size(COGITO_LABEL_CLASS_CONTENT_TITLE, 16);
    cy += cogito_text_height_size(ts) + gap;
  }

  // Reserve space for subtitle
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    int ss = cogito_label_class_font_size(COGITO_LABEL_CLASS_CONTENT_SUBTITLE, 14);
    cy += cogito_text_height_size(ss) + gap;
  }

  // Content children (skip overflow_btn and action buttons)
  int content_count = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if (c == n->card.overflow_btn) continue;
    bool is_action = false;
    for (size_t j = 0; j < n->card.action_len; j++) {
      if (n->card.actions[j] == c) { is_action = true; break; }
    }
    if (!is_action && c->view_id && c->view_id->data && strcmp(c->view_id->data, "card-action") == 0) {
      is_action = true;
    }
    if (is_action) continue;
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_w = avail_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;
    if (c->hexpand) cw = slot_w;
    else if (cw > slot_w) cw = slot_w;
    if (content_count > 0) cy += gap;
    cogito_layout_node(c, x + pad_l + c->margin_left, cy + c->margin_top, cw, ch);
    cy += ch + c->margin_top + c->margin_bottom;
    content_count++;
  }

  // Action buttons: right-aligned at card bottom
  bool has_actions = false;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if (c == n->card.overflow_btn) continue;
    bool is_action = false;
    for (size_t j = 0; j < n->card.action_len; j++) {
      if (n->card.actions[j] == c) { is_action = true; break; }
    }
    if (!is_action && c->view_id && c->view_id->data && strcmp(c->view_id->data, "card-action") == 0) {
      is_action = true;
    }
    if (is_action) {
      has_actions = true;
      break;
    }
  }

  if (has_actions) {
    int btn_h = 0;
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* c = n->children[i];
      if (c == n->card.overflow_btn) continue;
      bool is_action = false;
      for (size_t j = 0; j < n->card.action_len; j++) {
        if (n->card.actions[j] == c) { is_action = true; break; }
      }
      if (!is_action && c->view_id && c->view_id->data && strcmp(c->view_id->data, "card-action") == 0) {
        is_action = true;
      }
      if (!is_action) continue;
      int aw = 0, ah = 0;
      cogito_intrinsic_size(c, &aw, &ah);
      if (ah > btn_h) btn_h = ah;
    }
    int bottom_y = y + n->h - pad_b - btn_h;
    int bx = x + n->w - 8;
    for (int i = (int)n->len - 1; i >= 0; i--) {
      CogitoNode* c = n->children[i];
      if (c == n->card.overflow_btn) continue;
      bool is_action = false;
      for (size_t j = 0; j < n->card.action_len; j++) {
        if (n->card.actions[j] == c) { is_action = true; break; }
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

