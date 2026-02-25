static void cogito_toast_close_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!n || n->kind != COGITO_TOAST) return;
  CogitoStyle s = cogito_theme_resolve_node(n);
  int icon = s.has_icon_size ? s.icon_size : 12;
  if (icon < 8) icon = 8;
  int box = icon + 8;
  int pad_x = 12;
  int x = n->x + n->w - pad_x - box;
  int y = n->y + (n->h - box) / 2;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = box;
  if (out_h) *out_h = box;
}
static CogitoNode* cogito_toast_new_obj(ErgoStr* text) {
  CogitoNode* t = cogito_node_new(COGITO_TOAST);
  t->text = text;
  if (t->text) ergo_retain_val(EV_STR(t->text));
  return t;
}
static ErgoVal cogito_toast_new(ErgoVal textv) {
  ErgoStr* s = stdr_to_string(textv);
  return EV_OBJ(cogito_toast_new_obj(s));
}
static void cogito_toast_set_text(ErgoVal tv, ErgoVal textv) {
  if (tv.tag != EVT_OBJ) ergo_trap("cogito.toast_set_text expects toast");
  CogitoNode* n = (CogitoNode*)tv.as.p;
  ErgoStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_toast_on_click(ErgoVal tv, ErgoVal handler) {
  if (tv.tag != EVT_OBJ) ergo_trap("cogito.toast_on_click expects toast");
  CogitoNode* t = (CogitoNode*)tv.as.p;
  if (t->on_click) ergo_release_val(EV_FN(t->on_click));
  t->on_click = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(t->on_click));
}
static void cogito_toast_set_action(ErgoVal tv, ErgoVal textv, ErgoVal handler) {
  if (tv.tag != EVT_OBJ) ergo_trap("cogito.toast_set_action expects toast");
  CogitoNode* n = (CogitoNode*)tv.as.p;
  ErgoStr* ts = stdr_to_string(textv);
  if (n->action_text) ergo_release_val(EV_STR(n->action_text));
  n->action_text = ts;
  ergo_retain_val(EV_STR(n->action_text));
  if (n->on_action) ergo_release_val(EV_FN(n->on_action));
  if (handler.tag == EVT_FN) {
    n->on_action = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(n->on_action));
  } else {
    n->on_action = NULL;
  }
  cogito_window_relayout(cogito_node_window(n));
}
