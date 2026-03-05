static bool cogito_list_point_visible(CogitoNode* list, int x, int y) {
  if (!list) return false;
  if (!cogito_hit_rect(x, y, list->x, list->y, list->w, list->h)) return false;
  CogitoNode* p = list->parent;
  while (p) {
    if (p->kind == COGITO_SCROLLER && !cogito_hit_rect(x, y, p->x, p->y, p->w, p->h)) return false;
    if (p->kind == COGITO_DIALOG && !cogito_hit_rect(x, y, p->x, p->y, p->w, p->h)) return false;
    p = p->parent;
  }
  return true;
}

static bool cogito_rect_visible_local(int cx, int cy, int cw, int ch,
                                      int vx, int vy, int vw, int vh) {
  if (cw <= 0 || ch <= 0 || vw <= 0 || vh <= 0) return false;
  if (cx + cw <= vx) return false;
  if (cy + ch <= vy) return false;
  if (cx >= vx + vw) return false;
  if (cy >= vy + vh) return false;
  return true;
}

static int cogito_content_list_hit_index(CogitoNode* list, int x, int y) {
  if (!cogito_list_point_visible(list, x, y)) return -1;
  for (size_t i = 0; i < list->len; i++) {
    CogitoNode* item = list->children[i];
    if (!item || item->hidden) continue;
    if (!cogito_rect_visible_local(item->x, item->y, item->w, item->h, list->x, list->y, list->w, list->h)) continue;
    if (cogito_hit_node(item, x, y)) return (int)i;
  }
  return -1;
}
static CogitoNode* cogito_content_list_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_CONTENT_LIST);
  n->hexpand = true;
  n->gap = 4;
  return n;
}
static YisVal cogito_content_list_new(void) {
  CogitoNode* n = cogito_content_list_new_obj();
  return YV_OBJ(n);
}
static void cogito_content_list_on_select(YisVal listv, YisVal handler) {
  if (listv.tag != EVT_OBJ) yis_trap("cogito.content_list_on_select expects content_list");
  CogitoNode* list = (CogitoNode*)listv.as.p;
  if (list->on_select) yis_release_val(YV_FN(list->on_select));
  list->on_select = (YisFn*)handler.as.p;
  yis_retain_val(YV_FN(list->on_select));
}
static void cogito_content_list_on_activate(YisVal listv, YisVal handler) {
  if (listv.tag != EVT_OBJ) yis_trap("cogito.content_list_on_activate expects content_list");
  CogitoNode* list = (CogitoNode*)listv.as.p;
  if (list->on_activate) yis_release_val(YV_FN(list->on_activate));
  list->on_activate = (YisFn*)handler.as.p;
  yis_retain_val(YV_FN(list->on_activate));
}
static CogitoNode* cogito_find_list_grid_hit(CogitoNode* n, int x, int y, int* out_idx) {
  if (!n) return NULL;
  if (n->hidden) return NULL;
  if (n->kind == COGITO_DIALOG && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  if (n->kind == COGITO_SCROLLER && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* hit = cogito_find_list_grid_hit(active, x, y, out_idx);
      if (hit) return hit;
    }
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* hit = cogito_find_list_grid_hit(n->children[i], x, y, out_idx);
      if (hit) return hit;
    }
  }
  if (n->kind == COGITO_CONTENT_LIST || n->kind == COGITO_GRID) {
    int idx = cogito_content_list_hit_index(n, x, y);
    if (idx >= 0) {
      if (out_idx) *out_idx = idx;
      return n;
    }
  }
  return NULL;
}
static void cogito_handle_list_select(CogitoNode* list, int idx, bool activate) {
  if (list->kind == COGITO_GRID && !list->on_select && !list->on_activate) {
    return;
  }
  list->selected = idx;
  cogito_invoke_select(list, idx, activate);
}
static void cogito_handle_click(CogitoNode* win, int x, int y, bool activate) {
  int idx = -1;
  CogitoNode* list = cogito_find_list_grid_hit(win, x, y, &idx);
  if (list && idx >= 0) {
    cogito_handle_list_select(list, idx, activate);
  }
}
