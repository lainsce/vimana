static CogitoNode* cogito_banner_new_obj(YisStr* text) {
  CogitoNode* n = cogito_node_new(COGITO_BANNER);
  cogito_node_set_text(n, text);
  n->hexpand = true;
  return n;
}

static YisVal cogito_banner_new(YisVal textv) {
  YisStr* ts = stdr_to_string(textv);
  CogitoNode* n = cogito_banner_new_obj(ts);
  return EV_OBJ(n);
}

static void cogito_banner_set_action(YisVal bannerv, YisVal textv, YisVal handlerv) {
  if (bannerv.tag != EVT_OBJ) yis_trap("cogito.banner_set_action expects banner");
  CogitoNode* n = (CogitoNode*)bannerv.as.p;
  YisStr* ts = stdr_to_string(textv);
  if (n->action_text) yis_release_val(EV_STR(n->action_text));
  n->action_text = ts;
  yis_retain_val(EV_STR(ts));
  if (n->on_action) yis_release_val(EV_FN(n->on_action));
  if (handlerv.tag == EVT_FN) {
    n->on_action = (YisFn*)handlerv.as.p;
    yis_retain_val(EV_FN(n->on_action));
  } else {
    n->on_action = NULL;
  }
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_banner_set_icon(YisVal bannerv, YisVal iconv) {
  if (bannerv.tag != EVT_OBJ) yis_trap("cogito.banner_set_icon expects banner");
  CogitoNode* n = (CogitoNode*)bannerv.as.p;
  YisStr* s = stdr_to_string(iconv);
  if (n->icon) yis_release_val(EV_STR(n->icon));
  n->icon = s;
  yis_retain_val(EV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
