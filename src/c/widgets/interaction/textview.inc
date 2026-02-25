static CogitoNode* cogito_textview_new_obj(YisStr* text) {
  CogitoNode* tv = cogito_node_new(COGITO_TEXTVIEW);
  cogito_node_set_text(tv, text);
  return tv;
}
static YisVal cogito_textview_new(YisVal text) {
  YisStr* ts = stdr_to_string(text);
  CogitoNode* tv = cogito_textview_new_obj(ts);
  return YV_OBJ(tv);
}
static void cogito_textview_set_text(YisVal nodev, YisVal textv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textview_set_text expects textview");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  cogito_window_relayout(cogito_node_window(n));
}
static YisVal cogito_textview_get_text(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textview_get_text expects textview");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* s = n->text ? n->text : stdr_str_lit("");
  yis_retain_val(YV_STR(s));
  return YV_STR(s);
}
static void cogito_textview_on_change(YisVal nodev, YisVal handler) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textview_on_change expects textview");
  if (handler.tag != EVT_FN) yis_trap("cogito.textview_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->on_change) yis_release_val(YV_FN(n->on_change));
  n->on_change = (YisFn*)handler.as.p;
  yis_retain_val(YV_FN(n->on_change));
}
