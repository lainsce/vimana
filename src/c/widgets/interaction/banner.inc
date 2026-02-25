static CogitoNode* cogito_banner_new_obj(ErgoStr* text) {
  CogitoNode* n = cogito_node_new(COGITO_BANNER);
  cogito_node_set_text(n, text);
  n->hexpand = true;
  return n;
}

static ErgoVal cogito_banner_new(ErgoVal textv) {
  ErgoStr* ts = stdr_to_string(textv);
  CogitoNode* n = cogito_banner_new_obj(ts);
  return EV_OBJ(n);
}

static void cogito_banner_set_action(ErgoVal bannerv, ErgoVal textv, ErgoVal handlerv) {
  if (bannerv.tag != EVT_OBJ) ergo_trap("cogito.banner_set_action expects banner");
  CogitoNode* n = (CogitoNode*)bannerv.as.p;
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

static void cogito_banner_set_icon(ErgoVal bannerv, ErgoVal iconv) {
  if (bannerv.tag != EVT_OBJ) ergo_trap("cogito.banner_set_icon expects banner");
  CogitoNode* n = (CogitoNode*)bannerv.as.p;
  ErgoStr* s = stdr_to_string(iconv);
  if (n->icon) ergo_release_val(EV_STR(n->icon));
  n->icon = s;
  ergo_retain_val(EV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
