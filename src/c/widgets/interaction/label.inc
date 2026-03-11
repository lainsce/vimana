static CogitoNode* cogito_label_new_obj(YisStr* label) {
  CogitoNode* lab = cogito_node_new(COGITO_LABEL);
  cogito_node_set_text(lab, label);
  return lab;
}
static YisVal cogito_label_new(YisVal text) {
  YisStr* ts = stdr_to_string(text);
  CogitoNode* lab = cogito_label_new_obj(ts);
  return YV_OBJ(lab);
}
static void cogito_label_set_text(YisVal labelv, YisVal textv) {
  if (labelv.tag != EVT_OBJ) yis_trap("cogito.label_set_text expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  YisStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  if (n->marquee_active) {
    n->marquee_active = false;
    cogito_marquee_animating_count--;
  }
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_label_set_wrap(YisVal labelv, YisVal onv) {
  if (labelv.tag != EVT_OBJ) yis_trap("cogito.label_set_wrap expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  n->text_wrap = yis_as_bool(onv);
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_label_set_max_width_chars(YisVal labelv, YisVal maxv) {
  if (labelv.tag != EVT_OBJ) yis_trap("cogito.label_set_max_width_chars expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  int max_chars = (int)yis_as_int(maxv);
  if (max_chars < 0) {
    n->label_max_width_chars = 0;
    n->label_max_width_chars_set = false;
  } else {
    n->label_max_width_chars = max_chars;
    n->label_max_width_chars_set = true;
  }
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_label_set_line_chars(YisVal labelv, YisVal maxv) {
  if (labelv.tag != EVT_OBJ) yis_trap("cogito.label_set_line_chars expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  int line_chars = (int)yis_as_int(maxv);
  if (line_chars < 0) {
    n->label_line_chars = 0;
    n->label_line_chars_set = false;
  } else {
    n->label_line_chars = line_chars;
    n->label_line_chars_set = true;
  }
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_label_set_ellipsis(YisVal labelv, YisVal onv) {
  if (labelv.tag != EVT_OBJ) yis_trap("cogito.label_set_ellipsis expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  n->text_ellipsis = yis_as_bool(onv);
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_label_set_fade(YisVal labelv, YisVal onv) {
  if (labelv.tag != EVT_OBJ) yis_trap("cogito.label_set_fade expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  n->text_fade = yis_as_bool(onv);
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_label_set_marquee(YisVal labelv, YisVal onv) {
  if (labelv.tag != EVT_OBJ) yis_trap("cogito.label_set_marquee expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  bool on = yis_as_bool(onv);
  if (n->text_marquee == on) return;
  n->text_marquee = on;
  if (!on && n->marquee_active) {
    n->marquee_active = false;
    cogito_marquee_animating_count--;
  }
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_label_set_align(YisVal labelv, YisVal alignv) {
  if (labelv.tag != EVT_OBJ) yis_trap("cogito.label_set_align expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  n->text_align = (int)yis_as_int(alignv);
  if (n->text_align < 0) n->text_align = 0;
  if (n->text_align > 2) n->text_align = 2;
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_label_set_class(YisVal labelv, YisVal classv) {
  if (labelv.tag != EVT_OBJ) yis_trap("cogito.label_set_class expects label");
  cogito_node_set_class(labelv, classv);
}
