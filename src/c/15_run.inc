#include <SDL3/SDL.h>

static CogitoNode* cogito_find_stepper_hit(CogitoNode* n, int x, int y);

static const int COGITO_VIEW_DUAL_DRAG_COMMIT_DP = 8;
static const int COGITO_VIEW_DUAL_MIN_ITEM_W = 360;
static const int COGITO_VIEW_DUAL_DETENTE_W_1 = 360;
static const int COGITO_VIEW_DUAL_DETENTE_W_2 = 412;
static const int COGITO_VIEW_DUAL_DETENTE_TOLERANCE = 24;
static const int COGITO_VIEW_DUAL_HIDE_INTENT_FROM_DETENTE = 72;

#include "widgets/util/text_editing.inc"

static bool cogito_about_window_is_actions_row(const CogitoNode* n) {
  return n && n->kind == COGITO_HSTACK &&
         n->class_name && n->class_name->data &&
         strcmp(n->class_name->data,
  "about-window-actions") == 0;
}

static CogitoNode* cogito_about_window_for_action_button(CogitoNode* btn) {
  if (!btn || btn->kind != COGITO_BUTTON) return NULL;
  CogitoNode* row = btn->parent;
  if (!cogito_about_window_is_actions_row(row)) return NULL;
  CogitoNode* aw = row->parent;
  if (!aw || aw->kind != COGITO_ABOUT_WINDOW) return NULL;
  return aw;
}

static const char* cogito_about_window_action_url(CogitoNode* btn, CogitoNode* aw) {
  if (btn && btn->action_text && btn->action_text->data && btn->action_text->data[
    0
  ]) {
    return btn->action_text->data;
  }
  if (aw && aw->action_text && aw->action_text->data && aw->action_text->data[
    0
  ]) {
    return aw->action_text->data;
  }
  return NULL;
}

static bool cogito_node_is_or_descends_from_kind(CogitoNode* n, CogitoKind kind) {
  for (CogitoNode* p = n; p; p = p->parent) {
    if (p->kind == kind) return true;
  }
  return false;
}

static bool cogito_focus_is_text_editing(CogitoNode* n) {
  if (!n || n->disabled) return false;
  if ((n->kind == COGITO_TEXTFIELD || n->kind == COGITO_TEXTVIEW || n->kind == COGITO_SEARCHFIELD) &&
      n->editable) {
    return true;
  }
  return n->kind == COGITO_COLORPICKER && n->kd->colorpicker.hex_editing;
}

static bool cogito_point_hits_preferred_handle(CogitoNode* n, int mx, int my) {
  if (!n) return false;

  if (n->kind == COGITO_DIALOG_SLOT) {
    if (n->dialog && n->dialog_open) {
      if (cogito_point_hits_preferred_handle(n->dialog, mx, my)) return true;
    }
  } else if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active && cogito_point_hits_preferred_handle(active, mx, my)) return true;
  } else if (!(n->kind == COGITO_REVEALER && !n->revealer_show_children)) {
    for (size_t i = 0; i < n->len; i++) {
      if (cogito_point_hits_preferred_handle(n->children[i], mx, my)) return true;
    }
  }

  if (n->kind == COGITO_VIEW_DUAL) {
    float ratio = cogito_view_dual_current_ratio(n);
    if (cogito_view_dual_hit_handle_intent_for_ratio(n, ratio, mx, my)) {
      return true;
    }
  }

  if (n->kind == COGITO_BOTTOM_SHEET) {
    int hx = 0, hy = 0, hw = 0, hh = 0;
    cogito_bottom_sheet_handle_rect(n, &hx, &hy, &hw, &hh);
    if (cogito_hit_rect(mx, my, hx, hy, hw, hh)) return true;
  }

  if (n->kind == COGITO_SIDE_SHEET) {
    int hx = 0, hy = 0, hw = 0, hh = 0;
    cogito_side_sheet_handle_rect(n, &hx, &hy, &hw, &hh);
    if (cogito_hit_rect(mx, my, hx, hy, hw, hh)) return true;
  }

  return false;
}

static bool cogito_keyboard_toggle_focus(CogitoNode* n) {
  if (!n || n->disabled) return false;
  if (n->kind != COGITO_CHECKBOX && n->kind != COGITO_SWITCH && n->kind != COGITO_SWITCHBAR) return false;

  n->checked = !n->checked;
  if (n->kind == COGITO_CHECKBOX && n->group.tag != EVT_NULL) {
    if (n->checked) {
      CogitoNode* root = cogito_root(n);
      if (root) {
        for (size_t i = 0; i < root->len; i++) {
          CogitoNode* sib = root->children[i];
          if (!sib || sib == n) continue;
          if (sib->kind == COGITO_CHECKBOX && sib->group.tag == n->group.tag) {
            sib->checked = false;
          }
        }
      }
    }
  }
  cogito_invoke_change(n);
  return true;
}

static bool cogito_keyboard_adjust_slider(CogitoNode* n, int dir) {
  if (!n || n->kind != COGITO_SLIDER || n->disabled || dir == 0) return false;
  if (cogito_is_rtl()) dir = -dir;
  double span = n->kd->slider.max - n->kd->slider.min;
  double step = cogito_slider_step_normalized(n->kd->slider.step);
  if (step < 0.000001) {
    step = span / 20.0;
    if (step < 0.000001) step = 1.0;
  }
  double prev0 = n->kd->slider.value;
  double prev1 = n->kd->slider.value2;
  if (n->kd->slider.mode == COGITO_SLIDER_MODE_RANGE) {
    if (n->kd->slider.active_handle == 2) {
      n->kd->slider.value2 = cogito_slider_snap_to_step(n, n->kd->slider.value2 + (double)dir * step);
      if (n->kd->slider.value2 < n->kd->slider.value) n->kd->slider.value = n->kd->slider.value2;
    } else {
      n->kd->slider.active_handle = 1;
      n->kd->slider.value = cogito_slider_snap_to_step(n, n->kd->slider.value + (double)dir * step);
      if (n->kd->slider.value > n->kd->slider.value2) n->kd->slider.value2 = n->kd->slider.value;
    }
  } else {
    n->kd->slider.value = cogito_slider_snap_to_step(n, n->kd->slider.value + (double)dir * step);
    n->kd->slider.value2 = n->kd->slider.value;
  }
  if (n->kd->slider.value == prev0 && n->kd->slider.value2 == prev1) return false;
  cogito_invoke_change(n);
  return true;
}

static int cogito_slider_rail_h_for_size(uint8_t size) {
  int rail_h = 16;
  if (size == COGITO_SLIDER_SIZE_S) rail_h = 24;
  if (size == COGITO_SLIDER_SIZE_M) rail_h = 40;
  if (size == COGITO_SLIDER_SIZE_L) rail_h = 56;
  if (size == COGITO_SLIDER_SIZE_XL) rail_h = 96;
  return rail_h;
}

static uint8_t cogito_slider_pick_range_handle(CogitoNode* sld, int mx, int my) {
  if (!sld || sld->kind != COGITO_SLIDER || sld->kd->slider.mode != COGITO_SLIDER_MODE_RANGE) {
    return 0;
  }

  double span = sld->kd->slider.max - sld->kd->slider.min;
  int rail_h = cogito_slider_rail_h_for_size(sld->kd->slider.size);
  int pad = rail_h / 2 + 7;
  int x0 = sld->x + pad;
  int x1 = sld->x + sld->w - pad;
  if (x1 < x0) x1 = x0;
  int cy = sld->y + sld->h / 2;

  double t0 = 0.0;
  double t1 = 0.0;
  if (span > 0.0) {
    t0 = (sld->kd->slider.value - sld->kd->slider.min) / span;
    t1 = (sld->kd->slider.value2 - sld->kd->slider.min) / span;
  }
  if (t0 < 0.0) t0 = 0.0;
  if (t0 > 1.0) t0 = 1.0;
  if (t1 < 0.0) t1 = 0.0;
  if (t1 > 1.0) t1 = 1.0;
  if (cogito_is_rtl()) { t0 = 1.0 - t0; t1 = 1.0 - t1; }

  int k0 = x0 + (int)lround((double)(x1 - x0) * t0);
  int k1 = x0 + (int)lround((double)(x1 - x0) * t1);
  int d0 = abs(mx - k0);
  int d1 = abs(mx - k1);

  int hit_half_w = rail_h / 2;
  if (hit_half_w < 12) hit_half_w = 12;
  int hit_half_h = rail_h / 2 + 8;
  bool hit0 = (abs(mx - k0) <= hit_half_w) && (abs(my - cy) <= hit_half_h);
  bool hit1 = (abs(mx - k1) <= hit_half_w) && (abs(my - cy) <= hit_half_h);

  if (hit0 && !hit1) return 1;
  if (hit1 && !hit0) return 2;

  if (d1 < d0) return 2;
  if (d0 < d1) return 1;

  if (k0 == k1) {
    // When both thumbs overlap, alternate so either handle can be pulled apart.
    return (sld->kd->slider.active_handle == 2) ? 1 : 2;
  }
  return (mx >= (k0 + k1) / 2) ? 2 : 1;
}

static bool cogito_keyboard_adjust_stepper(CogitoNode* n, int dir) {
  if (!n || n->kind != COGITO_STEPPER || n->disabled || dir == 0) return false;
  double prev = n->kd->stepper.value;
  n->kd->stepper.value += (double)dir * n->kd->stepper.step;
  if (n->kd->stepper.value < n->kd->stepper.min) n->kd->stepper.value = n->kd->stepper.min;
  if (n->kd->stepper.value > n->kd->stepper.max) n->kd->stepper.value = n->kd->stepper.max;
  if (n->kd->stepper.value == prev) return false;
  cogito_invoke_change(n);
  return true;
}

static bool cogito_keyboard_move_indexed_selection(CogitoNode* n, int dir) {
  if (!n || n->disabled || dir == 0) return false;
  switch (n->kind) {
    case COGITO_TABS: {
      if (n->tab_len == 0) return false;
      int prev = n->selected;
      int next = prev < 0 ? 0 : prev + dir;
      if (next < 0) next = 0;
      if (next >= (int)n->tab_len) next = (int)n->tab_len - 1;
      if (next == prev) return false;
      cogito_tabs_set_selected(YV_OBJ(n), YV_INT(next));
      if (n->on_select) {
        YisVal arg = YV_OBJ(n);
        cogito_invoke(n->on_select, 1, &arg);
      }
      return true;
    }
    case COGITO_NAV_RAIL: {
      if (n->kd->nav_rail.len == 0) return false;
      int prev = n->kd->nav_rail.selected;
      int next = prev + dir;
      if (next < 0) next = 0;
      if (next >= (int)n->kd->nav_rail.len) next = (int)n->kd->nav_rail.len - 1;
      if (next == prev) return false;
      cogito_handle_nav_rail_select(n, next);
      return true;
    }
    case COGITO_BOTTOM_NAV: {
      if (n->kd->bottom_nav.len == 0) return false;
      int prev = n->kd->bottom_nav.selected;
      int next = prev + dir;
      if (next < 0) next = 0;
      if (next >= (int)n->kd->bottom_nav.len) next = (int)n->kd->bottom_nav.len - 1;
      if (next == prev) return false;
      cogito_handle_bottom_nav_select(n, next);
      return true;
    }
    case COGITO_BUTTON_GROUP: {
      if (n->len == 0) return false;
      int prev = n->selected;
      int next = prev + dir;
      if (next < 0) next = 0;
      if (next >= (int)n->len) next = (int)n->len - 1;
      if (next == prev) return false;
      n->selected = next;
      if (n->on_select) {
        YisVal arg = YV_INT(next);
        cogito_invoke(n->on_select, 1, &arg);
      }
      return true;
    }
    case COGITO_CAROUSEL: {
      if (n->len == 0) return false;
      int prev = n->kd->carousel.active_index;
      int next = prev + dir;
      if (next < 0) next = 0;
      if (next >= (int)n->len) next = (int)n->len - 1;
      if (next == prev) return false;
      n->kd->carousel.active_index = next;
      cogito_invoke_select(n, next, false);
      return true;
    }
    default:
      return false;
  }
}

static bool cogito_keyboard_activate_focus(CogitoNode* n, int mx, int my) {
  if (!n || n->disabled) return false;
  switch (n->kind) {
    case COGITO_BUTTON:
    case COGITO_FONTBUTTON:
    case COGITO_ICONBTN:
    case COGITO_FAB:
    case COGITO_FAB_MENU:
    case COGITO_CHIP:
    case COGITO_SPLIT_BUTTON:
      cogito_invoke_click(n);
      return true;
    case COGITO_CHECKBOX:
    case COGITO_SWITCH:
    case COGITO_SWITCHBAR:
      return cogito_keyboard_toggle_focus(n);
    case COGITO_DATEPICKER:
      cogito_datepicker_click(n, mx, my);
      return true;
    case COGITO_TIMEPICKER:
      cogito_timepicker_click(n, mx, my);
      return true;
    default:
      return false;
  }
}

static bool cogito_escape_close_embedded_overlays(CogitoNode* n) {
  if (!n) return false;

  for (size_t i = n->len; i > 0; i--) {
    if (cogito_escape_close_embedded_overlays(n->children[i - 1])) return true;
  }

  if (n->kind == COGITO_BOTTOM_SHEET) {
    int min_h = cogito_bottom_sheet_min_height(n);
    bool changed = n->kd->bottom_sheet.dragging || n->kd->bottom_sheet.expanded ||
                   n->kd->bottom_sheet.desired_h != min_h || n->h > (min_h + 1);
    if (!changed) return false;
    n->kd->bottom_sheet.desired_h = min_h;
    n->kd->bottom_sheet.expanded = false;
    n->kd->bottom_sheet.dragging = false;
    if (cogito_bottom_sheet_active == n) cogito_bottom_sheet_active = NULL;
    if (cogito_pointer_capture == n) cogito_pointer_capture = NULL;
    return true;
  }

  if (n->kind == COGITO_SIDE_SHEET) {
    // Sidebar mode is docked layout, not an overlay dismiss target.
    if (n->kd->side_sheet.mode == COGITO_SIDE_SHEET_SIDEBAR) return false;
    int min_w = cogito_side_sheet_min_width(n);
    bool changed = n->kd->side_sheet.dragging || n->kd->side_sheet.expanded ||
                   n->kd->side_sheet.desired_w != min_w || n->w > (min_w + 1);
    if (!changed) return false;
    n->kd->side_sheet.desired_w = min_w;
    n->kd->side_sheet.expanded = false;
    n->kd->side_sheet.dragging = false;
    if (cogito_side_sheet_active == n) cogito_side_sheet_active = NULL;
    if (cogito_pointer_capture == n) cogito_pointer_capture = NULL;
    return true;
  }

  return false;
}

static bool cogito_escape_close_overlays(CogitoNode* win) {
  if (!win) return false;

  if (cogito_menu.open) {
    cogito_menu_close();
    return true;
  }
  if (cogito_fab_menu_open_node) {
    CogitoNode* fm = cogito_fab_menu_open_node;
    cogito_fab_menu_open_node->popover_open = false;
    cogito_fab_menu_open_node = NULL;
    cogito_fab_menu_kbd_index = -1;
    cogito_overlay_focus_restore(win);
    cogito_a11y_emit_event("overlay-close", fm, "fab-menu");
    cogito_window_relayout(win);
    return true;
  }
  if (win->side_sheet_open && win->side_sheet_node) {
    cogito_window_clear_side_sheet(YV_OBJ(win));
    return true;
  }
  if (win->popover_open && win->popover) {
    cogito_popover_close(YV_OBJ(win));
    return true;
  }

  CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
    ? win->dialog
    : cogito_find_dialog_slot(win);
  if ((slot && slot->dialog_open && slot->dialog) || win->dialog_open) {
    cogito_window_clear_dialog(YV_OBJ(win));
    return true;
  }

  if (cogito_escape_close_embedded_overlays(win)) {
    cogito_window_relayout(win);
    return true;
  }

  return false;
}

static CogitoNode* cogito_focus_scope_root(CogitoNode* win) {
  if (!win) return NULL;
  if (win->popover_open && win->popover) return win->popover;
  CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
    ? win->dialog
    : cogito_find_dialog_slot(win);
  if (slot && slot->dialog_open && slot->dialog) return slot->dialog;
  if (win->dialog_open && win->dialog) return win->dialog;
  return win;
}

static bool cogito_menu_keyboard_handle(void) {
  if (!cogito_menu.open || !cogito_menu.owner || cogito_menu.owner->menu_len == 0) return false;
  int len = (int)cogito_menu.owner->menu_len;

  // Detect if any navigation key was pressed
  bool nav_up    = cogito_backend->is_key_pressed(SDL_SCANCODE_UP);
  bool nav_down  = cogito_backend->is_key_pressed(SDL_SCANCODE_DOWN);
  bool nav_home  = cogito_backend->is_key_pressed(SDL_SCANCODE_HOME);
  bool nav_end   = cogito_backend->is_key_pressed(SDL_SCANCODE_END);
  bool nav_tab   = cogito_backend->is_key_pressed(SDL_SCANCODE_TAB);
  bool nav_any   = nav_up || nav_down || nav_home || nav_end || nav_tab;

  // Don't activate keyboard focus until a navigation key is pressed
  if (cogito_menu_kbd_index < 0 && !nav_any) {
    // Check for Enter/Space on no selection — ignore
    return false;
  }

  // First navigation key activates focus on item 0
  if (cogito_menu_kbd_index < 0 || cogito_menu_kbd_index >= len) cogito_menu_kbd_index = 0;

  int next = cogito_menu_kbd_index;
  if (nav_up) next--;
  else if (nav_down) next++;
  else if (nav_home) next = 0;
  else if (nav_end) next = len - 1;
  else if (nav_tab) {
    bool reverse = cogito_backend->is_key_down(SDL_SCANCODE_LSHIFT) || cogito_backend->is_key_down(SDL_SCANCODE_RSHIFT);
    next += reverse ? -1 : 1;
  }

  if (next < 0) next = len - 1;
  if (next >= len) next = 0;
  if (next != cogito_menu_kbd_index) {
    cogito_menu_kbd_index = next;
    return true;
  }

  if (cogito_backend->is_key_pressed(SDL_SCANCODE_RETURN) ||
      cogito_backend->is_key_pressed(SDL_SCANCODE_KP_ENTER) ||
      cogito_backend->is_key_pressed(SDL_SCANCODE_SPACE)) {
    cogito_invoke_menu(cogito_menu.owner, (size_t)cogito_menu_kbd_index);
    cogito_menu_close();
    return true;
  }
  return false;
}

static bool cogito_fab_menu_keyboard_handle(CogitoNode* win) {
  // FAB Menu requires 2-7 items to be considered open
  if (!cogito_fab_menu_open_node || cogito_fab_menu_open_node->menu_len < 2) return false;
  int len = (int)cogito_fab_menu_open_node->menu_len;
  if (cogito_fab_menu_kbd_index < 0 || cogito_fab_menu_kbd_index >= len) cogito_fab_menu_kbd_index = 0;

  int next = cogito_fab_menu_kbd_index;
  if (cogito_backend->is_key_pressed(SDL_SCANCODE_UP)) next++;
  else if (cogito_backend->is_key_pressed(SDL_SCANCODE_DOWN)) next--;
  else if (cogito_backend->is_key_pressed(SDL_SCANCODE_HOME)) next = len - 1;
  else if (cogito_backend->is_key_pressed(SDL_SCANCODE_END)) next = 0;
  else if (cogito_backend->is_key_pressed(SDL_SCANCODE_TAB)) {
    bool reverse = cogito_backend->is_key_down(SDL_SCANCODE_LSHIFT) || cogito_backend->is_key_down(SDL_SCANCODE_RSHIFT);
    next += reverse ? 1 : -1;
  }
  if (next < 0) next = len - 1;
  if (next >= len) next = 0;
  if (next != cogito_fab_menu_kbd_index) {
    cogito_fab_menu_kbd_index = next;
    return true;
  }

  if (cogito_backend->is_key_pressed(SDL_SCANCODE_RETURN) ||
      cogito_backend->is_key_pressed(SDL_SCANCODE_KP_ENTER) ||
      cogito_backend->is_key_pressed(SDL_SCANCODE_SPACE)) {
    CogitoNode* owner = cogito_fab_menu_open_node;
    cogito_invoke_menu(cogito_fab_menu_open_node, (size_t)cogito_fab_menu_kbd_index);
    cogito_fab_menu_open_node->popover_open = false;
    cogito_fab_menu_open_node = NULL;
    cogito_fab_menu_kbd_index = -1;
    cogito_overlay_focus_restore(win);
    cogito_a11y_emit_event("overlay-close", owner, "fab-menu");
    if (win) cogito_window_relayout(win);
    return true;
  }
  return false;
}

static bool cogito_env_enabled(const char* name) {
  const char* v = getenv(name);
  return v && v[0] && v[0] != '0';
}

static inline double cogito_startup_now(void) {
#if defined(CLOCK_MONOTONIC)
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
  }
#endif
#if defined(TIME_UTC)
  struct timespec ts_utc;
  if (timespec_get(&ts_utc, TIME_UTC) == TIME_UTC) {
    return (double)ts_utc.tv_sec + (double)ts_utc.tv_nsec / 1000000000.0;
  }
#endif
  return 0.0;
}

#if defined(__APPLE__)
extern bool cogito_macos_set_dock_icon(const char *path);
#endif

static bool cogito_apply_window_icon(CogitoWindow* window, CogitoApp* app) {
  if (!window || !cogito_backend || !cogito_backend->window_set_icon) return false;

  const char* icon_path = cogito_app_get_icon_path(app);
  char resolved_icon[PATH_MAX];
  bool icon_set = false;
  if (icon_path) {
    if (cogito_image_resolve_path(icon_path, resolved_icon, sizeof(resolved_icon))) {
      icon_set = cogito_backend->window_set_icon(window, resolved_icon);
    }
  }
  if (!icon_set && cogito_script_dir[0]) {
    // Auto-detect icon file in script directory
    static const char* icon_names[] = {
      "icon.png",
      "icon.svg",
      "icon.jpg",
      "icon.bmp", NULL
    };
    for (int i = 0; icon_names[i]; i++) {
      if (cogito_image_resolve_path(icon_names[i], resolved_icon, sizeof(resolved_icon))) {
        if (cogito_backend->window_set_icon(window, resolved_icon)) {
          icon_set = true;
          break;
        }
      }
    }
  }
#if defined(__APPLE__)
  if (icon_set) {
    cogito_macos_set_dock_icon(resolved_icon);
  }
#endif
  return icon_set;
}

static void cogito_run(YisVal appv, YisVal winv) {
  if (appv.tag != EVT_OBJ) yis_trap("cogito.run expects app");
  if (winv.tag != EVT_OBJ) yis_trap("cogito.run expects window");
  // Keep timers created before app.run() so startup polling/animation loops
  // begin immediately when the main loop starts.
  cogito_timer_collect();
  CogitoApp* app = (CogitoApp*)appv.as.p;
  CogitoNode* win = (CogitoNode*)winv.as.p;
  bool startup_profile = cogito_env_enabled("COGITO_STARTUP_PROFILE");
  bool startup_exit = cogito_env_enabled("COGITO_STARTUP_EXIT");
  double startup_t0 = cogito_startup_now();
  double startup_t_after_theme_system = startup_t0;
  double startup_t_after_theme_apply = startup_t0;
  double startup_t_after_theme = startup_t0;
  double startup_t_after_window = startup_t0;
  double startup_t_after_fonts = startup_t0;
  double startup_t_after_layout = startup_t0;
  double startup_window_icon_ms = 0.0;
  size_t startup_draw_nodes = 0;
  int startup_layout_passes = 0;
  const char* title = (win->text && win->text->data && win->text->data[
    0
  ]) ? win->text->data : COGITO_USER_VERSION;
  const char* app_id = (app && app->app_id && app->app_id->data && app->app_id->data[
    0
  ]) ? app->app_id->data : NULL;
  const char* app_name = (app && app->app_name && app->app_name->data && app->app_name->data[
    0
  ]) ? app->app_name->data : title;
  cogito_active_window = win;
  cogito_active_app = app;
  cogito_update_system_theme(appv);
  startup_t_after_theme_system = cogito_startup_now();
  cogito_app_update_theme(appv);
  startup_t_after_theme_apply = cogito_startup_now();
  if (!cogito_theme_applied_last_update) {
    cogito_apply_style_tree(win);
  }
  startup_t_after_theme = cogito_startup_now();
  if (cogito_debug_style_enabled_internal()) {
    cogito_style_dump_tree_internal(win,
    0);
  }
  (void)SDL_SetHint(SDL_HINT_APP_NAME, app_name);
  if (app_id && app_id[
    0
  ]) {
    (void)SDL_SetHint(SDL_HINT_APP_ID, app_id);
  }
  (void)SDL_SetAppMetadata(app_name, NULL, app_id ? app_id : app_name);
#if defined(__APPLE__)
  cogito_macos_set_process_name(app_name);
#endif
  // Initialize SDL3 backend
  if (!cogito_backend) {
    cogito_backend_sdl3_init();
  }
  if (!cogito_backend || !cogito_backend->init()) {
    yis_trap("Failed to initialize SDL3 backend");
  }
  
  if (win->auto_size) {
    int dw = 0;
    int dh = 0;
    cogito_window_desired_size(win, &dw, &dh);
    if (dw > 0 && dh > 0) {
      win->w = dw;
      win->h = dh;
    }
  }
  
  CogitoNode* appbar = cogito_find_appbar(win);
  
  // Create window via backend
  bool borderless = (appbar != NULL);
#if defined(__APPLE__)
  borderless = false; // macOS uses native decorations
#endif
  CogitoWindow* window = cogito_backend->window_create(title, win->w, win->h, win->resizable, borderless, true);
  if (!window) {
    yis_trap("Failed to create window");
  }

  if (cogito_backend->window_set_min_size) {
    int min_w = win->resize_min_w < 0 ? 0 : win->resize_min_w;
    int min_h = win->resize_min_h < 0 ? 0 : win->resize_min_h;
    cogito_backend->window_set_min_size(window, min_w, min_h);
    if (win->w < min_w || win->h < min_h) {
      if (win->w < min_w) win->w = min_w;
      if (win->h < min_h) win->h = min_h;
      cogito_backend->window_set_size(window, win->w, win->h);
    }
  }

  cogito_active_backend_window = window;

  cogito_backend_ready = true;
  {
    const char* env_inspector = getenv("COGITO_INSPECTOR");
    cogito_inspector_enabled = true;
    cogito_inspector_open = env_inspector && env_inspector[
      0
    ] && env_inspector[
      0
    ] != '0';
    cogito_inspector_selected = win;
    cogito_inspector_view_x = 0;
    cogito_inspector_view_y = 0;
    cogito_inspector_view_w = 0;
    cogito_inspector_view_h = 0;
  }
  
#if defined(__APPLE__)
  cogito_configure_macos_window(appbar != NULL);
  bool macos_app_name_pending = !cogito_macos_apply_app_name(app_name);
#else
  bool macos_app_name_pending = false;
#endif
  cogito_apply_window_a11y_label(win);
  startup_t_after_window = cogito_startup_now();

  const char* font_path = NULL;
  static char app_baseline_font_buf[PATH_MAX];
  if (cogito_active_app) {
    const char* baseline_name = cogito_app_get_baseline_font_path(cogito_active_app);
    if (baseline_name && baseline_name[0]) {
      const char* resolved = cogito_resolve_font_by_name(baseline_name);
      if (resolved && resolved[0]) {
        strncpy(app_baseline_font_buf, resolved, PATH_MAX - 1);
        app_baseline_font_buf[PATH_MAX - 1] = '\0';
        font_path = app_baseline_font_buf;
      }
    }
  }
  if (!font_path || !font_path[0]) {
    font_path = getenv("COGITO_FONT");
  }
  if (!font_path || !font_path[
    0
  ]) {
    font_path = cogito_default_font_path();
  }
  const char* font_size_env = getenv("COGITO_FONT_SIZE");
  if (font_size_env && font_size_env[
    0
  ]) {
    int sz = atoi(font_size_env);
    if (sz > 6) cogito_font_size_value = sz;
  }
  
  if (font_path && font_path[
    0
  ]) {
    if (cogito_load_font_file(&cogito_font, font_path, cogito_font_size(), cogito_font_weight_normal())) {
      cogito_font_ready = true;
      cogito_font_loaded_size = cogito_font_size();
      cogito_font_from_env = true;
      cogito_font_path_active = font_path;
    }
  }
  
  if (!cogito_font_ready) {
    cogito_font_ready = true;
    cogito_font_loaded_size = cogito_font_size();
    cogito_font_from_env = false;
    cogito_font_path_active = NULL;
  }
  // Serif font
  const char* serif_font_path = getenv("COGITO_FONT_SERIF");
  if (!serif_font_path || !serif_font_path[
    0
  ]) {
    serif_font_path = cogito_guess_serif_font_path(font_path);
  }
  if (serif_font_path && serif_font_path[0]) {
    // Defer loading until a serif node is actually drawn.
    cogito_font_serif_from_env = true;
    cogito_font_serif_path_active = serif_font_path;
    cogito_font_serif_ready = false;
    cogito_font_serif_loaded_size = 0;
  } else {
    cogito_font_serif_from_env = false;
    cogito_font_serif_path_active = NULL;
    cogito_font_serif_ready = false;
    cogito_font_serif_loaded_size = 0;
  }
  // Mono font
  const char* mono_font_path = getenv("COGITO_FONT_MONO");
  if (!mono_font_path || !mono_font_path[
    0
  ]) {
    mono_font_path = cogito_guess_mono_font_path(font_path);
  }
  if (mono_font_path && mono_font_path[0]) {
    // Defer loading until a monospace node is actually drawn.
    cogito_font_mono_from_env = true;
    cogito_font_mono_path_active = mono_font_path;
    cogito_font_mono_ready = false;
    cogito_font_mono_loaded_size = 0;
  } else {
    cogito_font_mono_from_env = false;
    cogito_font_mono_path_active = NULL;
    cogito_font_mono_ready = false;
    cogito_font_mono_loaded_size = 0;
  }
  // Bold font
  const char* bold_font_path = NULL;
  static char app_emph_buf[PATH_MAX];
  cogito_font_emphasized_path_active = NULL;
  if (cogito_active_app) {
    const char* emph_name = cogito_app_get_emphasized_font_path(cogito_active_app);
    if (emph_name && emph_name[0]) {
      const char* resolved = cogito_resolve_font_by_name(emph_name);
      if (resolved && resolved[0]) {
        strncpy(app_emph_buf, resolved, PATH_MAX - 1);
        app_emph_buf[PATH_MAX - 1] = '\0';
        cogito_font_emphasized_path_active = app_emph_buf;
      }
    }
  }
  if (!bold_font_path || !bold_font_path[0]) {
    bold_font_path = getenv("COGITO_FONT_BOLD");
  }
  bool bold_from_regular = false;
  if (!bold_font_path || !bold_font_path[
    0
  ]) {
    bold_font_path = cogito_guess_bold_font_path(font_path);
    if (!bold_font_path && font_path && font_path[
      0
    ]) {
      bold_font_path = font_path;
      bold_from_regular = true;
    }
  }
  if (bold_font_path && bold_font_path[
    0
  ]) {
    bool weight_applied = false;
    int bold_weight = bold_from_regular ? cogito_font_weight_bold() : 0;
    if (cogito_load_font_file_ex(&cogito_font_bold, bold_font_path, cogito_font_size(), bold_weight, &weight_applied)) {
      if (bold_from_regular && !weight_applied) {
        // Variable font weight not applied -- fall back to faux bold
        cogito_font_bold_variable = true;
      } else {
        cogito_font_bold_variable = false;
      }
      cogito_font_bold_ready = true;
      cogito_font_bold_loaded_size = cogito_font_size();
      cogito_font_bold_from_env = true;
      cogito_font_bold_path_active = bold_font_path;
    }
  }
  // Medium font
  const char* medium_font_path = NULL;
  if (!medium_font_path || !medium_font_path[0]) {
    medium_font_path = getenv("COGITO_FONT_MEDIUM");
  }
  bool medium_from_regular = false;
  if (!medium_font_path || !medium_font_path[
    0
  ]) {
    medium_font_path = cogito_guess_medium_font_path(font_path);
    if (!medium_font_path && font_path && font_path[
      0
    ]) {
      medium_font_path = font_path;
      medium_from_regular = true;
    }
  }
  if (medium_font_path && medium_font_path[
    0
  ]) {
    bool weight_applied = false;
    int medium_weight = medium_from_regular ? cogito_font_weight_medium() : 0;
    if (cogito_load_font_file_ex(&cogito_font_medium, medium_font_path, cogito_font_size(), medium_weight, &weight_applied)) {
      if (medium_from_regular && !weight_applied) {
        // Variable font weight not applied -- fall back to faux medium
        cogito_font_medium_variable = true;
      } else {
        cogito_font_medium_variable = false;
      }
      cogito_font_medium_ready = true;
      cogito_font_medium_loaded_size = cogito_font_size();
      cogito_font_medium_from_env = true;
      cogito_font_medium_path_active = medium_font_path;
    }
  }
  startup_t_after_fonts = cogito_startup_now();

  if (win->auto_size) {
    int dw = 0;
    int dh = 0;
    cogito_window_desired_size(win, &dw, &dh);
    if (dw > 0 && dh > 0) {
      win->w = dw;
      win->h = dh;
      cogito_backend->window_set_size(window, dw, dh);
    }
  }
  cogito_layout_image_loaded_this_pass = false;
  cogito_layout_window(win);
  startup_layout_passes = 1;
  // A second pass is only needed if an image acquired intrinsic dimensions
  // during the first layout pass.
  if (cogito_layout_image_loaded_this_pass) {
    cogito_layout_image_loaded_this_pass = false;
    cogito_layout_window(win);
    startup_layout_passes = 2;
  }
  startup_t_after_layout = cogito_startup_now();
  CogitoWindow* inspector_window = NULL;
  const int inspector_w = 800;
  const int inspector_h = 500;
  const int inspector_appbar_h = 52;
  const int inspector_drag_hit_pad = 8;
  const int inspector_corner_r = 18;
#if defined(__APPLE__)
  const int inspector_shadow_margin = 0;
#else
  const int inspector_shadow_margin = 18;
#endif
  int inspector_traffic_btn = 14;
  int inspector_traffic_gap = 10;
  int inspector_traffic_pad_top = 15;
  int inspector_traffic_pad_x = 15;
  cogito_appbar_button_metrics(&inspector_traffic_btn, &inspector_traffic_gap, &inspector_traffic_pad_top, &inspector_traffic_pad_x);
  if (inspector_traffic_btn < 2) inspector_traffic_btn = 2;
  if (inspector_traffic_gap < 0) inspector_traffic_gap = 0;
  if (inspector_traffic_pad_top < 0) inspector_traffic_pad_top = 0;
  if (inspector_traffic_pad_x < 0) inspector_traffic_pad_x = 0;
  int* inspector_hit_test_params = (int*)malloc(6 * sizeof(int));
  if (inspector_hit_test_params) {
    inspector_hit_test_params[
      0
    ] = inspector_appbar_h;
    inspector_hit_test_params[
      1
    ] = inspector_shadow_margin;
    inspector_hit_test_params[
      2
    ] = inspector_traffic_btn;
    inspector_hit_test_params[
      3
    ] = inspector_traffic_gap;
    inspector_hit_test_params[
      4
    ] = inspector_traffic_pad_x;
    inspector_hit_test_params[
      5
    ] = inspector_traffic_pad_top;
  }
  bool inspector_native_drag = false;
  bool inspector_drag_pending = false;
  bool inspector_drag_active = false;
  int inspector_drag_start_gx = 0;
  int inspector_drag_start_gy = 0;
  int inspector_drag_start_wx = 0;
  int inspector_drag_start_wy = 0;
  if (cogito_inspector_open) {
    inspector_window = cogito_backend->window_create(
      "Inspector",
      inspector_w + inspector_shadow_margin * 2,
      inspector_h + inspector_shadow_margin * 2,
    true,
    true,
    false
    );
    if (inspector_window) {
      cogito_backend->window_show(inspector_window);
      if (cogito_backend->window_set_hit_test_callback) {
        cogito_backend->window_set_hit_test_callback(inspector_window, cogito_inspector_hit_test, (void*)inspector_hit_test_params);
        inspector_native_drag = true;
      }
#if defined(__APPLE__)
      cogito_macos_set_window_shadow(inspector_window, true);
#endif
    } else {
      cogito_inspector_open = false;
    }
  }

  #include "widgets/util/carousel_anim.inc"
  // Render the first frame before showing the window so the user never
  // sees a bare black framebuffer while layout/setup is finishing.
  {
    CogitoColor win_bg = win->bg_set ? win->bg : cogito_rgba(245,
    245,
    245,
    255);
    cogito_backend->begin_frame(window);
    cogito_icon_cache_frame_start(); // Initialize icon cache frame counter
    cogito_image_cache_frame_start();
    cogito_backend->clear(win_bg);
    if (startup_profile) {
      cogito_draw_count_nodes = 0;
      cogito_draw_count_enabled = true;
    }
    cogito_draw_node(win);
    if (startup_profile) {
      cogito_draw_count_enabled = false;
      startup_draw_nodes = cogito_draw_count_nodes;
    }
    cogito_backend->end_frame(window);
    cogito_backend->present(window);
  }
  double startup_t_done = cogito_startup_now();
  cogito_backend->window_show(window);
  {
    double icon_t0 = cogito_startup_now();
    (void)cogito_apply_window_icon(window, app);
    startup_window_icon_ms = (cogito_startup_now() - icon_t0) * 1000.0;
  }
  if (startup_profile) {
    double startup_t_done_with_icon = cogito_startup_now();
    double ms_window = (startup_t_after_window - startup_t_after_theme) * 1000.0;
    double ms_total = ((startup_t_done - startup_t0) * 1000.0) - ms_window;
    double ms_total_with_icon = ((startup_t_done_with_icon - startup_t0) * 1000.0) - ms_window;
    if (ms_total < 0.0) ms_total = 0.0;
    if (ms_total_with_icon < 0.0) ms_total_with_icon = 0.0;
    double ms_theme = (startup_t_after_theme - startup_t0) * 1000.0;
    double ms_theme_system = (startup_t_after_theme_system - startup_t0) * 1000.0;
    double ms_theme_apply = (startup_t_after_theme_apply - startup_t_after_theme_system) * 1000.0;
    double ms_theme_style = (startup_t_after_theme - startup_t_after_theme_apply) * 1000.0;
    double ms_fonts = (startup_t_after_fonts - startup_t_after_window) * 1000.0;
    double ms_layout = (startup_t_after_layout - startup_t_after_fonts) * 1000.0;
    double ms_draw = (startup_t_done - startup_t_after_layout) * 1000.0;
    fprintf(stderr,
            "[cogito-startup] total=%.2fms total_with_icon=%.2fms theme=%.2fms theme_system=%.2fms theme_apply=%.2fms theme_style=%.2fms fonts=%.2fms layout=%.2fms draw=%.2fms icon=%.2fms layout_passes=%d drawn_nodes=%zu\n",
            ms_total, ms_total_with_icon, ms_theme, ms_theme_system,
            ms_theme_apply, ms_theme_style, ms_fonts,
            ms_layout, ms_draw, startup_window_icon_ms,
            startup_layout_passes, startup_draw_nodes);
    fflush(stderr);
  }
  if (startup_exit) win->should_close = true;

  bool prev_main_pointer_valid = false;
  int prev_main_mx = 0;
  int prev_main_my = 0;
  bool prev_main_mouse_inside = false;
  bool prev_route_main_pointer = false;
  bool prev_inspector_pointer_known = false;
  int prev_inspector_mx = 0;
  int prev_inspector_my = 0;
  bool prev_inspector_mouse_inside = false;
  bool prev_inspector_mouse_in_frame = false;
  CogitoNode* prev_hover_node = NULL;
  bool prev_over_traffic_lights = false;
  bool appbar_drag_swallow = false;

  // Main event loop
  while (!cogito_backend->window_should_close(window) && !win->should_close) {
    // On macOS, wrap the entire loop body in an autoreleasepool.
    // SDL3's Metal backend and Cocoa calls create ObjC objects that get
    // autoreleased. Without a pool, they accumulate and leak every iteration.
#if defined(__APPLE__)
    extern void cogito_autorelease_push(void);
    extern void cogito_autorelease_pop(void);
    cogito_autorelease_push();
#endif
    // Poll events (true if any event was processed; used to decide if we need to redraw)
    bool had_events = cogito_backend->poll_events();
    cogito_refresh_now();
    cogito_relayout_defer_begin();
#if defined(__APPLE__)
    if (macos_app_name_pending) {
      macos_app_name_pending = !cogito_macos_apply_app_name(app_name);
    }
#endif
    
    if (cogito_active_app) {
      cogito_update_system_theme(appv);
    }
    bool timer_fired = cogito_timer_dispatch_due();
    if (timer_fired) cogito_queue_draw(win);
    // Only update inspector button metrics when the inspector is actually open
    if (cogito_inspector_open && inspector_window) {
      cogito_appbar_button_metrics(&inspector_traffic_btn, &inspector_traffic_gap, &inspector_traffic_pad_top, &inspector_traffic_pad_x);
      if (inspector_traffic_btn < 2) inspector_traffic_btn = 2;
      if (inspector_traffic_gap < 0) inspector_traffic_gap = 0;
      if (inspector_traffic_pad_top < 0) inspector_traffic_pad_top = 0;
      if (inspector_traffic_pad_x < 0) inspector_traffic_pad_x = 0;
      if (inspector_hit_test_params) {
        inspector_hit_test_params[
          2
        ] = inspector_traffic_btn;
        inspector_hit_test_params[
          3
        ] = inspector_traffic_gap;
        inspector_hit_test_params[
          4
        ] = inspector_traffic_pad_x;
        inspector_hit_test_params[
          5
        ] = inspector_traffic_pad_top;
      }
    }
    if (inspector_window && cogito_backend->window_should_close(inspector_window)) {
      cogito_backend->window_destroy(inspector_window);
      inspector_window = NULL;
      cogito_inspector_open = false;
      if (inspector_hit_test_params) {
        free(inspector_hit_test_params);
        inspector_hit_test_params = NULL;
      }
      inspector_native_drag = false;
      inspector_drag_pending = false;
      inspector_drag_active = false;
    }
    if (cogito_inspector_enabled && cogito_debug_inspector_toggle_pressed(cogito_backend)) {
      cogito_inspector_open = !cogito_inspector_open;
      if (cogito_inspector_open) {
        if (!inspector_window) {
          inspector_window = cogito_backend->window_create(
            "Inspector",
            inspector_w + inspector_shadow_margin * 2,
            inspector_h + inspector_shadow_margin * 2,
          true,
          true,
          false
          );
          if (inspector_window) {
            cogito_backend->window_show(inspector_window);
            if (cogito_backend->window_set_hit_test_callback) {
              cogito_backend->window_set_hit_test_callback(inspector_window, cogito_inspector_hit_test, (void*)inspector_hit_test_params);
              inspector_native_drag = true;
            }
#if defined(__APPLE__)
            cogito_macos_set_window_shadow(inspector_window, true);
#endif
          } else {
            cogito_inspector_open = false;
          }
        }
        if (cogito_inspector_open && !cogito_inspector_selected) cogito_inspector_selected = win;
      } else {
        if (inspector_window) {
          cogito_backend->window_destroy(inspector_window);
          inspector_window = NULL;
        }
        if (inspector_hit_test_params) {
          free(inspector_hit_test_params);
          inspector_hit_test_params = NULL;
        }
        inspector_native_drag = false;
        inspector_drag_pending = false;
        inspector_drag_active = false;
      }
    }
    // Deferred inspector window management: handle toggles from menu actions
    if (cogito_inspector_open && !inspector_window) {
      inspector_window = cogito_backend->window_create(
        "Inspector",
        inspector_w + inspector_shadow_margin * 2,
        inspector_h + inspector_shadow_margin * 2,
        true, true, false
      );
      if (inspector_window) {
        cogito_backend->window_show(inspector_window);
        if (cogito_backend->window_set_hit_test_callback) {
          cogito_backend->window_set_hit_test_callback(inspector_window, cogito_inspector_hit_test, (void*)inspector_hit_test_params);
          inspector_native_drag = true;
        }
#if defined(__APPLE__)
        cogito_macos_set_window_shadow(inspector_window, true);
#endif
      } else {
        cogito_inspector_open = false;
      }
      if (cogito_inspector_open && !cogito_inspector_selected) cogito_inspector_selected = win;
    } else if (!cogito_inspector_open && inspector_window) {
      cogito_backend->window_destroy(inspector_window);
      inspector_window = NULL;
      if (inspector_hit_test_params) {
        free(inspector_hit_test_params);
        inspector_hit_test_params = NULL;
      }
      inspector_native_drag = false;
      inspector_drag_pending = false;
      inspector_drag_active = false;
    }
    // Get window size
    int sw, sh;
    bool window_resized = false;
    cogito_backend->window_get_size(window, &sw, &sh);
    if (sw != win->w || sh != win->h) {
      win->w = sw;
      win->h = sh;
      cogito_window_relayout(win);
      window_resized = true;
      if (win->on_resize) {
        YisVal args[3];
        args[0] = YV_OBJ(win);
        args[1] = YV_INT(sw);
        args[2] = YV_INT(sh);
        cogito_invoke(win->on_resize, 3, args);
      }
    }
    // GTK-style selective invalidation: only redraw when something visual changed.
    // Mouse motion alone doesn't trigger redraw — hover-change detection below handles it.
    extern bool cogito_last_poll_had_non_motion;
    if (window_resized) cogito_queue_draw(win);
    if (had_events && cogito_last_poll_had_non_motion) cogito_queue_draw(win);

    // Get mouse position in the main window coordinate space.
    int mx = 0;
    int my = 0;
    bool main_mouse_inside = false;
    if (cogito_backend->get_mouse_position_in_window) {
      cogito_backend->get_mouse_position_in_window(window, &mx, &my);
      main_mouse_inside = (mx >= 0 && my >= 0 && mx < sw && my < sh);
    } else {
      cogito_backend->get_mouse_position(&mx, &my);
      main_mouse_inside = (mx >= 0 && my >= 0 && mx < sw && my < sh);
    }
    
    float wheel = cogito_backend->get_mouse_wheel_move();
    bool left_down = cogito_backend->is_mouse_button_down(0);
    bool left_pressed = cogito_backend->is_mouse_button_pressed(0);
    bool right_pressed = cogito_backend->is_mouse_button_pressed(2);
    bool left_released = cogito_backend->is_mouse_button_released(0);
    if (!left_down) {
      appbar_drag_swallow = false;
    }
    bool click = left_pressed; // Left button
    if (cogito_pointer_capture && left_released &&
        cogito_pointer_capture->kind != COGITO_DRAWING_AREA &&
        cogito_pointer_capture->kind != COGITO_VIEW_DUAL) {
      cogito_pointer_capture = NULL;
    }

    int imx = 0, imy = 0, iw = 0, ih = 0, iwx = 0, iwy = 0;
    bool inspector_mouse_known = false;
    bool inspector_mouse_inside = false;
    int inspector_gx = 0;
    int inspector_gy = 0;
    if (inspector_window && cogito_backend->get_mouse_position_in_window) {
      cogito_backend->get_mouse_position_in_window(inspector_window, &imx, &imy);
      cogito_backend->window_get_size(inspector_window, &iw, &ih);
      cogito_backend->window_get_position(inspector_window, &iwx, &iwy);
      inspector_gx = iwx + imx;
      inspector_gy = iwy + imy;
      inspector_mouse_known = true;
      inspector_mouse_inside = (imx >= 0 && imy >= 0 && imx < iw && imy < ih);
    }
    int inspector_frame_x = inspector_shadow_margin;
    int inspector_frame_y = inspector_shadow_margin;
    int inspector_frame_w = iw - inspector_shadow_margin * 2;
    int inspector_frame_h = ih - inspector_shadow_margin * 2;
    if (inspector_frame_w < 1) inspector_frame_w = 1;
    if (inspector_frame_h < 1) inspector_frame_h = 1;
    bool inspector_mouse_in_frame = inspector_mouse_inside &&
      imx >= inspector_frame_x &&
      imy >= inspector_frame_y &&
      imx < inspector_frame_x + inspector_frame_w &&
      imy < inspector_frame_y + inspector_frame_h;
    bool appbar_drag_capture = cogito_drag_active || cogito_drag_pending || appbar_drag_swallow;
    bool route_main_pointer = main_mouse_inside && !inspector_mouse_in_frame && !appbar_drag_capture;
    cogito_main_pointer_x = mx;
    cogito_main_pointer_y = my;
    cogito_main_pointer_valid = main_mouse_inside;
    cogito_main_pointer_routed = route_main_pointer;
    cogito_main_pointer_down = left_down;
    cogito_main_pointer_pressed = left_pressed;

    bool main_pointer_changed = !prev_main_pointer_valid ||
      mx != prev_main_mx || my != prev_main_my ||
      main_mouse_inside != prev_main_mouse_inside ||
      route_main_pointer != prev_route_main_pointer;
    bool inspector_pointer_changed = false;
    if (inspector_window) {
      inspector_pointer_changed = !prev_inspector_pointer_known ||
        inspector_mouse_known != prev_inspector_pointer_known ||
        imx != prev_inspector_mx || imy != prev_inspector_my ||
        inspector_mouse_inside != prev_inspector_mouse_inside ||
        inspector_mouse_in_frame != prev_inspector_mouse_in_frame;
    } else if (prev_inspector_pointer_known || prev_inspector_mouse_inside || prev_inspector_mouse_in_frame) {
      inspector_pointer_changed = true;
    }
    // Update stepper hit states for hover effects only when pointer routing/position changes.
    if (main_pointer_changed && route_main_pointer) {
      CogitoNode* stepper_hit = cogito_find_stepper_hit(win, mx, my);
      if (stepper_hit) {
        cogito_stepper_hit_test(stepper_hit, mx, my);
      }
    }
    // GTK-style hover-change detection: only redraw on mouse motion if the
    // hovered interactive widget changed (enter/leave a button, slider, etc.)
    if (main_pointer_changed) {
      CogitoNode* cur_hover = route_main_pointer
          ? cogito_find_clickable(win, mx, my) : NULL;
      if (cur_hover != prev_hover_node) {
        prev_hover_node = cur_hover;
        cogito_queue_draw(win);
      }
      // Menu hover highlight follows pointer even when underlying clickable node doesn't change.
      if (cogito_menu.open) {
        cogito_queue_draw(win);
      }
      // FAB Menu popup item hover highlight.
      if (cogito_fab_menu_open_node) {
        cogito_queue_draw(win);
      }
      // Overlay inspector (single-window mode) tracks pointer in the main window.
      if (cogito_inspector_open && !inspector_window) {
        cogito_queue_draw(win);
      }
      // Traffic light glyphs appear on cluster hover; redraw while pointer is near them.
      bool cur_over_traffic_lights = false;
      if (route_main_pointer) {
        CogitoNode* first_ab = cogito_find_appbar(win);
        if (first_ab && (first_ab->kd->appbar.btn_close_x >= 0 ||
                         first_ab->kd->appbar.btn_min_x >= 0 ||
                         first_ab->kd->appbar.btn_max_x >= 0)) {
          int btn = first_ab->kd->appbar.btn_size;
          int by = first_ab->kd->appbar.btn_y;
          int pad = 5;
          int cl = first_ab->kd->appbar.btn_close_x;
          int cr = cl + btn;
          if (first_ab->kd->appbar.btn_min_x >= 0) {
            int r = first_ab->kd->appbar.btn_min_x + btn;
            if (r > cr) cr = r;
          }
          if (first_ab->kd->appbar.btn_max_x >= 0) {
            int r = first_ab->kd->appbar.btn_max_x + btn;
            if (r > cr) cr = r;
          }
          cur_over_traffic_lights = cogito_hit_rect(mx, my, cl - pad, by - pad,
                                                    cr - cl + pad * 2, btn + pad * 2);
        }
      }
      if (cur_over_traffic_lights || cur_over_traffic_lights != prev_over_traffic_lights) {
        cogito_queue_draw(win);
      }
      prev_over_traffic_lights = cur_over_traffic_lights;
    }
    if (inspector_pointer_changed && cogito_inspector_open && inspector_window) {
      cogito_queue_draw(win);
    }

    prev_main_pointer_valid = true;
    prev_main_mx = mx;
    prev_main_my = my;
    prev_main_mouse_inside = main_mouse_inside;
    prev_route_main_pointer = route_main_pointer;
    if (inspector_window) {
      prev_inspector_pointer_known = inspector_mouse_known;
      prev_inspector_mx = imx;
      prev_inspector_my = imy;
      prev_inspector_mouse_inside = inspector_mouse_inside;
      prev_inspector_mouse_in_frame = inspector_mouse_in_frame;
    } else {
      prev_inspector_pointer_known = false;
      prev_inspector_mouse_inside = false;
      prev_inspector_mouse_in_frame = false;
    }

    int inspector_btn_close_x = -1;
    int inspector_btn_min_x = -1;
    int inspector_btn_max_x = -1;
    int inspector_btn_y = 0;
    CogitoTrafficLightsState inspector_lights = (CogitoTrafficLightsState){
      0
    };
    if (inspector_window) {
      int appbar_h = inspector_appbar_h;
      if (appbar_h > inspector_frame_h) appbar_h = inspector_frame_h;
      inspector_btn_y = inspector_frame_y + inspector_traffic_pad_top;
      if (inspector_btn_y + inspector_traffic_btn > inspector_frame_y + appbar_h) {
        inspector_btn_y = inspector_frame_y + (appbar_h - inspector_traffic_btn) / 2;
      }
      inspector_btn_close_x = inspector_frame_x + inspector_traffic_pad_x;
      inspector_btn_min_x = inspector_btn_close_x + inspector_traffic_btn + inspector_traffic_gap;
      inspector_btn_max_x = inspector_btn_min_x + inspector_traffic_btn + inspector_traffic_gap;
      bool down = left_down;
      cogito_traffic_lights_compute(
        inspector_btn_close_x, inspector_btn_min_x, inspector_btn_max_x, inspector_btn_y, inspector_traffic_btn,
        imx, imy, down, &inspector_lights
      );
    }

    if (!inspector_native_drag && inspector_drag_pending) {
      if (left_down) {
        if (inspector_mouse_known) {
          int dx = inspector_gx - inspector_drag_start_gx;
          int dy = inspector_gy - inspector_drag_start_gy;
          if (abs(dx) > 2 || abs(dy) > 2) {
            inspector_drag_active = true;
            inspector_drag_pending = false;
          }
        }
      } else {
        inspector_drag_pending = false;
      }
    }
    if (!inspector_native_drag && inspector_drag_active) {
      if (left_down) {
        if (inspector_mouse_known) {
          int dx = inspector_gx - inspector_drag_start_gx;
          int dy = inspector_gy - inspector_drag_start_gy;
          cogito_backend->window_set_position(inspector_window, inspector_drag_start_wx + dx, inspector_drag_start_wy + dy);
        }
      } else {
        inspector_drag_active = false;
      }
    }
    // Escape closes overlays only; it does not close the main window.
    if (cogito_backend->is_key_pressed(SDL_SCANCODE_ESCAPE)) {
      (void)cogito_escape_close_overlays(win);
    }
    if (cogito_menu_keyboard_handle()) {
      cogito_queue_draw(win);
    }
    if (cogito_fab_menu_keyboard_handle(win)) {
      cogito_queue_draw(win);
    }
    // Drag handling (non-macOS)
#if !defined(__APPLE__)
    if (cogito_drag_pending) {
      if (left_down) {
        int dx = mx - (int)cogito_drag_pending_mouse.x;
        int dy = my - (int)cogito_drag_pending_mouse.y;
        if (abs(dx) > 2 || abs(dy) > 2) {
          cogito_drag_active = true;
          cogito_drag_pending = false;
          cogito_drag_start_mouse = cogito_drag_pending_mouse;
          int wx, wy;
          cogito_backend->window_get_position(window, &wx, &wy);
          cogito_drag_start_win.x = (float)wx;
          cogito_drag_start_win.y = (float)wy;
        }
      } else {
        cogito_drag_pending = false;
      }
    }
    if (cogito_drag_active) {
      if (left_down) {
        float delta_x = (float)mx - cogito_drag_start_mouse.x;
        float delta_y = (float)my - cogito_drag_start_mouse.y;
        double now = cogito_now();
        if (now - cogito_drag_last_move > 0.016) {
          int new_x = (int)(cogito_drag_start_win.x + delta_x);
          int new_y = (int)(cogito_drag_start_win.y + delta_y);
          cogito_backend->window_set_position(window, new_x, new_y);
          cogito_drag_last_move = now;
        }
      } else {
        cogito_drag_active = false;
      }
    }
#endif

    // Mouse wheel scrolling
    if (wheel != 0.0f && route_main_pointer) {
      if (win->popover_open && win->popover) {
        CogitoNode* pop_hit = cogito_find_clickable(win->popover, mx, my);
        if (pop_hit && pop_hit->kind == COGITO_DATEPICKER && pop_hit->kd->datepicker.popover_panel) {
          if (cogito_datepicker_wheel(pop_hit, mx, my, wheel)) {
            cogito_relayout_subtree(pop_hit);
            wheel = 0.0f;
          }
        }
      }

      if (wheel != 0.0f) {
        CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
          ? win->dialog
          : cogito_find_dialog_slot(win);
        CogitoNode* root = (slot && slot->dialog) ? slot->dialog : win;
        CogitoNode* scroller = cogito_find_scroller(root, mx, my);
        if (scroller) {
          int delta = (int)(wheel * 10.0f);
          bool horiz = scroller->kd->scroller.horz && (cogito_backend->is_key_down(SDL_SCANCODE_LSHIFT) || 
                                                 cogito_backend->is_key_down(SDL_SCANCODE_RSHIFT) || 
                                                 !scroller->kd->scroller.vert);
          if (horiz) scroller->kd->scroller.scroll_x -= cogito_is_rtl() ? -delta : delta;
          else scroller->kd->scroller.scroll_y -= delta;
          CogitoNode* synced = cogito_scroller_sync_from(scroller);
          cogito_relayout_subtree(scroller);
          if (synced) cogito_relayout_subtree(synced);
        } else {
          // Check if mouse is over a carousel — scroll it horizontally
          CogitoNode* carousel = cogito_find_carousel(root, mx, my);
          if (carousel && (int)carousel->len > 1) {
            int max_scroll = 0;
            CAROUSEL_MAX_SCROLL(carousel, max_scroll);
            
            // Calculate unit width to check if leftmost item is wide enough for snapping
            int inner_w = carousel->w - carousel->padding_left - carousel->padding_right;
            if (inner_w < 0) inner_w = 0;
            int gaps = ((int)carousel->len >= 3) ? 2 : ((int)carousel->len - 1);
            if (gaps < 0) gaps = 0;
            int avail = inner_w - (gaps * 8);
            if (avail < 0) avail = 0;
            int unit_w = avail / 7;
            
            // Only snap if leftmost (tertiary) item width >= 56px
            bool should_snap = unit_w >= 56;
            
            // Use wheel delta to scroll (horizontal or vertical wheel both work)
            float wheel_dir = cogito_is_rtl() ? -wheel : wheel;
            double target_xf = carousel->kd->carousel.scroll_xf - (double)(wheel_dir * 80.0f);
            if (target_xf < 0.0) target_xf = 0.0;
            if (target_xf > (double)max_scroll) target_xf = (double)max_scroll;
            
            int snap_idx = 0;
            int snap_sx = 0;
            if (should_snap) {
              // Snap to nearest item
              CAROUSEL_NEAREST_INDEX(carousel, target_xf, max_scroll, snap_idx);
              CAROUSEL_SNAP_SCROLL(carousel, snap_idx, max_scroll, snap_sx);
            } else {
              // Without snapping: scroll freely to target position
              snap_sx = (int)(target_xf + 0.5);
              // Calculate active index from scroll progress (reverse of CAROUSEL_SNAP_SCROLL)
              if (max_scroll > 0) {
                snap_idx = (int)((target_xf / (double)max_scroll) * (double)((int)carousel->len - 1) + 0.5);
              } else {
                snap_idx = 0;
              }
              if (snap_idx < 0) snap_idx = 0;
              if (snap_idx >= (int)carousel->len) snap_idx = (int)carousel->len - 1;
            }
            // Start snap animation only if position actually changes
            carousel->kd->carousel.anim_from = carousel->kd->carousel.scroll_xf;
            carousel->kd->carousel.anim_to = (double)snap_sx;
            if (carousel->kd->carousel.anim_from != carousel->kd->carousel.anim_to) {
              carousel->kd->carousel.anim_start = cogito_now();
              if (!carousel->kd->carousel.animating) cogito_carousel_animating_count++;
              carousel->kd->carousel.animating = true;
            } else {
              carousel->kd->carousel.animating = false;
              carousel->kd->carousel.scroll_xf = carousel->kd->carousel.anim_to;
            }
            carousel->kd->carousel.active_index = snap_idx;
          }
        }
      }
    }
    /* Inspector window: wheel scrolls widget tree when cursor over panel */
    if (inspector_window && wheel != 0.0f && inspector_mouse_in_frame && imy >= inspector_frame_y + inspector_appbar_h) {
      int appbar_h = inspector_appbar_h;
      if (appbar_h > inspector_frame_h) appbar_h = inspector_frame_h;
      cogito_inspector_view_x = inspector_frame_x;
      cogito_inspector_view_y = inspector_frame_y + appbar_h;
      cogito_inspector_view_w = inspector_frame_w;
      cogito_inspector_view_h = inspector_frame_h - appbar_h - inspector_corner_r;
      if (cogito_inspector_view_h < 0) cogito_inspector_view_h = 0;
      if (cogito_inspector_handle_wheel(win, imx, imy, wheel)) {
        wheel = 0.0f;
        cogito_queue_draw(win);
      }
      cogito_inspector_view_x = 0;
      cogito_inspector_view_y = 0;
      cogito_inspector_view_w = 0;
      cogito_inspector_view_h = 0;
    }
    bool inspector_text_editing = cogito_inspector_is_text_editing();
    if (cogito_inspector_open && cogito_inspector_handle_text_input(win)) {
      inspector_text_editing = cogito_inspector_is_text_editing();
    }
    // Tab key navigation
    if (cogito_backend->is_key_pressed(SDL_SCANCODE_TAB) && !cogito_menu.open && !cogito_fab_menu_open_node && !inspector_text_editing) {
      CogitoNode* root = cogito_focus_scope_root(win);
      bool reverse = cogito_backend->is_key_down(SDL_SCANCODE_LSHIFT) || cogito_backend->is_key_down(SDL_SCANCODE_RSHIFT);
      bool found = false;
      CogitoNode* next = reverse
        ? cogito_focus_prev(root, cogito_focused, &found)
        : cogito_focus_next(root, cogito_focused, &found);
      if (!next) next = reverse ? cogito_focus_last(root) : cogito_focus_first(root);
      if (cogito_focused && cogito_focused != next && cogito_focused->kind == COGITO_COLORPICKER) {
        cogito_focused->kd->colorpicker.hex_editing = false;
      }
      if (next && next != cogito_focused) {
        cogito_a11y_emit_event("focus", next, "keyboard");
      }
      cogito_focused = next;
      cogito_focus_visible = (next != NULL);
    }
    if (cogito_focused && !cogito_focus_is_text_editing(cogito_focused) && !cogito_focused->disabled && !cogito_node_is_hidden(cogito_focused) && !inspector_text_editing) {
      bool relayout = false;
      int dir = 0;
      if (cogito_backend->is_key_pressed(SDL_SCANCODE_LEFT) ||
          cogito_backend->is_key_pressed(SDL_SCANCODE_UP)) {
        dir = -1;
      } else if (cogito_backend->is_key_pressed(SDL_SCANCODE_RIGHT) ||
                 cogito_backend->is_key_pressed(SDL_SCANCODE_DOWN)) {
        dir = 1;
      }
      if (dir != 0) {
        if (cogito_keyboard_adjust_slider(cogito_focused, dir)) {
          (void)cogito_colorpicker_panel_sync_from_slider(cogito_focused);
          relayout = true;
        }
        else if (cogito_keyboard_adjust_stepper(cogito_focused, dir)) relayout = true;
        else if (cogito_keyboard_move_indexed_selection(cogito_focused, dir)) relayout = true;
      }

      bool activate = cogito_backend->is_key_pressed(SDL_SCANCODE_RETURN) ||
                      cogito_backend->is_key_pressed(SDL_SCANCODE_KP_ENTER) ||
                      cogito_backend->is_key_pressed(SDL_SCANCODE_SPACE);
      if (activate) {
        int cx = cogito_focused->x + cogito_focused->w / 2;
        int cy = cogito_focused->y + cogito_focused->h / 2;
        cogito_theme_mark_action_origin(cx, cy);
        if (cogito_keyboard_activate_focus(cogito_focused, cx, cy)) relayout = true;
      }

      if (relayout) cogito_window_relayout(win);
    }
    // Text input handling
    bool focused_text_editable = cogito_focused &&
      (cogito_focused->kind == COGITO_TEXTFIELD || cogito_focused->kind == COGITO_TEXTVIEW || cogito_focused->kind == COGITO_SEARCHFIELD) &&
      cogito_focused->editable;
    bool focused_color_hex_edit = cogito_focused &&
      cogito_focused->kind == COGITO_COLORPICKER && cogito_focused->kd->colorpicker.hex_editing;
    if (cogito_focused && !cogito_focused->disabled && !cogito_node_is_hidden(cogito_focused) && (focused_text_editable || focused_color_hex_edit) && !inspector_text_editing) {
      bool changed = false;
      bool caret_changed = false;
      int ch = cogito_backend->get_char_pressed();
      while (ch > 0) {
        if (ch == '\t') {
          ch = cogito_backend->get_char_pressed();
          continue;
        }
        if ((cogito_focused->kind == COGITO_TEXTFIELD || cogito_focused->kind == COGITO_SEARCHFIELD) && (ch == '\n' || ch == '\r')) {
          ch = cogito_backend->get_char_pressed();
          continue;
        }
        if (cogito_focused->kind == COGITO_TEXTVIEW && (ch == '\n' || ch == '\r')) {
          ch = cogito_backend->get_char_pressed();
          continue;
        }
        if (cogito_focused->kind == COGITO_COLORPICKER) {
          char c = (char)ch;
          bool ok = (c == '#') || isxdigit((unsigned char)c);
          size_t len = cogito_focused->text ? cogito_focused->text->len : 0;
          size_t sel = 0;
          if (cogito_focused->kd->colorpicker.sel_start != cogito_focused->kd->colorpicker.sel_end) {
            int a = cogito_focused->kd->colorpicker.sel_start;
            int b = cogito_focused->kd->colorpicker.sel_end;
            if (a > b) { int tmp = a; a = b; b = tmp;
            }
            sel = (size_t)(b - a);
          }
          size_t new_len = len - sel + 1;
          if (ok && new_len <= 7) {
            if (c == '#' && cogito_focused->kd->colorpicker.caret != 0) ok = false;
            if (ok) {
              cogito_text_append_codepoint(cogito_focused, ch);
              changed = true;
            }
          }
        } else {
          cogito_text_append_codepoint(cogito_focused, ch);
          changed = true;
        }
        ch = cogito_backend->get_char_pressed();
      }
      if (cogito_backend->is_key_pressed(SDL_SCANCODE_BACKSPACE)) {
        cogito_text_backspace(cogito_focused);
        changed = true;
      }
      if (cogito_backend->is_key_pressed(SDL_SCANCODE_LEFT)) {
        if (cogito_text_move_caret_h(cogito_focused,
        -1)) caret_changed = true;
      }
      if (cogito_backend->is_key_pressed(SDL_SCANCODE_RIGHT)) {
        if (cogito_text_move_caret_h(cogito_focused, +1)) caret_changed = true;
      }
      if (cogito_focused->kind == COGITO_TEXTVIEW && cogito_backend->is_key_pressed(SDL_SCANCODE_UP)) {
        if (cogito_textview_move_caret_v(cogito_focused,
        -1)) caret_changed = true;
      }
      if (cogito_focused->kind == COGITO_TEXTVIEW && cogito_backend->is_key_pressed(SDL_SCANCODE_DOWN)) {
        if (cogito_textview_move_caret_v(cogito_focused, +1)) caret_changed = true;
      }
      if (cogito_focused->kind == COGITO_TEXTVIEW &&
          (cogito_backend->is_key_pressed(SDL_SCANCODE_RETURN) ||
           cogito_backend->is_key_pressed(SDL_SCANCODE_KP_ENTER))) {
        cogito_text_append_codepoint(cogito_focused, '\n');
        changed = true;
      }
      if (changed || caret_changed) {
        if (cogito_focused->kind == COGITO_COLORPICKER) {
          CogitoColor col = {
            0
          };
          const char* hex = cogito_focused->text ? cogito_focused->text->data : "";
          if (cogito_hex_to_color(hex, &col)) {
            cogito_rgb_to_hct(col, &cogito_focused->kd->colorpicker.h, &cogito_focused->kd->colorpicker.c, &cogito_focused->kd->colorpicker.t);
          }
        }
        if (changed) {
          cogito_invoke_change(cogito_focused);
        }
        cogito_window_relayout(win);
      }
    }
    // Slider active handling - ensure slider is activated when clicked
    if (!cogito_slider_active) {
      if (left_pressed) {
        // Check if we should activate a slider on press
        CogitoNode* slider_hit = cogito_find_clickable(win, mx, my);
        if (slider_hit && slider_hit->kind == COGITO_SLIDER && !slider_hit->disabled) {
          cogito_slider_active = slider_hit;
          cogito_pointer_capture = slider_hit;
          slider_hit->kd->slider.dragging = true;
          if (slider_hit->kd->slider.mode == COGITO_SLIDER_MODE_RANGE) {
            slider_hit->kd->slider.active_handle = cogito_slider_pick_range_handle(slider_hit, mx, my);
          } else {
            slider_hit->kd->slider.active_handle = 0;
          }
        }
      }
    }
    if (cogito_slider_active) {
      if (left_down) {
        CogitoNode* sld = cogito_slider_active;
        int rail_h = 16;
        if (sld->kd->slider.size == COGITO_SLIDER_SIZE_S) rail_h = 24;
        if (sld->kd->slider.size == COGITO_SLIDER_SIZE_M) rail_h = 40;
        if (sld->kd->slider.size == COGITO_SLIDER_SIZE_L) rail_h = 56;
        if (sld->kd->slider.size == COGITO_SLIDER_SIZE_XL) rail_h = 96;
        int pad = rail_h / 2 + 7;
        int x0 = sld->x + pad;
        int x1 = sld->x + sld->w - pad;
        if (x1 < x0) x1 = x0;
        double t = (x1 > x0) ? ((double)(mx - x0) / (double)(x1 - x0)) : 0.0;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        if (cogito_is_rtl()) t = 1.0 - t;
        double v = sld->kd->slider.min + t * (sld->kd->slider.max - sld->kd->slider.min);
        v = cogito_slider_snap_to_step(sld, v);
        bool changed = false;
        if (sld->kd->slider.mode == COGITO_SLIDER_MODE_RANGE) {
          double prev0 = sld->kd->slider.value;
          double prev1 = sld->kd->slider.value2;
          if (sld->kd->slider.active_handle == 2) {
            sld->kd->slider.value2 = v;
            if (sld->kd->slider.value2 < sld->kd->slider.value) sld->kd->slider.value2 = sld->kd->slider.value;
          } else {
            sld->kd->slider.value = v;
            if (sld->kd->slider.value > sld->kd->slider.value2) sld->kd->slider.value = sld->kd->slider.value2;
          }
          changed = (sld->kd->slider.value != prev0) || (sld->kd->slider.value2 != prev1);
        } else {
          if (v != sld->kd->slider.value) {
            sld->kd->slider.value = v;
            sld->kd->slider.value2 = v;
            changed = true;
          }
        }
        if (changed) {
          (void)cogito_colorpicker_panel_sync_from_slider(sld);
          cogito_invoke_change(sld);
          cogito_window_relayout(win);
        }
      } else {
        if (cogito_pointer_capture == cogito_slider_active) {
          cogito_pointer_capture = NULL;
        }
        cogito_slider_active->kd->slider.dragging = false;
        cogito_slider_active = NULL;
      }
    }
    // Carousel active handling (drag-to-scroll with velocity tracking)
    if (cogito_carousel_active) {
      if (left_down) {
        CogitoNode* cr = cogito_carousel_active;
        int dx = mx - cr->kd->carousel.drag_start_x;
        if (cogito_is_rtl()) dx = -dx;
        double new_xf = (double)cr->kd->carousel.drag_start_scroll - (double)dx;
        
        int max_scroll = 0;
        CAROUSEL_MAX_SCROLL(cr, max_scroll);
        
        if (new_xf < 0.0) new_xf = 0.0;
        if (new_xf > (double)max_scroll) new_xf = (double)max_scroll;
        
        // Track velocity: compare with last sample
        double now = cogito_now();
        double dt = now - cr->kd->carousel.drag_last_time;
        if (dt > 0.001) {
          double dxf = new_xf - cr->kd->carousel.drag_last_xf;
          // Exponential smoothing of velocity (px/sec)
          float inst_vel = (float)(dxf / dt);
          cr->kd->carousel.velocity = cr->kd->carousel.velocity * 0.6f + inst_vel * 0.4f;
          cr->kd->carousel.drag_last_time = now;
          cr->kd->carousel.drag_last_xf = new_xf;
        }
        
        cr->kd->carousel.scroll_xf = new_xf;
        int new_scroll = (int)(new_xf + 0.5);
        if (new_scroll != cr->kd->carousel.scroll_x) {
          cr->kd->carousel.scroll_x = new_scroll;
          cogito_window_relayout(win);
        }
        
        if (cogito_backend && cogito_backend->set_cursor) {
          cogito_backend->set_cursor(COGITO_CURSOR_GRABBING);
        }
      } else {
        // --- Release: start momentum + snap animation ---
        CogitoNode* cr = cogito_carousel_active;
        int max_scroll = 0;
        CAROUSEL_MAX_SCROLL(cr, max_scroll);
        
        // Calculate unit width to check if leftmost item is wide enough for snapping
        int inner_w = cr->w - cr->padding_left - cr->padding_right;
        if (inner_w < 0) inner_w = 0;
        int gaps = ((int)cr->len >= 3) ? 2 : ((int)cr->len - 1);
        if (gaps < 0) gaps = 0;
        int avail = inner_w - (gaps * 8);
        if (avail < 0) avail = 0;
        int unit_w = avail / 7;
        
        // Only snap if leftmost (tertiary) item width >= 56px
        bool should_snap = unit_w >= 56;
        
        // Project where momentum would carry us (velocity * decay_time)
        double projected = cr->kd->carousel.scroll_xf + (double)cr->kd->carousel.velocity * 0.4;
        if (projected < 0.0) projected = 0.0;
        if (projected > (double)max_scroll) projected = (double)max_scroll;
        
        int snap_idx = 0;
        int snap_sx = 0;
        if (should_snap) {
          // Snap to nearest item
          CAROUSEL_NEAREST_INDEX(cr, projected, max_scroll, snap_idx);
          CAROUSEL_SNAP_SCROLL(cr, snap_idx, max_scroll, snap_sx);
        } else {
          // Without snapping: momentum carries us to projected position
          snap_sx = (int)(projected + 0.5);
          // Calculate active index from scroll progress (reverse of CAROUSEL_SNAP_SCROLL)
          if (max_scroll > 0) {
            snap_idx = (int)((projected / (double)max_scroll) * (double)((int)cr->len - 1) + 0.5);
          } else {
            snap_idx = 0;
          }
          if (snap_idx < 0) snap_idx = 0;
          if (snap_idx >= (int)cr->len) snap_idx = (int)cr->len - 1;
        }
        // Start animation from current position to snap target (only if position changes)
        cr->kd->carousel.anim_from = cr->kd->carousel.scroll_xf;
        cr->kd->carousel.anim_to = (double)snap_sx;
        if (cr->kd->carousel.anim_from != cr->kd->carousel.anim_to) {
          cr->kd->carousel.anim_start = cogito_now();
          if (!cr->kd->carousel.animating) cogito_carousel_animating_count++;
          cr->kd->carousel.animating = true;
        } else {
          cr->kd->carousel.animating = false;
          cr->kd->carousel.scroll_xf = cr->kd->carousel.anim_to;
        }
        cr->kd->carousel.active_index = snap_idx;
        
        if (cogito_pointer_capture == cr) {
          cogito_pointer_capture = NULL;
        }
        cr->kd->carousel.dragging = false;
        cogito_carousel_active = NULL;
        
        if (cogito_backend && cogito_backend->set_cursor) {
          cogito_backend->set_cursor(COGITO_CURSOR_DEFAULT);
        }
      }
    }
    // Bottom sheet active handling
    if (cogito_bottom_sheet_active) {
      if (left_down) {
        CogitoNode* bs = cogito_bottom_sheet_active;
        CogitoNode* host = bs->parent;
        if (host) {
          int host_y = host->y;
          int host_h = host->h;
          if (host->kind == COGITO_ZSTACK && host->parent && host->parent->kind == COGITO_WINDOW) {
            host_y = 0;
            host_h = host->parent->h;
          }
          int dy = bs->kd->bottom_sheet.drag_start_y - my;
          bs->kd->bottom_sheet.desired_h = bs->kd->bottom_sheet.drag_start_h + dy;
          int clamped_h = cogito_bottom_sheet_target_height(bs, host_y, host_h);
          bs->kd->bottom_sheet.desired_h = clamped_h;
          int top_inset = cogito_bottom_sheet_top_inset(host_y);
          bs->kd->bottom_sheet.expanded = clamped_h >= (host_h - top_inset - 8);
          cogito_window_relayout(win);
        }
      } else {
        CogitoNode* bs = cogito_bottom_sheet_active;
        CogitoNode* host = bs->parent;
        if (host) {
          int host_y = host->y;
          int host_h = host->h;
          if (host->kind == COGITO_ZSTACK && host->parent && host->parent->kind == COGITO_WINDOW) {
            host_y = 0;
            host_h = host->parent->h;
          }
          int snap_h = cogito_bottom_sheet_snap_height(bs, host_y, host_h, bs->h);
          bs->kd->bottom_sheet.desired_h = snap_h;
          int top_inset = cogito_bottom_sheet_top_inset(host_y);
          bs->kd->bottom_sheet.expanded = snap_h >= (host_h - top_inset - 8);
          cogito_window_relayout(win);
        }
         bs->kd->bottom_sheet.dragging = false;
         if (cogito_pointer_capture == bs) {
           cogito_pointer_capture = NULL;
        }
         cogito_bottom_sheet_active = NULL;
      }
    }
    // Side sheet active handling
     if (cogito_side_sheet_active) {
       if (left_down) {
         CogitoNode* ss = cogito_side_sheet_active;
         CogitoNode* host = ss->parent;
         if (host) {
           int host_x = host->x;
           int host_w = host->w;
           if (host->kind == COGITO_ZSTACK && host->parent && host->parent->kind == COGITO_WINDOW) {
             host_x = 0;
             host_w = host->parent->w;
          }
           int dx = ss->kd->side_sheet.drag_start_x - mx;
           if (cogito_is_rtl()) dx = -dx;
           ss->kd->side_sheet.desired_w = ss->kd->side_sheet.drag_start_w + dx;
           int clamped_w = cogito_side_sheet_target_width(ss, host_x, host_w);
           ss->kd->side_sheet.desired_w = clamped_w;
           int left_inset = cogito_side_sheet_left_inset(host_x);
           ss->kd->side_sheet.expanded = clamped_w >= (host_w - left_inset - 8);
           cogito_window_relayout(win);
        }
      } else {
         CogitoNode* ss = cogito_side_sheet_active;
         CogitoNode* host = ss->parent;
         if (host) {
           int host_x = host->x;
           int host_w = host->w;
           if (host->kind == COGITO_ZSTACK && host->parent && host->parent->kind == COGITO_WINDOW) {
             host_x = 0;
             host_w = host->parent->w;
          }
           int snap_w = cogito_side_sheet_snap_width(ss, host_x, host_w, ss->w);
           ss->kd->side_sheet.desired_w = snap_w;
           int left_inset = cogito_side_sheet_left_inset(host_x);
           ss->kd->side_sheet.expanded = snap_w >= (host_w - left_inset - 8);
           cogito_window_relayout(win);
        }
         ss->kd->side_sheet.dragging = false;
         if (cogito_pointer_capture == ss) {
           cogito_pointer_capture = NULL;
        }
         cogito_side_sheet_active = NULL;
        cogito_bottom_sheet_active = NULL;
      }
    }
    // Color picker active handling
    if (cogito_colorpicker_active) {
      if (left_down) {
        CogitoNode* cp = cogito_colorpicker_active;
        int chan = cogito_colorpicker_channel;
        if (chan >= 0) {
          if (cogito_colorpicker_set_from_mouse(cp, chan, mx)) {
            cogito_colorpicker_panel_apply_hct_to_sliders(cp);
            cogito_window_relayout(win);
          }
        }
      } else {
        if (cogito_pointer_capture == cogito_colorpicker_active) {
          cogito_pointer_capture = NULL;
        }
        cogito_colorpicker_active = NULL;
        cogito_colorpicker_channel = -1;
      }
    }
    // Time picker dial drag handling
    if (cogito_timepicker_active) {
      if (left_down) {
        if (cogito_timepicker_drag_update(cogito_timepicker_active, mx, my)) {
          cogito_window_relayout(win);
        }
      } else {
        if (cogito_pointer_capture == cogito_timepicker_active) {
          cogito_pointer_capture = NULL;
        }
        cogito_timepicker_active = NULL;
      }
    }
    // Drawing area drag/release handling
    if (cogito_pointer_capture && cogito_pointer_capture->kind == COGITO_DRAWING_AREA) {
      CogitoNode* da = cogito_pointer_capture;
      bool left_down_da = left_down;
      bool stroke_active = da->kd->drawing_area.pressed || da->kd->drawing_area.dragging;
      if (left_down_da && da->kd->drawing_area.pressed) {
        int px = mx - da->x;
        int py = my - da->y;
        if (px != da->kd->drawing_area.pointer_x || py != da->kd->drawing_area.pointer_y) {
          da->kd->drawing_area.pointer_x = px;
          da->kd->drawing_area.pointer_y = py;
          cogito_drawing_area_add_point(da, px, py);
          da->kd->drawing_area.dragging = true;
          cogito_invoke_change(da);
          cogito_window_relayout(win);
        }
      } else if (!left_down_da) {
        da->kd->drawing_area.pointer_x = mx - da->x;
        da->kd->drawing_area.pointer_y = my - da->y;
        da->kd->drawing_area.pressed = false;
        da->kd->drawing_area.dragging = false;
        if (stroke_active && da->on_action) {
          YisVal arg = YV_OBJ(da);
          cogito_invoke(da->on_action, 1, &arg);
        }
        cogito_pointer_capture = NULL;
      }
    }
    // ViewDual drag handling
    if (cogito_pointer_capture && cogito_pointer_capture->kind == COGITO_VIEW_DUAL) {
      CogitoNode* vd = cogito_pointer_capture;
      const float edge_ratio_zone = 0.05f;
      if (left_down) {
        int drag_delta_x = mx - vd->kd->view_dual.drag_start_mouse_x;
        if (drag_delta_x < 0) drag_delta_x = -drag_delta_x;
        if (!vd->kd->view_dual.drag_committed && drag_delta_x >= COGITO_VIEW_DUAL_DRAG_COMMIT_DP) {
          vd->kd->view_dual.drag_committed = true;
        }
        if (!vd->kd->view_dual.drag_committed) {
          // Click-without-drag on the handle should not reveal a hidden pane.
        } else {
        int gap = vd->gap > 0 ? vd->gap : 8;
        int handle_w = 4;
        int split_w = gap * 2 + handle_w;
        const int min_item_w = COGITO_VIEW_DUAL_MIN_ITEM_W;
        if (vd->w > 0) {
          int content_x = vd->x + vd->padding_left;
          int content_w = vd->w - vd->padding_left - vd->padding_right;
          if (content_w < 0) content_w = 0;
          int available_w = content_w - split_w;
          if (available_w < 0) available_w = 0;
          int edge_left_x = content_x + 4;
          int edge_right_x = content_x + content_w - handle_w - 4;
          int new_handle_x = mx - vd->kd->view_dual.drag_offset;
          float new_ratio = vd->kd->view_dual.ratio;
          vd->kd->view_dual.edge_snap = 0;
          if (new_handle_x <= edge_left_x) {
            new_ratio = 0.0f;
            vd->kd->view_dual.edge_snap = -1;
          } else if (new_handle_x >= edge_right_x) {
            new_ratio = 1.0f;
            vd->kd->view_dual.edge_snap = 1;
          } else {
            int raw_left_w = new_handle_x - content_x - gap;
            const int min_detente_w = COGITO_VIEW_DUAL_DETENTE_TOLERANCE;
            int new_left_w = raw_left_w;
            float raw_ratio = content_w > 0
              ? (float)(raw_left_w + split_w / 2) / (float)content_w
              : 0.5f;
            if (raw_ratio <= edge_ratio_zone) {
              new_ratio = 0.0f;
              vd->kd->view_dual.edge_snap = -1;
            } else if (raw_ratio >= (1.0f - edge_ratio_zone)) {
              new_ratio = 1.0f;
              vd->kd->view_dual.edge_snap = 1;
            } else if (available_w >= min_item_w * 2) {
              int max_left = available_w - min_item_w;
              int hide_left_threshold = min_item_w - COGITO_VIEW_DUAL_HIDE_INTENT_FROM_DETENTE;
              int hide_right_threshold = max_left + COGITO_VIEW_DUAL_HIDE_INTENT_FROM_DETENTE;
              if (raw_left_w <= hide_left_threshold) {
                new_ratio = 0.0f;
                vd->kd->view_dual.edge_snap = -1;
              } else if (raw_left_w >= hide_right_threshold) {
                new_ratio = 1.0f;
                vd->kd->view_dual.edge_snap = 1;
              } else {
                if (new_left_w < min_item_w) new_left_w = min_item_w;
                if (new_left_w > max_left) new_left_w = max_left;
                new_ratio = content_w > 0
                  ? (float)(new_left_w + split_w / 2) / (float)content_w
                  : 0.5f;
              }
            } else {
              if (raw_left_w <= -min_detente_w) {
                new_ratio = 0.0f;
                vd->kd->view_dual.edge_snap = -1;
              } else if (raw_left_w >= (available_w + min_detente_w)) {
                new_ratio = 1.0f;
                vd->kd->view_dual.edge_snap = 1;
              } else {
                new_left_w = available_w / 2;
                new_ratio = content_w > 0
                  ? (float)(new_left_w + split_w / 2) / (float)content_w
                  : 0.5f;
              }
            }
            if (vd->kd->view_dual.edge_snap == 0) {
              int min_left = 0;
              int max_left = available_w;
              if (available_w >= min_item_w * 2) {
                min_left = min_item_w;
                max_left = available_w - min_item_w;
              }
              if (new_left_w < 0) new_left_w = 0;
              if (new_left_w > available_w) new_left_w = available_w;

              int best_left = new_left_w;
              int best_dist = COGITO_VIEW_DUAL_DETENTE_TOLERANCE + 1;

              int d360 = new_left_w - COGITO_VIEW_DUAL_DETENTE_W_1;
              if (d360 < 0) d360 = -d360;
              if (COGITO_VIEW_DUAL_DETENTE_W_1 >= min_left && COGITO_VIEW_DUAL_DETENTE_W_1 <= max_left &&
                  d360 <= COGITO_VIEW_DUAL_DETENTE_TOLERANCE && d360 < best_dist) {
                best_left = COGITO_VIEW_DUAL_DETENTE_W_1;
                best_dist = d360;
              }

              int d412 = new_left_w - COGITO_VIEW_DUAL_DETENTE_W_2;
              if (d412 < 0) d412 = -d412;
              if (COGITO_VIEW_DUAL_DETENTE_W_2 >= min_left && COGITO_VIEW_DUAL_DETENTE_W_2 <= max_left &&
                  d412 <= COGITO_VIEW_DUAL_DETENTE_TOLERANCE && d412 < best_dist) {
                best_left = COGITO_VIEW_DUAL_DETENTE_W_2;
                best_dist = d412;
              }

              int center_left = available_w / 2;
              int dcenter = new_left_w - center_left;
              if (dcenter < 0) dcenter = -dcenter;
              if (center_left >= min_left && center_left <= max_left &&
                  dcenter <= COGITO_VIEW_DUAL_DETENTE_TOLERANCE && dcenter < best_dist) {
                best_left = center_left;
                best_dist = dcenter;
              }

              if (best_dist <= COGITO_VIEW_DUAL_DETENTE_TOLERANCE) {
                new_left_w = best_left;
                new_ratio = content_w > 0
                  ? (float)(new_left_w + split_w / 2) / (float)content_w
                  : 0.5f;
              }
            }
          }
          if (new_ratio < 0.0f) new_ratio = 0.0f;
          if (new_ratio > 1.0f) new_ratio = 1.0f;
          if (new_ratio <= edge_ratio_zone) {
            new_ratio = 0.0f;
            vd->kd->view_dual.edge_snap = -1;
          } else if (new_ratio >= (1.0f - edge_ratio_zone)) {
            new_ratio = 1.0f;
            vd->kd->view_dual.edge_snap = 1;
          }
          if (new_ratio != vd->kd->view_dual.ratio) {
            cogito_view_dual_set_ratio_target(vd, new_ratio, false);
            cogito_window_relayout(win);
          }
        }
        }
      } else {
        if (vd->kd->view_dual.edge_snap < 0 || vd->kd->view_dual.ratio <= edge_ratio_zone) {
          cogito_view_dual_set_ratio_target(vd, 0.0f, true);
          cogito_window_relayout(win);
        } else if (vd->kd->view_dual.edge_snap > 0 || vd->kd->view_dual.ratio >= (1.0f - edge_ratio_zone)) {
          cogito_view_dual_set_ratio_target(vd, 1.0f, true);
          cogito_window_relayout(win);
        }
        vd->kd->view_dual.edge_snap = 0;
        vd->kd->view_dual.dragging = false;
        vd->kd->view_dual.drag_committed = false;
        cogito_pointer_capture = NULL;
      }
    }
    // Text selection handling
    if (cogito_focused && !cogito_focused->disabled && !cogito_node_is_hidden(cogito_focused) && (focused_text_editable || focused_color_hex_edit)) {
      bool selecting = (cogito_focused->kind == COGITO_COLORPICKER)
        ? cogito_focused->kd->colorpicker.selecting
        : cogito_focused->kd->text_input.selecting;
      if (left_down && selecting) {
        int pad = 8;
        int size = cogito_node_font_size(cogito_focused);
        const char* t = cogito_focused->text ? cogito_focused->text->data : "";
        if (cogito_focused->kind == COGITO_TEXTFIELD || cogito_focused->kind == COGITO_SEARCHFIELD) {
          int text_x = cogito_focused->x + 16;
          if (cogito_focused->kind == COGITO_SEARCHFIELD) {
            text_x = cogito_focused->x + (12 + 16 + 8);
          } else if (cogito_focused->icon && cogito_focused->icon->data && cogito_focused->icon->data[0]) {
            text_x = cogito_focused->x + (12 + 24 + 16);
          }
          if (cogito_focused->textfield_prefix && cogito_focused->textfield_prefix->data && cogito_focused->textfield_prefix->data[0]) {
            text_x += cogito_text_width_size_node(cogito_focused, cogito_focused->textfield_prefix->data, size);
          }
          int rel_x = mx - text_x;
          if (rel_x < 0) rel_x = 0;
          int idx = cogito_text_index_from_x(cogito_focused, t, size, rel_x);
          cogito_focused->kd->text_input.sel_end = idx;
          cogito_focused->kd->text_input.caret = idx;
        } else {
          if (cogito_focused->kind == COGITO_TEXTVIEW) {
            int rel_x = mx - (cogito_focused->x + pad);
            if (rel_x < 0) rel_x = 0;
            int line_h = cogito_text_height_size(size);
            int idx = cogito_text_line_index_from_pos(cogito_focused, t, size, cogito_focused->w - pad * 2, cogito_focused->y + pad, line_h, rel_x, my);
            cogito_focused->kd->text_input.sel_end = idx;
            cogito_focused->kd->text_input.caret = idx;
          } else if (cogito_focused->kind == COGITO_COLORPICKER) {
            int hx = 0, hy = 0, hw = 0, hh = 0;
            cogito_colorpicker_hex_rect(cogito_focused, &hx, &hy, &hw, &hh);
            int rel_x = mx - (hx + pad);
            if (rel_x < 0) rel_x = 0;
            int idx = cogito_text_index_from_x(cogito_focused, t, size, rel_x);
            cogito_focused->kd->colorpicker.sel_end = idx;
            cogito_focused->kd->colorpicker.caret = idx;
          }
        }
      }
      if (left_released) {
        if (cogito_focused->kind == COGITO_COLORPICKER) {
          cogito_focused->kd->colorpicker.selecting = false;
        } else {
          cogito_focused->kd->text_input.selecting = false;
        }
      }
    }
    /* Scrollbar thumb drag and track click (overlay scrollbars) */
    if (left_released && cogito_scrollbar_in_use) {
      cogito_scrollbar_in_use->kd->scroller.bar_in_use = false;
      cogito_scrollbar_in_use = NULL;
    }
    if (cogito_scrollbar_drag_scroller) {
      if (!left_down) {
        cogito_scrollbar_drag_scroller = NULL;
      } else {
        CogitoNode* sc = cogito_scrollbar_drag_scroller;
        int cur = cogito_scrollbar_drag_vert ? my : mx;
        int delta = cur - cogito_scrollbar_drag_start_mouse;
        // In RTL mode, horizontal scrollbar drag direction is inverted
        if (!cogito_scrollbar_drag_vert && cogito_is_rtl()) delta = -delta;
        int new_scroll = cogito_scrollbar_drag_start_scroll + (int)((long)delta * (long)cogito_scrollbar_drag_max / (long)cogito_scrollbar_drag_travel);
        if (new_scroll < 0) new_scroll = 0;
        if (new_scroll > cogito_scrollbar_drag_max) new_scroll = cogito_scrollbar_drag_max;
        if (cogito_scrollbar_drag_vert) sc->kd->scroller.scroll_y = new_scroll;
        else sc->kd->scroller.scroll_x = new_scroll;
        CogitoNode* synced = cogito_scroller_sync_from(sc);
        cogito_relayout_subtree(sc);
        if (synced) cogito_relayout_subtree(synced);
        cogito_queue_draw(win);
      }
    } else if (left_pressed && route_main_pointer) {
      CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
        ? win->dialog
        : cogito_find_dialog_slot(win);
      CogitoNode* root = (slot && slot->dialog) ? slot->dialog : win;
      bool prefer_handle_hit = cogito_point_hits_preferred_handle(root, mx, my);
      CogitoNode* sc = prefer_handle_hit ? NULL : cogito_find_scroller(root, mx, my);
      if (sc) {
        CogitoScrollbarHit hit = cogito_scroller_hit_scrollbar(sc, mx, my, COGITO_SCROLLBAR_WIDTH_WIDE, COGITO_SCROLLBAR_WIDTH_WIDE);
        /* If the deepest scroller has no scrollbar under the point, the click may be on an outer scroller's bar. Walk up to find the outermost ancestor scroller whose scrollbar is under the point. */
        if (hit == COGITO_SCROLLBAR_HIT_NONE) {
          CogitoNode* p = sc->parent;
          while (p) {
            if (p->kind == COGITO_SCROLLER && cogito_hit_rect(mx, my, p->x, p->y, p->w, p->h)) {
              CogitoScrollbarHit phit = cogito_scroller_hit_scrollbar(p, mx, my, COGITO_SCROLLBAR_WIDTH_WIDE, COGITO_SCROLLBAR_WIDTH_WIDE);
              if (phit != COGITO_SCROLLBAR_HIT_NONE) {
                sc = p;
                hit = phit;
              }
            }
            p = p->parent;
          }
        }
        if (hit == COGITO_SCROLLBAR_HIT_VERT_THUMB) {
          int max_s = 0, travel = 1;
          if (cogito_scroller_vert_drag_params(sc, &max_s, &travel)) {
            sc->kd->scroller.bar_in_use = true;
            cogito_scrollbar_in_use = sc;
            cogito_scrollbar_drag_scroller = sc;
            cogito_scrollbar_drag_vert = true;
            cogito_scrollbar_drag_start_mouse = my;
            cogito_scrollbar_drag_start_scroll = sc->kd->scroller.scroll_y;
            cogito_scrollbar_drag_max = max_s;
            cogito_scrollbar_drag_travel = travel;
            click = false;
          }
        } else if (hit == COGITO_SCROLLBAR_HIT_HORZ_THUMB) {
          int max_s = 0, travel = 1;
          if (cogito_scroller_horz_drag_params(sc, &max_s, &travel)) {
            sc->kd->scroller.bar_in_use = true;
            cogito_scrollbar_in_use = sc;
            cogito_scrollbar_drag_scroller = sc;
            cogito_scrollbar_drag_vert = false;
            cogito_scrollbar_drag_start_mouse = mx;
            cogito_scrollbar_drag_start_scroll = sc->kd->scroller.scroll_x;
            cogito_scrollbar_drag_max = max_s;
            cogito_scrollbar_drag_travel = travel;
            click = false;
          }
        } else if (hit == COGITO_SCROLLBAR_HIT_VERT_TRACK) {
          int vtx, vty, vtw, vth, vux, vuy, vuw, vuh;
          if (cogito_scroller_vert_scrollbar_rects(sc, COGITO_SCROLLBAR_WIDTH_WIDE, &vtx, &vty, &vtw, &vth, &vux, &vuy, &vuw, &vuh)) {
            int max_s = 0, travel = 1;
            if (cogito_scroller_vert_drag_params(sc, &max_s, &travel)) {
              sc->kd->scroller.bar_in_use = true;
              cogito_scrollbar_in_use = sc;
              sc->kd->scroller.scroll_y = cogito_scroller_track_to_scroll(my, vty, vth, vuh, max_s);
              if (sc->kd->scroller.scroll_y < 0) sc->kd->scroller.scroll_y = 0;
              if (sc->kd->scroller.scroll_y > max_s) sc->kd->scroller.scroll_y = max_s;
              CogitoNode* synced = cogito_scroller_sync_from(sc);
              cogito_relayout_subtree(sc);
              if (synced) cogito_relayout_subtree(synced);
              cogito_queue_draw(win);
              click = false;
            }
          }
        } else if (hit == COGITO_SCROLLBAR_HIT_HORZ_TRACK) {
          int htx, hty, htw, hth, hux, huy, huw, huh;
          if (cogito_scroller_horz_scrollbar_rects(sc, COGITO_SCROLLBAR_WIDTH_WIDE, &htx, &hty, &htw, &hth, &hux, &huy, &huw, &huh)) {
            int max_s = 0, travel = 1;
            if (cogito_scroller_horz_drag_params(sc, &max_s, &travel)) {
              sc->kd->scroller.bar_in_use = true;
              cogito_scrollbar_in_use = sc;
              sc->kd->scroller.scroll_x = cogito_scroller_track_to_scroll(mx, htx, htw, huw, max_s);
              // In RTL, horizontal track click maps inversely
              if (cogito_is_rtl()) sc->kd->scroller.scroll_x = max_s - sc->kd->scroller.scroll_x;
              if (sc->kd->scroller.scroll_x < 0) sc->kd->scroller.scroll_x = 0;
              if (sc->kd->scroller.scroll_x > max_s) sc->kd->scroller.scroll_x = max_s;
              CogitoNode* synced = cogito_scroller_sync_from(sc);
              cogito_relayout_subtree(sc);
              if (synced) cogito_relayout_subtree(synced);
              cogito_queue_draw(win);
              click = false;
            }
          }
        }
      }
    }
    // Click handling
    if (click) {
      if (cogito_inspector_open) {
        bool consumed = false;
        if (inspector_window) {
          if (inspector_mouse_in_frame) {
            int appbar_h = inspector_appbar_h;
            if (appbar_h > inspector_frame_h) appbar_h = inspector_frame_h;
            if (imy < inspector_frame_y + appbar_h + inspector_drag_hit_pad) {
              if (inspector_lights.over_close) {
                cogito_backend->window_destroy(inspector_window);
                inspector_window = NULL;
                cogito_inspector_open = false;
                inspector_native_drag = false;
                inspector_drag_pending = false;
                inspector_drag_active = false;
              } else if (inspector_lights.over_min) {
                if (cogito_backend->window_minimize) {
                  cogito_backend->window_minimize(inspector_window);
                }
              } else if (inspector_lights.over_max) {
                if (cogito_backend->window_is_maximized && cogito_backend->window_restore && cogito_backend->window_maximize) {
                  if (cogito_backend->window_is_maximized(inspector_window)) cogito_backend->window_restore(inspector_window);
                  else cogito_backend->window_maximize(inspector_window);
                } else if (cogito_backend->window_maximize) {
                  cogito_backend->window_maximize(inspector_window);
                }
              } else if (!inspector_native_drag) {
                inspector_drag_pending = true;
                inspector_drag_active = false;
                inspector_drag_start_gx = inspector_gx;
                inspector_drag_start_gy = inspector_gy;
                inspector_drag_start_wx = iwx;
                inspector_drag_start_wy = iwy;
              }
              consumed = true;
            } else {
              cogito_inspector_view_x = inspector_frame_x;
              cogito_inspector_view_y = inspector_frame_y + appbar_h;
              cogito_inspector_view_w = inspector_frame_w;
              if (cogito_inspector_view_w < 0) cogito_inspector_view_w = 0;
              cogito_inspector_view_h = inspector_frame_h - appbar_h - inspector_corner_r;
              if (cogito_inspector_view_h < 0) cogito_inspector_view_h = 0;
              (void)cogito_inspector_handle_click(win, imx, imy);
              consumed = true;
              cogito_inspector_view_x = 0;
              cogito_inspector_view_y = 0;
              cogito_inspector_view_w = 0;
              cogito_inspector_view_h = 0;
            }
          } else if (inspector_mouse_inside) {
            consumed = true;
          }
        } else {
          if (cogito_inspector_handle_click(win, mx, my)) {
            consumed = true;
          }
        }
        if (consumed) {
          click = false;
        } else {
          CogitoNode* picked = cogito_inspector_pick_node_recursive(win, mx, my);
          if (picked) {
            (void)cogito_inspector_commit_string_edit(win);
            cogito_inspector_selected = picked;
          }
        }
      }
    }
    if (click && !route_main_pointer) {
      click = false;
    }
    // Right-click on WindowBar opens its context menu
    if (right_pressed && !cogito_menu.open) {
      CogitoNode* wb = cogito_find_window_bar(win);
      if (wb && cogito_hit_node(wb, mx, my)) {
        cogito_menu_open_for(wb);
      }
    }
    // Left-click on WindowBar AppMenu button opens the menu
    if (click && !cogito_menu.open) {
      CogitoNode* wb = cogito_find_window_bar(win);
      if (wb && wb->kd) {
        int amx = wb->kd->window_bar.appmenu_x;
        int amy = wb->kd->window_bar.appmenu_y;
        int amw = wb->kd->window_bar.appmenu_w;
        int amh = wb->kd->window_bar.appmenu_h;
        if (mx >= amx && mx <= amx + amw && my >= amy && my <= amy + amh) {
          cogito_menu_open_for(wb);
          click = false; // consume the click
        }
      }
    }
    if (click) {
      cogito_theme_mark_action_origin(mx, my);
      static int cogito_debug_hit = -1;
      if (cogito_debug_hit < 0) {
        const char* env = getenv("COGITO_DEBUG_HIT");
        cogito_debug_hit = (env && env[
          0
        ] && env[
          0
        ] != '0') ? 1 : 0;
      }
      if (cogito_fab_menu_open_node) {
        // Any click closes the FAB menu; if an item was hit, invoke it first.
        CogitoNode* owner = cogito_fab_menu_open_node;
        int idx = cogito_fab_menu_item_hit(cogito_fab_menu_open_node, mx, my);
        if (idx >= 0) {
          cogito_invoke_menu(cogito_fab_menu_open_node, (size_t)idx);
        }
        cogito_fab_menu_open_node->popover_open = false;
        cogito_fab_menu_open_node = NULL;
        cogito_fab_menu_kbd_index = -1;
        cogito_overlay_focus_restore(win);
        cogito_a11y_emit_event("overlay-close", owner, "fab-menu");
        cogito_window_relayout(win);
      } else if (cogito_menu.open) {
        int idx = cogito_menu_hit(&cogito_menu, mx, my);
        if (idx >= 0 && cogito_menu.owner) {
          cogito_invoke_menu(cogito_menu.owner, (size_t)idx);
        }
        cogito_menu_close();
      } else {
        bool handled = false;
        CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
          ? win->dialog
          : cogito_find_dialog_slot(win);
        CogitoNode* active_dialog = (slot && slot->dialog)
          ? slot->dialog
          : ((win->dialog_open && win->dialog && win->dialog->kind == COGITO_DIALOG) ? win->dialog : NULL);
        if (win->popover_open && win->popover) {
          bool inside_popover = cogito_hit_node(win->popover, mx, my);
          bool inside_anchor = (win->popover_anchor && cogito_hit_node(win->popover_anchor, mx, my));
          if (!inside_popover && !inside_anchor) {
            cogito_popover_close(YV_OBJ(win));
          }
        }
        bool appbar_blocked_by_bottom_sheet = false;
        if (!active_dialog) {
          CogitoNode* appbar_pre_hit = cogito_find_clickable(win, mx, my);
          appbar_blocked_by_bottom_sheet = cogito_node_is_or_descends_from_kind(appbar_pre_hit, COGITO_BOTTOM_SHEET);
        }
        // Check for traffic light clicks on the first appbar
        if (!active_dialog && appbar && !appbar_blocked_by_bottom_sheet) {
          int btn_hit_pad = 1; // element_size + 2px target (1px on each side)
          if (appbar->kd->appbar.btn_close_x >= 0 &&
              cogito_hit_rect(mx, my,
                              appbar->kd->appbar.btn_close_x - btn_hit_pad,
                              appbar->kd->appbar.btn_y - btn_hit_pad,
                              appbar->kd->appbar.btn_size + 2,
                              appbar->kd->appbar.btn_size + 2)) {
            cogito_appbar_window_action(win,
            0);
            handled = true;
          } else if (appbar->kd->appbar.btn_min_x >= 0 &&
                     cogito_hit_rect(mx, my,
                                     appbar->kd->appbar.btn_min_x - btn_hit_pad,
                                     appbar->kd->appbar.btn_y - btn_hit_pad,
                                     appbar->kd->appbar.btn_size + 2,
                                     appbar->kd->appbar.btn_size + 2)) {
            cogito_appbar_window_action(win,
            1);
            handled = true;
          } else if (appbar->kd->appbar.btn_max_x >= 0 &&
                     cogito_hit_rect(mx, my,
                                     appbar->kd->appbar.btn_max_x - btn_hit_pad,
                                     appbar->kd->appbar.btn_y - btn_hit_pad,
                                     appbar->kd->appbar.btn_size + 2,
                                     appbar->kd->appbar.btn_size + 2)) {
            cogito_appbar_window_action(win,
            2);
            handled = true;
          }
        }
        // Check side sheet close button first (before appbar drag) since
        // the sheet overlays the appbar region and the close button lives
        // at the top of the sheet.
        if (!handled && win->side_sheet_open && win->side_sheet_node) {
          int sx = 0, sy = 0, sw = 0, sh = 0;
          cogito_side_sheet_close_rect(win->side_sheet_node, &sx, &sy, &sw, &sh);
          if (sw > 0 && sh > 0 && cogito_hit_rect_min48(mx, my, sx, sy, sw, sh)) {
            cogito_window_clear_side_sheet(YV_OBJ(win));
            cogito_window_relayout(win);
            handled = true;
          }
        }
        // Check if click is on ANY appbar for dragging (only if we didn't handle a button click)
        if (!handled && !active_dialog && !appbar_blocked_by_bottom_sheet) {
          CogitoNode* clicked_appbar = cogito_find_appbar_at_point(win, mx, my);
          // Use cogito_find_clickable on the appbar subtree directly — this
          // applies the same min-48 hit expansion the normal click path uses,
          // so a clickable widget always wins over the drag.
          if (clicked_appbar && !cogito_find_clickable(clicked_appbar, mx, my)) {
            appbar_drag_swallow = true;
#if defined(__APPLE__)
            cogito_macos_drag_window();
#else
            cogito_drag_pending = true;
            cogito_drag_pending_mouse.x = (float)mx;
            cogito_drag_pending_mouse.y = (float)my;
            cogito_drag_last_move = 0.0;
#endif
            handled = true; // Mark as handled to skip clickable search
          }
        }
        // Check if click is on the WindowBar for dragging
        if (!handled && !active_dialog) {
          CogitoNode* wb = cogito_find_window_bar(win);
          if (wb && cogito_hit_node(wb, mx, my)) {
            appbar_drag_swallow = true;
#if defined(__APPLE__)
            cogito_macos_drag_window();
#else
            cogito_drag_pending = true;
            cogito_drag_pending_mouse.x = (float)mx;
            cogito_drag_pending_mouse.y = (float)my;
            cogito_drag_last_move = 0.0;
#endif
            handled = true;
          }
        }
        if (!handled && active_dialog) {
          int dx = 0, dy = 0, dw = 0, dh = 0;
          cogito_dialog_close_rect(active_dialog, &dx, &dy, &dw, &dh);
          if (dw > 0 && dh > 0 && cogito_hit_rect_min48(mx, my, dx, dy, dw, dh)) {
            if (slot && slot->kind == COGITO_DIALOG_SLOT) {
              cogito_dialog_slot_clear(YV_OBJ(slot));
            } else {
              cogito_dialog_close(YV_OBJ(active_dialog));
            }
            handled = true;
          }
        }
        CogitoNode* hit = NULL;
        if (!handled) {
          CogitoNode* root = (slot && slot->dialog_open && slot->dialog) ? slot->dialog : (win->dialog_open && win->dialog ? win->dialog : win);
          hit = cogito_find_overlay_toast(root, mx, my);
          if (!hit && win->popover_open && win->popover) {
            hit = cogito_find_clickable(win->popover, mx, my);
          }
          if (!hit) {
            hit = (slot && slot->dialog_open && slot->dialog) ? cogito_find_clickable(slot, mx, my)
                                         : (win->dialog_open && win->dialog ? cogito_find_clickable(win->dialog, mx, my)
                                                                           : cogito_find_clickable(win, mx, my));
          }
        }
        if (cogito_debug_hit) {
          if (slot && slot->dialog_open && slot->dialog) {
            fprintf(stderr,
            "cogito: click (%d,%d) dialog rect (%d,%d %dx%d) hit=%p kind=%d\n",
                    mx, my, slot->dialog->x, slot->dialog->y, slot->dialog->w, slot->dialog->h,
                    (void*)hit, hit ? (int)hit->kind : -1);
          } else if (win->dialog_open && win->dialog) {
            fprintf(stderr,
            "cogito: click (%d,%d) legacy dialog rect (%d,%d %dx%d) hit=%p kind=%d\n",
                    mx, my, win->dialog->x, win->dialog->y, win->dialog->w, win->dialog->h,
                    (void*)hit, hit ? (int)hit->kind : -1);
          } else {
            fprintf(stderr,
            "cogito: click (%d,%d) no dialog hit=%p kind=%d\n",
                    mx, my, (void*)hit, hit ? (int)hit->kind : -1);
          }
          if (hit) {
            const char* t = (hit->text && hit->text->data) ? hit->text->data : "";
            fprintf(stderr,
            "cogito:   hit rect (%d,%d %dx%d) text=\"%s\"\n",
                    hit->x, hit->y, hit->w, hit->h, t);
          }
          fflush(stderr);
        }
        if (hit && hit->disabled) hit = NULL;
        CogitoNode* root = (slot && slot->dialog_open && slot->dialog) ? slot->dialog : (win->dialog_open && win->dialog ? win->dialog : win);
        CogitoNode* focus_hit = NULL;
        if (win->popover_open && win->popover) {
          focus_hit = cogito_find_text_input(win->popover, mx, my);
        }
        if (!focus_hit) {
          focus_hit = cogito_find_text_input(root, mx, my);
        }
          if (focus_hit && focus_hit->disabled) focus_hit = NULL;
        if (focus_hit) {
          if (cogito_focused && cogito_focused != focus_hit && cogito_focused->kind == COGITO_COLORPICKER) {
            cogito_focused->kd->colorpicker.hex_editing = false;
          }
          if (focus_hit != cogito_focused) cogito_a11y_emit_event("focus", focus_hit, "pointer");
          cogito_focused = focus_hit;
          cogito_focus_visible = true;
          int pad = 8;
          int size = cogito_node_font_size(focus_hit);
          const char* t = focus_hit->text ? focus_hit->text->data : "";
          if (focus_hit->kind == COGITO_TEXTFIELD || focus_hit->kind == COGITO_SEARCHFIELD) {
            int text_x = focus_hit->x + 16;
            if (focus_hit->kind == COGITO_SEARCHFIELD) {
              text_x = focus_hit->x + (12 + 16 + 8);
            } else if (focus_hit->icon && focus_hit->icon->data && focus_hit->icon->data[0]) {
              text_x = focus_hit->x + (12 + 24 + 16);
            }
            if (focus_hit->textfield_prefix && focus_hit->textfield_prefix->data && focus_hit->textfield_prefix->data[0]) {
              text_x += cogito_text_width_size_node(focus_hit, focus_hit->textfield_prefix->data, size);
            }
            int rel_x = mx - text_x;
            if (rel_x < 0) rel_x = 0;
            focus_hit->kd->text_input.caret = cogito_text_index_from_x(focus_hit, t, size, rel_x);
            focus_hit->kd->text_input.sel_start = focus_hit->kd->text_input.caret;
            focus_hit->kd->text_input.sel_end = focus_hit->kd->text_input.caret;
            focus_hit->kd->text_input.selecting = true;
          } else if (focus_hit->kind == COGITO_TEXTVIEW) {
            int rel_x = mx - (focus_hit->x + pad);
            if (rel_x < 0) rel_x = 0;
            int line_h = cogito_text_height_size(size);
            focus_hit->kd->text_input.caret = cogito_text_line_index_from_pos(focus_hit, t, size, focus_hit->w - pad * 2, focus_hit->y + pad, line_h, rel_x, my);
            focus_hit->kd->text_input.sel_start = focus_hit->kd->text_input.caret;
            focus_hit->kd->text_input.sel_end = focus_hit->kd->text_input.caret;
            focus_hit->kd->text_input.selecting = true;
          } else if (focus_hit->kind == COGITO_COLORPICKER) {
            int hx = 0, hy = 0, hw = 0, hh = 0;
            cogito_colorpicker_hex_rect(focus_hit, &hx, &hy, &hw, &hh);
            focus_hit->kd->colorpicker.hex_editing = cogito_hit_rect_min48(mx, my, hx, hy, hw, hh);
            int rel_x = mx - (hx + pad);
            if (rel_x < 0) rel_x = 0;
            focus_hit->kd->colorpicker.caret = cogito_text_index_from_x(focus_hit, t, size, rel_x);
            focus_hit->kd->colorpicker.sel_start = focus_hit->kd->colorpicker.caret;
            focus_hit->kd->colorpicker.sel_end = focus_hit->kd->colorpicker.caret;
            focus_hit->kd->colorpicker.selecting = true;
          }
        } else if (hit && cogito_node_focusable(hit)) {
          if (cogito_focused && cogito_focused != hit && cogito_focused->kind == COGITO_COLORPICKER) {
            cogito_focused->kd->colorpicker.hex_editing = false;
          }
          if (hit != cogito_focused) cogito_a11y_emit_event("focus", hit, "pointer");
          cogito_focused = hit;
          cogito_focus_visible = false;
        } else {
          if (cogito_focused && cogito_focused->kind == COGITO_COLORPICKER) {
            cogito_focused->kd->colorpicker.hex_editing = false;
          }
          cogito_focused = NULL;
          cogito_focus_visible = false;
        }
        if (hit) {
          // Trigger ripple for elements with Primary/Secondary/Tertiary backgrounds or Surface variant on button-like elements
          if (cogito_node_should_ripple(hit)) {
            cogito_ripple_start(hit, mx, my);
          }
          if (hit->kind == COGITO_BUTTON || hit->kind == COGITO_ICONBTN) {
            // Check if button is in a ButtonGroup - update selection first
            if (hit->parent && hit->parent->kind == COGITO_BUTTON_GROUP) {
              cogito_buttongroup_child_clicked(hit->parent, hit);
            }
            CogitoNode* about_host = (hit->kind == COGITO_BUTTON) ? cogito_about_window_for_action_button(hit) : NULL;
            const char* about_url = cogito_about_window_action_url(hit, about_host);
            if (about_host && about_url) {
              if (about_host->on_action) {
                YisVal arg = YV_OBJ(about_host);
                cogito_invoke(about_host->on_action,
                1, &arg);
              } else if (cogito_backend && cogito_backend->open_url) {
                cogito_backend->open_url(about_url);
              }
            } else if (hit->menu_len > 0) {
              cogito_menu_open_for(hit);
            } else {
              cogito_invoke_click(hit);
            }
          } else if (hit->kind == COGITO_FONTBUTTON) {
            cogito_fontbutton_click(hit);
          } else if (hit->kind == COGITO_SPLIT_BUTTON) {
            int arrow_x = hit->kd->split_btn.arrow_x;
            bool hit_arrow = cogito_is_rtl() ? (mx < arrow_x + hit->kd->split_btn.arrow_w) : (mx >= arrow_x);
            if (hit_arrow && hit->menu_len > 0) {
              cogito_menu_open_for(hit);
            } else {
              cogito_invoke_click(hit);
            }
          } else if (hit->kind == COGITO_VIEW_CHOOSER) {
            cogito_menu_open_for(hit);
          } else if (hit->kind == COGITO_CHECKBOX || hit->kind == COGITO_SWITCH || hit->kind == COGITO_SWITCHBAR) {
            bool prev = hit->checked;
            if (hit->kind == COGITO_CHECKBOX && hit->group.tag != EVT_NULL) {
              hit->checked = true;
              cogito_uncheck_group(win, hit, hit->group);
            } else {
              hit->checked = !hit->checked;
            }
            if (hit->checked && hit->group.tag != EVT_NULL) {
              cogito_uncheck_group(win, hit, hit->group);
            }
            if (!hit->checked) {
              hit->bg_set = false;
              hit->border_color_set = false;
              hit->border_width_set = false;
            }
            if (hit->checked != prev) {
              cogito_invoke_change(hit);
            }
          } else if (hit->kind == COGITO_DROPDOWN) {
            cogito_menu_open_for(hit);
          } else if (hit->kind == COGITO_SLIDER) {
            // Skip if slider was already activated by press handler
            if (cogito_slider_active != hit) {
              cogito_slider_active = hit;
              cogito_pointer_capture = hit;
              hit->kd->slider.dragging = true;
              if (hit->kd->slider.mode == COGITO_SLIDER_MODE_RANGE) {
                hit->kd->slider.active_handle = cogito_slider_pick_range_handle(hit, mx, my);
              } else {
                hit->kd->slider.active_handle = 0;
              }
            }
          } else if (hit->kind == COGITO_CAROUSEL_ITEM) {
            // Click on a carousel item: scroll to it and make it active
            CogitoNode* item = hit;
            CogitoNode* carousel = item->parent;
            if (carousel && carousel->kind == COGITO_CAROUSEL) {
              int max_scroll = 0;
              CAROUSEL_MAX_SCROLL(carousel, max_scroll);
              
              // Find the index of the clicked item
              int item_idx = -1;
              for (size_t i = 0; i < carousel->len; i++) {
                if (carousel->children[i
                ] == item) {
                  item_idx = (int)i;
                  break;
                }
              }
              
              if (item_idx >= 0) {
                // Calculate target scroll position to make this item active
                int snap_sx = 0;
                CAROUSEL_SNAP_SCROLL(carousel, item_idx, max_scroll, snap_sx);
                
                // Start animation to scroll to this item
                carousel->kd->carousel.anim_from = carousel->kd->carousel.scroll_xf;
                carousel->kd->carousel.anim_to = (double)snap_sx;
                if (carousel->kd->carousel.anim_from != carousel->kd->carousel.anim_to) {
                  carousel->kd->carousel.anim_start = cogito_now();
                  if (!carousel->kd->carousel.animating) cogito_carousel_animating_count++;
                  carousel->kd->carousel.animating = true;
                }
                carousel->kd->carousel.active_index = item_idx;
                cogito_window_relayout(win);
              }
            }
          } else if (hit->kind == COGITO_CAROUSEL) {
            cogito_carousel_active = hit;
            cogito_pointer_capture = hit;
            hit->kd->carousel.dragging = true;
            hit->kd->carousel.animating = false;
            hit->kd->carousel.drag_start_x = mx;
            hit->kd->carousel.drag_start_scroll = (int)hit->kd->carousel.scroll_xf;
            hit->kd->carousel.velocity = 0.0f;
            hit->kd->carousel.drag_last_time = cogito_now();
            hit->kd->carousel.drag_last_xf = hit->kd->carousel.scroll_xf;
            // Set cursor to grab
            if (cogito_backend && cogito_backend->set_cursor) {
              cogito_backend->set_cursor(COGITO_CURSOR_GRAB);
            }
          } else if (hit->kind == COGITO_TABS) {
            int idx = cogito_tabs_hit_index(hit, mx, my);
            if (idx >= 0) {
              cogito_tabs_set_selected(YV_OBJ(hit), YV_INT(idx));
              if (hit->on_select) {
                YisVal arg = YV_OBJ(hit);
                cogito_invoke(hit->on_select, 1, &arg);
              }
            }
          } else if (hit->kind == COGITO_DATEPICKER) {
            cogito_datepicker_click(hit, mx, my);
          } else if (hit->kind == COGITO_COLORPICKER) {
            if (hit->kd->colorpicker.popover_panel) {
              // Panel: handle footer first, then allow row-based drag as a fallback.
              if (!cogito_colorpicker_hit_panel(hit, mx, my)) {
                int chan = cogito_colorpicker_panel_hit_channel(hit, mx, my);
                if (chan >= 0) {
                  cogito_colorpicker_active = hit;
                  cogito_colorpicker_channel = chan;
                  cogito_pointer_capture = hit;
                  hit->kd->colorpicker.hex_editing = false;
                  if (cogito_colorpicker_set_from_mouse(hit, chan, mx)) {
                    cogito_colorpicker_panel_apply_hct_to_sliders(hit);
                    cogito_window_relayout(win);
                  }
                }
              }
            } else {
              // Trigger: open popover (icon zone) or enter hex edit (handled by focus block)
              cogito_colorpicker_click(hit, mx, my);
            }
          } else if (hit->kind == COGITO_TOAST) {
            if (hit->action_text && hit->action_button_w > 0 &&
                cogito_hit_rect_min48(mx, my, hit->action_button_x, hit->action_button_y,
                                      hit->action_button_w, hit->action_button_h)) {
              if (hit->on_action) {
                YisVal arg = YV_OBJ(hit);
                cogito_invoke(hit->on_action,
                1, &arg);
              }
            } else {
              cogito_invoke_click(hit);
            }
          } else if (hit->kind == COGITO_CHIP) {
            int cx = 0, cy = 0, cw = 0, ch = 0;
            cogito_chip_close_rect(hit, &cx, &cy, &cw, &ch);
            if (cw > 0 && ch > 0 && cogito_hit_rect_min48(mx, my, cx, cy, cw, ch)) {
              // Clicked on close button
              if (hit->on_action) {
                YisVal arg = YV_OBJ(hit);
                cogito_invoke(hit->on_action,
                1, &arg);
              }
            } else {
              // Clicked on chip body - toggle selected state
              hit->kd->chip.selected = !hit->kd->chip.selected;
              cogito_invoke_click(hit);
              cogito_window_relayout(win);
            }
          } else if (hit->kind == COGITO_BOTTOM_SHEET) {
            int hx = 0, hy = 0, hw = 0, hh = 0;
            cogito_bottom_sheet_handle_rect(hit, &hx, &hy, &hw, &hh);
            if (cogito_hit_rect_min48(mx, my, hx, hy, hw, hh)) {
              hit->kd->bottom_sheet.dragging = true;
              hit->kd->bottom_sheet.drag_start_y = my;
              hit->kd->bottom_sheet.drag_start_h = hit->h;
              hit->kd->bottom_sheet.desired_h = hit->h;
              cogito_bottom_sheet_active = hit;
              cogito_pointer_capture = hit;
            } else if (!cogito_hit_node(hit, mx, my)) {
              // Click on scrim (outside sheet body) — minimize
              int min_h = cogito_bottom_sheet_min_height(hit);
              hit->kd->bottom_sheet.desired_h = min_h;
              hit->kd->bottom_sheet.expanded = false;
              hit->kd->bottom_sheet.dragging = false;
              if (cogito_bottom_sheet_active == hit) cogito_bottom_sheet_active = NULL;
              if (cogito_pointer_capture == hit) cogito_pointer_capture = NULL;
              cogito_window_relayout(win);
            }
          } else if (hit->kind == COGITO_SIDE_SHEET) {
            int hx = 0, hy = 0, hw = 0, hh = 0;
            cogito_side_sheet_handle_rect(hit, &hx, &hy, &hw, &hh);
            if (cogito_hit_rect_min48(mx, my, hx, hy, hw, hh)) {
              hit->kd->side_sheet.dragging = true;
              hit->kd->side_sheet.drag_start_x = mx;
              hit->kd->side_sheet.drag_start_w = hit->w;
              hit->kd->side_sheet.desired_w = hit->w;
              cogito_side_sheet_active = hit;
              cogito_pointer_capture = hit;
            } else if (!cogito_hit_node(hit, mx, my)) {
              // Click on scrim (outside sheet body) — close
              if (hit->kd->side_sheet.mode != COGITO_SIDE_SHEET_SIDEBAR) {
                cogito_window_clear_side_sheet(YV_OBJ(win));
                cogito_window_relayout(win);
              }
            }
          } else if (hit->kind == COGITO_DRAWING_AREA) {
            hit->kd->drawing_area.pressed = true;
            hit->kd->drawing_area.dragging = false;
            hit->kd->drawing_area.pointer_x = mx - hit->x;
            hit->kd->drawing_area.pointer_y = my - hit->y;
            hit->kd->drawing_area.press_x = hit->kd->drawing_area.pointer_x;
            hit->kd->drawing_area.press_y = hit->kd->drawing_area.pointer_y;
            cogito_drawing_area_begin_stroke(
                hit, hit->kd->drawing_area.pointer_x, hit->kd->drawing_area.pointer_y);
            cogito_pointer_capture = hit;
            cogito_invoke_click(hit);
            cogito_window_relayout(win);
          } else if (hit->kind == COGITO_FAB) {
            cogito_invoke_click(hit);
          } else if (hit->kind == COGITO_FAB_MENU) {
            // FAB Menu requires 2 to 7 items to open
            if (hit->menu_len >= 2 && hit->menu_len <= 7) {
              cogito_overlay_focus_snapshot();
              hit->popover_open = true;
              cogito_fab_menu_open_node = hit;
              cogito_fab_menu_kbd_index = 0;
              cogito_a11y_emit_event("overlay-open", hit, "fab-menu");
              cogito_window_relayout(cogito_node_window(hit));
            }
          } else if (hit->kind == COGITO_STEPPER) {
            cogito_stepper_click(hit, mx, my);
          } else if (hit->kind == COGITO_TIMEPICKER) {
            cogito_timepicker_click(hit, mx, my);
          } else if (hit->kind == COGITO_TREEVIEW) {
            if (cogito_treeview_click(hit, mx, my)) {
              cogito_window_relayout(win);
            }
          } else if (hit->kind == COGITO_NAV_RAIL) {
            if (cogito_nav_rail_hit_toggle(hit, mx, my)) {
              hit->kd->nav_rail.expanded = !hit->kd->nav_rail.expanded;
              cogito_window_relayout(win);
            } else {
              int idx = cogito_nav_rail_hit_index(hit, mx, my);
              if (idx >= 0) {
                if (cogito_fab_menu_open_node) {
                  CogitoNode* owner = cogito_fab_menu_open_node;
                  cogito_fab_menu_open_node->popover_open = false;
                  cogito_fab_menu_open_node = NULL;
                  cogito_fab_menu_kbd_index = -1;
                  cogito_overlay_focus_restore(win);
                  cogito_a11y_emit_event("overlay-close", owner, "fab-menu");
                }
                cogito_handle_nav_rail_select(hit, idx);
                cogito_window_relayout(win);
              }
            }
          } else if (hit->kind == COGITO_BOTTOM_NAV) {
            int idx = cogito_bottom_nav_hit_index(hit, mx, my);
            if (idx >= 0) {
              if (cogito_fab_menu_open_node) {
                CogitoNode* owner = cogito_fab_menu_open_node;
                cogito_fab_menu_open_node->popover_open = false;
                cogito_fab_menu_open_node = NULL;
                cogito_fab_menu_kbd_index = -1;
                cogito_overlay_focus_restore(win);
                cogito_a11y_emit_event("overlay-close", owner, "fab-menu");
              }
              cogito_handle_bottom_nav_select(hit, idx);
              cogito_window_relayout(win);
            }
          } else if (hit->kind == COGITO_SETTINGS_WINDOW) {
            int sidebar_w = 200;
            if (mx >= hit->x && mx < hit->x + sidebar_w && my >= hit->y && my < hit->y + hit->h) {
              int item_h = 40;
              int idx = (my - hit->y - 48) / item_h;
              if (idx >= 0 && idx < (int)hit->len) {
                hit->selected = idx;
                cogito_window_relayout(win);
              }
            }
          } else if (hit->kind == COGITO_EMPTY_PAGE || hit->kind == COGITO_WELCOME_SCREEN || hit->kind == COGITO_ABOUT_WINDOW) {
            if (hit->action_text && hit->action_text->data && hit->action_text->data[
              0
            ]) {
              if (hit->action_button_w > 0 && cogito_hit_rect_min48(mx, my, hit->action_button_x, hit->action_button_y,
                          hit->action_button_w, hit->action_button_h)) {
                bool welcome_has_seen = (hit->kind == COGITO_WELCOME_SCREEN && cogito_welcome_screen_has_seen_enabled(hit));
                if (hit->on_action) {
                  YisVal arg = YV_OBJ(hit);
                  cogito_invoke(hit->on_action,
                  1, &arg);
                } else if (hit->kind == COGITO_ABOUT_WINDOW && cogito_backend && cogito_backend->open_url) {
                  cogito_backend->open_url(hit->action_text->data);
                }
                if (welcome_has_seen && hit->hint && hit->hint->data) {
                  cogito_welcome_screen_has_seen_set(hit->hint->data, true);
                }
                if (hit->kind == COGITO_WELCOME_SCREEN) {
                  CogitoNode* parent = hit->parent;
                  if (parent && parent->kind == COGITO_DIALOG_SLOT) {
                    cogito_dialog_slot_clear(YV_OBJ(parent));
                  } else if (win && win->dialog_open) {
                    cogito_window_clear_dialog(YV_OBJ(win));
                  }
                }
              }
            }
          } else if (hit->kind == COGITO_DIALOG_SLOT) {
            // Click on scrim (outside dialog) — dismiss
            if (hit->dialog_open && hit->dialog) {
              CogitoNode* dlg = hit->dialog;
              bool inside_dialog = (mx >= dlg->x && mx < dlg->x + dlg->w &&
                                    my >= dlg->y && my < dlg->y + dlg->h);
              if (!inside_dialog) {
                cogito_dialog_slot_clear(YV_OBJ(hit));
              }
            } else if (hit->dialog_open) {
              cogito_dialog_slot_clear(YV_OBJ(hit));
            }
          } else if (hit->kind == COGITO_CARD) {
            cogito_invoke_click(hit);
          } else if (hit->kind == COGITO_WEBVIEW) {
            cogito_invoke_click(hit);
            if (hit->kd->webview.open_external_on_click &&
                hit->icon && hit->icon->data && hit->icon->data[0] &&
                cogito_backend && cogito_backend->open_url) {
              const char* src = hit->icon->data;
              char href[2048];
              href[0] = '\0';
              if (cogito_wv_extract_first_href(src, href, (int)sizeof(href))) {
                cogito_backend->open_url(href);
              } else if (cogito_wv_is_probably_url(src)) {
                cogito_backend->open_url(src);
              }
            }
          } else if (hit->kind == COGITO_VIEW_DUAL) {
            float ratio = cogito_view_dual_current_ratio(hit);
            if (hit->kd->view_dual.ratio <= COGITO_VIEW_DUAL_EDGE_RATIO_ZONE && hit->kd->view_dual.ratio > 0.0f) {
              cogito_view_dual_set_ratio_target(hit, 0.0f, false);
              ratio = 0.0f;
              cogito_window_relayout(win);
            } else if (hit->kd->view_dual.ratio >= (1.0f - COGITO_VIEW_DUAL_EDGE_RATIO_ZONE) && hit->kd->view_dual.ratio < 1.0f) {
              cogito_view_dual_set_ratio_target(hit, 1.0f, false);
              ratio = 1.0f;
              cogito_window_relayout(win);
            }
            int handle_x = 0;
            int handle_w = 0;
            int intent_x = 0;
            int intent_y = 0;
            int intent_w = 0;
            int intent_h = 0;
            cogito_view_dual_handle_targets_for_ratio(hit, ratio,
                                                      &handle_x, &handle_w,
                                                      &intent_x, &intent_y,
                                                      &intent_w, &intent_h);
            if (hit->kd->view_dual.show_handle && cogito_hit_rect(mx, my, intent_x, intent_y, intent_w, intent_h)) {
              hit->kd->view_dual.dragging = true;
              hit->kd->view_dual.drag_committed = false;
              hit->kd->view_dual.drag_start_mouse_x = mx;
              hit->kd->view_dual.drag_offset = mx - handle_x;
              cogito_pointer_capture = hit;
            }
          }
        }
        if (slot && slot->dialog_open && slot->dialog) {
          cogito_handle_click(slot->dialog, mx, my, true);
        } else if (win->dialog_open && win->dialog) {
          cogito_handle_click(win->dialog, mx, my, true);
        } else {
          cogito_handle_click(win, mx, my, true);
        }
      }
    }
    // Flush any coalesced relayout requests from this event turn once.
    CogitoNode* deferred_relayout = cogito_relayout_defer_take();
    if (deferred_relayout) {
      cogito_window_relayout_now(deferred_relayout);
    }
    cogito_relayout_defer_end();

    // --- Carousel animation tick ---
    // Only traverse the tree when at least one carousel is animating.
    if (cogito_carousel_animating_count > 0) {
      CogitoNode* _stk[
        1024
      ];
      int _sp = 0;
      _stk[_sp++
      ] = win;
      bool _carousel_did_relayout = false;
      double _now = cogito_now();
      while (_sp > 0) {
        CogitoNode* _cn = _stk[--_sp
        ];
        if (!_cn) continue;
        if (_cn->kind == COGITO_CAROUSEL && _cn->kd->carousel.animating) {
          double t = 1.0;
          if (CAROUSEL_ANIM_DURATION > 0.0) {
            t = (_now - _cn->kd->carousel.anim_start) / CAROUSEL_ANIM_DURATION;
          }
          if (t >= 1.0) {
            t = 1.0;
            _cn->kd->carousel.animating = false;
            cogito_carousel_animating_count--;
          }
          // ease-out quad: t * (2 - t)
          double et = t * (2.0 - t);
          double sx = _cn->kd->carousel.anim_from + (_cn->kd->carousel.anim_to - _cn->kd->carousel.anim_from) * et;
          _cn->kd->carousel.scroll_xf = sx;
          int isx = (int)(sx + 0.5);
          if (isx != _cn->kd->carousel.scroll_x) {
            _cn->kd->carousel.scroll_x = isx;
            _carousel_did_relayout = true;
          }
        }
        for (size_t _ci = 0; _ci < _cn->len && _sp < 1023; _ci++) {
          _stk[_sp++
          ] = _cn->children[_ci
          ];
        }
      }
      if (_carousel_did_relayout) {
        cogito_window_relayout(win);
      }
      // Safety: clamp to zero if counter drifted
      if (cogito_carousel_animating_count < 0) cogito_carousel_animating_count = 0;
    }

    // --- ViewDual edge-hide animation tick ---
    {
      CogitoNode* _stk[
        1024
      ];
      int _sp = 0;
      _stk[_sp++
      ] = win;
      bool _view_dual_did_relayout = false;
      bool _view_dual_anim_running = false;
      double _now = cogito_now();
      const double _dur = 0.20;
      while (_sp > 0) {
        CogitoNode* _cn = _stk[--_sp
        ];
        if (!_cn) continue;
        if (_cn->kind == COGITO_VIEW_DUAL && _cn->kd->view_dual.animating) {
          double t = 1.0;
          if (_dur > 0.0) t = (_now - _cn->kd->view_dual.anim_start) / _dur;
          if (t >= 1.0) {
            t = 1.0;
            _cn->kd->view_dual.animating = false;
          } else {
            _view_dual_anim_running = true;
          }
          double et = t * (2.0 - t);
          float prev = _cn->kd->view_dual.anim_ratio;
          _cn->kd->view_dual.anim_ratio = _cn->kd->view_dual.anim_from +
            (float)((double)(_cn->kd->view_dual.anim_to - _cn->kd->view_dual.anim_from) * et);
          if (!_cn->kd->view_dual.animating) {
            _cn->kd->view_dual.anim_ratio = _cn->kd->view_dual.anim_to;
          }
          if (fabsf(prev - _cn->kd->view_dual.anim_ratio) > 0.0005f) {
            _view_dual_did_relayout = true;
          }
        }
        for (size_t _ci = 0; _ci < _cn->len && _sp < 1023; _ci++) {
          _stk[_sp++
          ] = _cn->children[_ci
          ];
        }
      }
      if (_view_dual_did_relayout) {
        cogito_window_relayout(win);
      }
      if (_view_dual_anim_running) {
        double _end = _now + 0.22;
        if (_end > cogito_anim_end_time) cogito_anim_end_time = _end;
      }
    }
    // Only redraw when something requested it (queue_draw) or continuous updates needed.
    // Do not redraw during window drag — we only move the window; no present() needed, so we wait for events and avoid CPU spike.
    bool window_being_dragged = cogito_drag_active || cogito_drag_pending 
        || inspector_drag_active || inspector_drag_pending;
    double now = cogito_refresh_now();
    bool anim_running = now < cogito_anim_end_time;
    bool need_redraw = !window_being_dragged && (cogito_draw_pending(win) || anim_running
        || (cogito_carousel_animating_count > 0) || (cogito_marquee_animating_count > 0));

    // Debug redraw: set COGITO_DEBUG_REDRAW=1 to see why each frame redraws
    {
      static int _dbg_init = -1;
      if (_dbg_init < 0) { const char* _e = getenv("COGITO_DEBUG_REDRAW"); _dbg_init = (_e && _e[
          0
        ] && _e[
          0
        ] != '0') ? 1 : 0;
      }
      if (_dbg_init) {
        static int _dbg_frame = 0;
        static int _dbg_redraw_streak = 0;
        _dbg_frame++;
        if (need_redraw) {
          _dbg_redraw_streak++;
          if (_dbg_redraw_streak <= 3 || _dbg_redraw_streak % 60 == 0) {
            fprintf(stderr,
            "[redraw] frame=%d streak=%d reason:", _dbg_frame, _dbg_redraw_streak);
            if (cogito_draw_pending(win)) fprintf(stderr,
            " draw_pending");
            if (anim_running) fprintf(stderr,
            " anim(%.0fms)", (cogito_anim_end_time - now) * 1000.0);
            if (cogito_carousel_animating_count > 0) fprintf(stderr,
            " carousel(%d)", cogito_carousel_animating_count);
            fprintf(stderr,
            "\n");
          }
        } else {
          if (_dbg_redraw_streak > 3) {
            fprintf(stderr,
            "[redraw] streak ended after %d frames\n", _dbg_redraw_streak);
          }
          _dbg_redraw_streak = 0;
        }
      }
    }

    if (need_redraw) {
      double __frame_start = now;
      int damage_x = 0, damage_y = 0, damage_w = 0, damage_h = 0;
      bool use_damage_clip = false;
      if (cogito_draw_pending(win) && cogito_draw_pending_damage(win, &damage_x, &damage_y, &damage_w, &damage_h)) {
        if (damage_x < 0) {
          damage_w += damage_x;
          damage_x = 0;
        }
        if (damage_y < 0) {
          damage_h += damage_y;
          damage_y = 0;
        }
        if (damage_x + damage_w > win->w) damage_w = win->w - damage_x;
        if (damage_y + damage_h > win->h) damage_h = win->h - damage_y;
        use_damage_clip = damage_w > 0 && damage_h > 0;
      }

      // Rendering
    cogito_backend->begin_frame(window);
    if (use_damage_clip && cogito_backend->begin_scissor) {
      cogito_backend->begin_scissor(damage_x, damage_y, damage_w, damage_h);
    }
    cogito_draw_set_clip(use_damage_clip, damage_x, damage_y, damage_w, damage_h);
    cogito_spring_dt_update(); // Update per-frame dt for all spring integrations
    cogito_frame_seq++; // Increment frame counter for spring deduplication
    cogito_icon_cache_frame_start(); // Evict old icon textures
    cogito_image_cache_frame_start();

    CogitoColor win_bg = win->bg_set ? win->bg : cogito_rgba(245,
      245,
      245,
      255);
    cogito_backend->clear(win_bg);

    cogito_tooltip_hovered = NULL;
    cogito_draw_node(win);
    cogito_draw_focus_ring();
    if (cogito_menu.open) {
      cogito_menu_draw_active = true;
      cogito_draw_menu();
      cogito_menu_draw_active = false;
    }
    if (cogito_inspector_open) {
      cogito_draw_inspector_highlight();
      if (!inspector_window) {
        cogito_draw_inspector_panel(win);
        }
      }
    if (!cogito_menu.open) {
      CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
        ? win->dialog
        : cogito_find_dialog_slot(win);
      CogitoNode* root = (slot && slot->dialog) ? slot->dialog : win;
      if (root != win) {
          // Modal dialog open: don't show tooltips from background layers.
        cogito_tooltip_hovered = NULL;
        }
      if (cogito_tooltip_hovered) {
        cogito_draw_tooltip(win, cogito_tooltip_hovered);
        } else if (route_main_pointer) {
        CogitoNode* tip = cogito_find_tooltip_node(root, mx, my);
        if (tip) {
          cogito_draw_tooltip(win, tip);
          }
        }
      }
    
    if (use_damage_clip && cogito_backend->end_scissor) {
      cogito_backend->end_scissor();
    }
    cogito_draw_set_clip(false, 0, 0, 0, 0);
    cogito_backend->end_frame(window);
    if (cogito_screenshot_pending) {
      cogito_screenshot_pending = false;
      if (cogito_backend->window_screenshot)
        cogito_backend->window_screenshot(window);
    }
    cogito_backend->present(window);
    cogito_draw_clear_pending(win);
    // Animation deadline is time-based, no decrement needed

    if (cogito_inspector_open && inspector_window) {
      int iw = 0;
      int ih = 0;
      int appbar_h = inspector_appbar_h;
      cogito_backend->window_get_size(inspector_window, &iw, &ih);
      int frame_x = inspector_shadow_margin;
      int frame_y = inspector_shadow_margin;
      int frame_w = iw - inspector_shadow_margin * 2;
      int frame_h = ih - inspector_shadow_margin * 2;
      if (frame_w < 1) frame_w = 1;
      if (frame_h < 1) frame_h = 1;
      if (appbar_h > frame_h) appbar_h = frame_h;
      cogito_backend->begin_frame(inspector_window);
      cogito_backend->clear(cogito_rgba(0,
        0,
        0,
        0));
      CogitoColor frame_bg = cogito_rgba(18,
        21,
        26,
        245);
      CogitoColor appbar_bg = frame_bg;
      CogitoColor appbar_fg = cogito_rgba(220,
        226,
        235,
        255);
      CogitoColor appbar_div = cogito_rgba(255,
        255,
        255,
        34);
      CogitoStyle inspector_appbar_style = cogito_theme.per_kind[COGITO_APPBAR
        ];
#if !defined(__APPLE__)
      cogito_draw_shadow_radii_level(frame_x, frame_y, frame_w, frame_h, inspector_corner_r, inspector_corner_r, inspector_corner_r, inspector_corner_r,
        4);
#endif
      cogito_draw_rect(frame_x, frame_y, frame_w, frame_h, frame_bg, inspector_corner_r);
      cogito_draw_rect(frame_x, frame_y, frame_w, appbar_h + inspector_corner_r, appbar_bg, inspector_corner_r);
      if (appbar_h > 0) {
        cogito_draw_line(frame_x, frame_y + appbar_h - 1, frame_x + frame_w, frame_y + appbar_h - 1, appbar_div);
        }
      cogito_draw_rect_lines(frame_x, frame_y, frame_w, frame_h, appbar_div, inspector_corner_r,
        1);
      CogitoColor inspector_fallback_btn = inspector_appbar_style.has_selection
        ? inspector_appbar_style.selection
        : cogito_rgba(184,
        184,
        184,
        255);
      CogitoColor inspector_fallback_border = appbar_fg;
      if (inspector_fallback_border.a > 80) inspector_fallback_border.a = 80;
      cogito_draw_traffic_lights_styled(
        &inspector_appbar_style, inspector_fallback_btn, inspector_fallback_border,
        inspector_btn_close_x, inspector_btn_min_x, inspector_btn_max_x,
        inspector_btn_y, inspector_traffic_btn,
        &inspector_lights,
        true
      );
      int title_size = 20;
      int title_y = (appbar_h - cogito_text_height_size(title_size)) / 2;
      if (title_y < 0) title_y = 0;
      int title_x = inspector_btn_max_x + inspector_traffic_btn + 12;
      cogito_draw_text_size_node(win, title_x, frame_y + title_y,
        "Inspector", appbar_fg, title_size, true, false);
      cogito_inspector_view_x = frame_x;
      cogito_inspector_view_y = frame_y + appbar_h;
      cogito_inspector_view_w = frame_w;
      if (cogito_inspector_view_w < 0) cogito_inspector_view_w = 0;
      cogito_inspector_view_h = frame_h - appbar_h - inspector_corner_r;
      if (cogito_inspector_view_h < 0) cogito_inspector_view_h = 0;
      cogito_draw_inspector_panel(win);
      cogito_inspector_view_x = 0;
      cogito_inspector_view_y = 0;
      cogito_inspector_view_w = 0;
      cogito_inspector_view_h = 0;
      cogito_backend->end_frame(inspector_window);
      cogito_backend->present(inspector_window);
      }

      // Cap continuous redraw loops to ~60 FPS to avoid saturating CPU.
      if (cogito_backend && cogito_backend->get_time && cogito_backend->sleep) {
        double __frame_end = cogito_refresh_now();
        double __elapsed = __frame_end - __frame_start;
        double __target = 1.0 / 60.0;
        if (!anim_running && cogito_carousel_animating_count <= 0 && cogito_marquee_animating_count <= 0) {
          // Static redraws (without active animation) can run slower.
          __target = had_events ? (1.0 / 50.0) : (1.0 / 36.0);
        }
        if (left_down || cogito_pointer_capture) {
          __target = 1.0 / 60.0;
        }
        if (__elapsed < __target) {
          uint32_t __sleep_ms = (uint32_t)((__target - __elapsed) * 1000.0);
          if (__sleep_ms > 0) cogito_backend->sleep(__sleep_ms);
        }
      }
    } else {
      // Idle: wait for an event or timeout.
      // SDL_WaitEventTimeout on macOS/Metal may not truly block (compositor
      // events wake it immediately), so always follow with a short sleep.
      uint32_t wait_ms = cogito_timer_next_wait_ms(100);
#if defined(__APPLE__)
      // On macOS we avoid SDL_WaitEventTimeout in idle path because it can
      // intermittently starve timer-driven callbacks. Keep the loop ticking
      // with a short bounded sleep and let the next iteration poll events.
      uint32_t idle_sleep_ms = wait_ms > 8 ? 8 : wait_ms;
      if (idle_sleep_ms == 0) idle_sleep_ms = 1;
      if (cogito_backend && cogito_backend->sleep) {
        cogito_backend->sleep(idle_sleep_ms);
      } else {
        struct timespec ts = {
          0,
          (long)idle_sleep_ms * 1000000L
        };
        nanosleep(&ts, NULL);
      }
#else
      cogito_backend->wait_event_timeout(wait_ms);
      struct timespec ts = {
        0,
        wait_ms <= 8 ? 1000000 : 8000000
      }; // 1-8ms sleep to avoid busy spin while keeping timer latency reasonable
      nanosleep(&ts, NULL);
#endif
    }
    cogito_relayout_defer_end();
#if defined(__APPLE__)
    cogito_autorelease_pop();
#endif
  }
  // Cleanup
  cogito_timer_clear_all_internal();
  cogito_font_cache_clear();
  cogito_ft_library_done();
  cogito_icon_cache_clear();
  cogito_image_cache_clear();
  cogito_hit_test_cleanup();
  if (cogito_font_ready && cogito_font_from_env && cogito_font) {
    cogito_backend->font_unload(cogito_font);
  }
  if (cogito_font_serif_ready && cogito_font_serif_from_env && cogito_font_serif) {
    cogito_backend->font_unload(cogito_font_serif);
  }
  if (cogito_font_mono_ready && cogito_font_mono_from_env && cogito_font_mono) {
    cogito_backend->font_unload(cogito_font_mono);
  }
  if (cogito_font_bold_ready && cogito_font_bold_from_env && cogito_font_bold) {
    cogito_backend->font_unload(cogito_font_bold);
  }
  if (cogito_font_medium_ready && cogito_font_medium_from_env && cogito_font_medium) {
    cogito_backend->font_unload(cogito_font_medium);
  }

  if (inspector_window) {
    cogito_backend->window_destroy(inspector_window);
    inspector_window = NULL;
  }
  if (inspector_hit_test_params) {
    free(inspector_hit_test_params);
    inspector_hit_test_params = NULL;
  }
  cogito_inspector_view_x = 0;
  cogito_inspector_view_y = 0;
  cogito_inspector_view_w = 0;
  cogito_inspector_view_h = 0;
  cogito_backend->window_destroy(window);
  cogito_active_backend_window = NULL;
  cogito_backend_ready = false;
  cogito_menu_close();
  if (cogito_fab_menu_open_node) {
    cogito_fab_menu_open_node->popover_open = false;
    cogito_fab_menu_open_node = NULL;
    cogito_fab_menu_kbd_index = -1;
  }
  if (cogito_user_sum) {
    free(cogito_user_sum);
    cogito_user_sum = NULL;
  }
}
