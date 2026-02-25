static CogitoNode* cogito_treeview_new_obj(void) {
  return cogito_node_new(COGITO_TREEVIEW);
}
static ErgoVal cogito_treeview_new(void) {
  return EV_OBJ(cogito_treeview_new_obj());
}

static bool cogito_treeview_click(CogitoNode* tree, int x, int y) {
  if (!tree || tree->kind != COGITO_TREEVIEW || !cogito_hit_node(tree, x, y)) return false;
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  cogito_treeview_inner_rect(tree, &inner_x, &inner_y, &inner_w, &inner_h);
  if (!cogito_hit_rect(x, y, inner_x, inner_y, inner_w, inner_h)) return true;

  size_t cap = cogito_treeview_visible_row_capacity(tree);
  if (cap == 0) return true;
  CogitoTreeRow* rows = (CogitoTreeRow*)calloc(cap, sizeof(CogitoTreeRow));
  if (!rows) return true;
  size_t row_count = cogito_treeview_collect_visible_rows(tree, rows, cap);
  int row_h = 0, pad = 0, indent_step = 0, chevron_w = 0, chevron_h = 0, chevron_gap = 0;
  cogito_treeview_metrics(&row_h, &pad, &indent_step, &chevron_w, &chevron_h, &chevron_gap);
  int row = row_h > 0 ? (y - inner_y) / row_h : -1;
  if (row >= 0 && (size_t)row < row_count) {
    CogitoNode* row_node = rows[row].node;
    int depth = rows[row].depth;
    int row_y = inner_y + row * row_h;
    int chevron_x = inner_x + pad + depth * indent_step;
    int chevron_y = row_y + (row_h - chevron_h) / 2;
    if (row_node && row_node->len > 0 &&
        cogito_hit_rect_min48(x, y, chevron_x, chevron_y, chevron_w, chevron_h)) {
      row_node->tree_collapsed = !row_node->tree_collapsed;
    } else {
      tree->selected = row;
      if (row_node && row_node->on_click) {
        cogito_invoke_click(row_node);
      }
      cogito_invoke_select(tree, row, true);
    }
  }
  free(rows);
  return true;
}
