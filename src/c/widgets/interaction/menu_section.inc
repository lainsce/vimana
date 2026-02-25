// MenuSection widget interaction
// A menu section that can be used to group menu items with intelligent dividers

static CogitoNode* cogito_menu_section_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_MENU_SECTION);
  n->menu_section.has_divider_before = false;
  n->menu_section.has_divider_after = false;
  return n;
}

__attribute__((unused))
static ErgoVal cogito_menu_section_new(ErgoVal beforev, ErgoVal afterv) {
  CogitoNode* n = cogito_menu_section_new_obj();
  if (beforev.tag != EVT_NULL) {
    n->menu_section.has_divider_before = ergo_as_bool(beforev);
  }
  if (afterv.tag != EVT_NULL) {
    n->menu_section.has_divider_after = ergo_as_bool(afterv);
  }
  return EV_OBJ(n);
}

void cogito_menu_section_set_divider_before(ErgoVal nv, ErgoVal beforev) {
  if (nv.tag != EVT_OBJ) ergo_trap("cogito.menu_section_set_divider_before expects menu_section");
  CogitoNode* n = (CogitoNode*)nv.as.p;
  if (n->kind != COGITO_MENU_SECTION) ergo_trap("cogito.menu_section_set_divider_before expects menu_section");
  n->menu_section.has_divider_before = ergo_as_bool(beforev);
  cogito_window_relayout(cogito_node_window(n));
}

ErgoVal cogito_menu_section_get_divider_before(ErgoVal nv) {
  if (nv.tag != EVT_OBJ) ergo_trap("cogito.menu_section_get_divider_before expects menu_section");
  CogitoNode* n = (CogitoNode*)nv.as.p;
  if (n->kind != COGITO_MENU_SECTION) ergo_trap("cogito.menu_section_get_divider_before expects menu_section");
  return EV_BOOL(n->menu_section.has_divider_before);
}

void cogito_menu_section_set_divider_after(ErgoVal nv, ErgoVal afterv) {
  if (nv.tag != EVT_OBJ) ergo_trap("cogito.menu_section_set_divider_after expects menu_section");
  CogitoNode* n = (CogitoNode*)nv.as.p;
  if (n->kind != COGITO_MENU_SECTION) ergo_trap("cogito.menu_section_set_divider_after expects menu_section");
  n->menu_section.has_divider_after = ergo_as_bool(afterv);
  cogito_window_relayout(cogito_node_window(n));
}

ErgoVal cogito_menu_section_get_divider_after(ErgoVal nv) {
  if (nv.tag != EVT_OBJ) ergo_trap("cogito.menu_section_get_divider_after expects menu_section");
  CogitoNode* n = (CogitoNode*)nv.as.p;
  if (n->kind != COGITO_MENU_SECTION) ergo_trap("cogito.menu_section_get_divider_after expects menu_section");
  return EV_BOOL(n->menu_section.has_divider_after);
}
// Add a menu item that starts a new section (with divider before it)
void cogito_menu_add_section_item(CogitoNode* n, ErgoStr* label, ErgoFn* handler) {
  if (!n) return;
  // Mark the last item as having a section divider after it
  if (n->menu_len > 0) {
    n->menu_section_after[n->menu_len - 1
    ] = true;
  }
  // Now add the new item
  cogito_node_add_menu(n, label, handler);
}