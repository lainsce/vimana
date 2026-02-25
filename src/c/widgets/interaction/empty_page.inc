static CogitoNode* cogito_empty_page_new_obj(ErgoStr* title) {
  CogitoNode* n = cogito_node_new(COGITO_EMPTY_PAGE);
  cogito_node_set_text(n, title);
  n->hexpand = true;
  n->vexpand = true;
  return n;
}
static ErgoVal cogito_empty_page_new(ErgoVal titlev) {
  ErgoStr* ts = stdr_to_string(titlev);
  CogitoNode* n = cogito_empty_page_new_obj(ts);
  return EV_OBJ(n);
}
static void cogito_empty_page_set_description(ErgoVal epv, ErgoVal descv) {
  if (epv.tag != EVT_OBJ) ergo_trap("cogito.empty_page_set_description expects empty_page");
  CogitoNode* n = (CogitoNode*)epv.as.p;
  ErgoStr* s = stdr_to_string(descv);
  if (n->subtitle) ergo_release_val(EV_STR(n->subtitle));
  n->subtitle = s;
  ergo_retain_val(EV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_empty_page_set_icon(ErgoVal epv, ErgoVal iconv) {
  if (epv.tag != EVT_OBJ) ergo_trap("cogito.empty_page_set_icon expects empty_page");
  CogitoNode* n = (CogitoNode*)epv.as.p;
  ErgoStr* s = stdr_to_string(iconv);
  if (n->icon) ergo_release_val(EV_STR(n->icon));
  n->icon = s;
  ergo_retain_val(EV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_empty_page_set_action(ErgoVal epv, ErgoVal textv, ErgoVal handlerv) {
  if (epv.tag != EVT_OBJ) ergo_trap("cogito.empty_page_set_action expects empty_page");
  CogitoNode* n = (CogitoNode*)epv.as.p;
  ErgoStr* ts = stdr_to_string(textv);
  if (n->action_text) ergo_release_val(EV_STR(n->action_text));
  n->action_text = ts;
  ergo_retain_val(EV_STR(ts));
  if (n->on_action) ergo_release_val(EV_FN(n->on_action));
  if (handlerv.tag == EVT_FN) {
    n->on_action = (ErgoFn*)handlerv.as.p;
    ergo_retain_val(EV_FN(n->on_action));
  } else {
    n->on_action = NULL;
  }
  cogito_window_relayout(cogito_node_window(n));
}
