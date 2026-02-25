static CogitoNode* cogito_split_button_new_obj(ErgoStr* text) {
  CogitoNode* n = cogito_node_new(COGITO_SPLIT_BUTTON);
  n->split_btn.size = COGITO_BUTTON_SIZE_M;
  cogito_node_set_text(n, text);
  return n;
}
static ErgoVal cogito_split_button_new(ErgoVal textv) {
  ErgoStr* ts = stdr_to_string(textv);
  CogitoNode* n = cogito_split_button_new_obj(ts);
  return EV_OBJ(n);
}
static void cogito_split_button_add_menu(ErgoVal btnv, ErgoVal label, ErgoVal handler) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.split_button_add_menu expects split_button");
  CogitoNode* n = (CogitoNode*)btnv.as.p;
  ErgoStr* ts = stdr_to_string(label);
  ErgoFn* fn = handler.tag == EVT_FN ? (ErgoFn*)handler.as.p : NULL;
  cogito_node_add_menu(n, ts, fn);
}

static void cogito_split_button_add_menu_section(ErgoVal btnv, ErgoVal label, ErgoVal handler) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.split_button_add_menu_section expects split_button");
  CogitoNode* n = (CogitoNode*)btnv.as.p;
  ErgoStr* ts = stdr_to_string(label);
  ErgoFn* fn = handler.tag == EVT_FN ? (ErgoFn*)handler.as.p : NULL;
  cogito_node_add_menu_section(n, ts, fn);
}

static void cogito_split_button_set_size(ErgoVal btnv, ErgoVal sizev) {
  if (btnv.tag != EVT_OBJ) return;
  CogitoNode* n = (CogitoNode*)btnv.as.p;
  n->split_btn.size = cogito_button_clamp_size_int((int)ergo_as_int(sizev));
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_split_button_set_variant(ErgoVal btnv, ErgoVal variantv) {
  if (btnv.tag != EVT_OBJ) return;
  CogitoNode* n = (CogitoNode*)btnv.as.p;
  // 0=filled (default), 1=outline, 2=text
  int v = (int)ergo_as_int(variantv);
  n->split_btn.outlined = (v == 1);
  n->split_btn.text_style = (v == 2);
  cogito_window_relayout(cogito_node_window(n));
}
