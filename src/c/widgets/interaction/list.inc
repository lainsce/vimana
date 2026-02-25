static int cogito_list_hit_index(CogitoNode* list, int x, int y) {
  if (!cogito_hit_node(list, x, y)) return -1;
  for (size_t i = 0; i < list->len; i++) {
    CogitoNode* item = list->children[i];
    if (cogito_hit_node(item, x, y)) return (int)i;
  }
  return -1;
}
static CogitoNode* cogito_list_new_obj(void) {
  return cogito_node_new(COGITO_LIST);
}
static ErgoVal cogito_list_new(void) {
  return EV_OBJ(cogito_list_new_obj());
}
static void cogito_list_on_select(ErgoVal listv, ErgoVal handler) {
  if (listv.tag != EVT_OBJ) ergo_trap("cogito.list_on_select expects list");
  CogitoNode* list = (CogitoNode*)listv.as.p;
  if (list->on_select) ergo_release_val(EV_FN(list->on_select));
  list->on_select = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(list->on_select));
}
static void cogito_list_on_activate(ErgoVal listv, ErgoVal handler) {
  if (listv.tag != EVT_OBJ) ergo_trap("cogito.list_on_activate expects list");
  CogitoNode* list = (CogitoNode*)listv.as.p;
  if (list->on_activate) ergo_release_val(EV_FN(list->on_activate));
  list->on_activate = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(list->on_activate));
}
static CogitoNode* cogito_find_list_grid_hit(CogitoNode* n, int x, int y, int* out_idx) {
  if (!n) return NULL;
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
  if (n->kind == COGITO_LIST || n->kind == COGITO_GRID) {
    int idx = cogito_list_hit_index(n, x, y);
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
