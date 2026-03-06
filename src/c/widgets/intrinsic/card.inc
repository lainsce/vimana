case COGITO_CARD: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  bool has_header_image = n->card.header_image != NULL;
  int header_content_gap = has_header_image ? 24 : 0;
  int default_pad = has_header_image ? 0 : 16;
  int pad_l = s.has_padding_left ? s.padding_left : default_pad;
  int pad_t = s.has_padding_top ? s.padding_top : default_pad;
  int pad_r = s.has_padding_right ? s.padding_right : pad_l;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
  int gap = n->gap;

  int img_h = n->card.header_image ? 160 : 0;

  int title_h = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_label_class_font_size(COGITO_LABEL_CLASS_CONTENT_TITLE, 16);
    title_h = cogito_text_height_size(ts) + gap;
  }

  int subtitle_h = 0;
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    int ss = cogito_label_class_font_size(COGITO_LABEL_CLASS_CONTENT_SUBTITLE, 14);
    subtitle_h = cogito_text_height_size(ss) + gap;
  }

  int content_h = 0;
  int max_cw = 0;
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
    if (cw > max_cw) max_cw = cw;
    content_h += ch + c->margin_top + c->margin_bottom;
    if (content_count > 0) content_h += gap;
    content_count++;
  }

  if (content_count > 0) {
    int body_size = cogito_label_class_font_size(COGITO_LABEL_CLASS_CONTENT_SUBTITLE, 14);
    int body_line_h = cogito_text_height_size(body_size);
    int max_body_h = body_line_h * 5;
    if (content_h > max_body_h) content_h = max_body_h;
  }

  int action_h = 0;
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
    int row_h = 0;
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
      if (ah > row_h) row_h = ah;
    }
    action_h = row_h + gap;
  }

  w = pad_l + pad_r + max_cw;
  h = img_h + (has_header_image ? header_content_gap : pad_t) + title_h + subtitle_h + content_h + action_h + pad_b;
  if (w < 200) w = 200;
  if (h < 60) h = 60;
  break;
}

