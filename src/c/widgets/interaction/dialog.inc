static CogitoNode* cogito_dialog_new_obj(ErgoStr* title) {
  CogitoNode* dlg = cogito_node_new(COGITO_DIALOG);
  cogito_node_set_text(dlg, title);
  return dlg;
}
#define COGITO_DIALOG_HEADER_INSET 24
#define COGITO_DIALOG_CLOSE_BOX 24
static void cogito_dialog_close_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!n || n->kind != COGITO_DIALOG) return;
  int pad_t = COGITO_DIALOG_HEADER_INSET;
  int pad_r = COGITO_DIALOG_HEADER_INSET;
  int title_size = 20;
  int title_h = cogito_text_height_size(title_size);
  int box = COGITO_DIALOG_CLOSE_BOX;
  int header_h = title_h > box ? title_h : box;
  int x = n->x + n->w - pad_r - box;
  int y = n->y + pad_t + (header_h - box) / 2;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = box;
  if (out_h) *out_h = box;
}
static CogitoNode* cogito_dialog_slot_new_obj(void) {
  return cogito_node_new(COGITO_DIALOG_SLOT);
}
static ErgoVal cogito_dialog_new(ErgoVal title) {
  ErgoStr* ts = stdr_to_string(title);
  CogitoNode* dlg = cogito_dialog_new_obj(ts);
  return EV_OBJ(dlg);
}
static ErgoVal cogito_dialog_slot_new(void) {
  return EV_OBJ(cogito_dialog_slot_new_obj());
}
static void cogito_dialog_slot_show(ErgoVal slotv, ErgoVal dialogv) {
  if (slotv.tag != EVT_OBJ) ergo_trap("cogito.dialog_slot_show expects dialog slot");
  if (dialogv.tag != EVT_OBJ) ergo_trap("cogito.dialog_slot_show expects dialog");
  CogitoNode* slot = (CogitoNode*)slotv.as.p;
  CogitoNode* dlg = (CogitoNode*)dialogv.as.p;
  CogitoNode* win = cogito_node_window(slot);
  cogito_overlay_focus_snapshot();
  if (slot->dialog) ergo_release_val(EV_OBJ(slot->dialog));
  slot->dialog = dlg;
  slot->dialog_open = true;
  dlg->parent = slot;
  ergo_retain_val(EV_OBJ(dlg));
  if (win) {
    win->dialog = slot;
    win->dialog_open = true;
  }
  cogito_apply_style_tree(dlg);
  cogito_window_relayout(cogito_node_window(slot));
  cogito_a11y_emit_event("overlay-open", dlg, "dialog");
}
static void cogito_dialog_slot_clear(ErgoVal slotv) {
  if (slotv.tag != EVT_OBJ) ergo_trap("cogito.dialog_slot_clear expects dialog slot");
  CogitoNode* slot = (CogitoNode*)slotv.as.p;
  slot->dialog_open = false;
  CogitoNode* win = cogito_node_window(slot);
  if (win && win->dialog == slot) {
    win->dialog_open = false;
  }
  cogito_overlay_focus_restore(win ? win : slot);
  cogito_window_relayout(cogito_node_window(slot));
  cogito_a11y_emit_event("overlay-close", slot, "dialog");
}
static void cogito_dialog_close(ErgoVal dialogv) {
  if (dialogv.tag != EVT_OBJ) ergo_trap("cogito.dialog_close expects dialog");
  CogitoNode* dlg = (CogitoNode*)dialogv.as.p;
  if (dlg->parent && dlg->parent->kind == COGITO_DIALOG_SLOT) {
    dlg->parent->dialog_open = false;
    CogitoNode* win = cogito_node_window(dlg->parent);
    if (win && win->dialog == dlg->parent) {
      win->dialog_open = false;
    }
    cogito_overlay_focus_restore(win ? win : dlg->parent);
    cogito_window_relayout(cogito_node_window(dlg->parent));
    cogito_a11y_emit_event("overlay-close", dlg, "dialog");
    return;
  }
  // Fallback for legacy direct-window dialogs or detached dialog objects.
  CogitoNode* win = cogito_node_window(dlg);
  if (win && win->dialog == dlg) {
    win->dialog_open = false;
    cogito_overlay_focus_restore(win);
    cogito_window_relayout(win);
    cogito_a11y_emit_event("overlay-close", dlg, "dialog");
    return;
  }
  if (win && win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog == dlg) {
    win->dialog->dialog_open = false;
    win->dialog_open = false;
    cogito_overlay_focus_restore(win);
    cogito_window_relayout(win);
    cogito_a11y_emit_event("overlay-close", dlg, "dialog");
  }
}
static void cogito_dialog_remove(ErgoVal dialogv) {
  if (dialogv.tag != EVT_OBJ) ergo_trap("cogito.dialog_remove expects dialog");
  CogitoNode* dlg = (CogitoNode*)dialogv.as.p;
  CogitoNode* parent = dlg->parent;
  if (parent) {
    cogito_container_remove_child(parent, dlg);
    cogito_window_relayout(cogito_node_window(parent));
  }
}
static ErgoVal cogito_node_parent(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_parent expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->parent) return EV_OBJ(n->parent);
  return EV_NULLV;
}
static CogitoNode* cogito_find_dialog_slot(CogitoNode* n) {
  if (!n) return NULL;
  if (n->kind == COGITO_DIALOG_SLOT && n->dialog_open && n->dialog) return n;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* hit = cogito_find_dialog_slot(n->children[i]);
    if (hit) return hit;
  }
  return NULL;
}
