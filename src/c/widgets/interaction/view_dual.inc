static CogitoNode* cogito_view_dual_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_VIEW_DUAL);
  n->hexpand = true;
  n->vexpand = true;
  return n;
}
static ErgoVal cogito_view_dual_new(void) {
  CogitoNode* n = cogito_view_dual_new_obj();
  return EV_OBJ(n);
}
static void cogito_view_dual_set_ratio(ErgoVal vdv, ErgoVal ratiov) {
  if (vdv.tag != EVT_OBJ) ergo_trap("cogito.view_dual_set_ratio expects view_dual");
  CogitoNode* n = (CogitoNode*)vdv.as.p;
  int handle_w = 4;
  const int min_item_w = 218;
  float r = (float)ergo_as_float(ratiov);
  if (r < 0.0f) r = 0.0f;
  if (r > 1.0f) r = 1.0f;
  if (n->w > 0) {
    int available_w = n->w - handle_w;
    if (available_w < 0) available_w = 0;
    int left_w = (int)(n->w * r) - handle_w / 2;
    if (available_w >= min_item_w * 2) {
      int max_left = available_w - min_item_w;
      if (left_w < min_item_w) left_w = min_item_w;
      if (left_w > max_left) left_w = max_left;
    } else {
      left_w = available_w / 2;
    }
    if (left_w < 0) left_w = 0;
    if (left_w > available_w) left_w = available_w;
    r = (float)(left_w + handle_w / 2) / (float)n->w;
  }
  n->view_dual.ratio = r;
  cogito_window_relayout(cogito_node_window(n));
}
