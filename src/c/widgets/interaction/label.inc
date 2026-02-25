static CogitoNode* cogito_label_new_obj(ErgoStr* label) {
  CogitoNode* lab = cogito_node_new(COGITO_LABEL);
  cogito_node_set_text(lab, label);
  return lab;
}
static ErgoVal cogito_label_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* lab = cogito_label_new_obj(ts);
  return EV_OBJ(lab);
}
static void cogito_label_set_text(ErgoVal labelv, ErgoVal textv) {
  if (labelv.tag != EVT_OBJ) ergo_trap("cogito.label_set_text expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  ErgoStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_label_set_wrap(ErgoVal labelv, ErgoVal onv) {
  if (labelv.tag != EVT_OBJ) ergo_trap("cogito.label_set_wrap expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  n->text_wrap = ergo_as_bool(onv);
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_label_set_ellipsis(ErgoVal labelv, ErgoVal onv) {
  if (labelv.tag != EVT_OBJ) ergo_trap("cogito.label_set_ellipsis expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  n->text_ellipsis = ergo_as_bool(onv);
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_label_set_align(ErgoVal labelv, ErgoVal alignv) {
  if (labelv.tag != EVT_OBJ) ergo_trap("cogito.label_set_align expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  n->text_align = (int)ergo_as_int(alignv);
  if (n->text_align < 0) n->text_align = 0;
  if (n->text_align > 2) n->text_align = 2;
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_label_set_class(ErgoVal labelv, ErgoVal classv) {
  if (labelv.tag != EVT_OBJ) ergo_trap("cogito.label_set_class expects label");
  cogito_node_set_class(labelv, classv);
}
