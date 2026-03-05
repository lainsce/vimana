// Appbar widget utility functions
// Metrics, positions, controls layout, and navigation helpers.

static void cogito_appbar_button_metrics(int* size, int* gap, int* pad_top, int* pad_right) {
  CogitoStyle s = cogito_theme.appbar_win_btn_all;
  if (size) {
    if (s.has_min_w) *size = s.min_w;
    else if (s.has_min_h) *size = s.min_h;
    else *size = 12;
  }
  if (gap) *gap = 10;
  if (pad_top) *pad_top = 14;
  if (pad_right) *pad_right = 14;
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
  int btn_gap = 10;
  int btn_pad_top = 14;
  int btn_pad_right = 14;
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
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = NULL;
    if (n->len > 0) {
      if (n->view_active_id) {
        for (size_t i = 0; i < n->len; i++) {
          CogitoNode* c = n->children[i];
          if (c && c->view_id && c->view_id->data && n->view_active_id->data &&
              strcmp(c->view_id->data, n->view_active_id->data) == 0) {
            active = c;
            break;
          }
        }
      }
      if (!active) active = n->children[0];
    }
    if (!active) return NULL;
    return cogito_find_appbar_recursive(active);
  }
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

static bool cogito_appbar_point_visible(const CogitoNode* appbar, int x, int y) {
  if (!appbar) return false;
  if (!cogito_hit_rect(x, y, appbar->x, appbar->y, appbar->w, appbar->h)) return false;
  const CogitoNode* p = appbar->parent;
  while (p) {
    if (p->kind == COGITO_SCROLLER || p->kind == COGITO_DIALOG ||
        p->kind == COGITO_CONTENT_LIST || p->kind == COGITO_GRID) {
      if (!cogito_hit_rect(x, y, p->x, p->y, p->w, p->h)) return false;
    }
    p = p->parent;
  }
  return true;
}

static CogitoNode* cogito_find_appbar_at_point_recursive(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_DIALOG && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  if ((n->kind == COGITO_SCROLLER || n->kind == COGITO_CONTENT_LIST || n->kind == COGITO_GRID) &&
      !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = NULL;
    if (n->len > 0) {
      if (n->view_active_id) {
        for (size_t i = 0; i < n->len; i++) {
          CogitoNode* c = n->children[i];
          if (c && c->view_id && c->view_id->data && n->view_active_id->data &&
              strcmp(c->view_id->data, n->view_active_id->data) == 0) {
            active = c;
            break;
          }
        }
      }
      if (!active) active = n->children[0];
    }
    if (!active) return NULL;
    return cogito_find_appbar_at_point_recursive(active, x, y);
  }
  if (n->kind == COGITO_APPBAR &&
      n->w > 0 && n->h > 0 &&
      cogito_appbar_point_visible(n, x, y) &&
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
  if (!c || !c->class_name || !c->class_name->data) return false;
  return cogito_class_has_token(c->class_name->data, "appbar-title");
}

static int cogito_label_class_font_size(int cls, int fallback);
static int cogito_text_height_size(int size);
static void cogito_intrinsic_size(CogitoNode* n, int* out_w, int* out_h);

enum {
  COGITO_APPBAR_TYPE_SEARCH = 0,
  COGITO_APPBAR_TYPE_SMALL = 1,
  COGITO_APPBAR_TYPE_MEDIUM = 2,
  COGITO_APPBAR_TYPE_LARGE = 3
};

static int cogito_appbar_type(const CogitoNode* n) {
  const char* cls = (n && n->class_name && n->class_name->data) ? n->class_name->data : NULL;
  if (!cls || !cls[0]) return COGITO_APPBAR_TYPE_MEDIUM;
  if (cogito_class_has_token(cls, "search")) return COGITO_APPBAR_TYPE_SEARCH;
  if (cogito_class_has_token(cls, "small")) return COGITO_APPBAR_TYPE_SMALL;
  if (cogito_class_has_token(cls, "large")) return COGITO_APPBAR_TYPE_LARGE;
  return COGITO_APPBAR_TYPE_MEDIUM;
}

static int cogito_appbar_title_font_size_for_type(int appbar_type) {
  if (appbar_type == COGITO_APPBAR_TYPE_SMALL) return cogito_label_class_font_size(COGITO_LABEL_CLASS_TITLE, 20);
  if (appbar_type == COGITO_APPBAR_TYPE_LARGE) return cogito_label_class_font_size(COGITO_LABEL_CLASS_DISPLAY, 36);
  return cogito_label_class_font_size(COGITO_LABEL_CLASS_TITLE, 24);
}

static int cogito_appbar_subtitle_font_size_for_type(int appbar_type) {
  if (appbar_type == COGITO_APPBAR_TYPE_SMALL) return cogito_label_class_font_size(COGITO_LABEL_CLASS_SUBTITLE, 14);
  if (appbar_type == COGITO_APPBAR_TYPE_LARGE) return cogito_label_class_font_size(COGITO_LABEL_CLASS_SUBTITLE, 20);
  return cogito_label_class_font_size(COGITO_LABEL_CLASS_SUBTITLE, 18);
}

static CogitoNode* cogito_appbar_title_widget(CogitoNode* appbar) {
  if (!appbar) return NULL;
  for (size_t i = 0; i < appbar->len; i++) {
    CogitoNode* c = appbar->children[i];
    if (cogito_appbar_is_title_widget(c)) return c;
  }
  return NULL;
}

static bool cogito_appbar_is_leading_child(const CogitoNode* c) {
  if (!c || !c->class_name || !c->class_name->data) return false;
  return cogito_class_has_token(c->class_name->data, "appbar-leading");
}

static bool cogito_appbar_is_space_char(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool cogito_appbar_is_surface_token(const char* token) {
  if (!token || !token[0]) return false;
  return strcmp(token, "surface-container-lowest") == 0 ||
         strcmp(token, "surface-container-low") == 0 ||
         strcmp(token, "surface-container") == 0 ||
         strcmp(token, "surface-container-high") == 0 ||
         strcmp(token, "surface-container-highest") == 0;
}

static const char* cogito_appbar_surface_token_for_level(int level) {
  if (level <= 0) return "surface-container-lowest";
  if (level == 1) return "surface-container-low";
  if (level == 2) return "surface-container";
  if (level == 3) return "surface-container-high";
  return "surface-container-highest";
}

static void cogito_appbar_apply_paired_surface(CogitoNode* appbar) {
  if (!appbar || appbar->kind != COGITO_APPBAR) return;

  if (!appbar->appbar.paired_scroller) {
    const char* app_cls = (appbar->class_name && appbar->class_name->data) ? appbar->class_name->data : "";
    if (cogito_class_has_token(app_cls, "sidebar") && appbar->parent) {
      for (size_t i = 0; i < appbar->parent->len; i++) {
        CogitoNode* sib = appbar->parent->children[i];
        if (!sib || sib == appbar || sib->kind != COGITO_SCROLLER) continue;
        const char* sc_cls = (sib->class_name && sib->class_name->data) ? sib->class_name->data : "";
        if (!cogito_class_has_token(sc_cls, "sidebar")) continue;
        appbar->appbar.paired_scroller = sib;
        int sc_level = 3;
        if (cogito_class_has_token(sc_cls, "surface-container-lowest")) sc_level = 0;
        else if (cogito_class_has_token(sc_cls, "surface-container-low")) sc_level = 1;
        else if (cogito_class_has_token(sc_cls, "surface-container-highest")) sc_level = 4;
        else if (cogito_class_has_token(sc_cls, "surface-container-high")) sc_level = 3;
        else if (cogito_class_has_token(sc_cls, "surface-container")) sc_level = 2;
        int target_level = sc_level + 1;
        if (target_level < 3) target_level = 3;
        if (target_level > 4) target_level = 4;
        appbar->appbar.paired_surface_level = target_level;
        if (appbar->appbar.pair_base_class[0] == 0) {
          strncpy(appbar->appbar.pair_base_class, app_cls, sizeof(appbar->appbar.pair_base_class) - 1);
          appbar->appbar.pair_base_class[sizeof(appbar->appbar.pair_base_class) - 1] = 0;
        }
        break;
      }
    }
  }

  if (!appbar->appbar.paired_scroller || appbar->appbar.pair_base_class[0] == 0) return;

  CogitoNode* sc = appbar->appbar.paired_scroller;
  bool elevate = (sc->kind == COGITO_SCROLLER && sc->len > 0 && sc->scroller.scroll_y > 0);
  const char* base = appbar->appbar.pair_base_class;

  char desired[256];
  desired[0] = 0;
  if (!elevate) {
    strncpy(desired, base, sizeof(desired) - 1);
    desired[sizeof(desired) - 1] = 0;
  } else {
    const char* p = base;
    size_t out = 0;
    while (*p) {
      while (*p && cogito_appbar_is_space_char(*p)) p++;
      if (!*p) break;
      const char* start = p;
      while (*p && !cogito_appbar_is_space_char(*p)) p++;
      size_t len = (size_t)(p - start);
      if (len == 0) continue;
      char tok[64];
      size_t copy = len < sizeof(tok) - 1 ? len : (sizeof(tok) - 1);
      memcpy(tok, start, copy);
      tok[copy] = 0;
      if (cogito_appbar_is_surface_token(tok)) continue;
      if (out > 0 && out + 1 < sizeof(desired)) desired[out++] = ' ';
      if (out + copy >= sizeof(desired)) break;
      memcpy(desired + out, tok, copy);
      out += copy;
      desired[out] = 0;
    }

    const char* surface_tok = cogito_appbar_surface_token_for_level(appbar->appbar.paired_surface_level);
    size_t slen = strlen(surface_tok);
    if (out > 0 && out + 1 < sizeof(desired)) desired[out++] = ' ';
    if (out + slen < sizeof(desired)) {
      memcpy(desired + out, surface_tok, slen);
      out += slen;
      desired[out] = 0;
    }
  }

  const char* cur = (appbar->class_name && appbar->class_name->data) ? appbar->class_name->data : "";
  if (strcmp(cur, desired) == 0) return;
  YisStr* ns = stdr_str_lit(desired);
  if (appbar->class_name) yis_release_val(YV_STR(appbar->class_name));
  appbar->class_name = ns;
}

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
  int appbar_type = cogito_appbar_type(n);

  CogitoNode* title_widget = cogito_appbar_title_widget(n);
  const int has_title_widget = title_widget != NULL;
  const int has_title_text = (n->text && n->text->len);
  const int has_title = has_title_widget || has_title_text;
  const int has_subtitle = (n->subtitle && n->subtitle->len);

  int title_h = 0;
  int subtitle_h = 0;
  int title_size = cogito_appbar_title_font_size_for_type(appbar_type);
  int subtitle_size = cogito_appbar_subtitle_font_size_for_type(appbar_type);

  if (has_title_widget) {
    int tw = 0;
    cogito_intrinsic_size(title_widget, &tw, &title_h);
    if (title_h <= 0) title_h = btn_size;
  } else if (has_title_text) {
    title_h = cogito_text_height_size(title_size);
  }
  if (has_subtitle) {
    subtitle_h = cogito_text_height_size(subtitle_size);
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

  if (appbar_type == COGITO_APPBAR_TYPE_SEARCH) {
    int appbar_h = 104;
    int y_buttons_center = y_buttons;
    int y_title = 0;
    if (has_title_widget) {
      y_title = n->y + appbar_h - title_h - 12;
    }
    int y_child = 0;
    if (has_children) {
      y_child = n->y + appbar_h - icon_row_h - 12;
    }
    if (out_buttons_y) *out_buttons_y = y_buttons_center;
    if (out_title_y) *out_title_y = y_title;
    if (out_subtitle_y) *out_subtitle_y = 0;
    if (out_child_y) *out_child_y = y_child;
    if (out_appbar_h) *out_appbar_h = appbar_h;
    return;
  }

  int title_gap = 12;
  int subtitle_gap = 12;
  int baseline_gap = 18;
  int bottom_pad = 12;
  int min_h = 64;
  if (appbar_type == COGITO_APPBAR_TYPE_SMALL) {
    title_gap = 8;
    subtitle_gap = 8;
    baseline_gap = 10;
    min_h = 104;
  } else if (appbar_type == COGITO_APPBAR_TYPE_LARGE) {
    title_gap = 14;
    subtitle_gap = 16;
    baseline_gap = 24;
    bottom_pad = 12;
    min_h = 104;
  }

  // Baseline where text rows start (same as your previous y1)
  int cursor_y = y_buttons + btn_size + baseline_gap;

  int y_title = 0;
  int y_subtitle = 0;

  if (has_title) {
    y_title = cursor_y;
    cursor_y += title_h + title_gap;
  }

  if (has_subtitle) {
    y_subtitle = cursor_y;
    cursor_y += subtitle_h + subtitle_gap;
  }

  int y_child = 0;
  if (has_children) {
    if (appbar_type == COGITO_APPBAR_TYPE_SMALL) {
      if (has_title) {
        y_child = y_title + (title_h - icon_row_h) / 2;
      } else {
        y_child = y_buttons + (btn_size - icon_row_h) / 2;
      }
    } else {
      y_child = cursor_y;
      cursor_y += icon_row_h;
    }
  }

  int h = (cursor_y - n->y) + bottom_pad;
  if (h < min_h) h = min_h;

  if (appbar_type == COGITO_APPBAR_TYPE_SMALL) {
    if (has_children) {
      y_child = n->y + h - icon_row_h - 12;
    }
    if (has_subtitle) {
      y_subtitle = n->y + h - subtitle_h - 12;
      if (has_title) {
        y_title = y_subtitle - title_h - 6;
      }
    } else if (has_title) {
      if (has_children) {
        y_title = y_child + (icon_row_h - title_h) / 2;
      } else {
        y_title = n->y + h - title_h - 12;
      }
    }
  }

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
  cogito_apply_bin_scope_to_style(&s);
  return s;
}
