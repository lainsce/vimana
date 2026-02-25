static CogitoNode* cogito_fab_new_obj(ErgoStr* icon) {
  CogitoNode* n = cogito_node_new(COGITO_FAB);
  n->align = 2;
  cogito_node_set_text(n, icon);  // Icon name
  return n;
}
static ErgoVal cogito_fab_new(ErgoVal icon) {
  ErgoStr* ts = stdr_to_string(icon);
  CogitoNode* n = cogito_fab_new_obj(ts);
  return EV_OBJ(n);
}
// size: 0=S(42x42), 1=M(56x56), 2=L(96x96)
static void cogito_fab_set_size(ErgoVal fabv, ErgoVal sizev) {
  if (fabv.tag != EVT_OBJ) ergo_trap("cogito.fab_set_size expects fab");
  CogitoNode* n = (CogitoNode*)fabv.as.p;
  int s = (int)ergo_as_int(sizev);
  if (s < 0) s = 0;
  if (s > 2) s = 2;
  n->fab.size = (uint8_t)s;
  cogito_window_relayout(cogito_node_window(n));
}

// color: 0=Primary, 1=Secondary, 2=Tertiary, 3=Surface
static void cogito_fab_set_color(ErgoVal fabv, ErgoVal colorv) {
  if (fabv.tag != EVT_OBJ) ergo_trap("cogito.fab_set_color expects fab");
  CogitoNode* n = (CogitoNode*)fabv.as.p;
  static const char* names[] = { "primary", "secondary", "tertiary", "surface" };
  int c = (int)ergo_as_int(colorv);
  if (c < 0) c = 0;
  if (c > 3) c = 3;
  ErgoStr* cls = stdr_str_from_slice(names[c], strlen(names[c]));
  if (n->class_name) ergo_release_val(EV_STR(n->class_name));
  n->class_name = cls;
  ergo_retain_val(EV_STR(n->class_name));
}

static void cogito_fab_menu_set_color(ErgoVal fabv, ErgoVal colorv) {
  if (fabv.tag != EVT_OBJ) ergo_trap("cogito.fab_menu_set_color expects fab_menu");
  CogitoNode* n = (CogitoNode*)fabv.as.p;
  static const char* names[] = { "primary", "secondary", "tertiary", "surface" };
  int c = (int)ergo_as_int(colorv);
  if (c < 0) c = 0;
  if (c > 3) c = 3;
  ErgoStr* cls = stdr_str_from_slice(names[c], strlen(names[c]));
  if (n->class_name) ergo_release_val(EV_STR(n->class_name));
  n->class_name = cls;
  ergo_retain_val(EV_STR(n->class_name));
}

static void cogito_fab_set_extended(ErgoVal fabv, ErgoVal extended, ErgoVal label) {
  if (fabv.tag != EVT_OBJ) ergo_trap("cogito.fab_set_extended expects fab");
  CogitoNode* n = (CogitoNode*)fabv.as.p;
  n->fab.extended = ergo_as_bool(extended);
  if (n->fab.extended) {
    ErgoStr* lbl = stdr_to_string(label);
    if (n->subtitle) ergo_release_val(EV_STR(n->subtitle));
    n->subtitle = lbl;
    ergo_retain_val(EV_STR(n->subtitle));
  }
  cogito_window_relayout(cogito_node_window(n));
}
