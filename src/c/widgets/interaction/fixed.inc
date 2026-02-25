static CogitoNode* cogito_fixed_new_obj(void) {
  return cogito_node_new(COGITO_FIXED);
}
static ErgoVal cogito_fixed_new(void) {
  return EV_OBJ(cogito_fixed_new_obj());
}
static void cogito_fixed_set_pos(ErgoVal fixedv, ErgoVal childv, ErgoVal xv, ErgoVal yv) {
  if (fixedv.tag != EVT_OBJ) ergo_trap("cogito.fixed_set_pos expects fixed");
  if (childv.tag != EVT_OBJ) ergo_trap("cogito.fixed_set_pos expects child");
  CogitoNode* child = (CogitoNode*)childv.as.p;
  child->fixed_x = (int)ergo_as_int(xv);
  child->fixed_y = (int)ergo_as_int(yv);
  child->fixed_pos_set = true;
  cogito_window_relayout(cogito_node_window(child));
}
