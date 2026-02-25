static CogitoNode* cogito_settings_page_new_obj(ErgoStr* title) {
  CogitoNode* n = cogito_node_new(COGITO_SETTINGS_PAGE);
  cogito_node_set_text(n, title);
  n->hexpand = true;
  n->gap = 12;
  return n;
}
static ErgoVal cogito_settings_page_new(ErgoVal titlev) {
  ErgoStr* ts = stdr_to_string(titlev);
  CogitoNode* n = cogito_settings_page_new_obj(ts);
  return EV_OBJ(n);
}
