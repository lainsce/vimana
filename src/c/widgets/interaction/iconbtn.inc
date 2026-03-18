static CogitoNode* cogito_iconbtn_new_obj(YisStr* label) {
  CogitoNode* btn = cogito_node_new(COGITO_ICONBTN);
  btn->kd->button.icon_shape = 0; // Default to circle (matches spec)
  btn->kd->button.size = COGITO_BUTTON_SIZE_M; // Default to M size
  btn->kd->button.icon_width = 1; // Default to square (default width)
  cogito_node_set_text(btn, label);
  return btn;
}
static YisVal cogito_iconbtn_new(YisVal text) {
  YisStr* ts = stdr_to_string(text);
  CogitoNode* btn = cogito_iconbtn_new_obj(ts);
  return YV_OBJ(btn);
}
static void cogito_iconbtn_add_menu(YisVal btnv, YisVal label, YisVal handler) {
  cogito_button_add_menu(btnv, label, handler);
}

static void cogito_iconbtn_add_menu_section(YisVal btnv, YisVal label, YisVal handler) {
  cogito_button_add_menu_section(btnv, label, handler);
}

// Icon button setters/getters
static void cogito_iconbtn_set_shape(YisVal btnv, YisVal shapev) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_set_shape expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_set_shape expects iconbtn");
  btn->kd->button.icon_shape = (int)yis_as_int(shapev);
  if (btn->kd->button.icon_shape < 0) btn->kd->button.icon_shape = 0;
  if (btn->kd->button.icon_shape > 1) btn->kd->button.icon_shape = 1;
  cogito_window_relayout(cogito_node_window(btn));
}

static YisVal cogito_iconbtn_get_shape(YisVal btnv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_get_shape expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_get_shape expects iconbtn");
  int v = btn->kd->button.icon_shape;
  if (v < 0) v = 0;
  if (v > 1) v = 1;
  return YV_INT(v);
}

static void cogito_iconbtn_set_color_style(YisVal btnv, YisVal stylev) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_set_color_style expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_set_color_style expects iconbtn");
  btn->kd->button.icon_color_style = (int)yis_as_int(stylev);
  if (btn->kd->button.icon_color_style < 0) btn->kd->button.icon_color_style = 0;
  if (btn->kd->button.icon_color_style > 3) btn->kd->button.icon_color_style = 3;
  cogito_window_relayout(cogito_node_window(btn));
}

static YisVal cogito_iconbtn_get_color_style(YisVal btnv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_get_color_style expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_get_color_style expects iconbtn");
  int v = btn->kd->button.icon_color_style;
  if (v < 0) v = 0;
  if (v > 3) v = 3;
  return YV_INT(v);
}

static void cogito_iconbtn_set_width(YisVal btnv, YisVal widthv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_set_width expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_set_width expects iconbtn");
  btn->kd->button.icon_width = (int)yis_as_int(widthv);
  if (btn->kd->button.icon_width < 0) btn->kd->button.icon_width = 0;
  if (btn->kd->button.icon_width > 2) btn->kd->button.icon_width = 2;
  cogito_window_relayout(cogito_node_window(btn));
}

static YisVal cogito_iconbtn_get_width(YisVal btnv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_get_width expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_get_width expects iconbtn");
  int v = btn->kd->button.icon_width;
  if (v < 0) v = 0;
  if (v > 2) v = 2;
  return YV_INT(v);
}

static void cogito_iconbtn_set_toggle(YisVal btnv, YisVal togglev) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_set_toggle expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_set_toggle expects iconbtn");
  btn->kd->button.icon_toggle = yis_as_bool(togglev);
  if (!btn->kd->button.icon_toggle) btn->checked = false;
  cogito_window_relayout(cogito_node_window(btn));
}

static YisVal cogito_iconbtn_get_toggle(YisVal btnv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_get_toggle expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_get_toggle expects iconbtn");
  return YV_BOOL(btn->kd->button.icon_toggle);
}

static void cogito_iconbtn_set_checked(YisVal btnv, YisVal checkedv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_set_checked expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_set_checked expects iconbtn");
  btn->checked = yis_as_bool(checkedv);
  cogito_window_relayout(cogito_node_window(btn));
}

static YisVal cogito_iconbtn_get_checked(YisVal btnv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_get_checked expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_get_checked expects iconbtn");
  return YV_BOOL(btn->checked);
}
static bool cogito_iconbtn_accepts_size(const CogitoNode* n) {
  return n && (n->kind == COGITO_ICONBTN);
}
static void cogito_iconbtn_set_size(YisVal btnv, YisVal sizev) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_set_size expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (!cogito_iconbtn_accepts_size(btn)) yis_trap("cogito.iconbtn_set_size expects iconbtn");
  btn->kd->button.size = cogito_iconbtn_clamp_size_int((int)yis_as_int(sizev));
  cogito_window_relayout(cogito_node_window(btn));
}

static YisVal cogito_iconbtn_get_size(YisVal btnv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_get_size expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (!cogito_iconbtn_accepts_size(btn)) yis_trap("cogito.iconbtn_get_size expects iconbtn");
  return YV_INT((int)cogito_iconbtn_clamp_size_int((int)btn->kd->button.size));
}

void cogito_iconbtn_set_menu_divider(YisVal btnv, YisVal dividerv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_set_menu_divider expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_set_menu_divider expects iconbtn");
  btn->kd->button.menu_divider = yis_as_bool(dividerv);
  cogito_window_relayout(cogito_node_window(btn));
}

YisVal cogito_iconbtn_get_menu_divider(YisVal btnv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_get_menu_divider expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_get_menu_divider expects iconbtn");
  return YV_BOOL(btn->kd->button.menu_divider);
}

void cogito_iconbtn_set_menu_item_gap(YisVal btnv, YisVal gapv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_set_menu_item_gap expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_set_menu_item_gap expects iconbtn");
  btn->kd->button.menu_item_gap = (int)yis_as_int(gapv);
  if (btn->kd->button.menu_item_gap < 0) btn->kd->button.menu_item_gap = 0;
  cogito_window_relayout(cogito_node_window(btn));
}

YisVal cogito_iconbtn_get_menu_item_gap(YisVal btnv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_get_menu_item_gap expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_get_menu_item_gap expects iconbtn");
  return YV_INT(btn->kd->button.menu_item_gap);
}

void cogito_iconbtn_set_menu_vibrant(YisVal btnv, YisVal vibrantv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_set_menu_vibrant expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_set_menu_vibrant expects iconbtn");
  btn->kd->button.menu_vibrant = yis_as_bool(vibrantv);
  cogito_window_relayout(cogito_node_window(btn));
}

YisVal cogito_iconbtn_get_menu_vibrant(YisVal btnv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_get_menu_vibrant expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.iconbtn_get_menu_vibrant expects iconbtn");
  return YV_BOOL(btn->kd->button.menu_vibrant);
}

void cogito_iconbtn_on_click(YisVal btnv, YisVal handler) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.iconbtn_on_click expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  static int cogito_debug_hit = -1;
  if (cogito_debug_hit < 0) {
    const char* env = getenv("COGITO_DEBUG_HIT");
    cogito_debug_hit = (env && env[0] && env[0] != '0') ? 1 : 0;
  }
  if (cogito_debug_hit) {
    fprintf(stderr, "cogito: set_on_click handler tag=%d (EVT_FN=%d)\n", (int)handler.tag, EVT_FN);
    fflush(stderr);
  }
  if (btn->on_click) yis_release_val(YV_FN(btn->on_click));
  if (handler.tag == EVT_FN) {
    btn->on_click = (YisFn*)handler.as.p;
    yis_retain_val(YV_FN(btn->on_click));
  } else {
    btn->on_click = NULL;
  }
  if (cogito_debug_hit) {
    const char* t = (btn->text && btn->text->data) ? btn->text->data : "";
    fprintf(stderr, "cogito: set_on_click text=\"%s\" handler=%p\n", t, (void*)btn->on_click);
    fflush(stderr);
  }
}