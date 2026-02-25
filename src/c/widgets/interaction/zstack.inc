static CogitoNode* cogito_zstack_new_obj(void) {
  return cogito_node_new(COGITO_ZSTACK);
}
static YisVal cogito_zstack_new(void) {
  return YV_OBJ(cogito_zstack_new_obj());
}
