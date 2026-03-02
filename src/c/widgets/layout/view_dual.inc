case COGITO_VIEW_DUAL: {
  float ratio = n->view_dual.ratio;
  if (ratio <= 0.0f) ratio = 0.5f;
  if (ratio > 1.0f) ratio = 1.0f;
  int gap = n->gap > 0 ? n->gap : 8;
  int handle_w = 4;
  int split_w = gap * 2 + handle_w;
  const int min_item_w = 218;

  int content_x = x + n->padding_left;
  int content_y = y + n->padding_top;
  int content_w = n->w - n->padding_left - n->padding_right;
  int content_h = n->h - n->padding_top - n->padding_bottom;
  if (content_w < 0) content_w = 0;
  if (content_h < 0) content_h = 0;

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
  int right_x = content_x + left_w + split_w;
  int right_w = content_w - left_w - split_w;
  if (right_w < 0) right_w = 0;

  if (n->len > 0) {
    CogitoNode* left = n->children[0];
    int lw = 0, lh = 0;
    cogito_intrinsic_size(left, &lw, &lh);
    cogito_layout_node(left, content_x, content_y, left_w, content_h);
  }
  if (n->len > 1) {
    CogitoNode* right = n->children[1];
    int rw = 0, rh = 0;
    cogito_intrinsic_size(right, &rw, &rh);
    cogito_layout_node(right, right_x, content_y, right_w, content_h);
  }
  break;
}
