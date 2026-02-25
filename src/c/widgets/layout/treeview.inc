typedef struct CogitoTreeRow {
  CogitoNode* node;
  int depth;
} CogitoTreeRow;

static void cogito_treeview_metrics(int* out_row_h, int* out_pad, int* out_indent_step,
                                    int* out_chevron_w, int* out_chevron_h, int* out_chevron_gap) {
  if (out_row_h) *out_row_h = 22;
  if (out_pad) *out_pad = 6;
  if (out_indent_step) *out_indent_step = 12;
  if (out_chevron_w) *out_chevron_w = 10;
  if (out_chevron_h) *out_chevron_h = 12;
  if (out_chevron_gap) *out_chevron_gap = 2;
}

static void cogito_treeview_inner_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  int inner_x = n->x + n->padding_left;
  int inner_y = n->y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  if (out_x) *out_x = inner_x;
  if (out_y) *out_y = inner_y;
  if (out_w) *out_w = inner_w;
  if (out_h) *out_h = inner_h;
}

static void cogito_treeview_node_label(CogitoNode* n, char* out, size_t cap) {
  if (!out || cap == 0) return;
  if (!n) {
    snprintf(out, cap, "<null>");
    return;
  }
  if (n->text && n->text->data && n->text->data[0]) {
    snprintf(out, cap, "%s", n->text->data);
    return;
  }
  const char* kind = cogito_kind_name(n->kind);
  const char* cls = (n->class_name && n->class_name->data && n->class_name->data[0]) ? n->class_name->data : NULL;
  if (cls) snprintf(out, cap, "%s.%s", kind, cls);
  else snprintf(out, cap, "%s", kind);
}

static size_t cogito_treeview_count_subtree_nodes(const CogitoNode* n) {
  if (!n) return 0;
  size_t total = 1;
  for (size_t i = 0; i < n->len; i++) {
    total += cogito_treeview_count_subtree_nodes(n->children[i]);
  }
  return total;
}

// Calculate the capacity needed for visible rows
static size_t cogito_treeview_visible_row_capacity(const CogitoNode* tree) {
  if (!tree) return 0;
  // Fast path: if no children collapsed, we can count more efficiently
  size_t total = 0;
  for (size_t i = 0; i < tree->len; i++) {
    total += cogito_treeview_count_subtree_nodes(tree->children[i]);
  }
  return total;
}

static size_t cogito_treeview_collect_rows_recursive(CogitoNode* n, int depth, CogitoTreeRow* rows, size_t cap) {
  if (!n || !rows || cap == 0) return 0;
  rows[0].node = n;
  rows[0].depth = depth;
  size_t used = 1;
  if (n->tree_collapsed) return used;
  for (size_t i = 0; i < n->len && used < cap; i++) {
    used += cogito_treeview_collect_rows_recursive(n->children[i], depth + 1, rows + used, cap - used);
  }
  return used;
}

static size_t cogito_treeview_collect_visible_rows(CogitoNode* tree, CogitoTreeRow* rows, size_t cap) {
  if (!tree || !rows || cap == 0) return 0;
  size_t used = 0;
  for (size_t i = 0; i < tree->len && used < cap; i++) {
    used += cogito_treeview_collect_rows_recursive(tree->children[i], 0, rows + used, cap - used);
  }
  return used;
}

static void cogito_treeview_hide_subtree(CogitoNode* n) {
  if (!n) return;
  n->x = 0;
  n->y = 0;
  n->w = 0;
  n->h = 0;
  for (size_t i = 0; i < n->len; i++) {
    cogito_treeview_hide_subtree(n->children[i]);
  }
}

static void cogito_layout_treeview(CogitoNode* n, int x, int y) {
  (void)x;
  (void)y;
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  cogito_treeview_inner_rect(n, &inner_x, &inner_y, &inner_w, &inner_h);
  for (size_t i = 0; i < n->len; i++) {
    cogito_treeview_hide_subtree(n->children[i]);
  }
  if (inner_w <= 0 || inner_h <= 0 || n->len == 0) return;

  size_t cap = cogito_treeview_visible_row_capacity(n);
  if (cap == 0) return;
  CogitoTreeRow* rows = (CogitoTreeRow*)calloc(cap, sizeof(CogitoTreeRow));
  if (!rows) return;
  size_t row_count = cogito_treeview_collect_visible_rows(n, rows, cap);
  int row_h = 0, pad = 0, indent_step = 0, chevron_w = 0, chevron_h = 0, chevron_gap = 0;
  cogito_treeview_metrics(&row_h, &pad, &indent_step, &chevron_w, &chevron_h, &chevron_gap);
  int y_cursor = inner_y;
  int max_y = inner_y + inner_h;
  for (size_t i = 0; i < row_count; i++) {
    if (y_cursor >= max_y) break;
    CogitoNode* c = rows[i].node;
    if (!c) continue;
    int depth = rows[i].depth;
    int indent_x = inner_x + pad + depth * indent_step;
    int label_x = indent_x + (c->len > 0 ? (chevron_w + chevron_gap) : 0);
    int row_w = inner_x + inner_w - pad - label_x;
    if (row_w < 0) row_w = 0;
    c->x = label_x;
    c->y = y_cursor;
    c->w = row_w;
    c->h = row_h;
    y_cursor += row_h;
  }
  free(rows);
}
