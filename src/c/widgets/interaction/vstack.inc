static CogitoNode* cogito_vstack_new_obj(void) {
  return cogito_node_new(COGITO_VSTACK);
}
static ErgoVal cogito_vstack_new(void) {
  return EV_OBJ(cogito_vstack_new_obj());
}
