static void cogito_style_set_padding(CogitoStyle* s, int top, int right, int bottom, int left) {
  s->has_padding = true;
  s->padding_top = top;
  s->padding_right = right;
  s->padding_bottom = bottom;
  s->padding_left = left;
}

static void cogito_style_set_margin(CogitoStyle* s, int top, int right, int bottom, int left) {
  s->has_margin = true;
  s->margin_top = top;
  s->margin_right = right;
  s->margin_bottom = bottom;
  s->margin_left = left;
}

static void cogito_style_set_radius(CogitoStyle* s, int tl, int tr, int br, int bl) {
  s->has_radius_tl = true;
  s->has_radius_tr = true;
  s->has_radius_br = true;
  s->has_radius_bl = true;
  s->radius_tl = tl;
  s->radius_tr = tr;
  s->radius_br = br;
  s->radius_bl = bl;
}

static int cogito_label_class_from_name(const char* s) {
  if (!s || !s[
    0
  ]) return COGITO_LABEL_CLASS_NONE;
  char key[
    64
  ];
  size_t k = 0;
  for (size_t i = 0; s[i
  ] && (k + 1) < sizeof(key); i++) {
    unsigned char ch = (unsigned char)s[i
    ];
    if (ch == '_' || ch == ' ') ch = '-';
    ch = (unsigned char)tolower(ch);
    if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-') {
      key[k++
      ] = (char)ch;
    }
  }
  key[k
  ] = 0;
  if (!key[
    0
  ]) return COGITO_LABEL_CLASS_NONE;

  if (strcmp(key,
  "title") == 0) return COGITO_LABEL_CLASS_TITLE;
  if (strcmp(key,
  "subtitle") == 0 || strcmp(key,
  "sub-title") == 0) return COGITO_LABEL_CLASS_SUBTITLE;
  if (strcmp(key,
  "body") == 0) return COGITO_LABEL_CLASS_BODY;
  if (strcmp(key,
  "big-display") == 0 || strcmp(key,
  "bigdisplay") == 0) return COGITO_LABEL_CLASS_BIG_DISPLAY;
  if (strcmp(key,
  "display") == 0) return COGITO_LABEL_CLASS_DISPLAY;
  if (strcmp(key,
  "view-title") == 0 || strcmp(key,
  "viewtitle") == 0) return COGITO_LABEL_CLASS_VIEW_TITLE;
  if (strcmp(key,
  "view-subtitle") == 0 || strcmp(key,
  "viewsubtitle") == 0) return COGITO_LABEL_CLASS_VIEW_SUBTITLE;
  if (strcmp(key,
  "heading") == 0) return COGITO_LABEL_CLASS_HEADING;
  if (strcmp(key,
  "content-title") == 0 || strcmp(key,
  "contenttitle") == 0) return COGITO_LABEL_CLASS_CONTENT_TITLE;
  if (strcmp(key,
  "content-subtitle") == 0 || strcmp(key,
  "contentsubtitle") == 0) return COGITO_LABEL_CLASS_CONTENT_SUBTITLE;
  if (strcmp(key,
  "caption") == 0) return COGITO_LABEL_CLASS_CAPTION;
  return COGITO_LABEL_CLASS_NONE;
}

static void cogito_theme_set_defaults(void) {
  cogito_style_clear(&cogito_theme.base);
  for (int i = 0; i < COGITO_KIND_COUNT; i++) {
    cogito_style_clear(&cogito_theme.per_kind[i
    ]);
    cogito_style_clear(&cogito_theme.per_kind_hover[i
    ]);
    cogito_style_clear(&cogito_theme.per_kind_active[i
    ]);
    cogito_style_clear(&cogito_theme.per_kind_focused[i
    ]);
    cogito_style_clear(&cogito_theme.per_kind_checked[i
    ]);
    cogito_style_clear(&cogito_theme.per_kind_disabled[i
    ]);
  }
  for (int i = 0; i < COGITO_LABEL_CLASS_COUNT; i++) {
    cogito_style_clear(&cogito_theme.label_class[i
    ]);
    cogito_style_clear(&cogito_theme.label_class_hover[i
    ]);
    cogito_style_clear(&cogito_theme.label_class_active[i
    ]);
    cogito_style_clear(&cogito_theme.label_class_focused[i
    ]);
    cogito_style_clear(&cogito_theme.label_class_checked[i
    ]);
    cogito_style_clear(&cogito_theme.label_class_disabled[i
    ]);
  }
  cogito_style_clear(&cogito_theme.class_mono);
  cogito_style_clear(&cogito_theme.class_mono_hover);
  cogito_style_clear(&cogito_theme.class_mono_active);
  cogito_style_clear(&cogito_theme.class_mono_focused);
  cogito_style_clear(&cogito_theme.class_mono_checked);
  cogito_style_clear(&cogito_theme.class_mono_disabled);
  cogito_style_clear(&cogito_theme.class_tabular);
  cogito_style_clear(&cogito_theme.class_tabular_hover);
  cogito_style_clear(&cogito_theme.class_tabular_active);
  cogito_style_clear(&cogito_theme.class_tabular_focused);
  cogito_style_clear(&cogito_theme.class_tabular_checked);
  cogito_style_clear(&cogito_theme.class_tabular_disabled);
  cogito_style_clear(&cogito_theme.menu);
  cogito_style_clear(&cogito_theme.menu_item);
  cogito_style_clear(&cogito_theme.appbar_child_button);
  cogito_style_clear(&cogito_theme.appbar_child_button_hover);
  cogito_style_clear(&cogito_theme.appbar_child_button_active);
  cogito_style_clear(&cogito_theme.appbar_child_iconbtn);
  cogito_style_clear(&cogito_theme.appbar_child_iconbtn_hover);
  cogito_style_clear(&cogito_theme.appbar_child_iconbtn_active);
  cogito_style_clear(&cogito_theme.appbar_win_btn_all);
  cogito_style_clear(&cogito_theme.appbar_win_btn_hover_all);
  cogito_style_clear(&cogito_theme.appbar_win_btn_active_all);
  for (int i = 0; i < 3; i++) {
    cogito_style_clear(&cogito_theme.appbar_win_btn[i
    ]);
    cogito_style_clear(&cogito_theme.appbar_win_btn_hover[i
    ]);
    cogito_style_clear(&cogito_theme.appbar_win_btn_active[i
    ]);
  }
  cogito_style_clear(&cogito_theme.checkbox_check);
  cogito_style_clear(&cogito_theme.checkbox_check_hover);
  cogito_style_clear(&cogito_theme.checkbox_check_active);
  cogito_style_clear(&cogito_theme.checkbox_check_checked);
  cogito_style_clear(&cogito_theme.radio_check);
  cogito_style_clear(&cogito_theme.radio_check_hover);
  cogito_style_clear(&cogito_theme.radio_check_active);
  cogito_style_clear(&cogito_theme.radio_check_checked);
  cogito_style_clear(&cogito_theme.checkbox_box);
  cogito_style_clear(&cogito_theme.checkbox_box_hover);
  cogito_style_clear(&cogito_theme.checkbox_box_active);
  cogito_style_clear(&cogito_theme.checkbox_box_checked);
  cogito_style_clear(&cogito_theme.radio_box);
  cogito_style_clear(&cogito_theme.radio_box_hover);
  cogito_style_clear(&cogito_theme.radio_box_active);
  cogito_style_clear(&cogito_theme.radio_box_checked);
  cogito_style_clear(&cogito_theme.switch_track);
  cogito_style_clear(&cogito_theme.switch_track_hover);
  cogito_style_clear(&cogito_theme.switch_track_active);
  cogito_style_clear(&cogito_theme.switch_track_checked);
  cogito_style_clear(&cogito_theme.switch_knob);
  cogito_style_clear(&cogito_theme.switch_knob_hover);
  cogito_style_clear(&cogito_theme.switch_knob_active);
  cogito_style_clear(&cogito_theme.switch_knob_checked);
  cogito_style_clear(&cogito_theme.content_list_item_first);
  cogito_style_clear(&cogito_theme.content_list_item_last);
  cogito_style_clear(&cogito_theme.content_list_item_middle);
  cogito_style_clear(&cogito_theme.content_list_item_single);
  cogito_style_clear(&cogito_theme.content_list_label);
}

static void cogito_theme_init(void) {
  if (!cogito_theme_initialized) {
    // During widget construction (before run), avoid the heavy dynamic theme
    // build. Initialize an empty baseline and defer full theme materialization
    // until a window is attached.
    if (!cogito_active_window) {
      cogito_theme_set_defaults();
      cogito_theme_initialized = true;
      cogito_theme_refresh_deferred = true;
      return;
    }
    // Use the layered theme pipeline (default sum + dynamic + optional user sum)
    // instead of directly loading a hardcoded file path.
    CogitoColor accent = cogito_theme_accent;
    if (accent.a == 0) accent = cogito_rgba(114,
    222,
    194,
    255);
    cogito_apply_theme(cogito_theme_dark, accent);
  }
}
// Optimized version that writes to output pointer to avoid large struct copies
static void cogito_style_merge_into(const CogitoStyle* base, const CogitoStyle* over, CogitoStyle* out) {
  *out = *base;
  if (over->has_bg) { out->has_bg = true; out->bg = over->bg;
  }
  if (over->has_text) { out->has_text = true; out->text = over->text;
  }
  if (over->has_border) { out->has_border = true; out->border = over->border;
  }
  if (over->has_border_width) { out->has_border_width = true; out->border_width = over->border_width;
  }
  if (over->has_radius) { out->has_radius = true; out->radius = over->radius;
  }
  if (over->has_selection) { out->has_selection = true; out->selection = over->selection;
  }
  if (over->has_font_size) { out->has_font_size = true; out->font_size = over->font_size;
  }
  if (over->has_track) { out->has_track = true; out->track = over->track;
  }
  if (over->has_track_on) { out->has_track_on = true; out->track_on = over->track_on;
  }
  if (over->has_knob) { out->has_knob = true; out->knob = over->knob;
  }
  if (over->has_check) { out->has_check = true; out->check = over->check;
  }
  if (over->has_font_weight) { out->has_font_weight = true; out->font_weight = over->font_weight;
  }
  if (over->has_letter_spacing) { out->has_letter_spacing = true; out->letter_spacing = over->letter_spacing;
  }
  if (over->has_font_tabular) { out->has_font_tabular = true; out->font_tabular = over->font_tabular;
  }
  if (over->has_font_mono) { out->has_font_mono = true; out->font_mono = over->font_mono;
  }
  if (over->has_min_w) { out->has_min_w = true; out->min_w = over->min_w;
  }
  if (over->has_min_h) { out->has_min_h = true; out->min_h = over->min_h;
  }
  if (over->has_max_w) { out->has_max_w = true; out->max_w = over->max_w;
  }
  if (over->has_max_h) { out->has_max_h = true; out->max_h = over->max_h;
  }
  if (over->has_icon_size) { out->has_icon_size = true; out->icon_size = over->icon_size;
  }
  if (over->has_icon_color) { out->has_icon_color = true; out->icon_color = over->icon_color;
  }
  if (over->has_transition_ms) { out->has_transition_ms = true; out->transition_ms = over->transition_ms;
  }
  if (over->has_transition_ease) { out->has_transition_ease = true; out->transition_ease = over->transition_ease;
  }
  if (over->has_appbar_btn_size) { out->has_appbar_btn_size = true; out->appbar_btn_size = over->appbar_btn_size;
  }
  if (over->has_appbar_btn_gap) { out->has_appbar_btn_gap = true; out->appbar_btn_gap = over->appbar_btn_gap;
  }
  if (over->has_appbar_btn_pad_top) { out->has_appbar_btn_pad_top = true; out->appbar_btn_pad_top = over->appbar_btn_pad_top;
  }
  if (over->has_appbar_btn_pad_right) { out->has_appbar_btn_pad_right = true; out->appbar_btn_pad_right = over->appbar_btn_pad_right;
  }
  if (over->has_appbar_btn_close_color) { out->has_appbar_btn_close_color = true; out->appbar_btn_close_color = over->appbar_btn_close_color;
  }
  if (over->has_appbar_btn_min_color) { out->has_appbar_btn_min_color = true; out->appbar_btn_min_color = over->appbar_btn_min_color;
  }
  if (over->has_appbar_btn_max_color) { out->has_appbar_btn_max_color = true; out->appbar_btn_max_color = over->appbar_btn_max_color;
  }
  if (over->has_appbar_btn_border_color) { out->has_appbar_btn_border_color = true; out->appbar_btn_border_color = over->appbar_btn_border_color;
  }
  if (over->has_appbar_btn_border_width) { out->has_appbar_btn_border_width = true; out->appbar_btn_border_width = over->appbar_btn_border_width;
  }

  if (over->has_border_style) { out->has_border_style = true; out->border_style = over->border_style;
  }
  if (over->has_radius_tl) { out->has_radius_tl = true; out->radius_tl = over->radius_tl;
  }
  if (over->has_radius_tr) { out->has_radius_tr = true; out->radius_tr = over->radius_tr;
  }
  if (over->has_radius_br) { out->has_radius_br = true; out->radius_br = over->radius_br;
  }
  if (over->has_radius_bl) { out->has_radius_bl = true; out->radius_bl = over->radius_bl;
  }
  if (over->has_font_family) {
    out->has_font_family = true;
    strncpy(out->font_family, over->font_family,
    63);
    out->font_family[
      63
    ] = '\0';
  }
  if (over->has_box_shadow) {
    out->has_box_shadow = true;
    out->box_shadow = over->box_shadow;
  }

  if (over->has_padding) {
    out->has_padding = true;
    out->padding_left = over->padding_left;
    out->padding_top = over->padding_top;
    out->padding_right = over->padding_right;
    out->padding_bottom = over->padding_bottom;
  }
  if (over->has_padding_left) { out->has_padding_left = true; out->padding_left = over->padding_left;
  }
  if (over->has_padding_top) { out->has_padding_top = true; out->padding_top = over->padding_top;
  }
  if (over->has_padding_right) { out->has_padding_right = true; out->padding_right = over->padding_right;
  }
  if (over->has_padding_bottom) { out->has_padding_bottom = true; out->padding_bottom = over->padding_bottom;
  }

  if (over->has_margin) {
    out->has_margin = true;
    out->margin_left = over->margin_left;
    out->margin_top = over->margin_top;
    out->margin_right = over->margin_right;
    out->margin_bottom = over->margin_bottom;
  }
  if (over->has_margin_left) { out->has_margin_left = true; out->margin_left = over->margin_left;
  }
  if (over->has_margin_top) { out->has_margin_top = true; out->margin_top = over->margin_top;
  }
  if (over->has_margin_right) { out->has_margin_right = true; out->margin_right = over->margin_right;
  }
  if (over->has_margin_bottom) { out->has_margin_bottom = true; out->margin_bottom = over->margin_bottom;
  }
  if (over->has_opacity) { out->has_opacity = true; out->opacity = over->opacity;
  }
  if (over->has_track_height) { out->has_track_height = true; out->track_height = over->track_height;
  }
  if (over->has_divider) { out->has_divider = true; out->divider = over->divider;
  }
  if (over->has_item_gap) { out->has_item_gap = true; out->item_gap = over->item_gap;
  }
}
// Legacy wrapper for compatibility
static CogitoStyle cogito_style_merge(const CogitoStyle* base, const CogitoStyle* over) {
  CogitoStyle out;
  cogito_style_merge_into(base, over, &out);
  return out;
}

static CogitoStyle cogito_theme_resolve(CogitoKind kind) {
  return cogito_style_merge(&cogito_theme.base, &cogito_theme.per_kind[kind
  ]);
}

static CogitoStyle cogito_theme_resolve_node(const CogitoNode* n) {
  CogitoStyle s = cogito_theme_resolve(n->kind);
  if (n->kind == COGITO_LABEL &&
      n->label.cls > COGITO_LABEL_CLASS_NONE &&
      n->label.cls < COGITO_LABEL_CLASS_COUNT) {
    s = cogito_style_merge(&s, &cogito_theme.label_class[n->label.cls
    ]);
  }
  if (n->font_mono) {
    s = cogito_style_merge(&s, &cogito_theme.class_mono);
  }
  if (n->font_tabular) {
    s = cogito_style_merge(&s, &cogito_theme.class_tabular);
  }
  if (n->kind == COGITO_BUTTON) {
    if (n->button.outlined) {
      s = cogito_style_merge(&s, &cogito_theme.button_outlined);
    }
    if (n->button.text_style) {
      s = cogito_style_merge(&s, &cogito_theme.button_text);
    }
  } else if (n->kind == COGITO_ICONBTN) {
    if (n->button.outlined) {
      s = cogito_style_merge(&s, &cogito_theme.iconbtn_outlined);
    }
  }
  if (n->parent && n->parent->kind == COGITO_APPBAR) {
    if (n->kind == COGITO_BUTTON) {
      s = cogito_style_merge(&s, &cogito_theme.appbar_child_button);
    } else if (n->kind == COGITO_ICONBTN) {
      s = cogito_style_merge(&s, &cogito_theme.appbar_child_iconbtn);
    }
  }
  if (n->parent && n->parent->kind == COGITO_CONTENT_LIST && n->kind == COGITO_LABEL) {
    s = cogito_style_merge(&s, &cogito_theme.content_list_label);
  }
  // Apply custom class styles if class_name is set
  if (n->class_name && n->class_name->data && n->class_name->len > 0) {
    // Preserve window background color - custom classes should not override it
    bool is_window = (n->kind == COGITO_WINDOW);
    CogitoColor saved_bg = s.bg;
    bool saved_has_bg = s.has_bg;
    for (int i = 0; i < cogito_theme.custom_class_count; i++) {
      // Match class name and widget kind; COGITO_KIND_COUNT means universal (any kind)
      if (strcmp(cogito_theme.custom_classes[i
      ].name, n->class_name->data) == 0 &&
          (cogito_theme.custom_classes[i
      ].kind == n->kind ||
           cogito_theme.custom_classes[i
      ].kind == COGITO_KIND_COUNT)) {
        s = cogito_style_merge(&s, &cogito_theme.custom_classes[i
        ].style);
      }
    }
    // Restore window background if it was overridden by custom class
    if (is_window && saved_has_bg) {
      s.bg = saved_bg;
      s.has_bg = saved_has_bg;
    }
  }
  return s;
}

static CogitoStyle cogito_checkbox_check_style(bool is_radio, bool checked, bool hover, bool active) {
  CogitoStyle base = is_radio ? cogito_theme.radio_check : cogito_theme.checkbox_check;
  if (checked) {
    CogitoStyle over = is_radio ? cogito_theme.radio_check_checked : cogito_theme.checkbox_check_checked;
    base = cogito_style_merge(&base, &over);
  }
  if (active) {
    CogitoStyle over = is_radio ? cogito_theme.radio_check_active : cogito_theme.checkbox_check_active;
    return cogito_style_merge(&base, &over);
  }
  if (hover) {
    CogitoStyle over = is_radio ? cogito_theme.radio_check_hover : cogito_theme.checkbox_check_hover;
    return cogito_style_merge(&base, &over);
  }
  return base;
}

static CogitoStyle cogito_checkbox_box_style(bool is_radio, bool checked, bool hover, bool active) {
  CogitoStyle base = is_radio ? cogito_theme.radio_box : cogito_theme.checkbox_box;
  if (checked) {
    CogitoStyle over = is_radio ? cogito_theme.radio_box_checked : cogito_theme.checkbox_box_checked;
    base = cogito_style_merge(&base, &over);
  }
  if (active) {
    CogitoStyle over = is_radio ? cogito_theme.radio_box_active : cogito_theme.checkbox_box_active;
    return cogito_style_merge(&base, &over);
  }
  if (hover) {
    CogitoStyle over = is_radio ? cogito_theme.radio_box_hover : cogito_theme.checkbox_box_hover;
    return cogito_style_merge(&base, &over);
  }
  return base;
}

static CogitoStyle cogito_switch_track_style(bool checked, bool hover, bool active) {
  CogitoStyle base = cogito_theme.switch_track;
  if (checked) {
    base = cogito_style_merge(&base, &cogito_theme.switch_track_checked);
  }
  if (active) {
    return cogito_style_merge(&base, &cogito_theme.switch_track_active);
  }
  if (hover) {
    return cogito_style_merge(&base, &cogito_theme.switch_track_hover);
  }
  return base;
}

static CogitoStyle cogito_switch_knob_style(bool checked, bool hover, bool active) {
  CogitoStyle base = cogito_theme.switch_knob;
  if (checked) {
    base = cogito_style_merge(&base, &cogito_theme.switch_knob_checked);
  }
  if (active) {
    return cogito_style_merge(&base, &cogito_theme.switch_knob_active);
  }
  if (hover) {
    return cogito_style_merge(&base, &cogito_theme.switch_knob_hover);
  }
  return base;
}

static void cogito_node_hover_state(CogitoNode* n, bool* out_hover, bool* out_active) {
  if (!out_hover || !out_active) return;
  *out_hover = false;
  *out_active = false;
  if (!n) return;
  if (n->disabled) return;
  double now = cogito_now();
  double latch_seconds = (double)COGITO_ACTIVE_LATCH_MS / 1000.0;
  if (cogito_pointer_capture) {
    if (cogito_pointer_capture != n) return;
    *out_hover = true;
    bool down = cogito_backend && cogito_backend->is_mouse_button_down && cogito_backend->is_mouse_button_down(0);
    bool pressed = cogito_backend && cogito_backend->is_mouse_button_pressed && cogito_backend->is_mouse_button_pressed(0);
    if (down || pressed) n->active_latch_until = now + latch_seconds;
    *out_active = down || (n->active_latch_until > now);
    return;
  }
  int mx = 0, my = 0;
  bool button_down = false;
  bool button_pressed = false;
  bool have_mouse = cogito_pointer_query(&mx, &my, &button_down, &button_pressed);
  if (!have_mouse) return;
  bool hover = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
  bool down = hover && button_down;
  bool pressed = hover && button_pressed;
  if (down || pressed) n->active_latch_until = now + latch_seconds;
  bool active = hover && (down || (n->active_latch_until > now));
  *out_hover = hover;
  *out_active = active;
}

static bool cogito_node_disabled(const CogitoNode* n);
static void cogito_toast_close_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h);

static CogitoStyle cogito_theme_resolve_node_state(const CogitoNode* n, bool hover, bool active) {
  CogitoStyle s = cogito_theme_resolve_node(n);
  bool checked = false;
  bool disabled = cogito_node_disabled(n);
  bool focused = (!disabled && n && n == cogito_focused && cogito_focus_visible);
  if (n && (n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH)) checked = n->checked;
  else if (n && n->kind == COGITO_CHIP) checked = n->chip.selected;
  // Apply iconbtn color style base BEFORE per_kind hover/active so color style wins
  bool iconbtn_has_custom_class = false;
  bool iconbtn_class_is_filled = false;
  bool iconbtn_class_is_tonal = false;
  bool iconbtn_class_is_outlined = false;
  bool iconbtn_class_is_iconic = false;
  if (n->class_name && n->class_name->data && n->class_name->data[
    0
  ]) {
    // Check if the class is one of the built-in icon button styles
    const char* class_name = n->class_name->data;
    iconbtn_class_is_filled = strcmp(class_name, "filled") == 0;
    iconbtn_class_is_tonal = strcmp(class_name, "tonal") == 0;
    iconbtn_class_is_outlined = strcmp(class_name, "outlined") == 0;
    iconbtn_class_is_iconic = strcmp(class_name, "iconic") == 0;
    iconbtn_has_custom_class = !iconbtn_class_is_filled && !iconbtn_class_is_tonal && 
                               !iconbtn_class_is_outlined && !iconbtn_class_is_iconic;
  }
  if (n->kind == COGITO_ICONBTN && !iconbtn_has_custom_class) {
    // Determine style: class name takes precedence over icon_color_style
    // If outlined property is set, always use outlined style
    if (n->button.outlined || iconbtn_class_is_outlined) {
      s = cogito_style_merge(&s, &cogito_theme.iconbtn_outlined);
      if (!disabled) {
        if (active) s = cogito_style_merge(&s, &cogito_theme.iconbtn_outlined_active);
        else if (hover) s = cogito_style_merge(&s, &cogito_theme.iconbtn_outlined_hover);
      }
    } else if (iconbtn_class_is_filled) {
      s = cogito_style_merge(&s, &cogito_theme.iconbtn_filled);
      if (!disabled) {
        if (active) s = cogito_style_merge(&s, &cogito_theme.iconbtn_filled_active);
        else if (hover) s = cogito_style_merge(&s, &cogito_theme.iconbtn_filled_hover);
      }
    } else if (iconbtn_class_is_tonal) {
      s = cogito_style_merge(&s, &cogito_theme.iconbtn_tonal);
      if (!disabled) {
        if (active) s = cogito_style_merge(&s, &cogito_theme.iconbtn_tonal_active);
        else if (hover) s = cogito_style_merge(&s, &cogito_theme.iconbtn_tonal_hover);
      }
    } else if (iconbtn_class_is_iconic) {
      s = cogito_style_merge(&s, &cogito_theme.iconbtn_iconic);
      if (!disabled) {
        if (active) s = cogito_style_merge(&s, &cogito_theme.iconbtn_iconic_active);
        else if (hover) s = cogito_style_merge(&s, &cogito_theme.iconbtn_iconic_hover);
      }
    } else {
      // No class specified, use icon_color_style
      int cs = n->button.icon_color_style;
      if (cs == 0) {
        s = cogito_style_merge(&s, &cogito_theme.iconbtn_filled);
        if (!disabled) {
          if (active) s = cogito_style_merge(&s, &cogito_theme.iconbtn_filled_active);
          else if (hover) s = cogito_style_merge(&s, &cogito_theme.iconbtn_filled_hover);
        }
      } else if (cs == 1) {
        s = cogito_style_merge(&s, &cogito_theme.iconbtn_tonal);
        if (!disabled) {
          if (active) s = cogito_style_merge(&s, &cogito_theme.iconbtn_tonal_active);
          else if (hover) s = cogito_style_merge(&s, &cogito_theme.iconbtn_tonal_hover);
        }
      } else if (cs == 2) {
        s = cogito_style_merge(&s, &cogito_theme.iconbtn_outlined);
        if (!disabled) {
          if (active) s = cogito_style_merge(&s, &cogito_theme.iconbtn_outlined_active);
          else if (hover) s = cogito_style_merge(&s, &cogito_theme.iconbtn_outlined_hover);
        }
      } else { // Iconic (3 or default)
        s = cogito_style_merge(&s, &cogito_theme.iconbtn_iconic);
        if (!disabled) {
          if (active) s = cogito_style_merge(&s, &cogito_theme.iconbtn_iconic_active);
          else if (hover) s = cogito_style_merge(&s, &cogito_theme.iconbtn_iconic_hover);
        }
      }
    }
    return s; // Skip per_kind hover/active for iconbtn — color style handles it
  }
  if (checked) s = cogito_style_merge(&s, &cogito_theme.per_kind_checked[n->kind
  ]);
  if (disabled) s = cogito_style_merge(&s, &cogito_theme.per_kind_disabled[n->kind
  ]);
  else if (active) s = cogito_style_merge(&s, &cogito_theme.per_kind_active[n->kind
  ]);
  else if (focused) s = cogito_style_merge(&s, &cogito_theme.per_kind_focused[n->kind
  ]);
  else if (hover) s = cogito_style_merge(&s, &cogito_theme.per_kind_hover[n->kind
  ]);
  if (n->kind == COGITO_LABEL &&
      n->label.cls > COGITO_LABEL_CLASS_NONE &&
      n->label.cls < COGITO_LABEL_CLASS_COUNT) {
    if (checked) s = cogito_style_merge(&s, &cogito_theme.label_class_checked[n->label.cls
    ]);
    if (disabled) {
      s = cogito_style_merge(&s, &cogito_theme.label_class_disabled[n->label.cls
      ]);
    }
    if (!disabled) {
      if (active) s = cogito_style_merge(&s, &cogito_theme.label_class_active[n->label.cls
      ]);
      else if (focused) s = cogito_style_merge(&s, &cogito_theme.label_class_focused[n->label.cls
      ]);
      else if (hover) s = cogito_style_merge(&s, &cogito_theme.label_class_hover[n->label.cls
      ]);
    }
  }
  if (n->font_mono) {
    if (checked) s = cogito_style_merge(&s, &cogito_theme.class_mono_checked);
    if (disabled) {
      s = cogito_style_merge(&s, &cogito_theme.class_mono_disabled);
    }
    if (!disabled) {
      if (active) s = cogito_style_merge(&s, &cogito_theme.class_mono_active);
      else if (focused) s = cogito_style_merge(&s, &cogito_theme.class_mono_focused);
      else if (hover) s = cogito_style_merge(&s, &cogito_theme.class_mono_hover);
    }
  }
  if (n->font_tabular) {
    if (checked) s = cogito_style_merge(&s, &cogito_theme.class_tabular_checked);
    if (disabled) {
      s = cogito_style_merge(&s, &cogito_theme.class_tabular_disabled);
    }
    if (!disabled) {
      if (active) s = cogito_style_merge(&s, &cogito_theme.class_tabular_active);
      else if (focused) s = cogito_style_merge(&s, &cogito_theme.class_tabular_focused);
      else if (hover) s = cogito_style_merge(&s, &cogito_theme.class_tabular_hover);
    }
  }
  if (n->kind == COGITO_BUTTON) {
    if (n->button.outlined) {
      if (disabled) {
        s = cogito_style_merge(&s, &cogito_theme.button_outlined_disabled);
      }
      if (!disabled) {
        if (active) s = cogito_style_merge(&s, &cogito_theme.button_outlined_active);
        else if (focused) s = cogito_style_merge(&s, &cogito_theme.button_outlined_focused);
        else if (hover) s = cogito_style_merge(&s, &cogito_theme.button_outlined_hover);
      }
    }
    if (n->button.text_style) {
      if (disabled) {
        s = cogito_style_merge(&s, &cogito_theme.button_text_disabled);
      }
      if (!disabled) {
        if (active) s = cogito_style_merge(&s, &cogito_theme.button_text_active);
        else if (focused) s = cogito_style_merge(&s, &cogito_theme.button_text_focused);
        else if (hover) s = cogito_style_merge(&s, &cogito_theme.button_text_hover);
      }
    }
  }
  // Apply custom class state variants
  if (n->class_name && n->class_name->data && n->class_name->len > 0) {
    for (int i = 0; i < cogito_theme.custom_class_count; i++) {
      if (strcmp(cogito_theme.custom_classes[i
      ].name, n->class_name->data) == 0 &&
          (cogito_theme.custom_classes[i
      ].kind == n->kind ||
           cogito_theme.custom_classes[i
      ].kind == COGITO_KIND_COUNT)) {
        if (checked) s = cogito_style_merge(&s, &cogito_theme.custom_classes[i
        ].checked);
        if (disabled) {
          s = cogito_style_merge(&s, &cogito_theme.custom_classes[i
          ].disabled);
        } else {
          if (active) s = cogito_style_merge(&s, &cogito_theme.custom_classes[i
          ].active);
          else if (focused) s = cogito_style_merge(&s, &cogito_theme.custom_classes[i
          ].focused);
          else if (hover) s = cogito_style_merge(&s, &cogito_theme.custom_classes[i
          ].hover);
        }
        break; // Found the matching custom class
      }
    }
  }
  return s;
}

static bool cogito_kind_fit_children_background(CogitoKind kind) {
  switch (kind) {
    case COGITO_DIALOG:
    case COGITO_FIXED:
    case COGITO_LIST:
    case COGITO_POPOVER:
    case COGITO_SCROLLER:
    case COGITO_TOASTS:
    case COGITO_TOOLBAR:
    case COGITO_TREEVIEW:
    case COGITO_VIEWSWITCHER:
      return true;
    default:
      return false;
  }
}

static int cogito_visual_padding_from_sum(int padding, bool has_sum_padding) {
  if (padding < 0) return 0;
  if (has_sum_padding && padding < 6) return padding;
  return 6;
}

static void cogito_apply_visual_padding(
  CogitoNode* n,
  bool has_padding_all,
  bool has_padding_left,
  bool has_padding_top,
  bool has_padding_right,
  bool has_padding_bottom
) {
  if (!n || n->padding_set) return;
  if (!cogito_kind_fit_children_background(n->kind)) return;
  if (n->len == 0) return;
  n->padding_left = cogito_visual_padding_from_sum(n->padding_left, has_padding_all || has_padding_left);
  n->padding_top = cogito_visual_padding_from_sum(n->padding_top, has_padding_all || has_padding_top);
  n->padding_right = cogito_visual_padding_from_sum(n->padding_right, has_padding_all || has_padding_right);
  n->padding_bottom = cogito_visual_padding_from_sum(n->padding_bottom, has_padding_all || has_padding_bottom);
}

static void cogito_apply_style_to_node(CogitoNode* n) {
  if (!n) return;
  cogito_theme_init();
  CogitoStyle s = cogito_theme_resolve_node(n);
  if (s.has_bg) { n->bg = s.bg; n->bg_set = true;
  }
  else { n->bg_set = false;
  }
  if (s.has_text) { n->text_color = s.text; n->text_color_set = true;
  }
  else { n->text_color_set = false;
  }
  if (s.has_border) { n->border_color = s.border; n->border_color_set = true;
  }
  else { n->border_color_set = false; n->border_color = cogito_rgba(0,
    0,
    0,
    0);
  }
  if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true;
  }
  else { n->border_width = 0; n->border_width_set = false;
  }
  // For iconbtn, radius is controlled by icon_shape in draw code; don't bake it in
  if (s.has_radius && n->kind != COGITO_ICONBTN) { n->border_radius = s.radius; n->radius_set = true;
  }
  else if (n->kind != COGITO_ICONBTN) { n->radius_set = false;
  }
  if (s.has_selection) { n->selection_color = s.selection; n->selection_set = true;
  }
  else { n->selection_set = false;
  }
  if (s.has_track) { n->track_color = s.track; n->track_set = true;
  }
  else { n->track_set = false;
  }
  if (s.has_track_on) { n->track_on_color = s.track_on; n->track_on_set = true;
  }
  else { n->track_on_set = false;
  }
  if (s.has_knob) { n->knob_color = s.knob; n->knob_set = true;
  }
  else { n->knob_set = false;
  }
  if (s.has_check) { n->check_color = s.check; n->check_set = true;
  }
  else { n->check_set = false;
  }
  if (s.has_font_weight && (n->kind == COGITO_LABEL || n->kind == COGITO_BUTTON)) {
    n->font_weight = s.font_weight;
    n->font_weight_set = true;
  } else if (n->kind == COGITO_LABEL || n->kind == COGITO_BUTTON) {
    n->font_weight_set = false;
    n->font_weight = 400;
  }
  if (s.has_letter_spacing) {
    n->letter_spacing = s.letter_spacing;
    n->letter_spacing_set = true;
  } else {
    n->letter_spacing_set = false;
    n->letter_spacing = 0.0f;
  }
  if (s.has_font_tabular) {
    n->font_tabular = s.font_tabular;
  }
  if (s.has_font_mono) {
    n->font_mono = s.font_mono;
  }
  // Only apply theme size constraints; never clear user-set values
  if (s.has_min_w) { n->min_w = s.min_w; n->min_w_set = true;
  }
  if (s.has_min_h) { n->min_h = s.min_h; n->min_h_set = true;
  }
  if (s.has_max_w) { n->max_w = s.max_w; n->max_w_set = true;
  }
  if (s.has_max_h) { n->max_h = s.max_h; n->max_h_set = true;
  }
  if (s.has_shadow) { n->shadow_level = s.shadow_level; n->shadow_set = true;
  }
  else { n->shadow_set = false;
  }
  if (s.has_border_style) { n->border_style = s.border_style; n->border_style_set = true;
  }
  else { n->border_style_set = false;
  }
  if (s.has_radius_tl) { n->radius_tl = s.radius_tl; n->radius_tl_set = true;
  }
  else { n->radius_tl_set = false;
  }
  if (s.has_radius_tr) { n->radius_tr = s.radius_tr; n->radius_tr_set = true;
  }
  else { n->radius_tr_set = false;
  }
  if (s.has_radius_br) { n->radius_br = s.radius_br; n->radius_br_set = true;
  }
  else { n->radius_br_set = false;
  }
  if (s.has_radius_bl) { n->radius_bl = s.radius_bl; n->radius_bl_set = true;
  }
  else { n->radius_bl_set = false;
  }
  if (s.has_font_family) {
    strncpy(n->font_family, s.font_family,
    63);
    n->font_family[
      63
    ] = '\0';
    n->font_family_set = true;
  } else {
    n->font_family_set = false;
  }
  if (s.has_box_shadow) { n->box_shadow = s.box_shadow; n->box_shadow_set = true;
  }
  else { n->box_shadow_set = false;
  }
  // Preserve explicit widget overrides (e.g. progress.set_thickness()).
  // Style track-height only provides the fallback value.
  if (!n->track_height_set && s.has_track_height) { n->track_height = s.track_height;
  }
  if (s.has_font_size) {
    if (n->kind == COGITO_WINDOW) {
      int sz = s.font_size;
      if (sz > 0 && sz != cogito_font_size_value) {
        cogito_font_size_value = sz;
        if (cogito_backend_ready && cogito_font_ready && cogito_font_from_env) {
          const char* font_path = cogito_font_path_active;
          if (font_path && font_path[
            0
          ] && cogito_font_loaded_size != sz) {
            cogito_backend->font_unload(cogito_font);
            if (cogito_load_font_file(&cogito_font, font_path, sz, cogito_font_weight_normal())) {
              cogito_font_ready = true;
              cogito_font_loaded_size = sz;
            } else {
              cogito_font_ready = false;
            }
          }
        }
        if (cogito_backend_ready && cogito_font_mono_ready && cogito_font_mono_from_env) {
          const char* mono_path = cogito_font_mono_path_active;
          if (mono_path && mono_path[
            0
          ] && cogito_font_mono_loaded_size != sz) {
            cogito_backend->font_unload(cogito_font_mono);
            if (cogito_load_font_file(&cogito_font_mono, mono_path, sz, cogito_font_weight_normal())) {
              cogito_font_mono_ready = true;
              cogito_font_mono_loaded_size = sz;
            } else {
              cogito_font_mono_ready = false;
            }
          }
        }
        if (cogito_backend_ready && cogito_font_serif_ready && cogito_font_serif_from_env) {
          const char* serif_path = cogito_font_serif_path_active;
          if (serif_path && serif_path[
            0
          ] && cogito_font_serif_loaded_size != sz) {
            cogito_backend->font_unload(cogito_font_serif);
            if (cogito_load_font_file(&cogito_font_serif, serif_path, sz, cogito_font_weight_normal())) {
              cogito_font_serif_ready = true;
              cogito_font_serif_loaded_size = sz;
            } else {
              cogito_font_serif_ready = false;
            }
          }
        }
        if (cogito_backend_ready && cogito_font_bold_ready && cogito_font_bold_from_env) {
          const char* bold_path = cogito_font_bold_path_active;
          if (bold_path && bold_path[
            0
          ] && cogito_font_bold_loaded_size != sz) {
            cogito_backend->font_unload(cogito_font_bold);
            bool weight_applied = false;
            if (cogito_load_font_file_ex(&cogito_font_bold, bold_path, sz, cogito_font_weight_bold(), &weight_applied)) {
              if (cogito_font_bold_variable && !weight_applied) {
                cogito_backend->font_unload(cogito_font_bold);
                cogito_font_bold_ready = false;
                cogito_font_bold_variable = false;
              } else {
                cogito_font_bold_ready = true;
                cogito_font_bold_loaded_size = sz;
              }
            } else {
              cogito_font_bold_ready = false;
              cogito_font_bold_variable = false;
            }
          }
        }
      }
    } else if (n->kind == COGITO_LABEL || n->kind == COGITO_BUTTON) {
      n->font_size = s.font_size;
      n->font_size_set = true;
    }
  } else if (n->kind == COGITO_LABEL || n->kind == COGITO_BUTTON) {
    n->font_size_set = false;
  }

  if (!n->padding_set) {
    bool has_padding_all = s.has_padding;
    bool has_padding_left = s.has_padding_left;
    bool has_padding_top = s.has_padding_top;
    bool has_padding_right = s.has_padding_right;
    bool has_padding_bottom = s.has_padding_bottom;
    if (s.has_padding) {
      n->padding_left = s.padding_left;
      n->padding_top = s.padding_top;
      n->padding_right = s.padding_right;
      n->padding_bottom = s.padding_bottom;
    }
    if (s.has_padding_left) n->padding_left = s.padding_left;
    if (s.has_padding_top) n->padding_top = s.padding_top;
    if (s.has_padding_right) n->padding_right = s.padding_right;
    if (s.has_padding_bottom) n->padding_bottom = s.padding_bottom;
    cogito_apply_visual_padding(
      n,
      has_padding_all,
      has_padding_left,
      has_padding_top,
      has_padding_right,
      has_padding_bottom
    );
  }

  if (!n->margin_set) {
    if (s.has_margin) {
      n->margin_left = s.margin_left;
      n->margin_top = s.margin_top;
      n->margin_right = s.margin_right;
      n->margin_bottom = s.margin_bottom;
    }
    if (s.has_margin_left) n->margin_left = s.margin_left;
    if (s.has_margin_top) n->margin_top = s.margin_top;
    if (s.has_margin_right) n->margin_right = s.margin_right;
    if (s.has_margin_bottom) n->margin_bottom = s.margin_bottom;
  }
  if (s.has_opacity) { n->opacity = s.opacity; n->opacity_set = true;
  }
  else { n->opacity_set = false; n->opacity = 1.0f;
  }
}

static void cogito_apply_style_tree(CogitoNode* n) {
  if (!n) return;
  cogito_apply_style_to_node(n);
  for (size_t i = 0; i < n->len; i++) {
    cogito_apply_style_tree(n->children[i
    ]);
  }
}

static int cogito_node_font_size(const CogitoNode* n);

// Style dump debugging system (kind_name, diagnostic output)
#include "widgets/util/style_dump.inc"

static int cogito_font_size(void) {
  return cogito_font_size_value > 0 ? cogito_font_size_value : 14;
}

static int cogito_label_font_size(const CogitoNode* n) {
  if (!n) return cogito_font_size();
  if (n->font_size_set && n->font_size > 0) return n->font_size;
  switch (n->label.cls) {
    case COGITO_LABEL_CLASS_BIG_DISPLAY: return 56;
    case COGITO_LABEL_CLASS_DISPLAY: return 36;
    case COGITO_LABEL_CLASS_VIEW_TITLE:
    case COGITO_LABEL_CLASS_TITLE: return 24;
    case COGITO_LABEL_CLASS_VIEW_SUBTITLE:
    case COGITO_LABEL_CLASS_SUBTITLE: return 18;
    case COGITO_LABEL_CLASS_HEADING: return 16;
    case COGITO_LABEL_CLASS_CONTENT_TITLE: return 16;
    case COGITO_LABEL_CLASS_CONTENT_SUBTITLE:
    case COGITO_LABEL_CLASS_BODY: return 14;
    case COGITO_LABEL_CLASS_CAPTION: return 12;
    default: return cogito_font_size();
  }
}

static bool cogito_label_bold(const CogitoNode* n);

static int cogito_node_font_size(const CogitoNode* n) {
  if (!n) return cogito_font_size();
  if (n->kind == COGITO_LABEL) return cogito_label_font_size(n);
  if (n->font_size_set && n->font_size > 0) return n->font_size;
  return cogito_font_size();
}

static bool cogito_node_bold(const CogitoNode* n) {
  if (!n) return false;
  if (n->kind == COGITO_LABEL) return cogito_label_bold(n);
  if (n->font_weight_set) return n->font_weight >= 600;
  return false;
}

static bool cogito_node_disabled(const CogitoNode* n) {
  return n && n->disabled;
}

static void cogito_apply_size_constraints(const CogitoNode* n, int* w, int* h) {
  if (!n || !w || !h) return;
  if (n->min_w_set && *w < n->min_w) *w = n->min_w;
  if (n->min_h_set && *h < n->min_h) *h = n->min_h;
  if (n->max_w_set && n->max_w > 0 && *w > n->max_w) *w = n->max_w;
  if (n->max_h_set && n->max_h > 0 && *h > n->max_h) *h = n->max_h;
}
// Appbar widget utilities (metrics, positions, navigation)
#include "widgets/util/appbar.inc"

static bool cogito_label_bold(const CogitoNode* n) {
  if (!n) return false;
  if (n->font_weight_set) return n->font_weight >= 600;
  return n->label.cls == COGITO_LABEL_CLASS_SUBTITLE ||
         n->label.cls == COGITO_LABEL_CLASS_VIEW_SUBTITLE ||
         n->label.cls == COGITO_LABEL_CLASS_CONTENT_TITLE ||
         n->label.cls == COGITO_LABEL_CLASS_CAPTION;
}

static int cogito_label_class_font_size(int cls, int fallback) {
  cogito_theme_init();
  if (cls > COGITO_LABEL_CLASS_NONE && cls < COGITO_LABEL_CLASS_COUNT) {
    CogitoStyle* s = &cogito_theme.label_class[cls
    ];
    if (s->has_font_size && s->font_size > 0) return s->font_size;
  }
  return fallback;
}

static bool cogito_label_class_bold(int cls, bool fallback) {
  cogito_theme_init();
  if (cls > COGITO_LABEL_CLASS_NONE && cls < COGITO_LABEL_CLASS_COUNT) {
    CogitoStyle* s = &cogito_theme.label_class[cls
    ];
    if (s->has_font_weight) return s->font_weight >= 600;
  }
  return fallback;
}

static CogitoColor cogito_label_class_color(int cls, CogitoColor fallback) {
  cogito_theme_init();
  if (cls > COGITO_LABEL_CLASS_NONE && cls < COGITO_LABEL_CLASS_COUNT) {
    CogitoStyle* s = &cogito_theme.label_class[cls
    ];
    if (s->has_text) return s->text;
  }
  return fallback;
}

static int cogito_css_hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

static void cogito_css_skip_ws(const char** p) {
  while (**p) {
    if (isspace((unsigned char)**p)) { (*p)++; continue;
    }
    if ((*p)[
      0
    ] == '/' && (*p)[
      1
    ] == '/') {
      (*p) += 2;
      while (**p && **p != '\n') (*p)++;
      continue;
    }
    if ((*p)[
      0
    ] == '/' && (*p)[
      1
    ] == '*') {
      (*p) += 2;
      while (**p && !((*p)[
        0
      ] == '*' && (*p)[
        1
      ] == '/')) (*p)++;
      if (**p) (*p) += 2;
      continue;
    }
    break;
  }
}

static bool cogito_css_read_ident(const char** p, char* out, size_t cap) {
  size_t len = 0;
  if (!isalpha((unsigned char)**p) && **p != '_' && **p != '-' && **p != '.' && **p != '#') return false;
  while (**p && (isalnum((unsigned char)**p) || **p == '_' || **p == '-' || **p == '.' || **p == '#')) {
    if (len + 1 < cap) out[len++
    ] = **p;
    (*p)++;
  }
  out[len
  ] = 0;
  return len > 0;
}

static bool cogito_css_read_number(const char** p, double* out) {
  char* end = NULL;
  double v = strtod(*p, &end);
  if (end == *p) return false;
  *p = end;
  if ((*p)[
    0
  ] == 'p' && (*p)[
    1
  ] == 'x') (*p) += 2;
  *out = v;
  return true;
}

static bool cogito_css_parse_hex_color(const char** p, CogitoColor* out) {
  if (**p != '#') return false;
  const char* s = *p + 1;
  int len = 0;
  while (cogito_css_hex_value(s[len
  ]) >= 0) len++;
  if (len != 3 && len != 4 && len != 6 && len != 8) return false;
  int r = 0, g = 0, b = 0, a = 255;
  if (len == 3 || len == 4) {
    r = cogito_css_hex_value(s[
      0
    ]) * 17;
    g = cogito_css_hex_value(s[
      1
    ]) * 17;
    b = cogito_css_hex_value(s[
      2
    ]) * 17;
    if (len == 4) a = cogito_css_hex_value(s[
      3
    ]) * 17;
  } else if (len == 6 || len == 8) {
    r = cogito_css_hex_value(s[
      0
    ]) * 16 + cogito_css_hex_value(s[
      1
    ]);
    g = cogito_css_hex_value(s[
      2
    ]) * 16 + cogito_css_hex_value(s[
      3
    ]);
    b = cogito_css_hex_value(s[
      4
    ]) * 16 + cogito_css_hex_value(s[
      5
    ]);
    if (len == 8) a = cogito_css_hex_value(s[
      6
    ]) * 16 + cogito_css_hex_value(s[
      7
    ]);
  }
  *out = cogito_rgba(r, g, b, a);
  *p += 1 + len;
  return true;
}

static bool cogito_css_parse_named_color(const char* name, CogitoColor* out) {
  if (!name) return false;
  if (strcmp(name,
  "transparent") == 0) { *out = cogito_rgba(0,
    0,
    0,
    0); return true;
  }
  if (strcmp(name,
  "white") == 0) { *out = cogito_rgba(255,
    255,
    255,
    255); return true;
  }
  if (strcmp(name,
  "black") == 0) { *out = cogito_rgba(0,
    0,
    0,
    255); return true;
  }
  return false;
}

static bool cogito_css_parse_rgb_color(const char** p, CogitoColor* out) {
  const char* s = *p;
  bool has_alpha = false;
  if (strncmp(s,
  "rgba",
  4) == 0) {
    has_alpha = true;
    s += 4;
  } else if (strncmp(s,
  "rgb",
  3) == 0) {
    s += 3;
  } else {
    return false;
  }
  cogito_css_skip_ws(&s);
  if (*s != '(') return false;
  s++;
  double vals[
    4
  ] = {
    0,
    0,
    0,
    1
  };
  int count = 0;
  while (*s && count < (has_alpha ? 4 : 3)) {
    cogito_css_skip_ws(&s);
    double v = 0;
    if (!cogito_css_read_number(&s, &v)) break;
    vals[count++
    ] = v;
    cogito_css_skip_ws(&s);
    if (*s == ',') s++;
  }
  cogito_css_skip_ws(&s);
  if (*s != ')') return false;
  s++;
  if (count < (has_alpha ? 4 : 3)) return false;
  int r = (int)lround(vals[
    0
  ]);
  int g = (int)lround(vals[
    1
  ]);
  int b = (int)lround(vals[
    2
  ]);
  int a = 255;
  if (has_alpha) {
    if (vals[
      3
    ] <= 1.0) a = (int)lround(vals[
      3
    ] * 255.0);
    else a = (int)lround(vals[
      3
    ]);
  }
  *out = cogito_rgba(r, g, b, a);
  *p = s;
  return true;
}

static bool cogito_css_parse_color(const char** p, CogitoColor* out) {
  cogito_css_skip_ws(p);
  if (**p == '#') return cogito_css_parse_hex_color(p, out);
  if (strncmp(*p,
  "rgb",
  3) == 0) return cogito_css_parse_rgb_color(p, out);
  char name[
    32
  ];
  const char* s = *p;
  if (!cogito_css_read_ident(&s, name, sizeof(name))) return false;
  if (cogito_css_parse_named_color(name, out)) { *p = s; return true;
  }
  return false;
}

static int cogito_css_parse_ints(const char** p, int* out, int max) {
  int count = 0;
  while (count < max) {
    cogito_css_skip_ws(p);
    double v = 0;
    if (!cogito_css_read_number(p, &v)) break;
    out[count++
    ] = (int)lround(v);
    cogito_css_skip_ws(p);
    if (**p == ',') (*p)++;
  }
  return count;
}
