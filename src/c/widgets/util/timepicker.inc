enum {
  COGITO_TIMEPICKER_MODE_HOUR = 0,
  COGITO_TIMEPICKER_MODE_MINUTE = 1,
  COGITO_TIMEPICKER_FIELD_MIN_WIDTH = 220,
  COGITO_TIMEPICKER_FIELD_HEIGHT = 56,
  COGITO_TIMEPICKER_POPOVER_W = 328,
  COGITO_TIMEPICKER_POP_SIDE = 24,
  COGITO_TIMEPICKER_POP_TOP = 24,
  COGITO_TIMEPICKER_POP_HEADER_H = 20,
  COGITO_TIMEPICKER_POP_TIME_H = 80,
  COGITO_TIMEPICKER_POP_TIME_TO_DIAL_GAP = 36,
  COGITO_TIMEPICKER_POP_DIAL_SIZE = 256,
  COGITO_TIMEPICKER_POP_FOOTER_H = 48,
  COGITO_TIMEPICKER_POP_BOTTOM_PAD = 24,
  COGITO_TIMEPICKER_POP_HOUR_W = 96,
  COGITO_TIMEPICKER_POP_MIN_W = 96,
  COGITO_TIMEPICKER_POP_HOUR_MIN_GAP = 24,
  COGITO_TIMEPICKER_POP_MIN_AMPM_GAP = 12,
  COGITO_TIMEPICKER_POP_AMPM_W = 52
};

typedef struct CogitoTimePickerPanelLayout {
  int header_x, header_y, header_w, header_h;
  int hour_x, hour_y, hour_w, hour_h;
  int minute_x, minute_y, minute_w, minute_h;
  int colon_x, colon_y;
  int ampm_x, ampm_y, ampm_w, ampm_h, ampm_split_h;
  int dial_x, dial_y, dial_d, dial_cx, dial_cy, dial_r;
  int footer_x, footer_y, footer_w, footer_h;
  int keyboard_x, keyboard_y, keyboard_w, keyboard_h;
  int cancel_x, cancel_y, cancel_w, cancel_h;
  int ok_x, ok_y, ok_w, ok_h;
} CogitoTimePickerPanelLayout;

static int cogito_timepicker_hour12_from_24(int hour24) {
  int h = hour24 % 12;
  if (h < 0) h += 12;
  if (h == 0) h = 12;
  return h;
}

static int cogito_timepicker_hour24_from_12(int hour12, bool pm) {
  int h12 = hour12;
  if (h12 < 1) h12 = 1;
  if (h12 > 12) h12 = 12;
  int h24 = h12 % 12;
  if (pm) h24 += 12;
  return h24;
}

static void cogito_timepicker_format_display(int hour24, int minute, bool include_ampm, char* out, size_t out_cap) {
  if (!out || out_cap == 0) return;
  int h = hour24;
  int m = minute;
  if (h < 0) h = 0;
  if (h > 23) h = 23;
  if (m < 0) m = 0;
  if (m > 59) m = 59;
  int h12 = cogito_timepicker_hour12_from_24(h);
  if (include_ampm) {
    snprintf(out, out_cap, "%02d:%02d %s", h12, m, h >= 12 ? "PM" : "AM");
  } else {
    snprintf(out, out_cap, "%02d:%02d", h12, m);
  }
}

// Get tertiary colors from Ensor scheme
static CogitoStyle cogito_timepicker_iconbtn_style(bool hover, bool active) {
  CogitoStyle s = cogito_theme_resolve(COGITO_ICONBTN);
  if (active) {
    s = cogito_style_merge(&s, &cogito_theme.per_kind_active[COGITO_ICONBTN]);
  } else if (hover) {
    s = cogito_style_merge(&s, &cogito_theme.per_kind_hover[COGITO_ICONBTN]);
  }
  return s;
}

static int cogito_timepicker_iconbtn_size(const CogitoStyle* s) {
  int min_w = (s && s->has_min_w) ? s->min_w : 48;
  int min_h = (s && s->has_min_h) ? s->min_h : 48;
  int size = min_w > min_h ? min_w : min_h;
  if (size < 24) size = 24;
  if (size > COGITO_TIMEPICKER_POP_FOOTER_H) size = COGITO_TIMEPICKER_POP_FOOTER_H;
  return size;
}

static int cogito_timepicker_iconbtn_icon_size(const CogitoStyle* s, int button_size) {
  int target = (s && s->has_icon_size) ? s->icon_size : 24;
  if (target < 8) target = 8;
  if (button_size > 0 && target > button_size) target = button_size;
  return target;
}

static void cogito_timepicker_ensure_time(CogitoNode* n) {
  if (!n || n->kind != COGITO_TIMEPICKER) return;
  if (n->timepicker.initialized) return;

  // Popover panels inherit current value from their owner.
  if (n->timepicker.popover_panel && n->timepicker.owner &&
      n->timepicker.owner->kind == COGITO_TIMEPICKER) {
    CogitoNode* owner = n->timepicker.owner;
    cogito_timepicker_ensure_time(owner);
    n->timepicker.hour = owner->timepicker.hour;
    n->timepicker.minute = owner->timepicker.minute;
    n->timepicker.ampm = owner->timepicker.ampm;
    n->timepicker.dial_visible = owner->timepicker.dial_visible;
    n->timepicker.mode = COGITO_TIMEPICKER_MODE_HOUR;
    n->timepicker.initialized = true;
    return;
  }

  time_t now = time(NULL);
  struct tm* lt = localtime(&now);
  if (lt) {
    n->timepicker.hour = lt->tm_hour;
    n->timepicker.minute = lt->tm_min;
  } else {
    n->timepicker.hour = 7;
    n->timepicker.minute = 0;
  }
  n->timepicker.ampm = (n->timepicker.hour >= 12);
  n->timepicker.dial_visible = true;
  n->timepicker.mode = COGITO_TIMEPICKER_MODE_HOUR;
  n->timepicker.initialized = true;
}

static void cogito_timepicker_panel_layout(const CogitoNode* n, CogitoTimePickerPanelLayout* out) {
  if (!n || !out) return;
  memset(out, 0, sizeof(*out));

  int panel_x = n->x;
  int panel_y = n->y;
  int panel_w = n->w;
  int panel_h = n->h;

  out->header_x = panel_x + COGITO_TIMEPICKER_POP_SIDE;
  out->header_y = panel_y + COGITO_TIMEPICKER_POP_TOP;
  out->header_w = panel_w - COGITO_TIMEPICKER_POP_SIDE * 2;
  out->header_h = COGITO_TIMEPICKER_POP_HEADER_H;

  out->hour_x = panel_x + COGITO_TIMEPICKER_POP_SIDE;
  out->hour_y = out->header_y + out->header_h;
  out->hour_w = COGITO_TIMEPICKER_POP_HOUR_W;
  out->hour_h = COGITO_TIMEPICKER_POP_TIME_H;

  out->minute_x = out->hour_x + out->hour_w + COGITO_TIMEPICKER_POP_HOUR_MIN_GAP;
  out->minute_y = out->hour_y;
  out->minute_w = COGITO_TIMEPICKER_POP_MIN_W;
  out->minute_h = COGITO_TIMEPICKER_POP_TIME_H;

  out->colon_x = out->hour_x + out->hour_w + (COGITO_TIMEPICKER_POP_HOUR_MIN_GAP / 2);
  out->colon_y = out->hour_y + out->hour_h / 2;

  out->ampm_x = out->minute_x + out->minute_w + COGITO_TIMEPICKER_POP_MIN_AMPM_GAP;
  out->ampm_y = out->hour_y;
  out->ampm_w = COGITO_TIMEPICKER_POP_AMPM_W;
  out->ampm_h = COGITO_TIMEPICKER_POP_TIME_H;
  out->ampm_split_h = out->ampm_h / 2;

  out->dial_d = COGITO_TIMEPICKER_POP_DIAL_SIZE;
  out->dial_x = panel_x + (panel_w - out->dial_d) / 2;
  out->dial_y = out->hour_y + out->hour_h + COGITO_TIMEPICKER_POP_TIME_TO_DIAL_GAP;
  out->dial_cx = out->dial_x + out->dial_d / 2;
  out->dial_cy = out->dial_y + out->dial_d / 2;
  out->dial_r = out->dial_d / 2;

  out->footer_x = panel_x + COGITO_TIMEPICKER_POP_SIDE;
  out->footer_w = panel_w - COGITO_TIMEPICKER_POP_SIDE * 2;
  out->footer_h = COGITO_TIMEPICKER_POP_FOOTER_H;
  out->footer_y = panel_y + panel_h - COGITO_TIMEPICKER_POP_BOTTOM_PAD - out->footer_h;

  CogitoStyle key_style = cogito_timepicker_iconbtn_style(false, false);
  int key_size = cogito_timepicker_iconbtn_size(&key_style);
  out->keyboard_w = key_size;
  out->keyboard_h = key_size;
  out->keyboard_x = out->footer_x;
  out->keyboard_y = out->footer_y + (out->footer_h - out->keyboard_h) / 2;

  out->ok_w = 40;
  out->ok_h = 32;
  out->ok_x = panel_x + panel_w - COGITO_TIMEPICKER_POP_SIDE - out->ok_w;
  out->ok_y = out->footer_y + (out->footer_h - out->ok_h) / 2;

  out->cancel_w = 72;
  out->cancel_h = 32;
  out->cancel_x = out->ok_x - 12 - out->cancel_w;
  out->cancel_y = out->footer_y + (out->footer_h - out->cancel_h) / 2;
}

static void cogito_timepicker_panel_value_point(const CogitoTimePickerPanelLayout* l, uint8_t mode, int value, int* out_x, int* out_y) {
  if (!l || !out_x || !out_y) return;
  const double kTau = 6.28318530717958647692;
  const double kHalfPi = 1.57079632679489661923;
  double turn = 0.0;
  if (mode == COGITO_TIMEPICKER_MODE_MINUTE) {
    int m = value % 60;
    if (m < 0) m += 60;
    turn = (double)m / 60.0;
  } else {
    int h = value % 12;
    if (h < 0) h += 12;
    turn = (double)h / 12.0;
  }
  double angle = turn * kTau - kHalfPi;
  int hand_r = l->dial_r - 30;
  *out_x = l->dial_cx + (int)lround(cos(angle) * (double)hand_r);
  *out_y = l->dial_cy + (int)lround(sin(angle) * (double)hand_r);
}

static double cogito_timepicker_panel_pointer_angle(int mx, int my, int cx, int cy) {
  const double kTau = 6.28318530717958647692;
  const double kHalfPi = 1.57079632679489661923;
  double angle = atan2((double)(my - cy), (double)(mx - cx)) + kHalfPi;
  while (angle < 0.0) angle += kTau;
  while (angle >= kTau) angle -= kTau;
  return angle;
}

static int cogito_timepicker_panel_angle_to_hour12(double angle) {
  const double kTau = 6.28318530717958647692;
  int slot = (int)lround((angle / kTau) * 12.0);
  if (slot <= 0) slot += 12;
  if (slot > 12) slot -= 12;
  return slot;
}

static int cogito_timepicker_panel_angle_to_minute(double angle) {
  const double kTau = 6.28318530717958647692;
  int slot = (int)lround((angle / kTau) * 60.0);
  slot %= 60;
  if (slot < 0) slot += 60;
  return slot;
}
