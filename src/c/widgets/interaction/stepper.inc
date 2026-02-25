static CogitoNode* cogito_stepper_new_obj(double min, double max, double value, double step) {
  CogitoNode* st = cogito_node_new(COGITO_STEPPER);
  st->stepper.min = min;
  st->stepper.max = max;
  if (st->stepper.max < st->stepper.min) {
    double tmp = st->stepper.min;
    st->stepper.min = st->stepper.max;
    st->stepper.max = tmp;
  }
  if (value < st->stepper.min) value = st->stepper.min;
  if (value > st->stepper.max) value = st->stepper.max;
  st->stepper.value = value;
  st->stepper.step = step;
  return st;
}
static ErgoVal cogito_stepper_new(ErgoVal minv, ErgoVal maxv, ErgoVal valv, ErgoVal stepv) {
  double min = ergo_as_float(minv);
  double max = ergo_as_float(maxv);
  double val = ergo_as_float(valv);
  double step = ergo_as_float(stepv);
  return EV_OBJ(cogito_stepper_new_obj(min, max, val, step));
}
static void cogito_stepper_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.stepper_on_change expects stepper");
  if (handler.tag != EVT_FN) ergo_trap("cogito.stepper_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_STEPPER) ergo_trap("cogito.stepper_on_change expects stepper");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
static ErgoVal cogito_stepper_get_value(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.stepper_value expects stepper");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_STEPPER) ergo_trap("cogito.stepper_value expects stepper");
  return EV_FLOAT(n->stepper.value);
}
static void cogito_stepper_hit_test(CogitoNode* n, int mx, int my) {
  int btn_w = n->h;
  n->stepper.hit_minus = cogito_hit_rect_min48(mx, my, n->x, n->y, btn_w, n->h);
  n->stepper.hit_plus = cogito_hit_rect_min48(mx, my, n->x + n->w - btn_w, n->y, btn_w, n->h);
}

static CogitoNode* cogito_find_stepper_hit(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_STEPPER) {
    if (x >= n->x && x < n->x + n->w && y >= n->y && y < n->y + n->h) {
      return n;
    }
  }
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* hit = cogito_find_stepper_hit(n->children[i], x, y);
    if (hit) return hit;
  }
  return NULL;
}
static void cogito_stepper_click(CogitoNode* n, int mx, int my) {
  int btn_w = n->h;
  bool hit_minus = cogito_hit_rect_min48(mx, my, n->x, n->y, btn_w, n->h);
  bool hit_plus = cogito_hit_rect_min48(mx, my, n->x + n->w - btn_w, n->y, btn_w, n->h);
  double prev = n->stepper.value;
  if (hit_minus && n->stepper.value > n->stepper.min) {
    n->stepper.value -= n->stepper.step;
    if (n->stepper.value < n->stepper.min) n->stepper.value = n->stepper.min;
  } else if (hit_plus && n->stepper.value < n->stepper.max) {
    n->stepper.value += n->stepper.step;
    if (n->stepper.value > n->stepper.max) n->stepper.value = n->stepper.max;
  }
  if (n->stepper.value != prev) {
    cogito_invoke_change(n);
  }
}
