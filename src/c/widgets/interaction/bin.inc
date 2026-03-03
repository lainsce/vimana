static void cogito_bin_set_accent_color(YisVal binv, YisVal colorv) {
  if (binv.tag != EVT_OBJ) yis_trap("cogito.bin_set_accent_color expects node");
  CogitoNode* n = (CogitoNode*)binv.as.p;

  if (colorv.tag == EVT_NULL) {
    n->bin_accent_set = false;
    CogitoNode* win = cogito_node_window(n);
    if (win) cogito_window_relayout(win);
    return;
  }

  YisStr* s = NULL;
  bool temp = false;
  if (colorv.tag == EVT_STR) {
    s = (YisStr*)colorv.as.p;
  } else {
    s = stdr_to_string(colorv);
    temp = true;
  }

  CogitoColor c = {0};
  if (s && s->data && cogito_hex_to_color(s->data, &c)) {
    n->bin_accent_color = c;
    n->bin_accent_set = true;
    CogitoNode* win = cogito_node_window(n);
    if (win) cogito_window_relayout(win);
  }

  if (temp && s) yis_release_val(YV_STR(s));
}
