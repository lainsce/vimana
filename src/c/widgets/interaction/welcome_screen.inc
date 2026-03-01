static void cogito_welcome_screen_has_seen_sanitize(const char* src, char* out, size_t out_cap) {
  if (!out || out_cap == 0) return;
  out[0] = 0;
  if (!src || !src[0]) return;
  size_t j = 0;
  for (size_t i = 0; src[i] && j + 1 < out_cap; i++) {
    unsigned char c = (unsigned char)src[i];
    bool ok = ((c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') ||
               c == '_' || c == '-' || c == '.');
    out[j++] = ok ? (char)c : '_';
  }
  out[j] = 0;
}

static bool cogito_welcome_screen_has_seen_path(const char* key, char* out, size_t out_cap) {
  if (!key || !key[0] || !out || out_cap == 0) return false;

  const char* app_id = "default";
  if (cogito_active_app && cogito_active_app->app_id &&
      cogito_active_app->app_id->data && cogito_active_app->app_id->data[0]) {
    app_id = cogito_active_app->app_id->data;
  }

  char app_id_safe[128];
  char key_safe[128];
  cogito_welcome_screen_has_seen_sanitize(app_id, app_id_safe, sizeof(app_id_safe));
  cogito_welcome_screen_has_seen_sanitize(key, key_safe, sizeof(key_safe));
  if (!app_id_safe[0]) snprintf(app_id_safe, sizeof(app_id_safe), "default");
  if (!key_safe[0]) return false;

  char* pref = SDL_GetPrefPath("cogito", app_id_safe);
  if (!pref) return false;
  int wrote = snprintf(out, out_cap, "%s%s%s%s", pref,
                       "welcome_has_seen_", key_safe, ".flag");
  SDL_free(pref);
  return wrote > 0 && (size_t)wrote < out_cap;
}

static bool cogito_welcome_screen_has_seen_get(const char* key) {
  char path[PATH_MAX];
  if (!cogito_welcome_screen_has_seen_path(key, path, sizeof(path))) return false;
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  int ch = fgetc(f);
  fclose(f);
  return ch == '1';
}

static void cogito_welcome_screen_has_seen_set(const char* key, bool value) {
  char path[PATH_MAX];
  if (!cogito_welcome_screen_has_seen_path(key, path, sizeof(path))) return;
  if (!value) {
    remove(path);
    return;
  }
  FILE* f = fopen(path, "wb");
  if (!f) return;
  fputc('1', f);
  fclose(f);
}

static bool cogito_welcome_screen_has_seen_enabled(const CogitoNode* n) {
  return n && n->hint && n->hint->data && n->hint->data[0];
}

static bool cogito_welcome_screen_should_hide(CogitoNode* n) {
  if (!cogito_welcome_screen_has_seen_enabled(n)) return false;
  return cogito_welcome_screen_has_seen_get(n->hint->data);
}

static CogitoNode* cogito_welcome_screen_new_obj(YisStr* title) {
  CogitoNode* n = cogito_node_new(COGITO_WELCOME_SCREEN);
  cogito_node_set_text(n, title);
  n->hexpand = true;
  n->vexpand = true;
  return n;
}
static YisVal cogito_welcome_screen_new(YisVal titlev) {
  YisStr* ts = stdr_to_string(titlev);
  CogitoNode* n = cogito_welcome_screen_new_obj(ts);
  return YV_OBJ(n);
}
static void cogito_welcome_screen_set_description(YisVal wsv, YisVal descv) {
  if (wsv.tag != EVT_OBJ) yis_trap("cogito.welcome_screen_set_description expects welcome_screen");
  CogitoNode* n = (CogitoNode*)wsv.as.p;
  YisStr* s = stdr_to_string(descv);
  if (n->subtitle) yis_release_val(YV_STR(n->subtitle));
  n->subtitle = s;
  yis_retain_val(YV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_welcome_screen_set_icon(YisVal wsv, YisVal iconv) {
  if (wsv.tag != EVT_OBJ) yis_trap("cogito.welcome_screen_set_icon expects welcome_screen");
  CogitoNode* n = (CogitoNode*)wsv.as.p;
  YisStr* s = stdr_to_string(iconv);
  if (n->icon) yis_release_val(YV_STR(n->icon));
  n->icon = s;
  yis_retain_val(YV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_welcome_screen_set_action(YisVal wsv, YisVal textv, YisVal handlerv) {
  if (wsv.tag != EVT_OBJ) yis_trap("cogito.welcome_screen_set_action expects welcome_screen");
  CogitoNode* n = (CogitoNode*)wsv.as.p;
  YisStr* ts = stdr_to_string(textv);
  if (n->action_text) yis_release_val(YV_STR(n->action_text));
  n->action_text = ts;
  yis_retain_val(YV_STR(ts));
  if (n->on_action) yis_release_val(YV_FN(n->on_action));
  if (handlerv.tag == EVT_FN) {
    n->on_action = (YisFn*)handlerv.as.p;
    yis_retain_val(YV_FN(n->on_action));
  } else {
    n->on_action = NULL;
  }
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_welcome_screen_set_has_seen(YisVal wsv, YisVal has_seenv) {
  if (wsv.tag != EVT_OBJ) yis_trap("cogito.welcome_screen_set_has_seen expects welcome_screen");
  CogitoNode* n = (CogitoNode*)wsv.as.p;
  YisStr* s = stdr_to_string(has_seenv);
  if (n->hint) yis_release_val(YV_STR(n->hint));
  n->hint = s;
  yis_retain_val(YV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
