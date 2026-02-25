static int cogito_tabs_hit_index(CogitoNode* n, int x, int y) {
  if (!n || n->kind != COGITO_TABS) return -1;
  if (!cogito_hit_node(n, x, y)) return -1;
  if (n->w <= 0 || n->tab_len == 0) return -1;
  const int slot_count = 4;
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
static ErgoVal cogito_tabs_new(void) {
  return EV_OBJ(cogito_tabs_new_obj());
}
static void cogito_tabs_set_items(ErgoVal tabsv, ErgoVal arrv) {
  if (tabsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_set_items expects tabs");
  if (arrv.tag != EVT_ARR) ergo_trap("cogito.tabs_set_items expects array");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  if (n->tab_labels) {
    for (size_t i = 0; i < n->tab_len; i++) {
      if (n->tab_labels[i]) ergo_release_val(EV_STR(n->tab_labels[i]));
      if (n->tab_icons && n->tab_icons[i]) ergo_release_val(EV_STR(n->tab_icons[i]));
    }
    free(n->tab_labels);
    free(n->tab_icons);
    n->tab_labels = NULL;
    n->tab_icons = NULL;
  }
  if (n->tab_ids) {
    for (size_t i = 0; i < n->tab_len; i++) {
      if (n->tab_ids[i]) ergo_release_val(EV_STR(n->tab_ids[i]));
    }
    free(n->tab_ids);
    n->tab_ids = NULL;
  }
  n->tab_len = 0;
  n->tab_cap = 0;
  ErgoArr* a = (ErgoArr*)arrv.as.p;
  if (!a || a->len == 0) { n->selected = -1; return; }
  if (a->len > 4) ergo_trap("cogito.tabs_set_items supports at most 4 items");
  n->tab_labels = (ErgoStr**)calloc((size_t)a->len, sizeof(ErgoStr*));
  n->tab_icons = (ErgoStr**)calloc((size_t)a->len, sizeof(ErgoStr*));
  n->tab_len = (size_t)a->len;
  n->tab_cap = n->tab_len;
  for (size_t i = 0; i < n->tab_len; i++) {
    ErgoVal v = ergo_arr_get(a, (int64_t)i);
    ErgoStr* label = NULL;
    ErgoStr* icon = NULL;
    if (v.tag == EVT_ARR && v.as.p) {
      ErgoArr* item = (ErgoArr*)v.as.p;
      if (item->len > 0) {
        ErgoVal lv = ergo_arr_get(item, 0);
        label = stdr_to_string(lv);
      }
      if (item->len > 1) {
        ErgoVal iv = ergo_arr_get(item, 1);
        if (iv.tag != EVT_NULL) icon = stdr_to_string(iv);
      }
    } else {
      label = stdr_to_string(v);
    }
    if (!label) label = stdr_str_lit("");
    n->tab_labels[i] = label;
    if (label) ergo_retain_val(EV_STR(label));
    n->tab_icons[i] = icon;
    if (icon) ergo_retain_val(EV_STR(icon));
  }
  if (n->selected < 0 && n->tab_len > 0) n->selected = 0;
  if (n->selected >= (int)n->tab_len) n->selected = (int)n->tab_len - 1;
  if (n->view_switcher && n->tab_ids && n->selected >= 0 && (size_t)n->selected < n->tab_len) {
    CogitoNode* vs = n->view_switcher;
    ErgoStr* id = n->tab_ids[n->selected];
    if (id) {
      if (vs->view_active_id) ergo_release_val(EV_STR(vs->view_active_id));
      vs->view_active_id = id;
      ergo_retain_val(EV_STR(vs->view_active_id));
    }
  }
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_tabs_set_ids(ErgoVal tabsv, ErgoVal arrv) {
  if (tabsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_set_ids expects tabs");
  if (arrv.tag != EVT_ARR) ergo_trap("cogito.tabs_set_ids expects array");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  if (n->tab_ids) {
    for (size_t i = 0; i < n->tab_len; i++) {
      if (n->tab_ids[i]) ergo_release_val(EV_STR(n->tab_ids[i]));
    }
    free(n->tab_ids);
    n->tab_ids = NULL;
  }
  ErgoArr* a = (ErgoArr*)arrv.as.p;
  size_t len = a ? (size_t)a->len : 0;
  if (len > 4) ergo_trap("cogito.tabs_set_ids supports at most 4 items");
  if (n->tab_len == 0) {
    n->tab_len = len;
    n->tab_cap = len;
    if (!n->tab_labels && len > 0) {
      n->tab_labels = (ErgoStr**)calloc(len, sizeof(ErgoStr*));
      n->tab_icons = (ErgoStr**)calloc(len, sizeof(ErgoStr*));
    }
  }
  if (n->tab_len == 0) return;
  n->tab_ids = (ErgoStr**)calloc(n->tab_len, sizeof(ErgoStr*));
  for (size_t i = 0; i < n->tab_len; i++) {
    if (i < len) {
      ErgoVal v = ergo_arr_get(a, (int64_t)i);
      ErgoStr* s = stdr_to_string(v);
      n->tab_ids[i] = s;
      if (s) ergo_retain_val(EV_STR(s));
    }
  }
  if (n->view_switcher && n->selected >= 0 && (size_t)n->selected < n->tab_len) {
    CogitoNode* vs = n->view_switcher;
    ErgoStr* id = n->tab_ids[n->selected];
    if (id) {
      if (vs->view_active_id) ergo_release_val(EV_STR(vs->view_active_id));
      vs->view_active_id = id;
      ergo_retain_val(EV_STR(vs->view_active_id));
    }
  }
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_tabs_set_selected(ErgoVal tabsv, ErgoVal idxv) {
  if (tabsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_set_selected expects tabs");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  int idx = (int)ergo_as_int(idxv);
  if (idx < 0) idx = -1;
  if (n->tab_len > 0 && idx >= (int)n->tab_len) idx = (int)n->tab_len - 1;
  n->selected = idx;
  if (n->view_switcher && n->tab_ids && idx >= 0 && (size_t)idx < n->tab_len) {
    CogitoNode* vs = n->view_switcher;
    ErgoStr* id = n->tab_ids[idx];
    if (id) {
      if (vs->view_active_id) ergo_release_val(EV_STR(vs->view_active_id));
      vs->view_active_id = id;
      ergo_retain_val(EV_STR(vs->view_active_id));
    }
  }
  cogito_window_relayout(cogito_node_window(n));
}
static ErgoVal cogito_tabs_get_selected(ErgoVal tabsv) {
  if (tabsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_get_selected expects tabs");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  return EV_INT(n->selected);
}
static void cogito_tabs_on_change(ErgoVal tabsv, ErgoVal handler) {
  if (tabsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_on_change expects tabs");
  if (handler.tag != EVT_FN) ergo_trap("cogito.tabs_on_change expects function");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
static void cogito_tabs_bind(ErgoVal tabsv, ErgoVal vsv) {
  if (tabsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_bind expects tabs");
  if (vsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_bind expects view_switcher");
  CogitoNode* t = (CogitoNode*)tabsv.as.p;
  CogitoNode* vs = (CogitoNode*)vsv.as.p;
  if (vs->kind != COGITO_VIEWSWITCHER) ergo_trap("cogito.tabs_bind expects view_switcher");
  t->view_switcher = vs;
  if (t->tab_ids && t->selected >= 0 && (size_t)t->selected < t->tab_len) {
    ErgoStr* id = t->tab_ids[t->selected];
    if (id) {
      if (vs->view_active_id) ergo_release_val(EV_STR(vs->view_active_id));
      vs->view_active_id = id;
      ergo_retain_val(EV_STR(vs->view_active_id));
    }
  }
}
