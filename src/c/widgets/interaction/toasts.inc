static CogitoNode* cogito_toasts_new_obj(void) {
  return cogito_node_new(COGITO_TOASTS);
}
static YisVal cogito_toasts_new(void) {
  return YV_OBJ(cogito_toasts_new_obj());
}
