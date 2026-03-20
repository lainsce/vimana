// undo.inc — Undo/Redo history for text input widgets

static CogitoUndoHistory* cogito_undo_ensure(CogitoNode* n) {
  if (!n || !n->kd) return NULL;
  CogitoUndoHistory* h = n->kd->text_input.undo;
  if (!h) {
    h = (CogitoUndoHistory*)calloc(1, sizeof(CogitoUndoHistory));
    if (!h) return NULL;
    h->pos = -1;
    n->kd->text_input.undo = h;
  }
  return h;
}

static void cogito_undo_free(CogitoUndoHistory* h) {
  if (!h) return;
  for (int i = 0; i < h->count; i++) {
    if (h->entries[i].text) yis_release_val(YV_STR(h->entries[i].text));
  }
  free(h);
}

// Push the current text state onto the undo stack.
// Call this BEFORE the text mutation so the snapshot captures the pre-edit state.
static void cogito_undo_push(CogitoNode* n) {
  CogitoUndoHistory* h = cogito_undo_ensure(n);
  if (!h) return;
  // Advance position in ring buffer
  int next = (h->pos + 1) % COGITO_UNDO_MAX;
  // Release old entry if overwriting
  if (h->entries[next].text) {
    yis_release_val(YV_STR(h->entries[next].text));
    h->entries[next].text = NULL;
  }
  // Snapshot current text
  YisStr* snap = n->text;
  if (snap) yis_retain_val(YV_STR(snap));
  h->entries[next].text = snap;
  h->entries[next].caret = n->kd->text_input.caret;
  h->entries[next].sel_start = n->kd->text_input.sel_start;
  h->entries[next].sel_end = n->kd->text_input.sel_end;
  h->pos = next;
  if (h->count < COGITO_UNDO_MAX) h->count++;
  // Discard redo history after a new edit
  h->redo_count = 0;
}

// Undo: restore the previous text state
static bool cogito_undo_perform(CogitoNode* n) {
  if (!n || !n->kd) return false;
  CogitoUndoHistory* h = n->kd->text_input.undo;
  if (!h || h->count == 0 || h->pos < 0) return false;
  int avail = h->count - h->redo_count;
  if (avail <= 0) return false;
  // Save current state for redo before restoring
  if (h->redo_count == 0) {
    // Push the present state so redo can get back here
    int redo_slot = (h->pos + 1) % COGITO_UNDO_MAX;
    if (h->entries[redo_slot].text) {
      yis_release_val(YV_STR(h->entries[redo_slot].text));
      h->entries[redo_slot].text = NULL;
    }
    YisStr* snap = n->text;
    if (snap) yis_retain_val(YV_STR(snap));
    h->entries[redo_slot].text = snap;
    h->entries[redo_slot].caret = n->kd->text_input.caret;
    h->entries[redo_slot].sel_start = n->kd->text_input.sel_start;
    h->entries[redo_slot].sel_end = n->kd->text_input.sel_end;
    if (h->count < COGITO_UNDO_MAX) h->count++;
  }
  // Restore from current pos
  CogitoUndoEntry* e = &h->entries[h->pos];
  cogito_node_set_text(n, e->text);
  n->kd->text_input.caret = e->caret;
  n->kd->text_input.sel_start = e->sel_start;
  n->kd->text_input.sel_end = e->sel_end;
  // Move pos backward
  h->pos = (h->pos - 1 + COGITO_UNDO_MAX) % COGITO_UNDO_MAX;
  h->redo_count++;
  return true;
}

// Redo: move forward in undo history
static bool cogito_redo_perform(CogitoNode* n) {
  if (!n || !n->kd) return false;
  CogitoUndoHistory* h = n->kd->text_input.undo;
  if (!h || h->redo_count <= 0) return false;
  // Move pos forward
  h->pos = (h->pos + 1) % COGITO_UNDO_MAX;
  h->redo_count--;
  // Restore the next entry
  int restore_slot = (h->pos + 1) % COGITO_UNDO_MAX;
  if (h->redo_count == 0) {
    // Restoring to head (present state)
    CogitoUndoEntry* e = &h->entries[restore_slot];
    cogito_node_set_text(n, e->text);
    n->kd->text_input.caret = e->caret;
    n->kd->text_input.sel_start = e->sel_start;
    n->kd->text_input.sel_end = e->sel_end;
  } else {
    CogitoUndoEntry* e = &h->entries[restore_slot];
    cogito_node_set_text(n, e->text);
    n->kd->text_input.caret = e->caret;
    n->kd->text_input.sel_start = e->sel_start;
    n->kd->text_input.sel_end = e->sel_end;
  }
  return true;
}
