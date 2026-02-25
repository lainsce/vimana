static CogitoNode* cogito_hstack_new_obj(void) {
  return cogito_node_new(COGITO_HSTACK);
}
static YisVal cogito_hstack_new(void) {
  return YV_OBJ(cogito_hstack_new_obj());
}
