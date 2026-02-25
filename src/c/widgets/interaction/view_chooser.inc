static CogitoNode* cogito_view_chooser_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_VIEW_CHOOSER);
  n->selected = 0;
  return n;
}
static YisVal cogito_view_chooser_new(void) {
  CogitoNode* n = cogito_view_chooser_new_obj();
  return YV_OBJ(n);
}
static void cogito_view_chooser_set_items(YisVal vcv, YisVal arrv) {
  if (vcv.tag != EVT_OBJ) yis_trap("cogito.view_chooser_set_items expects view_chooser");
  if (arrv.tag != EVT_ARR) yis_trap("cogito.view_chooser_set_items expects array");
  CogitoNode* n = (CogitoNode*)vcv.as.p;
  if (n->tab_labels) {
    for (size_t i = 0; i < n->tab_len; i++) {
      if (n->tab_labels[i]) yis_release_val(YV_STR(n->tab_labels[i]));
    }
    free(n->tab_labels);
    n->tab_labels = NULL;
    n->tab_len = 0;
  }
  if (n->menu_labels) {
    for (size_t i = 0; i < n->menu_len; i++) {
      if (n->menu_labels[i]) yis_release_val(YV_STR(n->menu_labels[i]));
      if (n->menu_handlers[i]) yis_release_val(YV_FN(n->menu_handlers[i]));
      if (n->menu_icons && n->menu_icons[i]) yis_release_val(YV_STR(n->menu_icons[i]));
      if (n->menu_shortcuts && n->menu_shortcuts[i]) yis_release_val(YV_STR(n->menu_shortcuts[i]));
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
  YisArr* a = (YisArr*)arrv.as.p;
  if (!a || a->len == 0) { n->selected = -1; return; }
  size_t len = (size_t)a->len;
  n->tab_labels = (YisStr**)malloc(sizeof(YisStr*) * len);
  n->tab_len = len;
  n->tab_cap = len;
  n->menu_labels = (YisStr**)malloc(sizeof(YisStr*) * len);
  n->menu_handlers = (YisFn**)malloc(sizeof(YisFn*) * len);
  n->menu_len = len;
  n->menu_cap = len;
  for (size_t i = 0; i < len; i++) {
    YisVal v = yis_arr_get(a, (int64_t)i);
    YisStr* s = stdr_to_string(v);
    n->tab_labels[i] = s;
    if (s) yis_retain_val(YV_STR(s));
    n->menu_labels[i] = s;
    if (s) yis_retain_val(YV_STR(s));
    n->menu_handlers[i] = NULL;
  }
  if (n->selected < 0 && len > 0) n->selected = 0;
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_view_chooser_bind(YisVal vcv, YisVal vsv) {
  if (vcv.tag != EVT_OBJ) yis_trap("cogito.view_chooser_bind expects view_chooser");
  if (vsv.tag != EVT_OBJ) yis_trap("cogito.view_chooser_bind expects view_switcher");
  CogitoNode* vc = (CogitoNode*)vcv.as.p;
  CogitoNode* vs = (CogitoNode*)vsv.as.p;
  vc->view_switcher = vs;
}
