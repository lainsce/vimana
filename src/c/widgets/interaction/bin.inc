static CogitoColor cogito_bin_stable_accent_seed(CogitoNode* n, CogitoColor in) {
  double h = 0.0;
  double c = 0.0;
  double t = 0.0;
  cogito_rgb_to_hct(in, &h, &c, &t);

  if (c < 12.0) c = 12.0;
  if (c > 48.0) c = 48.0;
  if (t < 36.0) t = 36.0;
  if (t > 68.0) t = 68.0;

  CogitoColor out = cogito_hct_to_rgb(h, c, t);
  out.a = 255;

  if (n && n->bin_accent_set) {
    out.r = (uint8_t)(((int)n->bin_accent_color.r * 7 + (int)out.r * 3) / 10);
    out.g = (uint8_t)(((int)n->bin_accent_color.g * 7 + (int)out.g * 3) / 10);
    out.b = (uint8_t)(((int)n->bin_accent_color.b * 7 + (int)out.b * 3) / 10);
    out.a = 255;
  }

  return out;
}

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
    n->bin_accent_color = cogito_bin_stable_accent_seed(n, c);
    n->bin_accent_set = true;
    CogitoNode* win = cogito_node_window(n);
    if (win) cogito_window_relayout(win);
  }

  if (temp && s) yis_release_val(YV_STR(s));
}
