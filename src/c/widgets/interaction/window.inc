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
static void cogito_window_set_builder(YisVal winv, YisVal builder) {
  if (winv.tag != EVT_OBJ) yis_trap("cogito.window_set_builder expects window");
  if (builder.tag != EVT_FN) yis_trap("cogito.window_set_builder expects function");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  if (win->builder) yis_release_val(YV_FN(win->builder));
  win->builder = (YisFn*)builder.as.p;
  yis_retain_val(YV_FN(win->builder));
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
  cogito_overlay_anim_start(ss, 0.0f, 1.0f, 0.24);
  yis_retain_val(YV_OBJ(win->side_sheet_node));
  cogito_apply_style_tree(ss);
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
  float from = cogito_overlay_anim_value(win->side_sheet_node, true);
  cogito_overlay_anim_start(win->side_sheet_node, from, 0.0f, 0.20);
  win->side_sheet_open = false;
  win->side_sheet_closing = true;
  cogito_window_relayout(win);
}
