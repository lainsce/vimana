case COGITO_CARD: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = s.has_padding_left ? s.padding_left : 16;
  int pad_t = s.has_padding_top ? s.padding_top : 16;
  int pad_r = s.has_padding_right ? s.padding_right : pad_l;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
  int gap = n->gap;

  int img_h = n->card.header_image ? 160 : 0;

  int title_h = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    title_h = cogito_text_height_size(ts) + gap;
  }

  int subtitle_h = 0;
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    subtitle_h = cogito_text_height_size(14) + gap;
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
    if (is_action) continue;
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (cw > max_cw) max_cw = cw;
    content_h += ch + c->margin_top + c->margin_bottom;
    if (content_count > 0) content_h += gap;
    content_count++;
  }

  int action_h = 0;
  if (n->card.action_len > 0) {
    int row_h = 0;
    for (size_t j = 0; j < n->card.action_len; j++) {
      int aw = 0, ah = 0;
      cogito_intrinsic_size(n->card.actions[j], &aw, &ah);
      if (ah > row_h) row_h = ah;
    }
    action_h = row_h + gap;
  }

  w = pad_l + pad_r + max_cw;
  h = img_h + pad_t + title_h + subtitle_h + content_h + action_h + pad_b;
  if (w < 200) w = 200;
  if (h < 60) h = 60;
  break;
}

