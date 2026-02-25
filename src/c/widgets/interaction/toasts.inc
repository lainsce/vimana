static CogitoNode* cogito_toasts_new_obj(void) {
  return cogito_node_new(COGITO_TOASTS);
}
static ErgoVal cogito_toasts_new(void) {
  return EV_OBJ(cogito_toasts_new_obj());
}
