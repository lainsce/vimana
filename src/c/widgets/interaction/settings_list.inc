static CogitoNode* cogito_settings_list_new_obj(YisStr* title) {
  CogitoNode* n = cogito_node_new(COGITO_SETTINGS_LIST);
  cogito_node_set_text(n, title);
  n->hexpand = true;
  n->gap = 0;
  return n;
}
static YisVal cogito_settings_list_new(YisVal titlev) {
  YisStr* ts = stdr_to_string(titlev);
  CogitoNode* n = cogito_settings_list_new_obj(ts);
  return YV_OBJ(n);
}
