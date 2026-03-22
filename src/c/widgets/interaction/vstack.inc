static CogitoNode* cogito_vstack_new_obj(void) {
  return cogito_node_new(COGITO_VSTACK);
}
static YisVal cogito_vstack_new(void) {
  return YV_OBJ(cogito_vstack_new_obj());
}

static void cogito_vstack_clear(YisVal vstackv) {
  if (vstackv.tag != EVT_OBJ) yis_trap("cogito.vstack_clear expects vstack");
  CogitoNode* n = (CogitoNode*)vstackv.as.p;
  cogito_clear_children(n);
  cogito_window_relayout(cogito_node_window(n));
}
