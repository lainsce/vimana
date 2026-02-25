static CogitoNode* cogito_tip_view_new_obj(ErgoStr* text) {
  CogitoNode* n = cogito_node_new(COGITO_TIP_VIEW);
  cogito_node_set_text(n, text);
  return n;
}
static ErgoVal cogito_tip_view_new(ErgoVal textv) {
  ErgoStr* ts = stdr_to_string(textv);
  CogitoNode* n = cogito_tip_view_new_obj(ts);
  return EV_OBJ(n);
}
static void cogito_tip_view_set_title(ErgoVal tvv, ErgoVal titlev) {
  if (tvv.tag != EVT_OBJ) ergo_trap("cogito.tip_view_set_title expects tip_view");
  CogitoNode* n = (CogitoNode*)tvv.as.p;
  ErgoStr* s = stdr_to_string(titlev);
  if (n->subtitle) ergo_release_val(EV_STR(n->subtitle));
  n->subtitle = s;
  ergo_retain_val(EV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
