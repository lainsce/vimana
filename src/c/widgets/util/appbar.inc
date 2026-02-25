// Appbar widget utility functions
// Metrics, positions, controls layout, and navigation helpers.

static void cogito_appbar_button_metrics(int* size, int* gap, int* pad_top, int* pad_right) {
  CogitoStyle s = cogito_theme.appbar_win_btn_all;
  if (size) {
    if (s.has_min_w) *size = s.min_w;
    else if (s.has_min_h) *size = s.min_h;
    else *size = 12;
  }
  if (gap) *gap = 9;
  if (pad_top) *pad_top = 9;
  if (pad_right) *pad_right = 9;
}

static bool cogito_tree_has_visible_bottom_sheet_above_y(const CogitoNode* n, int y_limit) {
  if (!n) return false;
  if (n->kind == COGITO_BOTTOM_SHEET && n->w > 0 && n->h > 0 && n->y <= y_limit) return true;
  for (size_t i = 0; i < n->len; i++) {
    if (cogito_tree_has_visible_bottom_sheet_above_y(n->children[i], y_limit)) return true;
  }
  return false;
}

enum { COGITO_WINDOW_DRAG_HIT_PAD = 8 };

static bool cogito_appbar_bottom_sheet_active(const CogitoNode* appbar) {
  if (!appbar) return false;
  CogitoNode* win = cogito_node_window((CogitoNode*)appbar);
  if (!win) return false;
  int appbar_bottom = appbar->y + appbar->h;
  return cogito_tree_has_visible_bottom_sheet_above_y(win, appbar_bottom);
}

static int cogito_appbar_titlebar_inset(void) {
  int btn_size = 12;
  int btn_gap = 9;
  int btn_pad_top = 9;
  int btn_pad_right = 9;
  cogito_appbar_button_metrics(&btn_size, &btn_gap, &btn_pad_top, &btn_pad_right);
  if (btn_size < 2) btn_size = 2;
  if (btn_pad_top < 0) btn_pad_top = 0;
  int inset = btn_pad_top + btn_size + 6;
  if (inset <= 0) inset = COGITO_BOTTOM_SHEET_TOP_INSET;
  return inset;
}

static int cogito_appbar_ctrl_index(char c) {
  if (c == 'C') return 0;
  if (c == 'I') return 1;
  if (c == 'M') return 2;
  return -1;
}

static void cogito_appbar_controls_positions(CogitoNode* n, int x, int w, int btn_size, int gap, int pad, bool allow_max, int* out_close, int* out_min, int* out_max) {
  int pos[3] = {-1, -1, -1};
  bool seen[3] = {false, false, false};
  const char* s = n->appbar.controls[0] ? n->appbar.controls : "CIM|";
  const char* split = strchr(s, '|');
  size_t left_len = split ? (size_t)(split - s) : strlen(s);
  const char* right = split ? split + 1 : "";
  size_t right_len = strlen(right);

  int lx = x + pad;
  for (size_t i = 0; i < left_len; i++) {
    int idx = cogito_appbar_ctrl_index(s[i]);
    if (!allow_max && idx == 2) continue;
    if (idx < 0 || seen[idx]) continue;
    pos[idx] = lx;
    seen[idx] = true;
    lx += btn_size + gap;
  }

  int rx = x + w - pad - btn_size;
  for (size_t i = 0; i < right_len; i++) {
    char c = right[right_len - 1 - i];
    int idx = cogito_appbar_ctrl_index(c);
    if (!allow_max && idx == 2) continue;
    if (idx < 0 || seen[idx]) continue;
    pos[idx] = rx;
    seen[idx] = true;
    rx -= btn_size + gap;
  }

  if (out_close) *out_close = pos[0];
  if (out_min) *out_min = pos[1];
  if (out_max) *out_max = pos[2];
}

// Helper functions for multi-appbar support
static CogitoNode* cogito_find_appbar_recursive(CogitoNode* n) {
  if (!n) return NULL;
  if (n->kind == COGITO_APPBAR) return n;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* found = cogito_find_appbar_recursive(n->children[i]);
    if (found) return found;
  }
  return NULL;
}

static CogitoNode* cogito_find_appbar(CogitoNode* win) {
  if (!win) return NULL;
  return cogito_find_appbar_recursive(win);
}

static CogitoNode* cogito_find_appbar_at_point_recursive(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_APPBAR &&
      cogito_hit_rect(x, y,
                      n->x - COGITO_WINDOW_DRAG_HIT_PAD,
                      n->y - COGITO_WINDOW_DRAG_HIT_PAD,
                      n->w + COGITO_WINDOW_DRAG_HIT_PAD * 2,
                      n->h + COGITO_WINDOW_DRAG_HIT_PAD * 2)) {
    return n;
  }
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* hit = cogito_find_appbar_at_point_recursive(n->children[i], x, y);
    if (hit) return hit;
  }
  return NULL;
}

static CogitoNode* cogito_find_appbar_at_point(CogitoNode* win, int x, int y) {
  if (!win) return NULL;
  return cogito_find_appbar_at_point_recursive(win, x, y);
}

static bool cogito_is_first_appbar(CogitoNode* win, CogitoNode* appbar) {
  if (!win || !appbar) return false;
  CogitoNode* first = cogito_find_appbar(win);
  return first == appbar;
}

static bool cogito_hit_rect_min48_local(int px, int py, int rx, int ry, int rw, int rh) {
  int hw = rw < 48 ? 48 : rw;
  int hh = rh < 48 ? 48 : rh;
  int hx = rx - (hw - rw) / 2;
  int hy = ry - (hh - rh) / 2;
  return cogito_hit_rect(px, py, hx, hy, hw, hh);
}

static bool cogito_point_over_appbar_buttons(CogitoNode* app, int x, int y) {
  if (!app) return false;
  int btn_hit_pad = 1; // element_size + 2px target (1px on each side)
  if (app->appbar.btn_close_x >= 0 &&
      cogito_hit_rect(x, y,
                      app->appbar.btn_close_x - btn_hit_pad,
                      app->appbar.btn_y - btn_hit_pad,
                      app->appbar.btn_size + 2,
                      app->appbar.btn_size + 2)) return true;
  if (app->appbar.btn_min_x >= 0 &&
      cogito_hit_rect(x, y,
                      app->appbar.btn_min_x - btn_hit_pad,
                      app->appbar.btn_y - btn_hit_pad,
                      app->appbar.btn_size + 2,
                      app->appbar.btn_size + 2)) return true;
  if (app->appbar.btn_max_x >= 0 &&
      cogito_hit_rect(x, y,
                      app->appbar.btn_max_x - btn_hit_pad,
                      app->appbar.btn_y - btn_hit_pad,
                      app->appbar.btn_size + 2,
                      app->appbar.btn_size + 2)) return true;
  for (size_t i = 0; i < app->len; i++) {
    CogitoNode* c = app->children[i];
    if (cogito_hit_rect_min48_local(x, y, c->x, c->y, c->w, c->h)) return true;
  }
  return false;
}

static bool cogito_appbar_is_title_widget(const CogitoNode* c) {
  if (!c || c->kind != COGITO_VIEW_CHOOSER) return false;
  return c->class_name && c->class_name->data &&
         strcmp(c->class_name->data, "appbar-title") == 0;
}

static CogitoNode* cogito_appbar_title_widget(CogitoNode* appbar) {
  if (!appbar) return NULL;
  for (size_t i = 0; i < appbar->len; i++) {
    CogitoNode* c = appbar->children[i];
    if (cogito_appbar_is_title_widget(c)) return c;
  }
  return NULL;
}

static int cogito_label_class_font_size(int cls, int fallback);
static int cogito_text_height_size(int size);
static void cogito_intrinsic_size(CogitoNode* n, int* out_w, int* out_h);

static void cogito_appbar_row_positions(
  CogitoNode* n,
  int* out_buttons_y,
  int* out_title_y,
  int* out_subtitle_y,
  int* out_child_y,
  int* out_appbar_h
) {
  int btn_size = 12;
  int btn_gap = 6;
  int btn_pad_top = 9;
  int btn_pad_right = 9;
  cogito_appbar_button_metrics(&btn_size, &btn_gap, &btn_pad_top, &btn_pad_right);

  CogitoNode* title_widget = cogito_appbar_title_widget(n);
  const int has_title_widget = title_widget != NULL;
  const int has_title_text = (n->text && n->text->len);
  const int has_title = has_title_widget || has_title_text;
  const int has_subtitle = (n->subtitle && n->subtitle->len);

  int title_h = 0;
  int subtitle_h = 0;

  if (has_title_widget) {
    int tw = 0;
    cogito_intrinsic_size(title_widget, &tw, &title_h);
    if (title_h <= 0) title_h = btn_size;
  } else if (has_title_text) {
    int tsz = cogito_label_class_font_size(COGITO_LABEL_CLASS_TITLE, 24);
    title_h = cogito_text_height_size(tsz);
  }
  if (has_subtitle) {
    int ssz = cogito_label_class_font_size(COGITO_LABEL_CLASS_SUBTITLE, 18);
    subtitle_h = cogito_text_height_size(ssz);
  }

  int icon_row_h = 0;
  int has_children = 0;
  if (n->len > 0) {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* c = n->children[i];
      if (title_widget && c == title_widget) continue;
      has_children = 1;
      int cw = 0;
      int ch = 0;
      cogito_intrinsic_size(c, &cw, &ch);
      if (ch > icon_row_h) icon_row_h = ch;
    }
    if (icon_row_h == 0) icon_row_h = btn_size;
  }

  const int y_buttons = n->y + btn_pad_top;

  // Baseline where text rows start (same as your previous y1)
  int cursor_y = y_buttons + btn_size + 18;

  int y_title = 0;
  int y_subtitle = 0;

  if (has_title) {
    y_title = cursor_y;
    cursor_y += title_h + 12;
  }

  if (has_subtitle) {
    y_subtitle = cursor_y;
    cursor_y += subtitle_h + 12;
  }

  int y_child = 0;
  if (has_children) {
    y_child = cursor_y;
    cursor_y += icon_row_h + 12; // bottom padding for the child row
  }

  const int h = cursor_y - n->y;

  if (out_buttons_y)  *out_buttons_y  = y_buttons;
  if (out_title_y)    *out_title_y    = y_title ? y_title : 0;     // 0 if no title
  if (out_subtitle_y) *out_subtitle_y = y_subtitle ? y_subtitle : 0;  // 0 if no subtitle
  if (out_child_y)    *out_child_y    = y_child ? y_child : 0;     // 0 if no children
  if (out_appbar_h)   *out_appbar_h   = h;
}

static CogitoStyle cogito_appbar_win_btn_style(int kind, int state) {
  // Optimized: use in-place merging to avoid struct copies
  CogitoStyle s;
  cogito_style_merge_into(&cogito_theme.appbar_win_btn_all, &cogito_theme.appbar_win_btn[kind], &s);
  if (state == 1) {
    cogito_style_merge_into(&s, &cogito_theme.appbar_win_btn_hover_all, &s);
    cogito_style_merge_into(&s, &cogito_theme.appbar_win_btn_hover[kind], &s);
  } else if (state == 2) {
    cogito_style_merge_into(&s, &cogito_theme.appbar_win_btn_active_all, &s);
    cogito_style_merge_into(&s, &cogito_theme.appbar_win_btn_active[kind], &s);
  }
  return s;
}
