static CogitoNode* cogito_searchfield_new_obj(YisStr* text) {
  CogitoNode* sf = cogito_node_new(COGITO_SEARCHFIELD);
  // Treat constructor text as placeholder; value starts empty.
  cogito_node_set_subtitle(sf, text);
  cogito_node_set_text(sf, NULL);
  return sf;
}
static YisVal cogito_searchfield_new(YisVal text) {
  YisStr* ts = stdr_to_string(text);
  CogitoNode* sf = cogito_searchfield_new_obj(ts);
  return EV_OBJ(sf);
}
static void cogito_searchfield_set_text(YisVal nodev, YisVal textv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.searchfield_set_text expects searchfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_SEARCHFIELD) yis_trap("cogito.searchfield_set_text expects searchfield");
  YisStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  cogito_window_relayout(cogito_node_window(n));
}
static YisVal cogito_searchfield_get_text(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.searchfield_get_text expects searchfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_SEARCHFIELD) yis_trap("cogito.searchfield_get_text expects searchfield");
  YisStr* s = n->text ? n->text : stdr_str_lit("");
  yis_retain_val(EV_STR(s));
  return EV_STR(s);
}
static void cogito_searchfield_on_change(YisVal nodev, YisVal handler) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.searchfield_on_change expects searchfield");
  if (handler.tag != EVT_FN) yis_trap("cogito.searchfield_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_SEARCHFIELD) yis_trap("cogito.searchfield_on_change expects searchfield");
  if (n->on_change) yis_release_val(EV_FN(n->on_change));
  n->on_change = (YisFn*)handler.as.p;
  yis_retain_val(EV_FN(n->on_change));
}
