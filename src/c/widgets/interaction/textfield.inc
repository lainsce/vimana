static CogitoNode* cogito_textfield_new_obj(YisStr* text) {
  CogitoNode* tf = cogito_node_new(COGITO_TEXTFIELD);
  // Treat constructor text as placeholder; value starts empty.
  cogito_node_set_subtitle(tf, text);
  cogito_node_set_text(tf, NULL);
  return tf;
}
static YisVal cogito_textfield_new(YisVal text) {
  YisStr* ts = stdr_to_string(text);
  CogitoNode* tf = cogito_textfield_new_obj(ts);
  return YV_OBJ(tf);
}
static void cogito_textfield_set_text(YisVal nodev, YisVal textv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textfield_set_text expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_textfield_set_regex(YisVal nodev, YisVal regexv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textfield_set_regex expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;

  YisStr* rs = NULL;
  bool temp = false;
  if (regexv.tag == EVT_STR) {
    rs = (YisStr*)regexv.as.p;
  } else if (regexv.tag != EVT_NULL) {
    rs = stdr_to_string(regexv);
    temp = true;
  }

  if (rs && (!rs->data || !rs->data[0])) {
    if (temp) yis_release_val(YV_STR(rs));
    rs = NULL;
    temp = false;
  }
  if (rs) yis_retain_val(YV_STR(rs));
  if (n->textfield_regex) yis_release_val(YV_STR(n->textfield_regex));
  n->textfield_regex = rs;
  if (temp && rs) yis_release_val(YV_STR(rs));
  cogito_textfield_refresh_regex_state(n);
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_textfield_get_regex(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textfield_get_regex expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* s = n->textfield_regex ? n->textfield_regex : stdr_str_lit("");
  yis_retain_val(YV_STR(s));
  return YV_STR(s);
}

static YisVal cogito_textfield_is_valid(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textfield_is_valid expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  cogito_textfield_refresh_regex_state(n);
  return YV_BOOL(!n->textfield_regex_invalid);
}

static YisVal cogito_textfield_get_text(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textfield_get_text expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* s = n->text ? n->text : stdr_str_lit("");
  yis_retain_val(YV_STR(s));
  return YV_STR(s);
}
static void cogito_textfield_set_hint(YisVal nodev, YisVal hintv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textfield_set_hint expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* hs = stdr_to_string(hintv);
  cogito_node_set_hint(n, hs);
  cogito_window_relayout(cogito_node_window(n));
}
static YisVal cogito_textfield_get_hint(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textfield_get_hint expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* s = n->hint ? n->hint : stdr_str_lit("");
  yis_retain_val(YV_STR(s));
  return YV_STR(s);
}
static void cogito_textfield_set_prefix(YisVal nodev, YisVal prefixv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textfield_set_prefix expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* ps = NULL;
  if (prefixv.tag == EVT_STR) {
    ps = (YisStr*)prefixv.as.p;
  } else if (prefixv.tag != EVT_NULL) {
    ps = stdr_to_string(prefixv);
  }
  if (ps && (!ps->data || !ps->data[0])) ps = NULL;
  if (ps) yis_retain_val(YV_STR(ps));
  if (n->textfield_prefix) yis_release_val(YV_STR(n->textfield_prefix));
  n->textfield_prefix = ps;
  cogito_window_relayout(cogito_node_window(n));
}
static YisVal cogito_textfield_get_prefix(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textfield_get_prefix expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* s = n->textfield_prefix ? n->textfield_prefix : stdr_str_lit("");
  yis_retain_val(YV_STR(s));
  return YV_STR(s);
}
static void cogito_textfield_set_suffix(YisVal nodev, YisVal suffixv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textfield_set_suffix expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* ss = NULL;
  if (suffixv.tag == EVT_STR) {
    ss = (YisStr*)suffixv.as.p;
  } else if (suffixv.tag != EVT_NULL) {
    ss = stdr_to_string(suffixv);
  }
  if (ss && (!ss->data || !ss->data[0])) ss = NULL;
  if (ss) yis_retain_val(YV_STR(ss));
  if (n->textfield_suffix) yis_release_val(YV_STR(n->textfield_suffix));
  n->textfield_suffix = ss;
  cogito_window_relayout(cogito_node_window(n));
}
static YisVal cogito_textfield_get_suffix(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textfield_get_suffix expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* s = n->textfield_suffix ? n->textfield_suffix : stdr_str_lit("");
  yis_retain_val(YV_STR(s));
  return YV_STR(s);
}
static void cogito_textfield_on_change(YisVal nodev, YisVal handler) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.textfield_on_change expects textfield");
  if (handler.tag != EVT_FN) yis_trap("cogito.textfield_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->on_change) yis_release_val(YV_FN(n->on_change));
  n->on_change = (YisFn*)handler.as.p;
  yis_retain_val(YV_FN(n->on_change));
}
static CogitoNode* cogito_find_text_input(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (cogito_node_is_hidden(n)) return NULL;
  if (n->kind == COGITO_WINDOW && n->popover_open && n->popover) {
    CogitoNode* pop_hit = cogito_find_text_input(n->popover, x, y);
    if (pop_hit) return pop_hit;
  }
  if (n->kind == COGITO_WINDOW && n->side_sheet_open && n->side_sheet_node) {
    CogitoNode* ss = n->side_sheet_node;
    CogitoNode* ss_hit = cogito_find_text_input(ss, x, y);
    if (ss_hit) return ss_hit;
    if (ss->side_sheet.mode != COGITO_SIDE_SHEET_SIDEBAR &&
        cogito_hit_rect(x, y, 0, 0, n->w, n->h)) {
      return NULL;
    }
  }
  if (n->kind == COGITO_DIALOG_SLOT && n->dialog && n->dialog_open) {
    return cogito_find_text_input(n->dialog, x, y);
  }
  if (n->kind == COGITO_DIALOG && !cogito_hit_node(n, x, y)) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* hit = cogito_find_text_input(active, x, y);
      if (hit) return hit;
    }
  } else {
    for (size_t i = n->len; i > 0; i--) {
      CogitoNode* hit = cogito_find_text_input(n->children[i - 1], x, y);
      if (hit) return hit;
    }
  }
  if ((n->kind == COGITO_TEXTFIELD || n->kind == COGITO_TEXTVIEW || n->kind == COGITO_SEARCHFIELD) &&
      n->editable &&
      cogito_hit_node_min48(n, x, y)) return n;
  if (n->kind == COGITO_COLORPICKER) {
    int hx = 0, hy = 0, hw = 0, hh = 0;
    cogito_colorpicker_hex_rect(n, &hx, &hy, &hw, &hh);
    if (cogito_hit_rect_min48(x, y, hx, hy, hw, hh)) return n;
  }
  return NULL;
}
