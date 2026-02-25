case COGITO_VIEW_DUAL: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
  CogitoColor text_col = s.has_text ? s.text : n->text_color;
  
  float ratio = n->view_dual.ratio;
  if (ratio <= 0.0f) ratio = 0.5f;
  if (ratio > 1.0f) ratio = 1.0f;
  
  // Calculate handle position
  int handle_w = 4;
  const int min_item_w = 218;
  int available_w = n->w - handle_w;
  if (available_w < 0) available_w = 0;
  int left_w = (int)(n->w * ratio) - handle_w / 2;
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
  if (handle_h > n->h) handle_h = n->h;
  if (handle_h < 1) handle_h = 1;
  int handle_x = n->x + left_w;
  int handle_y = n->y + (n->h - handle_h) / 2;

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
