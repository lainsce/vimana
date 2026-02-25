static bool cogito_timepicker_panel_set_from_pointer(CogitoNode* panel, int mx, int my, bool require_hit, bool* out_changed) {
  if (out_changed) *out_changed = false;
  if (!panel || panel->kind != COGITO_TIMEPICKER || !panel->timepicker.popover_panel) return false;
  if (!panel->timepicker.dial_visible) return false;

  CogitoTimePickerPanelLayout ly;
  cogito_timepicker_panel_layout(panel, &ly);
  int dx = mx - ly.dial_cx;
  int dy = my - ly.dial_cy;
  int dist2 = dx * dx + dy * dy;
  int hit_r = ly.dial_r + 10;
  if (require_hit && dist2 > hit_r * hit_r) return false;
  if (dist2 < 36) return false;

  uint8_t mode = panel->timepicker.mode;
  if (mode != COGITO_TIMEPICKER_MODE_MINUTE) mode = COGITO_TIMEPICKER_MODE_HOUR;
  double angle = cogito_timepicker_panel_pointer_angle(mx, my, ly.dial_cx, ly.dial_cy);
  bool changed = false;
  if (mode == COGITO_TIMEPICKER_MODE_MINUTE) {
    int new_minute = cogito_timepicker_panel_angle_to_minute(angle);
    if (new_minute != panel->timepicker.minute) {
      panel->timepicker.minute = new_minute;
      changed = true;
    }
  } else {
    int hour12 = cogito_timepicker_panel_angle_to_hour12(angle);
    int new_hour = cogito_timepicker_hour24_from_12(hour12, panel->timepicker.hour >= 12);
    if (new_hour != panel->timepicker.hour) {
      panel->timepicker.hour = new_hour;
      panel->timepicker.ampm = (new_hour >= 12);
      changed = true;
    }
  }
  if (out_changed) *out_changed = changed;
  return true;
}

static bool cogito_timepicker_drag_update(CogitoNode* panel, int mx, int my) {
  bool changed = false;
  if (!cogito_timepicker_panel_set_from_pointer(panel, mx, my, false, &changed)) return false;
  return changed;
}

static CogitoNode* cogito_timepicker_open_panel(CogitoNode* owner) {
  if (!owner || owner->kind != COGITO_TIMEPICKER) return NULL;
  cogito_timepicker_ensure_time(owner);

  CogitoNode* pop = cogito_popover_new_obj();
  pop->padding_set = true;
  pop->padding_left = 0;
  pop->padding_top = 0;
  pop->padding_right = 0;
  pop->padding_bottom = 0;
  pop->gap = 0;
  pop->radius_set = true;
  pop->border_radius = 24;

  CogitoNode* panel = cogito_node_new(COGITO_TIMEPICKER);
  panel->editable = false;
  panel->timepicker.popover_panel = true;
  panel->timepicker.owner = owner;
  panel->timepicker.hour = owner->timepicker.hour;
  panel->timepicker.minute = owner->timepicker.minute;
  panel->timepicker.ampm = owner->timepicker.ampm;
  panel->timepicker.dial_visible = owner->timepicker.dial_visible;
  panel->timepicker.mode = COGITO_TIMEPICKER_MODE_HOUR;
  panel->timepicker.initialized = true;

  cogito_children_add(pop, panel);
  ergo_release_val(EV_OBJ(panel));  // child list now owns the panel reference
  return pop;
}

static void cogito_timepicker_show_popover(CogitoNode* anchor) {
  if (!anchor || anchor->kind != COGITO_TIMEPICKER) return;
  CogitoNode* win = cogito_node_window(anchor);
  if (!win) return;

  CogitoNode* pop = cogito_timepicker_open_panel(anchor);
  if (!pop) return;
  cogito_popover_show(EV_OBJ(anchor), EV_OBJ(pop));
  ergo_release_val(EV_OBJ(pop));  // window popover slot now owns the reference
}

static bool cogito_timepicker_hit_panel(CogitoNode* panel, int mx, int my) {
  if (!panel || panel->kind != COGITO_TIMEPICKER || !panel->timepicker.popover_panel) return false;
  cogito_timepicker_ensure_time(panel);

  CogitoNode* owner = panel->timepicker.owner;
  CogitoNode* win = cogito_node_window(panel);
  CogitoTimePickerPanelLayout ly;
  cogito_timepicker_panel_layout(panel, &ly);

  if (cogito_hit_rect_min48(mx, my, ly.keyboard_x, ly.keyboard_y, ly.keyboard_w, ly.keyboard_h)) {
    panel->timepicker.dial_visible = !panel->timepicker.dial_visible;
    if (!panel->timepicker.dial_visible) {
      if (cogito_pointer_capture == panel) cogito_pointer_capture = NULL;
      if (cogito_timepicker_active == panel) cogito_timepicker_active = NULL;
    }
    CogitoNode* relayout_win = win ? win : cogito_node_window(owner ? owner : panel);
    cogito_window_relayout(relayout_win);
    return true;
  }

  if (cogito_hit_rect_min48(mx, my, ly.cancel_x, ly.cancel_y, ly.cancel_w, ly.cancel_h)) {
    if (owner && owner->kind == COGITO_TIMEPICKER) {
      owner->timepicker.dial_visible = panel->timepicker.dial_visible;
    }
    if (cogito_pointer_capture == panel) cogito_pointer_capture = NULL;
    if (cogito_timepicker_active == panel) cogito_timepicker_active = NULL;
    if (win) cogito_popover_close(EV_OBJ(win));
    return true;
  }

  if (cogito_hit_rect_min48(mx, my, ly.ok_x, ly.ok_y, ly.ok_w, ly.ok_h)) {
    bool changed = false;
    if (owner && owner->kind == COGITO_TIMEPICKER) {
      cogito_timepicker_ensure_time(owner);
      changed = owner->timepicker.hour != panel->timepicker.hour ||
                owner->timepicker.minute != panel->timepicker.minute;
      owner->timepicker.hour = panel->timepicker.hour;
      owner->timepicker.minute = panel->timepicker.minute;
      owner->timepicker.ampm = (owner->timepicker.hour >= 12);
      owner->timepicker.dial_visible = panel->timepicker.dial_visible;
      owner->timepicker.initialized = true;
    }
    if (cogito_pointer_capture == panel) cogito_pointer_capture = NULL;
    if (cogito_timepicker_active == panel) cogito_timepicker_active = NULL;
    if (win) cogito_popover_close(EV_OBJ(win));
    if (changed && owner) cogito_invoke_change(owner);
    return true;
  }

  bool needs_relayout = false;
  if (cogito_hit_rect_min48(mx, my, ly.hour_x, ly.hour_y, ly.hour_w, ly.hour_h)) {
    if (panel->timepicker.mode != COGITO_TIMEPICKER_MODE_HOUR) {
      panel->timepicker.mode = COGITO_TIMEPICKER_MODE_HOUR;
      needs_relayout = true;
    }
  } else if (cogito_hit_rect_min48(mx, my, ly.minute_x, ly.minute_y, ly.minute_w, ly.minute_h)) {
    if (panel->timepicker.mode != COGITO_TIMEPICKER_MODE_MINUTE) {
      panel->timepicker.mode = COGITO_TIMEPICKER_MODE_MINUTE;
      needs_relayout = true;
    }
  } else if (cogito_hit_rect_min48(mx, my, ly.ampm_x, ly.ampm_y, ly.ampm_w, ly.ampm_split_h)) {
    if (panel->timepicker.hour >= 12) {
      panel->timepicker.hour -= 12;
      panel->timepicker.ampm = false;
      needs_relayout = true;
    }
  } else if (cogito_hit_rect_min48(mx, my, ly.ampm_x, ly.ampm_y + ly.ampm_split_h, ly.ampm_w, ly.ampm_h - ly.ampm_split_h)) {
    if (panel->timepicker.hour < 12) {
      panel->timepicker.hour += 12;
      panel->timepicker.ampm = true;
      needs_relayout = true;
    }
  } else {
    bool dial_changed = false;
    bool dial_hit = cogito_timepicker_panel_set_from_pointer(panel, mx, my, true, &dial_changed);
    if (dial_hit) {
      needs_relayout = needs_relayout || dial_changed;
      cogito_timepicker_active = panel;
      cogito_pointer_capture = panel;
    }
  }

  if (needs_relayout) {
    CogitoNode* relayout_win = win ? win : cogito_node_window(owner ? owner : panel);
    cogito_window_relayout(relayout_win);
    return true;
  }
  return cogito_hit_rect(mx, my, panel->x, panel->y, panel->w, panel->h);
}

// TimePicker hit testing:
// - anchor mode: toggles the popover panel.
// - panel mode: handles full picker interactions and commit/cancel.
static bool cogito_timepicker_hit(CogitoNode* n, int mx, int my, bool clicked) {
  if (!n || n->kind != COGITO_TIMEPICKER || !clicked) return false;

  if (n->timepicker.popover_panel) {
    return cogito_timepicker_hit_panel(n, mx, my);
  }

  CogitoNode* win = cogito_node_window(n);
  if (!win) return false;
  if (win->popover_open && win->popover_anchor == n) {
    cogito_popover_close(EV_OBJ(win));
    return true;
  }
  cogito_timepicker_show_popover(n);
  return true;
}

static CogitoNode* cogito_timepicker_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_TIMEPICKER);
  n->editable = false;
  n->timepicker.mode = COGITO_TIMEPICKER_MODE_HOUR;
  n->timepicker.initialized = false;
  n->timepicker.popover_panel = false;
  n->timepicker.dial_visible = true;
  n->timepicker.owner = NULL;
  cogito_timepicker_ensure_time(n);
  return n;
}

static ErgoVal cogito_timepicker_new(void) {
  return EV_OBJ(cogito_timepicker_new_obj());
}

static void cogito_timepicker_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.timepicker_on_change expects timepicker");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_TIMEPICKER) ergo_trap("cogito.timepicker_on_change expects timepicker");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  if (handler.tag == EVT_FN) {
    n->on_change = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(n->on_change));
  } else {
    n->on_change = NULL;
  }
}

static ErgoVal cogito_timepicker_get_hour(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) return EV_INT(0);
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (!n || n->kind != COGITO_TIMEPICKER) return EV_INT(0);
  cogito_timepicker_ensure_time(n);
  return EV_INT(n->timepicker.hour);
}

static ErgoVal cogito_timepicker_get_minute(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) return EV_INT(0);
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (!n || n->kind != COGITO_TIMEPICKER) return EV_INT(0);
  cogito_timepicker_ensure_time(n);
  return EV_INT(n->timepicker.minute);
}

static void cogito_timepicker_set_time(ErgoVal nodev, ErgoVal hourv, ErgoVal minv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.timepicker_set_time expects timepicker");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (!n || n->kind != COGITO_TIMEPICKER) ergo_trap("cogito.timepicker_set_time expects timepicker");

  int h = 0, m = 0;
  if (hourv.tag == EVT_INT) h = (int)hourv.as.i;
  else if (hourv.tag == EVT_FLOAT) h = (int)hourv.as.f;
  if (minv.tag == EVT_INT) m = (int)minv.as.i;
  else if (minv.tag == EVT_FLOAT) m = (int)minv.as.f;

  if (h < 0) h = 0;
  if (h > 23) h = 23;
  if (m < 0) m = 0;
  if (m > 59) m = 59;

  n->timepicker.hour = h;
  n->timepicker.minute = m;
  n->timepicker.ampm = (h >= 12);
  n->timepicker.initialized = true;

  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_timepicker_click(CogitoNode* n, int mx, int my) {
  (void)cogito_timepicker_hit(n, mx, my, true);
}
