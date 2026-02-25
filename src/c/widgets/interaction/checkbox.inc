static CogitoNode* cogito_checkbox_new_obj(YisStr* label, YisVal group) {
  CogitoNode* cb = cogito_node_new(COGITO_CHECKBOX);
  cogito_node_set_text(cb, label);
  cogito_node_set_group(cb, group);
  return cb;
}
static YisVal cogito_checkbox_new(YisVal text, YisVal group) {
  YisStr* ts = stdr_to_string(text);
  CogitoNode* cb = cogito_checkbox_new_obj(ts, group);
  return YV_OBJ(cb);
}
static void cogito_checkbox_set_checked(YisVal cbv, YisVal checked) {
  if (cbv.tag != EVT_OBJ) yis_trap("cogito.checkbox_set_checked expects checkbox");
  CogitoNode* cb = (CogitoNode*)cbv.as.p;
  cb->checked = yis_as_bool(checked);
}
static YisVal cogito_checkbox_get_checked(YisVal cbv) {
  if (cbv.tag != EVT_OBJ) yis_trap("cogito.checkbox_get_checked expects checkbox");
  CogitoNode* cb = (CogitoNode*)cbv.as.p;
  return YV_BOOL(cb->checked);
}
static void cogito_checkbox_on_change(YisVal cbv, YisVal handler) {
  if (cbv.tag != EVT_OBJ) yis_trap("cogito.checkbox_on_change expects checkbox");
  CogitoNode* cb = (CogitoNode*)cbv.as.p;
  if (cb->on_change) yis_release_val(YV_FN(cb->on_change));
  cb->on_change = (YisFn*)handler.as.p;
  yis_retain_val(YV_FN(cb->on_change));
}
static void cogito_uncheck_group(CogitoNode* root, CogitoNode* except, YisVal group) {
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
