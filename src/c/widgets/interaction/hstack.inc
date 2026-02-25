static CogitoNode* cogito_hstack_new_obj(void) {
  return cogito_node_new(COGITO_HSTACK);
}
static ErgoVal cogito_hstack_new(void) {
  return EV_OBJ(cogito_hstack_new_obj());
}
