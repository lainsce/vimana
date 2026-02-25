static CogitoNode* cogito_toolbar_new_obj(void) {
  return cogito_node_new(COGITO_TOOLBAR);
}
static ErgoVal cogito_toolbar_new(void) {
  return EV_OBJ(cogito_toolbar_new_obj());
}

void cogito_toolbar_set_vibrant(ErgoVal toolbarv, ErgoVal vibrantv) {
  if (toolbarv.tag != EVT_OBJ) ergo_trap("cogito.toolbar_set_vibrant expects toolbar");
  CogitoNode* toolbar = (CogitoNode*)toolbarv.as.p;
  if (toolbar->kind != COGITO_TOOLBAR) ergo_trap("cogito.toolbar_set_vibrant expects toolbar");
  toolbar->toolbar.vibrant = ergo_as_bool(vibrantv);
  cogito_window_relayout(cogito_node_window(toolbar));
}

ErgoVal cogito_toolbar_get_vibrant(ErgoVal toolbarv) {
  if (toolbarv.tag != EVT_OBJ) ergo_trap("cogito.toolbar_get_vibrant expects toolbar");
  CogitoNode* toolbar = (CogitoNode*)toolbarv.as.p;
  if (toolbar->kind != COGITO_TOOLBAR) ergo_trap("cogito.toolbar_get_vibrant expects toolbar");
  return EV_BOOL(toolbar->toolbar.vibrant);
}

void cogito_toolbar_set_vertical(ErgoVal toolbarv, ErgoVal verticalv) {
  if (toolbarv.tag != EVT_OBJ) ergo_trap("cogito.toolbar_set_vertical expects toolbar");
  CogitoNode* toolbar = (CogitoNode*)toolbarv.as.p;
  if (toolbar->kind != COGITO_TOOLBAR) ergo_trap("cogito.toolbar_set_vertical expects toolbar");
  toolbar->toolbar.vertical = ergo_as_bool(verticalv);
  cogito_window_relayout(cogito_node_window(toolbar));
}

ErgoVal cogito_toolbar_get_vertical(ErgoVal toolbarv) {
  if (toolbarv.tag != EVT_OBJ) ergo_trap("cogito.toolbar_get_vertical expects toolbar");
  CogitoNode* toolbar = (CogitoNode*)toolbarv.as.p;
  if (toolbar->kind != COGITO_TOOLBAR) ergo_trap("cogito.toolbar_get_vertical expects toolbar");
  return EV_BOOL(toolbar->toolbar.vertical);
}
