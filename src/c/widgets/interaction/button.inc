static CogitoNode* cogito_button_new_obj(YisStr* label) {
  CogitoNode* btn = cogito_node_new(COGITO_BUTTON);
  btn->button.size = COGITO_BUTTON_SIZE_M; // Default to M size
  cogito_node_set_text(btn, label);
  return btn;
}
static YisVal cogito_button_new(YisVal text) {
  if (cogito_debug_enabled()) {
    fprintf(stderr, "cogito: button_new\n");
    fflush(stderr);
  }
  YisStr* ts = stdr_to_string(text);
  CogitoNode* btn = cogito_button_new_obj(ts);
  return YV_OBJ(btn);
}
static bool cogito_button_accepts_size(const CogitoNode* n) {
  return n && (n->kind == COGITO_BUTTON);
}
static void cogito_button_set_text(YisVal btnv, YisVal text) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.button_set_text expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  YisStr* ts = stdr_to_string(text);
  static int debug = -1;
  if (debug < 0) {
    const char* env = getenv("COGITO_DEBUG_HIT");
    debug = (env && env[0] && env[0] != '0') ? 1 : 0;
  }
  if (debug) {
    const char* old_text = (btn->text && btn->text->data) ? btn->text->data : "";
    const char* new_text = (ts && ts->data) ? ts->data : "";
    fprintf(stderr, "cogito: button_set_text old=\"%s\" new=\"%s\"\n", old_text, new_text);
    fflush(stderr);
  }
  cogito_node_set_text(btn, ts);
  if (debug) {
    fprintf(stderr, "cogito: button_set_text calling relayout\n");
    fflush(stderr);
  }
  cogito_window_relayout(cogito_node_window(btn));
  if (debug) {
    fprintf(stderr, "cogito: button_set_text relayout done\n");
    fflush(stderr);
  }
}
static void cogito_button_set_size(YisVal btnv, YisVal sizev) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.button_set_size expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (!cogito_button_accepts_size(btn)) yis_trap("cogito.button_set_size expects button");
  btn->button.size = cogito_button_clamp_size_int((int)yis_as_int(sizev));
  cogito_window_relayout(cogito_node_window(btn));
}

static YisVal cogito_button_get_size(YisVal btnv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.button_get_size expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (!cogito_button_accepts_size(btn)) yis_trap("cogito.button_get_size expects button");
  return YV_INT((int)cogito_button_clamp_size_int((int)btn->button.size));
}
void cogito_button_on_click(YisVal btnv, YisVal handler) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.button_on_click expects button");
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
static void cogito_button_add_menu(YisVal btnv, YisVal label, YisVal handler) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.button_add_menu expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  YisStr* ts = stdr_to_string(label);
  YisFn* fn = handler.tag == EVT_FN ? (YisFn*)handler.as.p : NULL;
  cogito_node_add_menu(btn, ts, fn);
}

static void cogito_button_add_menu_section(YisVal btnv, YisVal label, YisVal handler) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.button_add_menu_section expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  YisStr* ts = stdr_to_string(label);
  YisFn* fn = handler.tag == EVT_FN ? (YisFn*)handler.as.p : NULL;
  cogito_node_add_menu_section(btn, ts, fn);
}

void cogito_button_set_menu_divider(YisVal btnv, YisVal dividerv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.button_set_menu_divider expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.button_set_menu_divider expects button");
  btn->button.menu_divider = yis_as_bool(dividerv);
  cogito_window_relayout(cogito_node_window(btn));
}

YisVal cogito_button_get_menu_divider(YisVal btnv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.button_get_menu_divider expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.button_get_menu_divider expects button");
  return YV_BOOL(btn->button.menu_divider);
}

void cogito_button_set_menu_item_gap(YisVal btnv, YisVal gapv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.button_set_menu_item_gap expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.button_set_menu_item_gap expects button");
  btn->button.menu_item_gap = (int)yis_as_int(gapv);
  if (btn->button.menu_item_gap < 0) btn->button.menu_item_gap = 0;
  cogito_window_relayout(cogito_node_window(btn));
}

YisVal cogito_button_get_menu_item_gap(YisVal btnv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.button_get_menu_item_gap expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.button_get_menu_item_gap expects button");
  return YV_INT(btn->button.menu_item_gap);
}

void cogito_button_set_menu_vibrant(YisVal btnv, YisVal vibrantv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.button_set_menu_vibrant expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.button_set_menu_vibrant expects button");
  btn->button.menu_vibrant = yis_as_bool(vibrantv);
  cogito_window_relayout(cogito_node_window(btn));
}

YisVal cogito_button_get_menu_vibrant(YisVal btnv) {
  if (btnv.tag != EVT_OBJ) yis_trap("cogito.button_get_menu_vibrant expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->kind != COGITO_ICONBTN) yis_trap("cogito.button_get_menu_vibrant expects button");
  return YV_BOOL(btn->button.menu_vibrant);
}