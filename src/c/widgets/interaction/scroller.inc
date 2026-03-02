static CogitoNode* cogito_scroller_new_obj(void) {
  return cogito_node_new(COGITO_SCROLLER);
}
static YisVal cogito_scroller_new(void) {
  return YV_OBJ(cogito_scroller_new_obj());
}
static void cogito_scroller_set_axes(YisVal scv, YisVal horzv, YisVal vertv) {
  if (scv.tag != EVT_OBJ) yis_trap("cogito.scroller_set_axes expects scroller");
  CogitoNode* sc = (CogitoNode*)scv.as.p;
  sc->scroller.horz = yis_as_bool(horzv);
  sc->scroller.vert = yis_as_bool(vertv);
  cogito_window_relayout(cogito_node_window(sc));
}
static void cogito_scroller_set_sync(YisVal scv, YisVal other) {
  if (scv.tag != EVT_OBJ) yis_trap("cogito.scroller_set_sync expects scroller");
  CogitoNode* sc = (CogitoNode*)scv.as.p;
  if (other.tag == EVT_NULL) {
    sc->scroller.sync_with = NULL;
    cogito_window_relayout(cogito_node_window(sc));
    return;
  }
  if (other.tag != EVT_OBJ) yis_trap("cogito.scroller_set_sync expects scroller or null");
  CogitoNode* peer = (CogitoNode*)other.as.p;
  if (!peer || peer->kind != COGITO_SCROLLER) yis_trap("cogito.scroller_set_sync expects scroller or null");
  sc->scroller.sync_with = (peer == sc) ? NULL : peer;
  cogito_window_relayout(cogito_node_window(sc));
}

static CogitoNode* cogito_scroller_sync_from(CogitoNode* src) {
  if (!src || src->kind != COGITO_SCROLLER) return NULL;
  CogitoNode* dst = src->scroller.sync_with;
  if (!dst || dst == src || dst->kind != COGITO_SCROLLER) return NULL;

  CogitoNode* src_win = cogito_node_window(src);
  CogitoNode* dst_win = cogito_node_window(dst);
  if (!src_win || !dst_win || src_win != dst_win) return NULL;

  int src_inner_w = src->w - src->padding_left - src->padding_right;
  int src_inner_h = src->h - src->padding_top - src->padding_bottom;
  int dst_inner_w = dst->w - dst->padding_left - dst->padding_right;
  int dst_inner_h = dst->h - dst->padding_top - dst->padding_bottom;
  if (src_inner_w < 0) src_inner_w = 0;
  if (src_inner_h < 0) src_inner_h = 0;
  if (dst_inner_w < 0) dst_inner_w = 0;
  if (dst_inner_h < 0) dst_inner_h = 0;

  int src_max_x = src->scroller.content_w - src_inner_w;
  int src_max_y = src->scroller.content_h - src_inner_h;
  int dst_max_x = dst->scroller.content_w - dst_inner_w;
  int dst_max_y = dst->scroller.content_h - dst_inner_h;
  if (src_max_x < 0) src_max_x = 0;
  if (src_max_y < 0) src_max_y = 0;
  if (dst_max_x < 0) dst_max_x = 0;
  if (dst_max_y < 0) dst_max_y = 0;

  if (src->scroller.horz && dst->scroller.horz) {
    if (src_max_x > 0 && dst_max_x > 0) {
      double p = (double)src->scroller.scroll_x / (double)src_max_x;
      if (p < 0.0) p = 0.0;
      if (p > 1.0) p = 1.0;
      dst->scroller.scroll_x = (int)lround(p * (double)dst_max_x);
    } else {
      dst->scroller.scroll_x = 0;
    }
  }

  if (src->scroller.vert && dst->scroller.vert) {
    if (src_max_y > 0 && dst_max_y > 0) {
      double p = (double)src->scroller.scroll_y / (double)src_max_y;
      if (p < 0.0) p = 0.0;
      if (p > 1.0) p = 1.0;
      dst->scroller.scroll_y = (int)lround(p * (double)dst_max_y);
    } else {
      dst->scroller.scroll_y = 0;
    }
  }

  double now = cogito_now();
  if (dst->scroller.bar_alpha < 200) dst->scroller.bar_alpha = 200;
  dst->scroller.bar_hover_until = now + 0.35;

  return dst;
}

static CogitoNode* cogito_find_scroller(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_SCROLLER && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* child = cogito_find_scroller(active, x, y);
      if (child) return child;
    }
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* child = cogito_find_scroller(n->children[i], x, y);
      if (child) return child;
    }
  }
  if (n->kind == COGITO_SCROLLER && cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return n;
  return NULL;
}

// Find the deepest COGITO_CAROUSEL under the cursor (for wheel scrolling).
static CogitoNode* cogito_find_carousel(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_CAROUSEL && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* child = cogito_find_carousel(n->children[i], x, y);
    if (child) return child;
  }
  if (n->kind == COGITO_CAROUSEL && cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return n;
  return NULL;
}
