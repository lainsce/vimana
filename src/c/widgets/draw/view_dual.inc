case COGITO_VIEW_DUAL: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
  CogitoColor text_col = s.has_text ? s.text : n->text_color;
  
  float ratio = n->view_dual.ratio;
  if (ratio <= 0.0f) ratio = 0.5f;
  if (ratio > 1.0f) ratio = 1.0f;

  int content_x = n->x + n->padding_left;
  int content_y = n->y + n->padding_top;
  int content_w = n->w - n->padding_left - n->padding_right;
  int content_h = n->h - n->padding_top - n->padding_bottom;
  if (content_w < 0) content_w = 0;
  if (content_h < 0) content_h = 0;
  
  // Calculate handle position
  int gap = n->gap > 0 ? n->gap : 8;
  int handle_w = 4;
  int split_w = gap * 2 + handle_w;
  const int min_item_w = 218;
  int available_w = content_w - split_w;
  if (available_w < 0) available_w = 0;
  int left_w = (int)(content_w * ratio) - split_w / 2;
  if (available_w >= min_item_w * 2) {
    int max_left = available_w - min_item_w;
    if (left_w < min_item_w) left_w = min_item_w;
    if (left_w > max_left) left_w = max_left;
  } else {
    left_w = available_w / 2;
  }
  if (left_w < 0) left_w = 0;
  if (left_w > available_w) left_w = available_w;
  int handle_h = 64;
  if (handle_h > content_h) handle_h = content_h;
  if (handle_h < 1) handle_h = 1;
  int handle_x = content_x + left_w + gap;
  int handle_y = content_y + (content_h - handle_h) / 2;

  // Draw children (panels on either side of handle)
  for (size_t i = 0; i < n->len && i < 2; i++) {
    cogito_draw_node(n->children[i]);
  }

  // Draw resize handle in on-surface color for dark/light contrast.
  CogitoColor handle_color = text_col;
  float alpha_mul = active ? 0.72f : (hover ? 0.58f : 0.46f);
  handle_color.a = (uint8_t)(text_col.a * alpha_mul);
  if (handle_color.a < 72) handle_color.a = 72;
  cogito_draw_rect(handle_x, handle_y, handle_w, handle_h, handle_color, handle_w / 2);
  break;
}
