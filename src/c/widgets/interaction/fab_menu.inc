static CogitoNode* cogito_fab_menu_new_obj(const char* icon) {
  CogitoNode* n = cogito_node_new(COGITO_FAB_MENU);
  n->align = 2;
  n->w = 56;
  n->h = 56;
  if (icon) {
    YisStr* icon_str = stdr_str_from_slice(icon, strlen(icon));
    cogito_node_set_text(n, icon_str);
  }
  n->popover_open = false;
  return n;
}

static YisVal cogito_fab_menu_new(YisVal icon) {
  YisStr* ts = stdr_to_string(icon);
  CogitoNode* n = cogito_fab_menu_new_obj(ts ? ts->data : NULL);
  return EV_OBJ(n);
}

void cogito_fab_menu_add_item(YisVal fab_menuv, YisVal icon, YisVal handler) {
  if (fab_menuv.tag != EVT_OBJ) yis_trap("cogito.fab_menu_add_item expects fab_menu");
  CogitoNode* n = (CogitoNode*)fab_menuv.as.p;
  YisStr* label = stdr_to_string(icon);
  YisFn* fn = (handler.tag == EVT_FN) ? (YisFn*)handler.as.p : NULL;
  cogito_node_add_menu(n, label, fn);
  cogito_window_relayout(cogito_node_window(n));
}