// Style dump debugging system
// Provides detailed diagnostic output for computed styles on nodes.

static bool cogito_debug_style_enabled_internal(void) {
  const char* v = getenv("COGITO_DEBUG_STYLE");
  return v && v[0] && v[0] != '0';
}

static const char* cogito_kind_name(CogitoKind kind) {
  switch (kind) {
    case COGITO_APPBAR: return "appbar";
    case COGITO_BOTTOM_NAV: return "bottom-nav";
    case COGITO_BUTTON: return "button";
    case COGITO_CAROUSEL: return "carousel";
    case COGITO_CAROUSEL_ITEM: return "carousel_item";
    case COGITO_CHECKBOX: return "checkbox";
    case COGITO_CHIP: return "chip";
    case COGITO_COLORPICKER: return "colorpicker";
    case COGITO_DATEPICKER: return "datepicker";
    case COGITO_DIALOG: return "dialog";
    case COGITO_DIALOG_SLOT: return "dialogslot";
    case COGITO_DROPDOWN: return "dropdown";
    case COGITO_FAB: return "fab";
    case COGITO_FAB_MENU: return "fab-menu";
    case COGITO_FIXED: return "fixed";
    case COGITO_GRID: return "grid";
    case COGITO_HSTACK: return "hstack";
    case COGITO_ICONBTN: return "iconbtn";
    case COGITO_IMAGE: return "image";
    case COGITO_LABEL: return "label";
    case COGITO_LIST: return "list";
    case COGITO_NAV_RAIL: return "nav-rail";
    case COGITO_POPOVER: return "popover";
    case COGITO_PROGRESS: return "progress";
    case COGITO_SCROLLER: return "scroller";
    case COGITO_SEARCHFIELD: return "searchfield";
    case COGITO_BUTTON_GROUP: return "buttongroup";
    case COGITO_SLIDER: return "slider";
    case COGITO_STEPPER: return "stepper";
    case COGITO_SWITCH: return "switch";
    case COGITO_TABS: return "tabs";
    case COGITO_TEXTFIELD: return "textfield";
    case COGITO_TEXTVIEW: return "textview";
    case COGITO_TOAST: return "toast";
    case COGITO_TOASTS: return "toasts";
    case COGITO_TOOLBAR: return "toolbar";
    case COGITO_TOOLTIP: return "tooltip";
    case COGITO_TREEVIEW: return "treeview";
    case COGITO_VIEWSWITCHER: return "viewswitcher";
    case COGITO_VSTACK: return "vstack";
    case COGITO_WINDOW: return "window";
    case COGITO_ZSTACK: return "zstack";
    case COGITO_DIVIDER: return "divider";
    case COGITO_CARD: return "card";
    case COGITO_AVATAR: return "avatar";
    case COGITO_BADGE: return "badge";
    case COGITO_BANNER: return "banner";
    case COGITO_BOTTOM_SHEET: return "bottom-sheet";
    case COGITO_SIDE_SHEET: return "side-sheet";
    case COGITO_TIMEPICKER: return "timepicker";
    case COGITO_ACTIVE_INDICATOR: return "active-indicator";
    case COGITO_SWITCHBAR: return "switchbar";
    case COGITO_CONTENT_LIST: return "content-list";
    case COGITO_EMPTY_PAGE: return "empty-page";
    case COGITO_TIP_VIEW: return "tip-view";
    case COGITO_SETTINGS_WINDOW: return "settings-window";
    case COGITO_SETTINGS_PAGE: return "settings-page";
    case COGITO_SETTINGS_LIST: return "settings-list";
    case COGITO_SETTINGS_ROW: return "settings-row";
    case COGITO_WELCOME_SCREEN: return "welcome-screen";
    case COGITO_VIEW_DUAL: return "view-dual";
    case COGITO_VIEW_CHOOSER: return "view-chooser";
    case COGITO_ABOUT_WINDOW: return "about-window";
    case COGITO_SPLIT_BUTTON: return "split-button";
    case COGITO_DRAWING_AREA: return "drawing-area";
    case COGITO_SHAPE: return "shape";
    case COGITO_MENU_SECTION: return "menu-section";
    case COGITO_KIND_COUNT: return "unknown";
  }
  return "unknown";
}

typedef struct CogitoStyleDumpOrigins {
  const char* background;
  const char* color;
  const char* border_color;
  const char* border_width;
  const char* border_radius;
  const char* padding;
  const char* font_size;
  const char* opacity;
} CogitoStyleDumpOrigins;

static void cogito_style_dump_apply_source(CogitoStyle* out, CogitoStyleDumpOrigins* origins, const CogitoStyle* over, const char* source) {
  if (!out || !origins || !over || !source) return;
  if (over->has_bg) {
    out->has_bg = true;
    out->bg = over->bg;
    origins->background = source;
  }
  if (over->has_text) {
    out->has_text = true;
    out->text = over->text;
    origins->color = source;
  }
  if (over->has_border) {
    out->has_border = true;
    out->border = over->border;
    origins->border_color = source;
  }
  if (over->has_border_width) {
    out->has_border_width = true;
    out->border_width = over->border_width;
    origins->border_width = source;
  }
  if (over->has_radius) {
    out->has_radius = true;
    out->radius = over->radius;
    origins->border_radius = source;
  }
  if (over->has_radius_tl) {
    out->has_radius_tl = true;
    out->radius_tl = over->radius_tl;
    origins->border_radius = source;
  }
  if (over->has_radius_tr) {
    out->has_radius_tr = true;
    out->radius_tr = over->radius_tr;
    origins->border_radius = source;
  }
  if (over->has_radius_br) {
    out->has_radius_br = true;
    out->radius_br = over->radius_br;
    origins->border_radius = source;
  }
  if (over->has_radius_bl) {
    out->has_radius_bl = true;
    out->radius_bl = over->radius_bl;
    origins->border_radius = source;
  }
  if (over->has_padding) {
    out->has_padding = true;
    out->padding_left = over->padding_left;
    out->padding_top = over->padding_top;
    out->padding_right = over->padding_right;
    out->padding_bottom = over->padding_bottom;
    origins->padding = source;
  }
  if (over->has_padding_left) {
    out->has_padding_left = true;
    out->padding_left = over->padding_left;
    origins->padding = source;
  }
  if (over->has_padding_top) {
    out->has_padding_top = true;
    out->padding_top = over->padding_top;
    origins->padding = source;
  }
  if (over->has_padding_right) {
    out->has_padding_right = true;
    out->padding_right = over->padding_right;
    origins->padding = source;
  }
  if (over->has_padding_bottom) {
    out->has_padding_bottom = true;
    out->padding_bottom = over->padding_bottom;
    origins->padding = source;
  }
  if (over->has_font_size) {
    out->has_font_size = true;
    out->font_size = over->font_size;
    origins->font_size = source;
  }
  if (over->has_opacity) {
    out->has_opacity = true;
    out->opacity = over->opacity;
    origins->opacity = source;
  }
}

static void cogito_style_dump_color_value(CogitoColor c, char* out, size_t cap) {
  if (!out || cap == 0) return;
  if (c.a == 255) {
    snprintf(out, cap, "#%02X%02X%02X", (unsigned int)c.r, (unsigned int)c.g, (unsigned int)c.b);
  } else {
    snprintf(out, cap, "#%02X%02X%02X%02X", (unsigned int)c.r, (unsigned int)c.g, (unsigned int)c.b, (unsigned int)c.a);
  }
}

static void cogito_style_dump_indent(int depth) {
  if (depth < 0) depth = 0;
  for (int i = 0; i < depth; i++) fputs("  ", stderr);
}

static void cogito_style_dump_line(int depth, const char* name, const char* value, const char* origin) {
  cogito_style_dump_indent(depth);
  fprintf(stderr, "  %-20s = %-20s [%s]\n", name, value ? value : "(unset)", origin ? origin : "default");
}

static void cogito_style_dump_print_node(const CogitoNode* n, int depth) {
  if (!n) return;
  cogito_theme_init();

  CogitoStyle s;
  cogito_style_clear(&s);
  CogitoStyleDumpOrigins origins = {0};

  char kind_origin[64];
  snprintf(kind_origin, sizeof(kind_origin), "kind:%s", cogito_kind_name(n->kind));

  const char* state_hover = "state:hover";
  const char* state_active = "state:active";
  const char* state_focused = "state:focus";
  const char* state_checked = "state:checked";
  const char* state_disabled = "state:disabled";

  cogito_style_dump_apply_source(&s, &origins, &cogito_theme.base, "base:*");
  cogito_style_dump_apply_source(&s, &origins, &cogito_theme.per_kind[n->kind], kind_origin);

  if (n->kind == COGITO_LABEL &&
      n->label.cls > COGITO_LABEL_CLASS_NONE &&
      n->label.cls < COGITO_LABEL_CLASS_COUNT) {
    cogito_style_dump_apply_source(&s, &origins, &cogito_theme.label_class[n->label.cls], "class:label");
  }
  if (n->font_mono) {
    cogito_style_dump_apply_source(&s, &origins, &cogito_theme.class_mono, "class:mono");
  }
  if (n->font_tabular) {
    cogito_style_dump_apply_source(&s, &origins, &cogito_theme.class_tabular, "class:tabular");
  }
  if (n->button.outlined) {
    cogito_style_dump_apply_source(&s, &origins, &cogito_theme.button_outlined, "class:outlined");
  }
  if (n->button.text_style) {
    cogito_style_dump_apply_source(&s, &origins, &cogito_theme.button_text, "class:text");
  }
  if (n->parent && n->parent->kind == COGITO_APPBAR) {
    if (n->kind == COGITO_BUTTON) {
      cogito_style_dump_apply_source(&s, &origins, &cogito_theme.appbar_child_button, "class:appbar-button");
    } else if (n->kind == COGITO_ICONBTN) {
      cogito_style_dump_apply_source(&s, &origins, &cogito_theme.appbar_child_iconbtn, "class:appbar-iconbtn");
    }
  }

  char custom_origin[96] = {0};
  int custom_index = -1;
  if (n->class_name && n->class_name->data && n->class_name->len > 0) {
    for (int i = 0; i < cogito_theme.custom_class_count; i++) {
      if (strcmp(cogito_theme.custom_classes[i].name, n->class_name->data) == 0 &&
          (cogito_theme.custom_classes[i].kind == n->kind ||
           cogito_theme.custom_classes[i].kind == COGITO_KIND_COUNT)) {
        custom_index = i;
        snprintf(custom_origin, sizeof(custom_origin), "custom:%s", cogito_theme.custom_classes[i].name);
        CogitoColor saved_bg = s.bg;
        bool saved_has_bg = s.has_bg;
        cogito_style_dump_apply_source(&s, &origins, &cogito_theme.custom_classes[i].style, custom_origin);
        if (n->kind == COGITO_WINDOW && saved_has_bg) {
          s.bg = saved_bg;
          s.has_bg = saved_has_bg;
          origins.background = kind_origin;
        }
        break;
      }
    }
  }

  bool checked = false;
  if (n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH) checked = n->checked;
  else if (n->kind == COGITO_CHIP) checked = n->chip.selected;
  bool disabled = cogito_node_disabled(n);
  bool focused = (!disabled && n == cogito_focused);
  bool hover = false;
  bool active = false;
  if (!disabled && cogito_backend_ready && cogito_backend) {
    cogito_node_hover_state((CogitoNode*)n, &hover, &active);
  }

  if (checked) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.per_kind_checked[n->kind], state_checked);
  if (disabled) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.per_kind_disabled[n->kind], state_disabled);
  else if (active) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.per_kind_active[n->kind], state_active);
  else if (focused) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.per_kind_focused[n->kind], state_focused);
  else if (hover) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.per_kind_hover[n->kind], state_hover);
  if (n->kind == COGITO_LABEL &&
      n->label.cls > COGITO_LABEL_CLASS_NONE &&
      n->label.cls < COGITO_LABEL_CLASS_COUNT) {
    if (checked) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.label_class_checked[n->label.cls], state_checked);
    if (disabled) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.label_class_disabled[n->label.cls], state_disabled);
    else if (active) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.label_class_active[n->label.cls], state_active);
    else if (focused) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.label_class_focused[n->label.cls], state_focused);
    else if (hover) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.label_class_hover[n->label.cls], state_hover);
  }
  if (n->font_mono) {
    if (checked) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.class_mono_checked, state_checked);
    if (disabled) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.class_mono_disabled, state_disabled);
    else if (active) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.class_mono_active, state_active);
    else if (focused) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.class_mono_focused, state_focused);
    else if (hover) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.class_mono_hover, state_hover);
  }
  if (n->font_tabular) {
    if (checked) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.class_tabular_checked, state_checked);
    if (disabled) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.class_tabular_disabled, state_disabled);
    else if (active) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.class_tabular_active, state_active);
    else if (focused) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.class_tabular_focused, state_focused);
    else if (hover) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.class_tabular_hover, state_hover);
  }
  if (n->button.outlined) {
    if (disabled) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.button_outlined_disabled, state_disabled);
    else if (active) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.button_outlined_active, state_active);
    else if (focused) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.button_outlined_focused, state_focused);
    else if (hover) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.button_outlined_hover, state_hover);
  }
  if (n->button.text_style) {
    if (disabled) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.button_text_disabled, state_disabled);
    else if (active) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.button_text_active, state_active);
    else if (focused) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.button_text_focused, state_focused);
    else if (hover) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.button_text_hover, state_hover);
  }
  if (custom_index >= 0) {
    if (checked) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.custom_classes[custom_index].checked, state_checked);
    if (disabled) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.custom_classes[custom_index].disabled, state_disabled);
    else if (active) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.custom_classes[custom_index].active, state_active);
    else if (focused) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.custom_classes[custom_index].focused, state_focused);
    else if (hover) cogito_style_dump_apply_source(&s, &origins, &cogito_theme.custom_classes[custom_index].hover, state_hover);
  }

  if (n->padding_set) {
    s.has_padding = true;
    s.padding_left = n->padding_left;
    s.padding_top = n->padding_top;
    s.padding_right = n->padding_right;
    s.padding_bottom = n->padding_bottom;
    origins.padding = "inline:node";
  }

  char bg[20];
  char text[20];
  char border[20];
  char border_width[24];
  char radius[24];
  char padding[48];
  char font_size[24];
  char opacity[24];

  if (s.has_bg) cogito_style_dump_color_value(s.bg, bg, sizeof(bg));
  else snprintf(bg, sizeof(bg), "(unset)");
  if (s.has_text) cogito_style_dump_color_value(s.text, text, sizeof(text));
  else snprintf(text, sizeof(text), "(unset)");
  if (s.has_border) cogito_style_dump_color_value(s.border, border, sizeof(border));
  else snprintf(border, sizeof(border), "(unset)");
  snprintf(border_width, sizeof(border_width), "%d", s.has_border_width ? s.border_width : 0);
  if (s.has_radius) snprintf(radius, sizeof(radius), "%d", s.radius);
  else if (s.has_radius_tl) snprintf(radius, sizeof(radius), "%d", s.radius_tl);
  else snprintf(radius, sizeof(radius), "0");
  snprintf(padding, sizeof(padding), "%d %d %d %d", s.padding_top, s.padding_right, s.padding_bottom, s.padding_left);
  snprintf(font_size, sizeof(font_size), "%d", s.has_font_size ? s.font_size : cogito_node_font_size(n));
  snprintf(opacity, sizeof(opacity), "%.2f", s.has_opacity ? s.opacity : 1.0f);

  cogito_style_dump_indent(depth);
  fprintf(stderr, "=== Style Dump: %s ===\n", cogito_kind_name(n->kind));
  if (n->class_name && n->class_name->data && n->class_name->len > 0) {
    cogito_style_dump_indent(depth);
    fprintf(stderr, "Class: %s\n", n->class_name->data);
  }
  cogito_style_dump_indent(depth);
  fprintf(stderr, "State: hover=%d active=%d checked=%d disabled=%d\n", hover ? 1 : 0, active ? 1 : 0, checked ? 1 : 0, disabled ? 1 : 0);
  cogito_style_dump_indent(depth);
  fprintf(stderr, "----------------------------------------\n");
  cogito_style_dump_line(depth, "background", bg, origins.background);
  cogito_style_dump_line(depth, "color", text, origins.color);
  cogito_style_dump_line(depth, "border-color", border, origins.border_color);
  cogito_style_dump_line(depth, "border-width", border_width, origins.border_width);
  cogito_style_dump_line(depth, "border-radius", radius, origins.border_radius);
  cogito_style_dump_line(depth, "padding", padding, origins.padding);
  cogito_style_dump_line(depth, "font-size", font_size, origins.font_size);
  cogito_style_dump_line(depth, "opacity", opacity, origins.opacity);
  cogito_style_dump_indent(depth);
  fprintf(stderr, "========================================\n");
}

static void cogito_style_dump_internal(CogitoNode* node) {
  if (!node) return;
  cogito_style_dump_print_node(node, 0);
}

static void cogito_style_dump_tree_internal(CogitoNode* root, int depth) {
  if (!root) return;
  if (depth < 0) depth = 0;
  cogito_style_dump_print_node(root, depth);
  for (size_t i = 0; i < root->len; i++) {
    cogito_style_dump_tree_internal(root->children[i], depth + 1);
  }
}
