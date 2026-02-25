static CogitoNode* cogito_checkbox_new_obj(ErgoStr* label, ErgoVal group) {
  CogitoNode* cb = cogito_node_new(COGITO_CHECKBOX);
  cogito_node_set_text(cb, label);
  cogito_node_set_group(cb, group);
  return cb;
}
static ErgoVal cogito_checkbox_new(ErgoVal text, ErgoVal group) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* cb = cogito_checkbox_new_obj(ts, group);
  return EV_OBJ(cb);
}
static void cogito_checkbox_set_checked(ErgoVal cbv, ErgoVal checked) {
  if (cbv.tag != EVT_OBJ) ergo_trap("cogito.checkbox_set_checked expects checkbox");
  CogitoNode* cb = (CogitoNode*)cbv.as.p;
  cb->checked = ergo_as_bool(checked);
}
static ErgoVal cogito_checkbox_get_checked(ErgoVal cbv) {
  if (cbv.tag != EVT_OBJ) ergo_trap("cogito.checkbox_get_checked expects checkbox");
  CogitoNode* cb = (CogitoNode*)cbv.as.p;
  return EV_BOOL(cb->checked);
}
static void cogito_checkbox_on_change(ErgoVal cbv, ErgoVal handler) {
  if (cbv.tag != EVT_OBJ) ergo_trap("cogito.checkbox_on_change expects checkbox");
  CogitoNode* cb = (CogitoNode*)cbv.as.p;
  if (cb->on_change) ergo_release_val(EV_FN(cb->on_change));
  cb->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(cb->on_change));
}
static void cogito_uncheck_group(CogitoNode* root, CogitoNode* except, ErgoVal group) {
  if (!root) return;
  if ((root->kind == COGITO_CHECKBOX || root->kind == COGITO_SWITCH) && root != except) {
    if (group.tag != EVT_NULL && cogito_group_match(root->group, group)) {
      root->checked = false;
      root->bg_set = false;
      root->border_color_set = false;
      root->border_width_set = false;
    }
  }
  for (size_t i = 0; i < root->len; i++) {
    cogito_uncheck_group(root->children[i], except, group);
  }
}
