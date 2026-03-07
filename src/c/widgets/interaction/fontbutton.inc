static YisVal cogito_hstack_new(void);
static YisVal cogito_vstack_new(void);
static YisVal cogito_scroller_new(void);
static void cogito_scroller_set_axes(YisVal scv, YisVal horzv, YisVal vertv);
static YisVal cogito_content_list_new(void);
static CogitoNode* cogito_label_new_obj(YisStr* label);
static void cogito_content_list_on_select(YisVal listv, YisVal handler);
static void cogito_content_list_on_activate(YisVal listv, YisVal handler);
static void cogito_window_set_dialog(YisVal winv, YisVal dialogv);
static void cogito_container_add(YisVal parentv, YisVal childv);
static void cogito_container_set_align(YisVal nodev, YisVal alignv);
static void cogito_container_set_hexpand(YisVal nodev, YisVal expandv);
static void cogito_container_set_vexpand(YisVal nodev, YisVal expandv);
static void cogito_container_set_gap(YisVal nodev, YisVal gapv);
static void cogito_container_set_padding(YisVal nodev, YisVal top, YisVal right, YisVal bottom, YisVal left);
static void cogito_label_set_wrap(YisVal labelv, YisVal onv);
static void cogito_label_set_ellipsis(YisVal labelv, YisVal onv);

typedef struct {
  CogitoNode* owner;
  CogitoNode* dialog;
  CogitoNode* list;
  CogitoNode* preview_name;
  CogitoNode* preview_display;
  CogitoNode* preview_body;
  CogitoNode* preview_emphasis;
  char* font_blob;
  char* extra_name;
  char** font_names;
  int font_count;
  int selected;
} CogitoFontDialogState;

static CogitoFontDialogState cogito_font_dialog_state = {0};

static YisFn* cogito_font_dialog_make_fn(YisFnImpl impl) {
  YisFn* fn = (YisFn*)malloc(sizeof(*fn));
  if (!fn) return NULL;
  fn->ref = 1;
  fn->arity = -1;
  fn->env = NULL;
  fn->fn = impl;
  return fn;
}

static YisStr* cogito_font_dialog_new_str(const char* text) {
  return stdr_str_lit(text ? text : "");
}

static CogitoNode* cogito_font_dialog_new_label_node(const char* text) {
  YisStr* s = cogito_font_dialog_new_str(text);
  CogitoNode* n = cogito_label_new_obj(s);
  yis_release_val(YV_STR(s));
  return n;
}

static CogitoNode* cogito_font_dialog_new_button_node(const char* text) {
  YisStr* s = cogito_font_dialog_new_str(text);
  CogitoNode* n = cogito_button_new_obj(s);
  yis_release_val(YV_STR(s));
  return n;
}

static CogitoNode* cogito_font_dialog_new_dialog_node(const char* text) {
  YisStr* s = cogito_font_dialog_new_str(text);
  CogitoNode* n = cogito_dialog_new_obj(s);
  yis_release_val(YV_STR(s));
  return n;
}

static void cogito_font_dialog_set_text_node(CogitoNode* n, const char* text) {
  if (!n) return;
  YisStr* s = cogito_font_dialog_new_str(text);
  cogito_node_set_text(n, s);
  yis_release_val(YV_STR(s));
}

static void cogito_font_dialog_set_class_node(CogitoNode* n, const char* class_name) {
  if (!n) return;
  YisStr* s = cogito_font_dialog_new_str(class_name);
  cogito_node_set_class(YV_OBJ(n), YV_STR(s));
  yis_release_val(YV_STR(s));
}

static void cogito_font_dialog_reset(void) {
  if (cogito_font_dialog_state.font_names) free(cogito_font_dialog_state.font_names);
  if (cogito_font_dialog_state.font_blob) free(cogito_font_dialog_state.font_blob);
  if (cogito_font_dialog_state.extra_name) free(cogito_font_dialog_state.extra_name);
  memset(&cogito_font_dialog_state, 0, sizeof(cogito_font_dialog_state));
}

static char* cogito_font_dialog_trim(char* s) {
  if (!s) return s;
  while (*s && isspace((unsigned char)*s)) s++;
  size_t len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[len - 1])) {
    s[len - 1] = '\0';
    len--;
  }
  return s;
}

static bool cogito_font_dialog_has_name(char** names, int count, const char* name) {
  if (!names || !name || !name[0]) return false;
  for (int i = 1; i < count; i++) {
    if (names[i] && cogito_font_family_eq(names[i], name)) return true;
  }
  return false;
}

static bool cogito_font_dialog_prepare_names(const char* current_name) {
  cogito_font_dialog_state.font_blob = cogito_list_font_family_names();
  int cap = 32;
  if (cogito_font_dialog_state.font_blob) {
    cap = 2;
    for (char* p = cogito_font_dialog_state.font_blob; *p; p++) {
      if (*p == '\n' || *p == ',') cap++;
    }
  }
  cogito_font_dialog_state.font_names = (char**)calloc((size_t)cap, sizeof(char*));
  if (!cogito_font_dialog_state.font_names) return false;

  cogito_font_dialog_state.font_names[0] = NULL;
  cogito_font_dialog_state.font_count = 1;

  if (cogito_font_dialog_state.font_blob) {
    char* p = cogito_font_dialog_state.font_blob;
    while (*p) {
      char* start = p;
      while (*p && *p != '\n' && *p != ',') p++;
      char save = *p;
      *p = '\0';
      char* trimmed = cogito_font_dialog_trim(start);
      if (trimmed[0] && strcmp(trimmed, "System Default") != 0 &&
          !cogito_font_dialog_has_name(cogito_font_dialog_state.font_names,
                                       cogito_font_dialog_state.font_count,
                                       trimmed)) {
        cogito_font_dialog_state.font_names[cogito_font_dialog_state.font_count++] = trimmed;
      }
      if (!save) break;
      p++;
    }
  }

  if (current_name && current_name[0] && strcmp(current_name, "System Default") != 0 &&
      !cogito_font_dialog_has_name(cogito_font_dialog_state.font_names,
                                   cogito_font_dialog_state.font_count,
                                   current_name)) {
    cogito_font_dialog_state.extra_name = strdup(current_name);
    if (cogito_font_dialog_state.extra_name) {
      cogito_font_dialog_state.font_names[cogito_font_dialog_state.font_count++] =
        cogito_font_dialog_state.extra_name;
    }
  }

  cogito_font_dialog_state.selected = 0;
  if (current_name && current_name[0] && strcmp(current_name, "System Default") != 0) {
    for (int i = 1; i < cogito_font_dialog_state.font_count; i++) {
      if (cogito_font_dialog_state.font_names[i] &&
          cogito_font_family_eq(cogito_font_dialog_state.font_names[i], current_name)) {
        cogito_font_dialog_state.selected = i;
        break;
      }
    }
  }
  return true;
}

static const char* cogito_font_dialog_selected_name(void) {
  if (cogito_font_dialog_state.selected <= 0 ||
      cogito_font_dialog_state.selected >= cogito_font_dialog_state.font_count) {
    return NULL;
  }
  return cogito_font_dialog_state.font_names[cogito_font_dialog_state.selected];
}

static void cogito_font_dialog_set_preview_family(CogitoNode* n, const char* family) {
  if (!n) return;
  if (family && family[0]) {
    strncpy(n->font_family, family, sizeof(n->font_family) - 1);
    n->font_family[sizeof(n->font_family) - 1] = '\0';
    n->font_family_set = true;
  } else {
    n->font_family[0] = '\0';
    n->font_family_set = false;
  }
}

static void cogito_font_dialog_refresh_preview(void) {
  const char* family = cogito_font_dialog_selected_name();
  const char* label = family ? family : "System Default";
  if (cogito_font_dialog_state.preview_name) {
    cogito_font_dialog_set_text_node(cogito_font_dialog_state.preview_name, label);
  }
  cogito_font_dialog_set_preview_family(cogito_font_dialog_state.preview_display, family);
  cogito_font_dialog_set_preview_family(cogito_font_dialog_state.preview_body, family);
  cogito_font_dialog_set_preview_family(cogito_font_dialog_state.preview_emphasis, family);
  CogitoNode* win = cogito_font_dialog_state.dialog
    ? cogito_node_window(cogito_font_dialog_state.dialog)
    : NULL;
  if (win) cogito_window_relayout(win);
}

static void cogito_font_dialog_close_only(void) {
  if (cogito_font_dialog_state.dialog) {
    cogito_dialog_close(YV_OBJ(cogito_font_dialog_state.dialog));
  }
  cogito_font_dialog_reset();
}

static void cogito_font_dialog_apply_choice(void) {
  CogitoNode* owner = cogito_font_dialog_state.owner;
  if (!owner || owner->kind != COGITO_FONTBUTTON) {
    cogito_font_dialog_close_only();
    return;
  }
  const char* picked = cogito_font_dialog_selected_name();
  const char* current = (owner->text && owner->text->data) ? owner->text->data : NULL;
  if (current && strcmp(current, "System Default") == 0) current = NULL;
  bool changed = false;
  if ((!current && picked) || (current && !picked)) {
    changed = true;
  } else if (current && picked && strcmp(current, picked) != 0) {
    changed = true;
  }
  cogito_font_dialog_set_text_node(owner, picked ? picked : "System Default");
  CogitoNode* win = cogito_node_window(owner);
  if (win) cogito_window_relayout(win);
  if (changed) cogito_invoke_change(owner);
  cogito_font_dialog_close_only();
}

static YisVal cogito_font_dialog_choose_cb(void* env, int argc, YisVal* argv) {
  (void)env;
  (void)argc;
  (void)argv;
  cogito_font_dialog_apply_choice();
  return YV_NULLV;
}

static YisVal cogito_font_dialog_cancel_cb(void* env, int argc, YisVal* argv) {
  (void)env;
  (void)argc;
  (void)argv;
  cogito_font_dialog_close_only();
  return YV_NULLV;
}

static YisVal cogito_font_dialog_select_cb(void* env, int argc, YisVal* argv) {
  (void)env;
  int idx = (argc > 0 && argv) ? (int)yis_as_int(argv[0]) : -1;
  if (idx < 0 || idx >= cogito_font_dialog_state.font_count) return YV_NULLV;
  cogito_font_dialog_state.selected = idx;
  cogito_font_dialog_refresh_preview();
  return YV_NULLV;
}

static YisVal cogito_font_dialog_activate_cb(void* env, int argc, YisVal* argv) {
  YisVal ret = cogito_font_dialog_select_cb(env, argc, argv);
  cogito_font_dialog_apply_choice();
  return ret;
}

static void cogito_fontbutton_open_dialog(CogitoNode* n) {
  if (!n) return;

  if (cogito_font_dialog_state.dialog) {
    cogito_font_dialog_close_only();
  }

  const char* current = (n->text && n->text->data) ? n->text->data : NULL;
  if (current && strcmp(current, "System Default") == 0) current = NULL;
  if (!cogito_font_dialog_prepare_names(current)) {
    cogito_font_dialog_reset();
    return;
  }

  cogito_font_dialog_state.owner = n;

  CogitoNode* dialog = cogito_font_dialog_new_dialog_node("Choose Font");
  dialog->min_w_set = true;
  dialog->min_w = 620;
  dialog->min_h_set = true;
  dialog->min_h = 560;

  CogitoNode* main = (CogitoNode*)cogito_vstack_new().as.p;
  cogito_container_set_gap(YV_OBJ(main), YV_INT(14));
  cogito_container_set_hexpand(YV_OBJ(main), YV_BOOL(true));
  cogito_container_set_vexpand(YV_OBJ(main), YV_BOOL(true));

  CogitoNode* preview = (CogitoNode*)cogito_vstack_new().as.p;
  cogito_container_set_hexpand(YV_OBJ(preview), YV_BOOL(true));
  preview->min_h_set = true;
  preview->min_h = 168;
  preview->border_radius = 12;
  cogito_container_set_gap(YV_OBJ(preview), YV_INT(12));
  cogito_container_set_padding(YV_OBJ(preview), YV_INT(16), YV_INT(16), YV_INT(16), YV_INT(16));
  cogito_font_dialog_set_class_node(preview, "surface-container");

  CogitoNode* preview_caption = cogito_font_dialog_new_label_node("Preview");
  cogito_font_dialog_set_class_node(preview_caption, "caption");
  cogito_container_add(YV_OBJ(preview), YV_OBJ(preview_caption));

  CogitoNode* preview_name = cogito_font_dialog_new_label_node("");
  cogito_font_dialog_set_class_node(preview_name, "body");
  cogito_container_set_hexpand(YV_OBJ(preview_name), YV_BOOL(true));
  cogito_label_set_ellipsis(YV_OBJ(preview_name), YV_BOOL(true));
  cogito_container_add(YV_OBJ(preview), YV_OBJ(preview_name));

  CogitoNode* preview_display = cogito_font_dialog_new_label_node("Sphinx of Quartz, judge my vow!");
  cogito_font_dialog_set_class_node(preview_display, "view-title");
  cogito_container_set_hexpand(YV_OBJ(preview_display), YV_BOOL(true));
  cogito_label_set_wrap(YV_OBJ(preview_display), YV_BOOL(true));
  cogito_container_add(YV_OBJ(preview), YV_OBJ(preview_display));

  CogitoNode* preview_body = cogito_font_dialog_new_label_node("Sphinx of Quartz, judge my vow!");
  cogito_label_set_wrap(YV_OBJ(preview_body), YV_BOOL(true));
  cogito_font_dialog_set_class_node(preview_body, "body");
  cogito_container_set_hexpand(YV_OBJ(preview_body), YV_BOOL(true));
  cogito_container_add(YV_OBJ(preview), YV_OBJ(preview_body));

  CogitoNode* preview_emphasis = cogito_font_dialog_new_label_node("Sphinx of Quartz, judge my vow!");
  cogito_font_dialog_set_class_node(preview_emphasis, "content-title emphasized");
  cogito_container_set_hexpand(YV_OBJ(preview_emphasis), YV_BOOL(true));
  cogito_label_set_wrap(YV_OBJ(preview_emphasis), YV_BOOL(true));
  cogito_container_add(YV_OBJ(preview), YV_OBJ(preview_emphasis));

  cogito_container_add(YV_OBJ(main), YV_OBJ(preview));

  CogitoNode* list_scroller = (CogitoNode*)cogito_scroller_new().as.p;
  cogito_scroller_set_axes(YV_OBJ(list_scroller), YV_BOOL(false), YV_BOOL(true));
  cogito_font_dialog_set_class_node(list_scroller, "transparent");
  list_scroller->min_h_set = true;
  list_scroller->min_h = 260;
  cogito_container_set_hexpand(YV_OBJ(list_scroller), YV_BOOL(true));
  cogito_container_set_vexpand(YV_OBJ(list_scroller), YV_BOOL(true));

  CogitoNode* list = (CogitoNode*)cogito_content_list_new().as.p;
  cogito_container_set_hexpand(YV_OBJ(list), YV_BOOL(true));
  cogito_container_set_vexpand(YV_OBJ(list), YV_BOOL(true));
  for (int i = 0; i < cogito_font_dialog_state.font_count; i++) {
    const char* name = cogito_font_dialog_state.font_names[i];
    CogitoNode* row = cogito_font_dialog_new_label_node(name ? name : "System Default");
    cogito_font_dialog_set_class_node(row, "body");
    cogito_container_set_hexpand(YV_OBJ(row), YV_BOOL(true));
    cogito_label_set_ellipsis(YV_OBJ(row), YV_BOOL(true));
    cogito_container_set_padding(YV_OBJ(row), YV_INT(10), YV_INT(12), YV_INT(10), YV_INT(12));
    cogito_container_add(YV_OBJ(list), YV_OBJ(row));
  }
  YisFn* select_fn = cogito_font_dialog_make_fn(cogito_font_dialog_select_cb);
  YisFn* activate_fn = cogito_font_dialog_make_fn(cogito_font_dialog_activate_cb);
  if (select_fn) {
    cogito_content_list_on_select(YV_OBJ(list), YV_FN(select_fn));
    yis_release_val(YV_FN(select_fn));
  }
  if (activate_fn) {
    cogito_content_list_on_activate(YV_OBJ(list), YV_FN(activate_fn));
    yis_release_val(YV_FN(activate_fn));
  }
  list->selected = cogito_font_dialog_state.selected;
  cogito_container_add(YV_OBJ(list_scroller), YV_OBJ(list));
  cogito_container_add(YV_OBJ(main), YV_OBJ(list_scroller));

  CogitoNode* actions = (CogitoNode*)cogito_hstack_new().as.p;
  cogito_container_set_gap(YV_OBJ(actions), YV_INT(12));
  cogito_container_set_align(YV_OBJ(actions), YV_INT(2));

  CogitoNode* cancel = cogito_font_dialog_new_button_node("Cancel");
  cogito_font_dialog_set_class_node(cancel, "text");
  YisFn* cancel_fn = cogito_font_dialog_make_fn(cogito_font_dialog_cancel_cb);
  if (cancel_fn) {
    cogito_button_on_click(YV_OBJ(cancel), YV_FN(cancel_fn));
    yis_release_val(YV_FN(cancel_fn));
  }
  cogito_container_add(YV_OBJ(actions), YV_OBJ(cancel));

  CogitoNode* choose = cogito_font_dialog_new_button_node("Choose");
  YisFn* choose_fn = cogito_font_dialog_make_fn(cogito_font_dialog_choose_cb);
  if (choose_fn) {
    cogito_button_on_click(YV_OBJ(choose), YV_FN(choose_fn));
    yis_release_val(YV_FN(choose_fn));
  }
  cogito_container_add(YV_OBJ(actions), YV_OBJ(choose));

  cogito_container_add(YV_OBJ(dialog), YV_OBJ(main));
  cogito_container_add(YV_OBJ(dialog), YV_OBJ(actions));

  cogito_font_dialog_state.dialog = dialog;
  cogito_font_dialog_state.list = list;
  cogito_font_dialog_state.preview_name = preview_name;
  cogito_font_dialog_state.preview_display = preview_display;
  cogito_font_dialog_state.preview_body = preview_body;
  cogito_font_dialog_state.preview_emphasis = preview_emphasis;

  cogito_font_dialog_refresh_preview();

  CogitoNode* win = cogito_node_window(n);
  if (win) {
    cogito_window_set_dialog(YV_OBJ(win), YV_OBJ(dialog));
  } else {
    cogito_font_dialog_close_only();
  }
}

static CogitoNode* cogito_fontbutton_new_obj(void) {
  CogitoNode* btn = cogito_node_new(COGITO_FONTBUTTON);
  btn->button.size = COGITO_BUTTON_SIZE_S;
  cogito_node_set_text(btn, stdr_str_lit("System Default"));
  return btn;
}

static YisVal cogito_fontbutton_new(void) {
  return YV_OBJ(cogito_fontbutton_new_obj());
}

static void cogito_fontbutton_on_change(YisVal nodev, YisVal handler) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.fontbutton_on_change expects fontbutton");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_FONTBUTTON) yis_trap("cogito.fontbutton_on_change expects fontbutton");
  if (n->on_change) yis_release_val(YV_FN(n->on_change));
  if (handler.tag == EVT_FN) {
    n->on_change = (YisFn*)handler.as.p;
    yis_retain_val(YV_FN(n->on_change));
  } else {
    n->on_change = NULL;
  }
}

static __attribute__((unused)) void cogito_fontbutton_set_font(YisVal nodev, YisVal fontv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.fontbutton_set_font expects fontbutton");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_FONTBUTTON) yis_trap("cogito.fontbutton_set_font expects fontbutton");
  YisStr* s = NULL;
  bool temp = false;
  if (fontv.tag == EVT_STR) {
    s = (YisStr*)fontv.as.p;
  } else if (fontv.tag != EVT_NULL) {
    s = stdr_to_string(fontv);
    temp = true;
  }
  if (!s || !s->data || !s->data[0]) {
    cogito_node_set_text(n, stdr_str_lit("System Default"));
  } else {
    cogito_node_set_text(n, s);
  }
  if (temp && s) yis_release_val(YV_STR(s));
  CogitoNode* win = cogito_node_window(n);
  if (win) cogito_window_relayout(win);
}

static __attribute__((unused)) YisVal cogito_fontbutton_get_font(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.fontbutton_get_font expects fontbutton");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_FONTBUTTON) yis_trap("cogito.fontbutton_get_font expects fontbutton");
  YisStr* s = n->text ? n->text : stdr_str_lit("System Default");
  yis_retain_val(YV_STR(s));
  return YV_STR(s);
}

static void cogito_fontbutton_click(CogitoNode* n) {
  if (!n || n->kind != COGITO_FONTBUTTON || n->disabled) return;
  cogito_fontbutton_open_dialog(n);
}