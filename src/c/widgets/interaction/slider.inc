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

static uint8_t cogito_slider_clamp_size_int(int size) {
  if (size < COGITO_SLIDER_SIZE_XS) return (uint8_t)COGITO_SLIDER_SIZE_XS;
  if (size > COGITO_SLIDER_SIZE_XL) return (uint8_t)COGITO_SLIDER_SIZE_XL;
  return (uint8_t)size;
}

static void cogito_slider_normalize_range(CogitoNode* n) {
  if (!n) return;
  if (n->slider.max < n->slider.min) {
    double tmp = n->slider.min;
    n->slider.min = n->slider.max;
    n->slider.max = tmp;
  }
  n->slider.value = cogito_slider_clamp(n->slider.value, n->slider.min, n->slider.max);
  n->slider.value2 = cogito_slider_clamp(n->slider.value2, n->slider.min, n->slider.max);
  if (n->slider.value2 < n->slider.value) {
    double tmp = n->slider.value;
    n->slider.value = n->slider.value2;
    n->slider.value2 = tmp;
  }
}

static CogitoNode* cogito_slider_new_obj(double min, double max, double value) {
  CogitoNode* sl = cogito_node_new(COGITO_SLIDER);
  sl->slider.min = min;
  sl->slider.max = max;
  if (sl->slider.max < sl->slider.min) {
    double tmp = sl->slider.min;
    sl->slider.min = sl->slider.max;
    sl->slider.max = tmp;
  }
  sl->slider.value = cogito_slider_clamp(value, sl->slider.min, sl->slider.max);
  sl->slider.value2 = sl->slider.value;
  sl->slider.mode = (sl->slider.min < 0.0 && sl->slider.max > 0.0)
      ? COGITO_SLIDER_MODE_CENTERED
      : COGITO_SLIDER_MODE_NORMAL;
  sl->slider.active_handle = 0;
  sl->slider.size = COGITO_SLIDER_SIZE_XS;
  return sl;
}

static CogitoNode* cogito_slider_range_new_obj(double min, double max, double start, double end) {
  CogitoNode* sl = cogito_node_new(COGITO_SLIDER);
  sl->slider.min = min;
  sl->slider.max = max;
  sl->slider.value = start;
  sl->slider.value2 = end;
  sl->slider.mode = COGITO_SLIDER_MODE_RANGE;
  sl->slider.active_handle = 1;
  sl->slider.size = COGITO_SLIDER_SIZE_XS;
  cogito_slider_normalize_range(sl);
  return sl;
}

static ErgoVal cogito_slider_new(ErgoVal minv, ErgoVal maxv, ErgoVal valv) {
  double min = ergo_as_float(minv);
  double max = ergo_as_float(maxv);
  double val = ergo_as_float(valv);
  return EV_OBJ(cogito_slider_new_obj(min, max, val));
}

static ErgoVal cogito_slider_range_new(ErgoVal minv, ErgoVal maxv, ErgoVal startv, ErgoVal endv) {
  double min = ergo_as_float(minv);
  double max = ergo_as_float(maxv);
  double start = ergo_as_float(startv);
  double end = ergo_as_float(endv);
  return EV_OBJ(cogito_slider_range_new_obj(min, max, start, end));
}

static void cogito_slider_set_value(ErgoVal nodev, ErgoVal valv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_set_value expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  double v = cogito_slider_clamp(ergo_as_float(valv), n->slider.min, n->slider.max);
  if (n->slider.mode == COGITO_SLIDER_MODE_RANGE) {
    n->slider.value = v;
    if (n->slider.value > n->slider.value2) n->slider.value2 = n->slider.value;
    n->slider.active_handle = 1;
  } else {
    n->slider.value = v;
    n->slider.value2 = v;
    n->slider.active_handle = 0;
  }
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_slider_get_value(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_get_value expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_FLOAT(n->slider.value);
}

static void cogito_slider_set_centered(ErgoVal nodev, ErgoVal onv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_set_centered expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  bool on = ergo_as_bool(onv);
  if (n->slider.mode == COGITO_SLIDER_MODE_RANGE && !on) {
    n->slider.mode = COGITO_SLIDER_MODE_NORMAL;
  } else if (n->slider.mode == COGITO_SLIDER_MODE_RANGE && on) {
    n->slider.mode = (n->slider.min < 0.0 && n->slider.max > 0.0)
        ? COGITO_SLIDER_MODE_CENTERED
        : COGITO_SLIDER_MODE_NORMAL;
    n->slider.value2 = n->slider.value;
  } else if (on && n->slider.min < 0.0 && n->slider.max > 0.0) {
    n->slider.mode = COGITO_SLIDER_MODE_CENTERED;
  } else {
    n->slider.mode = COGITO_SLIDER_MODE_NORMAL;
  }
  n->slider.active_handle = 0;
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_slider_get_centered(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_get_centered expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_BOOL(n->slider.mode == COGITO_SLIDER_MODE_CENTERED);
}

static void cogito_slider_set_range(ErgoVal nodev, ErgoVal startv, ErgoVal endv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_set_range expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->slider.mode = COGITO_SLIDER_MODE_RANGE;
  n->slider.value = ergo_as_float(startv);
  n->slider.value2 = ergo_as_float(endv);
  n->slider.active_handle = 1;
  cogito_slider_normalize_range(n);
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_slider_set_range_start(ErgoVal nodev, ErgoVal startv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_set_range_start expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->slider.mode != COGITO_SLIDER_MODE_RANGE) {
    n->slider.mode = COGITO_SLIDER_MODE_RANGE;
    n->slider.value2 = n->slider.value;
  }
  double v = cogito_slider_clamp(ergo_as_float(startv), n->slider.min, n->slider.max);
  n->slider.value = v;
  if (n->slider.value > n->slider.value2) n->slider.value2 = n->slider.value;
  n->slider.active_handle = 1;
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_slider_set_range_end(ErgoVal nodev, ErgoVal endv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_set_range_end expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->slider.mode != COGITO_SLIDER_MODE_RANGE) {
    n->slider.mode = COGITO_SLIDER_MODE_RANGE;
    n->slider.value2 = n->slider.value;
  }
  double v = cogito_slider_clamp(ergo_as_float(endv), n->slider.min, n->slider.max);
  n->slider.value2 = v;
  if (n->slider.value2 < n->slider.value) n->slider.value = n->slider.value2;
  n->slider.active_handle = 2;
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_slider_get_range_start(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_get_range_start expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_FLOAT(n->slider.value);
}

static ErgoVal cogito_slider_get_range_end(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_get_range_end expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_FLOAT(n->slider.value2);
}

static void cogito_slider_set_size(ErgoVal nodev, ErgoVal sizev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_set_size expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->slider.size = cogito_slider_clamp_size_int((int)ergo_as_int(sizev));
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_slider_get_size(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_get_size expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_INT((int)n->slider.size);
}

static void cogito_slider_set_icon(ErgoVal nodev, ErgoVal iconv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_set_icon expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (iconv.tag == EVT_NULL) {
    cogito_node_set_icon(n, NULL);
  } else if (iconv.tag == EVT_STR) {
    cogito_node_set_icon(n, (ErgoStr*)iconv.as.p);
  } else {
    ErgoStr* s = stdr_to_string(iconv);
    cogito_node_set_icon(n, s);
    if (s) ergo_release_val(EV_STR(s));
  }
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_slider_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_on_change expects slider");
  if (handler.tag != EVT_FN) ergo_trap("cogito.slider_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
