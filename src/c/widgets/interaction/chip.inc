static CogitoNode* cogito_chip_new_obj(ErgoStr* label) {
  CogitoNode* n = cogito_node_new(COGITO_CHIP);
  cogito_node_set_text(n, label);
  return n;
}
static ErgoVal cogito_chip_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* n = cogito_chip_new_obj(ts);
  return EV_OBJ(n);
}
static void cogito_chip_set_selected(ErgoVal chipv, ErgoVal sel) {
  if (chipv.tag != EVT_OBJ) ergo_trap("cogito.chip_set_selected expects chip");
  CogitoNode* n = (CogitoNode*)chipv.as.p;
  n->chip.selected = ergo_as_bool(sel);
  cogito_window_relayout(cogito_node_window(n));
}
static ErgoVal cogito_chip_get_selected(ErgoVal chipv) {
  if (chipv.tag != EVT_OBJ) ergo_trap("cogito.chip_get_selected expects chip");
  CogitoNode* n = (CogitoNode*)chipv.as.p;
  return EV_BOOL(n->chip.selected);
}
static void cogito_chip_set_closable(ErgoVal chipv, ErgoVal closable) {
  if (chipv.tag != EVT_OBJ) ergo_trap("cogito.chip_set_closable expects chip");
  CogitoNode* n = (CogitoNode*)chipv.as.p;
  n->chip.has_close = ergo_as_bool(closable);
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_chip_on_close(ErgoVal chipv, ErgoVal handler) {
  if (chipv.tag != EVT_OBJ) ergo_trap("cogito.chip_on_close expects chip");
  CogitoNode* n = (CogitoNode*)chipv.as.p;
  if (n->on_action) ergo_release_val(EV_FN(n->on_action));
  if (handler.tag == EVT_FN) {
    n->on_action = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(n->on_action));
  } else {
    n->on_action = NULL;
  }
}

static void cogito_chip_close_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!n || n->kind != COGITO_CHIP) return;
  if (!n->chip.has_close) {
    if (out_x) *out_x = 0;
    if (out_y) *out_y = 0;
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    return;
  }
  int icon_size = 12;
  int icon_x = n->x + n->w - 16 - icon_size;  // Right side, matches draw code
  int icon_y = n->y + (n->h - icon_size) / 2;
  if (out_x) *out_x = icon_x;
  if (out_y) *out_y = icon_y;
  if (out_w) *out_w = icon_size;
  if (out_h) *out_h = icon_size;
}
