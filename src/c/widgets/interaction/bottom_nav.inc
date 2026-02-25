static CogitoNode* cogito_bottom_nav_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_BOTTOM_NAV);
  n->bottom_nav.labels = NULL;
  n->bottom_nav.icons = NULL;
  n->bottom_nav.len = 0;
  n->bottom_nav.cap = 0;
  n->bottom_nav.selected = 0;
  return n;
}

static YisVal cogito_bottom_nav_new(void) {
  CogitoNode* n = cogito_bottom_nav_new_obj();
  return EV_OBJ(n);
}

static void cogito_bottom_nav_set_items(YisVal navv, YisVal labelsv, YisVal iconsv) {
  if (navv.tag != EVT_OBJ) yis_trap("cogito.bottom_nav_set_items expects bottom_nav");
  CogitoNode* n = (CogitoNode*)navv.as.p;

  // Clear existing items
  if (n->bottom_nav.labels) {
    for (size_t i = 0; i < n->bottom_nav.len; i++) {
      if (n->bottom_nav.labels[i]) yis_release_val(EV_STR(n->bottom_nav.labels[i]));
      if (n->bottom_nav.icons && n->bottom_nav.icons[i]) yis_release_val(EV_STR(n->bottom_nav.icons[i]));
    }
    free(n->bottom_nav.labels);
    free(n->bottom_nav.icons);
  }

  // Parse labels array
  if (labelsv.tag != EVT_ARR) yis_trap("cogito.bottom_nav_set_items expects array of labels");
  YisArr* labels = (YisArr*)labelsv.as.p;
  size_t count = labels->len;

  n->bottom_nav.len = count;
  n->bottom_nav.cap = count;
  n->bottom_nav.labels = (YisStr**)calloc(count, sizeof(YisStr*));
  n->bottom_nav.icons = (YisStr**)calloc(count, sizeof(YisStr*));

  for (size_t i = 0; i < count; i++) {
    YisVal lv = labels->items[i];
    YisStr* ls = stdr_to_string(lv);
    n->bottom_nav.labels[i] = ls;
    yis_retain_val(EV_STR(ls));
  }

  // Parse icons array if provided
  if (iconsv.tag == EVT_ARR) {
    YisArr* icons = (YisArr*)iconsv.as.p;
    size_t icon_count = icons->len < count ? icons->len : count;
    for (size_t i = 0; i < icon_count; i++) {
      YisVal iv = icons->items[i];
      YisStr* is = stdr_to_string(iv);
      n->bottom_nav.icons[i] = is;
      yis_retain_val(EV_STR(is));
    }
  }

  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_bottom_nav_get_selected(YisVal navv) {
  if (navv.tag != EVT_OBJ) yis_trap("cogito.bottom_nav_get_selected expects bottom_nav");
  CogitoNode* n = (CogitoNode*)navv.as.p;
  return EV_INT(n->bottom_nav.selected);
}

static void cogito_bottom_nav_set_selected(YisVal navv, YisVal idxv) {
  if (navv.tag != EVT_OBJ) yis_trap("cogito.bottom_nav_set_selected expects bottom_nav");
  CogitoNode* n = (CogitoNode*)navv.as.p;
  int idx = (int)yis_as_int(idxv);
  if (idx >= 0 && idx < (int)n->bottom_nav.len) {
    n->bottom_nav.selected = idx;
  }
}

static void cogito_bottom_nav_on_change(YisVal navv, YisVal handler) {
  if (navv.tag != EVT_OBJ) yis_trap("cogito.bottom_nav_on_change expects bottom_nav");
  CogitoNode* n = (CogitoNode*)navv.as.p;
  if (n->on_select) yis_release_val(EV_FN(n->on_select));
  if (handler.tag == EVT_FN) {
    n->on_select = (YisFn*)handler.as.p;
    yis_retain_val(EV_FN(n->on_select));
  } else {
    n->on_select = NULL;
  }
}

static int cogito_bottom_nav_hit_index(CogitoNode* n, int x, int y) {
  if (!n || n->kind != COGITO_BOTTOM_NAV) return -1;
  if (!cogito_hit_node(n, x, y)) return -1;

  if (n->bottom_nav.len == 0) return -1;
  int reserved_right = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if (!c || c->kind != COGITO_FAB || c->w <= 0) continue;
    reserved_right = c->w + 18;
    break;
  }
  int items_w = n->w - reserved_right;
  if (items_w < 1) items_w = n->w;
  if (x >= n->x + items_w) return -1;
  int item_w = items_w / (int)n->bottom_nav.len;
  if (item_w < 1) return -1;
  for (int idx = 0; idx < (int)n->bottom_nav.len; idx++) {
    int ix = n->x + idx * item_w;
    int iw = (idx == (int)n->bottom_nav.len - 1) ? (n->x + items_w - ix) : item_w;
    if (iw < 1) iw = 1;
    if (cogito_hit_rect_min48(x, y, ix, n->y, iw, n->h)) return idx;
  }
  return -1;
}

static inline CogitoNode* cogito_find_bottom_nav_hit(CogitoNode* n, int x, int y, int* out_idx) {
  if (!n) return NULL;
  if (n->kind == COGITO_BOTTOM_NAV) {
    int idx = cogito_bottom_nav_hit_index(n, x, y);
    if (idx >= 0) {
      if (out_idx) *out_idx = idx;
      return n;
    }
  }
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* hit = cogito_find_bottom_nav_hit(n->children[i], x, y, out_idx);
    if (hit) return hit;
  }
  return NULL;
}

static void cogito_handle_bottom_nav_select(CogitoNode* nav, int idx) {
  nav->bottom_nav.selected = idx;
  cogito_invoke_select(nav, idx, false);
}
