static CogitoNode* cogito_settings_row_new_obj(YisStr* label) {
  CogitoNode* n = cogito_node_new(COGITO_SETTINGS_ROW);
  cogito_node_set_text(n, label);
  n->hexpand = true;
  return n;
}
static YisVal cogito_settings_row_new(YisVal labelv) {
  YisStr* ts = stdr_to_string(labelv);
  CogitoNode* n = cogito_settings_row_new_obj(ts);
  return YV_OBJ(n);
}
