static void cogito_view_switcher_set_active(YisVal vsv, YisVal idv);

static void cogito_tabs_sync_bound_view(CogitoNode* tabs, int idx) {
  if (!tabs || !tabs->view_switcher || !tabs->tab_ids) return;
  if (idx < 0 || (size_t)idx >= tabs->tab_len) return;
  YisStr* id = tabs->tab_ids[idx];
  if (!id) return;
  cogito_view_switcher_set_active(YV_OBJ(tabs->view_switcher), YV_STR(id));
}

static int cogito_tabs_hit_index(CogitoNode* n, int x, int y) {
  if (!n || n->kind != COGITO_TABS) return -1;
  if (!cogito_hit_node(n, x, y)) return -1;
  if (n->w <= 0 || n->tab_len == 0) return -1;
  int slot_count = (int)n->tab_len;
  if (slot_count < 1) slot_count = 1;
  int slot_w = n->w / slot_count;
  if (slot_w < 1) slot_w = 1;
  for (int i = 0; i < slot_count; i++) {
    if (i >= (int)n->tab_len) break;
    int sx = n->x + i * slot_w;
    int sw = (i == slot_count - 1) ? (n->x + n->w - sx) : slot_w;
    if (sw < 1) sw = 1;
    if (cogito_hit_rect_min48(x, y, sx, n->y, sw, n->h)) return i;
  }
  return -1;
}
static CogitoNode* cogito_tabs_new_obj(void) {
  CogitoNode* t = cogito_node_new(COGITO_TABS);
  t->selected = 0;
  t->hexpand = true;
  return t;
}
static YisVal cogito_tabs_new(void) {
  return YV_OBJ(cogito_tabs_new_obj());
}
static void cogito_tabs_set_items(YisVal tabsv, YisVal arrv) {
  if (tabsv.tag != EVT_OBJ) yis_trap("cogito.tabs_set_items expects tabs");
  if (arrv.tag != EVT_ARR) yis_trap("cogito.tabs_set_items expects array");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  if (n->tab_labels) {
    for (size_t i = 0; i < n->tab_len; i++) {
      if (n->tab_labels[i]) yis_release_val(YV_STR(n->tab_labels[i]));
      if (n->tab_icons && n->tab_icons[i]) yis_release_val(YV_STR(n->tab_icons[i]));
    }
    free(n->tab_labels);
    free(n->tab_icons);
    n->tab_labels = NULL;
    n->tab_icons = NULL;
  }
  if (n->tab_ids) {
    for (size_t i = 0; i < n->tab_len; i++) {
      if (n->tab_ids[i]) yis_release_val(YV_STR(n->tab_ids[i]));
    }
    free(n->tab_ids);
    n->tab_ids = NULL;
  }
  n->tab_len = 0;
  n->tab_cap = 0;
  YisArr* a = (YisArr*)arrv.as.p;
  if (!a || a->len == 0) { n->selected = -1; return; }
  if (a->len > 4) yis_trap("cogito.tabs_set_items supports at most 4 items");
  n->tab_labels = (YisStr**)calloc((size_t)a->len, sizeof(YisStr*));
  n->tab_icons = (YisStr**)calloc((size_t)a->len, sizeof(YisStr*));
  n->tab_len = (size_t)a->len;
  n->tab_cap = n->tab_len;
  for (size_t i = 0; i < n->tab_len; i++) {
    YisVal v = yis_arr_get(a, (int64_t)i);
    YisStr* label = NULL;
    YisStr* icon = NULL;
    if (v.tag == EVT_ARR && v.as.p) {
      YisArr* item = (YisArr*)v.as.p;
      if (item->len > 0) {
        YisVal lv = yis_arr_get(item, 0);
        label = stdr_to_string(lv);
      }
      if (item->len > 1) {
        YisVal iv = yis_arr_get(item, 1);
        if (iv.tag != EVT_NULL) icon = stdr_to_string(iv);
      }
    } else {
      label = stdr_to_string(v);
    }
    if (!label) label = stdr_str_lit("");
    n->tab_labels[i] = label;
    if (label) yis_retain_val(YV_STR(label));
    n->tab_icons[i] = icon;
    if (icon) yis_retain_val(YV_STR(icon));
  }
  if (n->selected < 0 && n->tab_len > 0) n->selected = 0;
  if (n->selected >= (int)n->tab_len) n->selected = (int)n->tab_len - 1;
  cogito_tabs_sync_bound_view(n, n->selected);
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_tabs_set_ids(YisVal tabsv, YisVal arrv) {
  if (tabsv.tag != EVT_OBJ) yis_trap("cogito.tabs_set_ids expects tabs");
  if (arrv.tag != EVT_ARR) yis_trap("cogito.tabs_set_ids expects array");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  if (n->tab_ids) {
    for (size_t i = 0; i < n->tab_len; i++) {
      if (n->tab_ids[i]) yis_release_val(YV_STR(n->tab_ids[i]));
    }
    free(n->tab_ids);
    n->tab_ids = NULL;
  }
  YisArr* a = (YisArr*)arrv.as.p;
  size_t len = a ? (size_t)a->len : 0;
  if (len > 4) yis_trap("cogito.tabs_set_ids supports at most 4 items");
  if (n->tab_len == 0) {
    n->tab_len = len;
    n->tab_cap = len;
    if (!n->tab_labels && len > 0) {
      n->tab_labels = (YisStr**)calloc(len, sizeof(YisStr*));
      n->tab_icons = (YisStr**)calloc(len, sizeof(YisStr*));
    }
  }
  if (n->tab_len == 0) return;
  n->tab_ids = (YisStr**)calloc(n->tab_len, sizeof(YisStr*));
  for (size_t i = 0; i < n->tab_len; i++) {
    if (i < len) {
      YisVal v = yis_arr_get(a, (int64_t)i);
      YisStr* s = stdr_to_string(v);
      n->tab_ids[i] = s;
      if (s) yis_retain_val(YV_STR(s));
    }
  }
  cogito_tabs_sync_bound_view(n, n->selected);
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_tabs_set_selected(YisVal tabsv, YisVal idxv) {
  if (tabsv.tag != EVT_OBJ) yis_trap("cogito.tabs_set_selected expects tabs");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  int idx = (int)yis_as_int(idxv);
  if (idx < 0) idx = -1;
  if (n->tab_len > 0 && idx >= (int)n->tab_len) idx = (int)n->tab_len - 1;
  n->selected = idx;
  cogito_tabs_sync_bound_view(n, idx);
  cogito_window_relayout(cogito_node_window(n));
}
static YisVal cogito_tabs_get_selected(YisVal tabsv) {
  if (tabsv.tag != EVT_OBJ) yis_trap("cogito.tabs_get_selected expects tabs");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  return YV_INT(n->selected);
}
static void cogito_tabs_on_change(YisVal tabsv, YisVal handler) {
  if (tabsv.tag != EVT_OBJ) yis_trap("cogito.tabs_on_change expects tabs");
  if (handler.tag != EVT_FN) yis_trap("cogito.tabs_on_change expects function");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  if (n->on_select) yis_release_val(YV_FN(n->on_select));
  n->on_select = (YisFn*)handler.as.p;
  yis_retain_val(YV_FN(n->on_select));
}
static void cogito_tabs_bind(YisVal tabsv, YisVal vsv) {
  if (tabsv.tag != EVT_OBJ) yis_trap("cogito.tabs_bind expects tabs");
  if (vsv.tag != EVT_OBJ) yis_trap("cogito.tabs_bind expects view_switcher");
  CogitoNode* t = (CogitoNode*)tabsv.as.p;
  CogitoNode* vs = (CogitoNode*)vsv.as.p;
  if (vs->kind != COGITO_VIEWSWITCHER) yis_trap("cogito.tabs_bind expects view_switcher");
  t->view_switcher = vs;
  cogito_tabs_sync_bound_view(t, t->selected);
}
