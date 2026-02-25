static CogitoNode* cogito_tip_view_new_obj(YisStr* text) {
  CogitoNode* n = cogito_node_new(COGITO_TIP_VIEW);
  cogito_node_set_text(n, text);
  return n;
}
static YisVal cogito_tip_view_new(YisVal textv) {
  YisStr* ts = stdr_to_string(textv);
  CogitoNode* n = cogito_tip_view_new_obj(ts);
  return YV_OBJ(n);
}
static void cogito_tip_view_set_title(YisVal tvv, YisVal titlev) {
  if (tvv.tag != EVT_OBJ) yis_trap("cogito.tip_view_set_title expects tip_view");
  CogitoNode* n = (CogitoNode*)tvv.as.p;
  YisStr* s = stdr_to_string(titlev);
  if (n->subtitle) yis_release_val(YV_STR(n->subtitle));
  n->subtitle = s;
  yis_retain_val(YV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
