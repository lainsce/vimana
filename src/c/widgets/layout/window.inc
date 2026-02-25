enum {
  COGITO_TOOLBAR_MAX_SLOTS = 6,
  COGITO_TOOLBAR_DOCKED_GAP = 32,
  COGITO_TOOLBAR_FLOATING_GAP = 8,
  COGITO_TOOLBAR_FLOATING_COMPACT_GAP = 4,
  COGITO_TOOLBAR_FLOATING_SIDE_PADDING = 8,
  COGITO_TOOLBAR_FLOATING_COMPACT_SIDE_PADDING = 8,
  COGITO_TOOLBAR_OUTER_PADDING = 16,
  COGITO_TOOLBAR_FLOATING_MARGIN = 16,
  COGITO_TOOLBAR_FLOATING_MIN_SLOT_WIDTH = 88,
  COGITO_TOOLBAR_VERTICAL_RIGHT_MARGIN = 24
};

static bool cogito_toolbar_has_class(const CogitoNode* toolbar, const char* token) {
  if (!toolbar || !toolbar->class_name || !toolbar->class_name->data || !token || !token[0]) return false;
  const char* cls = toolbar->class_name->data;
  return strstr(cls, token) != NULL;
}

static bool cogito_toolbar_is_floating(const CogitoNode* toolbar) {
  return cogito_toolbar_has_class(toolbar, "floating");
}

static bool cogito_toolbar_is_compact(const CogitoNode* toolbar) {
  return cogito_toolbar_has_class(toolbar, "compact");
}

static bool cogito_toolbar_is_vertical(const CogitoNode* toolbar) {
  if (!toolbar || toolbar->kind != COGITO_TOOLBAR) return false;
  return toolbar->toolbar.vertical;
}

static int cogito_toolbar_reserved_height(const CogitoNode* toolbar, int toolbar_h) {
  if (!toolbar || toolbar_h <= 0) return 0;
  int reserve = toolbar_h;
  if (cogito_toolbar_is_floating(toolbar)) {
    reserve += COGITO_TOOLBAR_FLOATING_MARGIN;
  }
  return reserve;
}

static int cogito_toolbar_floating_width(const CogitoNode* toolbar, int window_w) {
  int max_w = window_w - (COGITO_TOOLBAR_FLOATING_MARGIN * 2);
  if (max_w < 1) max_w = window_w;

  // Find FAB child if any
  const CogitoNode* fab_child = NULL;
  if (toolbar) {
    for (size_t i = 0; i < toolbar->len; i++) {
      const CogitoNode* c = toolbar->children[i];
      if (c && c->kind == COGITO_FAB) {
        fab_child = c;
        break;
      }
    }
  }

  bool compact = cogito_toolbar_is_compact(toolbar);
  if (compact || fab_child) {
    int count = 0;
    int content_w = 0;
    if (toolbar) {
      size_t max_count = toolbar->len;
      if (max_count > COGITO_TOOLBAR_MAX_SLOTS) max_count = COGITO_TOOLBAR_MAX_SLOTS;
      for (size_t i = 0; i < max_count; i++) {
        CogitoNode* c = toolbar->children[i];
        if (!c) continue;
        // Skip FAB in width calculation for floating toolbar
        if (fab_child && c->kind == COGITO_FAB) continue;
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        if (cw < 1) cw = 1;
        if (c->kind == COGITO_BUTTON && !c->hexpand) {
          int font_px = cogito_node_font_size(c);
          int text_w = 0;
          if (c->text && c->text->data && c->text->data[0]) {
            text_w = cogito_text_width_size_node(c, c->text->data, font_px);
          }
          int pad_x = c->padding_left + c->padding_right;
          if (pad_x <= 0) pad_x = 24;
          int compact_w = text_w + pad_x;
          int min_tap = ch > 0 ? ch : 40;
          if (min_tap < 32) min_tap = 32;
          if (compact_w < min_tap) compact_w = min_tap;
          cw = compact_w;
        }
        content_w += cw;
        count++;
      }
    }
    if (count > 1) {
      int gap = compact ? COGITO_TOOLBAR_FLOATING_COMPACT_GAP : COGITO_TOOLBAR_FLOATING_GAP;
      content_w += gap * (count - 1);
    }
    int side = COGITO_TOOLBAR_FLOATING_SIDE_PADDING;
    int preferred_w = (side * 2) + content_w;
    if (preferred_w > max_w) preferred_w = max_w;
    if (preferred_w < 1) preferred_w = 1;
    return preferred_w;
  }

  int slot_w = COGITO_TOOLBAR_FLOATING_MIN_SLOT_WIDTH;
  if (toolbar) {
    size_t count = toolbar->len;
    if (count > COGITO_TOOLBAR_MAX_SLOTS) count = COGITO_TOOLBAR_MAX_SLOTS;
    for (size_t i = 0; i < count; i++) {
      CogitoNode* c = toolbar->children[i];
      if (!c) continue;
      int cw = 0;
      int ch = 0;
      cogito_intrinsic_size(c, &cw, &ch);
      if (cw > slot_w) slot_w = cw;
    }
  }

  int preferred_w = (COGITO_TOOLBAR_FLOATING_MARGIN * 2) +
                    (slot_w * COGITO_TOOLBAR_MAX_SLOTS) +
                    (COGITO_TOOLBAR_FLOATING_GAP * (COGITO_TOOLBAR_MAX_SLOTS - 1));
  if (preferred_w > max_w) preferred_w = max_w;
  if (preferred_w < 1) preferred_w = 1;
  return preferred_w;
}

static void cogito_layout_window(CogitoNode* win) {
  win->x = 0;
  win->y = 0;
  int cx = win->padding_left;
  int cy = win->padding_top;
  int appbar_h = 0;
  int toolbar_h = 0;
  int toolbar_reserved_h = 0;
  int nav_rail_w = 0;
  int bottom_nav_h = 0;
  bool toolbar_floating = false;
  CogitoNode* appbar = NULL;
  CogitoNode* toolbar = NULL;
  CogitoNode* nav_rail = NULL;
  CogitoNode* bottom_nav = NULL;
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_APPBAR) {
      appbar = win->children[i];
      break;
    }
  }
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_TOOLBAR) {
      toolbar = win->children[i];
      break;
    }
  }
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_NAV_RAIL) {
      nav_rail = win->children[i];
      break;
    }
  }
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_BOTTOM_NAV) {
      bottom_nav = win->children[i];
      break;
    }
  }
  if (appbar) {
    cogito_intrinsic_size(appbar, NULL, &appbar_h);
    cogito_layout_node(appbar, 0, 0, win->w, appbar_h);
    cy += appbar_h;
  }
  if (toolbar) {
    bool toolbar_vertical = cogito_toolbar_is_vertical(toolbar);
    cogito_intrinsic_size(toolbar, NULL, &toolbar_h);
    toolbar_floating = cogito_toolbar_is_floating(toolbar);
    toolbar_reserved_h = cogito_toolbar_reserved_height(toolbar, toolbar_h);
    int tx = 0;
    int tw = win->w;
    int ty = win->h - toolbar_h;
    if (toolbar_vertical) {
      // Vertical toolbar: position 24px from right, full height minus appbar
      int toolbar_w = 0;
      cogito_intrinsic_size(toolbar, &toolbar_w, NULL);
      if (toolbar_w < 1) toolbar_w = 56;  // Default width for vertical toolbar
      tx = win->w - toolbar_w - COGITO_TOOLBAR_VERTICAL_RIGHT_MARGIN;
      ty = appbar ? appbar_h : 0;
      tw = toolbar_w;
      toolbar_h = win->h - ty;
      if (toolbar_h < 0) toolbar_h = 0;
    } else if (toolbar_floating) {
      tw = cogito_toolbar_floating_width(toolbar, win->w);
      tx = (win->w - tw) / 2;
      int min_x = COGITO_TOOLBAR_FLOATING_MARGIN;
      int max_x = win->w - COGITO_TOOLBAR_FLOATING_MARGIN - tw;
      if (max_x < min_x) {
        min_x = 0;
        max_x = 0;
      }
      if (tx < min_x) tx = min_x;
      if (tx > max_x) tx = max_x;
      ty = win->h - toolbar_h - COGITO_TOOLBAR_FLOATING_MARGIN;
    }
    if (ty < 0) ty = 0;
    cogito_layout_node(toolbar, tx, ty, tw, toolbar_h);
  }
  if (nav_rail) {
    cogito_intrinsic_size(nav_rail, &nav_rail_w, NULL);
    int rail_y = appbar ? appbar_h : 0;
    int rail_h = win->h - rail_y;
    if (toolbar) rail_h -= toolbar_reserved_h;
    if (bottom_nav) rail_h -= 80;  // Reserve space for bottom nav
    if (rail_h < 0) rail_h = 0;
    cogito_layout_node(nav_rail, 0, rail_y, nav_rail_w, rail_h);
    cx += nav_rail_w;
  }
  if (bottom_nav) {
    cogito_intrinsic_size(bottom_nav, NULL, &bottom_nav_h);
    int nav_y = win->h - bottom_nav_h;
    if (nav_y < 0) nav_y = 0;
    cogito_layout_node(bottom_nav, 0, nav_y, win->w, bottom_nav_h);
  }
  int appbar_gap = appbar ? 6 : 0;
  int toolbar_gap = toolbar ? (toolbar_floating ? 0 : 6) : 0;
  bool toolbar_vertical = toolbar ? cogito_toolbar_is_vertical(toolbar) : false;
  int toolbar_w = 0;
  if (toolbar && toolbar_vertical) {
    cogito_intrinsic_size(toolbar, &toolbar_w, NULL);
    if (toolbar_w < 1) toolbar_w = 56;
    toolbar_w += COGITO_TOOLBAR_VERTICAL_RIGHT_MARGIN;
  }

  // Calculate available space for expanding widgets
  int avail_w = win->w - win->padding_left - win->padding_right;
  int avail_h = win->h - win->padding_top - win->padding_bottom;
  if (appbar) avail_h -= appbar_h + appbar_gap;
  if (toolbar && !toolbar_vertical) avail_h -= toolbar_reserved_h + toolbar_gap;
  if (bottom_nav) avail_h -= bottom_nav_h;
  if (nav_rail) avail_w -= nav_rail_w;
  if (toolbar_vertical) avail_w -= toolbar_w;
  if (avail_w < 0) avail_w = 0;
  if (avail_h < 0) avail_h = 0;
  int content_x = win->padding_left + (nav_rail ? nav_rail_w : 0);
  int content_y = win->padding_top + appbar_h + appbar_gap;

  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR || c->kind == COGITO_NAV_RAIL || c->kind == COGITO_BOTTOM_NAV) continue;

    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int ch_total = ch + c->margin_top + c->margin_bottom;
    int slot_w = avail_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;

    // Check if widget should expand (only if explicit hexpand/vexpand)
    bool should_hexpand = c->hexpand;
    bool should_vexpand = c->vexpand;

    if (should_hexpand || should_vexpand) {
      int target_w = should_hexpand ? avail_w : cw;
      int target_h = should_vexpand ? avail_h : ch;
      cogito_layout_node(c, content_x, content_y, target_w, target_h);
      cy = content_y + target_h;
    } else {
      if (cw > slot_w) cw = slot_w;
      cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
      cy += ch_total;
    }
  }
  if (win->dialog) {
    if (win->dialog->kind == COGITO_DIALOG_SLOT) {
      if (win->dialog->dialog_open && win->dialog->dialog) {
        cogito_layout_node(win->dialog, 0, 0, win->w, win->h);
      }
    } else if (win->dialog_open) {
      int dw = 0;
      int dh = 0;
      cogito_intrinsic_size(win->dialog, &dw, &dh);
      if (dw <= 0) dw = 320;
      if (dh <= 0) dh = 200;
      int dx = (win->w - dw) / 2;
      int dy = (win->h - dh) / 2;
      cogito_layout_node(win->dialog, dx, dy, dw, dh);
    }
  }
  if (win->popover_open && win->popover) {
    int pw = 0;
    int ph = 0;
    cogito_intrinsic_size(win->popover, &pw, &ph);
    if (pw <= 0) pw = 280;
    if (ph <= 0) ph = 180;
    cogito_layout_node(win->popover, 0, 0, pw, ph);
  }
  if (win->side_sheet_open && win->side_sheet_node) {
    int ss_w = 0;
    int ss_h = 0;
    cogito_intrinsic_size(win->side_sheet_node, &ss_w, &ss_h);
    if (ss_w <= 0) ss_w = 320;
    if (ss_w > win->w) ss_w = win->w;
    ss_h = win->h;
    // For sidebar mode: position at right edge, pushing window width
    // For other modes: overlay on right
    if (win->side_sheet_node->side_sheet.mode == COGITO_SIDE_SHEET_SIDEBAR) {
        // Sidebar docked on right - positioned at right edge
        int ss_x = win->w - ss_w;
        cogito_layout_node(win->side_sheet_node, ss_x, 0, ss_w, ss_h);
    } else {
        // Modal/Inset overlay
        int ss_x = win->w - ss_w;
        cogito_layout_node(win->side_sheet_node, ss_x, 0, ss_w, ss_h);
    }
  }
}

static void cogito_window_desired_size(CogitoNode* win, int* out_w, int* out_h) {
int max_w = win->padding_left;
  int cy = win->padding_top;
  int appbar_h = 0;
  int toolbar_h = 0;
  int toolbar_reserved_h = 0;
  int toolbar_vertical_w = 0;
  int sidebar_w = 0;
  // Check if there's a sidebar side sheet and get its width
  if (win->side_sheet_open && win->side_sheet_node && 
      win->side_sheet_node->side_sheet.mode == COGITO_SIDE_SHEET_SIDEBAR) {
      cogito_intrinsic_size(win->side_sheet_node, &sidebar_w, NULL);
  }
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR) {
      cogito_intrinsic_size(c, NULL, &appbar_h);
      int row_w = win->padding_left + win->w;
      if (row_w > max_w) max_w = row_w;
      break;
    }
  }
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_TOOLBAR) {
      cogito_intrinsic_size(c, NULL, &toolbar_h);
      bool toolbar_vertical = cogito_toolbar_is_vertical(c);
      if (toolbar_vertical) {
        int tv_w = 0;
        cogito_intrinsic_size(c, &tv_w, NULL);
        if (tv_w < 1) tv_w = 56;
        toolbar_vertical_w = tv_w + COGITO_TOOLBAR_VERTICAL_RIGHT_MARGIN;
      } else {
        toolbar_reserved_h = cogito_toolbar_reserved_height(c, toolbar_h);
      }
      int row_w = win->padding_left + win->w;
      if (row_w > max_w) max_w = row_w;
      break;
    }
  }
  if (appbar_h > 0) cy += appbar_h;
  if (toolbar_reserved_h > 0) cy += toolbar_reserved_h;
  // Check if any child wants to expand
  bool has_vexpanding_child = false;
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR) continue;
    if (c->vexpand) {
      has_vexpanding_child = true;
      break;
    }
  }

  // Calculate intrinsic sizes for all children
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR) continue;
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cw_total = cw + c->margin_left + c->margin_right;
    int ch_total = ch + c->margin_top + c->margin_bottom;
    int row_w = win->padding_left + cw_total;
    if (row_w > max_w) max_w = row_w;
    cy += ch_total;
  }
  int dw = max_w + win->padding_right + sidebar_w + toolbar_vertical_w;
  // Calculate minimum height needed for intrinsic content
  int min_h = cy + win->padding_bottom;
  // If there's an expanding child, use max of minimum height and current window height
  // Otherwise, use the calculated minimum height for auto-sizing
  int dh = has_vexpanding_child ? (win->h > min_h ? win->h : min_h) : min_h;
  cogito_apply_size_constraints(win, &dw, &dh);
  if (out_w) *out_w = dw;
  if (out_h) *out_h = dh;
}

static void cogito_window_relayout_now(CogitoNode* win) {
  if (!win || win->kind != COGITO_WINDOW) return;
  cogito_layout_begin();
  cogito_layout_window(win);
  cogito_a11y_sync_tree(win);
  if (cogito_layout_changed()) cogito_queue_draw(win);
}

static void cogito_relayout_subtree(CogitoNode* n) {
  if (!n) return;
  if (n->kind == COGITO_WINDOW) {
    cogito_window_relayout(n);
    return;
  }
  CogitoNode* win = cogito_node_window(n);
  if (!win) return;
  cogito_layout_begin();
  cogito_layout_node(n, n->x, n->y, n->w, n->h);
  cogito_a11y_sync_tree(win);
  if (cogito_layout_changed()) {
    int pad = 4;
    cogito_queue_draw_rect(win, n->x - pad, n->y - pad, n->w + pad * 2, n->h + pad * 2);
  }
}

static void cogito_window_relayout(CogitoNode* win) {
  if (!win) return;
  if (win->kind != COGITO_WINDOW) {
    win = cogito_node_window(win);
    if (!win) return;
  }
  if (cogito_relayout_deferred) {
    cogito_relayout_defer_request(win);
    return;
  }
  cogito_window_relayout_now(win);
}
