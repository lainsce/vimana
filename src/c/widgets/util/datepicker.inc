enum {
  COGITO_DATEPICKER_MODE_CALENDAR = 0,
  COGITO_DATEPICKER_MODE_MONTH_LIST = 1,
  COGITO_DATEPICKER_MODE_YEAR_LIST = 2,

  COGITO_DATEPICKER_FIELD_MIN_WIDTH = 220,
  COGITO_DATEPICKER_FIELD_HEIGHT = 56,
  COGITO_DATEPICKER_FIELD_BOX_HEIGHT = 56,

  COGITO_DATEPICKER_POPOVER_W = 360,
  COGITO_DATEPICKER_POPOVER_H = 460,
  COGITO_DATEPICKER_POP_SIDE = 12,
  COGITO_DATEPICKER_POP_TOP = 20,
  COGITO_DATEPICKER_POP_HEADER_H = 24,
  COGITO_DATEPICKER_POP_HEADER_TO_WEEK_GAP = 30,
  COGITO_DATEPICKER_POP_WEEK_H = 24,
  COGITO_DATEPICKER_POP_WEEK_TO_GRID_GAP = 16,
  COGITO_DATEPICKER_POP_CELL = 40,
  COGITO_DATEPICKER_POP_ROWS = 6,
  COGITO_DATEPICKER_POP_COLS = 7,
  COGITO_DATEPICKER_POP_FOOTER_GAP = 8,
  COGITO_DATEPICKER_POP_FOOTER_H = 36,
  COGITO_DATEPICKER_POP_BOTTOM = 12,
  COGITO_DATEPICKER_POP_NAV_BTN = 24,

  COGITO_DATEPICKER_SELECTOR_TOP_GAP = 20,
  COGITO_DATEPICKER_SELECTOR_ROW_H = 48,
  COGITO_DATEPICKER_SELECTOR_BOTTOM = 12,
  COGITO_DATEPICKER_SELECTOR_SCROLLBAR_W = 10,

  COGITO_DATEPICKER_YEAR_MIN = 1900,
  COGITO_DATEPICKER_YEAR_MAX = 2100
};

typedef struct CogitoDatePickerPanelLayout {
  int header_x, header_y, header_w, header_h;
  int month_prev_x, month_prev_y, month_prev_w, month_prev_h;
  int month_label_x, month_label_y, month_label_w, month_label_h;
  int month_next_x, month_next_y, month_next_w, month_next_h;
  int year_prev_x, year_prev_y, year_prev_w, year_prev_h;
  int year_label_x, year_label_y, year_label_w, year_label_h;
  int year_next_x, year_next_y, year_next_w, year_next_h;

  int week_x, week_y, week_w, week_h;
  int grid_x, grid_y, grid_w, grid_h, cell_w, cell_h;

  int selector_x, selector_y, selector_w, selector_h;
  int selector_row_h;
  int selector_scroll_x, selector_scroll_y, selector_scroll_w, selector_scroll_h;

  int footer_x, footer_y, footer_w, footer_h;
  int cancel_x, cancel_y, cancel_w, cancel_h;
  int ok_x, ok_y, ok_w, ok_h;
} CogitoDatePickerPanelLayout;

static const char* cogito_datepicker_month_short(int month) {
  static const char* names[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  if (month < 1 || month > 12) return "";
  return names[month - 1];
}

static const char* cogito_datepicker_month_long(int month) {
  static const char* names[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
  };
  if (month < 1 || month > 12) return "";
  return names[month - 1];
}

static bool cogito_is_leap_year(int y) {
  return ((y % 4) == 0 && (y % 100) != 0) || (y % 400) == 0;
}

static int cogito_days_in_month(int y, int m) {
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m < 1) m = 1;
  if (m > 12) m = 12;
  int d = days[m - 1];
  if (m == 2 && cogito_is_leap_year(y)) d = 29;
  return d;
}

static int cogito_weekday(int y, int m, int d) {
  struct tm t;
  memset(&t, 0, sizeof(t));
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  mktime(&t);
  return t.tm_wday;
}

static void cogito_datepicker_normalize_year_month(int* y, int* m) {
  if (!y || !m) return;
  while (*m < 1) {
    *m += 12;
    *y -= 1;
  }
  while (*m > 12) {
    *m -= 12;
    *y += 1;
  }
}

static void cogito_datepicker_today(int* out_year, int* out_month, int* out_day) {
  if (out_year) *out_year = 2025;
  if (out_month) *out_month = 1;
  if (out_day) *out_day = 1;
  time_t now = time(NULL);
  struct tm* lt = localtime(&now);
  if (!lt) return;
  if (out_year) *out_year = lt->tm_year + 1900;
  if (out_month) *out_month = lt->tm_mon + 1;
  if (out_day) *out_day = lt->tm_mday;
}

static bool cogito_datepicker_same_date(int y1, int m1, int d1, int y2, int m2, int d2) {
  return y1 == y2 && m1 == m2 && d1 == d2;
}

static uint8_t cogito_datepicker_mode(const CogitoNode* n) {
  if (!n || n->kind != COGITO_DATEPICKER) return COGITO_DATEPICKER_MODE_CALENDAR;
  uint8_t m = n->datepicker.mode;
  if (m > COGITO_DATEPICKER_MODE_YEAR_LIST) m = COGITO_DATEPICKER_MODE_CALENDAR;
  return m;
}

static int cogito_datepicker_selector_total(uint8_t mode) {
  if (mode == COGITO_DATEPICKER_MODE_MONTH_LIST) return 12;
  if (mode == COGITO_DATEPICKER_MODE_YEAR_LIST) return COGITO_DATEPICKER_YEAR_MAX - COGITO_DATEPICKER_YEAR_MIN + 1;
  return 0;
}

static int cogito_datepicker_selector_selected_index(const CogitoNode* n, uint8_t mode) {
  if (!n || n->kind != COGITO_DATEPICKER) return 0;
  if (mode == COGITO_DATEPICKER_MODE_MONTH_LIST) {
    int idx = n->datepicker.view_month - 1;
    if (idx < 0) idx = 0;
    if (idx > 11) idx = 11;
    return idx;
  }
  if (mode == COGITO_DATEPICKER_MODE_YEAR_LIST) {
    int idx = n->datepicker.view_year - COGITO_DATEPICKER_YEAR_MIN;
    int max_idx = cogito_datepicker_selector_total(mode) - 1;
    if (idx < 0) idx = 0;
    if (idx > max_idx) idx = max_idx;
    return idx;
  }
  return 0;
}

static void cogito_datepicker_selector_apply_index(CogitoNode* n, uint8_t mode, int idx) {
  if (!n || n->kind != COGITO_DATEPICKER) return;
  if (mode == COGITO_DATEPICKER_MODE_MONTH_LIST) {
    if (idx < 0) idx = 0;
    if (idx > 11) idx = 11;
    n->datepicker.view_month = idx + 1;
    cogito_datepicker_normalize_year_month(&n->datepicker.view_year, &n->datepicker.view_month);
    return;
  }
  if (mode == COGITO_DATEPICKER_MODE_YEAR_LIST) {
    int max_idx = cogito_datepicker_selector_total(mode) - 1;
    if (idx < 0) idx = 0;
    if (idx > max_idx) idx = max_idx;
    n->datepicker.view_year = COGITO_DATEPICKER_YEAR_MIN + idx;
    cogito_datepicker_normalize_year_month(&n->datepicker.view_year, &n->datepicker.view_month);
  }
}

static int cogito_datepicker_selector_visible_rows(const CogitoDatePickerPanelLayout* ly) {
  if (!ly) return 1;
  int row_h = ly->selector_row_h > 0 ? ly->selector_row_h : COGITO_DATEPICKER_SELECTOR_ROW_H;
  if (row_h < 1) row_h = 1;
  int rows = ly->selector_h / row_h;
  if (rows < 1) rows = 1;
  return rows;
}

static void cogito_datepicker_selector_clamp_start(CogitoNode* n, uint8_t mode, int visible_rows) {
  if (!n || n->kind != COGITO_DATEPICKER) return;
  int total = cogito_datepicker_selector_total(mode);
  if (visible_rows < 1) visible_rows = 1;
  int max_start = total - visible_rows;
  if (max_start < 0) max_start = 0;
  if (n->datepicker.list_start < 0) n->datepicker.list_start = 0;
  if (n->datepicker.list_start > max_start) n->datepicker.list_start = max_start;
}

static void cogito_datepicker_selector_center_start(CogitoNode* n, uint8_t mode, int visible_rows) {
  if (!n || n->kind != COGITO_DATEPICKER) return;
  int sel = cogito_datepicker_selector_selected_index(n, mode);
  int start = sel - visible_rows / 2;
  n->datepicker.list_start = start;
  cogito_datepicker_selector_clamp_start(n, mode, visible_rows);
}

static void cogito_datepicker_selector_set_mode(CogitoNode* n, uint8_t mode, const CogitoDatePickerPanelLayout* ly) {
  if (!n || n->kind != COGITO_DATEPICKER) return;
  if (mode > COGITO_DATEPICKER_MODE_YEAR_LIST) mode = COGITO_DATEPICKER_MODE_CALENDAR;
  n->datepicker.mode = mode;
  if (mode == COGITO_DATEPICKER_MODE_CALENDAR) {
    n->datepicker.list_start = 0;
    return;
  }
  int rows = cogito_datepicker_selector_visible_rows(ly);
  cogito_datepicker_selector_center_start(n, mode, rows);
}

static void cogito_datepicker_clamp_selected(CogitoNode* n) {
  if (!n || n->kind != COGITO_DATEPICKER) return;
  if (n->datepicker.month < 1) n->datepicker.month = 1;
  if (n->datepicker.month > 12) n->datepicker.month = 12;
  int dim = cogito_days_in_month(n->datepicker.year, n->datepicker.month);
  if (n->datepicker.day < 1) n->datepicker.day = 1;
  if (n->datepicker.day > dim) n->datepicker.day = dim;
}

static void cogito_datepicker_ensure_date(CogitoNode* n) {
  if (!n || n->kind != COGITO_DATEPICKER) return;

  if (n->datepicker.popover_panel && n->datepicker.owner &&
      n->datepicker.owner->kind == COGITO_DATEPICKER) {
    CogitoNode* owner = n->datepicker.owner;
    cogito_datepicker_ensure_date(owner);
    if (n->datepicker.year <= 0 || n->datepicker.month <= 0 || n->datepicker.day <= 0) {
      n->datepicker.year = owner->datepicker.year;
      n->datepicker.month = owner->datepicker.month;
      n->datepicker.day = owner->datepicker.day;
    }
    if (n->datepicker.view_year <= 0 || n->datepicker.view_month <= 0) {
      n->datepicker.view_year = owner->datepicker.year;
      n->datepicker.view_month = owner->datepicker.month;
    }
  } else if (n->datepicker.year <= 0 || n->datepicker.month <= 0 || n->datepicker.day <= 0) {
    cogito_datepicker_today(&n->datepicker.year, &n->datepicker.month, &n->datepicker.day);
  }

  cogito_datepicker_clamp_selected(n);

  if (n->datepicker.view_year <= 0 || n->datepicker.view_month <= 0) {
    n->datepicker.view_year = n->datepicker.year;
    n->datepicker.view_month = n->datepicker.month;
  }
  cogito_datepicker_normalize_year_month(&n->datepicker.view_year, &n->datepicker.view_month);

  if (n->datepicker.mode > COGITO_DATEPICKER_MODE_YEAR_LIST) {
    n->datepicker.mode = COGITO_DATEPICKER_MODE_CALENDAR;
  }
  if (n->datepicker.list_start < 0) n->datepicker.list_start = 0;
}

static void cogito_datepicker_set_selected(CogitoNode* n, int year, int month, int day) {
  if (!n || n->kind != COGITO_DATEPICKER) return;
  n->datepicker.year = year;
  n->datepicker.month = month;
  n->datepicker.day = day;
  cogito_datepicker_clamp_selected(n);
}

static void cogito_datepicker_shift_view_month(CogitoNode* n, int delta) {
  if (!n || n->kind != COGITO_DATEPICKER) return;
  cogito_datepicker_ensure_date(n);
  n->datepicker.view_month += delta;
  cogito_datepicker_normalize_year_month(&n->datepicker.view_year, &n->datepicker.view_month);
}

static void cogito_datepicker_shift_view_year(CogitoNode* n, int delta) {
  if (!n || n->kind != COGITO_DATEPICKER) return;
  cogito_datepicker_ensure_date(n);
  n->datepicker.view_year += delta;
  cogito_datepicker_normalize_year_month(&n->datepicker.view_year, &n->datepicker.view_month);
}

static void cogito_datepicker_format_display(int year, int month, int day, char* out, size_t out_cap) {
  if (!out || out_cap == 0) return;
  if (month < 1) month = 1;
  if (month > 12) month = 12;
  int dim = cogito_days_in_month(year, month);
  if (day < 1) day = 1;
  if (day > dim) day = dim;
  snprintf(out, out_cap, "%02d/%02d/%04d", month, day, year);
}

static void cogito_datepicker_field_box_rect(const CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!n) return;
  if (out_x) *out_x = n->x;
  if (out_y) *out_y = n->y;
  if (out_w) *out_w = n->w;
  int h = n->h;
  if (h > COGITO_DATEPICKER_FIELD_BOX_HEIGHT) h = COGITO_DATEPICKER_FIELD_BOX_HEIGHT;
  if (h < 0) h = 0;
  if (out_h) *out_h = h;
}

static void cogito_datepicker_panel_layout(const CogitoNode* n, CogitoDatePickerPanelLayout* out) {
  if (!n || !out) return;
  memset(out, 0, sizeof(*out));

  int panel_x = n->x;
  int panel_y = n->y;
  int panel_w = n->w;
  int panel_h = n->h;

  out->header_x = panel_x + COGITO_DATEPICKER_POP_SIDE;
  out->header_y = panel_y + COGITO_DATEPICKER_POP_TOP;
  out->header_w = panel_w - COGITO_DATEPICKER_POP_SIDE * 2;
  out->header_h = COGITO_DATEPICKER_POP_HEADER_H;

  int nav = COGITO_DATEPICKER_POP_NAV_BTN;
  int group_gap = 20;
  int group_w = (out->header_w - group_gap) / 2;
  int min_group_w = nav * 2 + 36;
  if (group_w < min_group_w) {
    group_w = min_group_w;
    group_gap = out->header_w - group_w * 2;
    if (group_gap < 8) group_gap = 8;
  }

  int month_group_x = out->header_x;
  int year_group_x = month_group_x + group_w + group_gap;

  out->month_prev_x = month_group_x;
  out->month_prev_y = out->header_y;
  out->month_prev_w = nav;
  out->month_prev_h = out->header_h;
  out->month_next_x = month_group_x + group_w - nav;
  out->month_next_y = out->header_y;
  out->month_next_w = nav;
  out->month_next_h = out->header_h;
  out->month_label_x = out->month_prev_x + nav;
  out->month_label_y = out->header_y;
  out->month_label_w = group_w - nav * 2;
  out->month_label_h = out->header_h;

  out->year_prev_x = year_group_x;
  out->year_prev_y = out->header_y;
  out->year_prev_w = nav;
  out->year_prev_h = out->header_h;
  out->year_next_x = year_group_x + group_w - nav;
  out->year_next_y = out->header_y;
  out->year_next_w = nav;
  out->year_next_h = out->header_h;
  out->year_label_x = out->year_prev_x + nav;
  out->year_label_y = out->header_y;
  out->year_label_w = group_w - nav * 2;
  out->year_label_h = out->header_h;

  out->week_w = COGITO_DATEPICKER_POP_COLS * COGITO_DATEPICKER_POP_CELL;
  out->week_h = COGITO_DATEPICKER_POP_WEEK_H;
  out->week_x = panel_x + (panel_w - out->week_w) / 2;
  out->week_y = out->header_y + out->header_h + COGITO_DATEPICKER_POP_HEADER_TO_WEEK_GAP;

  out->cell_w = COGITO_DATEPICKER_POP_CELL;
  out->cell_h = COGITO_DATEPICKER_POP_CELL;
  out->grid_w = COGITO_DATEPICKER_POP_COLS * out->cell_w;
  out->grid_h = COGITO_DATEPICKER_POP_ROWS * out->cell_h;
  out->grid_x = out->week_x;
  out->grid_y = out->week_y + out->week_h + COGITO_DATEPICKER_POP_WEEK_TO_GRID_GAP;

  out->selector_x = panel_x;
  out->selector_y = out->header_y + out->header_h + COGITO_DATEPICKER_SELECTOR_TOP_GAP;
  out->selector_w = panel_w;
  out->selector_h = panel_h - out->selector_y + panel_y - COGITO_DATEPICKER_SELECTOR_BOTTOM;
  if (out->selector_h < COGITO_DATEPICKER_SELECTOR_ROW_H) out->selector_h = COGITO_DATEPICKER_SELECTOR_ROW_H;
  out->selector_row_h = COGITO_DATEPICKER_SELECTOR_ROW_H;

  out->selector_scroll_w = COGITO_DATEPICKER_SELECTOR_SCROLLBAR_W;
  out->selector_scroll_h = out->selector_h - 16;
  if (out->selector_scroll_h < 20) out->selector_scroll_h = 20;
  out->selector_scroll_x = out->selector_x + out->selector_w - COGITO_DATEPICKER_POP_SIDE;
  out->selector_scroll_y = out->selector_y + (out->selector_h - out->selector_scroll_h) / 2;

  out->footer_x = panel_x + COGITO_DATEPICKER_POP_SIDE;
  out->footer_w = panel_w - COGITO_DATEPICKER_POP_SIDE * 2;
  out->footer_h = COGITO_DATEPICKER_POP_FOOTER_H;
  out->footer_y = panel_y + panel_h - COGITO_DATEPICKER_POP_BOTTOM - out->footer_h;

  out->ok_w = 44;
  out->ok_h = 32;
  out->ok_x = panel_x + panel_w - COGITO_DATEPICKER_POP_SIDE - out->ok_w;
  out->ok_y = out->footer_y + (out->footer_h - out->ok_h) / 2;

  out->cancel_w = 84;
  out->cancel_h = 32;
  out->cancel_x = out->ok_x - 16 - out->cancel_w;
  out->cancel_y = out->ok_y;
}

static void cogito_datepicker_panel_cell_date(const CogitoNode* n, int idx,
                                              int* out_year, int* out_month, int* out_day,
                                              bool* out_in_view) {
  if (!n || n->kind != COGITO_DATEPICKER) return;
  int vy = n->datepicker.view_year;
  int vm = n->datepicker.view_month;
  if (vy <= 0 || vm <= 0) {
    vy = n->datepicker.year;
    vm = n->datepicker.month;
  }
  cogito_datepicker_normalize_year_month(&vy, &vm);

  int first = cogito_weekday(vy, vm, 1);
  int dim = cogito_days_in_month(vy, vm);

  int py = vy;
  int pm = vm - 1;
  cogito_datepicker_normalize_year_month(&py, &pm);
  int prev_dim = cogito_days_in_month(py, pm);

  int ny = vy;
  int nm = vm + 1;
  cogito_datepicker_normalize_year_month(&ny, &nm);

  int y = vy;
  int m = vm;
  int d = 1;
  bool in_view = true;

  if (idx < first) {
    in_view = false;
    y = py;
    m = pm;
    d = prev_dim - first + idx + 1;
  } else if (idx >= first + dim) {
    in_view = false;
    y = ny;
    m = nm;
    d = idx - (first + dim) + 1;
  } else {
    d = idx - first + 1;
  }

  if (out_year) *out_year = y;
  if (out_month) *out_month = m;
  if (out_day) *out_day = d;
  if (out_in_view) *out_in_view = in_view;
}

static bool cogito_datepicker_hit_rect_min48_local(int px, int py, int rx, int ry, int rw, int rh) {
  int hw = rw < 48 ? 48 : rw;
  int hh = rh < 48 ? 48 : rh;
  int hx = rx - (hw - rw) / 2;
  int hy = ry - (hh - rh) / 2;
  return cogito_hit_rect(px, py, hx, hy, hw, hh);
}

static bool cogito_datepicker_panel_hit_day(CogitoNode* n, int mx, int my,
                                            int* out_year, int* out_month, int* out_day,
                                            bool* out_in_view) {
  if (!n || n->kind != COGITO_DATEPICKER) return false;
  CogitoDatePickerPanelLayout ly;
  cogito_datepicker_panel_layout(n, &ly);
  int best_idx = -1;
  int best_dist2 = INT_MAX;
  for (int row = 0; row < COGITO_DATEPICKER_POP_ROWS; row++) {
    for (int col = 0; col < COGITO_DATEPICKER_POP_COLS; col++) {
      int cell_x = ly.grid_x + col * ly.cell_w;
      int cell_y = ly.grid_y + row * ly.cell_h;
      if (!cogito_datepicker_hit_rect_min48_local(mx, my, cell_x, cell_y, ly.cell_w, ly.cell_h)) continue;
      int cx = cell_x + ly.cell_w / 2;
      int cy = cell_y + ly.cell_h / 2;
      int dx = mx - cx;
      int dy = my - cy;
      int dist2 = dx * dx + dy * dy;
      if (dist2 < best_dist2) {
        best_dist2 = dist2;
        best_idx = row * COGITO_DATEPICKER_POP_COLS + col;
      }
    }
  }
  if (best_idx < 0) return false;
  cogito_datepicker_panel_cell_date(n, best_idx, out_year, out_month, out_day, out_in_view);
  return true;
}

static bool cogito_datepicker_panel_selector_scrollbar_rect(CogitoNode* n,
                                                            int* out_track_x, int* out_track_y,
                                                            int* out_track_w, int* out_track_h,
                                                            int* out_thumb_x, int* out_thumb_y,
                                                            int* out_thumb_w, int* out_thumb_h);

static bool cogito_datepicker_panel_hit_selector_item(CogitoNode* n, int mx, int my, int* out_index) {
  if (!n || n->kind != COGITO_DATEPICKER) return false;
  uint8_t mode = cogito_datepicker_mode(n);
  if (mode == COGITO_DATEPICKER_MODE_CALENDAR) return false;

  CogitoDatePickerPanelLayout ly;
  cogito_datepicker_panel_layout(n, &ly);
  int hit_x = ly.selector_x;
  int hit_w = ly.selector_w;

  int tx = 0, ty = 0, tw = 0, th = 0, ux = 0, uy = 0, uw = 0, uh = 0;
  bool has_scrollbar = cogito_datepicker_panel_selector_scrollbar_rect(n, &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh);
  if (has_scrollbar) {
    hit_w = tx - hit_x - 4;  // Reserve a lane on the right for scrollbar hit-testing.
    if (hit_w < 0) hit_w = 0;
  }

  int rows = cogito_datepicker_selector_visible_rows(&ly);
  int total = cogito_datepicker_selector_total(mode);
  if (total <= 0) return false;

  cogito_datepicker_selector_clamp_start(n, mode, rows);

  int row = -1;
  int best_dist2 = INT_MAX;
  for (int r = 0; r < rows; r++) {
    int ry = ly.selector_y + r * ly.selector_row_h;
    if (!cogito_datepicker_hit_rect_min48_local(mx, my, hit_x, ry, hit_w, ly.selector_row_h)) continue;
    int cx = hit_x + hit_w / 2;
    int cy = ry + ly.selector_row_h / 2;
    int dx = mx - cx;
    int dy = my - cy;
    int dist2 = dx * dx + dy * dy;
    if (dist2 < best_dist2) {
      best_dist2 = dist2;
      row = r;
    }
  }
  if (row < 0 || row >= rows) return false;

  int idx = n->datepicker.list_start + row;
  if (idx < 0 || idx >= total) return false;
  if (out_index) *out_index = idx;
  return true;
}

static bool cogito_datepicker_panel_selector_scrollbar_rect(CogitoNode* n,
                                                            int* out_track_x, int* out_track_y,
                                                            int* out_track_w, int* out_track_h,
                                                            int* out_thumb_x, int* out_thumb_y,
                                                            int* out_thumb_w, int* out_thumb_h) {
  if (!n || n->kind != COGITO_DATEPICKER) return false;
  uint8_t mode = cogito_datepicker_mode(n);
  if (mode == COGITO_DATEPICKER_MODE_CALENDAR) return false;

  CogitoDatePickerPanelLayout ly;
  cogito_datepicker_panel_layout(n, &ly);
  int rows = cogito_datepicker_selector_visible_rows(&ly);
  int total = cogito_datepicker_selector_total(mode);
  if (total <= rows) return false;

  cogito_datepicker_selector_clamp_start(n, mode, rows);

  int tx = ly.selector_scroll_x;
  int ty = ly.selector_scroll_y;
  int tw = ly.selector_scroll_w;
  int th = ly.selector_scroll_h;

  int thumb_h = (int)lround((double)th * (double)rows / (double)total);
  if (thumb_h < 24) thumb_h = 24;
  if (thumb_h > th) thumb_h = th;

  int max_start = total - rows;
  int travel = th - thumb_h;
  int thumb_y = ty;
  if (travel > 0 && max_start > 0) {
    thumb_y = ty + (int)lround((double)travel * (double)n->datepicker.list_start / (double)max_start);
  }

  if (out_track_x) *out_track_x = tx;
  if (out_track_y) *out_track_y = ty;
  if (out_track_w) *out_track_w = tw;
  if (out_track_h) *out_track_h = th;
  if (out_thumb_x) *out_thumb_x = tx;
  if (out_thumb_y) *out_thumb_y = thumb_y;
  if (out_thumb_w) *out_thumb_w = tw;
  if (out_thumb_h) *out_thumb_h = thumb_h;
  return true;
}

static bool cogito_datepicker_panel_selector_set_start_from_track(CogitoNode* n, int my) {
  if (!n || n->kind != COGITO_DATEPICKER) return false;
  uint8_t mode = cogito_datepicker_mode(n);
  if (mode == COGITO_DATEPICKER_MODE_CALENDAR) return false;

  CogitoDatePickerPanelLayout ly;
  cogito_datepicker_panel_layout(n, &ly);
  int rows = cogito_datepicker_selector_visible_rows(&ly);
  int total = cogito_datepicker_selector_total(mode);
  if (total <= rows) return false;

  int tx = 0, ty = 0, tw = 0, th = 0, ux = 0, uy = 0, uw = 0, uh = 0;
  if (!cogito_datepicker_panel_selector_scrollbar_rect(n, &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh)) return false;

  int thumb_center = my - ty - uh / 2;
  if (thumb_center < 0) thumb_center = 0;
  int travel = th - uh;
  if (travel < 1) travel = 1;
  if (thumb_center > travel) thumb_center = travel;

  int max_start = total - rows;
  int new_start = (int)lround((double)thumb_center * (double)max_start / (double)travel);
  n->datepicker.list_start = new_start;
  cogito_datepicker_selector_clamp_start(n, mode, rows);
  return true;
}
