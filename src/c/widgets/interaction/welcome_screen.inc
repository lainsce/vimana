static CogitoNode* cogito_welcome_screen_new_obj(YisStr* title) {
  CogitoNode* n = cogito_node_new(COGITO_WELCOME_SCREEN);
  cogito_node_set_text(n, title);
  n->hexpand = true;
  n->vexpand = true;
  return n;
}
static YisVal cogito_welcome_screen_new(YisVal titlev) {
  YisStr* ts = stdr_to_string(titlev);
  CogitoNode* n = cogito_welcome_screen_new_obj(ts);
  return YV_OBJ(n);
}
static void cogito_welcome_screen_set_description(YisVal wsv, YisVal descv) {
  if (wsv.tag != EVT_OBJ) yis_trap("cogito.welcome_screen_set_description expects welcome_screen");
  CogitoNode* n = (CogitoNode*)wsv.as.p;
  YisStr* s = stdr_to_string(descv);
  if (n->subtitle) yis_release_val(YV_STR(n->subtitle));
  n->subtitle = s;
  yis_retain_val(YV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_welcome_screen_set_icon(YisVal wsv, YisVal iconv) {
  if (wsv.tag != EVT_OBJ) yis_trap("cogito.welcome_screen_set_icon expects welcome_screen");
  CogitoNode* n = (CogitoNode*)wsv.as.p;
  YisStr* s = stdr_to_string(iconv);
  if (n->icon) yis_release_val(YV_STR(n->icon));
  n->icon = s;
  yis_retain_val(YV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_welcome_screen_set_action(YisVal wsv, YisVal textv, YisVal handlerv) {
  if (wsv.tag != EVT_OBJ) yis_trap("cogito.welcome_screen_set_action expects welcome_screen");
  CogitoNode* n = (CogitoNode*)wsv.as.p;
  YisStr* ts = stdr_to_string(textv);
  if (n->action_text) yis_release_val(YV_STR(n->action_text));
  n->action_text = ts;
  yis_retain_val(YV_STR(ts));
  if (n->on_action) yis_release_val(YV_FN(n->on_action));
  if (handlerv.tag == EVT_FN) {
    n->on_action = (YisFn*)handlerv.as.p;
    yis_retain_val(YV_FN(n->on_action));
  } else {
    n->on_action = NULL;
  }
  cogito_window_relayout(cogito_node_window(n));
}
