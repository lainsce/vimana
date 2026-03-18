static CogitoNode* cogito_carousel_new_obj(void) {
  CogitoNode* cr = cogito_node_new(COGITO_CAROUSEL);
  cr->kd->carousel.dragging = false;
  cr->kd->carousel.scroll_x = 0;
  cr->kd->carousel.drag_start_x = 0;
  cr->kd->carousel.drag_start_scroll = 0.0f;
  cr->kd->carousel.active_index = 0;
  cr->kd->carousel.item_count = 0;
  cr->kd->carousel.velocity = 0.0f;
  cr->kd->carousel.scroll_xf = 0.0;
  cr->kd->carousel.anim_start = 0.0;
  cr->kd->carousel.anim_from = 0.0;
  cr->kd->carousel.anim_to = 0.0;
  cr->kd->carousel.animating = false;
  cr->kd->carousel.drag_last_time = 0.0;
  cr->kd->carousel.drag_last_xf = 0.0;
  return cr;
}

static YisVal cogito_carousel_new(void) {
  return YV_OBJ(cogito_carousel_new_obj());
}

static void cogito_carousel_set_active_index(YisVal nodev, YisVal idxv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.carousel_set_active_index expects carousel");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  int idx = (int)yis_as_int(idxv);
  if (idx < 0) idx = 0;
  if (idx >= (int)n->len) idx = (int)n->len - 1;
  if (idx < 0) idx = 0;
  n->kd->carousel.active_index = idx;
  
  // Also update scroll_x so the layout function computes the same active index.
  // scroll_x = (idx / (count-1)) * max_scroll
  // Replicate the max_scroll formula from cogito_layout_carousel / 14_run.inc.
  int count = (int)n->len;
  if (count > 1) {
    int inner_w = n->w - n->padding_left - n->padding_right;
    if (inner_w < 0) inner_w = 0;
    int gaps = (count >= 3) ? 2 : (count - 1);
    if (gaps < 0) gaps = 0;
    int avail = inner_w - (gaps * 8);
    if (avail < 0) avail = 0;
    int est_ratio_sum = (count >= 3) ? (count + 5) : 7;
    int est_content = est_ratio_sum * avail / 7 + (count - 1) * 8;
    int max_scroll = est_content - inner_w;
    if (max_scroll < 0) max_scroll = 0;
    n->kd->carousel.scroll_x = (int)((double)idx / (double)(count - 1) * (double)max_scroll + 0.5);
    if (n->kd->carousel.scroll_x < 0) n->kd->carousel.scroll_x = 0;
    if (n->kd->carousel.scroll_x > max_scroll) n->kd->carousel.scroll_x = max_scroll;
    n->kd->carousel.scroll_xf = (double)n->kd->carousel.scroll_x;
  } else {
    n->kd->carousel.scroll_x = 0;
    n->kd->carousel.scroll_xf = 0.0;
  }
  n->kd->carousel.animating = false;
  
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_carousel_get_active_index(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.carousel_get_active_index expects carousel");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return YV_INT((int64_t)n->kd->carousel.active_index);
}


