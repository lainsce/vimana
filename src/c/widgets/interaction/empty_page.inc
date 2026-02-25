static CogitoNode* cogito_empty_page_new_obj(YisStr* title) {
  CogitoNode* n = cogito_node_new(COGITO_EMPTY_PAGE);
  cogito_node_set_text(n, title);
  n->hexpand = true;
  n->vexpand = true;
  return n;
}
static YisVal cogito_empty_page_new(YisVal titlev) {
  YisStr* ts = stdr_to_string(titlev);
  CogitoNode* n = cogito_empty_page_new_obj(ts);
  return YV_OBJ(n);
}
static void cogito_empty_page_set_description(YisVal epv, YisVal descv) {
  if (epv.tag != EVT_OBJ) yis_trap("cogito.empty_page_set_description expects empty_page");
  CogitoNode* n = (CogitoNode*)epv.as.p;
  YisStr* s = stdr_to_string(descv);
  if (n->subtitle) yis_release_val(YV_STR(n->subtitle));
  n->subtitle = s;
  yis_retain_val(YV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_empty_page_set_icon(YisVal epv, YisVal iconv) {
  if (epv.tag != EVT_OBJ) yis_trap("cogito.empty_page_set_icon expects empty_page");
  CogitoNode* n = (CogitoNode*)epv.as.p;
  YisStr* s = stdr_to_string(iconv);
  if (n->icon) yis_release_val(YV_STR(n->icon));
  n->icon = s;
  yis_retain_val(YV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_empty_page_set_action(YisVal epv, YisVal textv, YisVal handlerv) {
  if (epv.tag != EVT_OBJ) yis_trap("cogito.empty_page_set_action expects empty_page");
  CogitoNode* n = (CogitoNode*)epv.as.p;
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
