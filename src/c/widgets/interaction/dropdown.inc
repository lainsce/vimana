static CogitoNode* cogito_dropdown_new_obj(void) {
  CogitoNode* dd = cogito_node_new(COGITO_DROPDOWN);
  dd->selected = -1;
  return dd;
}
static ErgoVal cogito_dropdown_new(void) {
  return EV_OBJ(cogito_dropdown_new_obj());
}
static void cogito_dropdown_set_items(ErgoVal nodev, ErgoVal arrv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.dropdown_set_items expects dropdown");
  if (arrv.tag != EVT_ARR) ergo_trap("cogito.dropdown_set_items expects array");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->menu_labels) {
    for (size_t i = 0; i < n->menu_len; i++) {
      if (n->menu_labels[i]) ergo_release_val(EV_STR(n->menu_labels[i]));
      if (n->menu_handlers[i]) ergo_release_val(EV_FN(n->menu_handlers[i]));
      if (n->menu_icons && n->menu_icons[i]) ergo_release_val(EV_STR(n->menu_icons[i]));
      if (n->menu_shortcuts && n->menu_shortcuts[i]) ergo_release_val(EV_STR(n->menu_shortcuts[i]));
    }
    free(n->menu_labels);
    free(n->menu_handlers);
    free(n->menu_icons);
    free(n->menu_shortcuts);
    free(n->menu_submenu);
    free(n->menu_toggled);
    n->menu_labels = NULL;
    n->menu_handlers = NULL;
    n->menu_icons = NULL;
    n->menu_shortcuts = NULL;
    n->menu_submenu = NULL;
    n->menu_toggled = NULL;
    n->menu_len = 0;
    n->menu_cap = 0;
  }
  ErgoArr* a = (ErgoArr*)arrv.as.p;
  if (!a || a->len == 0) { n->selected = -1; return; }
  n->menu_labels = (ErgoStr**)malloc(sizeof(ErgoStr*) * (size_t)a->len);
  n->menu_handlers = (ErgoFn**)malloc(sizeof(ErgoFn*) * (size_t)a->len);
  n->menu_len = (size_t)a->len;
  n->menu_cap = n->menu_len;
  for (size_t i = 0; i < n->menu_len; i++) {
    ErgoVal v = ergo_arr_get(a, (int64_t)i);
    ErgoStr* s = stdr_to_string(v);
    n->menu_labels[i] = s;
    n->menu_handlers[i] = NULL;
    if (s) ergo_retain_val(EV_STR(s));
  }
  if (n->selected < 0 && n->menu_len > 0) n->selected = 0;
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_dropdown_set_selected(ErgoVal nodev, ErgoVal idxv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.dropdown_set_selected expects dropdown");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  int idx = (int)ergo_as_int(idxv);
  if (idx < 0) idx = -1;
  if (n->menu_len > 0 && idx >= (int)n->menu_len) idx = (int)n->menu_len - 1;
  n->selected = idx;
  cogito_window_relayout(cogito_node_window(n));
}
static ErgoVal cogito_dropdown_get_selected(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.dropdown_get_selected expects dropdown");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_INT(n->selected);
}
static void cogito_dropdown_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.dropdown_on_change expects dropdown");
  if (handler.tag != EVT_FN) ergo_trap("cogito.dropdown_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
