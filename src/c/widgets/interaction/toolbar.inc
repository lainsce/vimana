static CogitoNode* cogito_toolbar_new_obj(void) {
  return cogito_node_new(COGITO_TOOLBAR);
}
static YisVal cogito_toolbar_new(void) {
  return YV_OBJ(cogito_toolbar_new_obj());
}

void cogito_toolbar_set_vibrant(YisVal toolbarv, YisVal vibrantv) {
  if (toolbarv.tag != EVT_OBJ) yis_trap("cogito.toolbar_set_vibrant expects toolbar");
  CogitoNode* toolbar = (CogitoNode*)toolbarv.as.p;
  if (toolbar->kind != COGITO_TOOLBAR) yis_trap("cogito.toolbar_set_vibrant expects toolbar");
  toolbar->toolbar.vibrant = yis_as_bool(vibrantv);
  cogito_window_relayout(cogito_node_window(toolbar));
}

YisVal cogito_toolbar_get_vibrant(YisVal toolbarv) {
  if (toolbarv.tag != EVT_OBJ) yis_trap("cogito.toolbar_get_vibrant expects toolbar");
  CogitoNode* toolbar = (CogitoNode*)toolbarv.as.p;
  if (toolbar->kind != COGITO_TOOLBAR) yis_trap("cogito.toolbar_get_vibrant expects toolbar");
  return YV_BOOL(toolbar->toolbar.vibrant);
}

void cogito_toolbar_set_vertical(YisVal toolbarv, YisVal verticalv) {
  if (toolbarv.tag != EVT_OBJ) yis_trap("cogito.toolbar_set_vertical expects toolbar");
  CogitoNode* toolbar = (CogitoNode*)toolbarv.as.p;
  if (toolbar->kind != COGITO_TOOLBAR) yis_trap("cogito.toolbar_set_vertical expects toolbar");
  toolbar->toolbar.vertical = yis_as_bool(verticalv);
  cogito_window_relayout(cogito_node_window(toolbar));
}

YisVal cogito_toolbar_get_vertical(YisVal toolbarv) {
  if (toolbarv.tag != EVT_OBJ) yis_trap("cogito.toolbar_get_vertical expects toolbar");
  CogitoNode* toolbar = (CogitoNode*)toolbarv.as.p;
  if (toolbar->kind != COGITO_TOOLBAR) yis_trap("cogito.toolbar_get_vertical expects toolbar");
  return YV_BOOL(toolbar->toolbar.vertical);
}
