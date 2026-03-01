static CogitoNode* cogito_toast_new_obj(YisStr* text) {
  CogitoNode* t = cogito_node_new(COGITO_TOAST);
  t->text = text;
  if (t->text) yis_retain_val(YV_STR(t->text));
  return t;
}
static YisVal cogito_toast_new(YisVal textv) {
  YisStr* s = stdr_to_string(textv);
  return YV_OBJ(cogito_toast_new_obj(s));
}
static void cogito_toast_set_text(YisVal tv, YisVal textv) {
  if (tv.tag != EVT_OBJ) yis_trap("cogito.toast_set_text expects toast");
  CogitoNode* n = (CogitoNode*)tv.as.p;
  YisStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_toast_on_click(YisVal tv, YisVal handler) {
  if (tv.tag != EVT_OBJ) yis_trap("cogito.toast_on_click expects toast");
  CogitoNode* t = (CogitoNode*)tv.as.p;
  if (t->on_click) yis_release_val(YV_FN(t->on_click));
  t->on_click = (YisFn*)handler.as.p;
  yis_retain_val(YV_FN(t->on_click));
}
static void cogito_toast_set_action(YisVal tv, YisVal textv, YisVal handler) {
  if (tv.tag != EVT_OBJ) yis_trap("cogito.toast_set_action expects toast");
  CogitoNode* n = (CogitoNode*)tv.as.p;
  YisStr* ts = stdr_to_string(textv);
  if (n->action_text) yis_release_val(YV_STR(n->action_text));
  n->action_text = ts;
  yis_retain_val(YV_STR(n->action_text));
  if (n->on_action) yis_release_val(YV_FN(n->on_action));
  if (handler.tag == EVT_FN) {
    n->on_action = (YisFn*)handler.as.p;
    yis_retain_val(YV_FN(n->on_action));
  } else {
    n->on_action = NULL;
  }
  cogito_window_relayout(cogito_node_window(n));
}
