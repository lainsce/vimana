static CogitoNode* cogito_context_menu_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_CONTEXT_MENU);
  n->hidden = true; // Never rendered directly
  return n;
}

__attribute__((unused))
static YisVal cogito_context_menu_new(void) {
  CogitoNode* n = cogito_context_menu_new_obj();
  return YV_OBJ(n);
}

__attribute__((unused))
static void cogito_context_menu_add_item(YisVal menuv, YisVal label, YisVal handler) {
  if (menuv.tag != EVT_OBJ) yis_trap("cogito.context_menu_add_item expects context menu");
  CogitoNode* n = (CogitoNode*)menuv.as.p;
  YisStr* ts = stdr_to_string(label);
  YisFn* fn = handler.tag == EVT_FN ? (YisFn*)handler.as.p : NULL;
  cogito_node_add_menu(n, ts, fn);
}

__attribute__((unused))
static void cogito_context_menu_add_section(YisVal menuv, YisVal label, YisVal handler) {
  if (menuv.tag != EVT_OBJ) yis_trap("cogito.context_menu_add_section expects context menu");
  CogitoNode* n = (CogitoNode*)menuv.as.p;
  YisStr* ts = stdr_to_string(label);
  YisFn* fn = handler.tag == EVT_FN ? (YisFn*)handler.as.p : NULL;
  cogito_node_add_menu_section(n, ts, fn);
}

__attribute__((unused))
static void cogito_context_menu_set_icon(YisVal menuv, YisVal iconv) {
  if (menuv.tag != EVT_OBJ) yis_trap("cogito.context_menu_set_icon expects context menu");
  CogitoNode* n = (CogitoNode*)menuv.as.p;
  YisStr* ts = stdr_to_string(iconv);
  cogito_node_menu_set_icon(n, ts);
}

__attribute__((unused))
static void cogito_context_menu_set_shortcut(YisVal menuv, YisVal shortcutv) {
  if (menuv.tag != EVT_OBJ) yis_trap("cogito.context_menu_set_shortcut expects context menu");
  CogitoNode* n = (CogitoNode*)menuv.as.p;
  YisStr* ts = stdr_to_string(shortcutv);
  cogito_node_menu_set_shortcut(n, ts);
}

__attribute__((unused))
static void cogito_node_set_context_menu(YisVal nodev, YisVal menuv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_set_context_menu expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  CogitoNode* menu = (menuv.tag == EVT_OBJ) ? (CogitoNode*)menuv.as.p : NULL;
  if (n->context_menu) yis_release_val(YV_OBJ(n->context_menu));
  n->context_menu = menu;
  if (menu) yis_retain_val(YV_OBJ(menu));
}

// Find deepest node with a context_menu that contains (x, y)
static CogitoNode* cogito_find_context_menu_node(CogitoNode* n, int x, int y) {
  if (!n || cogito_node_is_hidden(n)) return NULL;
  if (!cogito_hit_node(n, x, y)) return NULL;
  // Check children deepest-first
  for (size_t i = n->child_count; i > 0; i--) {
    CogitoNode* hit = cogito_find_context_menu_node(n->children[i - 1], x, y);
    if (hit) return hit;
  }
  // Check this node
  if (n->context_menu && n->context_menu->menu_len > 0) return n;
  return NULL;
}
