static CogitoNode* cogito_switchbar_new_obj(YisStr* text) {
  CogitoNode* n = cogito_node_new(COGITO_SWITCHBAR);
  cogito_node_set_text(n, text);
  n->hexpand = true;
  return n;
}
static YisVal cogito_switchbar_new(YisVal textv) {
  YisStr* ts = stdr_to_string(textv);
  CogitoNode* n = cogito_switchbar_new_obj(ts);
  return EV_OBJ(n);
}
static void cogito_switchbar_set_checked(YisVal sbv, YisVal checked) {
  if (sbv.tag != EVT_OBJ) yis_trap("cogito.switchbar_set_checked expects switchbar");
  CogitoNode* n = (CogitoNode*)sbv.as.p;
  n->checked = yis_as_bool(checked);
}
static YisVal cogito_switchbar_get_checked(YisVal sbv) {
  if (sbv.tag != EVT_OBJ) yis_trap("cogito.switchbar_get_checked expects switchbar");
  CogitoNode* n = (CogitoNode*)sbv.as.p;
  return EV_BOOL(n->checked);
}
static void cogito_switchbar_on_change(YisVal sbv, YisVal handler) {
  if (sbv.tag != EVT_OBJ) yis_trap("cogito.switchbar_on_change expects switchbar");
  CogitoNode* n = (CogitoNode*)sbv.as.p;
  if (n->on_change) yis_release_val(EV_FN(n->on_change));
  n->on_change = (YisFn*)handler.as.p;
  yis_retain_val(EV_FN(n->on_change));
}
