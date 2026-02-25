// CogitoApp lifecycle management
// App object creation, accent color, system theme detection.

static void cogito_app_drop(ErgoObj* o) {
  CogitoApp* app = (CogitoApp*)o;
  if (app->app_id) {
    ergo_release_val(EV_STR(app->app_id));
    app->app_id = NULL;
  }
  if (app->app_name) {
    ergo_release_val(EV_STR(app->app_name));
    app->app_name = NULL;
  }
  if (app->app_icon) {
    ergo_release_val(EV_STR(app->app_icon));
    app->app_icon = NULL;
  }
}

static bool cogito_system_prefers_reduced_motion(void) {
  const char* v = getenv("COGITO_REDUCED_MOTION");
  if (!v || !v[0]) v = getenv("ERGO_REDUCED_MOTION");
  if (v && (strcmp(v, "1") == 0 || strcmp(v, "true") == 0 || strcmp(v, "yes") == 0 || strcmp(v, "on") == 0)) return true;
  v = getenv("COGITO_MOTION");
  if (!v || !v[0]) v = getenv("ERGO_MOTION");
  if (v && (strstr(v, "reduce") || strstr(v, "REDUCE") || strstr(v, "minimal"))) return true;
  return false;
}

static bool cogito_system_prefers_high_contrast(void) {
  const char* v = getenv("COGITO_HIGH_CONTRAST");
  if (!v || !v[0]) v = getenv("ERGO_HIGH_CONTRAST");
  if (v && (strcmp(v, "1") == 0 || strcmp(v, "true") == 0 || strcmp(v, "yes") == 0 || strcmp(v, "on") == 0)) return true;
  v = getenv("COGITO_CONTRAST");
  if (!v || !v[0]) v = getenv("ERGO_CONTRAST");
  if (v && (strstr(v, "high") || strstr(v, "HIGH"))) return true;
  return false;
}

static CogitoApp* cogito_app_new_obj(void) {
  CogitoApp* app = (CogitoApp*)ergo_obj_new(sizeof(CogitoApp), cogito_app_drop);
  app->native = NULL;
  app->app_id = NULL;
  app->app_name = NULL;
  app->accent_override = false;
  app->dark_override = false;
  app->dark_mode = false;
  app->accent_color = cogito_rgba(114, 222, 194, 255);
  app->accent_set = false;
  app->system_dark = false;
  app->system_accent = cogito_rgba(114, 222, 194, 255);
  app->system_accent_set = false;
  app->system_theme_next_check = 0.0;
  app->app_icon = NULL;
  return app;
}

static CogitoColor cogito_app_effective_accent(ErgoVal appv) {
  CogitoApp* app = (CogitoApp*)appv.as.p;
  if (app->accent_override && app->accent_set) return app->accent_color;
  if (app->system_accent_set) return app->system_accent;
  if (app->accent_set) return app->accent_color;
  return cogito_rgba(114, 222, 194, 255);
}

static void cogito_app_update_theme(ErgoVal appv) {
  CogitoApp* app = (CogitoApp*)appv.as.p;
  bool dark = app ? (app->dark_override ? app->dark_mode : app->system_dark) : false;
  CogitoColor accent = app ? cogito_app_effective_accent(appv) : cogito_rgba(114, 222, 194, 255);
  cogito_theme_applied_last_update = false;
  if (getenv("COGITO_THEME_TRACE")) {
    fprintf(stderr,
            "cogito: app_update_theme dark=%d accent=#%02X%02X%02X override=%d accent_set=%d system_accent_set=%d\n",
            (int)dark, accent.r, accent.g, accent.b,
            app ? (int)app->accent_override : 0,
            app ? (int)app->accent_set : 0,
            app ? (int)app->system_accent_set : 0);
    fflush(stderr);
  }
  // During startup scripts, many theme setters run before a window exists.
  // Defer expensive SUM rebuilds until the window is attached.
  if (!cogito_active_window) {
    cogito_theme_refresh_deferred = true;
    return;
  }
  if (cogito_theme_refresh_deferred ||
      !cogito_theme_initialized ||
      dark != cogito_theme_dark ||
      accent.r != cogito_theme_accent.r || accent.g != cogito_theme_accent.g ||
      accent.b != cogito_theme_accent.b) {
    cogito_theme_refresh_deferred = false;
    cogito_apply_theme(dark, accent);
    cogito_theme_applied_last_update = true;
  }
}

static void cogito_update_system_theme(ErgoVal appv) {
  CogitoApp* app = (CogitoApp*)appv.as.p;
  if (!app) return;
  {
    double now = cogito_now();
    if (app->system_theme_next_check > 0.0 && now < app->system_theme_next_check) {
      return;
    }
    // Poll at a short interval instead of every loop iteration.
    app->system_theme_next_check = now + 0.25;
  }
  bool dark = cogito_system_is_dark();
  CogitoColor acc = {0};
  bool has_acc = cogito_system_accent(&acc);
  bool changed = false;
  bool reduced_motion = cogito_system_prefers_reduced_motion();
  bool high_contrast = cogito_system_prefers_high_contrast();
  if (dark != app->system_dark) {
    app->system_dark = dark;
    if (!app->dark_override) changed = true;
  }
  if (has_acc && (!app->system_accent_set ||
      acc.r != app->system_accent.r || acc.g != app->system_accent.g || acc.b != app->system_accent.b)) {
    app->system_accent = acc;
    app->system_accent_set = true;
    if (!app->accent_override) changed = true;
  }
  // Poll system personalization settings
  int sys_scheme = cogito_system_ensor_scheme();
  if (sys_scheme > 0 && sys_scheme != cogito_theme_ensor_variant) {
    cogito_theme_ensor_variant = sys_scheme;
    changed = true;
  }
  double sys_contrast = cogito_system_contrast();
  if (high_contrast && sys_contrast < 0.8) sys_contrast = 1.0;
  if (sys_contrast != cogito_theme_contrast) {
    cogito_theme_contrast = sys_contrast;
    changed = true;
  }
  if (reduced_motion != cogito_prefers_reduced_motion) {
    cogito_prefers_reduced_motion = reduced_motion;
    changed = true;
  }
  if (high_contrast != cogito_prefers_high_contrast) {
    cogito_prefers_high_contrast = high_contrast;
    changed = true;
  }
  cogito_theme_font_weight = cogito_system_font_weight();
  cogito_theme_roundness = cogito_system_roundness();
  cogito_theme_spacing = cogito_system_spacing();
  if (changed) cogito_app_update_theme(appv);
}

static void cogito_app_set_appid(ErgoVal appv, ErgoVal idv) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.app_set_appid expects app");
  CogitoApp* app = (CogitoApp*)appv.as.p;
  ErgoStr* s = NULL;
  bool temp = false;
  if (idv.tag == EVT_STR) {
    s = (ErgoStr*)idv.as.p;
    if (s) ergo_retain_val(EV_STR(s));
  } else if (idv.tag != EVT_NULL) {
    s = stdr_to_string(idv);
    temp = true;
  }
  if (app->app_id) ergo_release_val(EV_STR(app->app_id));
  app->app_id = s;
  if (temp && s) {
    // app now owns temp string (ref=1)
  }
}

static void cogito_app_set_app_name(ErgoVal appv, ErgoVal namev) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.app_set_app_name expects app");
  CogitoApp* app = (CogitoApp*)appv.as.p;
  ErgoStr* s = NULL;
  bool temp = false;
  if (namev.tag == EVT_STR) {
    s = (ErgoStr*)namev.as.p;
    if (s) ergo_retain_val(EV_STR(s));
  } else if (namev.tag != EVT_NULL) {
    s = stdr_to_string(namev);
    temp = true;
  }
  if (app->app_name) ergo_release_val(EV_STR(app->app_name));
  app->app_name = s;
  if (temp && s) {
    // app now owns temp string (ref=1)
  }
}

static void cogito_app_set_accent_color(ErgoVal appv, ErgoVal colorv, ErgoVal follow_systemv) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.app_set_accent_color expects app");
  CogitoApp* app = (CogitoApp*)appv.as.p;
  bool follow_system = ergo_as_bool(follow_systemv);
  app->accent_override = !follow_system;
  if (colorv.tag != EVT_NULL) {
    ErgoStr* s = NULL;
    bool temp = false;
    if (colorv.tag == EVT_STR) {
      s = (ErgoStr*)colorv.as.p;
    } else {
      s = stdr_to_string(colorv);
      temp = true;
    }
    CogitoColor c = {0};
    if (s && s->data && cogito_hex_to_color(s->data, &c)) {
      app->accent_color = c;
      app->accent_set = true;
      if (getenv("COGITO_THEME_TRACE")) {
        fprintf(stderr, "cogito: app_set_accent_color color=%s parsed=#%02X%02X%02X follow_system=%d\n",
                s->data ? s->data : "", c.r, c.g, c.b, (int)follow_system);
        fflush(stderr);
      }
    } else if (getenv("COGITO_THEME_TRACE")) {
      fprintf(stderr, "cogito: app_set_accent_color failed to parse color=%s\n", (s && s->data) ? s->data : "");
      fflush(stderr);
    }
    if (temp && s) ergo_release_val(EV_STR(s));
  }
  cogito_app_update_theme(appv);
}

static void cogito_app_set_dark_mode(ErgoVal appv, ErgoVal darkv, ErgoVal follow_systemv) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.app_set_dark_mode expects app");
  CogitoApp* app = (CogitoApp*)appv.as.p;
  bool dark = ergo_as_bool(darkv);
  bool follow_system = ergo_as_bool(follow_systemv);
  bool prev_override = app->dark_override;
  bool prev_dark = app->dark_mode;
  app->dark_override = !follow_system;
  if (!follow_system) {
    app->dark_mode = dark;
  }
  if ((!follow_system && (prev_override != app->dark_override || prev_dark != app->dark_mode)) ||
      (follow_system && prev_override != app->dark_override)) {
    cogito_theme_request_window_reveal();
  }
  cogito_app_update_theme(appv);
}

static void cogito_app_set_icon(ErgoVal appv, ErgoVal pathv) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.app_set_icon expects app");
  CogitoApp* app = (CogitoApp*)appv.as.p;
  ErgoStr* s = NULL;
  bool temp = false;
  if (pathv.tag == EVT_STR) {
    s = (ErgoStr*)pathv.as.p;
    if (s) ergo_retain_val(EV_STR(s));
  } else if (pathv.tag != EVT_NULL) {
    s = stdr_to_string(pathv);
    temp = true;
  }
  if (app->app_icon) ergo_release_val(EV_STR(app->app_icon));
  app->app_icon = s;
  if (temp && s) {
    // app now owns temp string (ref=1)
  }
}

static const char* cogito_app_get_icon_path(CogitoApp* app) {
  if (app && app->app_icon && app->app_icon->data && app->app_icon->data[0]) {
    return app->app_icon->data;
  }
  return NULL;
}

static ErgoVal cogito_app_new(void) {
  if (cogito_debug_enabled()) {
    fprintf(stderr, "cogito: app_new\n");
    fflush(stderr);
  }
  CogitoApp* app = cogito_app_new_obj();
  ErgoVal appv = EV_OBJ(app);
  cogito_app_update_theme(appv);
  return EV_OBJ(app);
}
