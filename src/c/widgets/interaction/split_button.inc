static CogitoNode* cogito_split_button_new_obj(YisStr* text) {
  CogitoNode* n = cogito_node_new(COGITO_SPLIT_BUTTON);
  n->split_btn.size = COGITO_BUTTON_SIZE_M;
  cogito_node_set_text(n, text);
  return n;
}
static YisVal cogito_split_button_new(YisVal textv) {
  YisStr* ts = stdr_to_string(textv);
  CogitoNode* n = cogito_split_button_new_obj(ts);
  return YV_OBJ(n);
}
static void cogito_split_button_add_menu(YisVal btnv, YisVal label, YisVal handler) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.split_button_add_menu expects split_button");
  CogitoNode* n = (CogitoNode*)btnv.as.p;
  YisStr* ts = stdr_to_string(label);
  YisFn* fn = handler.tag == EVT_FN ? (YisFn*)handler.as.p : NULL;
  cogito_node_add_menu(n, ts, fn);
}

static void cogito_split_button_add_menu_section(YisVal btnv, YisVal label, YisVal handler) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.split_button_add_menu_section expects split_button");
  CogitoNode* n = (CogitoNode*)btnv.as.p;
  YisStr* ts = stdr_to_string(label);
  YisFn* fn = handler.tag == EVT_FN ? (YisFn*)handler.as.p : NULL;
  cogito_node_add_menu_section(n, ts, fn);
}

static void cogito_split_button_set_size(YisVal btnv, YisVal sizev) {
  if (btnv.tag != EVT_OBJ) return;
  CogitoNode* n = (CogitoNode*)btnv.as.p;
  n->split_btn.size = cogito_button_clamp_size_int((int)yis_as_int(sizev));
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_split_button_set_variant(YisVal btnv, YisVal variantv) {
  if (btnv.tag != EVT_OBJ) return;
  CogitoNode* n = (CogitoNode*)btnv.as.p;
  // 0=filled (default), 1=outline, 2=text
  int v = (int)yis_as_int(variantv);
  n->split_btn.outlined = (v == 1);
  n->split_btn.text_style = (v == 2);
  cogito_window_relayout(cogito_node_window(n));
}
