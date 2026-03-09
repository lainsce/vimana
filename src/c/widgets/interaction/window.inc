static void cogito_window_rebuild(CogitoNode* win) {
  if (!win || !win->builder) return;
  cogito_clear_children(win);
  YisVal wv = YV_OBJ(win);
  yis_retain_val(wv);
  YisVal ret = win->builder->fn(win->builder->env, 1, &wv);
  yis_release_val(wv);
  yis_release_val(ret);
  cogito_apply_style_tree(win);
  if (win->auto_size) {
    int dw = 0;
    int dh = 0;
    cogito_window_desired_size(win, &dw, &dh);
    if (dw > 0 && dh > 0) {
      win->w = dw;
      win->h = dh;
      if (cogito_backend_ready && cogito_active_backend_window) {
        cogito_backend->window_set_size(cogito_active_backend_window, dw, dh);
      }
    }
  }
  cogito_layout_window(win);
}
static CogitoNode* cogito_window_new_obj(YisStr* title, int w, int h) {
  CogitoNode* win = cogito_node_new(COGITO_WINDOW);
  cogito_node_set_text(win, title);
  win->w = w;
  win->h = h;
  win->auto_size = true;
  win->resize_min_w = 0;
  win->resize_min_h = 0;
  return win;
}
static YisVal cogito_window_new(YisVal title, YisVal w, YisVal h) {
  if (cogito_debug_enabled()) {
    fprintf(stderr, "cogito: window_new\n");
    fflush(stderr);
  }
  YisStr* ts = stdr_to_string(title);
  int wi = (int)yis_as_int(w);
  int hi = (int)yis_as_int(h);
  CogitoNode* win = cogito_window_new_obj(ts, wi, hi);
  return YV_OBJ(win);
}
static void cogito_window_set_autosize(YisVal winv, YisVal onv) {
  if (winv.tag != EVT_OBJ) yis_trap("cogito.window_set_autosize expects window");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  win->auto_size = yis_as_bool(onv);
}
static void cogito_window_set_resizable(YisVal winv, YisVal onv) {
  if (winv.tag != EVT_OBJ) yis_trap("cogito.window_set_resizable expects window");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  win->resizable = yis_as_bool(onv);
  // Window resizability is set at creation time in SDL3
  // For runtime changes, we'd need to recreate the window or use SDL hints
  (void)win;
  cogito_window_relayout(win);
}

static void cogito_window_set_size_request(YisVal winv, YisVal wv, YisVal hv) {
  if (winv.tag != EVT_OBJ) yis_trap("cogito.window_set_size_request expects window");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  int min_w = (int)yis_as_int(wv);
  int min_h = (int)yis_as_int(hv);
  if (min_w < 0) min_w = 0;
  if (min_h < 0) min_h = 0;

  win->resize_min_w = min_w;
  win->resize_min_h = min_h;

  if (win->w < min_w) win->w = min_w;
  if (win->h < min_h) win->h = min_h;

  if (cogito_backend_ready && cogito_active_backend_window) {
    if (cogito_backend->window_set_min_size) {
      cogito_backend->window_set_min_size(cogito_active_backend_window, min_w, min_h);
    }
    cogito_backend->window_set_size(cogito_active_backend_window, win->w, win->h);
  }

  cogito_window_relayout(win);
}

static void cogito_window_set_builder(YisVal winv, YisVal builder) {
  if (winv.tag != EVT_OBJ) yis_trap("cogito.window_set_builder expects window");
  if (builder.tag != EVT_FN) yis_trap("cogito.window_set_builder expects function");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  if (win->builder) yis_release_val(YV_FN(win->builder));
  win->builder = (YisFn*)builder.as.p;
  yis_retain_val(YV_FN(win->builder));
}
static void cogito_window_on_resize(YisVal winv, YisVal handler) {
  if (winv.tag != EVT_OBJ) yis_trap("cogito.window_on_resize expects window");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  if (win->on_resize) yis_release_val(YV_FN(win->on_resize));
  if (handler.tag == EVT_FN) {
    win->on_resize = (YisFn*)handler.as.p;
    yis_retain_val(YV_FN(win->on_resize));
  } else {
    win->on_resize = NULL;
  }
}
static YisVal cogito_window_is_compact(YisVal winv) {
  if (winv.tag != EVT_OBJ) return YV_BOOL(false);
  CogitoNode* win = (CogitoNode*)winv.as.p;
  return YV_BOOL(cogito_window_node_is_compact(win));
}
static void cogito_window_set_dialog(YisVal winv, YisVal dialogv) {
  if (winv.tag != EVT_OBJ) yis_trap("cogito.window_set_dialog expects window");
  if (dialogv.tag != EVT_OBJ) yis_trap("cogito.window_set_dialog expects dialog");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  CogitoNode* dlg = (CogitoNode*)dialogv.as.p;
  if (dlg->kind == COGITO_WELCOME_SCREEN && cogito_welcome_screen_should_hide(dlg)) {
    win->dialog_open = false;
    cogito_window_relayout(win);
    return;
  }
  if (dlg->kind == COGITO_DIALOG_SLOT) {
    if (win->dialog && win->dialog != dlg) {
      yis_release_val(YV_OBJ(win->dialog));
    }
    win->dialog = dlg;
    win->dialog->parent = win;
    yis_retain_val(YV_OBJ(win->dialog));
    // Only mark as open if the slot actually has a dialog
    if (dlg->dialog && dlg->dialog_open) {
      win->dialog_open = true;
      dlg->dialog->parent = dlg;
    } else {
      win->dialog_open = false;
    }
    cogito_apply_style_tree(dlg);
    cogito_window_relayout(win);
    return;
  }
  if (!win->dialog || win->dialog->kind != COGITO_DIALOG_SLOT) {
    if (win->dialog) {
      yis_release_val(YV_OBJ(win->dialog));
      win->dialog = NULL;
    }
    win->dialog = cogito_dialog_slot_new_obj();
    win->dialog->parent = win;
    yis_retain_val(YV_OBJ(win->dialog));
  }
  cogito_dialog_slot_show(YV_OBJ(win->dialog), dialogv);
  win->dialog_open = true;
  cogito_overlay_anim_start(dlg, 0.0f, 1.0f, 0.22);
}
static void cogito_window_clear_dialog(YisVal winv) {
  if (winv.tag != EVT_OBJ) yis_trap("cogito.window_clear_dialog expects window");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  static int cogito_debug_hit = -1;
  if (cogito_debug_hit < 0) {
    const char* env = getenv("COGITO_DEBUG_HIT");
    cogito_debug_hit = (env && env[0] && env[0] != '0') ? 1 : 0;
  }
  if (cogito_debug_hit) {
    fprintf(stderr, "cogito: window_clear_dialog win=%p dialog=%p kind=%d open=%d\n",
            (void*)win, win ? (void*)win->dialog : NULL,
            win && win->dialog ? (int)win->dialog->kind : -1,
            win ? (int)win->dialog_open : 0);
    fflush(stderr);
  }
  if (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT) {
    cogito_dialog_slot_clear(YV_OBJ(win->dialog));
  } else if (win->dialog) {
    float from = cogito_overlay_anim_value(win->dialog, true);
    cogito_overlay_anim_start(win->dialog, from, 0.0f, 0.18);
  }
  win->dialog_open = false;
  cogito_overlay_focus_restore(win);
  if (cogito_debug_hit) {
    fprintf(stderr, "cogito: window_clear_dialog calling relayout\n");
    fflush(stderr);
  }
  cogito_window_relayout(win);
  if (cogito_debug_hit) {
    fprintf(stderr, "cogito: window_clear_dialog relayout done\n");
    fflush(stderr);
  }
}

static void cogito_window_set_side_sheet(YisVal winv, YisVal side_sheetv) {
  if (winv.tag != EVT_OBJ) yis_trap("cogito.window_set_side_sheet expects window");
  if (side_sheetv.tag != EVT_OBJ) yis_trap("cogito.window_set_side_sheet expects side_sheet");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  CogitoNode* ss = (CogitoNode*)side_sheetv.as.p;
  if (ss->kind != COGITO_SIDE_SHEET) yis_trap("cogito.window_set_side_sheet expects SideSheet node");

  if (win->side_sheet_node && win->side_sheet_node != ss) {
    yis_release_val(YV_OBJ(win->side_sheet_node));
  }
  win->side_sheet_node = ss;
  win->side_sheet_node->parent = win;
  win->side_sheet_open = true;
  win->side_sheet_closing = false;
  if (ss->side_sheet.mode == COGITO_SIDE_SHEET_SIDEBAR) {
    cogito_overlay_anim_start(ss, 1.0f, 1.0f, 0.0);
  } else {
    cogito_overlay_anim_start(ss, 0.0f, 1.0f, 0.24);
  }
  yis_retain_val(YV_OBJ(win->side_sheet_node));
  cogito_apply_style_tree(ss);

  // Keep coplanar sidebar width accounting exact using stored applied width,
  // so repeated rebuilds/open/close never drift due intrinsic-width changes.
  if (cogito_backend && cogito_backend->window_set_size && cogito_active_backend_window) {
    int new_sidebar_w = 0;
    if (ss->side_sheet.mode == COGITO_SIDE_SHEET_SIDEBAR) {
      cogito_intrinsic_size(ss, &new_sidebar_w, NULL);
      if (new_sidebar_w < 0) new_sidebar_w = 0;
    }
    int old_applied_w = win->side_sheet_sidebar_applied_w;
    if (old_applied_w < 0) old_applied_w = 0;
    int delta_w = new_sidebar_w - old_applied_w;
    if (delta_w != 0) {
      int target_w = win->w + delta_w;
      if (target_w < 1) target_w = 1;
      win->w = target_w;
      cogito_backend->window_set_size(cogito_active_backend_window, win->w, win->h);
    }
    win->side_sheet_sidebar_applied_w = new_sidebar_w;
  }
  cogito_window_relayout(win);
}

static void cogito_window_clear_side_sheet(YisVal winv) {
  if (winv.tag != EVT_OBJ) yis_trap("cogito.window_clear_side_sheet expects window");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  if (!win->side_sheet_node) {
    win->side_sheet_open = false;
    win->side_sheet_closing = false;
    return;
  }
  // Shrink by exactly the width previously applied for sidebar mode.
  if (cogito_backend && cogito_backend->window_set_size && cogito_active_backend_window) {
    int applied_w = win->side_sheet_sidebar_applied_w;
    if (applied_w < 0) applied_w = 0;
    if (applied_w > 0) {
      int target_w = win->w - applied_w;
      if (target_w < 1) target_w = 1;
      win->w = target_w;
      cogito_backend->window_set_size(cogito_active_backend_window, win->w, win->h);
    }
    win->side_sheet_sidebar_applied_w = 0;
  }

  // Sidebar closes instantly — no slide animation.
  if (win->side_sheet_node->side_sheet.mode == COGITO_SIDE_SHEET_SIDEBAR) {
    win->side_sheet_open = false;
    win->side_sheet_closing = false;
    yis_release_val(YV_OBJ(win->side_sheet_node));
    win->side_sheet_node = NULL;
    cogito_window_relayout(win);
    return;
  }
  float from = cogito_overlay_anim_value(win->side_sheet_node, true);
  cogito_overlay_anim_start(win->side_sheet_node, from, 0.0f, 0.20);
  win->side_sheet_open = false;
  win->side_sheet_closing = true;
  cogito_window_relayout(win);
}
