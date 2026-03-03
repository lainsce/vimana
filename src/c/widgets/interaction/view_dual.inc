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

static const int COGITO_VIEW_DUAL_INTERACTION_MIN_ITEM_W = 360;
static const int COGITO_VIEW_DUAL_INTERACTION_DETENTE_W_1 = 360;
static const int COGITO_VIEW_DUAL_INTERACTION_DETENTE_W_2 = 412;
static const int COGITO_VIEW_DUAL_INTERACTION_DETENTE_TOLERANCE = 24;

static void cogito_view_dual_set_ratio_target(CogitoNode* n, float r, bool animate) {
  if (!n) return;
  if (r < 0.0f) r = 0.0f;
  if (r > 1.0f) r = 1.0f;
  n->view_dual.ratio = r;
  if (!animate) {
    n->view_dual.anim_ratio = r;
    n->view_dual.anim_from = r;
    n->view_dual.anim_to = r;
    n->view_dual.animating = false;
    return;
  }

  n->view_dual.anim_from = n->view_dual.anim_ratio;
  n->view_dual.anim_to = r;
  n->view_dual.anim_start = cogito_now();
  n->view_dual.animating = (fabsf(n->view_dual.anim_to - n->view_dual.anim_from) > 0.0005f);
  if (!n->view_dual.animating) {
    n->view_dual.anim_ratio = r;
  }
}

static void cogito_view_dual_set_ratio(YisVal vdv, YisVal ratiov) {
  if (vdv.tag != EVT_OBJ) yis_trap("cogito.view_dual_set_ratio expects view_dual");
  CogitoNode* n = (CogitoNode*)vdv.as.p;
  const float edge_ratio_zone = 0.05f;
  int gap = n->gap > 0 ? n->gap : 8;
  int handle_w = 4;
  int split_w = gap * 2 + handle_w;
  const int min_item_w = COGITO_VIEW_DUAL_INTERACTION_MIN_ITEM_W;
  const int detente_w_1 = COGITO_VIEW_DUAL_INTERACTION_DETENTE_W_1;
  const int detente_w_2 = COGITO_VIEW_DUAL_INTERACTION_DETENTE_W_2;
  const int detente_tol = COGITO_VIEW_DUAL_INTERACTION_DETENTE_TOLERANCE;
  float r = (float)yis_as_float(ratiov);
  if (r < 0.0f) r = 0.0f;
  if (r > 1.0f) r = 1.0f;
  if (r <= edge_ratio_zone) r = 0.0f;
  if (r >= (1.0f - edge_ratio_zone)) r = 1.0f;
  if (r <= 0.0f) {
    cogito_view_dual_set_ratio_target(n, 0.0f, true);
    cogito_window_relayout(cogito_node_window(n));
    return;
  }
  if (r >= 1.0f) {
    cogito_view_dual_set_ratio_target(n, 1.0f, true);
    cogito_window_relayout(cogito_node_window(n));
    return;
  }
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

    int min_left = 0;
    int max_left = available_w;
    if (available_w >= min_item_w * 2) {
      min_left = min_item_w;
      max_left = available_w - min_item_w;
    }

    int best_left = left_w;
    int best_dist = detente_tol + 1;

    int d360 = left_w - detente_w_1;
    if (d360 < 0) d360 = -d360;
    if (detente_w_1 >= min_left && detente_w_1 <= max_left && d360 <= detente_tol && d360 < best_dist) {
      best_left = detente_w_1;
      best_dist = d360;
    }

    int d412 = left_w - detente_w_2;
    if (d412 < 0) d412 = -d412;
    if (detente_w_2 >= min_left && detente_w_2 <= max_left && d412 <= detente_tol && d412 < best_dist) {
      best_left = detente_w_2;
      best_dist = d412;
    }

    int center_left = available_w / 2;
    int dcenter = left_w - center_left;
    if (dcenter < 0) dcenter = -dcenter;
    if (center_left >= min_left && center_left <= max_left && dcenter <= detente_tol && dcenter < best_dist) {
      best_left = center_left;
      best_dist = dcenter;
    }

    if (best_dist <= detente_tol) {
      left_w = best_left;
    }

    if (content_w > 0) r = (float)(left_w + split_w / 2) / (float)content_w;
    else r = 0.5f;
  }
  cogito_view_dual_set_ratio_target(n, r, true);
  cogito_window_relayout(cogito_node_window(n));
}
