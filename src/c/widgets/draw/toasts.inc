static void cogito_draw_toasts_overlay(CogitoNode* n) {
  if (!n) return;
  if (n->kind == COGITO_TOASTS) {
    cogito_toasts_force_draw = true;
    cogito_draw_node(n);
    cogito_toasts_force_draw = false;
    return;
  }
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_toasts_overlay(n->children[i]);
  }
  if ((n->kind == COGITO_WINDOW || n->kind == COGITO_DIALOG_SLOT) &&
      n->dialog && n->dialog_open) {
    cogito_draw_toasts_overlay(n->dialog);
  }
}

static void cogito_draw_fab_overlay(CogitoNode* n) {
  if (!n) return;
  if (n->kind == COGITO_FAB_MENU) {
    // Only use the overlay when the popup is open, so menu items float above
    // all content. When closed, the FAB draws normally in the regular pass
    // (which is correctly gated by ViewSwitcher, avoiding ghost draws).
    if (n->popover_open && n->menu_len > 0) {
      cogito_fab_menu_force_draw = true;
      cogito_draw_node(n);
      cogito_fab_menu_force_draw = false;
    }
    return;
  }
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_fab_overlay(n->children[i]);
  }
}
