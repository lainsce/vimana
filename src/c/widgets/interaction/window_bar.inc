static CogitoNode* cogito_window_bar_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_WINDOW_BAR);
  cogito_node_add_menu(n, stdr_str_from_slice("Screenshot", 10), NULL);
  cogito_node_menu_set_icon(n, stdr_str_from_slice("sf:camera", 9));
  cogito_node_menu_set_shortcut(n, stdr_str_from_slice("\xe2\x87\xa7\xe2\x8c\x98S", 7));
  cogito_node_add_menu_section(n, stdr_str_from_slice("Inspect", 7), NULL);
  cogito_node_menu_set_icon(n, stdr_str_from_slice("sf:hammer", 9));
  cogito_node_menu_set_shortcut(n, stdr_str_from_slice("\xe2\x8c\xa5\xe2\x8c\x98I", 7));
  cogito_node_add_menu(n, stdr_str_from_slice("Minimize", 8), NULL);
  cogito_node_menu_set_icon(n, stdr_str_from_slice("sf:minus", 8));
  cogito_node_menu_set_shortcut(n, stdr_str_from_slice("\xe2\x8c\x98M", 4));
  cogito_node_add_menu(n, stdr_str_from_slice("Maximize", 8), NULL);
  cogito_node_menu_set_icon(n, stdr_str_from_slice("sf:arrow.up.left.and.arrow.down.right", 37));
  cogito_node_add_menu_section(n, stdr_str_from_slice("Close", 5), NULL);
  cogito_node_menu_set_icon(n, stdr_str_from_slice("sf:xmark", 8));
  cogito_node_menu_set_shortcut(n, stdr_str_from_slice("\xe2\x8c\x98W", 4));
  return n;
}

static YisVal cogito_window_bar_new(void) {
  return YV_OBJ(cogito_window_bar_new_obj());
}

// Built-in menu item handler for WindowBar (indices match creation order above)
enum {
  COGITO_WINDOW_BAR_SCREENSHOT = 0,
  COGITO_WINDOW_BAR_INSPECT    = 1,
  COGITO_WINDOW_BAR_MINIMIZE   = 2,
  COGITO_WINDOW_BAR_MAXIMIZE   = 3,
  COGITO_WINDOW_BAR_CLOSE      = 4
};

static void cogito_window_bar_screenshot(CogitoNode* win_node) {
  if (!win_node || !cogito_backend || !cogito_active_backend_window) return;
  CogitoWindow* bw = cogito_active_backend_window;
  if (cogito_backend->window_screenshot)
    cogito_backend->window_screenshot(bw);
}

static void cogito_window_bar_invoke_menu(CogitoNode* n, size_t idx) {
  if (!n) return;
  CogitoNode* win = n;
  while (win && win->kind != COGITO_WINDOW) win = win->parent;
  if (!win) return;
  switch (idx) {
    case COGITO_WINDOW_BAR_SCREENSHOT:
      cogito_window_bar_screenshot(win);
      break;
    case COGITO_WINDOW_BAR_INSPECT:
      if (cogito_inspector_enabled) {
        cogito_inspector_open = !cogito_inspector_open;
      }
      break;
    case COGITO_WINDOW_BAR_MINIMIZE:
      cogito_appbar_window_action(win, 1);
      break;
    case COGITO_WINDOW_BAR_MAXIMIZE:
      cogito_appbar_window_action(win, 2);
      break;
    case COGITO_WINDOW_BAR_CLOSE:
      cogito_appbar_window_action(win, 0);
      break;
    default:
      break;
  }
}

static CogitoNode* cogito_find_window_bar(CogitoNode* win) {
  if (!win) return NULL;
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i] && win->children[i]->kind == COGITO_WINDOW_BAR)
      return win->children[i];
  }
  return NULL;
}
