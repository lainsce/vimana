static CogitoNode* cogito_view_dual_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_VIEW_DUAL);
  n->hexpand = true;
  n->vexpand = true;
  return n;
}
static YisVal cogito_view_dual_new(void) {
  CogitoNode* n = cogito_view_dual_new_obj();
  return YV_OBJ(n);
}
static void cogito_view_dual_set_ratio(YisVal vdv, YisVal ratiov) {
  if (vdv.tag != EVT_OBJ) yis_trap("cogito.view_dual_set_ratio expects view_dual");
  CogitoNode* n = (CogitoNode*)vdv.as.p;
  int gap = n->gap > 0 ? n->gap : 8;
  int handle_w = 4;
  int split_w = gap * 2 + handle_w;
  const int min_item_w = 218;
  float r = (float)yis_as_float(ratiov);
  if (r < 0.0f) r = 0.0f;
  if (r > 1.0f) r = 1.0f;
  if (n->w > 0) {
    int content_w = n->w - n->padding_left - n->padding_right;
    if (content_w < 0) content_w = 0;
    int available_w = content_w - split_w;
    if (available_w < 0) available_w = 0;
    int left_w = (int)(content_w * r) - split_w / 2;
    if (available_w >= min_item_w * 2) {
      int max_left = available_w - min_item_w;
      if (left_w < min_item_w) left_w = min_item_w;
      if (left_w > max_left) left_w = max_left;
    } else {
      left_w = available_w / 2;
    }
    if (left_w < 0) left_w = 0;
    if (left_w > available_w) left_w = available_w;
    if (content_w > 0) r = (float)(left_w + split_w / 2) / (float)content_w;
    else r = 0.5f;
  }
  n->view_dual.ratio = r;
  cogito_window_relayout(cogito_node_window(n));
}
