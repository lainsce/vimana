static double cogito_slider_clamp(double v, double min, double max) {
  if (max < min) {
    double tmp = min;
    min = max;
    max = tmp;
  }
  if (v < min) v = min;
  if (v > max) v = max;
  return v;
}

static double cogito_slider_step_normalized(double step) {
  if (step < 0.0) step = -step;
  if (step < 0.000001) return 0.0;
  return step;
}

static double cogito_slider_snap_to_step(const CogitoNode* n, double v) {
  if (!n) return v;
  v = cogito_slider_clamp(v, n->kd->slider.min, n->kd->slider.max);
  double step = cogito_slider_step_normalized(n->kd->slider.step);
  if (step <= 0.0) return v;
  double offset = (v - n->kd->slider.min) / step;
  double snapped = n->kd->slider.min + floor(offset + 0.5) * step;
  return cogito_slider_clamp(snapped, n->kd->slider.min, n->kd->slider.max);
}

static uint8_t cogito_slider_clamp_size_int(int size) {
  if (size < COGITO_SLIDER_SIZE_XS) return (uint8_t)COGITO_SLIDER_SIZE_XS;
  if (size > COGITO_SLIDER_SIZE_XL) return (uint8_t)COGITO_SLIDER_SIZE_XL;
  return (uint8_t)size;
}

static void cogito_slider_normalize_range(CogitoNode* n) {
  if (!n) return;
  if (n->kd->slider.max < n->kd->slider.min) {
    double tmp = n->kd->slider.min;
    n->kd->slider.min = n->kd->slider.max;
    n->kd->slider.max = tmp;
  }
  n->kd->slider.value = cogito_slider_snap_to_step(n, n->kd->slider.value);
  n->kd->slider.value2 = cogito_slider_snap_to_step(n, n->kd->slider.value2);
  if (n->kd->slider.value2 < n->kd->slider.value) {
    double tmp = n->kd->slider.value;
    n->kd->slider.value = n->kd->slider.value2;
    n->kd->slider.value2 = tmp;
  }
}

static CogitoNode* cogito_slider_new_obj(double min, double max, double value) {
  CogitoNode* sl = cogito_node_new(COGITO_SLIDER);
  sl->kd->slider.min = min;
  sl->kd->slider.max = max;
  if (sl->kd->slider.max < sl->kd->slider.min) {
    double tmp = sl->kd->slider.min;
    sl->kd->slider.min = sl->kd->slider.max;
    sl->kd->slider.max = tmp;
  }
  sl->kd->slider.value = cogito_slider_clamp(value, sl->kd->slider.min, sl->kd->slider.max);
  sl->kd->slider.value2 = sl->kd->slider.value;
  sl->kd->slider.step = 0.0;
  sl->kd->slider.mode = (sl->kd->slider.min < 0.0 && sl->kd->slider.max > 0.0)
      ? COGITO_SLIDER_MODE_CENTERED
      : COGITO_SLIDER_MODE_NORMAL;
  sl->kd->slider.show_step_dots = false;
  sl->kd->slider.show_end_dots = true;
  sl->kd->slider.show_value_bubble = true;
  sl->kd->slider.bubble_scale = 1.0;
  sl->kd->slider.active_handle = 0;
  sl->kd->slider.size = COGITO_SLIDER_SIZE_XS;
  sl->kd->slider.shape = COGITO_SLIDER_SHAPE_FLAT;
  return sl;
}

static CogitoNode* cogito_slider_range_new_obj(double min, double max, double start, double end) {
  CogitoNode* sl = cogito_node_new(COGITO_SLIDER);
  sl->kd->slider.min = min;
  sl->kd->slider.max = max;
  sl->kd->slider.value = start;
  sl->kd->slider.value2 = end;
  sl->kd->slider.step = 0.0;
  sl->kd->slider.mode = COGITO_SLIDER_MODE_RANGE;
  sl->kd->slider.show_step_dots = false;
  sl->kd->slider.show_end_dots = true;
  sl->kd->slider.show_value_bubble = true;
  sl->kd->slider.bubble_scale = 1.0;
  sl->kd->slider.active_handle = 1;
  sl->kd->slider.size = COGITO_SLIDER_SIZE_XS;
  sl->kd->slider.shape = COGITO_SLIDER_SHAPE_FLAT;
  cogito_slider_normalize_range(sl);
  return sl;
}

static YisVal cogito_slider_new(YisVal minv, YisVal maxv, YisVal valv) {
  double min = yis_as_float(minv);
  double max = yis_as_float(maxv);
  double val = yis_as_float(valv);
  return YV_OBJ(cogito_slider_new_obj(min, max, val));
}

static YisVal cogito_slider_range_new(YisVal minv, YisVal maxv, YisVal startv, YisVal endv) {
  double min = yis_as_float(minv);
  double max = yis_as_float(maxv);
  double start = yis_as_float(startv);
  double end = yis_as_float(endv);
  return YV_OBJ(cogito_slider_range_new_obj(min, max, start, end));
}

static void cogito_slider_set_value(YisVal nodev, YisVal valv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_value expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  double v = cogito_slider_snap_to_step(n, yis_as_float(valv));
  if (n->kd->slider.mode == COGITO_SLIDER_MODE_RANGE) {
    n->kd->slider.value = v;
    if (n->kd->slider.value > n->kd->slider.value2) n->kd->slider.value2 = n->kd->slider.value;
    n->kd->slider.active_handle = 1;
  } else {
    n->kd->slider.value = v;
    n->kd->slider.value2 = v;
    n->kd->slider.active_handle = 0;
  }
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_slider_get_value(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_get_value expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return YV_FLOAT(n->kd->slider.value);
}

static void cogito_slider_set_bounds(YisVal nodev, YisVal minv, YisVal maxv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_bounds expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->kd->slider.min = yis_as_float(minv);
  n->kd->slider.max = yis_as_float(maxv);
  if (n->kd->slider.max < n->kd->slider.min) {
    double tmp = n->kd->slider.min;
    n->kd->slider.min = n->kd->slider.max;
    n->kd->slider.max = tmp;
  }
  n->kd->slider.value = cogito_slider_snap_to_step(n, n->kd->slider.value);
  n->kd->slider.value2 = cogito_slider_snap_to_step(n, n->kd->slider.value2);
  if (n->kd->slider.mode == COGITO_SLIDER_MODE_RANGE && n->kd->slider.value2 < n->kd->slider.value) {
    double tmp = n->kd->slider.value;
    n->kd->slider.value = n->kd->slider.value2;
    n->kd->slider.value2 = tmp;
  }
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_slider_set_centered(YisVal nodev, YisVal onv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_centered expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  bool on = yis_as_bool(onv);
  if (n->kd->slider.mode == COGITO_SLIDER_MODE_RANGE && !on) {
    n->kd->slider.mode = COGITO_SLIDER_MODE_NORMAL;
  } else if (n->kd->slider.mode == COGITO_SLIDER_MODE_RANGE && on) {
    n->kd->slider.mode = (n->kd->slider.min < 0.0 && n->kd->slider.max > 0.0)
        ? COGITO_SLIDER_MODE_CENTERED
        : COGITO_SLIDER_MODE_NORMAL;
    n->kd->slider.value2 = n->kd->slider.value;
  } else if (on && n->kd->slider.min < 0.0 && n->kd->slider.max > 0.0) {
    n->kd->slider.mode = COGITO_SLIDER_MODE_CENTERED;
  } else {
    n->kd->slider.mode = COGITO_SLIDER_MODE_NORMAL;
  }
  n->kd->slider.active_handle = 0;
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_slider_get_centered(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_get_centered expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return YV_BOOL(n->kd->slider.mode == COGITO_SLIDER_MODE_CENTERED);
}

static void cogito_slider_set_range(YisVal nodev, YisVal startv, YisVal endv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_range expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->kd->slider.mode = COGITO_SLIDER_MODE_RANGE;
  n->kd->slider.value = cogito_slider_snap_to_step(n, yis_as_float(startv));
  n->kd->slider.value2 = cogito_slider_snap_to_step(n, yis_as_float(endv));
  n->kd->slider.active_handle = 1;
  cogito_slider_normalize_range(n);
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_slider_set_range_start(YisVal nodev, YisVal startv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_range_start expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kd->slider.mode != COGITO_SLIDER_MODE_RANGE) {
    n->kd->slider.mode = COGITO_SLIDER_MODE_RANGE;
    n->kd->slider.value2 = n->kd->slider.value;
  }
  double v = cogito_slider_snap_to_step(n, yis_as_float(startv));
  n->kd->slider.value = v;
  if (n->kd->slider.value > n->kd->slider.value2) n->kd->slider.value2 = n->kd->slider.value;
  n->kd->slider.active_handle = 1;
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_slider_set_range_end(YisVal nodev, YisVal endv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_range_end expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kd->slider.mode != COGITO_SLIDER_MODE_RANGE) {
    n->kd->slider.mode = COGITO_SLIDER_MODE_RANGE;
    n->kd->slider.value2 = n->kd->slider.value;
  }
  double v = cogito_slider_snap_to_step(n, yis_as_float(endv));
  n->kd->slider.value2 = v;
  if (n->kd->slider.value2 < n->kd->slider.value) n->kd->slider.value = n->kd->slider.value2;
  n->kd->slider.active_handle = 2;
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_slider_get_range_start(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_get_range_start expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return YV_FLOAT(n->kd->slider.value);
}

static YisVal cogito_slider_get_range_end(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_get_range_end expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return YV_FLOAT(n->kd->slider.value2);
}

static void cogito_slider_set_step(YisVal nodev, YisVal stepv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_step expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->kd->slider.step = cogito_slider_step_normalized(yis_as_float(stepv));
  if (n->kd->slider.mode == COGITO_SLIDER_MODE_RANGE) {
    cogito_slider_normalize_range(n);
  } else {
    n->kd->slider.value = cogito_slider_snap_to_step(n, n->kd->slider.value);
    n->kd->slider.value2 = n->kd->slider.value;
  }
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_slider_get_step(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_get_step expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return YV_FLOAT(n->kd->slider.step);
}

static void cogito_slider_set_step_dots(YisVal nodev, YisVal onv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_step_dots expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->kd->slider.show_step_dots = yis_as_bool(onv);
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_slider_get_step_dots(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_get_step_dots expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return YV_BOOL(n->kd->slider.show_step_dots);
}

static void cogito_slider_set_end_dots(YisVal nodev, YisVal onv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_end_dots expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->kd->slider.show_end_dots = yis_as_bool(onv);
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_slider_get_end_dots(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_get_end_dots expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return YV_BOOL(n->kd->slider.show_end_dots);
}

static void cogito_slider_set_value_bubble(YisVal nodev, YisVal onv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_value_bubble expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->kd->slider.show_value_bubble = yis_as_bool(onv);
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_slider_get_value_bubble(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_get_value_bubble expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return YV_BOOL(n->kd->slider.show_value_bubble);
}

static void cogito_slider_set_bubble_format(YisVal nodev, YisVal fmtv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_bubble_format expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kd->slider.bubble_format) {
    yis_release_val(YV_STR(n->kd->slider.bubble_format));
    n->kd->slider.bubble_format = NULL;
  }
  if (fmtv.tag == EVT_NULL) {
    cogito_window_relayout(cogito_node_window(n));
    return;
  }
  YisStr* s = stdr_to_string(fmtv);
  n->kd->slider.bubble_format = s;
  if (s) yis_retain_val(YV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_slider_get_bubble_format(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_get_bubble_format expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* s = n->kd->slider.bubble_format ? n->kd->slider.bubble_format : stdr_str_lit("");
  yis_retain_val(YV_STR(s));
  return YV_STR(s);
}

static void cogito_slider_set_bubble_scale(YisVal nodev, YisVal scalev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_bubble_scale expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->kd->slider.bubble_scale = yis_as_float(scalev);
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_slider_get_bubble_scale(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_get_bubble_scale expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return YV_FLOAT(n->kd->slider.bubble_scale);
}

static void cogito_slider_set_size(YisVal nodev, YisVal sizev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_size expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->kd->slider.size = cogito_slider_clamp_size_int((int)yis_as_int(sizev));
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_slider_get_size(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_get_size expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return YV_INT((int)n->kd->slider.size);
}

static void cogito_slider_set_icon(YisVal nodev, YisVal iconv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_icon expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (iconv.tag == EVT_NULL) {
    cogito_node_set_icon(n, NULL);
  } else if (iconv.tag == EVT_STR) {
    cogito_node_set_icon(n, (YisStr*)iconv.as.p);
  } else {
    YisStr* s = stdr_to_string(iconv);
    cogito_node_set_icon(n, s);
    if (s) yis_release_val(YV_STR(s));
  }
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_slider_on_change(YisVal nodev, YisVal handler) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_on_change expects slider");
  if (handler.tag != EVT_FN) yis_trap("cogito.slider_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->on_change) yis_release_val(YV_FN(n->on_change));
  n->on_change = (YisFn*)handler.as.p;
  yis_retain_val(YV_FN(n->on_change));
}

static void cogito_slider_set_wavy(YisVal nodev, YisVal onv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_set_wavy expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->kd->slider.shape = yis_as_bool(onv) ? COGITO_SLIDER_SHAPE_WAVY : COGITO_SLIDER_SHAPE_FLAT;
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_slider_get_wavy(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.slider_get_wavy expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return YV_BOOL(n->kd->slider.shape == COGITO_SLIDER_SHAPE_WAVY);
}
