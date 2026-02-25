static CogitoNode* cogito_iconbtn_new_obj(ErgoStr* label) {
  CogitoNode* btn = cogito_node_new(COGITO_ICONBTN);
  btn->button.icon_shape = 0; // Default to circle (matches spec)
  btn->button.size = COGITO_BUTTON_SIZE_M; // Default to M size
  btn->button.icon_width = 1; // Default to square (default width)
  cogito_node_set_text(btn, label);
  return btn;
}
static ErgoVal cogito_iconbtn_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* btn = cogito_iconbtn_new_obj(ts);
  return EV_OBJ(btn);
}
static void cogito_iconbtn_add_menu(ErgoVal btnv, ErgoVal label, ErgoVal handler) {
  cogito_button_add_menu(btnv, label, handler);
}

static void cogito_iconbtn_add_menu_section(ErgoVal btnv, ErgoVal label, ErgoVal handler) {
  cogito_button_add_menu_section(btnv, label, handler);
}

// Icon button setters/getters
static void cogito_iconbtn_set_shape(ErgoVal btnv, ErgoVal shapev) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_set_shape expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_set_shape expects iconbtn");
  btn->button.icon_shape = (int)ergo_as_int(shapev);
  if (btn->button.icon_shape < 0) btn->button.icon_shape = 0;
  if (btn->button.icon_shape > 1) btn->button.icon_shape = 1;
  cogito_window_relayout(cogito_node_window(btn));
}

static ErgoVal cogito_iconbtn_get_shape(ErgoVal btnv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_get_shape expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_get_shape expects iconbtn");
  int v = btn->button.icon_shape;
  if (v < 0) v = 0;
  if (v > 1) v = 1;
  return EV_INT(v);
}

static void cogito_iconbtn_set_color_style(ErgoVal btnv, ErgoVal stylev) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_set_color_style expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_set_color_style expects iconbtn");
  btn->button.icon_color_style = (int)ergo_as_int(stylev);
  if (btn->button.icon_color_style < 0) btn->button.icon_color_style = 0;
  if (btn->button.icon_color_style > 3) btn->button.icon_color_style = 3;
  cogito_window_relayout(cogito_node_window(btn));
}

static ErgoVal cogito_iconbtn_get_color_style(ErgoVal btnv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_get_color_style expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_get_color_style expects iconbtn");
  int v = btn->button.icon_color_style;
  if (v < 0) v = 0;
  if (v > 3) v = 3;
  return EV_INT(v);
}

static void cogito_iconbtn_set_width(ErgoVal btnv, ErgoVal widthv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_set_width expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_set_width expects iconbtn");
  btn->button.icon_width = (int)ergo_as_int(widthv);
  if (btn->button.icon_width < 0) btn->button.icon_width = 0;
  if (btn->button.icon_width > 2) btn->button.icon_width = 2;
  cogito_window_relayout(cogito_node_window(btn));
}

static ErgoVal cogito_iconbtn_get_width(ErgoVal btnv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_get_width expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_get_width expects iconbtn");
  int v = btn->button.icon_width;
  if (v < 0) v = 0;
  if (v > 2) v = 2;
  return EV_INT(v);
}

static void cogito_iconbtn_set_toggle(ErgoVal btnv, ErgoVal togglev) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_set_toggle expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_set_toggle expects iconbtn");
  btn->button.icon_toggle = ergo_as_bool(togglev);
  if (!btn->button.icon_toggle) btn->checked = false;
  cogito_window_relayout(cogito_node_window(btn));
}

static ErgoVal cogito_iconbtn_get_toggle(ErgoVal btnv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_get_toggle expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_get_toggle expects iconbtn");
  return EV_BOOL(btn->button.icon_toggle);
}

static void cogito_iconbtn_set_checked(ErgoVal btnv, ErgoVal checkedv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_set_checked expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_set_checked expects iconbtn");
  btn->checked = ergo_as_bool(checkedv);
  cogito_window_relayout(cogito_node_window(btn));
}

static ErgoVal cogito_iconbtn_get_checked(ErgoVal btnv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_get_checked expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_get_checked expects iconbtn");
  return EV_BOOL(btn->checked);
}
static bool cogito_iconbtn_accepts_size(const CogitoNode* n) {
  return n && (n->kind == COGITO_ICONBTN);
}
static void cogito_iconbtn_set_size(ErgoVal btnv, ErgoVal sizev) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_set_size expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (!cogito_iconbtn_accepts_size(btn)) ergo_trap("cogito.iconbtn_set_size expects iconbtn");
  btn->button.size = cogito_iconbtn_clamp_size_int((int)ergo_as_int(sizev));
  cogito_window_relayout(cogito_node_window(btn));
}

static ErgoVal cogito_iconbtn_get_size(ErgoVal btnv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_get_size expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (!cogito_iconbtn_accepts_size(btn)) ergo_trap("cogito.iconbtn_get_size expects iconbtn");
  return EV_INT((int)cogito_iconbtn_clamp_size_int((int)btn->button.size));
}

void cogito_iconbtn_set_menu_divider(ErgoVal btnv, ErgoVal dividerv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_set_menu_divider expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_set_menu_divider expects iconbtn");
  btn->button.menu_divider = ergo_as_bool(dividerv);
  cogito_window_relayout(cogito_node_window(btn));
}

ErgoVal cogito_iconbtn_get_menu_divider(ErgoVal btnv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_get_menu_divider expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_get_menu_divider expects iconbtn");
  return EV_BOOL(btn->button.menu_divider);
}

void cogito_iconbtn_set_menu_item_gap(ErgoVal btnv, ErgoVal gapv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_set_menu_item_gap expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_set_menu_item_gap expects iconbtn");
  btn->button.menu_item_gap = (int)ergo_as_int(gapv);
  if (btn->button.menu_item_gap < 0) btn->button.menu_item_gap = 0;
  cogito_window_relayout(cogito_node_window(btn));
}

ErgoVal cogito_iconbtn_get_menu_item_gap(ErgoVal btnv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_get_menu_item_gap expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_get_menu_item_gap expects iconbtn");
  return EV_INT(btn->button.menu_item_gap);
}

void cogito_iconbtn_set_menu_vibrant(ErgoVal btnv, ErgoVal vibrantv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_set_menu_vibrant expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_set_menu_vibrant expects iconbtn");
  btn->button.menu_vibrant = ergo_as_bool(vibrantv);
  cogito_window_relayout(cogito_node_window(btn));
}

ErgoVal cogito_iconbtn_get_menu_vibrant(ErgoVal btnv) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_get_menu_vibrant expects iconbtn");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) ergo_trap("cogito.iconbtn_get_menu_vibrant expects iconbtn");
  return EV_BOOL(btn->button.menu_vibrant);
}

void cogito_iconbtn_on_click(ErgoVal btnv, ErgoVal handler) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_on_click expects iconbtn");
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
  if (btn->on_click) ergo_release_val(EV_FN(btn->on_click));
  if (handler.tag == EVT_FN) {
    btn->on_click = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(btn->on_click));
  } else {
    btn->on_click = NULL;
  }
  if (cogito_debug_hit) {
    const char* t = (btn->text && btn->text->data) ? btn->text->data : "";
    fprintf(stderr, "cogito: set_on_click text=\"%s\" handler=%p\n", t, (void*)btn->on_click);
    fflush(stderr);
  }
}