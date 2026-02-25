static CogitoNode* cogito_settings_row_new_obj(ErgoStr* label) {
  CogitoNode* n = cogito_node_new(COGITO_SETTINGS_ROW);
  cogito_node_set_text(n, label);
  n->hexpand = true;
  return n;
}
static ErgoVal cogito_settings_row_new(ErgoVal labelv) {
  ErgoStr* ts = stdr_to_string(labelv);
  CogitoNode* n = cogito_settings_row_new_obj(ts);
  return EV_OBJ(n);
}
