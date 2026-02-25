static CogitoNode* cogito_content_list_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_CONTENT_LIST);
  n->hexpand = true;
  n->gap = 4;
  return n;
}
static YisVal cogito_content_list_new(void) {
  CogitoNode* n = cogito_content_list_new_obj();
  return YV_OBJ(n);
}
