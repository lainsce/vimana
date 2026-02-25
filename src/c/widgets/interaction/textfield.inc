static CogitoNode* cogito_textfield_new_obj(ErgoStr* text) {
  CogitoNode* tf = cogito_node_new(COGITO_TEXTFIELD);
  // Treat constructor text as placeholder; value starts empty.
  cogito_node_set_subtitle(tf, text);
  cogito_node_set_text(tf, NULL);
  return tf;
}
static ErgoVal cogito_textfield_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* tf = cogito_textfield_new_obj(ts);
  return EV_OBJ(tf);
}
static void cogito_textfield_set_text(ErgoVal nodev, ErgoVal textv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textfield_set_text expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  cogito_window_relayout(cogito_node_window(n));
}
static ErgoVal cogito_textfield_get_text(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textfield_get_text expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* s = n->text ? n->text : stdr_str_lit("");
  ergo_retain_val(EV_STR(s));
  return EV_STR(s);
}
static void cogito_textfield_set_hint(ErgoVal nodev, ErgoVal hintv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textfield_set_hint expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* hs = stdr_to_string(hintv);
  cogito_node_set_hint(n, hs);
  cogito_window_relayout(cogito_node_window(n));
}
static ErgoVal cogito_textfield_get_hint(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textfield_get_hint expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* s = n->hint ? n->hint : stdr_str_lit("");
  ergo_retain_val(EV_STR(s));
  return EV_STR(s);
}
static void cogito_textfield_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textfield_on_change expects textfield");
  if (handler.tag != EVT_FN) ergo_trap("cogito.textfield_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
static CogitoNode* cogito_find_text_input(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_WINDOW && n->popover_open && n->popover) {
    CogitoNode* pop_hit = cogito_find_text_input(n->popover, x, y);
    if (pop_hit) return pop_hit;
  }
  if (n->kind == COGITO_DIALOG_SLOT && n->dialog && n->dialog_open) {
    return cogito_find_text_input(n->dialog, x, y);
  }
  if (n->kind == COGITO_DIALOG && !cogito_hit_node(n, x, y)) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* hit = cogito_find_text_input(active, x, y);
      if (hit) return hit;
    }
  } else {
    for (size_t i = n->len; i > 0; i--) {
      CogitoNode* hit = cogito_find_text_input(n->children[i - 1], x, y);
      if (hit) return hit;
    }
  }
  if ((n->kind == COGITO_TEXTFIELD || n->kind == COGITO_TEXTVIEW || n->kind == COGITO_SEARCHFIELD) &&
      n->editable &&
      cogito_hit_node_min48(n, x, y)) return n;
  if (n->kind == COGITO_COLORPICKER) {
    int hx = 0, hy = 0, hw = 0, hh = 0;
    cogito_colorpicker_hex_rect(n, &hx, &hy, &hw, &hh);
    if (cogito_hit_rect_min48(x, y, hx, hy, hw, hh)) return n;
  }
  return NULL;
}
