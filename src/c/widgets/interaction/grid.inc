static CogitoNode* cogito_grid_new_obj(int cols) {
  CogitoNode* g = cogito_node_new(COGITO_GRID);
  g->grid.cols = cols > 0 ? cols : 1;
  g->grid_align_x = 0;
  g->grid_align_y = 0;
  return g;
}
static YisVal cogito_grid_new(YisVal cols) {
  int c = (int)yis_as_int(cols);
  return YV_OBJ(cogito_grid_new_obj(c));
}
static void cogito_grid_add(YisVal gridv, YisVal childv, YisVal colv, YisVal rowv, YisVal col_spanv, YisVal row_spanv) {
  if (gridv.tag != EVT_OBJ) yis_trap("cogito.grid_add expects grid");
  if (childv.tag != EVT_OBJ) yis_trap("cogito.grid_add expects child");
  CogitoNode* g = (CogitoNode*)gridv.as.p;
  if (!g || g->kind != COGITO_GRID) yis_trap("cogito.grid_add expects grid");
  CogitoNode* c = (CogitoNode*)childv.as.p;
  int col = (int)yis_as_int(colv);
  int row = (int)yis_as_int(rowv);
  int col_span = (int)yis_as_int(col_spanv);
  int row_span = (int)yis_as_int(row_spanv);
  if (col < 0 || row < 0) yis_trap("cogito.grid_add expects non-negative col/row");
  if (col_span < 1 || row_span < 1) yis_trap("cogito.grid_add expects span >= 1");

  c->grid_col = col;
  c->grid_row = row;
  c->grid_pos_set = true;
  c->grid_span_x = col_span;
  c->grid_span_y = row_span;

  cogito_children_add(g, c);
  cogito_apply_style_tree(c);
  if (cogito_backend_ready) {
    CogitoNode* win = cogito_node_window(g);
    if (win) cogito_window_relayout(win);
  }
}
static void cogito_grid_set_gap(YisVal gridv, YisVal xv, YisVal yv) {
  if (gridv.tag != EVT_OBJ) yis_trap("cogito.grid_set_gap expects grid");
  CogitoNode* g = (CogitoNode*)gridv.as.p;
  g->grid.gap_x = (int)yis_as_int(xv);
  g->grid.gap_y = (int)yis_as_int(yv);
  cogito_window_relayout(cogito_node_window(g));
}
static void cogito_grid_set_cols(YisVal gridv, YisVal colsv) {
  if (gridv.tag != EVT_OBJ) yis_trap("cogito.grid_set_cols expects grid");
  CogitoNode* g = (CogitoNode*)gridv.as.p;
  int cols = (int)yis_as_int(colsv);
  if (cols < 1) cols = 1;
  if (g->grid.cols == cols) return;
  g->grid.cols = cols;
  cogito_window_relayout(cogito_node_window(g));
}
static void cogito_grid_set_span(YisVal childv, YisVal colsv, YisVal rowsv) {
  if (childv.tag != EVT_OBJ) yis_trap("cogito.grid_set_span expects child");
  CogitoNode* c = (CogitoNode*)childv.as.p;
  c->grid_span_x = (int)yis_as_int(colsv);
  c->grid_span_y = (int)yis_as_int(rowsv);
  if (c->grid_span_x < 1) c->grid_span_x = 1;
  if (c->grid_span_y < 1) c->grid_span_y = 1;
  cogito_window_relayout(cogito_node_window(c));
}
static void cogito_grid_set_align(YisVal childv, YisVal halignv, YisVal valignv) {
  if (childv.tag != EVT_OBJ) yis_trap("cogito.grid_set_align expects child");
  CogitoNode* c = (CogitoNode*)childv.as.p;
  c->grid_align_x = (int)yis_as_int(halignv);
  c->grid_align_y = (int)yis_as_int(valignv);
  cogito_window_relayout(cogito_node_window(c));
}
static void cogito_grid_on_select(YisVal gridv, YisVal handler) {
  if (gridv.tag != EVT_OBJ) yis_trap("cogito.grid_on_select expects grid");
  CogitoNode* grid = (CogitoNode*)gridv.as.p;
  if (grid->on_select) yis_release_val(YV_FN(grid->on_select));
  grid->on_select = (YisFn*)handler.as.p;
  yis_retain_val(YV_FN(grid->on_select));
}
static void cogito_grid_on_activate(YisVal gridv, YisVal handler) {
  if (gridv.tag != EVT_OBJ) yis_trap("cogito.grid_on_activate expects grid");
  CogitoNode* grid = (CogitoNode*)gridv.as.p;
  if (grid->on_activate) yis_release_val(YV_FN(grid->on_activate));
  grid->on_activate = (YisFn*)handler.as.p;
  yis_retain_val(YV_FN(grid->on_activate));
}
static void cogito_grid_clear(YisVal gridv) {
  if (gridv.tag != EVT_OBJ) yis_trap("cogito.grid_clear expects grid");
  CogitoNode* grid = (CogitoNode*)gridv.as.p;
  cogito_clear_children(grid);
  grid->selected = -1;
  cogito_window_relayout(cogito_node_window(grid));
}
