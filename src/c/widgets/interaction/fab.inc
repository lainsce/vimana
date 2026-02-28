static CogitoNode* cogito_fab_new_obj(YisStr* icon) {
  CogitoNode* n = cogito_node_new(COGITO_FAB);
  n->align = 2;
  cogito_node_set_text(n, icon);  // Icon name
  return n;
}
static YisVal cogito_fab_new(YisVal icon) {
  YisStr* ts = stdr_to_string(icon);
  CogitoNode* n = cogito_fab_new_obj(ts);
  return YV_OBJ(n);
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

// color: 0=Primary, 1=Secondary, 2=Tertiary, 3=PrimaryContainer, 4=SecondaryContainer, 5=TertiaryContainer
static void cogito_fab_set_color(YisVal fabv, YisVal colorv) {
  if (fabv.tag != EVT_OBJ) yis_trap("cogito.fab_set_color expects fab");
  CogitoNode* n = (CogitoNode*)fabv.as.p;
  int c = (int)yis_as_int(colorv);
  if (c < 0) c = 0;
  if (c > 5) c = 5;
  n->fab.variant = (uint8_t)c;
}

// color: 0=Primary, 1=Secondary, 2=Tertiary
static void cogito_fab_menu_set_color(YisVal fabv, YisVal colorv) {
  if (fabv.tag != EVT_OBJ) yis_trap("cogito.fab_menu_set_color expects fab_menu");
  CogitoNode* n = (CogitoNode*)fabv.as.p;
  int c = (int)yis_as_int(colorv);
  if (c < 0) c = 0;
  if (c > 2) c = 2;
  n->fab_menu.variant = (uint8_t)c;
}

// size: 0=S(56x56), 1=M(80x80), 2=L(96x96)
static void cogito_fab_menu_set_size(YisVal fabv, YisVal sizev) {
  if (fabv.tag != EVT_OBJ) yis_trap("cogito.fab_menu_set_size expects fab_menu");
  CogitoNode* n = (CogitoNode*)fabv.as.p;
  int s = (int)yis_as_int(sizev);
  if (s < 0) s = 0;
  if (s > 2) s = 2;
  n->fab_menu.size = (uint8_t)s;
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_fab_set_extended(YisVal fabv, YisVal extended, YisVal label) {
  if (fabv.tag != EVT_OBJ) yis_trap("cogito.fab_set_extended expects fab");
  CogitoNode* n = (CogitoNode*)fabv.as.p;
  n->fab.extended = yis_as_bool(extended);
  if (n->fab.extended) {
    YisStr* lbl = stdr_to_string(label);
    if (n->subtitle) yis_release_val(YV_STR(n->subtitle));
    n->subtitle = lbl;
    yis_retain_val(YV_STR(n->subtitle));
  }
  cogito_window_relayout(cogito_node_window(n));
}
