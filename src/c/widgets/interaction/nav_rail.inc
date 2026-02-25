static CogitoNode* cogito_nav_rail_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_NAV_RAIL);
  n->nav_rail.labels = NULL;
  n->nav_rail.icons = NULL;
  n->nav_rail.badges = NULL;
  n->nav_rail.len = 0;
  n->nav_rail.cap = 0;
  n->nav_rail.selected = 0;
  n->nav_rail.expanded = false;
  n->nav_rail.show_toggle = true;
  n->nav_rail.no_label = false;
  return n;
}

static ErgoVal cogito_nav_rail_new(void) {
  CogitoNode* n = cogito_nav_rail_new_obj();
  return EV_OBJ(n);
}

static void cogito_nav_rail_set_items(ErgoVal railv, ErgoVal labelsv, ErgoVal iconsv) {
  if (railv.tag != EVT_OBJ) ergo_trap("cogito.nav_rail_set_items expects nav_rail");
  CogitoNode* n = (CogitoNode*)railv.as.p;

  // Clear existing items
  if (n->nav_rail.labels || n->nav_rail.badges) {
    for (size_t i = 0; i < n->nav_rail.len; i++) {
      if (n->nav_rail.labels && n->nav_rail.labels[i]) ergo_release_val(EV_STR(n->nav_rail.labels[i]));
      if (n->nav_rail.icons && n->nav_rail.icons[i]) ergo_release_val(EV_STR(n->nav_rail.icons[i]));
    }
    free(n->nav_rail.labels);
    free(n->nav_rail.icons);
    free(n->nav_rail.badges);
  }

  // Parse labels array
  if (labelsv.tag != EVT_ARR) ergo_trap("cogito.nav_rail_set_items expects array of labels");
  ErgoArr* labels = (ErgoArr*)labelsv.as.p;
  size_t count = labels->len;

  n->nav_rail.len = count;
  n->nav_rail.cap = count;
  n->nav_rail.labels = (ErgoStr**)calloc(count, sizeof(ErgoStr*));
  n->nav_rail.icons = (ErgoStr**)calloc(count, sizeof(ErgoStr*));
  n->nav_rail.badges = (int*)calloc(count, sizeof(int));

  for (size_t i = 0; i < count; i++) {
    ErgoVal lv = labels->items[i];
    ErgoStr* ls = stdr_to_string(lv);
    n->nav_rail.labels[i] = ls;
    ergo_retain_val(EV_STR(ls));
  }

  // Parse icons array if provided
  if (iconsv.tag == EVT_ARR) {
    ErgoArr* icons = (ErgoArr*)iconsv.as.p;
    size_t icon_count = icons->len < count ? icons->len : count;
    for (size_t i = 0; i < icon_count; i++) {
      ErgoVal iv = icons->items[i];
      ErgoStr* is = stdr_to_string(iv);
      n->nav_rail.icons[i] = is;
      ergo_retain_val(EV_STR(is));
    }
  }

  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_nav_rail_set_badges(ErgoVal railv, ErgoVal badgesv) {
  if (railv.tag != EVT_OBJ) ergo_trap("cogito.nav_rail_set_badges expects nav_rail");
  CogitoNode* n = (CogitoNode*)railv.as.p;
  if (n->kind != COGITO_NAV_RAIL) ergo_trap("cogito.nav_rail_set_badges expects nav_rail");

  size_t count = n->nav_rail.len;
  if (count == 0) return;
  if (!n->nav_rail.badges) {
    n->nav_rail.badges = (int*)calloc(count, sizeof(int));
  }
  if (!n->nav_rail.badges) return;
  memset(n->nav_rail.badges, 0, sizeof(int) * count);

  if (badgesv.tag == EVT_NULL) {
    cogito_window_relayout(cogito_node_window(n));
    return;
  }
  if (badgesv.tag != EVT_ARR) ergo_trap("cogito.nav_rail_set_badges expects array");

  ErgoArr* badges = (ErgoArr*)badgesv.as.p;
  size_t use = badges->len < count ? badges->len : count;
  for (size_t i = 0; i < use; i++) {
    ErgoVal bv = badges->items[i];
    int badge = 0;
    switch (bv.tag) {
      case EVT_NULL:
        badge = 0;
        break;
      case EVT_BOOL:
        badge = bv.as.b ? -1 : 0;
        break;
      case EVT_INT:
        badge = (int)bv.as.i;
        if (badge < 0) badge = -1;
        break;
      case EVT_FLOAT: {
        int v = (int)lround(bv.as.f);
        badge = v < 0 ? -1 : v;
        break;
      }
      default:
        ergo_trap("cogito.nav_rail_set_badges expects int/bool/null entries");
    }
    n->nav_rail.badges[i] = badge;
  }

  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_nav_rail_set_toggle(ErgoVal railv, ErgoVal onv) {
  if (railv.tag != EVT_OBJ) ergo_trap("cogito.nav_rail_set_toggle expects nav_rail");
  CogitoNode* n = (CogitoNode*)railv.as.p;
  if (n->kind != COGITO_NAV_RAIL) ergo_trap("cogito.nav_rail_set_toggle expects nav_rail");
  n->nav_rail.show_toggle = ergo_as_bool(onv);
  if (!n->nav_rail.show_toggle) n->nav_rail.expanded = false;
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_nav_rail_get_selected(ErgoVal railv) {
  if (railv.tag != EVT_OBJ) ergo_trap("cogito.nav_rail_get_selected expects nav_rail");
  CogitoNode* n = (CogitoNode*)railv.as.p;
  return EV_INT(n->nav_rail.selected);
}

static void cogito_nav_rail_set_selected(ErgoVal railv, ErgoVal idxv) {
  if (railv.tag != EVT_OBJ) ergo_trap("cogito.nav_rail_set_selected expects nav_rail");
  CogitoNode* n = (CogitoNode*)railv.as.p;
  int idx = (int)ergo_as_int(idxv);
  if (idx >= 0 && idx < (int)n->nav_rail.len) {
    n->nav_rail.selected = idx;
  }
}

static void cogito_nav_rail_on_change(ErgoVal railv, ErgoVal handler) {
  if (railv.tag != EVT_OBJ) ergo_trap("cogito.nav_rail_on_change expects nav_rail");
  CogitoNode* n = (CogitoNode*)railv.as.p;
  if (n->on_select) ergo_release_val(EV_FN(n->on_select));
  if (handler.tag == EVT_FN) {
    n->on_select = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(n->on_select));
  } else {
    n->on_select = NULL;
  }
}

static void cogito_nav_rail_toggle_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!n) return;
  if (!n->nav_rail.show_toggle) {
    if (out_x) *out_x = 0;
    if (out_y) *out_y = 0;
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    return;
  }
  int rail_w = n->w > 0 ? n->w : COGITO_NAV_RAIL_COLLAPSED_WIDTH;
  int x = n->x + (rail_w - COGITO_NAV_RAIL_SHARED_TOGGLE_BUTTON_SIZE) / 2;
  if (n->nav_rail.expanded) {
    int indicator_w = rail_w - (COGITO_NAV_RAIL_EXPANDED_SIDE_PADDING * 2);
    if (indicator_w < COGITO_NAV_RAIL_EXPANDED_INDICATOR_MIN_WIDTH) {
      indicator_w = COGITO_NAV_RAIL_EXPANDED_INDICATOR_MIN_WIDTH;
    }
    int bg_x = n->x + (rail_w - indicator_w) / 2;
    int content_x = bg_x + COGITO_NAV_RAIL_EXPANDED_CONTENT_INSET;
    int align_x = content_x - (COGITO_NAV_RAIL_SHARED_TOGGLE_BUTTON_SIZE - COGITO_NAV_RAIL_SHARED_TOGGLE_ICON_SIZE) / 2;
    int min_x = n->x;
    int max_x = n->x + rail_w - COGITO_NAV_RAIL_SHARED_TOGGLE_BUTTON_SIZE;
    if (align_x < min_x) align_x = min_x;
    if (align_x > max_x) align_x = max_x;
    x = align_x;
  }
  int top_offset = n->nav_rail.expanded
    ? COGITO_NAV_RAIL_EXPANDED_TOP_OFFSET
    : COGITO_NAV_RAIL_COLLAPSED_TOP_OFFSET;
  int y = n->y + n->padding_top + top_offset;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = COGITO_NAV_RAIL_SHARED_TOGGLE_BUTTON_SIZE;
  if (out_h) *out_h = COGITO_NAV_RAIL_SHARED_TOGGLE_BUTTON_SIZE;
}

static bool cogito_nav_rail_hit_toggle(CogitoNode* n, int x, int y) {
  if (!n || n->kind != COGITO_NAV_RAIL || !n->nav_rail.show_toggle) return false;
  int tx = 0, ty = 0, tw = 0, th = 0;
  cogito_nav_rail_toggle_rect(n, &tx, &ty, &tw, &th);
  return cogito_hit_rect_min48(x, y, tx, ty, tw, th);
}

static int cogito_nav_rail_items_start_y(CogitoNode* n) {
  if (!n) return 0;
  int top_offset = n->nav_rail.expanded
    ? COGITO_NAV_RAIL_EXPANDED_TOP_OFFSET
    : COGITO_NAV_RAIL_COLLAPSED_TOP_OFFSET;
  int section_gap = n->nav_rail.expanded
    ? COGITO_NAV_RAIL_EXPANDED_SECTION_GAP
    : COGITO_NAV_RAIL_COLLAPSED_SECTION_GAP;
  int item_y = n->y + n->padding_top + top_offset + section_gap;
  if (n->nav_rail.show_toggle) {
    item_y += COGITO_NAV_RAIL_SHARED_TOGGLE_BUTTON_SIZE;
  }
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if (!c || c->kind != COGITO_FAB || c->w <= 0 || c->h <= 0) continue;
    int min_item_y = c->y + c->h + section_gap;
    if (item_y < min_item_y) item_y = min_item_y;
    break;
  }
  return item_y;
}

static int cogito_nav_rail_item_step(const CogitoNode* n) {
  if (n && n->nav_rail.expanded) return COGITO_NAV_RAIL_EXPANDED_ITEM_STEP;
  return COGITO_NAV_RAIL_COLLAPSED_ITEM_STEP;
}

static int cogito_nav_rail_hit_index(CogitoNode* n, int x, int y) {
  if (!n || n->kind != COGITO_NAV_RAIL) return -1;
  int rail_w = n->w > 0 ? n->w : COGITO_NAV_RAIL_COLLAPSED_WIDTH;
  if (x < n->x || x >= n->x + rail_w || y < n->y || y >= n->y + n->h) return -1;
  if (cogito_nav_rail_hit_toggle(n, x, y)) return -1;

  int item_y = cogito_nav_rail_items_start_y(n);
  int item_step = cogito_nav_rail_item_step(n);

  for (size_t i = 0; i < n->nav_rail.len; i++) {
    if (cogito_hit_rect_min48(x, y, n->x, item_y, rail_w, item_step)) return (int)i;
    item_y += item_step;
  }
  return -1;
}

static inline CogitoNode* cogito_find_nav_rail_hit(CogitoNode* n, int x, int y, int* out_idx) {
  if (!n) return NULL;
  if (n->kind == COGITO_NAV_RAIL) {
    if (cogito_nav_rail_hit_toggle(n, x, y)) return n;
    int idx = cogito_nav_rail_hit_index(n, x, y);
    if (idx >= 0) {
      if (out_idx) *out_idx = idx;
      return n;
    }
  }
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* hit = cogito_find_nav_rail_hit(n->children[i], x, y, out_idx);
    if (hit) return hit;
  }
  return NULL;
}

static void cogito_handle_nav_rail_select(CogitoNode* rail, int idx) {
  rail->nav_rail.selected = idx;
  cogito_invoke_select(rail, idx, false);
}
