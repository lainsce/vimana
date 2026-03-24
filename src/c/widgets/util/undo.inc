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
    if (h->entries[i].spans) free(h->entries[i].spans);
    if (h->entries[i].para_props) free(h->entries[i].para_props);
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
  if (h->entries[next].spans) { free(h->entries[next].spans); h->entries[next].spans = NULL; }
  if (h->entries[next].para_props) { free(h->entries[next].para_props); h->entries[next].para_props = NULL; }
  // Snapshot current text
  YisStr* snap = n->text;
  if (snap) yis_retain_val(YV_STR(snap));
  h->entries[next].text = snap;
  h->entries[next].caret = n->kd->text_input.caret;
  h->entries[next].sel_start = n->kd->text_input.sel_start;
  h->entries[next].sel_end = n->kd->text_input.sel_end;
  h->entries[next].spans = cogito_spans_clone(n->spans, n->span_count);
  h->entries[next].span_count = n->span_count;
  h->entries[next].para_props = cogito_para_props_clone(n->para_props, n->para_props_count);
  h->entries[next].para_props_count = n->para_props_count;
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
    if (h->entries[redo_slot].spans) { free(h->entries[redo_slot].spans); h->entries[redo_slot].spans = NULL; }
    if (h->entries[redo_slot].para_props) { free(h->entries[redo_slot].para_props); h->entries[redo_slot].para_props = NULL; }
    YisStr* snap = n->text;
    if (snap) yis_retain_val(YV_STR(snap));
    h->entries[redo_slot].text = snap;
    h->entries[redo_slot].caret = n->kd->text_input.caret;
    h->entries[redo_slot].sel_start = n->kd->text_input.sel_start;
    h->entries[redo_slot].sel_end = n->kd->text_input.sel_end;
    h->entries[redo_slot].spans = cogito_spans_clone(n->spans, n->span_count);
    h->entries[redo_slot].span_count = n->span_count;
    h->entries[redo_slot].para_props = cogito_para_props_clone(n->para_props, n->para_props_count);
    h->entries[redo_slot].para_props_count = n->para_props_count;
    if (h->count < COGITO_UNDO_MAX) h->count++;
  }
  // Restore from current pos
  CogitoUndoEntry* e = &h->entries[h->pos];
  cogito_node_set_text(n, e->text);
  n->kd->text_input.caret = e->caret;
  n->kd->text_input.sel_start = e->sel_start;
  n->kd->text_input.sel_end = e->sel_end;
  // Restore spans
  if (n->spans) { free(n->spans); n->spans = NULL; }
  n->spans = cogito_spans_clone(e->spans, e->span_count);
  n->span_count = e->span_count;
  n->span_cap = e->span_count;
  // Restore paragraph props
  if (n->para_props) { free(n->para_props); n->para_props = NULL; }
  n->para_props = cogito_para_props_clone(e->para_props, e->para_props_count);
  n->para_props_count = e->para_props_count;
  n->para_props_cap = e->para_props_count;
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
    if (n->spans) { free(n->spans); n->spans = NULL; }
    n->spans = cogito_spans_clone(e->spans, e->span_count);
    n->span_count = e->span_count;
    n->span_cap = e->span_count;
    if (n->para_props) { free(n->para_props); n->para_props = NULL; }
    n->para_props = cogito_para_props_clone(e->para_props, e->para_props_count);
    n->para_props_count = e->para_props_count;
    n->para_props_cap = e->para_props_count;
  } else {
    CogitoUndoEntry* e = &h->entries[restore_slot];
    cogito_node_set_text(n, e->text);
    n->kd->text_input.caret = e->caret;
    n->kd->text_input.sel_start = e->sel_start;
    n->kd->text_input.sel_end = e->sel_end;
    if (n->spans) { free(n->spans); n->spans = NULL; }
    n->spans = cogito_spans_clone(e->spans, e->span_count);
    n->span_count = e->span_count;
    n->span_cap = e->span_count;
    if (n->para_props) { free(n->para_props); n->para_props = NULL; }
    n->para_props = cogito_para_props_clone(e->para_props, e->para_props_count);
    n->para_props_count = e->para_props_count;
    n->para_props_cap = e->para_props_count;
  }
  return true;
}
