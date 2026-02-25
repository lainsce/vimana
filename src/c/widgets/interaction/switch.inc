static CogitoNode* cogito_switch_new_obj(ErgoStr* label) {
  CogitoNode* sw = cogito_node_new(COGITO_SWITCH);
  cogito_node_set_text(sw, label);
  return sw;
}
static ErgoVal cogito_switch_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* sw = cogito_switch_new_obj(ts);
  return EV_OBJ(sw);
}
static void cogito_switch_set_checked(ErgoVal swv, ErgoVal checked) {
  if (swv.tag != EVT_OBJ) ergo_trap("cogito.switch_set_checked expects switch");
  CogitoNode* sw = (CogitoNode*)swv.as.p;
  sw->checked = ergo_as_bool(checked);
}
static ErgoVal cogito_switch_get_checked(ErgoVal swv) {
  if (swv.tag != EVT_OBJ) ergo_trap("cogito.switch_get_checked expects switch");
  CogitoNode* sw = (CogitoNode*)swv.as.p;
  return EV_BOOL(sw->checked);
}
static void cogito_switch_on_change(ErgoVal swv, ErgoVal handler) {
  if (swv.tag != EVT_OBJ) ergo_trap("cogito.switch_on_change expects switch");
  CogitoNode* sw = (CogitoNode*)swv.as.p;
  if (sw->on_change) ergo_release_val(EV_FN(sw->on_change));
  sw->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(sw->on_change));
}
