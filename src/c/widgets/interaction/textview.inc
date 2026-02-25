static CogitoNode* cogito_textview_new_obj(ErgoStr* text) {
  CogitoNode* tv = cogito_node_new(COGITO_TEXTVIEW);
  cogito_node_set_text(tv, text);
  return tv;
}
static ErgoVal cogito_textview_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* tv = cogito_textview_new_obj(ts);
  return EV_OBJ(tv);
}
static void cogito_textview_set_text(ErgoVal nodev, ErgoVal textv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textview_set_text expects textview");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  cogito_window_relayout(cogito_node_window(n));
}
static ErgoVal cogito_textview_get_text(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textview_get_text expects textview");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* s = n->text ? n->text : stdr_str_lit("");
  ergo_retain_val(EV_STR(s));
  return EV_STR(s);
}
static void cogito_textview_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textview_on_change expects textview");
  if (handler.tag != EVT_FN) ergo_trap("cogito.textview_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
