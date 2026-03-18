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

static const float COGITO_VIEW_DUAL_EDGE_RATIO_ZONE = 0.05f;
static const int COGITO_VIEW_DUAL_HANDLE_W = 4;
static const int COGITO_VIEW_DUAL_HANDLE_EDGE_INSET = 4;
static const int COGITO_VIEW_DUAL_HANDLE_INTENT_W = 16;
static const int COGITO_VIEW_DUAL_HIDDEN_GAP_W = 16;
static const int COGITO_VIEW_DUAL_INTERACTION_MIN_ITEM_W = 360;
static const int COGITO_VIEW_DUAL_INTERACTION_DETENTE_W_1 = 360;
static const int COGITO_VIEW_DUAL_INTERACTION_DETENTE_W_2 = 412;
static const int COGITO_VIEW_DUAL_INTERACTION_DETENTE_TOLERANCE = 24;

static float cogito_view_dual_clamp_ratio(float ratio) {
  if (ratio < 0.0f) return 0.0f;
  if (ratio > 1.0f) return 1.0f;
  return ratio;
}

static float cogito_view_dual_current_ratio(const CogitoNode* n) {
  if (!n) return 0.5f;
  float ratio = n->kd->view_dual.animating ? n->kd->view_dual.anim_ratio : n->kd->view_dual.ratio;
  return cogito_view_dual_clamp_ratio(ratio);
}

static void cogito_view_dual_content_bounds(const CogitoNode* n,
                                            int* out_x, int* out_y,
                                            int* out_w, int* out_h) {
  int x = n ? (n->x + n->padding_left) : 0;
  int y = n ? (n->y + n->padding_top) : 0;
  int w = n ? (n->w - n->padding_left - n->padding_right) : 0;
  int h = n ? (n->h - n->padding_top - n->padding_bottom) : 0;
  if (w < 0) w = 0;
  if (h < 0) h = 0;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
}

static int cogito_view_dual_handle_x_for_ratio(const CogitoNode* n, float ratio,
                                                int content_x, int content_w,
                                                int handle_w) {
  int gap = n && n->gap > 0 ? n->gap : 8;
  int split_w = gap * 2 + handle_w;
  int min_item_w = COGITO_VIEW_DUAL_INTERACTION_MIN_ITEM_W;
  int available_w = content_w - split_w;
  if (available_w < 0) available_w = 0;

  float r = cogito_view_dual_clamp_ratio(ratio);
  int handle_x = content_x + COGITO_VIEW_DUAL_HANDLE_EDGE_INSET;
  if (r >= (1.0f - COGITO_VIEW_DUAL_EDGE_RATIO_ZONE)) {
    handle_x = content_x + content_w - handle_w - COGITO_VIEW_DUAL_HANDLE_EDGE_INSET;
  } else if (r > COGITO_VIEW_DUAL_EDGE_RATIO_ZONE) {
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
    handle_x = content_x + left_w + gap;
  }

  int handle_min_x = content_x + COGITO_VIEW_DUAL_HANDLE_EDGE_INSET;
  int handle_max_x = content_x + content_w - COGITO_VIEW_DUAL_HANDLE_EDGE_INSET - handle_w;
  if (handle_x < handle_min_x) handle_x = handle_min_x;
  if (handle_x > handle_max_x) handle_x = handle_max_x;
  return handle_x;
}

static void cogito_view_dual_handle_targets_for_ratio(const CogitoNode* n, float ratio,
                                                       int* out_handle_x, int* out_handle_w,
                                                       int* out_intent_x, int* out_intent_y,
                                                       int* out_intent_w, int* out_intent_h) {
  int content_x = 0, content_y = 0, content_w = 0, content_h = 0;
  cogito_view_dual_content_bounds(n, &content_x, &content_y, &content_w, &content_h);

  int handle_w = COGITO_VIEW_DUAL_HANDLE_W;
  int handle_x = cogito_view_dual_handle_x_for_ratio(n, ratio, content_x, content_w, handle_w);
  int intent_w = COGITO_VIEW_DUAL_HANDLE_INTENT_W;
  if (intent_w < handle_w) intent_w = handle_w;
  int intent_x = handle_x - (intent_w - handle_w) / 2;

  if (out_handle_x) *out_handle_x = handle_x;
  if (out_handle_w) *out_handle_w = handle_w;
  if (out_intent_x) *out_intent_x = intent_x;
  if (out_intent_y) *out_intent_y = content_y;
  if (out_intent_w) *out_intent_w = intent_w;
  if (out_intent_h) *out_intent_h = content_h;
}

static bool cogito_view_dual_hit_handle_intent_for_ratio(const CogitoNode* n,
                                                          float ratio,
                                                          int mx, int my) {
  if (!n || !n->kd->view_dual.show_handle) return false;
  int ix = 0, iy = 0, iw = 0, ih = 0;
  cogito_view_dual_handle_targets_for_ratio(n, ratio, NULL, NULL, &ix, &iy, &iw, &ih);
  return cogito_hit_rect(mx, my, ix, iy, iw, ih);
}

static void cogito_view_dual_set_ratio_target(CogitoNode* n, float r, bool animate) {
  if (!n) return;
  r = cogito_view_dual_clamp_ratio(r);
  n->kd->view_dual.ratio = r;
  if (!animate) {
    n->kd->view_dual.anim_ratio = r;
    n->kd->view_dual.anim_from = r;
    n->kd->view_dual.anim_to = r;
    n->kd->view_dual.animating = false;
    return;
  }

  n->kd->view_dual.anim_from = n->kd->view_dual.anim_ratio;
  n->kd->view_dual.anim_to = r;
  n->kd->view_dual.anim_start = cogito_now();
  n->kd->view_dual.animating = (fabsf(n->kd->view_dual.anim_to - n->kd->view_dual.anim_from) > 0.0005f);
  if (!n->kd->view_dual.animating) {
    n->kd->view_dual.anim_ratio = r;
  }
}

static void cogito_view_dual_set_ratio(YisVal vdv, YisVal ratiov) {
  if (vdv.tag != EVT_OBJ) yis_trap("cogito.view_dual_set_ratio expects view_dual");
  CogitoNode* n = (CogitoNode*)vdv.as.p;
  int gap = n->gap > 0 ? n->gap : 8;
  int handle_w = n->kd->view_dual.show_handle ? COGITO_VIEW_DUAL_HANDLE_W : 0;
  int split_w = n->kd->view_dual.show_handle
    ? (gap * 2 + handle_w)
    : COGITO_VIEW_DUAL_HIDDEN_GAP_W;
  const int min_item_w = COGITO_VIEW_DUAL_INTERACTION_MIN_ITEM_W;
  const int detente_w_1 = COGITO_VIEW_DUAL_INTERACTION_DETENTE_W_1;
  const int detente_w_2 = COGITO_VIEW_DUAL_INTERACTION_DETENTE_W_2;
  const int detente_tol = COGITO_VIEW_DUAL_INTERACTION_DETENTE_TOLERANCE;
  float r = cogito_view_dual_clamp_ratio((float)yis_as_float(ratiov));
  if (r <= COGITO_VIEW_DUAL_EDGE_RATIO_ZONE) r = 0.0f;
  if (r >= (1.0f - COGITO_VIEW_DUAL_EDGE_RATIO_ZONE)) r = 1.0f;
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

static void cogito_view_dual_set_show_handle(YisVal vdv, YisVal showv) {
  if (vdv.tag != EVT_OBJ) yis_trap("cogito.view_dual_set_show_handle expects view_dual");
  CogitoNode* n = (CogitoNode*)vdv.as.p;
  bool show = yis_as_bool(showv);
  n->kd->view_dual.show_handle = show;
  if (!show) {
    n->kd->view_dual.dragging = false;
    n->kd->view_dual.drag_committed = false;
    if (cogito_pointer_capture == n) {
      cogito_pointer_capture = NULL;
    }
  }
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_view_dual_get_show_handle(YisVal vdv) {
  if (vdv.tag != EVT_OBJ) yis_trap("cogito.view_dual_get_show_handle expects view_dual");
  CogitoNode* n = (CogitoNode*)vdv.as.p;
  return YV_BOOL(n->kd->view_dual.show_handle);
}
