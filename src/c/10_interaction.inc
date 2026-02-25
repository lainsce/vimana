static void cogito_invoke_change(CogitoNode* n);
static CogitoNode* cogito_root(CogitoNode* n);
static bool cogito_hit_node(CogitoNode* n, int x, int y);
static bool cogito_hit_rect_min48(int x, int y, int rx, int ry, int rw, int rh);
static bool cogito_hit_node_min48(const CogitoNode* n, int x, int y);
static void cogito_clear_children(CogitoNode* node);
static void cogito_timer_cancel_owner_tree(CogitoNode* root);
static void cogito_node_set_class(YisVal nodev, YisVal classv);

static void cogito_invoke(YisFn* fn, int argc, YisVal* argv) {
  if (!fn) return;
  static int cogito_debug_invoke = -1;
  if (cogito_debug_invoke < 0) {
    const char* env = getenv("COGITO_DEBUG_HIT");
    cogito_debug_invoke = (env && env[0] && env[0] != '0') ? 1 : 0;
  }
  if (cogito_debug_invoke) {
    fprintf(stderr, "cogito: invoke fn=%p impl=%p env=%p argc=%d\n",
            (void*)fn, (void*)fn->fn, fn->env, argc);
    fflush(stderr);
  }
  YisVal ret = fn->fn(fn->env, argc, argv);
  if (cogito_debug_invoke) {
    fprintf(stderr, "cogito: invoke ret tag=%d\n", (int)ret.tag);
    fflush(stderr);
  }
  if (ret.tag != EVT_NULL) {
    yis_release_val(ret);
  }
  if (cogito_debug_invoke) {
    fprintf(stderr, "cogito: invoke done fn=%p\n", (void*)fn);
    fflush(stderr);
  }
}

static void cogito_invoke_click(CogitoNode* n) {
  if (!n || !n->on_click) return;
  static int cogito_debug_click = -1;
  if (cogito_debug_click < 0) {
    const char* env = getenv("COGITO_DEBUG_HIT");
    cogito_debug_click = (env && env[0] && env[0] != '0') ? 1 : 0;
  }
  if (cogito_debug_click) {
    const char* t = (n->text && n->text->data) ? n->text->data : "";
    CogitoNode* win = cogito_node_window(n);
    fprintf(stderr, "cogito: invoke_click kind=%d text=\"%s\" handler=%p\n",
            (int)n->kind, t, (void*)n->on_click);
    if (win) {
      fprintf(stderr, "cogito: invoke_click window=%p dialog_open=%d dialog=%p dialog_kind=%d\n",
              (void*)win, (int)win->dialog_open, (void*)win->dialog,
              win->dialog ? (int)win->dialog->kind : -1);
    } else {
      fprintf(stderr, "cogito: invoke_click window=NULL\n");
    }
    fflush(stderr);
  }
  YisVal arg = EV_OBJ(n);
  cogito_invoke(n->on_click, 1, &arg);
  cogito_a11y_emit_event("activate", n, "click");
}

static void cogito_invoke_menu(CogitoNode* n, size_t idx) {
  if (!n || idx >= n->menu_len) return;
  if (n->kind == COGITO_DROPDOWN) {
    n->selected = (int)idx;
    cogito_invoke_change(n);
    return;
  }
  YisFn* fn = n->menu_handlers[idx];
  if (!fn) return;
  YisVal arg = EV_OBJ(n);
  cogito_invoke(fn, 1, &arg);
}

static void cogito_invoke_change(CogitoNode* n) {
  if (!n || !n->on_change) return;
  YisVal arg = EV_OBJ(n);
  cogito_invoke(n->on_change, 1, &arg);
  cogito_a11y_emit_event("value-changed", n, "");
}

static void cogito_invoke_select(CogitoNode* n, int idx, bool activate) {
  YisVal arg = EV_INT(idx);
  if (n->on_select) cogito_invoke(n->on_select, 1, &arg);
  if (activate && n->on_activate) cogito_invoke(n->on_activate, 1, &arg);
}

static void cogito_dialog_slot_show(YisVal slotv, YisVal dialogv);
static void cogito_dialog_slot_clear(YisVal slotv);
static void cogito_dialog_close(YisVal dialogv);
static void cogito_dialog_remove(YisVal dialogv);
static YisVal cogito_node_parent(YisVal nodev);
static void cogito_container_remove_child(CogitoNode* parent, CogitoNode* child);

#include "widgets/interaction/button.inc"
#include "widgets/interaction/carousel.inc"
#include "widgets/interaction/carousel_item.inc"
#include "widgets/interaction/checkbox.inc"
#include "widgets/interaction/chip.inc"
#include "widgets/interaction/colorpicker.inc"
#include "widgets/interaction/datepicker.inc"
#include "widgets/interaction/dialog.inc"
#include "widgets/interaction/popover.inc"
#include "widgets/interaction/dropdown.inc"
#include "widgets/interaction/divider.inc"
#include "widgets/interaction/card.inc"
#include "widgets/interaction/avatar.inc"
#include "widgets/interaction/badge.inc"
#include "widgets/interaction/banner.inc"
#include "widgets/interaction/bottom_sheet.inc"
#include "widgets/interaction/side_sheet.inc"
#include "widgets/interaction/timepicker.inc"
#include "widgets/interaction/active_indicator.inc"
#include "widgets/interaction/switchbar.inc"
#include "widgets/interaction/content_list.inc"
#include "widgets/interaction/empty_page.inc"
#include "widgets/interaction/tip_view.inc"
#include "widgets/interaction/settings_window.inc"
#include "widgets/interaction/settings_page.inc"
#include "widgets/interaction/settings_list.inc"
#include "widgets/interaction/settings_row.inc"
#include "widgets/interaction/welcome_screen.inc"
#include "widgets/interaction/view_dual.inc"
#include "widgets/interaction/view_chooser.inc"
#include "widgets/interaction/about_window.inc"
#include "widgets/interaction/split_button.inc"
#include "widgets/interaction/drawing_area.inc"
#include "widgets/interaction/shape.inc"
#include "widgets/interaction/menu_button.inc"
#include "widgets/interaction/menu_section.inc"
#include "widgets/interaction/fab.inc"
#include "widgets/interaction/fab_menu.inc"
#include "widgets/interaction/fixed.inc"
#include "widgets/interaction/grid.inc"
#include "widgets/interaction/hstack.inc"
#include "widgets/interaction/iconbtn.inc"
#include "widgets/interaction/image.inc"
#include "widgets/interaction/label.inc"
#include "widgets/interaction/list.inc"
#include "widgets/interaction/nav_rail.inc"
#include "widgets/interaction/bottom_nav.inc"
#include "widgets/interaction/progress.inc"
#include "widgets/interaction/scroller.inc"
#include "widgets/interaction/searchfield.inc"
#include "widgets/interaction/buttongroup.inc"
#include "widgets/interaction/slider.inc"
#include "widgets/interaction/stepper.inc"
#include "widgets/interaction/switch.inc"
#include "widgets/interaction/tabs.inc"
#include "widgets/interaction/textfield.inc"
#include "widgets/interaction/textview.inc"
#include "widgets/interaction/toast.inc"
#include "widgets/interaction/toasts.inc"
#include "widgets/interaction/toolbar.inc"
#include "widgets/interaction/treeview.inc"
#include "widgets/interaction/tooltip.inc"
#include "widgets/interaction/view_switcher.inc"
#include "widgets/interaction/vstack.inc"
#include "widgets/interaction/window.inc"
#include "widgets/interaction/zstack.inc"
#include "widgets/interaction/appbar.inc"

static void cogito_state_drop(YisObj* o) {
  CogitoState* s = (CogitoState*)o;
  if (s->value.tag != EVT_NULL) {
    yis_release_val(s->value);
    s->value = EV_NULLV;
  }
}

__attribute__((unused))
static CogitoState* cogito_state_new_obj(YisVal initial) {
  CogitoState* s = (CogitoState*)yis_obj_new(sizeof(CogitoState), cogito_state_drop);
  s->value = initial;
  if (initial.tag != EVT_NULL) yis_retain_val(initial);
  return s;
}

static CogitoNode* cogito_root(CogitoNode* n) {
  CogitoNode* cur = n;
  while (cur && cur->parent) cur = cur->parent;
  return cur;
}

static CogitoNode* cogito_node_window(CogitoNode* n) {
  CogitoNode* root = cogito_root(n);
  if (root && root->kind == COGITO_WINDOW) return root;
  return NULL;
}

static CogitoNode* cogito_node_get_parent_internal(CogitoNode* n) {
  return n ? n->parent : NULL;
}

static size_t cogito_node_get_child_count_internal(CogitoNode* n) {
  return n ? n->len : 0;
}

static CogitoNode* cogito_node_get_child_internal(CogitoNode* n, size_t index) {
  if (!n || index >= n->len) return NULL;
  return n->children[index];
}

static bool cogito_hit_rect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && y >= ry && x <= rx + rw && y <= ry + rh;
}

static bool cogito_hit_rect_min48(int x, int y, int rx, int ry, int rw, int rh) {
  int hit_w = rw < 48 ? 48 : rw;
  int hit_h = rh < 48 ? 48 : rh;
  int hit_x = rx - (hit_w - rw) / 2;
  int hit_y = ry - (hit_h - rh) / 2;
  return cogito_hit_rect(x, y, hit_x, hit_y, hit_w, hit_h);
}

static bool cogito_hit_node(CogitoNode* n, int x, int y) {
  return cogito_hit_rect(x, y, n->x, n->y, n->w, n->h);
}

// Expanded hit test: inflates the hit rect to a minimum of 48×48dp (centered),
// so small interactive widgets meet the accessibility touch-target requirement (§11.1).
static bool cogito_hit_node_min48(const CogitoNode* n, int x, int y) {
  return cogito_hit_rect_min48(x, y, n->x, n->y, n->w, n->h);
}

static CogitoNode* cogito_find_clickable(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_WINDOW && n->popover_open && n->popover) {
    CogitoNode* pop_hit = cogito_find_clickable(n->popover, x, y);
    if (pop_hit) return pop_hit;
  }
  if (n->kind == COGITO_ZSTACK) {
    CogitoNode* sheet = NULL;
    for (size_t i = n->len; i > 0; i--) {
      CogitoNode* c = n->children[i - 1];
      if (c && (c->kind == COGITO_BOTTOM_SHEET || c->kind == COGITO_SIDE_SHEET)) {
        sheet = c;
        break;
      }
    }
    int hit_x = n->x;
    int hit_y = n->y;
    int hit_w = n->w;
    int hit_h = n->h;
    CogitoNode* win = cogito_node_window(n);
    if (sheet && win && n->parent == win) {
      hit_x = 0;
      hit_y = 0;
      hit_w = win->w;
      hit_h = win->h;
    }
    if (sheet && cogito_hit_rect(x, y, hit_x, hit_y, hit_w, hit_h)) {
      // Modal behavior: sheet captures taps across the host area.
      CogitoNode* child_hit = cogito_find_clickable(sheet, x, y);
      if (child_hit) return child_hit;
      return sheet;
    }
  }
  if (n->kind == COGITO_DIALOG_SLOT) {
    if (n->dialog && n->dialog_open) {
      return cogito_find_clickable(n->dialog, x, y);
    }
    return NULL;
  }
  if (n->kind == COGITO_DIALOG && !cogito_hit_node(n, x, y)) return NULL;
  if (n->kind == COGITO_SCROLLER && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  if (n->kind == COGITO_TREEVIEW) {
    if (cogito_hit_node(n, x, y)) return n;
    return NULL;
  }
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* child = cogito_find_clickable(active, x, y);
      if (child) return child;
    }
  } else {
    for (size_t i = n->len; i > 0; i--) {
      CogitoNode* child = cogito_find_clickable(n->children[i - 1], x, y);
      if (child) return child;
    }
  }
  if (n->kind == COGITO_BUTTON || n->kind == COGITO_ICONBTN ||
      n->kind == COGITO_LIST || n->kind == COGITO_GRID ||
      n->kind == COGITO_TREEVIEW ||
      n->kind == COGITO_TEXTFIELD || n->kind == COGITO_TEXTVIEW ||
      n->kind == COGITO_DROPDOWN || n->kind == COGITO_SLIDER ||
      n->kind == COGITO_CAROUSEL || n->kind == COGITO_CAROUSEL_ITEM ||
      n->kind == COGITO_TABS || n->kind == COGITO_TOAST ||
      n->kind == COGITO_DATEPICKER || n->kind == COGITO_COLORPICKER ||
      n->kind == COGITO_FAB || n->kind == COGITO_FAB_MENU ||
      n->kind == COGITO_BUTTON_GROUP ||
      n->kind == COGITO_SEARCHFIELD ||
      n->kind == COGITO_NAV_RAIL || n->kind == COGITO_BOTTOM_NAV ||
      n->kind == COGITO_TIMEPICKER || n->kind == COGITO_BANNER ||
      n->kind == COGITO_BOTTOM_SHEET || n->kind == COGITO_SIDE_SHEET ||
      n->kind == COGITO_SWITCHBAR || n->kind == COGITO_SPLIT_BUTTON ||
      n->kind == COGITO_DRAWING_AREA ||
      n->kind == COGITO_VIEW_CHOOSER || n->kind == COGITO_VIEW_DUAL ||
      n->kind == COGITO_SETTINGS_WINDOW ||
      n->kind == COGITO_EMPTY_PAGE || n->kind == COGITO_WELCOME_SCREEN ||
      n->kind == COGITO_ABOUT_WINDOW) {
    if (cogito_hit_node_min48(n, x, y)) return n;
  }
  // Small interactive widgets: expand hit target to 48×48dp minimum (§11.1 Accessibility).
  // The visual size is unchanged; only the clickable/hover detection area is enlarged.
  if (n->kind == COGITO_CHIP || n->kind == COGITO_STEPPER ||
      n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH) {
    if (cogito_hit_node_min48(n, x, y)) return n;
  }
  // Card is only clickable when an on_click handler is registered
  if (n->kind == COGITO_CARD && n->on_click) {
    if (cogito_hit_node(n, x, y)) return n;
  }
  return NULL;
}

static CogitoNode* cogito_find_overlay_toast(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if ((n->kind == COGITO_WINDOW || n->kind == COGITO_DIALOG_SLOT) &&
      n->dialog && n->dialog_open) {
    CogitoNode* d = cogito_find_overlay_toast(n->dialog, x, y);
    if (d) return d;
  }
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) return cogito_find_overlay_toast(active, x, y);
  } else {
    for (size_t i = n->len; i > 0; i--) {
      CogitoNode* child = cogito_find_overlay_toast(n->children[i - 1], x, y);
      if (child) return child;
    }
  }
  if (n->kind == COGITO_TOAST && cogito_hit_node(n, x, y)) return n;
  return NULL;
}

static bool cogito_node_focusable(const CogitoNode* n) {
  if (!n || n->disabled) return false;
  switch (n->kind) {
    #include "widgets/interaction/focusable/textfield.inc"
    #include "widgets/interaction/focusable/textview.inc"
    #include "widgets/interaction/focusable/dropdown.inc"
    #include "widgets/interaction/focusable/slider.inc"
    #include "widgets/interaction/focusable/button.inc"
    #include "widgets/interaction/focusable/carousel.inc"
    #include "widgets/interaction/focusable/carousel_item.inc"
    #include "widgets/interaction/focusable/iconbtn.inc"
    #include "widgets/interaction/focusable/checkbox.inc"
    #include "widgets/interaction/focusable/chip.inc"
    #include "widgets/interaction/focusable/switch.inc"
    #include "widgets/interaction/focusable/colorpicker.inc"
    #include "widgets/interaction/focusable/datepicker.inc"
    #include "widgets/interaction/focusable/fab.inc"
    #include "widgets/interaction/focusable/fab_menu.inc"
    #include "widgets/interaction/focusable/nav_rail.inc"
    #include "widgets/interaction/focusable/bottom_nav.inc"
    #include "widgets/interaction/focusable/stepper.inc"
    #include "widgets/interaction/focusable/buttongroup.inc"
    #include "widgets/interaction/focusable/searchfield.inc"
    case COGITO_SPLIT_BUTTON:
    case COGITO_TABS:
    case COGITO_TIMEPICKER:
    case COGITO_TREEVIEW:
      return true;
    default:
      return false;
  }
}

static CogitoNode* cogito_focus_next(CogitoNode* n, CogitoNode* current, bool* found_current) {
  if (!n) return NULL;
  if (n == current) *found_current = true;
  else if (*found_current && cogito_node_focusable(n)) return n;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* hit = cogito_focus_next(active, current, found_current);
      if (hit) return hit;
    }
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* hit = cogito_focus_next(n->children[i], current, found_current);
      if (hit) return hit;
    }
  }
  return NULL;
}

static CogitoNode* cogito_focus_prev(CogitoNode* n, CogitoNode* current, bool* found_current) {
  if (!n) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* hit = cogito_focus_prev(active, current, found_current);
      if (hit) return hit;
    }
  } else {
    for (size_t i = n->len; i > 0; i--) {
      CogitoNode* hit = cogito_focus_prev(n->children[i - 1], current, found_current);
      if (hit) return hit;
    }
  }
  if (n == current) {
    *found_current = true;
    return NULL;
  }
  if (*found_current && cogito_node_focusable(n)) return n;
  return NULL;
}

static CogitoNode* cogito_focus_first(CogitoNode* n) {
  if (!n) return NULL;
  if (cogito_node_focusable(n)) return n;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) return cogito_focus_first(active);
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* hit = cogito_focus_first(n->children[i]);
      if (hit) return hit;
    }
  }
  return NULL;
}

static CogitoNode* cogito_focus_last(CogitoNode* n) {
  if (!n) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) return cogito_focus_last(active);
  } else {
    for (size_t i = n->len; i > 0; i--) {
      CogitoNode* hit = cogito_focus_last(n->children[i - 1]);
      if (hit) return hit;
    }
  }
  if (cogito_node_focusable(n)) return n;
  return NULL;
}

static void cogito_container_remove_child(CogitoNode* parent, CogitoNode* child) {
  if (!parent || !child || !parent->children) return;
  size_t idx = parent->len;
  for (size_t i = 0; i < parent->len; i++) {
    if (parent->children[i] == child) { idx = i; break; }
  }
  if (idx >= parent->len) return;
  cogito_timer_cancel_owner_tree(parent->children[idx]);
  yis_release_val(EV_OBJ(parent->children[idx]));
  for (size_t i = idx + 1; i < parent->len; i++) {
    parent->children[i - 1] = parent->children[i];
  }
  parent->len--;
}

static void cogito_clear_children(CogitoNode* node) {
  if (!node || !node->children) return;
  for (size_t i = 0; i < node->len; i++) {
    cogito_timer_cancel_owner_tree(node->children[i]);
    yis_release_val(EV_OBJ(node->children[i]));
  }
  free(node->children);
  node->children = NULL;
  node->len = 0;
  node->cap = 0;
}

#include "widgets/util/app.inc"
#include "widgets/util/timer.inc"

#include "widgets/util/node_api.inc"
