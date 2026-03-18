static bool cogito_is_fab_or_docked_toolbar(CogitoNode* n) {
  if (!n) return false;
  if (n->kind == COGITO_FAB) return true;
  if (n->kind == COGITO_FAB_MENU) return true;
  // Toolbar is docked if not floating and positioned at bottom
  if (n->kind == COGITO_TOOLBAR) {
    bool floating = (n->class_name && n->class_name->data && strstr(n->class_name->data, "floating"));
    return !floating && !n->kd->toolbar.vertical;
  }
  return false;
}

static CogitoNode* cogito_find_fab_toolbar(CogitoNode* n) {
  if (!n) return NULL;
  if (cogito_is_fab_or_docked_toolbar(n)) return n;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* found = cogito_find_fab_toolbar(n->children[i]);
    if (found) return found;
  }
  return NULL;
}

static void cogito_layout_toasts(CogitoNode* n, int x, int y) {
  CogitoNode* win = cogito_node_window(n);
  int base_x = win ? 0 : x;
  int base_y = win ? 0 : y;
  int base_w = win ? win->w : n->w;
  int base_h = win ? win->h : n->h;
  if (base_w < 0) base_w = 0;
  if (base_h < 0) base_h = 0;
  if (win) {
    n->x = base_x;
    n->y = base_y;
    n->w = base_w;
    n->h = base_h;
  }
  int inner_x = base_x + n->padding_left;
  int inner_y = base_y + n->padding_top;
  int inner_w = base_w - n->padding_left - n->padding_right;
  int inner_h = base_h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  int gap = n->gap > 0 ? n->gap : 6;

  // Check for FAB, FABMenu, or docked Toolbar - position toasts above them
  int bottom_margin = 0;
  if (win) {
    CogitoNode* fab_toolbar = cogito_find_fab_toolbar(win);
    if (fab_toolbar) {
      bottom_margin = fab_toolbar->h + 12; // 12dp clearance
    }
  }

  int cy = inner_y + inner_h - bottom_margin;
  // Limit to 5 visible toasts (newest first, since we iterate backwards)
  const size_t max_toasts = 5;
  size_t start_idx = n->len > max_toasts ? n->len - max_toasts : 0;
  for (size_t idx = n->len; idx-- > start_idx;) {
    CogitoNode* c = n->children[idx];
    // Hide excess toasts beyond the limit
    if (idx < start_idx) {
      c->hidden = true;
      continue;
    }
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_w = inner_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;
    if (cw > slot_w) cw = slot_w;
    cy -= c->margin_bottom + ch;
    int cy_child = cy;
    int min_y = inner_y + c->margin_top;
    if (cy_child < min_y) cy_child = min_y;
    int cx = inner_x + c->margin_left + (slot_w - cw) / 2;
    if (cx < inner_x + c->margin_left) cx = inner_x + c->margin_left;
    cogito_layout_node(c, cx, cy_child, cw, ch);
    cy = cy_child - c->margin_top;
    // Add gap only between items, not before the first one (we're iterating backwards)
    if (idx > start_idx) cy -= gap;
  }
  // Hide any excess toasts that weren't iterated
  for (size_t i = 0; i < start_idx && i < n->len; i++) {
    n->children[i]->hidden = true;
  }
}
