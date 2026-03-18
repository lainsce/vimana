static bool cogito_timepicker_panel_set_from_pointer(CogitoNode* panel, int mx, int my, bool require_hit, bool* out_changed) {
  if (out_changed) *out_changed = false;
  if (!panel || panel->kind != COGITO_TIMEPICKER || !panel->kd->timepicker.popover_panel) return false;
  if (!panel->kd->timepicker.dial_visible) return false;

  CogitoTimePickerPanelLayout ly;
  cogito_timepicker_panel_layout(panel, &ly);
  int dx = mx - ly.dial_cx;
  int dy = my - ly.dial_cy;
  int dist2 = dx * dx + dy * dy;
  int hit_r = ly.dial_r + 10;
  if (require_hit && dist2 > hit_r * hit_r) return false;
  if (dist2 < 36) return false;

  uint8_t mode = panel->kd->timepicker.mode;
  if (mode != COGITO_TIMEPICKER_MODE_MINUTE) mode = COGITO_TIMEPICKER_MODE_HOUR;
  double angle = cogito_timepicker_panel_pointer_angle(mx, my, ly.dial_cx, ly.dial_cy);
  bool changed = false;
  if (mode == COGITO_TIMEPICKER_MODE_MINUTE) {
    int new_minute = cogito_timepicker_panel_angle_to_minute(angle);
    if (new_minute != panel->kd->timepicker.minute) {
      panel->kd->timepicker.minute = new_minute;
      changed = true;
    }
  } else {
    int hour12 = cogito_timepicker_panel_angle_to_hour12(angle);
    int new_hour = cogito_timepicker_hour24_from_12(hour12, panel->kd->timepicker.hour >= 12);
    if (new_hour != panel->kd->timepicker.hour) {
      panel->kd->timepicker.hour = new_hour;
      panel->kd->timepicker.ampm = (new_hour >= 12);
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
  panel->kd->timepicker.popover_panel = true;
  panel->kd->timepicker.owner = owner;
  panel->kd->timepicker.hour = owner->kd->timepicker.hour;
  panel->kd->timepicker.minute = owner->kd->timepicker.minute;
  panel->kd->timepicker.ampm = owner->kd->timepicker.ampm;
  panel->kd->timepicker.dial_visible = owner->kd->timepicker.dial_visible;
  panel->kd->timepicker.mode = COGITO_TIMEPICKER_MODE_HOUR;
  panel->kd->timepicker.initialized = true;

  cogito_children_add(pop, panel);
  yis_release_val(YV_OBJ(panel));  // child list now owns the panel reference
  return pop;
}

static void cogito_timepicker_show_popover(CogitoNode* anchor) {
  if (!anchor || anchor->kind != COGITO_TIMEPICKER) return;
  CogitoNode* win = cogito_node_window(anchor);
  if (!win) return;

  CogitoNode* pop = cogito_timepicker_open_panel(anchor);
  if (!pop) return;
  cogito_popover_show(YV_OBJ(anchor), YV_OBJ(pop));
  yis_release_val(YV_OBJ(pop));  // window popover slot now owns the reference
}

static bool cogito_timepicker_hit_panel(CogitoNode* panel, int mx, int my) {
  if (!panel || panel->kind != COGITO_TIMEPICKER || !panel->kd->timepicker.popover_panel) return false;
  cogito_timepicker_ensure_time(panel);

  CogitoNode* owner = panel->kd->timepicker.owner;
  CogitoNode* win = cogito_node_window(panel);
  CogitoTimePickerPanelLayout ly;
  cogito_timepicker_panel_layout(panel, &ly);

  if (cogito_hit_rect_min48(mx, my, ly.keyboard_x, ly.keyboard_y, ly.keyboard_w, ly.keyboard_h)) {
    panel->kd->timepicker.dial_visible = !panel->kd->timepicker.dial_visible;
    if (!panel->kd->timepicker.dial_visible) {
      if (cogito_pointer_capture == panel) cogito_pointer_capture = NULL;
      if (cogito_timepicker_active == panel) cogito_timepicker_active = NULL;
    }
    CogitoNode* relayout_win = win ? win : cogito_node_window(owner ? owner : panel);
    cogito_window_relayout(relayout_win);
    return true;
  }

  if (cogito_hit_rect_min48(mx, my, ly.cancel_x, ly.cancel_y, ly.cancel_w, ly.cancel_h)) {
    if (owner && owner->kind == COGITO_TIMEPICKER) {
      owner->kd->timepicker.dial_visible = panel->kd->timepicker.dial_visible;
    }
    if (cogito_pointer_capture == panel) cogito_pointer_capture = NULL;
    if (cogito_timepicker_active == panel) cogito_timepicker_active = NULL;
    if (win) cogito_popover_close(YV_OBJ(win));
    return true;
  }

  if (cogito_hit_rect_min48(mx, my, ly.ok_x, ly.ok_y, ly.ok_w, ly.ok_h)) {
    bool changed = false;
    if (owner && owner->kind == COGITO_TIMEPICKER) {
      cogito_timepicker_ensure_time(owner);
      changed = owner->kd->timepicker.hour != panel->kd->timepicker.hour ||
                owner->kd->timepicker.minute != panel->kd->timepicker.minute;
      owner->kd->timepicker.hour = panel->kd->timepicker.hour;
      owner->kd->timepicker.minute = panel->kd->timepicker.minute;
      owner->kd->timepicker.ampm = (owner->kd->timepicker.hour >= 12);
      owner->kd->timepicker.dial_visible = panel->kd->timepicker.dial_visible;
      owner->kd->timepicker.initialized = true;
    }
    if (cogito_pointer_capture == panel) cogito_pointer_capture = NULL;
    if (cogito_timepicker_active == panel) cogito_timepicker_active = NULL;
    if (win) cogito_popover_close(YV_OBJ(win));
    if (changed && owner) cogito_invoke_change(owner);
    return true;
  }

  bool needs_relayout = false;
  if (cogito_hit_rect_min48(mx, my, ly.hour_x, ly.hour_y, ly.hour_w, ly.hour_h)) {
    if (panel->kd->timepicker.mode != COGITO_TIMEPICKER_MODE_HOUR) {
      panel->kd->timepicker.mode = COGITO_TIMEPICKER_MODE_HOUR;
      needs_relayout = true;
    }
  } else if (cogito_hit_rect_min48(mx, my, ly.minute_x, ly.minute_y, ly.minute_w, ly.minute_h)) {
    if (panel->kd->timepicker.mode != COGITO_TIMEPICKER_MODE_MINUTE) {
      panel->kd->timepicker.mode = COGITO_TIMEPICKER_MODE_MINUTE;
      needs_relayout = true;
    }
  } else if (cogito_hit_rect_min48(mx, my, ly.ampm_x, ly.ampm_y, ly.ampm_w, ly.ampm_split_h)) {
    if (panel->kd->timepicker.hour >= 12) {
      panel->kd->timepicker.hour -= 12;
      panel->kd->timepicker.ampm = false;
      needs_relayout = true;
    }
  } else if (cogito_hit_rect_min48(mx, my, ly.ampm_x, ly.ampm_y + ly.ampm_split_h, ly.ampm_w, ly.ampm_h - ly.ampm_split_h)) {
    if (panel->kd->timepicker.hour < 12) {
      panel->kd->timepicker.hour += 12;
      panel->kd->timepicker.ampm = true;
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

  if (n->kd->timepicker.popover_panel) {
    return cogito_timepicker_hit_panel(n, mx, my);
  }

  CogitoNode* win = cogito_node_window(n);
  if (!win) return false;
  if (win->popover_open && win->popover_anchor == n) {
    cogito_popover_close(YV_OBJ(win));
    return true;
  }
  cogito_timepicker_show_popover(n);
  return true;
}

static CogitoNode* cogito_timepicker_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_TIMEPICKER);
  n->editable = false;
  n->kd->timepicker.mode = COGITO_TIMEPICKER_MODE_HOUR;
  n->kd->timepicker.initialized = false;
  n->kd->timepicker.popover_panel = false;
  n->kd->timepicker.dial_visible = true;
  n->kd->timepicker.owner = NULL;
  cogito_timepicker_ensure_time(n);
  return n;
}

static YisVal cogito_timepicker_new(void) {
  return YV_OBJ(cogito_timepicker_new_obj());
}

static void cogito_timepicker_on_change(YisVal nodev, YisVal handler) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.timepicker_on_change expects timepicker");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_TIMEPICKER) yis_trap("cogito.timepicker_on_change expects timepicker");
  if (n->on_change) yis_release_val(YV_FN(n->on_change));
  if (handler.tag == EVT_FN) {
    n->on_change = (YisFn*)handler.as.p;
    yis_retain_val(YV_FN(n->on_change));
  } else {
    n->on_change = NULL;
  }
}

static YisVal cogito_timepicker_get_hour(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) return YV_INT(0);
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (!n || n->kind != COGITO_TIMEPICKER) return YV_INT(0);
  cogito_timepicker_ensure_time(n);
  return YV_INT(n->kd->timepicker.hour);
}

static YisVal cogito_timepicker_get_minute(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) return YV_INT(0);
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (!n || n->kind != COGITO_TIMEPICKER) return YV_INT(0);
  cogito_timepicker_ensure_time(n);
  return YV_INT(n->kd->timepicker.minute);
}

static void cogito_timepicker_set_time(YisVal nodev, YisVal hourv, YisVal minv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.timepicker_set_time expects timepicker");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (!n || n->kind != COGITO_TIMEPICKER) yis_trap("cogito.timepicker_set_time expects timepicker");

  int h = 0, m = 0;
  if (hourv.tag == EVT_INT) h = (int)hourv.as.i;
  else if (hourv.tag == EVT_FLOAT) h = (int)hourv.as.f;
  if (minv.tag == EVT_INT) m = (int)minv.as.i;
  else if (minv.tag == EVT_FLOAT) m = (int)minv.as.f;

  if (h < 0) h = 0;
  if (h > 23) h = 23;
  if (m < 0) m = 0;
  if (m > 59) m = 59;

  n->kd->timepicker.hour = h;
  n->kd->timepicker.minute = m;
  n->kd->timepicker.ampm = (h >= 12);
  n->kd->timepicker.initialized = true;

  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_timepicker_click(CogitoNode* n, int mx, int my) {
  (void)cogito_timepicker_hit(n, mx, my, true);
}
