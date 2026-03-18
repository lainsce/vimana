static void cogito_draw_toasts_overlay(CogitoNode* n) {
  if (!n) return;
  if (cogito_node_is_hidden(n)) return;
  if (n->kind == COGITO_TOASTS) {
    CogitoBinOverlayScopeState bin_scope_state = {0};
    cogito_bin_scope_begin_for_node(n, &bin_scope_state);
    cogito_toasts_force_draw = true;
    cogito_draw_node(n);
    cogito_toasts_force_draw = false;
    cogito_bin_scope_end_for_node(&bin_scope_state);
    return;
  }
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) cogito_draw_toasts_overlay(active);
  } else {
    for (size_t i = 0; i < n->len; i++) {
      cogito_draw_toasts_overlay(n->children[i]);
    }
  }
  if ((n->kind == COGITO_WINDOW || n->kind == COGITO_DIALOG_SLOT) &&
      n->dialog && n->dialog_open) {
    cogito_draw_toasts_overlay(n->dialog);
  }
}

static void cogito_draw_fab_overlay(CogitoNode* n) {
  if (!n) return;
  if (cogito_node_is_hidden(n)) return;
  if (n->kind == COGITO_FAB_MENU) {
    // Only use the overlay when the popup is open, so menu items float above
    // all content. When closed, the FAB draws normally in the regular pass
    // (which is correctly gated by ViewSwitcher, avoiding ghost draws).
    if (n->popover_open && n->menu_len > 0) {
      CogitoBinOverlayScopeState bin_scope_state = {0};
      cogito_bin_scope_begin_for_node(n, &bin_scope_state);
      cogito_fab_menu_force_draw = true;
      cogito_draw_node(n);
      cogito_fab_menu_force_draw = false;
      cogito_bin_scope_end_for_node(&bin_scope_state);
    }
    return;
  }
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) cogito_draw_fab_overlay(active);
  } else {
    for (size_t i = 0; i < n->len; i++) {
      cogito_draw_fab_overlay(n->children[i]);
    }
  }
}

static void cogito_draw_appbar_traffic_overlay(CogitoNode* n) {
  if (!n) return;

  if (n->kind == COGITO_APPBAR) {
    if (n->kd->appbar.btn_close_x >= 0 || n->kd->appbar.btn_min_x >= 0 || n->kd->appbar.btn_max_x >= 0) {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor text = s.has_text ? s.text : n->text_color;
      CogitoColor border = s.has_border ? s.border : n->border_color;

      int btn = n->kd->appbar.btn_size;
      if (btn < 1) btn = 12;
      int by = n->kd->appbar.btn_y;

      int mx = 0, my = 0;
      bool down = false;
      (void)cogito_pointer_query(&mx, &my, &down, NULL);

      CogitoTrafficLightsState lights = {0};
      cogito_traffic_lights_compute(
          n->kd->appbar.btn_close_x, n->kd->appbar.btn_min_x, n->kd->appbar.btn_max_x,
          by, btn, mx, my, down, &lights);

      CogitoColor fallback_btn = s.has_selection ? s.selection : n->selection_color;
      fallback_btn = cogito_bin_scope_remap_color(fallback_btn);
      CogitoColor fallback_border = border;
      if (!s.has_border) fallback_border = text;
      if (fallback_border.a > 80) fallback_border.a = 80;

      CogitoBinOverlayScopeState bin_scope_state = {0};
      cogito_bin_scope_begin_for_node(n, &bin_scope_state);
      cogito_draw_traffic_lights_styled(
          &s, fallback_btn, fallback_border,
          n->kd->appbar.btn_close_x, n->kd->appbar.btn_min_x, n->kd->appbar.btn_max_x,
          by, btn, &lights, false);
      cogito_bin_scope_end_for_node(&bin_scope_state);
    }
    return;
  }

  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_appbar_traffic_overlay(n->children[i]);
  }

  if ((n->kind == COGITO_WINDOW || n->kind == COGITO_DIALOG_SLOT) &&
      n->dialog && n->dialog_open) {
    cogito_draw_appbar_traffic_overlay(n->dialog);
  }
}
