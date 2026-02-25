static void cogito_appbar_window_action(CogitoNode* win, int action) {
  if (action == 0) {
    if (win) win->should_close = true;
  } else if (action == 1) {
    if (cogito_backend && cogito_backend->window_minimize) {
      CogitoWindow* backend_win = cogito_backend_window_for_node(win);
      if (backend_win) {
        cogito_backend->window_minimize(backend_win);
      }
    }
  } else if (action == 2) {
    if (cogito_backend) {
      CogitoWindow* backend_win = cogito_backend_window_for_node(win);
      if (backend_win) {
        bool is_max = false;
        if (cogito_backend->window_is_maximized) {
          is_max = cogito_backend->window_is_maximized(backend_win);
        }
        if (is_max) {
          if (cogito_backend->window_restore) {
            cogito_backend->window_restore(backend_win);
          }
        } else {
          if (cogito_backend->window_maximize) {
            cogito_backend->window_maximize(backend_win);
          }
        }
      }
    }
  }
}
static CogitoNode* cogito_appbar_new_obj(ErgoStr* title, ErgoStr* subtitle) {
  CogitoNode* ab = cogito_node_new(COGITO_APPBAR);
  cogito_node_set_text(ab, title);
  cogito_node_set_subtitle(ab, subtitle);
  ab->appbar.btn_close_x = -1;
  ab->appbar.btn_min_x = -1;
  ab->appbar.btn_max_x = -1;
  strncpy(ab->appbar.controls, "CIM|", sizeof(ab->appbar.controls) - 1);
  ab->appbar.controls[sizeof(ab->appbar.controls) - 1] = 0;
  return ab;
}
static ErgoVal cogito_appbar_new(ErgoVal title, ErgoVal subtitle) {
  ErgoStr* ts = stdr_to_string(title);
  ErgoStr* ss = stdr_to_string(subtitle);
  CogitoNode* ab = cogito_appbar_new_obj(ts, ss);
  return EV_OBJ(ab);
}
static ErgoVal cogito_appbar_add_button(ErgoVal appv, ErgoVal text, ErgoVal handler) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.appbar_add_button expects appbar");
  CogitoNode* ab = (CogitoNode*)appv.as.p;
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* btn = cogito_iconbtn_new_obj(ts);
  // AppBar action icons default to iconic treatment.
  btn->button.icon_color_style = 3;
  if (handler.tag == EVT_FN) {
    btn->on_click = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(btn->on_click));
  }
  cogito_children_add(ab, btn);
  return EV_OBJ(btn);
}
static void cogito_appbar_set_controls(ErgoVal appv, ErgoVal layout) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.appbar_set_controls expects appbar");
  if (layout.tag != EVT_STR) ergo_trap("cogito.appbar_set_controls expects string");
  CogitoNode* ab = (CogitoNode*)appv.as.p;
  ErgoStr* ls = (ErgoStr*)layout.as.p;
  const char* s = ls ? ls->data : "";
  char buf[16];
  size_t j = 0;
  bool has_split = false;
  for (size_t i = 0; s[i] && j + 1 < sizeof(buf); i++) {
    char c = s[i];
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c == 'C' || c == 'M' || c == 'I') {
      buf[j++] = c;
    } else if (c == '|') {
      if (!has_split) {
        buf[j++] = c;
        has_split = true;
      }
    }
  }
  buf[j] = 0;
  if (j == 0) {
    strncpy(ab->appbar.controls, "CIM|", sizeof(ab->appbar.controls) - 1);
    ab->appbar.controls[sizeof(ab->appbar.controls) - 1] = 0;
  } else {
    strncpy(ab->appbar.controls, buf, sizeof(ab->appbar.controls) - 1);
    ab->appbar.controls[sizeof(ab->appbar.controls) - 1] = 0;
  }
  CogitoNode* win = cogito_root(ab);
  if (win && win->kind == COGITO_WINDOW) {
    cogito_window_relayout(win);
  }
}

static void cogito_appbar_set_title(ErgoVal appv, ErgoVal titlev) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.appbar_set_title expects appbar");
  CogitoNode* ab = (CogitoNode*)appv.as.p;
  ErgoStr* ts = stdr_to_string(titlev);
  cogito_node_set_text(ab, ts);
  CogitoNode* win = cogito_root(ab);
  if (win && win->kind == COGITO_WINDOW) {
    cogito_window_relayout(win);
  }
}

static void cogito_appbar_set_subtitle(ErgoVal appv, ErgoVal subtitlev) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.appbar_set_subtitle expects appbar");
  CogitoNode* ab = (CogitoNode*)appv.as.p;
  ErgoStr* ss = stdr_to_string(subtitlev);
  cogito_node_set_subtitle(ab, ss);
  CogitoNode* win = cogito_root(ab);
  if (win && win->kind == COGITO_WINDOW) {
    cogito_window_relayout(win);
  }
}
// Helper functions moved to 02_theme.inc to be available earlier
