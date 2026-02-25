static CogitoNode* cogito_grid_new_obj(int cols) {
  CogitoNode* g = cogito_node_new(COGITO_GRID);
  g->grid.cols = cols > 0 ? cols : 1;
  g->grid_align_x = 0;
  g->grid_align_y = 0;
  return g;
}
static YisVal cogito_grid_new(YisVal cols) {
  int c = (int)yis_as_int(cols);
  return EV_OBJ(cogito_grid_new_obj(c));
}
static void cogito_grid_set_gap(YisVal gridv, YisVal xv, YisVal yv) {
  if (gridv.tag != EVT_OBJ) yis_trap("cogito.grid_set_gap expects grid");
  CogitoNode* g = (CogitoNode*)gridv.as.p;
  g->grid.gap_x = (int)yis_as_int(xv);
  g->grid.gap_y = (int)yis_as_int(yv);
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
  if (grid->on_select) yis_release_val(EV_FN(grid->on_select));
  grid->on_select = (YisFn*)handler.as.p;
  yis_retain_val(EV_FN(grid->on_select));
}
static void cogito_grid_on_activate(YisVal gridv, YisVal handler) {
  if (gridv.tag != EVT_OBJ) yis_trap("cogito.grid_on_activate expects grid");
  CogitoNode* grid = (CogitoNode*)gridv.as.p;
  if (grid->on_activate) yis_release_val(EV_FN(grid->on_activate));
  grid->on_activate = (YisFn*)handler.as.p;
  yis_retain_val(EV_FN(grid->on_activate));
}
