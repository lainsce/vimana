static void cogito_revealer_set_show_children(YisVal revealerv, YisVal showv) {
  if (revealerv.tag != EVT_OBJ) yis_trap("cogito.revealer_set_show_children expects node");
  CogitoNode* n = (CogitoNode*)revealerv.as.p;
  bool show = yis_as_bool(showv);
  if (n->revealer_show_children == show) return;
  n->revealer_show_children = show;
  CogitoNode* win = cogito_node_window(n);
  if (win) cogito_window_relayout(win);
}
