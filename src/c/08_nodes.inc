static bool cogito_val_equal(YisVal a, YisVal b) {
  if (a.tag != b.tag) return false;
  switch (a.tag) {
    case EVT_NULL: return true;
    case EVT_INT: return a.as.i == b.as.i;
    case EVT_FLOAT: return a.as.f == b.as.f;
    case EVT_BOOL: return a.as.b == b.as.b;
    case EVT_STR:
    case EVT_ARR:
    case EVT_OBJ:
    case EVT_DICT:
    case EVT_FN:
      return a.as.p == b.as.p;
  }
  return false;
}

static void cogito_timer_cancel_owner_tree(CogitoNode* root);

static bool cogito_group_match(YisVal a, YisVal b) {
  if (a.tag != b.tag) return false;
  if (a.tag == EVT_STR) {
    YisStr* sa = (YisStr*)a.as.p;
    YisStr* sb = (YisStr*)b.as.p;
    if (!sa || !sb) return false;
    if (sa->len != sb->len) return false;
    return memcmp(sa->data, sb->data, sa->len) == 0;
  }
  return cogito_val_equal(a, b);
}

static bool cogito_str_eq(const YisStr* a, const YisStr* b) {
  if (!a || !b) return false;
  if (a->len != b->len) return false;
  return memcmp(a->data, b->data, a->len) == 0;
}

static void cogito_node_drop(YisObj* o) {
  CogitoNode* n = (CogitoNode*)o;
  cogito_timer_cancel_owner_tree(n);
  if (n->text) {
    yis_release_val(YV_STR(n->text));
    n->text = NULL;
  }
  if (n->subtitle) {
    yis_release_val(YV_STR(n->subtitle));
    n->subtitle = NULL;
  }
  if (n->hint) {
    yis_release_val(YV_STR(n->hint));
    n->hint = NULL;
  }
  if (n->tooltip) {
    yis_release_val(YV_STR(n->tooltip));
    n->tooltip = NULL;
  }
  if (n->icon) {
    yis_release_val(YV_STR(n->icon));
    n->icon = NULL;
  }
  if ((n->kind == COGITO_IMAGE || n->kind == COGITO_AVATAR) &&
      n->kd->image.texture) {
    cogito_image_cache_release_texture(n->kd->image.texture);
    n->kd->image.texture = NULL;
  }
  if ((n->kind == COGITO_IMAGE || n->kind == COGITO_AVATAR) &&
      n->kd->image.glow_texture && cogito_backend &&
      cogito_backend->texture_destroy) {
    cogito_backend->texture_destroy(n->kd->image.glow_texture);
    n->kd->image.glow_texture = NULL;
    n->kd->image.glow_source = NULL;
  }
  if (n->on_click) {
    yis_release_val(YV_FN(n->on_click));
    n->on_click = NULL;
  }
  if (n->on_change) {
    yis_release_val(YV_FN(n->on_change));
    n->on_change = NULL;
  }
  if (n->on_select) {
    yis_release_val(YV_FN(n->on_select));
    n->on_select = NULL;
  }
  if (n->on_activate) {
    yis_release_val(YV_FN(n->on_activate));
    n->on_activate = NULL;
  }
  if (n->builder) {
    yis_release_val(YV_FN(n->builder));
    n->builder = NULL;
  }
  if (n->on_action) {
    yis_release_val(YV_FN(n->on_action));
    n->on_action = NULL;
  }
  if (n->on_draw) {
    yis_release_val(YV_FN(n->on_draw));
    n->on_draw = NULL;
  }
  if (n->on_resize) {
    yis_release_val(YV_FN(n->on_resize));
    n->on_resize = NULL;
  }
  if (n->action_text) {
    yis_release_val(YV_STR(n->action_text));
    n->action_text = NULL;
  }
  if (n->class_name) {
    yis_release_val(YV_STR(n->class_name));
    n->class_name = NULL;
  }
  if (n->group.tag != EVT_NULL) {
    yis_release_val(n->group);
    n->group = YV_NULLV;
  }
  if (n->menu_labels) {
    for (size_t i = 0; i < n->menu_len; i++) {
      if (n->menu_labels[i]) yis_release_val(YV_STR(n->menu_labels[i]));
      if (n->menu_handlers[i]) yis_release_val(YV_FN(n->menu_handlers[i]));
      if (n->menu_icons && n->menu_icons[i]) yis_release_val(YV_STR(n->menu_icons[i]));
      if (n->menu_shortcuts && n->menu_shortcuts[i]) yis_release_val(YV_STR(n->menu_shortcuts[i]));
    }
    free(n->menu_labels);
    free(n->menu_handlers);
    free(n->menu_section_after);
    free(n->menu_icons);
    free(n->menu_shortcuts);
    free(n->menu_submenu);
    free(n->menu_toggled);
    n->menu_labels = NULL;
    n->menu_handlers = NULL;
    n->menu_section_after = NULL;
    n->menu_icons = NULL;
    n->menu_shortcuts = NULL;
    n->menu_submenu = NULL;
    n->menu_toggled = NULL;
    n->menu_len = 0;
    n->menu_cap = 0;
  }
  if (n->a11y_label) {
    yis_release_val(YV_STR(n->a11y_label));
    n->a11y_label = NULL;
  }
  if (n->a11y_role) {
    yis_release_val(YV_STR(n->a11y_role));
    n->a11y_role = NULL;
  }
  if (n->tab_labels) {
    for (size_t i = 0; i < n->tab_len; i++) {
      if (n->tab_labels[i]) yis_release_val(YV_STR(n->tab_labels[i]));
      if (n->tab_icons && n->tab_icons[i]) yis_release_val(YV_STR(n->tab_icons[i]));
      if (n->tab_ids && n->tab_ids[i]) yis_release_val(YV_STR(n->tab_ids[i]));
    }
    free(n->tab_labels);
    free(n->tab_icons);
    free(n->tab_ids);
    n->tab_labels = NULL;
    n->tab_icons = NULL;
    n->tab_ids = NULL;
    n->tab_len = 0;
    n->tab_cap = 0;
  }
  if (n->kind == COGITO_NAV_RAIL) {
    if (n->kd->nav_rail.labels) {
      for (size_t i = 0; i < n->kd->nav_rail.len; i++) {
        if (n->kd->nav_rail.labels[i]) yis_release_val(YV_STR(n->kd->nav_rail.labels[i]));
        if (n->kd->nav_rail.icons && n->kd->nav_rail.icons[i]) yis_release_val(YV_STR(n->kd->nav_rail.icons[i]));
      }
      free(n->kd->nav_rail.labels);
      free(n->kd->nav_rail.icons);
    }
    if (n->kd->nav_rail.badges) free(n->kd->nav_rail.badges);
    if (n->kd->nav_rail.item_springs) free(n->kd->nav_rail.item_springs);
  }
  if (n->kind == COGITO_BOTTOM_NAV && n->kd->bottom_nav.labels) {
    for (size_t i = 0; i < n->kd->bottom_nav.len; i++) {
      if (n->kd->bottom_nav.labels[i]) yis_release_val(YV_STR(n->kd->bottom_nav.labels[i]));
      if (n->kd->bottom_nav.icons && n->kd->bottom_nav.icons[i]) yis_release_val(YV_STR(n->kd->bottom_nav.icons[i]));
    }
    free(n->kd->bottom_nav.labels);
    free(n->kd->bottom_nav.icons);
  }
  if (n->kind == COGITO_SLIDER && n->kd->slider.bubble_format) {
    yis_release_val(YV_STR(n->kd->slider.bubble_format));
    n->kd->slider.bubble_format = NULL;
  }
  if (n->textfield_regex) {
    yis_release_val(YV_STR(n->textfield_regex));
    n->textfield_regex = NULL;
  }
  if (n->textfield_prefix) {
    yis_release_val(YV_STR(n->textfield_prefix));
    n->textfield_prefix = NULL;
  }
  if (n->textfield_suffix) {
    yis_release_val(YV_STR(n->textfield_suffix));
    n->textfield_suffix = NULL;
  }
  if (n->view_id) {
    yis_release_val(YV_STR(n->view_id));
    n->view_id = NULL;
  }
  if (n->view_active_id) {
    yis_release_val(YV_STR(n->view_active_id));
    n->view_active_id = NULL;
  }
  if (n->view_builder_ids) {
    for (size_t i = 0; i < n->view_builder_len; i++) {
      if (n->view_builder_ids[i]) yis_release_val(YV_STR(n->view_builder_ids[i]));
      if (n->view_builders[i]) yis_release_val(YV_FN(n->view_builders[i]));
    }
    free(n->view_builder_ids);
    free(n->view_builders);
    n->view_builder_ids = NULL;
    n->view_builders = NULL;
    n->view_builder_len = 0;
    n->view_builder_cap = 0;
  }
  if (n->children) {
    for (size_t i = 0; i < n->len; i++) {
      yis_release_val(YV_OBJ(n->children[i]));
    }
    free(n->children);
    n->children = NULL;
    n->len = 0;
    n->cap = 0;
  }
  if (n->kind == COGITO_CARD) {
    if (n->kd->card.header_image) {
      yis_release_val(YV_OBJ(n->kd->card.header_image));
      n->kd->card.header_image = NULL;
    }
    if (n->kd->card.actions) {
      free(n->kd->card.actions);
      n->kd->card.actions = NULL;
    }
  }
  if (n->dialog) {
    yis_release_val(YV_OBJ(n->dialog));
    n->dialog = NULL;
  }
  // Free per-kind data
  free(n->kd);
  n->kd = NULL;
}

static CogitoNode* cogito_node_new(CogitoKind kind) {
  CogitoNode* n = (CogitoNode*)yis_obj_new(sizeof(CogitoNode), cogito_node_drop);
  // Zero everything after the YisObj base (which yis_obj_new already set up)
  memset((char*)n + sizeof(YisObj), 0, sizeof(CogitoNode) - sizeof(YisObj));
  n->kind = kind;
  // Non-zero common defaults
  n->selected = -1;
  n->resizable = true;
  n->editable = true;
  n->grid_span_x = 0;
  n->grid_span_y = 0;
  n->grid_align_x = 0;
  n->grid_align_y = 0;
  n->text_color = cogito_rgba(30, 30, 30, 255);
  n->border_color = cogito_rgba(25, 25, 25, 255);
  n->selection_color = cogito_rgba(208, 220, 245, 255);
  n->track_color = cogito_rgba(170, 170, 170, 255);
  n->track_on_color = cogito_rgba(90, 160, 110, 255);
  n->knob_color = cogito_rgba(250, 250, 250, 255);
  n->check_color = cogito_rgba(30, 30, 30, 255);
  n->font_weight = 400;
  n->opacity = 1.0f;
  n->opacity_from = 1.0f;
  n->opacity_target = 1.0f;
  // Layout widgets default to 0 gap, content widgets default to 8 for natural spacing
  bool is_layout = (kind == COGITO_VSTACK || kind == COGITO_HSTACK || kind == COGITO_ZSTACK ||
                    kind == COGITO_FIXED || kind == COGITO_SCROLLER || kind == COGITO_GRID ||
                    kind == COGITO_REVEALER || kind == COGITO_BIN);
  n->gap = is_layout ? 0 : 8;
  n->revealer_show_children = true;
  n->bin_accent_set = false;
  n->bin_accent_color = cogito_rgba(0, 0, 0, 0);

  // Sane geometry defaults:
  // - margin: 0 for everything
  // - padding: 6 for leaf-like widgets, 0 for container-capable widgets
  // - alignment: begin/begin baseline (intrinsic-first + top-left origin)
  // - expand: explicit opt-in only
  bool can_have_children =
    kind == COGITO_APPBAR ||
    kind == COGITO_BIN ||
    kind == COGITO_BOTTOM_NAV ||
    kind == COGITO_CAROUSEL ||
    kind == COGITO_CAROUSEL_ITEM ||
    kind == COGITO_COLORPICKER ||
    kind == COGITO_DIALOG ||
    kind == COGITO_DIALOG_SLOT ||
    kind == COGITO_FIXED ||
    kind == COGITO_GRID ||
    kind == COGITO_HSTACK ||
    kind == COGITO_NAV_RAIL ||
    kind == COGITO_POPOVER ||
    kind == COGITO_SCROLLER ||
    kind == COGITO_REVEALER ||
    kind == COGITO_BUTTON_GROUP ||
    kind == COGITO_TOASTS ||
    kind == COGITO_TOOLBAR ||
    kind == COGITO_TREEVIEW ||
    kind == COGITO_VIEWSWITCHER ||
    kind == COGITO_VSTACK ||
    kind == COGITO_WINDOW ||
    kind == COGITO_ZSTACK ||
    kind == COGITO_CARD ||
    kind == COGITO_BOTTOM_SHEET ||
    kind == COGITO_SWITCHBAR ||
    kind == COGITO_CONTENT_LIST ||
    kind == COGITO_EMPTY_PAGE ||
    kind == COGITO_TIP_VIEW ||
    kind == COGITO_SETTINGS_WINDOW ||
    kind == COGITO_SETTINGS_PAGE ||
    kind == COGITO_SETTINGS_LIST ||
    kind == COGITO_SETTINGS_ROW ||
    kind == COGITO_WELCOME_SCREEN ||
    kind == COGITO_VIEW_DUAL ||
    kind == COGITO_VIEW_CHOOSER ||
    kind == COGITO_ABOUT_WINDOW ||
    kind == COGITO_MENU_SECTION ||
    kind == COGITO_SIDE_SHEET;

  n->margin_left = 0;
  n->margin_top = 0;
  n->margin_right = 0;
  n->margin_bottom = 0;

  int default_pad = can_have_children ? 0 : 6;
  n->padding_left = default_pad;
  n->padding_top = default_pad;
  n->padding_right = default_pad;
  n->padding_bottom = default_pad;

  n->align = 0;
  n->hexpand = false;
  n->vexpand = false;
  n->hexpand_set = false;
  n->vexpand_set = false;
  n->label_max_width_chars = 0;
  n->label_max_width_chars_set = false;
  n->grid_col = 0;
  n->grid_row = 0;
  n->grid_pos_set = false;

  // Kind-specific non-zero defaults
  switch (kind) {
    case COGITO_SCROLLER: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoScrollerData)); n->kd->scroller.vert = true; break;
    case COGITO_SLIDER:
      n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoSliderData));
      n->kd->slider.show_value_bubble = true;
      n->kd->slider.bubble_scale = 1.0;
      break;
    case COGITO_STEPPER: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoStepperData)); n->kd->stepper.max = 100.0; n->kd->stepper.step = 1.0; break;
    case COGITO_COLORPICKER:
      n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoColorpickerData));
      n->kd->colorpicker.h = 236.0;
      n->kd->colorpicker.c = 41.0;
      n->kd->colorpicker.t = 40.0;
      cogito_colorpicker_sync_hex(n);
      break;
    case COGITO_IMAGE: case COGITO_AVATAR: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoImageData)); n->kd->image.avg_lum = 0.5f; break;
    case COGITO_VIEW_DUAL:
      n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoViewDualData));
      n->kd->view_dual.ratio = 0.5f;
      n->kd->view_dual.anim_ratio = 0.5f;
      n->kd->view_dual.anim_from = 0.5f;
      n->kd->view_dual.anim_to = 0.5f;
      n->kd->view_dual.animating = false;
      n->kd->view_dual.show_handle = true;
      n->kd->view_dual.edge_snap = 0;
      break;
    case COGITO_WINDOW:
      n->window_size_class = COGITO_WINDOW_SIZE_MEDIUM;
      break;
    case COGITO_SIDE_SHEET:
      n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoSideSheetData));
      n->kd->side_sheet.mode = COGITO_SIDE_SHEET_MODAL;
      n->kd->side_sheet.preferred_mode = COGITO_SIDE_SHEET_MODAL;
      break;
    // Kinds with per-kind data but only zero-initialized defaults
    case COGITO_DATEPICKER: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoDatepickerData)); break;
    case COGITO_TIMEPICKER: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoTimepickerData)); break;
    case COGITO_APPBAR: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoAppbarData)); break;
    case COGITO_GRID: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoGridData)); break;
    case COGITO_CAROUSEL: case COGITO_CAROUSEL_ITEM: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoCarouselData)); break;
    case COGITO_TEXTFIELD: case COGITO_TEXTVIEW: case COGITO_SEARCHFIELD: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoTextInputData)); break;
    case COGITO_NAV_RAIL: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoNavRailData)); break;
    case COGITO_BOTTOM_NAV: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoBottomNavData)); break;
    case COGITO_BADGE: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoBadgeData)); break;
    case COGITO_PROGRESS: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoProgressData)); break;
    case COGITO_BUTTON: case COGITO_FONTBUTTON: case COGITO_ICONBTN: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoButtonData)); break;
    case COGITO_CHIP: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoChipData)); break;
    case COGITO_BOTTOM_SHEET: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoBottomSheetData)); break;
    case COGITO_FAB: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoFabData)); break;
    case COGITO_FAB_MENU: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoFabMenuData)); break;
    case COGITO_DIVIDER: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoDividerData)); break;
    case COGITO_LABEL: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoLabelData)); break;
    case COGITO_ACTIVE_INDICATOR: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoIndicatorData)); break;
    case COGITO_SPLIT_BUTTON: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoSplitBtnData)); break;
    case COGITO_DRAWING_AREA: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoDrawingAreaData)); break;
    case COGITO_WEBVIEW: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoWebviewData)); break;
    case COGITO_SHAPE: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoShapeData)); break;
    case COGITO_MENU_SECTION: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoMenuSectionData)); break;
    case COGITO_TOOLBAR: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoToolbarData)); break;
    case COGITO_BUTTON_GROUP: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoButtonGroupData)); break;
    case COGITO_CARD: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoCardData)); break;
    case COGITO_WINDOW_BAR: n->kd = (CogitoKindData*)calloc(1, sizeof(CogitoWindowBarData)); break;
    default: break;
  }
  cogito_apply_style_to_node(n);
  return n;
}

static bool cogito_str_has_text(const YisStr* s) {
  return s && s->data && s->data[0];
}

static const char* cogito_node_default_a11y_role(CogitoKind kind) {
  switch (kind) {
    case COGITO_WINDOW: return "window";
    case COGITO_DIALOG:
    case COGITO_SETTINGS_WINDOW:
    case COGITO_ABOUT_WINDOW:
    case COGITO_BOTTOM_SHEET:
    case COGITO_SIDE_SHEET:
      return "dialog";
    case COGITO_BUTTON:
    case COGITO_FONTBUTTON:
    case COGITO_ICONBTN:
    case COGITO_FAB:
    case COGITO_FAB_MENU:
    case COGITO_SPLIT_BUTTON:
    case COGITO_CHIP:
      return "button";
    case COGITO_CHECKBOX: return "checkbox";
    case COGITO_SWITCH:
    case COGITO_SWITCHBAR:
      return "switch";
    case COGITO_TEXTFIELD:
    case COGITO_TEXTVIEW:
    case COGITO_SEARCHFIELD:
      return "textbox";
    case COGITO_DROPDOWN: return "combobox";
    case COGITO_SLIDER:
    case COGITO_STEPPER:
      return "slider";
    case COGITO_PROGRESS: return "progressbar";
    case COGITO_DATEPICKER:
    case COGITO_TIMEPICKER:
    case COGITO_COLORPICKER:
      return "picker";
    case COGITO_TABS: return "tablist";
    case COGITO_NAV_RAIL:
    case COGITO_BOTTOM_NAV:
    case COGITO_BUTTON_GROUP:
    case COGITO_VIEW_CHOOSER:
      return "radiogroup";
    case COGITO_CONTENT_LIST:
      return "list";
    case COGITO_GRID: return "grid";
    case COGITO_TREEVIEW: return "tree";
    case COGITO_TOOLBAR:
    case COGITO_APPBAR:
    case COGITO_WINDOW_BAR:
      return "toolbar";
    case COGITO_LABEL:
    case COGITO_BADGE:
    case COGITO_ACTIVE_INDICATOR:
      return "text";
    case COGITO_IMAGE:
    case COGITO_AVATAR:
      return "img";
    case COGITO_WEBVIEW:
      return "document";
    case COGITO_DIVIDER: return "separator";
    case COGITO_TOAST:
    case COGITO_TOASTS:
    case COGITO_BANNER:
      return "status";
    case COGITO_TOOLTIP:
    case COGITO_TIP_VIEW:
      return "tooltip";
    case COGITO_VSTACK:
    case COGITO_HSTACK:
    case COGITO_ZSTACK:
    case COGITO_REVEALER:
    case COGITO_BIN:
    case COGITO_FIXED:
    case COGITO_SCROLLER:
    case COGITO_CARD:
    case COGITO_MENU_SECTION:
    case COGITO_SETTINGS_PAGE:
    case COGITO_SETTINGS_LIST:
    case COGITO_SETTINGS_ROW:
    case COGITO_WELCOME_SCREEN:
    case COGITO_EMPTY_PAGE:
    case COGITO_VIEW_DUAL:
    case COGITO_CAROUSEL:
    case COGITO_CAROUSEL_ITEM:
      return "group";
    default:
      return "region";
  }
}

static const char* cogito_node_effective_a11y_role(const CogitoNode* n) {
  if (!n) return "region";
  if (cogito_str_has_text(n->a11y_role)) return n->a11y_role->data;
  return cogito_node_default_a11y_role(n->kind);
}

static const char* cogito_node_effective_a11y_label(const CogitoNode* n, char* out, size_t out_sz) {
  if (!n || !out || out_sz == 0) return "";
  out[0] = '\0';
  if (cogito_str_has_text(n->a11y_label)) return n->a11y_label->data;
  if (cogito_str_has_text(n->text)) return n->text->data;
  if (cogito_str_has_text(n->subtitle)) return n->subtitle->data;
  if (cogito_str_has_text(n->hint)) return n->hint->data;
  if (cogito_str_has_text(n->tooltip)) return n->tooltip->data;
  if (cogito_str_has_text(n->action_text)) return n->action_text->data;
  if (cogito_str_has_text(n->view_id)) return n->view_id->data;
  if (cogito_str_has_text(n->icon)) return n->icon->data;
  snprintf(out, out_sz, "%s", cogito_node_effective_a11y_role(n));
  return out;
}

static void cogito_a11y_emit_event(const char* event, const CogitoNode* n, const char* detail) {
  const char* path = getenv("COGITO_A11Y_EVENTS_PATH");
  if (!path || !path[0]) {
#if defined(__APPLE__)
    path = "/tmp/cogito_a11y_events_macos.jsonl";
#else
    path = "/tmp/cogito_a11y_events_linux.jsonl";
#endif
  }
  FILE* f = fopen(path, "a");
  if (!f) return;
  char label_buf[192];
  const char* label = cogito_node_effective_a11y_label(n, label_buf, sizeof(label_buf));
  const char* role = cogito_node_effective_a11y_role(n);
  fprintf(f, "{\"event\":\"");
  for (const char* p = event ? event : "event"; *p; p++) {
    if (*p == '"' || *p == '\\') fputc('\\', f);
    if (*p == '\n') { fputs("\\n", f); continue; }
    fputc(*p, f);
  }
  fputs("\",\"role\":\"", f);
  for (const char* p = role ? role : "region"; *p; p++) {
    if (*p == '"' || *p == '\\') fputc('\\', f);
    if (*p == '\n') { fputs("\\n", f); continue; }
    fputc(*p, f);
  }
  fputs("\",\"label\":\"", f);
  for (const char* p = label ? label : ""; *p; p++) {
    if (*p == '"' || *p == '\\') fputc('\\', f);
    if (*p == '\n') { fputs("\\n", f); continue; }
    fputc(*p, f);
  }
  fputs("\",\"detail\":\"", f);
  for (const char* p = detail ? detail : ""; *p; p++) {
    if (*p == '"' || *p == '\\') fputc('\\', f);
    if (*p == '\n') { fputs("\\n", f); continue; }
    fputc(*p, f);
  }
  fputs("\"}\n", f);
  fclose(f);
}

static void cogito_a11y_dump_node_json(FILE* f, const CogitoNode* n) {
  if (!f || !n) return;
  char label_buf[192];
  const char* role = cogito_node_effective_a11y_role(n);
  const char* label = cogito_node_effective_a11y_label(n, label_buf, sizeof(label_buf));
  const char* state = "normal";
  if (n->disabled) state = "disabled";
  else if (n->checked) state = "checked";
  fprintf(f, "{\"id\":\"%p\",\"role\":\"", (void*)n);
  for (const char* p = role ? role : "region"; *p; p++) {
    if (*p == '"' || *p == '\\') fputc('\\', f);
    if (*p == '\n') { fputs("\\n", f); continue; }
    fputc(*p, f);
  }
  fputs("\",\"label\":\"", f);
  for (const char* p = label ? label : ""; *p; p++) {
    if (*p == '"' || *p == '\\') fputc('\\', f);
    if (*p == '\n') { fputs("\\n", f); continue; }
    fputc(*p, f);
  }
  fprintf(f, "\",\"state\":\"%s\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"children\":[",
          state, n->x, n->y, n->w, n->h);
  bool first = true;
  for (size_t i = 0; i < n->len; i++) {
    const CogitoNode* c = n->children[i];
    if (!c || c->w <= 0 || c->h <= 0) continue;
    if (!first) fprintf(f, ",");
    cogito_a11y_dump_node_json(f, c);
    first = false;
  }
  fprintf(f, "]}");
}

static void cogito_a11y_sync_tree(CogitoNode* win) {
  if (!win || win->kind != COGITO_WINDOW) return;
  const char* path = getenv("COGITO_A11Y_TREE_PATH");
  if (!path || !path[0]) {
#if defined(__APPLE__)
    path = "/tmp/cogito_a11y_tree_macos.json";
#else
    path = "/tmp/cogito_a11y_tree_linux.json";
#endif
  }
  FILE* f = fopen(path, "w");
  if (!f) return;
  cogito_a11y_dump_node_json(f, win);
  fputc('\n', f);
  fclose(f);
}

static void cogito_apply_window_a11y_label(CogitoNode* n) {
  if (!n || n->kind != COGITO_WINDOW) return;
  if (!cogito_backend_ready || !cogito_backend || !cogito_active_backend_window) return;

  char label_buf[256];
  const char* label = cogito_node_effective_a11y_label(n, label_buf, sizeof(label_buf));

#if defined(__APPLE__)
  cogito_macos_set_window_a11y_label(label);
#endif

  if (cogito_backend->window_set_title) {
    const char* visible_title = (n->text && n->text->data && n->text->data[0])
      ? n->text->data
      : COGITO_USER_VERSION;
    const char* title = cogito_str_has_text(n->a11y_label) ? label : visible_title;
    cogito_backend->window_set_title(cogito_active_backend_window, title);
  }
  cogito_a11y_sync_tree(n);
}

static void cogito_node_set_text(CogitoNode* n, YisStr* s) {
  if (!n) return;
  if (s) yis_retain_val(YV_STR(s));
  if (n->text) yis_release_val(YV_STR(n->text));
  n->text = s;
  int len = n->text ? (int)n->text->len : 0;
  // Only update text_input for widgets that store editing state in text_input.
  if (n->kind == COGITO_TEXTFIELD || n->kind == COGITO_TEXTVIEW || n->kind == COGITO_SEARCHFIELD) {
    if (n->kd->text_input.caret > len) n->kd->text_input.caret = len;
    if (n->kd->text_input.sel_start > len) n->kd->text_input.sel_start = len;
    if (n->kd->text_input.sel_end > len) n->kd->text_input.sel_end = len;
  } else if (n->kind == COGITO_COLORPICKER) {
    if (n->kd->colorpicker.caret > len) n->kd->colorpicker.caret = len;
    if (n->kd->colorpicker.sel_start > len) n->kd->colorpicker.sel_start = len;
    if (n->kd->colorpicker.sel_end > len) n->kd->colorpicker.sel_end = len;
  }
  if (n->kind == COGITO_TEXTFIELD) {
    cogito_textfield_refresh_regex_state(n);
  }
}

static void cogito_node_set_subtitle(CogitoNode* n, YisStr* s) {
  if (!n) return;
  if (s) yis_retain_val(YV_STR(s));
  if (n->subtitle) yis_release_val(YV_STR(n->subtitle));
  n->subtitle = s;
}

static void cogito_node_set_hint(CogitoNode* n, YisStr* s) {
  if (!n) return;
  if (s) yis_retain_val(YV_STR(s));
  if (n->hint) yis_release_val(YV_STR(n->hint));
  n->hint = s;
}

static void cogito_node_set_tooltip(CogitoNode* n, YisStr* s) {
  if (!n) return;
  if (s) yis_retain_val(YV_STR(s));
  if (n->tooltip) yis_release_val(YV_STR(n->tooltip));
  n->tooltip = s;
}

static void cogito_node_set_icon(CogitoNode* n, YisStr* s) {
  if (!n) return;
  if (s) yis_retain_val(YV_STR(s));
  if (n->icon) yis_release_val(YV_STR(n->icon));
  n->icon = s;
}

static void cogito_node_set_group(CogitoNode* n, YisVal group) {
  if (!n) return;
  if (group.tag != EVT_NULL) yis_retain_val(group);
  if (n->group.tag != EVT_NULL) yis_release_val(n->group);
  n->group = group;
}

static void cogito_children_add(CogitoNode* parent, CogitoNode* child) {
  if (!parent || !child) return;
  if (parent->len + 1 > parent->cap) {
    size_t next = parent->cap == 0 ? 4 : parent->cap * 2;
    parent->children = (CogitoNode**)realloc(parent->children, sizeof(CogitoNode*) * next);
    parent->cap = next;
  }
  parent->children[parent->len++] = child;
  child->parent = parent;
  yis_retain_val(YV_OBJ(child));
}

static void cogito_node_add_menu(CogitoNode* n, YisStr* label, YisFn* handler) {
  if (!n) return;
  if (n->menu_len + 1 > n->menu_cap) {
    size_t next = n->menu_cap == 0 ? 4 : n->menu_cap * 2;
    n->menu_labels = (YisStr**)realloc(n->menu_labels, sizeof(YisStr*) * next);
    n->menu_handlers = (YisFn**)realloc(n->menu_handlers, sizeof(YisFn*) * next);
    n->menu_section_after = (bool*)realloc(n->menu_section_after, sizeof(bool) * next);
    n->menu_icons = (YisStr**)realloc(n->menu_icons, sizeof(YisStr*) * next);
    n->menu_shortcuts = (YisStr**)realloc(n->menu_shortcuts, sizeof(YisStr*) * next);
    n->menu_submenu = (bool*)realloc(n->menu_submenu, sizeof(bool) * next);
    n->menu_toggled = (bool*)realloc(n->menu_toggled, sizeof(bool) * next);
    n->menu_cap = next;
  }
  n->menu_labels[n->menu_len] = label;
  n->menu_handlers[n->menu_len] = handler;
  n->menu_section_after[n->menu_len] = false;
  n->menu_icons[n->menu_len] = NULL;
  n->menu_shortcuts[n->menu_len] = NULL;
  n->menu_submenu[n->menu_len] = false;
  n->menu_toggled[n->menu_len] = false;
  if (label) yis_retain_val(YV_STR(label));
  if (handler) yis_retain_val(YV_FN(handler));
  n->menu_len++;
}

// Set leading icon on the last-added menu item
static void cogito_node_menu_set_icon(CogitoNode* n, YisStr* icon) {
  if (!n || n->menu_len == 0) return;
  size_t idx = n->menu_len - 1;
  if (n->menu_icons[idx]) yis_release_val(YV_STR(n->menu_icons[idx]));
  n->menu_icons[idx] = icon;
  if (icon) yis_retain_val(YV_STR(icon));
}

// Set trailing shortcut text on the last-added menu item
static void cogito_node_menu_set_shortcut(CogitoNode* n, YisStr* shortcut) {
  if (!n || n->menu_len == 0) return;
  size_t idx = n->menu_len - 1;
  if (n->menu_shortcuts[idx]) yis_release_val(YV_STR(n->menu_shortcuts[idx]));
  n->menu_shortcuts[idx] = shortcut;
  if (shortcut) yis_retain_val(YV_STR(shortcut));
}

// Mark the last-added menu item as a submenu (trailing arrow)
static void cogito_node_menu_set_submenu(CogitoNode* n, bool submenu) {
  if (!n || n->menu_len == 0) return;
  n->menu_submenu[n->menu_len - 1] = submenu;
}

// Mark the last-added menu item as toggled
static void cogito_node_menu_set_toggled(CogitoNode* n, bool toggled) {
  if (!n || n->menu_len == 0) return;
  n->menu_toggled[n->menu_len - 1] = toggled;
}

// Add a menu item with a section divider after it
void cogito_node_add_menu_section(CogitoNode* n, YisStr* label, YisFn* handler) {
  if (!n) return;
  // Mark the last item as having a section divider after it
  if (n->menu_len > 0) {
    n->menu_section_after[n->menu_len - 1] = true;
  }
  // Now add the new item
  cogito_node_add_menu(n, label, handler);
}

// WebView content renderer (needed by intrinsic sizing below and by draw)
#include "widgets/util/webview_render.inc"

static void cogito_intrinsic_size(CogitoNode* n, int* out_w, int* out_h) {
  if (cogito_node_is_hidden(n)) {
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    return;
  }
  int text_w = 0;
  int text_h = cogito_text_height();
  if (n->text) {
    int size = cogito_node_font_size(n);
    text_w = cogito_text_width_size_node(n, n->text->data, size);
    text_h = cogito_text_height_size(size);
  }
  int w = 0;
  int h = 0;
  int pad = 8;
  switch (n->kind) {

    #include "widgets/intrinsic/button.inc"

    #include "widgets/intrinsic/fontbutton.inc"

    #include "widgets/intrinsic/carousel.inc"

    #include "widgets/intrinsic/carousel_item.inc"

    #include "widgets/intrinsic/iconbtn.inc"

    #include "widgets/intrinsic/image.inc"

    #include "widgets/intrinsic/label.inc"

    #include "widgets/intrinsic/checkbox.inc"

    #include "widgets/intrinsic/chip.inc"

    #include "widgets/intrinsic/switch.inc"

    #include "widgets/intrinsic/textfield.inc"

    #include "widgets/intrinsic/textview.inc"

    #include "widgets/intrinsic/searchfield.inc"

    #include "widgets/intrinsic/dropdown.inc"

    #include "widgets/intrinsic/datepicker.inc"

    #include "widgets/intrinsic/fab.inc"

    #include "widgets/intrinsic/fab_menu.inc"

    #include "widgets/intrinsic/nav_rail.inc"

    #include "widgets/intrinsic/bottom_nav.inc"

    #include "widgets/intrinsic/stepper.inc"

    #include "widgets/intrinsic/slider.inc"

    #include "widgets/intrinsic/tabs.inc"

    #include "widgets/intrinsic/buttongroup.inc"

    #include "widgets/intrinsic/view_switcher.inc"

    #include "widgets/intrinsic/progress.inc"

    #include "widgets/intrinsic/divider.inc"

    #include "widgets/intrinsic/menu_section.inc"

    #include "widgets/intrinsic/webview.inc"

    #include "widgets/intrinsic/card.inc"

    #include "widgets/intrinsic/avatar.inc"

    #include "widgets/intrinsic/badge.inc"

    #include "widgets/intrinsic/banner.inc"

    #include "widgets/intrinsic/bottom_sheet.inc"

    #include "widgets/intrinsic/side_sheet.inc"

    #include "widgets/intrinsic/timepicker.inc"

    #include "widgets/intrinsic/active_indicator.inc"

    #include "widgets/intrinsic/switchbar.inc"

    #include "widgets/intrinsic/content_list.inc"

    #include "widgets/intrinsic/empty_page.inc"

    #include "widgets/intrinsic/tip_view.inc"

    #include "widgets/intrinsic/settings_window.inc"

    #include "widgets/intrinsic/settings_page.inc"

    #include "widgets/intrinsic/settings_list.inc"

    #include "widgets/intrinsic/settings_row.inc"

    #include "widgets/intrinsic/welcome_screen.inc"

    #include "widgets/intrinsic/view_dual.inc"

    #include "widgets/intrinsic/view_chooser.inc"

    #include "widgets/intrinsic/about_window.inc"

    #include "widgets/intrinsic/split_button.inc"

    #include "widgets/intrinsic/drawing_area.inc"

    #include "widgets/intrinsic/shape.inc"

    #include "widgets/intrinsic/revealer.inc"

    #include "widgets/intrinsic/bin.inc"

    #include "widgets/intrinsic/treeview.inc"

    #include "widgets/intrinsic/colorpicker.inc"

    #include "widgets/intrinsic/toasts.inc"

    #include "widgets/intrinsic/toolbar.inc"

    #include "widgets/intrinsic/toast.inc"

    #include "widgets/intrinsic/tooltip.inc"

    #include "widgets/intrinsic/fixed.inc"

    #include "widgets/intrinsic/scroller.inc"

    #include "widgets/intrinsic/zstack.inc"

    #include "widgets/intrinsic/list.inc"

    #include "widgets/intrinsic/grid.inc"

    #include "widgets/intrinsic/dialog.inc"

    #include "widgets/intrinsic/dialog_slot.inc"

    #include "widgets/intrinsic/popover.inc"

    #include "widgets/intrinsic/appbar.inc"

    #include "widgets/intrinsic/vstack.inc"

    #include "widgets/intrinsic/hstack.inc"

    #include "widgets/intrinsic/window.inc"

    #include "widgets/intrinsic/window_bar.inc"

    case COGITO_KIND_COUNT:
      break;

  }
  w += n->padding_left + n->padding_right;
  h += n->padding_top + n->padding_bottom;
  cogito_apply_size_constraints(n, &w, &h);
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
}
