static CogitoNode* cogito_switch_new_obj(YisStr* label) {
  CogitoNode* sw = cogito_node_new(COGITO_SWITCH);
  cogito_node_set_text(sw, label);
  return sw;
}
static YisVal cogito_switch_new(YisVal text) {
  YisStr* ts = stdr_to_string(text);
  CogitoNode* sw = cogito_switch_new_obj(ts);
  return YV_OBJ(sw);
}
static void cogito_switch_set_checked(YisVal swv, YisVal checked) {
  if (swv.tag != EVT_OBJ) yis_trap("cogito.switch_set_checked expects switch");
  CogitoNode* sw = (CogitoNode*)swv.as.p;
  sw->checked = yis_as_bool(checked);
}
static YisVal cogito_switch_get_checked(YisVal swv) {
  if (swv.tag != EVT_OBJ) yis_trap("cogito.switch_get_checked expects switch");
  CogitoNode* sw = (CogitoNode*)swv.as.p;
  return YV_BOOL(sw->checked);
}
static void cogito_switch_on_change(YisVal swv, YisVal handler) {
  if (swv.tag != EVT_OBJ) yis_trap("cogito.switch_on_change expects switch");
  CogitoNode* sw = (CogitoNode*)swv.as.p;
  if (sw->on_change) yis_release_val(YV_FN(sw->on_change));
  sw->on_change = (YisFn*)handler.as.p;
  yis_retain_val(YV_FN(sw->on_change));
}
