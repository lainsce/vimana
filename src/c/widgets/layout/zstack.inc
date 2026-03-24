static void cogito_layout_zstack(CogitoNode* n, int x, int y) {
  int inner_x = x + n->padding_left;
  int inner_y = y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if (c->kind == COGITO_BOTTOM_SHEET) {
      int min_h = 0;
      cogito_intrinsic_size(c, NULL, &min_h);
      if (min_h < 100) min_h = 100;

      int host_x = x;
      int host_y = y;
      int host_w = n->w;
      int host_h = n->h;
      if (n->parent && n->parent->kind == COGITO_WINDOW) {
        CogitoNode* win = n->parent;
        host_x = 0;
        host_y = 0;
        host_w = win->w;
        host_h = win->h;
      }
      if (host_w < 0) host_w = 0;
      if (host_h < 0) host_h = 0;

      int top_inset = host_y <= 0 ? cogito_appbar_titlebar_inset() : 0;
      int max_h = host_h - top_inset;
      if (max_h < min_h) max_h = min_h;

      int target_w = host_w;
      if (target_w < 0) target_w = 0;
      int target_h = c->kd->bottom_sheet.desired_h > 0 ? c->kd->bottom_sheet.desired_h : min_h;
      if (target_h < min_h) target_h = min_h;
      if (target_h > max_h) target_h = max_h;
      c->kd->bottom_sheet.desired_h = target_h;
      c->kd->bottom_sheet.expanded = target_h >= (max_h - 8);

      int target_x = host_x;
      int target_y = host_y + host_h - target_h;

      cogito_layout_node(c, target_x, target_y, target_w, target_h);
      continue;
    }

    if (c->kind == COGITO_SIDE_SHEET) {
      int min_w = 0;
      cogito_intrinsic_size(c, &min_w, NULL);
      if (min_w < 200) min_w = 200;

      int host_x = x;
      int host_y = y;
      int host_w = n->w;
      int host_h = n->h;
      if (n->parent && n->parent->kind == COGITO_WINDOW) {
        CogitoNode* win = n->parent;
        host_x = 0;
        host_y = 0;
        host_w = win->w;
        host_h = win->h;
      }
      if (host_w < 0) host_w = 0;
      if (host_h < 0) host_h = 0;

      int left_inset = host_x <= 0 ? 0 : 0;
      int max_w = host_w - left_inset;
      if (max_w < min_w) max_w = min_w;

      int target_h = host_h;
      if (target_h < 0) target_h = 0;
      int target_w = c->kd->side_sheet.desired_w > 0 ? c->kd->side_sheet.desired_w : min_w;
      if (target_w < min_w) target_w = min_w;
      if (target_w > max_w) target_w = max_w;
      c->kd->side_sheet.desired_w = target_w;
      c->kd->side_sheet.expanded = target_w >= (max_w - 8);

      // Position side sheet based on mode
      int target_x;
      bool rtl = cogito_is_rtl();
      if (c->kd->side_sheet.mode == COGITO_SIDE_SHEET_SIDEBAR) {
          // Sidebar mode: position adjacent to window. RTL: left side, LTR: right side
          target_x = rtl ? (host_x - target_w) : (host_x + host_w);
      } else {
          // Modal/Inset mode: overlay. RTL: left side, LTR: right side
          target_x = rtl ? host_x : (host_x + host_w - target_w);
      }
      int target_y = host_y;

      cogito_layout_node(c, target_x, target_y, target_w, target_h);
      continue;
    }

    int cw = 0;
    int ch = 0;
    int ox = 0;
    int oy = 0;
    int avail_w = inner_w - c->margin_left - c->margin_right;
    int avail_h = inner_h - c->margin_top - c->margin_bottom;
    if (avail_w < 0) avail_w = 0;
    if (avail_h < 0) avail_h = 0;
    if (c->kind == COGITO_DIALOG_SLOT) {
      cw = avail_w;
      ch = avail_h;
    } else {
      cogito_intrinsic_size(c, &cw, &ch);
      if (c->hexpand) cw = avail_w;
      if (c->vexpand) ch = avail_h;
    }
    int align = c->align;
    if (align < 0 || align > 8) align = 0;
    int row_use = align / 3;
    int col_use = align % 3;
    if (cogito_is_rtl()) col_use = cogito_resolve_halign(col_use);
    if (c->kind == COGITO_TOASTS) {
      col_use = 2;
      row_use = 2;
    }
    if (c->kind != COGITO_DIALOG_SLOT && c->kind != COGITO_TOASTS) {
      bool fill_w = cogito_layout_effective_hexpand(c) || (col_use == 0 && !c->halign_set);
      bool fill_h = cogito_layout_effective_vexpand(c) || (row_use == 0 && !c->valign_set);
      if (fill_w) cw = avail_w;
      if (fill_h) ch = avail_h;
    }
    if (cw > avail_w) cw = avail_w;
    if (ch > avail_h) ch = avail_h;
    if (col_use == 1) ox = (avail_w - cw) / 2;
    else if (col_use == 2) ox = avail_w - cw;
    if (row_use == 1) oy = (avail_h - ch) / 2;
    else if (row_use == 2) oy = avail_h - ch;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    cogito_layout_node(c, inner_x + c->margin_left + ox, inner_y + c->margin_top + oy, cw, ch);
  }
}
