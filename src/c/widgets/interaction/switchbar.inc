static CogitoNode* cogito_switchbar_new_obj(ErgoStr* text) {
  CogitoNode* n = cogito_node_new(COGITO_SWITCHBAR);
  cogito_node_set_text(n, text);
  n->hexpand = true;
  return n;
}
static ErgoVal cogito_switchbar_new(ErgoVal textv) {
  ErgoStr* ts = stdr_to_string(textv);
  CogitoNode* n = cogito_switchbar_new_obj(ts);
  return EV_OBJ(n);
}
static void cogito_switchbar_set_checked(ErgoVal sbv, ErgoVal checked) {
  if (sbv.tag != EVT_OBJ) ergo_trap("cogito.switchbar_set_checked expects switchbar");
  CogitoNode* n = (CogitoNode*)sbv.as.p;
  n->checked = ergo_as_bool(checked);
}
static ErgoVal cogito_switchbar_get_checked(ErgoVal sbv) {
  if (sbv.tag != EVT_OBJ) ergo_trap("cogito.switchbar_get_checked expects switchbar");
  CogitoNode* n = (CogitoNode*)sbv.as.p;
  return EV_BOOL(n->checked);
}
static void cogito_switchbar_on_change(ErgoVal sbv, ErgoVal handler) {
  if (sbv.tag != EVT_OBJ) ergo_trap("cogito.switchbar_on_change expects switchbar");
  CogitoNode* n = (CogitoNode*)sbv.as.p;
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
