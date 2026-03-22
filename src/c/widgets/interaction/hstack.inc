static CogitoNode* cogito_hstack_new_obj(void) {
  return cogito_node_new(COGITO_HSTACK);
}
static YisVal cogito_hstack_new(void) {
  return YV_OBJ(cogito_hstack_new_obj());
}

static void cogito_hstack_clear(YisVal hstackv) {
  if (hstackv.tag != EVT_OBJ) yis_trap("cogito.hstack_clear expects hstack");
  CogitoNode* n = (CogitoNode*)hstackv.as.p;
  cogito_clear_children(n);
  cogito_window_relayout(cogito_node_window(n));
}
