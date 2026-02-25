static CogitoNode* cogito_progress_new_obj(double value) {
  CogitoNode* p = cogito_node_new(COGITO_PROGRESS);
  if (value < 0.0) value = 0.0;
  if (value > 1.0) value = 1.0;
  p->progress.value = value;
  p->progress.indeterminate = false;
  p->progress.circular = false;
  p->progress.shape = COGITO_PROGRESS_SHAPE_FLAT;
  return p;
}
static ErgoVal cogito_progress_new(ErgoVal valv) {
  double v = ergo_as_float(valv);
  return EV_OBJ(cogito_progress_new_obj(v));
}
static void cogito_progress_set_value(ErgoVal pv, ErgoVal valv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_set_value expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  double v = ergo_as_float(valv);
  if (v < 0.0) v = 0.0;
  if (v > 1.0) v = 1.0;
  p->progress.value = v;
  cogito_window_relayout(cogito_node_window(p));
}
static ErgoVal cogito_progress_get_value(ErgoVal pv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_get_value expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  return EV_FLOAT(p->progress.value);
}
static void cogito_progress_set_indeterminate(ErgoVal pv, ErgoVal onv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_set_indeterminate expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  p->progress.indeterminate = ergo_as_bool(onv);
  cogito_window_relayout(cogito_node_window(p));
}
static ErgoVal cogito_progress_get_indeterminate(ErgoVal pv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_get_indeterminate expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  return EV_BOOL(p->progress.indeterminate);
}
static void cogito_progress_set_thickness(ErgoVal pv, ErgoVal pxv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_set_thickness expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  int thickness = (int)lround(ergo_as_float(pxv));
  if (thickness < 1) thickness = 1;
  p->track_height = thickness;
  p->track_height_set = true;
  cogito_window_relayout(cogito_node_window(p));
}
static ErgoVal cogito_progress_get_thickness(ErgoVal pv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_get_thickness expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  return EV_INT(p->track_height_set ? p->track_height : 4);
}
static void cogito_progress_set_wavy(ErgoVal pv, ErgoVal onv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_set_wavy expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  p->progress.shape = ergo_as_bool(onv) ? COGITO_PROGRESS_SHAPE_WAVY : COGITO_PROGRESS_SHAPE_FLAT;
  cogito_window_relayout(cogito_node_window(p));
}
static ErgoVal cogito_progress_get_wavy(ErgoVal pv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_get_wavy expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  return EV_BOOL(p->progress.shape == COGITO_PROGRESS_SHAPE_WAVY);
}
static void cogito_progress_set_circular(ErgoVal pv, ErgoVal onv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_set_circular expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  p->progress.circular = ergo_as_bool(onv);
  cogito_window_relayout(cogito_node_window(p));
}
static ErgoVal cogito_progress_get_circular(ErgoVal pv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_get_circular expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  return EV_BOOL(p->progress.circular);
}
