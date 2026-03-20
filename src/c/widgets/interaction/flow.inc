static CogitoNode* cogito_flow_new_obj(void) {
  return cogito_node_new(COGITO_FLOW);
}
static YisVal cogito_flow_new(void) {
  return YV_OBJ(cogito_flow_new_obj());
}
