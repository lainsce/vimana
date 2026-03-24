static int cogito_inspector_hit_test(CogitoWindow* win, int x, int y, void* user) {
  (void)win;
  enum { COGITO_WINDOW_DRAG_HIT_PAD = 8 };
  const int* params = (const int*)user;
  int appbar_h = (params && params[0] > 0) ? params[0] : 52;
  int margin = (params && params[1] > 0) ? params[1] : 0;
  int btn = (params && params[2] > 0) ? params[2] : 14;
  int gap = (params && params[3] > 0) ? params[3] : 9;
  int pad_x = (params && params[4] > 0) ? params[4] : 9;
  int pad_top = (params && params[5] >= 0) ? params[5] : 9;
  int by = margin + pad_top;
  if (by + btn > margin + appbar_h) by = margin + (appbar_h - btn) / 2;
  int close_x = margin + pad_x;
  int min_x = close_x + btn + gap;
  int max_x = min_x + btn + gap;
  if (cogito_hit_rect(x, y, close_x, by, btn, btn) ||
      cogito_hit_rect(x, y, min_x, by, btn, btn) ||
      cogito_hit_rect(x, y, max_x, by, btn, btn)) {
    return 0; // normal, so clicks on traffic lights are delivered to UI handlers.
  }
  // Cogito hit-test enum values: 0 = normal, 1 = draggable.
  return (y >= (margin - COGITO_WINDOW_DRAG_HIT_PAD) &&
          y < (margin + appbar_h + COGITO_WINDOW_DRAG_HIT_PAD) &&
          x >= (margin - COGITO_WINDOW_DRAG_HIT_PAD)) ? 1 : 0;
}

static void cogito_inspector_color_hex(CogitoColor c, char* out, size_t cap) {
  if (!out || cap == 0) return;
  if (c.a == 255) snprintf(out, cap, "#%02X%02X%02X", (unsigned int)c.r, (unsigned int)c.g, (unsigned int)c.b);
  else snprintf(out, cap, "#%02X%02X%02X%02X", (unsigned int)c.r, (unsigned int)c.g, (unsigned int)c.b, (unsigned int)c.a);
}

enum {
  COGITO_INSPECTOR_TAB_PROPERTIES = 0,
  COGITO_INSPECTOR_TAB_DIAGRAM = 1
};

enum {
  COGITO_INSPECTOR_PROP_NONE = 0,
  COGITO_INSPECTOR_PROP_TEXT,
  COGITO_INSPECTOR_PROP_SUBTITLE,
  COGITO_INSPECTOR_PROP_HINT,
  COGITO_INSPECTOR_PROP_ICON,
  COGITO_INSPECTOR_PROP_TOOLTIP,
  COGITO_INSPECTOR_PROP_ACTION_TEXT,
  COGITO_INSPECTOR_PROP_VIEW_ID,
  COGITO_INSPECTOR_PROP_CLASS,
  COGITO_INSPECTOR_PROP_A11Y_LABEL,
  COGITO_INSPECTOR_PROP_A11Y_ROLE,
  COGITO_INSPECTOR_PROP_FONT_FAMILY,
  COGITO_INSPECTOR_PROP_BORDER_WIDTH,
  COGITO_INSPECTOR_PROP_BORDER_RADIUS,
  COGITO_INSPECTOR_PROP_OPACITY,
  COGITO_INSPECTOR_PROP_PADDING_TOP,
  COGITO_INSPECTOR_PROP_PADDING_RIGHT,
  COGITO_INSPECTOR_PROP_PADDING_BOTTOM,
  COGITO_INSPECTOR_PROP_PADDING_LEFT,
  COGITO_INSPECTOR_PROP_MARGIN_TOP,
  COGITO_INSPECTOR_PROP_MARGIN_RIGHT,
  COGITO_INSPECTOR_PROP_MARGIN_BOTTOM,
  COGITO_INSPECTOR_PROP_MARGIN_LEFT,
  COGITO_INSPECTOR_PROP_ALIGN,
  COGITO_INSPECTOR_PROP_TEXT_ALIGN,
  COGITO_INSPECTOR_PROP_LABEL_MAX_CHARS,
  COGITO_INSPECTOR_PROP_GAP,
  COGITO_INSPECTOR_PROP_MIN_W,
  COGITO_INSPECTOR_PROP_MIN_H,
  COGITO_INSPECTOR_PROP_MAX_W,
  COGITO_INSPECTOR_PROP_MAX_H,
  COGITO_INSPECTOR_PROP_GRID_COL,
  COGITO_INSPECTOR_PROP_GRID_ROW,
  COGITO_INSPECTOR_PROP_GRID_SPAN_X,
  COGITO_INSPECTOR_PROP_GRID_SPAN_Y,
  COGITO_INSPECTOR_PROP_GRID_ALIGN_X,
  COGITO_INSPECTOR_PROP_GRID_ALIGN_Y,
  COGITO_INSPECTOR_PROP_RESIZE_MIN_W,
  COGITO_INSPECTOR_PROP_RESIZE_MIN_H,
  COGITO_INSPECTOR_PROP_CHECKED,
  COGITO_INSPECTOR_PROP_EDITABLE,
  COGITO_INSPECTOR_PROP_RESIZABLE,
  COGITO_INSPECTOR_PROP_TEXT_WRAP,
  COGITO_INSPECTOR_PROP_TEXT_ELLIPSIS,
  COGITO_INSPECTOR_PROP_FONT_MONO,
  COGITO_INSPECTOR_PROP_FONT_TABULAR,
  COGITO_INSPECTOR_PROP_AUTO_SIZE,
  COGITO_INSPECTOR_PROP_GRID_POS_SET,
  COGITO_INSPECTOR_PROP_FIXED_POS_SET,
  COGITO_INSPECTOR_PROP_HEXPAND_SET,
  COGITO_INSPECTOR_PROP_VEXPAND_SET,
  COGITO_INSPECTOR_PROP_HEXPAND,
  COGITO_INSPECTOR_PROP_VEXPAND,
  COGITO_INSPECTOR_PROP_HIDDEN,
  COGITO_INSPECTOR_PROP_DISABLED
};

typedef struct {
  int id;
  int x;
  int y;
  int w;
  int h;
  int value_x;
  int action; /* 0=normal, 1=increment stepper, -1=decrement stepper */
} CogitoInspectorPropRow;

#define COGITO_INSPECTOR_MAX_PROP_ROWS 320
#define COGITO_INSPECTOR_EDIT_BUF_CAP 1024
static CogitoInspectorPropRow cogito_inspector_prop_rows[COGITO_INSPECTOR_MAX_PROP_ROWS];
static int cogito_inspector_prop_row_count = 0;
static int cogito_inspector_right_tab = COGITO_INSPECTOR_TAB_PROPERTIES;
static int cogito_inspector_props_scroll_y = 0;
static int cogito_inspector_props_scroll_max = 0;
static int cogito_inspector_edit_prop = COGITO_INSPECTOR_PROP_NONE;
static CogitoNode* cogito_inspector_edit_node = NULL;
static char cogito_inspector_edit_buf[COGITO_INSPECTOR_EDIT_BUF_CAP];
static int cogito_inspector_edit_len = 0;

static bool cogito_inspector_prop_is_string(int id) {
  return id == COGITO_INSPECTOR_PROP_TEXT ||
         id == COGITO_INSPECTOR_PROP_SUBTITLE ||
         id == COGITO_INSPECTOR_PROP_HINT ||
         id == COGITO_INSPECTOR_PROP_ICON ||
         id == COGITO_INSPECTOR_PROP_TOOLTIP ||
         id == COGITO_INSPECTOR_PROP_ACTION_TEXT ||
         id == COGITO_INSPECTOR_PROP_VIEW_ID ||
         id == COGITO_INSPECTOR_PROP_CLASS ||
         id == COGITO_INSPECTOR_PROP_A11Y_LABEL ||
         id == COGITO_INSPECTOR_PROP_A11Y_ROLE ||
         id == COGITO_INSPECTOR_PROP_FONT_FAMILY;
}

static bool cogito_inspector_is_text_editing(void) {
  return cogito_inspector_edit_node && cogito_inspector_prop_is_string(cogito_inspector_edit_prop);
}

static void cogito_inspector_set_yisstr(YisStr** dst, const char* text) {
  if (!dst) return;
  if (*dst) {
    yis_release_val(YV_STR(*dst));
    *dst = NULL;
  }
  YisStr* ns = stdr_str_from_slice(text ? text : "", text ? strlen(text) : 0);
  *dst = ns;
  yis_retain_val(YV_STR(ns));
}

static const char* cogito_inspector_get_string_prop(CogitoNode* n, int prop_id) {
  if (!n) return "";
  if (prop_id == COGITO_INSPECTOR_PROP_TEXT) return (n->text && n->text->data) ? n->text->data : "";
  if (prop_id == COGITO_INSPECTOR_PROP_SUBTITLE) return (n->subtitle && n->subtitle->data) ? n->subtitle->data : "";
  if (prop_id == COGITO_INSPECTOR_PROP_HINT) return (n->hint && n->hint->data) ? n->hint->data : "";
  if (prop_id == COGITO_INSPECTOR_PROP_ICON) return (n->icon && n->icon->data) ? n->icon->data : "";
  if (prop_id == COGITO_INSPECTOR_PROP_TOOLTIP) return (n->tooltip && n->tooltip->data) ? n->tooltip->data : "";
  if (prop_id == COGITO_INSPECTOR_PROP_ACTION_TEXT) return (n->action_text && n->action_text->data) ? n->action_text->data : "";
  if (prop_id == COGITO_INSPECTOR_PROP_VIEW_ID) return (n->view_id && n->view_id->data) ? n->view_id->data : "";
  if (prop_id == COGITO_INSPECTOR_PROP_CLASS) return (n->class_name && n->class_name->data) ? n->class_name->data : "";
  if (prop_id == COGITO_INSPECTOR_PROP_A11Y_LABEL) return (n->a11y_label && n->a11y_label->data) ? n->a11y_label->data : "";
  if (prop_id == COGITO_INSPECTOR_PROP_A11Y_ROLE) return (n->a11y_role && n->a11y_role->data) ? n->a11y_role->data : "";
  if (prop_id == COGITO_INSPECTOR_PROP_FONT_FAMILY) return n->font_family;
  return "";
}

static const char* cogito_inspector_string_prop_for_row(CogitoNode* n, int prop_id, char* out, size_t cap) {
  if (!out || cap == 0) return "";
  if (cogito_inspector_edit_node == n && cogito_inspector_edit_prop == prop_id) {
    snprintf(out, cap, "%s_", cogito_inspector_edit_buf);
    return out;
  }
  const char* v = cogito_inspector_get_string_prop(n, prop_id);
  if (!v || !v[0]) return "";
  return v;
}

static bool cogito_inspector_apply_string_edit(CogitoNode* n, int prop_id, const char* text) {
  if (!n || !cogito_inspector_prop_is_string(prop_id)) return false;
  const char* src = text ? text : "";
  if (prop_id == COGITO_INSPECTOR_PROP_TEXT) {
    cogito_inspector_set_yisstr(&n->text, src);
    return true;
  }
  if (prop_id == COGITO_INSPECTOR_PROP_SUBTITLE) {
    cogito_inspector_set_yisstr(&n->subtitle, src);
    return true;
  }
  if (prop_id == COGITO_INSPECTOR_PROP_HINT) {
    cogito_inspector_set_yisstr(&n->hint, src);
    return true;
  }
  if (prop_id == COGITO_INSPECTOR_PROP_ICON) {
    cogito_inspector_set_yisstr(&n->icon, src);
    return true;
  }
  if (prop_id == COGITO_INSPECTOR_PROP_TOOLTIP) {
    cogito_inspector_set_yisstr(&n->tooltip, src);
    return true;
  }
  if (prop_id == COGITO_INSPECTOR_PROP_ACTION_TEXT) {
    cogito_inspector_set_yisstr(&n->action_text, src);
    return true;
  }
  if (prop_id == COGITO_INSPECTOR_PROP_VIEW_ID) {
    cogito_inspector_set_yisstr(&n->view_id, src);
    return true;
  }
  if (prop_id == COGITO_INSPECTOR_PROP_CLASS) {
    cogito_inspector_set_yisstr(&n->class_name, src);
    return true;
  }
  if (prop_id == COGITO_INSPECTOR_PROP_A11Y_LABEL) {
    cogito_inspector_set_yisstr(&n->a11y_label, src);
    return true;
  }
  if (prop_id == COGITO_INSPECTOR_PROP_A11Y_ROLE) {
    cogito_inspector_set_yisstr(&n->a11y_role, src);
    return true;
  }
  if (prop_id == COGITO_INSPECTOR_PROP_FONT_FAMILY) {
    strncpy(n->font_family, src, sizeof(n->font_family) - 1);
    n->font_family[sizeof(n->font_family) - 1] = '\0';
    n->font_family_set = true;
    return true;
  }
  return false;
}

static void cogito_inspector_begin_string_edit(CogitoNode* n, int prop_id) {
  if (!n || !cogito_inspector_prop_is_string(prop_id)) return;
  const char* v = cogito_inspector_get_string_prop(n, prop_id);
  size_t len = v ? strlen(v) : 0;
  if (len >= COGITO_INSPECTOR_EDIT_BUF_CAP) len = COGITO_INSPECTOR_EDIT_BUF_CAP - 1;
  if (len > 0) memcpy(cogito_inspector_edit_buf, v, len);
  cogito_inspector_edit_buf[len] = '\0';
  cogito_inspector_edit_len = (int)len;
  cogito_inspector_edit_prop = prop_id;
  cogito_inspector_edit_node = n;
}

static void cogito_inspector_cancel_string_edit(void) {
  cogito_inspector_edit_prop = COGITO_INSPECTOR_PROP_NONE;
  cogito_inspector_edit_node = NULL;
  cogito_inspector_edit_len = 0;
  cogito_inspector_edit_buf[0] = '\0';
}

static bool cogito_inspector_commit_string_edit(CogitoNode* win) {
  if (!cogito_inspector_is_text_editing()) return false;
  if (!cogito_inspector_edit_node) {
    cogito_inspector_cancel_string_edit();
    return false;
  }
  bool changed = cogito_inspector_apply_string_edit(cogito_inspector_edit_node, cogito_inspector_edit_prop, cogito_inspector_edit_buf);
  CogitoNode* owner = cogito_node_window(cogito_inspector_edit_node);
  if (!owner) owner = win;
  cogito_inspector_cancel_string_edit();
  if (changed && owner) {
    cogito_window_relayout(owner);
    cogito_queue_draw(owner);
  }
  return changed;
}

static void cogito_inspector_prop_rows_reset(void) {
  cogito_inspector_prop_row_count = 0;
}

static void cogito_inspector_prop_row_add(int id, int x, int y, int w, int h, int value_x) {
  if (id == COGITO_INSPECTOR_PROP_NONE) return;
  if (w <= 0 || h <= 0) return;
  if (cogito_inspector_prop_row_count >= COGITO_INSPECTOR_MAX_PROP_ROWS) return;
  CogitoInspectorPropRow* r = &cogito_inspector_prop_rows[cogito_inspector_prop_row_count++];
  r->id = id;
  r->x = x;
  r->y = y;
  r->w = w;
  r->h = h;
  r->value_x = value_x;
  r->action = 0;
}

static void cogito_inspector_prop_row_add_stepper(int id, int x, int y, int w, int h, int action) {
  if (id == COGITO_INSPECTOR_PROP_NONE) return;
  if (w <= 0 || h <= 0) return;
  if (cogito_inspector_prop_row_count >= COGITO_INSPECTOR_MAX_PROP_ROWS) return;
  CogitoInspectorPropRow* r = &cogito_inspector_prop_rows[cogito_inspector_prop_row_count++];
  r->id = id;
  r->x = x;
  r->y = y;
  r->w = w;
  r->h = h;
  r->value_x = x;
  r->action = action;
}

static CogitoInspectorPropRow* cogito_inspector_hit_prop_row(int x, int y) {
  for (int i = cogito_inspector_prop_row_count - 1; i >= 0; i--) {
    CogitoInspectorPropRow* r = &cogito_inspector_prop_rows[i];
    if (cogito_hit_rect(x, y, r->x, r->y, r->w, r->h)) {
      return r;
    }
  }
  return NULL;
}

static bool cogito_inspector_scroll_properties(CogitoNode* win, float wheel_delta) {
  if (!win || wheel_delta == 0.0f) return false;
  int delta = (int)(wheel_delta * 10.0f);
  cogito_inspector_props_scroll_y -= delta;
  if (cogito_inspector_props_scroll_y > cogito_inspector_props_scroll_max) {
    cogito_inspector_props_scroll_y = cogito_inspector_props_scroll_max;
  }
  if (cogito_inspector_props_scroll_y < 0) {
    cogito_inspector_props_scroll_y = 0;
  }
  cogito_queue_draw(win);
  return true;
}

static bool cogito_inspector_prop_is_toggle(int id) {
  return id == COGITO_INSPECTOR_PROP_CHECKED ||
         id == COGITO_INSPECTOR_PROP_EDITABLE ||
         id == COGITO_INSPECTOR_PROP_RESIZABLE ||
         id == COGITO_INSPECTOR_PROP_TEXT_WRAP ||
         id == COGITO_INSPECTOR_PROP_TEXT_ELLIPSIS ||
         id == COGITO_INSPECTOR_PROP_FONT_MONO ||
         id == COGITO_INSPECTOR_PROP_FONT_TABULAR ||
         id == COGITO_INSPECTOR_PROP_AUTO_SIZE ||
         id == COGITO_INSPECTOR_PROP_GRID_POS_SET ||
         id == COGITO_INSPECTOR_PROP_FIXED_POS_SET ||
         id == COGITO_INSPECTOR_PROP_HEXPAND_SET ||
         id == COGITO_INSPECTOR_PROP_VEXPAND_SET ||
         id == COGITO_INSPECTOR_PROP_HEXPAND ||
         id == COGITO_INSPECTOR_PROP_VEXPAND ||
         id == COGITO_INSPECTOR_PROP_HIDDEN ||
         id == COGITO_INSPECTOR_PROP_DISABLED;
}

static bool cogito_inspector_prop_is_numeric(int id) {
  return id == COGITO_INSPECTOR_PROP_BORDER_WIDTH ||
         id == COGITO_INSPECTOR_PROP_BORDER_RADIUS ||
         id == COGITO_INSPECTOR_PROP_OPACITY ||
         id == COGITO_INSPECTOR_PROP_PADDING_TOP ||
         id == COGITO_INSPECTOR_PROP_PADDING_RIGHT ||
         id == COGITO_INSPECTOR_PROP_PADDING_BOTTOM ||
         id == COGITO_INSPECTOR_PROP_PADDING_LEFT ||
         id == COGITO_INSPECTOR_PROP_MARGIN_TOP ||
         id == COGITO_INSPECTOR_PROP_MARGIN_RIGHT ||
         id == COGITO_INSPECTOR_PROP_MARGIN_BOTTOM ||
         id == COGITO_INSPECTOR_PROP_MARGIN_LEFT ||
         id == COGITO_INSPECTOR_PROP_ALIGN ||
         id == COGITO_INSPECTOR_PROP_TEXT_ALIGN ||
         id == COGITO_INSPECTOR_PROP_LABEL_MAX_CHARS ||
         id == COGITO_INSPECTOR_PROP_GAP ||
         id == COGITO_INSPECTOR_PROP_MIN_W ||
         id == COGITO_INSPECTOR_PROP_MIN_H ||
         id == COGITO_INSPECTOR_PROP_MAX_W ||
         id == COGITO_INSPECTOR_PROP_MAX_H ||
         id == COGITO_INSPECTOR_PROP_GRID_COL ||
         id == COGITO_INSPECTOR_PROP_GRID_ROW ||
         id == COGITO_INSPECTOR_PROP_GRID_SPAN_X ||
         id == COGITO_INSPECTOR_PROP_GRID_SPAN_Y ||
         id == COGITO_INSPECTOR_PROP_GRID_ALIGN_X ||
         id == COGITO_INSPECTOR_PROP_GRID_ALIGN_Y ||
         id == COGITO_INSPECTOR_PROP_RESIZE_MIN_W ||
         id == COGITO_INSPECTOR_PROP_RESIZE_MIN_H;
}

static bool cogito_inspector_apply_edit(CogitoNode* n, int prop_id, int wheel_dir, bool toggle_click) {
  if (!n || prop_id == COGITO_INSPECTOR_PROP_NONE) return false;
  bool changed = false;
  int d = wheel_dir;
  if (d > 1) d = 1;
  if (d < -1) d = -1;

  if (toggle_click && cogito_inspector_prop_is_toggle(prop_id)) {
    if (prop_id == COGITO_INSPECTOR_PROP_CHECKED) {
      n->checked = !n->checked;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_EDITABLE) {
      n->editable = !n->editable;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_RESIZABLE) {
      n->resizable = !n->resizable;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_TEXT_WRAP) {
      n->text_wrap = !n->text_wrap;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_TEXT_ELLIPSIS) {
      n->text_ellipsis = !n->text_ellipsis;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_FONT_MONO) {
      n->font_mono = !n->font_mono;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_FONT_TABULAR) {
      n->font_tabular = !n->font_tabular;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_AUTO_SIZE) {
      n->auto_size = !n->auto_size;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_GRID_POS_SET) {
      n->grid_pos_set = !n->grid_pos_set;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_FIXED_POS_SET) {
      n->fixed_pos_set = !n->fixed_pos_set;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_HEXPAND_SET) {
      n->hexpand_set = !n->hexpand_set;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_VEXPAND_SET) {
      n->vexpand_set = !n->vexpand_set;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_HEXPAND) {
      n->hexpand = !n->hexpand;
      n->hexpand_set = true;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_VEXPAND) {
      n->vexpand = !n->vexpand;
      n->vexpand_set = true;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_HIDDEN) {
      n->hidden = !n->hidden;
      changed = true;
    } else if (prop_id == COGITO_INSPECTOR_PROP_DISABLED) {
      n->disabled = !n->disabled;
      changed = true;
    }
    return changed;
  }

  if (!cogito_inspector_prop_is_numeric(prop_id) || d == 0) return false;

  if (prop_id == COGITO_INSPECTOR_PROP_BORDER_WIDTH) {
    int v = n->border_width + d;
    if (v < 0) v = 0;
    if (v != n->border_width) {
      n->border_width = v;
      n->border_width_set = true;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_BORDER_RADIUS) {
    int v = n->border_radius + d;
    if (v < 0) v = 0;
    if (v != n->border_radius) {
      n->border_radius = v;
      n->radius_set = true;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_OPACITY) {
    float v = n->opacity_set ? n->opacity : 1.0f;
    v += (float)d * 0.05f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    float diff = v - n->opacity;
    if (diff < 0.0f) diff = -diff;
    if (!n->opacity_set || diff > 0.0001f) {
      n->opacity = v;
      n->opacity_set = true;
      n->has_opacity = true;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_PADDING_TOP) {
    int v = n->padding_top + d;
    if (v < 0) v = 0;
    if (v != n->padding_top) {
      n->padding_top = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_PADDING_RIGHT) {
    int v = n->padding_right + d;
    if (v < 0) v = 0;
    if (v != n->padding_right) {
      n->padding_right = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_PADDING_BOTTOM) {
    int v = n->padding_bottom + d;
    if (v < 0) v = 0;
    if (v != n->padding_bottom) {
      n->padding_bottom = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_PADDING_LEFT) {
    int v = n->padding_left + d;
    if (v < 0) v = 0;
    if (v != n->padding_left) {
      n->padding_left = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_MARGIN_TOP) {
    int v = n->margin_top + d;
    if (v < 0) v = 0;
    if (v != n->margin_top) {
      n->margin_top = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_MARGIN_RIGHT) {
    int v = n->margin_right + d;
    if (v < 0) v = 0;
    if (v != n->margin_right) {
      n->margin_right = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_MARGIN_BOTTOM) {
    int v = n->margin_bottom + d;
    if (v < 0) v = 0;
    if (v != n->margin_bottom) {
      n->margin_bottom = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_MARGIN_LEFT) {
    int v = n->margin_left + d;
    if (v < 0) v = 0;
    if (v != n->margin_left) {
      n->margin_left = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_ALIGN) {
    int v = n->align + d;
    if (v < 0) v = 0;
    if (v > 8) v = 8;
    if (v != n->align) {
      n->align = v;
      n->halign_set = true;
      n->valign_set = true;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_TEXT_ALIGN) {
    int v = n->text_align + d;
    if (v < 0) v = 0;
    if (v > 2) v = 2;
    if (v != n->text_align) {
      n->text_align = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_LABEL_MAX_CHARS) {
    int step = 2;
    int cur = n->label_max_width_chars_set ? n->label_max_width_chars : 0;
    int v = cur + d * step;
    if (v < 0) v = 0;
    if (!n->label_max_width_chars_set || v != n->label_max_width_chars) {
      n->label_max_width_chars = v;
      n->label_max_width_chars_set = true;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_GAP) {
    int v = n->gap + d;
    if (v < 0) v = 0;
    if (v != n->gap) {
      n->gap = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_MIN_W) {
    int step = 4;
    int cur = n->min_w_set ? n->min_w : (n->w > 0 ? n->w : 0);
    int v = cur + d * step;
    if (v < 0) v = 0;
    if (!n->min_w_set || v != n->min_w) {
      n->min_w = v;
      n->min_w_set = true;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_MIN_H) {
    int step = 4;
    int cur = n->min_h_set ? n->min_h : (n->h > 0 ? n->h : 0);
    int v = cur + d * step;
    if (v < 0) v = 0;
    if (!n->min_h_set || v != n->min_h) {
      n->min_h = v;
      n->min_h_set = true;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_MAX_W) {
    int step = 4;
    int cur = (n->max_w_set && n->max_w > 0) ? n->max_w : (n->w > 0 ? n->w : 64);
    int v = cur + d * step;
    if (v < 1) v = 1;
    if (!n->max_w_set || v != n->max_w) {
      n->max_w = v;
      n->max_w_set = true;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_MAX_H) {
    int step = 4;
    int cur = (n->max_h_set && n->max_h > 0) ? n->max_h : (n->h > 0 ? n->h : 64);
    int v = cur + d * step;
    if (v < 1) v = 1;
    if (!n->max_h_set || v != n->max_h) {
      n->max_h = v;
      n->max_h_set = true;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_GRID_COL) {
    int v = n->grid_col + d;
    if (v < 0) v = 0;
    if (v != n->grid_col) {
      n->grid_col = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_GRID_ROW) {
    int v = n->grid_row + d;
    if (v < 0) v = 0;
    if (v != n->grid_row) {
      n->grid_row = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_GRID_SPAN_X) {
    int v = n->grid_span_x + d;
    if (v < 1) v = 1;
    if (v != n->grid_span_x) {
      n->grid_span_x = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_GRID_SPAN_Y) {
    int v = n->grid_span_y + d;
    if (v < 1) v = 1;
    if (v != n->grid_span_y) {
      n->grid_span_y = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_GRID_ALIGN_X) {
    int v = n->grid_align_x + d;
    if (v < 0) v = 0;
    if (v > 2) v = 2;
    if (v != n->grid_align_x) {
      n->grid_align_x = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_GRID_ALIGN_Y) {
    int v = n->grid_align_y + d;
    if (v < 0) v = 0;
    if (v > 2) v = 2;
    if (v != n->grid_align_y) {
      n->grid_align_y = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_RESIZE_MIN_W) {
    int step = 4;
    int v = n->resize_min_w + d * step;
    if (v < 0) v = 0;
    if (v != n->resize_min_w) {
      n->resize_min_w = v;
      changed = true;
    }
  } else if (prop_id == COGITO_INSPECTOR_PROP_RESIZE_MIN_H) {
    int step = 4;
    int v = n->resize_min_h + d * step;
    if (v < 0) v = 0;
    if (v != n->resize_min_h) {
      n->resize_min_h = v;
      changed = true;
    }
  }

  return changed;
}

static void cogito_inspector_panel_rect(CogitoNode* win, int* out_x, int* out_y, int* out_w, int* out_h, int* out_tree_w) {
  if (!win) return;
  int view_x = cogito_inspector_view_x;
  int view_y = cogito_inspector_view_y;
  int view_w = cogito_inspector_view_w > 0 ? cogito_inspector_view_w : win->w;
  int view_h = cogito_inspector_view_h > 0 ? cogito_inspector_view_h : win->h;
  if (view_w < 0) view_w = 0;
  if (view_h < 0) view_h = 0;
  int panel_w = view_w;
  int x = view_x;
  int y = view_y;
  int h = view_h > COGITO_INSPECTOR_STATUS_H ? view_h - COGITO_INSPECTOR_STATUS_H : view_h;
  int tree_w = (panel_w * 30) / 100;
  if (tree_w < 180) tree_w = 180;
  if (tree_w > 300) tree_w = 300;
  if (tree_w > panel_w - 220) tree_w = panel_w - 220;
  if (tree_w < 100) tree_w = 100;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = panel_w;
  if (out_h) *out_h = h;
  if (out_tree_w) *out_tree_w = tree_w;
}

static void cogito_inspector_tree_metrics(int* out_header_h, int* out_row_h, int* out_pad) {
  if (out_header_h) *out_header_h = 32;
  if (out_row_h) *out_row_h = 22;
  if (out_pad) *out_pad = 6;
}

static bool cogito_inspector_is_collapsed(CogitoNode* n) {
  if (!n) return false;
  for (int i = 0; i < cogito_inspector_collapsed_count; i++) {
    if (cogito_inspector_collapsed_nodes[i] == n) {
      return cogito_inspector_collapsed_values[i];
    }
  }
  return false;
}

static void cogito_inspector_set_collapsed(CogitoNode* n, bool collapsed) {
  if (!n) return;
  for (int i = 0; i < cogito_inspector_collapsed_count; i++) {
    if (cogito_inspector_collapsed_nodes[i] == n) {
      if (collapsed) {
        cogito_inspector_collapsed_values[i] = true;
      } else {
        for (int j = i; j + 1 < cogito_inspector_collapsed_count; j++) {
          cogito_inspector_collapsed_nodes[j] = cogito_inspector_collapsed_nodes[j + 1];
          cogito_inspector_collapsed_values[j] = cogito_inspector_collapsed_values[j + 1];
        }
        cogito_inspector_collapsed_count--;
      }
      return;
    }
  }
  if (collapsed && cogito_inspector_collapsed_count < COGITO_INSPECTOR_MAX_NODES) {
    cogito_inspector_collapsed_nodes[cogito_inspector_collapsed_count] = n;
    cogito_inspector_collapsed_values[cogito_inspector_collapsed_count] = true;
    cogito_inspector_collapsed_count++;
  }
}

static void cogito_inspector_collect_nodes(CogitoNode* n, int depth) {
  if (!n) return;
  if (cogito_inspector_node_count >= COGITO_INSPECTOR_MAX_NODES) return;
  cogito_inspector_nodes[cogito_inspector_node_count] = n;
  cogito_inspector_depths[cogito_inspector_node_count] = depth;
  cogito_inspector_node_count++;
  if (n->len == 0 || cogito_inspector_is_collapsed(n)) return;
  for (size_t i = 0; i < n->len; i++) {
    cogito_inspector_collect_nodes(n->children[i], depth + 1);
  }
}

static void cogito_inspector_rebuild_tree(CogitoNode* win) {
  cogito_inspector_node_count = 0;
  cogito_inspector_collect_nodes(win, 0);
}

static void cogito_inspector_node_label(CogitoNode* n, char* out, size_t cap) {
  if (!out || cap == 0) return;
  if (!n) {
    snprintf(out, cap, "<null>");
    return;
  }
  const char* kind = cogito_kind_name(n->kind);
  const char* cls = (n->class_name && n->class_name->data && n->class_name->data[0]) ? n->class_name->data : NULL;
  if (cls) snprintf(out, cap, "%s.%s", kind, cls);
  else snprintf(out, cap, "%s", kind);
}

static CogitoNode* cogito_inspector_pick_node_recursive(CogitoNode* n, int x, int y) {
  if (!n || !cogito_hit_node(n, x, y)) return NULL;
  for (size_t i = n->len; i > 0; i--) {
    CogitoNode* hit = cogito_inspector_pick_node_recursive(n->children[i - 1], x, y);
    if (hit) return hit;
  }
  return n;
}

static void cogito_inspector_right_panel_metrics(int px, int py, int pw, int ph, int tree_w, int pad,
                                                 int* out_rx, int* out_rw,
                                                 int* out_tab_y, int* out_tab_h,
                                                 int* out_tab0_x, int* out_tab0_w,
                                                 int* out_tab1_x, int* out_tab1_w,
                                                 int* out_content_y, int* out_content_h) {
  int rx = px + tree_w + pad;
  int rw = pw - tree_w - pad * 2;
  if (rw < 40) rw = 40;

  int tab_y = py + 38;
  int tab_h = 22;
  int gap = 8;
  int tab_w = (rw - gap) / 2;
  if (tab_w < 60) tab_w = (rw > 0) ? rw : 0;
  int tab0_x = rx;
  int tab1_x = rx + tab_w + gap;
  int tab1_w = rw - tab_w - gap;
  if (tab1_w < 0) tab1_w = 0;

  int content_y = tab_y + tab_h + 8;
  int content_h = ph - (content_y - py) - 6;
  if (content_h < 0) content_h = 0;

  if (out_rx) *out_rx = rx;
  if (out_rw) *out_rw = rw;
  if (out_tab_y) *out_tab_y = tab_y;
  if (out_tab_h) *out_tab_h = tab_h;
  if (out_tab0_x) *out_tab0_x = tab0_x;
  if (out_tab0_w) *out_tab0_w = tab_w;
  if (out_tab1_x) *out_tab1_x = tab1_x;
  if (out_tab1_w) *out_tab1_w = tab1_w;
  if (out_content_y) *out_content_y = content_y;
  if (out_content_h) *out_content_h = content_h;
}

/* Returns true if wheel was consumed (scrolled widget tree). local_x, local_y are in inspector content coords. */
static bool cogito_inspector_handle_wheel(CogitoNode* win, int local_x, int local_y, float wheel_delta) {
  if (!cogito_inspector_open || !win || wheel_delta == 0.0f) return false;
  int px = 0, py = 0, pw = 0, ph = 0, tree_w = 0;
  cogito_inspector_panel_rect(win, &px, &py, &pw, &ph, &tree_w);
  int header_h = 0;
  int row_h = 0;
  int pad = 0;
  cogito_inspector_tree_metrics(&header_h, &row_h, &pad);
  int list_y = py + header_h + 4;
  int list_h = ph - (header_h + 8);
  if (list_h < 0) list_h = 0;

  int rx = 0, rw = 0, tab_y = 0, tab_h = 0, tab0_x = 0, tab0_w = 0, tab1_x = 0, tab1_w = 0, content_y = 0, content_h = 0;
  cogito_inspector_right_panel_metrics(
    px, py, pw, ph, tree_w, pad,
    &rx, &rw, &tab_y, &tab_h,
    &tab0_x, &tab0_w, &tab1_x, &tab1_w,
    &content_y, &content_h
  );

  if (cogito_hit_rect(local_x, local_y, rx, content_y, rw, content_h) && cogito_inspector_right_tab == COGITO_INSPECTOR_TAB_PROPERTIES) {
    CogitoNode* s = cogito_inspector_selected ? cogito_inspector_selected : win;
    CogitoInspectorPropRow* row = cogito_inspector_hit_prop_row(local_x, local_y);
    int row_id = row ? row->id : COGITO_INSPECTOR_PROP_NONE;
    bool over_value_col = row && local_x >= row->value_x;
    if (s && row_id != COGITO_INSPECTOR_PROP_NONE && cogito_inspector_prop_is_numeric(row_id) && over_value_col) {
      int dir = (wheel_delta > 0.0f) ? 1 : -1;
      if (cogito_inspector_apply_edit(s, row_id, dir, false)) {
        CogitoNode* owner = cogito_node_window(s);
        if (!owner) owner = win;
        if (owner) {
          cogito_window_relayout(owner);
          cogito_queue_draw(owner);
        }
        return true;
      }
    }
    return cogito_inspector_scroll_properties(win, wheel_delta);
  }

  if (!cogito_hit_rect(local_x, local_y, px, list_y, tree_w, list_h)) return false;
  cogito_inspector_rebuild_tree(win);
  int total_tree_h = cogito_inspector_node_count * row_h;
  int max_scroll = total_tree_h > list_h ? total_tree_h - list_h : 0;
  int delta = (int)(wheel_delta * 10.0f);
  cogito_inspector_tree_scroll_y -= delta;
  if (cogito_inspector_tree_scroll_y > max_scroll) cogito_inspector_tree_scroll_y = max_scroll;
  if (cogito_inspector_tree_scroll_y < 0) cogito_inspector_tree_scroll_y = 0;
  return true;
}

static bool cogito_inspector_handle_click(CogitoNode* win, int x, int y) {
  if (!cogito_inspector_open || !win) return false;
  int px = 0, py = 0, pw = 0, ph = 0, tree_w = 0;
  cogito_inspector_panel_rect(win, &px, &py, &pw, &ph, &tree_w);
  if (!cogito_hit_rect(x, y, px, py, pw, ph)) return false;

  int header_h = 0;
  int row_h = 0;
  int pad = 0;
  cogito_inspector_tree_metrics(&header_h, &row_h, &pad);

  int rx = 0, rw = 0, tab_y = 0, tab_h = 0, tab0_x = 0, tab0_w = 0, tab1_x = 0, tab1_w = 0, content_y = 0, content_h = 0;
  cogito_inspector_right_panel_metrics(
    px, py, pw, ph, tree_w, pad,
    &rx, &rw, &tab_y, &tab_h,
    &tab0_x, &tab0_w, &tab1_x, &tab1_w,
    &content_y, &content_h
  );

  if (cogito_hit_rect(x, y, tab0_x, tab_y, tab0_w, tab_h)) {
    (void)cogito_inspector_commit_string_edit(win);
    cogito_inspector_right_tab = COGITO_INSPECTOR_TAB_PROPERTIES;
    cogito_queue_draw(win);
    return true;
  }
  if (cogito_hit_rect(x, y, tab1_x, tab_y, tab1_w, tab_h)) {
    (void)cogito_inspector_commit_string_edit(win);
    cogito_inspector_right_tab = COGITO_INSPECTOR_TAB_DIAGRAM;
    cogito_queue_draw(win);
    return true;
  }

  if (cogito_hit_rect(x, y, rx, content_y, rw, content_h) && cogito_inspector_right_tab == COGITO_INSPECTOR_TAB_PROPERTIES) {
    CogitoNode* s = cogito_inspector_selected ? cogito_inspector_selected : win;
    CogitoInspectorPropRow* row = cogito_inspector_hit_prop_row(x, y);
    int row_id = row ? row->id : COGITO_INSPECTOR_PROP_NONE;
    int row_action = row ? row->action : 0;
    bool over_value_col = row && x >= row->value_x;
    /* Stepper button click */
    if (s && row_id != COGITO_INSPECTOR_PROP_NONE && cogito_inspector_prop_is_numeric(row_id) && row_action != 0) {
      (void)cogito_inspector_commit_string_edit(win);
      if (cogito_inspector_apply_edit(s, row_id, row_action, false)) {
        CogitoNode* owner = cogito_node_window(s);
        if (!owner) owner = win;
        if (owner) {
          cogito_window_relayout(owner);
          cogito_queue_draw(owner);
        }
      }
      return true;
    }
    if (s && row_id != COGITO_INSPECTOR_PROP_NONE && cogito_inspector_prop_is_string(row_id) && over_value_col) {
      cogito_inspector_begin_string_edit(s, row_id);
      cogito_queue_draw(win);
      return true;
    }
    if (s && row_id != COGITO_INSPECTOR_PROP_NONE && cogito_inspector_prop_is_toggle(row_id)) {
      (void)cogito_inspector_commit_string_edit(win);
      if (cogito_inspector_apply_edit(s, row_id, 0, true)) {
        CogitoNode* owner = cogito_node_window(s);
        if (!owner) owner = win;
        if (owner) {
          cogito_window_relayout(owner);
          cogito_queue_draw(owner);
        }
      }
      return true;
    }
    if (cogito_inspector_is_text_editing()) {
      (void)cogito_inspector_commit_string_edit(win);
      return true;
    }
  }

  cogito_inspector_rebuild_tree(win);
  int list_y = py + header_h + 4;
  int list_h = ph - (header_h + 8);
  if (list_h < 0) list_h = 0;
  int total_tree_h = cogito_inspector_node_count * row_h;
  int max_scroll = total_tree_h > list_h ? total_tree_h - list_h : 0;
  if (cogito_inspector_tree_scroll_y > max_scroll) cogito_inspector_tree_scroll_y = max_scroll;
  if (cogito_inspector_tree_scroll_y < 0) cogito_inspector_tree_scroll_y = 0;
  if (cogito_hit_rect(x, y, px, list_y, tree_w, list_h)) {
    int row = (y - list_y + cogito_inspector_tree_scroll_y) / row_h;
    if (row >= 0 && row < cogito_inspector_node_count) {
      CogitoNode* n = cogito_inspector_nodes[row];
      int depth = cogito_inspector_depths[row];
      int row_y = list_y + row * row_h;
      int indent_x = px + pad + depth * 12;
      int chevron_x = indent_x;
      int chevron_y = row_y + (row_h - 12) / 2;
      int chevron_w = 10;
      int chevron_h = 12;
      if (n && n->len > 0 && cogito_hit_rect(x, y, chevron_x, chevron_y, chevron_w, chevron_h)) {
        (void)cogito_inspector_commit_string_edit(win);
        cogito_inspector_set_collapsed(n, !cogito_inspector_is_collapsed(n));
      } else {
        (void)cogito_inspector_commit_string_edit(win);
        cogito_inspector_selected = n;
        cogito_inspector_props_scroll_y = 0;
        cogito_inspector_props_scroll_max = 0;
      }
    }
  }
  return true;
}

static bool cogito_inspector_handle_text_input(CogitoNode* win) {
  if (!cogito_inspector_is_text_editing()) return false;
  if (!cogito_inspector_edit_node) {
    cogito_inspector_cancel_string_edit();
    return false;
  }
  bool changed = false;
  int ch = cogito_backend->get_char_pressed();
  while (ch > 0) {
    if (ch == '\t' || ch == '\n' || ch == '\r') {
      ch = cogito_backend->get_char_pressed();
      continue;
    }
    if (ch >= 32 && ch <= 126 && cogito_inspector_edit_len < (COGITO_INSPECTOR_EDIT_BUF_CAP - 1)) {
      cogito_inspector_edit_buf[cogito_inspector_edit_len++] = (char)ch;
      cogito_inspector_edit_buf[cogito_inspector_edit_len] = '\0';
      changed = true;
    }
    ch = cogito_backend->get_char_pressed();
  }
  if (cogito_backend->is_key_pressed(SDL_SCANCODE_BACKSPACE) && cogito_inspector_edit_len > 0) {
    cogito_inspector_edit_len--;
    cogito_inspector_edit_buf[cogito_inspector_edit_len] = '\0';
    changed = true;
  }
  if (cogito_backend->is_key_pressed(SDL_SCANCODE_ESCAPE)) {
    cogito_inspector_cancel_string_edit();
    cogito_queue_draw(win);
    return true;
  }
  if (cogito_backend->is_key_pressed(SDL_SCANCODE_RETURN) ||
      cogito_backend->is_key_pressed(SDL_SCANCODE_KP_ENTER)) {
    (void)cogito_inspector_commit_string_edit(win);
    cogito_queue_draw(win);
    return true;
  }
  if (changed) {
    cogito_queue_draw(win);
    return true;
  }
  return true;
}

static void cogito_draw_inspector_highlight(void) {
  if (!cogito_inspector_open || !cogito_inspector_selected) return;
  CogitoNode* n = cogito_inspector_selected;
  if (n->w <= 0 || n->h <= 0) return;
  CogitoColor red = cogito_rgba(220, 44, 44, 255);
  cogito_draw_rect_lines(n->x - 1, n->y - 1, n->w + 2, n->h + 2, red, n->border_radius, 2);
}

/* Draws a toggle/switch row for a boolean property. Returns next y. */
static int cogito_inspector_draw_switch_row(CogitoNode* win, int x, int y, int w,
                                            const char* key, bool value,
                                            CogitoColor key_color,
                                            CogitoColor row_bg, CogitoColor row_border,
                                            int text_size, int row_pad,
                                            int prop_id) {
  if (!win || !key || w <= 0) return y;
  const int row_h = 28;
  cogito_draw_rect(x, y, w, row_h, row_bg, 4);
  cogito_draw_rect_lines(x, y, w, row_h, row_border, 4, 1);

  int key_x = x + row_pad;
  cogito_draw_text_size_node(win, key_x, y + (row_h - cogito_text_height_size(text_size)) / 2, key, key_color, text_size, false, false);

  /* Switch track: pill shape */
  const int track_w = 34;
  const int track_h = 18;
  const int thumb_d = 12;
  int track_x = x + w - row_pad - track_w;
  int track_y = y + (row_h - track_h) / 2;
  int thumb_r = thumb_d / 2;
  CogitoColor on_track  = cogito_rgba(52, 168, 83,  220);
  CogitoColor off_track = cogito_rgba(90, 98, 112, 180);
  CogitoColor thumb_col = cogito_rgba(240, 242, 245, 255);
  cogito_draw_rect(track_x, track_y, track_w, track_h, value ? on_track : off_track, track_h / 2);
  int thumb_cx = value ? (track_x + track_w - thumb_r - 3) : (track_x + thumb_r + 3);
  int thumb_cy = track_y + track_h / 2;
  cogito_draw_rect(thumb_cx - thumb_r, thumb_cy - thumb_r, thumb_d, thumb_d, thumb_col, thumb_r);

  int value_x = track_x;
  cogito_inspector_prop_row_add(prop_id, x, y, w, row_h, value_x);
  return y + row_h + 4;
}

/* Draws a text-field row for a string property. Returns next y. */
static int cogito_inspector_draw_string_row(CogitoNode* win, int x, int y, int w,
                                            const char* key, const char* value,
                                            CogitoColor key_color,
                                            CogitoColor row_bg, CogitoColor row_border,
                                            int text_size, int row_pad,
                                            int prop_id,
                                            bool is_editing) {
  if (!win || !key || w <= 0) return y;
  int row_h = cogito_text_height_size(text_size) + row_pad * 2;
  if (row_h < 22) row_h = 22;
  cogito_draw_rect(x, y, w, row_h, row_bg, 4);
  cogito_draw_rect_lines(x, y, w, row_h, row_border, 4, 1);

  int key_x = x + row_pad;
  int key_w = cogito_text_width_size_node(win, key, text_size);
  cogito_draw_text_size_node(win, key_x, y + row_pad, key, key_color, text_size, false, false);

  /* Text field box on the right */
  int field_x = key_x + key_w + 10;
  int field_w = x + w - row_pad - field_x;
  if (field_w < 20) field_w = 20;
  int field_h = row_h - row_pad;
  int field_y = y + row_pad / 2;
  CogitoColor field_bg     = cogito_rgba(14, 18, 24, 220);
  CogitoColor field_border = is_editing ? cogito_rgba(82, 148, 255, 255) : cogito_rgba(75, 82, 95, 160);
  CogitoColor field_text   = is_editing ? cogito_rgba(138, 214, 255, 255) : cogito_rgba(210, 218, 230, 255);
  cogito_draw_rect(field_x, field_y, field_w, field_h, field_bg, 4);
  cogito_draw_rect_lines(field_x, field_y, field_w, field_h, field_border, 4, 1);

  int text_y = field_y + (field_h - cogito_text_height_size(text_size)) / 2;
  int clip_pad = 4;
  if (cogito_backend && cogito_backend->begin_scissor) {
    cogito_backend->begin_scissor(field_x + clip_pad, field_y, field_w - clip_pad * 2, field_h);
  }
  if (value && value[0]) {
    cogito_draw_text_size_node(win, field_x + clip_pad, text_y, value, field_text, text_size, false, false);
  }
  if (cogito_backend && cogito_backend->end_scissor) {
    cogito_backend->end_scissor();
  }

  cogito_inspector_prop_row_add(prop_id, x, y, w, row_h, field_x);
  return y + row_h + 4;
}

/* Draws a stepper row for a numeric property. Returns next y. */
static int cogito_inspector_draw_stepper_row(CogitoNode* win, int x, int y, int w,
                                             const char* key, const char* value,
                                             CogitoColor key_color,
                                             CogitoColor row_bg, CogitoColor row_border,
                                             int text_size, int row_pad,
                                             int prop_id) {
  if (!win || !key || w <= 0) return y;
  int row_h = cogito_text_height_size(text_size) + row_pad * 2;
  if (row_h < 22) row_h = 22;
  cogito_draw_rect(x, y, w, row_h, row_bg, 4);
  cogito_draw_rect_lines(x, y, w, row_h, row_border, 4, 1);

  int key_x = x + row_pad;
  cogito_draw_text_size_node(win, key_x, y + row_pad, key, key_color, text_size, false, false);

  /* Stepper: [-] value [+] on the right */
  const int btn_w = 20;
  const int btn_h = row_h - row_pad;
  int stepper_right = x + w - row_pad;
  int inc_x = stepper_right - btn_w;
  int inc_y = y + row_pad / 2;
  int val_w = cogito_text_width_size_node(win, value ? value : "0", text_size);
  if (val_w < 24) val_w = 24;
  int dec_x = inc_x - val_w - 4 - btn_w;
  int dec_y = inc_y;

  CogitoColor btn_bg    = cogito_rgba(44, 50, 62, 255);
  CogitoColor btn_bdr   = cogito_rgba(75, 82, 95, 200);
  CogitoColor btn_text  = cogito_rgba(200, 210, 225, 255);
  CogitoColor val_text  = cogito_rgba(138, 214, 255, 255);

  cogito_draw_rect(dec_x, dec_y, btn_w, btn_h, btn_bg, 4);
  cogito_draw_rect_lines(dec_x, dec_y, btn_w, btn_h, btn_bdr, 4, 1);
  cogito_draw_text_size_node(win,
    dec_x + (btn_w - cogito_text_width_size_node(win, "-", text_size)) / 2,
    dec_y + (btn_h - cogito_text_height_size(text_size)) / 2,
    "-", btn_text, text_size, false, false);

  int val_x = dec_x + btn_w + 4;
  int val_y = y + row_pad;
  cogito_draw_text_size_node(win,
    val_x + (val_w - cogito_text_width_size_node(win, value ? value : "0", text_size)) / 2,
    val_y, value ? value : "0", val_text, text_size, false, false);

  cogito_draw_rect(inc_x, inc_y, btn_w, btn_h, btn_bg, 4);
  cogito_draw_rect_lines(inc_x, inc_y, btn_w, btn_h, btn_bdr, 4, 1);
  cogito_draw_text_size_node(win,
    inc_x + (btn_w - cogito_text_width_size_node(win, "+", text_size)) / 2,
    inc_y + (btn_h - cogito_text_height_size(text_size)) / 2,
    "+", btn_text, text_size, false, false);

  /* Register: whole row for wheel, dec button, inc button */
  int value_x = dec_x;
  cogito_inspector_prop_row_add(prop_id, x, y, w, row_h, value_x);
  cogito_inspector_prop_row_add_stepper(prop_id, dec_x, dec_y, btn_w, btn_h, -1);
  cogito_inspector_prop_row_add_stepper(prop_id, inc_x, inc_y, btn_w, btn_h,  1);
  return y + row_h + 4;
}



static int cogito_inspector_draw_prop_kv_row(CogitoNode* win, int x, int y, int w,
                                             const char* key, const char* value,
                                             CogitoColor key_color, CogitoColor value_color,
                                             CogitoColor row_bg, CogitoColor row_border,
                                             int text_size, int row_pad,
                                             int prop_id) {
  if (!win || !key || !value || w <= 0) return y;
  int row_h = cogito_text_height_size(text_size) + row_pad * 2;
  if (row_h < 16) row_h = 16;
  cogito_draw_rect(x, y, w, row_h, row_bg, 4);
  cogito_draw_rect_lines(x, y, w, row_h, row_border, 4, 1);

  int key_x = x + row_pad;
  int key_w = cogito_text_width_size_node(win, key, text_size);
  int value_w = cogito_text_width_size_node(win, value, text_size);
  int value_x = x + w - row_pad - value_w;
  int min_value_x = key_x + key_w + 10;
  if (value_x < min_value_x) value_x = min_value_x;
  cogito_inspector_prop_row_add(prop_id, x, y, w, row_h, value_x);

  cogito_draw_text_size_node(win, key_x, y + row_pad, key, key_color, text_size, false, false);
  cogito_draw_text_size_node(win, value_x, y + row_pad, value, value_color, text_size, false, false);
  return y + row_h + 4;
}

static int cogito_inspector_draw_geometry_state_widget(CogitoNode* win, int x, int y, int w,
                                                       int geom_w, int geom_h,
                                                       bool hover, bool active, bool focused, bool disabled,
                                                       CogitoColor text, CogitoColor muted) {
  if (!win || w <= 0) return y;
  const int label_size = 12;
  const int label_h = cogito_text_height_size(label_size);
  if (geom_w < 1) geom_w = 1;
  if (geom_h < 1) geom_h = 1;

  bool representation = (geom_w > 300 || geom_h > 300);
  int box_w = geom_w;
  int box_h = geom_h;
  if (representation) {
    float sx = 140.0f / (float)geom_w;
    float sy = 140.0f / (float)geom_h;
    float s = sx < sy ? sx : sy;
    if (s > 1.0f) s = 1.0f;
    box_w = (int)lroundf((float)geom_w * s);
    box_h = (int)lroundf((float)geom_h * s);
    if (box_w < 1) box_w = 1;
    if (box_h < 1) box_h = 1;
  }

  int canvas_pad = 8;
  int canvas_x = x + canvas_pad;
  int canvas_w = w - canvas_pad * 2;
  if (canvas_w < 16) canvas_w = 16;
  int box_x = x + (w - box_w) / 2;
  int top_y = y;
  int box_y = top_y + label_h + 4;

  char val[32];
  snprintf(val, sizeof(val), "w: %d%s", geom_w, representation ? " (repr)" : "");
  int tw = cogito_text_width_size_node(win, val, label_size);
  int wx = box_x + (box_w - tw) / 2;
  if (wx < canvas_x) wx = canvas_x;
  if (wx + tw > canvas_x + canvas_w) wx = canvas_x + canvas_w - tw;
  cogito_draw_text_size_node(win, wx, top_y, val, muted, label_size, false, false);

  CogitoColor box_bg = cogito_rgba(46, 204, 113, 50);
  CogitoColor box_border = cogito_rgba(22, 120, 66, 210);
  if (cogito_backend && cogito_backend->begin_scissor && cogito_backend->end_scissor) {
    cogito_backend->begin_scissor(canvas_x, box_y, canvas_w, box_h);
    cogito_draw_rect(box_x, box_y, box_w, box_h, box_bg, 0);
    cogito_draw_rect_lines(box_x, box_y, box_w, box_h, box_border, 0, 2);
    cogito_backend->end_scissor();
  } else {
    cogito_draw_rect(box_x, box_y, box_w, box_h, box_bg, 0);
    cogito_draw_rect_lines(box_x, box_y, box_w, box_h, box_border, 0, 2);
  }

  snprintf(val, sizeof(val), "h: %d", geom_h);
  int vh = cogito_text_height_size(label_size);
  int hw = cogito_text_width_size_node(win, val, label_size);
  int hx = x + w - hw - 2;
  int hy = box_y + (box_h - vh) / 2;
  cogito_draw_text_size_node(win, hx, hy, val, muted, label_size, false, false);

  int sy = box_y + box_h + 8;
  bool any = false;
  const char* states[4] = {"hover", "active", "focused", "disabled"};
  bool flags[4] = {hover, active, focused, disabled};
  for (int i = 0; i < 4; i++) {
    if (!flags[i]) continue;
    any = true;
    int sw = cogito_text_width_size_node(win, states[i], label_size);
    cogito_draw_text_size_node(win, x + (w - sw) / 2, sy, states[i], text, label_size, true, false);
    sy += label_h + 2;
  }
  if (!any) {
    const char* none = "no active state";
    int sw = cogito_text_width_size_node(win, none, label_size);
    cogito_draw_text_size_node(win, x + (w - sw) / 2, sy, none, muted, label_size, false, false);
    sy += label_h + 2;
  }
  return sy + 2;
}

static bool cogito_inspector_show_prop(CogitoNode* s, int prop_id) {
  if (!s) return false;
  CogitoKind k = s->kind;
  CogitoKind pk = s->parent ? s->parent->kind : (CogitoKind)-1;
  switch (prop_id) {
    case COGITO_INSPECTOR_PROP_TEXT:
      return k == COGITO_LABEL || k == COGITO_BUTTON || k == COGITO_CHECKBOX ||
             k == COGITO_CHIP || k == COGITO_BADGE || k == COGITO_TOAST ||
             k == COGITO_AVATAR || k == COGITO_EMPTY_PAGE || k == COGITO_TIP_VIEW ||
             k == COGITO_BANNER || k == COGITO_SETTINGS_ROW || k == COGITO_SETTINGS_PAGE ||
             k == COGITO_SETTINGS_WINDOW || k == COGITO_BUTTON_GROUP || k == COGITO_SPLIT_BUTTON ||
             k == COGITO_DROPDOWN || k == COGITO_TABS || k == COGITO_VIEWSWITCHER ||
             k == COGITO_TOOLBAR || k == COGITO_CONTENT_LIST || k == COGITO_BOTTOM_NAV ||
             k == COGITO_POPOVER || k == COGITO_CARD || k == COGITO_WELCOME_SCREEN ||
             k == COGITO_ABOUT_WINDOW || k == COGITO_DIALOG || k == COGITO_FAB ||
             k == COGITO_TEXTFIELD || k == COGITO_SEARCHFIELD || k == COGITO_TEXTVIEW ||
             k == COGITO_SWITCH || k == COGITO_NAV_RAIL;
    case COGITO_INSPECTOR_PROP_SUBTITLE:
      return k == COGITO_SETTINGS_ROW || k == COGITO_CONTENT_LIST || k == COGITO_TOAST ||
             k == COGITO_CARD || k == COGITO_BANNER || k == COGITO_DIALOG;
    case COGITO_INSPECTOR_PROP_HINT:
      return k == COGITO_TEXTFIELD || k == COGITO_SEARCHFIELD || k == COGITO_TEXTVIEW;
    case COGITO_INSPECTOR_PROP_ICON:
      return k == COGITO_BUTTON || k == COGITO_ICONBTN || k == COGITO_CHIP ||
             k == COGITO_FAB || k == COGITO_NAV_RAIL || k == COGITO_BOTTOM_NAV ||
             k == COGITO_TOOLBAR || k == COGITO_SEARCHFIELD || k == COGITO_TEXTFIELD ||
             k == COGITO_SPLIT_BUTTON || k == COGITO_BADGE || k == COGITO_AVATAR ||
             k == COGITO_EMPTY_PAGE || k == COGITO_SETTINGS_ROW || k == COGITO_CONTENT_LIST ||
             k == COGITO_APPBAR;
    case COGITO_INSPECTOR_PROP_TOOLTIP:
      return k != COGITO_WINDOW && k != COGITO_HSTACK && k != COGITO_VSTACK &&
             k != COGITO_GRID && k != COGITO_FIXED && k != COGITO_ZSTACK &&
             k != COGITO_SCROLLER && k != COGITO_BIN && k != COGITO_REVEALER &&
             k != COGITO_TOASTS && k != COGITO_TOAST;
    case COGITO_INSPECTOR_PROP_ACTION_TEXT:
      return k == COGITO_BUTTON || k == COGITO_DIALOG || k == COGITO_BANNER || k == COGITO_TOAST;
    case COGITO_INSPECTOR_PROP_FONT_FAMILY:
    case COGITO_INSPECTOR_PROP_TEXT_ALIGN:
    case COGITO_INSPECTOR_PROP_FONT_MONO:
    case COGITO_INSPECTOR_PROP_FONT_TABULAR:
      return k == COGITO_LABEL || k == COGITO_TEXTFIELD || k == COGITO_TEXTVIEW ||
             k == COGITO_SEARCHFIELD;
    case COGITO_INSPECTOR_PROP_TEXT_WRAP:
      return k == COGITO_LABEL || k == COGITO_TEXTVIEW || k == COGITO_BUTTON;
    case COGITO_INSPECTOR_PROP_TEXT_ELLIPSIS:
      return k == COGITO_LABEL || k == COGITO_TEXTFIELD || k == COGITO_SEARCHFIELD;
    case COGITO_INSPECTOR_PROP_LABEL_MAX_CHARS:
      return k == COGITO_LABEL || k == COGITO_CHIP || k == COGITO_BUTTON;
    case COGITO_INSPECTOR_PROP_AUTO_SIZE:
      return k == COGITO_LABEL || k == COGITO_IMAGE;
    case COGITO_INSPECTOR_PROP_GAP:
      return k == COGITO_HSTACK || k == COGITO_VSTACK || k == COGITO_GRID ||
             k == COGITO_TOOLBAR || k == COGITO_BOTTOM_NAV || k == COGITO_NAV_RAIL ||
             k == COGITO_APPBAR || k == COGITO_BUTTON_GROUP || k == COGITO_TABS ||
             k == COGITO_VIEWSWITCHER || k == COGITO_SWITCHBAR || k == COGITO_FAB_MENU;
    case COGITO_INSPECTOR_PROP_GRID_COL:
    case COGITO_INSPECTOR_PROP_GRID_ROW:
    case COGITO_INSPECTOR_PROP_GRID_SPAN_X:
    case COGITO_INSPECTOR_PROP_GRID_SPAN_Y:
    case COGITO_INSPECTOR_PROP_GRID_ALIGN_X:
    case COGITO_INSPECTOR_PROP_GRID_ALIGN_Y:
    case COGITO_INSPECTOR_PROP_GRID_POS_SET:
      return s->grid_pos_set || pk == COGITO_GRID;
    case COGITO_INSPECTOR_PROP_FIXED_POS_SET:
      return s->fixed_pos_set || pk == COGITO_FIXED || pk == COGITO_ZSTACK;
    case COGITO_INSPECTOR_PROP_CHECKED:
      return k == COGITO_CHECKBOX || k == COGITO_SWITCH || k == COGITO_CHIP;
    case COGITO_INSPECTOR_PROP_EDITABLE:
      return k == COGITO_TEXTFIELD || k == COGITO_TEXTVIEW || k == COGITO_SEARCHFIELD;
    case COGITO_INSPECTOR_PROP_RESIZABLE:
    case COGITO_INSPECTOR_PROP_RESIZE_MIN_W:
    case COGITO_INSPECTOR_PROP_RESIZE_MIN_H:
      return k == COGITO_WINDOW || k == COGITO_DIALOG;
    default:
      return true;
  }
}

static void cogito_draw_inspector_panel(CogitoNode* win) {
  if (!cogito_inspector_open || !win) return;
  cogito_inspector_rebuild_tree(win);
  if (!cogito_inspector_selected) cogito_inspector_selected = win;
  cogito_inspector_prop_rows_reset();

  int px = 0, py = 0, pw = 0, ph = 0, tree_w = 0;
  cogito_inspector_panel_rect(win, &px, &py, &pw, &ph, &tree_w);
  int header_h = 0;
  int row_h = 0;
  int pad = 0;
  cogito_inspector_tree_metrics(&header_h, &row_h, &pad);
  const int text_size = 13;

  CogitoColor bg = cogito_rgba(18, 21, 26, 255);
  CogitoColor panel = cogito_rgba(23, 26, 32, 255);
  CogitoColor border = cogito_rgba(75, 82, 95, 255);
  CogitoColor text = cogito_rgba(220, 226, 235, 255);
  CogitoColor muted = cogito_rgba(160, 170, 184, 255);
  CogitoColor selected_bg = cogito_rgba(45, 74, 124, 255);
  CogitoColor prop_row_bg = cogito_rgba(28, 33, 40, 255);
  CogitoColor prop_row_bg_alt = cogito_rgba(32, 37, 46, 255);
  CogitoColor prop_row_border = cogito_rgba(75, 82, 95, 128);
  CogitoColor tab_bg = cogito_rgba(32, 36, 44, 255);
  CogitoColor tab_active_bg = cogito_rgba(58, 88, 140, 255);

  int status_y = py + ph;
  int status_h = COGITO_INSPECTOR_STATUS_H;
  cogito_draw_rect(px, py, pw, ph + status_h, bg, 0);
  cogito_draw_rect(px, py, tree_w, ph + status_h, panel, 0);
  cogito_draw_line(px + tree_w, py, px + tree_w, py + ph + status_h, border);
  cogito_draw_line(px, py + header_h, px + pw, py + header_h, border);
  cogito_draw_rect(px, status_y, pw, status_h, prop_row_bg, 0);
  cogito_draw_line(px, status_y, px + pw, status_y, border);
  cogito_draw_text_size_node(win, px + pad, status_y + (status_h - cogito_text_height_size(12)) / 2, COGITO_USER_VERSION, muted, 12, false, false);

  cogito_draw_text_size_node(win, px + pad, py + header_h - 16, "Widget Tree", muted, 13, false, false);

  int rx = 0, rw = 0, tab_y = 0, tab_h = 0, tab0_x = 0, tab0_w = 0, tab1_x = 0, tab1_w = 0, content_y = 0, content_h = 0;
  cogito_inspector_right_panel_metrics(
    px, py, pw, ph, tree_w, pad,
    &rx, &rw, &tab_y, &tab_h,
    &tab0_x, &tab0_w, &tab1_x, &tab1_w,
    &content_y, &content_h
  );

  cogito_draw_text_size_node(win, px + tree_w + pad, py + header_h - 16, "Selected Node", muted, 13, false, false);
  cogito_draw_rect(tab0_x, tab_y, tab0_w, tab_h,
                   cogito_inspector_right_tab == COGITO_INSPECTOR_TAB_PROPERTIES ? tab_active_bg : tab_bg, 6);
  cogito_draw_rect(tab1_x, tab_y, tab1_w, tab_h,
                   cogito_inspector_right_tab == COGITO_INSPECTOR_TAB_DIAGRAM ? tab_active_bg : tab_bg, 6);
  cogito_draw_rect_lines(tab0_x, tab_y, tab0_w, tab_h, border, 6, 1);
  cogito_draw_rect_lines(tab1_x, tab_y, tab1_w, tab_h, border, 6, 1);
  cogito_draw_text_size_node(
    win,
    tab0_x + (tab0_w - cogito_text_width_size_node(win, "Properties", 12)) / 2,
    tab_y + (tab_h - cogito_text_height_size(12)) / 2,
    "Properties",
    cogito_inspector_right_tab == COGITO_INSPECTOR_TAB_PROPERTIES ? text : muted,
    12,
    false,
    false
  );
  cogito_draw_text_size_node(
    win,
    tab1_x + (tab1_w - cogito_text_width_size_node(win, "Diagram", 12)) / 2,
    tab_y + (tab_h - cogito_text_height_size(12)) / 2,
    "Diagram",
    cogito_inspector_right_tab == COGITO_INSPECTOR_TAB_DIAGRAM ? text : muted,
    12,
    false,
    false
  );

  int list_y = py + header_h + 4;
  int list_h = ph - (header_h + 8);
  if (list_h < 0) list_h = 0;
  int visible_rows = list_h / row_h + 2;
  if (visible_rows < 0) visible_rows = 0;
  int total_tree_h = cogito_inspector_node_count * row_h;
  int max_scroll = total_tree_h > list_h ? total_tree_h - list_h : 0;
  if (cogito_inspector_tree_scroll_y > max_scroll) cogito_inspector_tree_scroll_y = max_scroll;
  if (cogito_inspector_tree_scroll_y < 0) cogito_inspector_tree_scroll_y = 0;
  int first_row = cogito_inspector_tree_scroll_y / row_h;
  int y_offset = cogito_inspector_tree_scroll_y % row_h;
  for (int i = 0; i < visible_rows; i++) {
    int row_index = first_row + i;
    if (row_index >= cogito_inspector_node_count) break;
    int row_y = list_y - y_offset + i * row_h;
    if (row_y + row_h < list_y || row_y >= list_y + list_h) continue;
    CogitoNode* n = cogito_inspector_nodes[row_index];
    int depth = cogito_inspector_depths[row_index];
    if (n == cogito_inspector_selected) {
      cogito_draw_rect(px + 1, row_y, tree_w - 2, row_h, selected_bg, 0);
    }
    int indent_x = px + pad + depth * 12;
    int label_x = indent_x;
    if (n && n->len > 0) {
      bool collapsed = cogito_inspector_is_collapsed(n);
      int cx = indent_x + 4;
      int cy = row_y + row_h / 2;
      if (collapsed) {
        cogito_draw_line(cx - 1, cy - 3, cx + 2, cy, muted);
        cogito_draw_line(cx - 1, cy + 3, cx + 2, cy, muted);
      } else {
        cogito_draw_line(cx - 3, cy - 1, cx, cy + 2, muted);
        cogito_draw_line(cx + 3, cy - 1, cx, cy + 2, muted);
      }
      label_x = indent_x + 12;
    }
    char label[128];
    cogito_inspector_node_label(n, label, sizeof(label));
    if (label_x > px + tree_w - 60) label_x = px + tree_w - 60;
    if (cogito_backend && cogito_backend->begin_scissor) {
      cogito_backend->begin_scissor(px, row_y, tree_w - 10, row_h);
    }
    cogito_draw_text_size_node(win, label_x, row_y + 2, label, text, text_size, false, false);
    if (cogito_backend && cogito_backend->end_scissor) {
      cogito_backend->end_scissor();
    }
  }
  /* Vertical scrollbar for widget tree */
  if (max_scroll > 0 && list_h > 0) {
    int sb_x = px + tree_w - 8;
    int sb_w = 6;
    int thumb_h = list_h * list_h / total_tree_h;
    if (thumb_h < 20) thumb_h = 20;
    if (thumb_h > list_h) thumb_h = list_h;
    int thumb_y = list_y + (int)((long)(list_h - thumb_h) * (long)cogito_inspector_tree_scroll_y / (long)(max_scroll > 0 ? max_scroll : 1));
    if (thumb_y < list_y) thumb_y = list_y;
    if (thumb_y + thumb_h > list_y + list_h) thumb_y = list_y + list_h - thumb_h;
    cogito_draw_rect(sb_x, list_y, sb_w, list_h, prop_row_bg, 2);
    cogito_draw_rect(sb_x, thumb_y, sb_w, thumb_h, border, 2);
  }

  CogitoNode* s = cogito_inspector_selected;
  if (!s) return;
  if (cogito_backend && cogito_backend->begin_scissor) {
    cogito_backend->begin_scissor(rx, content_y, rw, content_h);
  }
  if (cogito_inspector_right_tab != COGITO_INSPECTOR_TAB_PROPERTIES) {
    cogito_inspector_props_scroll_y = 0;
    cogito_inspector_props_scroll_max = 0;
  }
  if (cogito_inspector_props_scroll_y > cogito_inspector_props_scroll_max) {
    cogito_inspector_props_scroll_y = cogito_inspector_props_scroll_max;
  }
  if (cogito_inspector_props_scroll_y < 0) {
    cogito_inspector_props_scroll_y = 0;
  }
  int content_origin_y = content_y;
  if (cogito_inspector_right_tab == COGITO_INSPECTOR_TAB_PROPERTIES) {
    content_origin_y = content_y - cogito_inspector_props_scroll_y;
  }
  int ry = content_origin_y;
  const int row_pad = 6;
  bool row_alt = false;

  char line[192];
  char value[192];
  char svalue[COGITO_INSPECTOR_EDIT_BUF_CAP + 8];
  char cbuf1[20], cbuf2[20], cbuf3[20], cbuf4[20];

  bool hover = false, active = false;
  if (!s->disabled && cogito_backend_ready && cogito_backend) {
    cogito_node_hover_state(s, &hover, &active);
  }
  bool focused = (s == cogito_focused);
  bool disabled = s->disabled;

  if (cogito_inspector_right_tab == COGITO_INSPECTOR_TAB_PROPERTIES) {
    cogito_inspector_node_label(s, line, sizeof(line));
    ry = cogito_inspector_draw_prop_kv_row(
      win, rx, ry, rw, "node", line, muted, text,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, 13, row_pad,
      COGITO_INSPECTOR_PROP_NONE
    );
    row_alt = !row_alt;

    snprintf(value, sizeof(value), "%s", cogito_kind_name(s->kind));
    ry = cogito_inspector_draw_prop_kv_row(
      win, rx, ry, rw, "kind", value, muted, text,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_NONE
    );
    row_alt = !row_alt;

    ry = cogito_inspector_draw_string_row(
      win, rx, ry, rw, "class", cogito_inspector_string_prop_for_row(s, COGITO_INSPECTOR_PROP_CLASS, svalue, sizeof(svalue)), muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_CLASS,
      cogito_inspector_edit_node == s && cogito_inspector_edit_prop == COGITO_INSPECTOR_PROP_CLASS
    );
    row_alt = !row_alt;

    ry = cogito_inspector_draw_string_row(
      win, rx, ry, rw, "view-id", cogito_inspector_string_prop_for_row(s, COGITO_INSPECTOR_PROP_VIEW_ID, svalue, sizeof(svalue)), muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_VIEW_ID,
      cogito_inspector_edit_node == s && cogito_inspector_edit_prop == COGITO_INSPECTOR_PROP_VIEW_ID
    );
    row_alt = !row_alt;

    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_TEXT)) {
      ry = cogito_inspector_draw_string_row(
        win, rx, ry, rw, "text", cogito_inspector_string_prop_for_row(s, COGITO_INSPECTOR_PROP_TEXT, svalue, sizeof(svalue)), muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_TEXT,
        cogito_inspector_edit_node == s && cogito_inspector_edit_prop == COGITO_INSPECTOR_PROP_TEXT
      );
      row_alt = !row_alt;
    }

    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_SUBTITLE)) {
      ry = cogito_inspector_draw_string_row(
        win, rx, ry, rw, "subtitle", cogito_inspector_string_prop_for_row(s, COGITO_INSPECTOR_PROP_SUBTITLE, svalue, sizeof(svalue)), muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_SUBTITLE,
        cogito_inspector_edit_node == s && cogito_inspector_edit_prop == COGITO_INSPECTOR_PROP_SUBTITLE
      );
      row_alt = !row_alt;
    }

    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_HINT)) {
      ry = cogito_inspector_draw_string_row(
        win, rx, ry, rw, "hint", cogito_inspector_string_prop_for_row(s, COGITO_INSPECTOR_PROP_HINT, svalue, sizeof(svalue)), muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_HINT,
        cogito_inspector_edit_node == s && cogito_inspector_edit_prop == COGITO_INSPECTOR_PROP_HINT
      );
      row_alt = !row_alt;
    }
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_ICON)) {
      ry = cogito_inspector_draw_string_row(
        win, rx, ry, rw, "icon", cogito_inspector_string_prop_for_row(s, COGITO_INSPECTOR_PROP_ICON, svalue, sizeof(svalue)), muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_ICON,
        cogito_inspector_edit_node == s && cogito_inspector_edit_prop == COGITO_INSPECTOR_PROP_ICON
      );
      row_alt = !row_alt;
    }
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_TOOLTIP)) {
      ry = cogito_inspector_draw_string_row(
        win, rx, ry, rw, "tooltip", cogito_inspector_string_prop_for_row(s, COGITO_INSPECTOR_PROP_TOOLTIP, svalue, sizeof(svalue)), muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_TOOLTIP,
        cogito_inspector_edit_node == s && cogito_inspector_edit_prop == COGITO_INSPECTOR_PROP_TOOLTIP
      );
      row_alt = !row_alt;
    }
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_ACTION_TEXT)) {
      ry = cogito_inspector_draw_string_row(
        win, rx, ry, rw, "action-text", cogito_inspector_string_prop_for_row(s, COGITO_INSPECTOR_PROP_ACTION_TEXT, svalue, sizeof(svalue)), muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_ACTION_TEXT,
        cogito_inspector_edit_node == s && cogito_inspector_edit_prop == COGITO_INSPECTOR_PROP_ACTION_TEXT
      );
      row_alt = !row_alt;
    }
    ry = cogito_inspector_draw_string_row(
      win, rx, ry, rw, "a11y-label", cogito_inspector_string_prop_for_row(s, COGITO_INSPECTOR_PROP_A11Y_LABEL, svalue, sizeof(svalue)), muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_A11Y_LABEL,
      cogito_inspector_edit_node == s && cogito_inspector_edit_prop == COGITO_INSPECTOR_PROP_A11Y_LABEL
    );
    row_alt = !row_alt;
    ry = cogito_inspector_draw_string_row(
      win, rx, ry, rw, "a11y-role", cogito_inspector_string_prop_for_row(s, COGITO_INSPECTOR_PROP_A11Y_ROLE, svalue, sizeof(svalue)), muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_A11Y_ROLE,
      cogito_inspector_edit_node == s && cogito_inspector_edit_prop == COGITO_INSPECTOR_PROP_A11Y_ROLE
    );
    row_alt = !row_alt;
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_FONT_FAMILY)) {
      ry = cogito_inspector_draw_string_row(
        win, rx, ry, rw, "font-family", cogito_inspector_string_prop_for_row(s, COGITO_INSPECTOR_PROP_FONT_FAMILY, svalue, sizeof(svalue)), muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_FONT_FAMILY,
        cogito_inspector_edit_node == s && cogito_inspector_edit_prop == COGITO_INSPECTOR_PROP_FONT_FAMILY
      );
      row_alt = !row_alt;
    }

    snprintf(value, sizeof(value), "%d", s->x);
    ry = cogito_inspector_draw_prop_kv_row(
      win, rx, ry, rw, "x", value, muted, text,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_NONE
    );
    row_alt = !row_alt;
    snprintf(value, sizeof(value), "%d", s->y);
    ry = cogito_inspector_draw_prop_kv_row(
      win, rx, ry, rw, "y", value, muted, text,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_NONE
    );
    row_alt = !row_alt;
    snprintf(value, sizeof(value), "%d", s->w);
    ry = cogito_inspector_draw_prop_kv_row(
      win, rx, ry, rw, "w", value, muted, text,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_NONE
    );
    row_alt = !row_alt;
    snprintf(value, sizeof(value), "%d", s->h);
    ry = cogito_inspector_draw_prop_kv_row(
      win, rx, ry, rw, "h", value, muted, text,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_NONE
    );
    row_alt = !row_alt;
  }

  cogito_inspector_color_hex(s->bg, cbuf1, sizeof(cbuf1));
  cogito_inspector_color_hex(s->text_color, cbuf2, sizeof(cbuf2));
  cogito_inspector_color_hex(s->border_color, cbuf3, sizeof(cbuf3));
  cogito_inspector_color_hex(s->selection_color, cbuf4, sizeof(cbuf4));

  if (cogito_inspector_right_tab == COGITO_INSPECTOR_TAB_PROPERTIES) {
    ry = cogito_inspector_draw_prop_kv_row(
      win, rx, ry, rw, "background", cbuf1, muted, text,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_NONE
    );
    row_alt = !row_alt;
    ry = cogito_inspector_draw_prop_kv_row(
      win, rx, ry, rw, "color", cbuf2, muted, text,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_NONE
    );
    row_alt = !row_alt;
    ry = cogito_inspector_draw_prop_kv_row(
      win, rx, ry, rw, "border-color", cbuf3, muted, text,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_NONE
    );
    row_alt = !row_alt;
    ry = cogito_inspector_draw_prop_kv_row(
      win, rx, ry, rw, "selection-color", cbuf4, muted, text,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_NONE
    );
    row_alt = !row_alt;

    snprintf(value, sizeof(value), "%d", s->border_width);
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "border-width", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_BORDER_WIDTH
    );
    row_alt = !row_alt;

    snprintf(value, sizeof(value), "%d", s->border_radius);
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "border-radius", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_BORDER_RADIUS
    );
    row_alt = !row_alt;

    snprintf(value, sizeof(value), "%.2f", s->opacity_set ? s->opacity : 1.0f);
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "opacity", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_OPACITY
    );
    row_alt = !row_alt;

    snprintf(value, sizeof(value), "%d", s->padding_top);
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "padding-top", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_PADDING_TOP
    );
    row_alt = !row_alt;
    snprintf(value, sizeof(value), "%d", s->padding_right);
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "padding-right", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_PADDING_RIGHT
    );
    row_alt = !row_alt;
    snprintf(value, sizeof(value), "%d", s->padding_bottom);
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "padding-bottom", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_PADDING_BOTTOM
    );
    row_alt = !row_alt;
    snprintf(value, sizeof(value), "%d", s->padding_left);
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "padding-left", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_PADDING_LEFT
    );
    row_alt = !row_alt;

    snprintf(value, sizeof(value), "%d", s->margin_top);
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "margin-top", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_MARGIN_TOP
    );
    row_alt = !row_alt;
    snprintf(value, sizeof(value), "%d", s->margin_right);
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "margin-right", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_MARGIN_RIGHT
    );
    row_alt = !row_alt;
    snprintf(value, sizeof(value), "%d", s->margin_bottom);
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "margin-bottom", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_MARGIN_BOTTOM
    );
    row_alt = !row_alt;
    snprintf(value, sizeof(value), "%d", s->margin_left);
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "margin-left", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_MARGIN_LEFT
    );
    row_alt = !row_alt;

    snprintf(value, sizeof(value), "%d", s->align);
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "align", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_ALIGN
    );
    row_alt = !row_alt;

    snprintf(value, sizeof(value), "%d", s->text_align);
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_TEXT_ALIGN)) {
      ry = cogito_inspector_draw_stepper_row(
        win, rx, ry, rw, "text-align", value, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_TEXT_ALIGN
      );
      row_alt = !row_alt;
    }

    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_LABEL_MAX_CHARS)) {
      if (s->label_max_width_chars_set) snprintf(value, sizeof(value), "%d", s->label_max_width_chars);
      else snprintf(value, sizeof(value), "unset");
      ry = cogito_inspector_draw_stepper_row(
        win, rx, ry, rw, "label-max-chars", value, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_LABEL_MAX_CHARS
      );
      row_alt = !row_alt;
    }

    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_GAP)) {
      snprintf(value, sizeof(value), "%d", s->gap);
      ry = cogito_inspector_draw_stepper_row(
        win, rx, ry, rw, "gap", value, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_GAP
      );
      row_alt = !row_alt;
    }

    if (s->min_w_set) snprintf(value, sizeof(value), "%d", s->min_w);
    else snprintf(value, sizeof(value), "unset");
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "min-w", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_MIN_W
    );
    row_alt = !row_alt;

    if (s->min_h_set) snprintf(value, sizeof(value), "%d", s->min_h);
    else snprintf(value, sizeof(value), "unset");
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "min-h", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_MIN_H
    );
    row_alt = !row_alt;

    if (s->max_w_set) snprintf(value, sizeof(value), "%d", s->max_w);
    else snprintf(value, sizeof(value), "unset");
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "max-w", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_MAX_W
    );
    row_alt = !row_alt;

    if (s->max_h_set) snprintf(value, sizeof(value), "%d", s->max_h);
    else snprintf(value, sizeof(value), "unset");
    ry = cogito_inspector_draw_stepper_row(
      win, rx, ry, rw, "max-h", value, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_MAX_H
    );
    row_alt = !row_alt;

    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_GRID_COL)) {
      snprintf(value, sizeof(value), "%d", s->grid_col);
      ry = cogito_inspector_draw_stepper_row(
        win, rx, ry, rw, "grid-col", value, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_GRID_COL
      );
      row_alt = !row_alt;
      snprintf(value, sizeof(value), "%d", s->grid_row);
      ry = cogito_inspector_draw_stepper_row(
        win, rx, ry, rw, "grid-row", value, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_GRID_ROW
      );
      row_alt = !row_alt;
      snprintf(value, sizeof(value), "%d", s->grid_span_x);
      ry = cogito_inspector_draw_stepper_row(
        win, rx, ry, rw, "grid-span-x", value, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_GRID_SPAN_X
      );
      row_alt = !row_alt;
      snprintf(value, sizeof(value), "%d", s->grid_span_y);
      ry = cogito_inspector_draw_stepper_row(
        win, rx, ry, rw, "grid-span-y", value, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_GRID_SPAN_Y
      );
      row_alt = !row_alt;
      snprintf(value, sizeof(value), "%d", s->grid_align_x);
      ry = cogito_inspector_draw_stepper_row(
        win, rx, ry, rw, "grid-align-x", value, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_GRID_ALIGN_X
      );
      row_alt = !row_alt;
      snprintf(value, sizeof(value), "%d", s->grid_align_y);
      ry = cogito_inspector_draw_stepper_row(
        win, rx, ry, rw, "grid-align-y", value, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_GRID_ALIGN_Y
      );
      row_alt = !row_alt;
    }
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_RESIZE_MIN_W)) {
      snprintf(value, sizeof(value), "%d", s->resize_min_w);
      ry = cogito_inspector_draw_stepper_row(
        win, rx, ry, rw, "resize-min-w", value, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_RESIZE_MIN_W
      );
      row_alt = !row_alt;
      snprintf(value, sizeof(value), "%d", s->resize_min_h);
      ry = cogito_inspector_draw_stepper_row(
        win, rx, ry, rw, "resize-min-h", value, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_RESIZE_MIN_H
      );
      row_alt = !row_alt;
    }

    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_CHECKED)) {
      ry = cogito_inspector_draw_switch_row(
        win, rx, ry, rw, "checked", s->checked, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_CHECKED
      );
      row_alt = !row_alt;
    }

    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_EDITABLE)) {
      ry = cogito_inspector_draw_switch_row(
        win, rx, ry, rw, "editable", s->editable, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_EDITABLE
      );
      row_alt = !row_alt;
    }
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_RESIZABLE)) {
      ry = cogito_inspector_draw_switch_row(
        win, rx, ry, rw, "resizable", s->resizable, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_RESIZABLE
      );
      row_alt = !row_alt;
    }
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_TEXT_WRAP)) {
      ry = cogito_inspector_draw_switch_row(
        win, rx, ry, rw, "text-wrap", s->text_wrap, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_TEXT_WRAP
      );
      row_alt = !row_alt;
    }
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_TEXT_ELLIPSIS)) {
      ry = cogito_inspector_draw_switch_row(
        win, rx, ry, rw, "text-ellipsis", s->text_ellipsis, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_TEXT_ELLIPSIS
      );
      row_alt = !row_alt;
    }
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_FONT_MONO)) {
      ry = cogito_inspector_draw_switch_row(
        win, rx, ry, rw, "font-mono", s->font_mono, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_FONT_MONO
      );
      row_alt = !row_alt;
    }
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_FONT_TABULAR)) {
      ry = cogito_inspector_draw_switch_row(
        win, rx, ry, rw, "font-tabular", s->font_tabular, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_FONT_TABULAR
      );
      row_alt = !row_alt;
    }
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_AUTO_SIZE)) {
      ry = cogito_inspector_draw_switch_row(
        win, rx, ry, rw, "auto-size", s->auto_size, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_AUTO_SIZE
      );
      row_alt = !row_alt;
    }
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_GRID_POS_SET)) {
      ry = cogito_inspector_draw_switch_row(
        win, rx, ry, rw, "grid-pos-set", s->grid_pos_set, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_GRID_POS_SET
      );
      row_alt = !row_alt;
    }
    if (cogito_inspector_show_prop(s, COGITO_INSPECTOR_PROP_FIXED_POS_SET)) {
      ry = cogito_inspector_draw_switch_row(
        win, rx, ry, rw, "fixed-pos-set", s->fixed_pos_set, muted,
        row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
        COGITO_INSPECTOR_PROP_FIXED_POS_SET
      );
      row_alt = !row_alt;
    }
    ry = cogito_inspector_draw_switch_row(
      win, rx, ry, rw, "hexpand-set", s->hexpand_set, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_HEXPAND_SET
    );
    row_alt = !row_alt;
    ry = cogito_inspector_draw_switch_row(
      win, rx, ry, rw, "vexpand-set", s->vexpand_set, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_VEXPAND_SET
    );
    row_alt = !row_alt;

    ry = cogito_inspector_draw_switch_row(
      win, rx, ry, rw, "hexpand", s->hexpand, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_HEXPAND
    );
    row_alt = !row_alt;

    ry = cogito_inspector_draw_switch_row(
      win, rx, ry, rw, "vexpand", s->vexpand, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_VEXPAND
    );
    row_alt = !row_alt;

    ry = cogito_inspector_draw_switch_row(
      win, rx, ry, rw, "hidden", s->hidden, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_HIDDEN
    );
    row_alt = !row_alt;

    ry = cogito_inspector_draw_switch_row(
      win, rx, ry, rw, "disabled", s->disabled, muted,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad,
      COGITO_INSPECTOR_PROP_DISABLED
    );

    ry += 4;
    cogito_draw_text_size_node(win, rx + 2, ry, "click text field: edit string  click [-][+]: adjust number  click switch: toggle", muted, 11, false, false);

    int total_h = ry - content_origin_y;
    if (total_h < 0) total_h = 0;
    int max_scroll = total_h > content_h ? total_h - content_h : 0;
    cogito_inspector_props_scroll_max = max_scroll;
    if (cogito_inspector_props_scroll_y > max_scroll) {
      cogito_inspector_props_scroll_y = max_scroll;
      cogito_queue_draw(win);
    }
  } else {
    cogito_inspector_node_label(s, line, sizeof(line));
    ry = cogito_inspector_draw_prop_kv_row(
      win, rx, ry, rw, "node", line, muted, text,
      row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, 13, row_pad,
      COGITO_INSPECTOR_PROP_NONE
    );
    ry += 6;
    (void)cogito_inspector_draw_geometry_state_widget(
      win, rx, ry, rw,
      s->w, s->h,
      hover, active, focused, disabled,
      text, muted
    );
  }

  if (cogito_backend && cogito_backend->end_scissor) {
    cogito_backend->end_scissor();
  }

  if (cogito_inspector_right_tab == COGITO_INSPECTOR_TAB_PROPERTIES && content_h > 0 && cogito_inspector_props_scroll_max > 0) {
    int sb_w = 6;
    int sb_x = rx + rw - sb_w - 2;
    int thumb_h = content_h * content_h / (content_h + cogito_inspector_props_scroll_max);
    if (thumb_h < 20) thumb_h = 20;
    if (thumb_h > content_h) thumb_h = content_h;
    int thumb_y = content_y + (int)((long)(content_h - thumb_h) * (long)cogito_inspector_props_scroll_y /
                                     (long)(cogito_inspector_props_scroll_max > 0 ? cogito_inspector_props_scroll_max : 1));
    if (thumb_y < content_y) thumb_y = content_y;
    if (thumb_y + thumb_h > content_y + content_h) thumb_y = content_y + content_h - thumb_h;
    cogito_draw_rect(sb_x, content_y, sb_w, content_h, prop_row_bg, 2);
    cogito_draw_rect(sb_x, thumb_y, sb_w, thumb_h, border, 2);
  }
}
