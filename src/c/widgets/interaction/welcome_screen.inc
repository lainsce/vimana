static CogitoNode* cogito_welcome_screen_new_obj(ErgoStr* title) {
  CogitoNode* n = cogito_node_new(COGITO_WELCOME_SCREEN);
  cogito_node_set_text(n, title);
  n->hexpand = true;
  n->vexpand = true;
  return n;
}
static ErgoVal cogito_welcome_screen_new(ErgoVal titlev) {
  ErgoStr* ts = stdr_to_string(titlev);
  CogitoNode* n = cogito_welcome_screen_new_obj(ts);
  return EV_OBJ(n);
}
static void cogito_welcome_screen_set_description(ErgoVal wsv, ErgoVal descv) {
  if (wsv.tag != EVT_OBJ) ergo_trap("cogito.welcome_screen_set_description expects welcome_screen");
  CogitoNode* n = (CogitoNode*)wsv.as.p;
  ErgoStr* s = stdr_to_string(descv);
  if (n->subtitle) ergo_release_val(EV_STR(n->subtitle));
  n->subtitle = s;
  ergo_retain_val(EV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_welcome_screen_set_icon(ErgoVal wsv, ErgoVal iconv) {
  if (wsv.tag != EVT_OBJ) ergo_trap("cogito.welcome_screen_set_icon expects welcome_screen");
  CogitoNode* n = (CogitoNode*)wsv.as.p;
  ErgoStr* s = stdr_to_string(iconv);
  if (n->icon) ergo_release_val(EV_STR(n->icon));
  n->icon = s;
  ergo_retain_val(EV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_welcome_screen_set_action(ErgoVal wsv, ErgoVal textv, ErgoVal handlerv) {
  if (wsv.tag != EVT_OBJ) ergo_trap("cogito.welcome_screen_set_action expects welcome_screen");
  CogitoNode* n = (CogitoNode*)wsv.as.p;
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
