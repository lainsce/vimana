static YisStr* cogito_about_window_strlit(const char* s) {
  if (!s) return NULL;
  return stdr_str_from_slice(s, (int)strlen(s));
}

static void cogito_about_window_apply_class(CogitoNode* n, const char* class_name) {
  if (!n || !class_name || !class_name[0]) return;
  YisStr* cls = cogito_about_window_strlit(class_name);
  if (!cls) return;
  cogito_node_set_class(YV_OBJ(n), YV_STR(cls));
  yis_release_val(YV_STR(cls));
}

static bool cogito_about_window_has_class(const CogitoNode* n, const char* class_name) {
  return n && class_name && n->class_name && n->class_name->data &&
         strcmp(n->class_name->data, class_name) == 0;
}

static CogitoNode* cogito_about_window_find_child(CogitoNode* aw, CogitoKind kind, const char* class_name) {
  if (!aw) return NULL;
  for (size_t i = 0; i < aw->len; i++) {
    CogitoNode* c = aw->children[i];
    if (c->kind != kind) continue;
    if (!class_name || cogito_about_window_has_class(c, class_name)) return c;
  }
  return NULL;
}

static CogitoNode* cogito_about_window_icon_node(CogitoNode* aw) {
  return cogito_about_window_find_child(aw, COGITO_IMAGE, "about-window-icon");
}

static CogitoNode* cogito_about_window_title_node(CogitoNode* aw) {
  return cogito_about_window_find_child(aw, COGITO_LABEL, "about-window-title");
}

static CogitoNode* cogito_about_window_meta_node(CogitoNode* aw) {
  return cogito_about_window_find_child(aw, COGITO_LABEL, "about-window-license");
}

static CogitoNode* cogito_about_window_actions_node(CogitoNode* aw) {
  return cogito_about_window_find_child(aw, COGITO_HSTACK, "about-window-actions");
}

static CogitoNode* cogito_about_window_action_button(CogitoNode* aw, size_t index) {
  CogitoNode* actions = cogito_about_window_actions_node(aw);
  if (!actions) return NULL;
  size_t seen = 0;
  for (size_t i = 0; i < actions->len; i++) {
    CogitoNode* c = actions->children[i];
    if (c->kind != COGITO_BUTTON) continue;
    if (seen == index) return c;
    seen++;
  }
  return NULL;
}

static void cogito_about_window_set_button_url(CogitoNode* btn, YisStr* url) {
  if (!btn || btn->kind != COGITO_BUTTON) return;
  if (btn->action_text == url) return;
  if (btn->action_text) {
    yis_release_val(YV_STR(btn->action_text));
    btn->action_text = NULL;
  }
  if (url) {
    btn->action_text = url;
    yis_retain_val(YV_STR(url));
  }
}

static CogitoNode* cogito_about_window_ensure_issue_action(CogitoNode* aw) {
  CogitoNode* issue = cogito_about_window_action_button(aw, 1);
  if (issue) return issue;
  CogitoNode* actions = cogito_about_window_actions_node(aw);
  if (!actions) return NULL;

  issue = cogito_node_new(COGITO_BUTTON);
  YisStr* txt = cogito_about_window_strlit("Report a Bug");
  cogito_node_set_text(issue, txt);
  if (txt) yis_release_val(YV_STR(txt));
  cogito_about_window_apply_class(issue, "outlined");
  issue->disabled = true;
  cogito_children_add(actions, issue);
  return issue;
}

static void cogito_about_window_sync_title(CogitoNode* aw) {
  CogitoNode* title = cogito_about_window_title_node(aw);
  if (!title) return;
  if (title->text != aw->text) {
    cogito_node_set_text(title, aw->text);
  }
}

static void cogito_about_window_sync_icon(CogitoNode* aw) {
  CogitoNode* icon = cogito_about_window_icon_node(aw);
  if (!icon) return;
  YisStr* source = aw->icon;
  if (icon->icon != source) {
    cogito_node_set_icon(icon, source);
  }
}

static void cogito_about_window_sync_meta(CogitoNode* aw) {
  CogitoNode* meta = cogito_about_window_meta_node(aw);
  if (!meta) return;
  YisStr* text = NULL;
  if (aw->tooltip && aw->tooltip->data && aw->tooltip->data[0]) {
    text = aw->tooltip;
  } else if (aw->subtitle && aw->subtitle->data && aw->subtitle->data[0]) {
    text = aw->subtitle;
  }
  if (meta->text != text) {
    cogito_node_set_text(meta, text);
  }
}

static void cogito_about_window_sync_actions(CogitoNode* aw) {
  CogitoNode* primary = cogito_about_window_action_button(aw, 0);
  YisStr* website = (aw->action_text && aw->action_text->data && aw->action_text->data[0])
                       ? aw->action_text
                       : NULL;
  if (primary) {
    cogito_about_window_set_button_url(primary, website);
    primary->disabled = (website == NULL);
  }

  CogitoNode* issue = cogito_about_window_action_button(aw, 1);
  if (issue) {
    bool has_issue = issue->action_text && issue->action_text->data && issue->action_text->data[0];
    issue->disabled = !has_issue;
  }
}

static CogitoNode* cogito_about_window_new_obj(YisStr* app_name, YisStr* version) {
  CogitoNode* n = cogito_node_new(COGITO_ABOUT_WINDOW);
  cogito_node_set_text(n, app_name);
  cogito_node_set_subtitle(n, version);
  n->gap = 12;
  cogito_about_window_apply_class(n, "about-window");

  CogitoNode* icon = cogito_node_new(COGITO_IMAGE);
  icon->align = 1;
  cogito_about_window_apply_class(icon, "about-window-icon");
  cogito_children_add(n, icon);

  CogitoNode* title = cogito_node_new(COGITO_LABEL);
  title->align = 1;
  title->text_align = 1;
  title->margin_top = 12;
  title->margin_right = 0;
  title->margin_bottom = 0;
  title->margin_left = 0;
  title->margin_set = true;
  cogito_about_window_apply_class(title, "about-window-title");
  cogito_children_add(n, title);

  CogitoNode* meta = cogito_node_new(COGITO_LABEL);
  meta->align = 1;
  meta->text_align = 1;
  meta->text_wrap = true;
  meta->hexpand = true;
  cogito_about_window_apply_class(meta, "about-window-license");
  cogito_children_add(n, meta);

  CogitoNode* actions = cogito_node_new(COGITO_HSTACK);
  actions->align = 1;
  actions->gap = 12;
  actions->margin_top = 12;
  actions->margin_right = 0;
  actions->margin_bottom = 0;
  actions->margin_left = 0;
  actions->margin_set = true;
  actions->padding_left = 0;
  actions->padding_top = 0;
  actions->padding_right = 0;
  actions->padding_bottom = 0;
  actions->padding_set = true;
  cogito_about_window_apply_class(actions, "about-window-actions");

  CogitoNode* more = cogito_node_new(COGITO_BUTTON);
  YisStr* more_text = cogito_about_window_strlit("More info");
  cogito_node_set_text(more, more_text);
  if (more_text) yis_release_val(YV_STR(more_text));
  cogito_about_window_apply_class(more, "outlined");
  cogito_children_add(actions, more);
  cogito_children_add(n, actions);

  cogito_about_window_sync_title(n);
  cogito_about_window_sync_icon(n);
  cogito_about_window_sync_meta(n);
  cogito_about_window_sync_actions(n);
  return n;
}

static YisVal cogito_about_window_new(YisVal namev, YisVal versionv) {
  YisStr* ns = stdr_to_string(namev);
  YisStr* vs = stdr_to_string(versionv);
  CogitoNode* n = cogito_about_window_new_obj(ns, vs);
  return YV_OBJ(n);
}

static void cogito_about_window_set_icon(YisVal awv, YisVal iconv) {
  if (awv.tag != EVT_OBJ) yis_trap("cogito.about_window_set_icon expects about_window");
  CogitoNode* n = (CogitoNode*)awv.as.p;
  YisStr* s = stdr_to_string(iconv);
  cogito_node_set_icon(n, s);
  cogito_about_window_sync_icon(n);
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_about_window_set_description(YisVal awv, YisVal descv) {
  if (awv.tag != EVT_OBJ) yis_trap("cogito.about_window_set_description expects about_window");
  CogitoNode* n = (CogitoNode*)awv.as.p;
  YisStr* s = stdr_to_string(descv);
  cogito_node_set_tooltip(n, s);
  cogito_about_window_sync_meta(n);
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_about_window_set_website(YisVal awv, YisVal urlv) {
  if (awv.tag != EVT_OBJ) yis_trap("cogito.about_window_set_website expects about_window");
  CogitoNode* n = (CogitoNode*)awv.as.p;
  YisStr* s = stdr_to_string(urlv);
  if (n->action_text) yis_release_val(YV_STR(n->action_text));
  n->action_text = s;
  yis_retain_val(YV_STR(s));
  cogito_about_window_sync_actions(n);
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_about_window_set_issue_url(YisVal awv, YisVal urlv) {
  if (awv.tag != EVT_OBJ) yis_trap("cogito.about_window_set_issue_url expects about_window");
  CogitoNode* n = (CogitoNode*)awv.as.p;
  YisStr* s = stdr_to_string(urlv);
  CogitoNode* issue = cogito_about_window_ensure_issue_action(n);
  if (issue) {
    YisStr* url = (s && s->data && s->data[0]) ? s : NULL;
    cogito_about_window_set_button_url(issue, url);
  }
  cogito_about_window_sync_actions(n);
  cogito_window_relayout(cogito_node_window(n));
}
