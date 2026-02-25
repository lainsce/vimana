static CogitoNode* cogito_popover_new_obj(void) {
  CogitoNode* pop = cogito_node_new(COGITO_POPOVER);
  return pop;
}

__attribute__((unused))
static ErgoVal cogito_popover_new(void) {
  CogitoNode* pop = cogito_popover_new_obj();
  return EV_OBJ(pop);
}

__attribute__((unused))
static void cogito_popover_show(ErgoVal anchorv, ErgoVal popoverv) {
  if (anchorv.tag != EVT_OBJ) ergo_trap("cogito.popover_show expects anchor node");
  if (popoverv.tag != EVT_OBJ) ergo_trap("cogito.popover_show expects popover");
  CogitoNode* anchor = (CogitoNode*)anchorv.as.p;
  CogitoNode* pop = (CogitoNode*)popoverv.as.p;

  // Find window to attach popover to
  CogitoNode* win = cogito_node_window(anchor);
  if (!win) return;
  cogito_overlay_focus_snapshot();

  // Release old popover if any
  if (win->popover) ergo_release_val(EV_OBJ(win->popover));

  win->popover = pop;
  win->popover_anchor = anchor;
  win->popover_open = true;
  pop->parent = win;
  ergo_retain_val(EV_OBJ(pop));

  cogito_apply_style_tree(pop);
  cogito_window_relayout(win);
  cogito_a11y_emit_event("overlay-open", pop, "popover");
}

__attribute__((unused))
static void cogito_popover_close(ErgoVal windowv) {
  if (windowv.tag != EVT_OBJ) ergo_trap("cogito.popover_close expects window");
  CogitoNode* win = (CogitoNode*)windowv.as.p;

  if (win->popover) {
    CogitoNode* cur = cogito_pointer_capture;
    while (cur) {
      if (cur == win->popover) {
        cogito_pointer_capture = NULL;
        break;
      }
      cur = cur->parent;
    }
    cur = cogito_timepicker_active;
    while (cur) {
      if (cur == win->popover) {
        cogito_timepicker_active = NULL;
        break;
      }
      cur = cur->parent;
    }
  }

  if (win->popover) {
    ergo_release_val(EV_OBJ(win->popover));
    win->popover = NULL;
  }
  win->popover_anchor = NULL;
  win->popover_open = false;
  cogito_overlay_focus_restore(win);
  cogito_window_relayout(win);
  cogito_a11y_emit_event("overlay-close", win, "popover");
}

__attribute__((unused))
static CogitoNode* cogito_find_popover(CogitoNode* n) {
  if (!n) return NULL;
  if (n->kind == COGITO_WINDOW && n->popover_open && n->popover) return n;
  return cogito_find_popover(n->parent);
}
