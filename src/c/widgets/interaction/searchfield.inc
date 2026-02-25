static CogitoNode* cogito_searchfield_new_obj(ErgoStr* text) {
  CogitoNode* sf = cogito_node_new(COGITO_SEARCHFIELD);
  // Treat constructor text as placeholder; value starts empty.
  cogito_node_set_subtitle(sf, text);
  cogito_node_set_text(sf, NULL);
  return sf;
}
static ErgoVal cogito_searchfield_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* sf = cogito_searchfield_new_obj(ts);
  return EV_OBJ(sf);
}
static void cogito_searchfield_set_text(ErgoVal nodev, ErgoVal textv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.searchfield_set_text expects searchfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_SEARCHFIELD) ergo_trap("cogito.searchfield_set_text expects searchfield");
  ErgoStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  cogito_window_relayout(cogito_node_window(n));
}
static ErgoVal cogito_searchfield_get_text(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.searchfield_get_text expects searchfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_SEARCHFIELD) ergo_trap("cogito.searchfield_get_text expects searchfield");
  ErgoStr* s = n->text ? n->text : stdr_str_lit("");
  ergo_retain_val(EV_STR(s));
  return EV_STR(s);
}
static void cogito_searchfield_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.searchfield_on_change expects searchfield");
  if (handler.tag != EVT_FN) ergo_trap("cogito.searchfield_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_SEARCHFIELD) ergo_trap("cogito.searchfield_on_change expects searchfield");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
