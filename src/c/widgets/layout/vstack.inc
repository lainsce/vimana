static void cogito_layout_vstack(CogitoNode* n, int x, int y) {
  int pad = n->gap;
  int avail_w = n->w - n->padding_left - n->padding_right;
  int avail_h = n->h - n->padding_top - n->padding_bottom;
  if (avail_w < 0) avail_w = 0;
  if (avail_h < 0) avail_h = 0;
  size_t count = n->len;
  if (count == 0) return;
  int intrinsic_w_stack[64];
  int intrinsic_h_stack[64];
  int assigned_h_stack[64];
  bool expand_stack[64];
  int* intrinsic_w = NULL;
  int* intrinsic_h = NULL;
  int* assigned_h = NULL;
  bool* expand = NULL;
  void* heap_block = NULL;
  if (count <= 64) {
    intrinsic_w = intrinsic_w_stack;
    intrinsic_h = intrinsic_h_stack;
    assigned_h = assigned_h_stack;
    expand = expand_stack;
  } else {
    size_t iw_bytes = sizeof(int) * count;
    size_t ih_bytes = sizeof(int) * count;
    size_t ah_bytes = sizeof(int) * count;
    size_t ex_bytes = sizeof(bool) * count;
    size_t total_bytes = iw_bytes + ih_bytes + ah_bytes + ex_bytes;
    heap_block = calloc(1, total_bytes);
    if (!heap_block) return;
    char* p = (char*)heap_block;
    intrinsic_w = (int*)p;
    p += iw_bytes;
    intrinsic_h = (int*)p;
    p += ih_bytes;
    assigned_h = (int*)p;
    p += ah_bytes;
    expand = (bool*)p;
  }
  int total_margins = 0;
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (cw < 0) cw = 0;
    if (ch < 0) ch = 0;
    intrinsic_w[i] = cw;
    intrinsic_h[i] = ch;
    assigned_h[i] = ch;
    expand[i] = c->vexpand;
    total_margins += c->margin_top + c->margin_bottom;
  }
  int total_gaps = (count > 0) ? (int)(count - 1) * pad : 0;
  int avail_content_h = avail_h - total_margins - total_gaps;
  if (avail_content_h < 0) avail_content_h = 0;
  cogito_distribute_axis_sizes(count, intrinsic_h, expand, avail_content_h, assigned_h);
  // Appbars reserve hard vertical space; shrink siblings first.
  int appbar_restore = 0;
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    if (!c || c->kind != COGITO_APPBAR) continue;
    if (assigned_h[i] < intrinsic_h[i]) {
      appbar_restore += (intrinsic_h[i] - assigned_h[i]);
      assigned_h[i] = intrinsic_h[i];
    }
  }
  while (appbar_restore > 0) {
    size_t shrinkable = 0;
    for (size_t i = 0; i < count; i++) {
      CogitoNode* c = n->children[i];
      if (!c || c->kind == COGITO_APPBAR) continue;
      if (assigned_h[i] > 0) shrinkable++;
    }
    if (shrinkable == 0) break;
    int per = appbar_restore / (int)shrinkable;
    if (per < 1) per = 1;
    int rem = (per > 1) ? appbar_restore % (int)shrinkable : 0;
    int removed = 0;
    int r = 0;
    for (size_t i = 0; i < count && removed < appbar_restore; i++) {
      CogitoNode* c = n->children[i];
      if (!c || c->kind == COGITO_APPBAR || assigned_h[i] <= 0) continue;
      int take = per + (r < rem ? 1 : 0);
      r++;
      if (take > assigned_h[i]) take = assigned_h[i];
      assigned_h[i] -= take;
      removed += take;
    }
    if (removed == 0) break;
    appbar_restore -= removed;
  }

  int cy = y + n->padding_top;
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int cw = intrinsic_w[i];
    int ch = assigned_h[i];
    int slot_w = avail_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;

    bool is_chrome = (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR ||
                      c->kind == COGITO_BOTTOM_NAV);
    if (c->hexpand || is_chrome) {
      cw = slot_w;
      if (c->kind == COGITO_LABEL && c->text_wrap && !c->vexpand) {
        ch = cogito_text_wrap_height(c, c->text ? c->text->data : "", cogito_node_font_size(c), slot_w);
        if (ch <= 0) ch = cogito_text_height_size(cogito_node_font_size(c));
      }
    } else if (cw > slot_w) {
      cw = slot_w;
    }

    int child_col = c->align % 3;
    if (child_col < 0 || child_col > 2) child_col = 0;
    int cx = x + n->padding_left;
    int ox = 0;
    if (child_col == 1) {
      ox = (slot_w - cw) / 2;
    } else if (child_col == 2) {
      ox = slot_w - cw;
    }
    if (ox < 0) ox = 0;
    cogito_layout_node(c, cx + ox + c->margin_left, cy + c->margin_top, cw, ch);
    cy += ch + c->margin_top + c->margin_bottom;
    if (i < count - 1) cy += pad;
  }
  if (heap_block) free(heap_block);
}
