static CogitoNode* cogito_fab_new_obj(YisStr* icon) {
  CogitoNode* n = cogito_node_new(COGITO_FAB);
  n->align = 2;
  cogito_node_set_text(n, icon);  // Icon name
  return n;
}
static YisVal cogito_fab_new(YisVal icon) {
  YisStr* ts = stdr_to_string(icon);
  CogitoNode* n = cogito_fab_new_obj(ts);
  return EV_OBJ(n);
}
// size: 0=S(42x42), 1=M(56x56), 2=L(96x96)
static void cogito_fab_set_size(YisVal fabv, YisVal sizev) {
  if (fabv.tag != EVT_OBJ) yis_trap("cogito.fab_set_size expects fab");
  CogitoNode* n = (CogitoNode*)fabv.as.p;
  int s = (int)yis_as_int(sizev);
  if (s < 0) s = 0;
  if (s > 2) s = 2;
  n->fab.size = (uint8_t)s;
  cogito_window_relayout(cogito_node_window(n));
}

// color: 0=Primary, 1=Secondary, 2=Tertiary, 3=Surface
static void cogito_fab_set_color(YisVal fabv, YisVal colorv) {
  if (fabv.tag != EVT_OBJ) yis_trap("cogito.fab_set_color expects fab");
  CogitoNode* n = (CogitoNode*)fabv.as.p;
  static const char* names[] = { "primary", "secondary", "tertiary", "surface" };
  int c = (int)yis_as_int(colorv);
  if (c < 0) c = 0;
  if (c > 3) c = 3;
  YisStr* cls = stdr_str_from_slice(names[c], strlen(names[c]));
  if (n->class_name) yis_release_val(EV_STR(n->class_name));
  n->class_name = cls;
  yis_retain_val(EV_STR(n->class_name));
}

static void cogito_fab_menu_set_color(YisVal fabv, YisVal colorv) {
  if (fabv.tag != EVT_OBJ) yis_trap("cogito.fab_menu_set_color expects fab_menu");
  CogitoNode* n = (CogitoNode*)fabv.as.p;
  static const char* names[] = { "primary", "secondary", "tertiary", "surface" };
  int c = (int)yis_as_int(colorv);
  if (c < 0) c = 0;
  if (c > 3) c = 3;
  YisStr* cls = stdr_str_from_slice(names[c], strlen(names[c]));
  if (n->class_name) yis_release_val(EV_STR(n->class_name));
  n->class_name = cls;
  yis_retain_val(EV_STR(n->class_name));
}

static void cogito_fab_set_extended(YisVal fabv, YisVal extended, YisVal label) {
  if (fabv.tag != EVT_OBJ) yis_trap("cogito.fab_set_extended expects fab");
  CogitoNode* n = (CogitoNode*)fabv.as.p;
  n->fab.extended = yis_as_bool(extended);
  if (n->fab.extended) {
    YisStr* lbl = stdr_to_string(label);
    if (n->subtitle) yis_release_val(EV_STR(n->subtitle));
    n->subtitle = lbl;
    yis_retain_val(EV_STR(n->subtitle));
  }
  cogito_window_relayout(cogito_node_window(n));
}
