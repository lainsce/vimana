static CogitoNode* cogito_active_indicator_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_ACTIVE_INDICATOR);
  return n;
}
static ErgoVal cogito_active_indicator_new(void) {
  CogitoNode* n = cogito_active_indicator_new_obj();
  return EV_OBJ(n);
}
