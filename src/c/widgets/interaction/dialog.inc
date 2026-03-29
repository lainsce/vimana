static CogitoNode* cogito_dialog_new_obj(YisStr* title) {
  CogitoNode* dlg = cogito_node_new(COGITO_DIALOG);
  cogito_node_set_text(dlg, title);
  return dlg;
}
#define COGITO_DIALOG_HEADER_INSET 24
#define COGITO_DIALOG_CLOSE_BOX 24
static void cogito_dialog_close_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!n || n->kind != COGITO_DIALOG) return;
  if (n->kd->dialog.no_close) {
    if (out_x) *out_x = 0;
    if (out_y) *out_y = 0;
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    return;
  }
  int pad_t = COGITO_DIALOG_HEADER_INSET;
  int pad_r = COGITO_DIALOG_HEADER_INSET;
  int title_size = 20;
  int title_h = cogito_text_height_size(title_size);
  int box = COGITO_DIALOG_CLOSE_BOX;
  int header_h = title_h > box ? title_h : box;
  int x = cogito_is_rtl() ? (n->x + COGITO_DIALOG_HEADER_INSET) : (n->x + n->w - pad_r - box);
  int y = n->y + pad_t + (header_h - box) / 2;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = box;
  if (out_h) *out_h = box;
}
static CogitoNode* cogito_dialog_slot_new_obj(void) {
  return cogito_node_new(COGITO_DIALOG_SLOT);
}
static YisVal cogito_dialog_new(YisVal title) {
  YisStr* ts = stdr_to_string(title);
  CogitoNode* dlg = cogito_dialog_new_obj(ts);
  return YV_OBJ(dlg);
}
static void cogito_dialog_set_no_close(YisVal dialogv, YisVal nc) {
  if (dialogv.tag != EVT_OBJ) yis_trap("cogito.dialog_set_no_close expects dialog");
  CogitoNode* n = (CogitoNode*)dialogv.as.p;
  n->kd->dialog.no_close = yis_as_bool(nc);
  cogito_window_relayout(cogito_node_window(n));
}
static YisVal cogito_dialog_get_no_close(YisVal dialogv) {
  if (dialogv.tag != EVT_OBJ) yis_trap("cogito.dialog_get_no_close expects dialog");
  CogitoNode* n = (CogitoNode*)dialogv.as.p;
  return YV_BOOL(n->kd->dialog.no_close);
}
static YisVal cogito_dialog_slot_new(void) {
  return YV_OBJ(cogito_dialog_slot_new_obj());
}
static void cogito_dialog_slot_show(YisVal slotv, YisVal dialogv) {
  if (slotv.tag != EVT_OBJ) yis_trap("cogito.dialog_slot_show expects dialog slot");
  if (dialogv.tag != EVT_OBJ) yis_trap("cogito.dialog_slot_show expects dialog");
  CogitoNode* slot = (CogitoNode*)slotv.as.p;
  CogitoNode* dlg = (CogitoNode*)dialogv.as.p;
  CogitoNode* win = cogito_node_window(slot);
  cogito_overlay_focus_snapshot();
  if (slot->dialog) yis_release_val(YV_OBJ(slot->dialog));
  slot->dialog = dlg;
  slot->dialog_open = true;
  cogito_overlay_anim_start(dlg, 0.0f, 1.0f, 0.22);
  dlg->parent = slot;
  yis_retain_val(YV_OBJ(dlg));
  if (win) {
    win->dialog = slot;
    win->dialog_open = true;
  }
  cogito_apply_style_tree(dlg);
  cogito_window_relayout(cogito_node_window(slot));
  cogito_a11y_emit_event("overlay-open", dlg, "dialog");
}
static void cogito_dialog_slot_clear(YisVal slotv) {
  if (slotv.tag != EVT_OBJ) yis_trap("cogito.dialog_slot_clear expects dialog slot");
  CogitoNode* slot = (CogitoNode*)slotv.as.p;
  if (slot->dialog) {
    float from = cogito_overlay_anim_value(slot->dialog, true);
    cogito_overlay_anim_start(slot->dialog, from, 0.0f, 0.18);
  }
  slot->dialog_open = false;
  CogitoNode* win = cogito_node_window(slot);
  if (win && win->dialog == slot) {
    win->dialog_open = false;
  }
  cogito_overlay_focus_restore(win ? win : slot);
  cogito_window_relayout(cogito_node_window(slot));
  cogito_a11y_emit_event("overlay-close", slot, "dialog");
}
static void cogito_dialog_close(YisVal dialogv) {
  if (dialogv.tag != EVT_OBJ) yis_trap("cogito.dialog_close expects dialog");
  CogitoNode* dlg = (CogitoNode*)dialogv.as.p;
  if (dlg->parent && dlg->parent->kind == COGITO_DIALOG_SLOT) {
    float from = cogito_overlay_anim_value(dlg, true);
    cogito_overlay_anim_start(dlg, from, 0.0f, 0.18);
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
    float from = cogito_overlay_anim_value(dlg, true);
    cogito_overlay_anim_start(dlg, from, 0.0f, 0.18);
    win->dialog_open = false;
    cogito_overlay_focus_restore(win);
    cogito_window_relayout(win);
    cogito_a11y_emit_event("overlay-close", dlg, "dialog");
    return;
  }
  if (win && win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog == dlg) {
    float from = cogito_overlay_anim_value(dlg, true);
    cogito_overlay_anim_start(dlg, from, 0.0f, 0.18);
    win->dialog->dialog_open = false;
    win->dialog_open = false;
    cogito_overlay_focus_restore(win);
    cogito_window_relayout(win);
    cogito_a11y_emit_event("overlay-close", dlg, "dialog");
  }
}
static void cogito_dialog_remove(YisVal dialogv) {
  if (dialogv.tag != EVT_OBJ) yis_trap("cogito.dialog_remove expects dialog");
  CogitoNode* dlg = (CogitoNode*)dialogv.as.p;
  CogitoNode* parent = dlg->parent;
  if (parent) {
    cogito_container_remove_child(parent, dlg);
    cogito_window_relayout(cogito_node_window(parent));
  }
}
static YisVal cogito_node_parent(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_parent expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->parent) return YV_OBJ(n->parent);
  return YV_NULLV;
}
static CogitoNode* cogito_find_dialog_slot(CogitoNode* n) {
  if (!n) return NULL;
  if (cogito_node_is_hidden(n)) return NULL;
  if (n->kind == COGITO_DIALOG_SLOT && n->dialog_open && n->dialog) return n;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* hit = cogito_find_dialog_slot(n->children[i]);
    if (hit) return hit;
  }
  return NULL;
}
