case COGITO_ABOUT_WINDOW: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = 24;
  int pad_t = 24;
  int pad_r = 24;
  int pad_b = 24;
  if (n->padding_set) {
    pad_l = n->padding_left;
    pad_t = n->padding_top;
    pad_r = n->padding_right;
    pad_b = n->padding_bottom;
  } else {
    if (s.has_padding) {
      pad_l = s.padding_left;
      pad_t = s.padding_top;
      pad_r = s.padding_right;
      pad_b = s.padding_bottom;
    }
    if (s.has_padding_left) pad_l = s.padding_left;
    if (s.has_padding_top) pad_t = s.padding_top;
    if (s.has_padding_right) pad_r = s.padding_right;
    if (s.has_padding_bottom) pad_b = s.padding_bottom;
  }
  int old_pad_l = n->padding_left;
  int old_pad_t = n->padding_top;
  int old_pad_r = n->padding_right;
  int old_pad_b = n->padding_bottom;
  n->padding_left = pad_l;
  n->padding_top = pad_t;
  n->padding_right = pad_r;
  n->padding_bottom = pad_b;
  cogito_layout_vstack(n, x, y);
  n->padding_left = old_pad_l;
  n->padding_top = old_pad_t;
  n->padding_right = old_pad_r;
  n->padding_bottom = old_pad_b;

  n->action_button_x = 0;
  n->action_button_y = 0;
  n->action_button_w = 0;
  n->action_button_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* row = n->children[i];
    if (!row || row->kind != COGITO_HSTACK) continue;
    if (!row->class_name || !row->class_name->data ||
        strcmp(row->class_name->data, "about-window-actions") != 0) continue;
    for (size_t j = 0; j < row->len; j++) {
      CogitoNode* btn = row->children[j];
      if (!btn || btn->kind != COGITO_BUTTON) continue;
      n->action_button_x = btn->x;
      n->action_button_y = btn->y;
      n->action_button_w = btn->w;
      n->action_button_h = btn->h;
      break;
    }
    if (n->action_button_w > 0) break;
  }
  break;
}
