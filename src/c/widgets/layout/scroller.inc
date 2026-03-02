typedef struct {
  char* key;
  int scroll_x;
  int scroll_y;
  unsigned long long stamp;
} CogitoScrollerState;

enum { COGITO_SCROLLER_STATE_CAP = 128 };
static CogitoScrollerState cogito_scroller_states[COGITO_SCROLLER_STATE_CAP];
static unsigned long long cogito_scroller_state_tick = 1;

static char* cogito_scroller_state_dup(const char* s) {
  if (!s || !s[0]) return NULL;
  size_t n = strlen(s);
  char* out = (char*)malloc(n + 1);
  if (!out) return NULL;
  memcpy(out, s, n + 1);
  return out;
}

static void cogito_scroller_state_make_key(const CogitoNode* n, char* out, size_t out_sz) {
  if (!out || out_sz == 0) return;
  out[0] = '\0';
  if (!n) return;

  if (n->view_id && n->view_id->data && n->view_id->data[0]) {
    snprintf(out, out_sz, "id:%s", n->view_id->data);
    return;
  }

  const CogitoNode* win = cogito_node_window((CogitoNode*)n);
  int w = snprintf(out, out_sz, "auto:%p", (void*)win);
  if (w < 0) return;
  size_t used = (size_t)w;
  if (used >= out_sz) used = out_sz - 1;

  const CogitoNode* chain[64];
  int depth = 0;
  const CogitoNode* cur = n;
  while (cur && depth < 64) {
    chain[depth++] = cur;
    if (cur == win) break;
    cur = cur->parent;
  }

  for (int i = depth - 1; i >= 0; i--) {
    const CogitoNode* node = chain[i];
    int idx = 0;
    if (node->parent) {
      for (size_t j = 0; j < node->parent->len; j++) {
        if (node->parent->children[j] == node) {
          idx = (int)j;
          break;
        }
      }
    }
    if (used >= out_sz - 1) break;
    int wr = snprintf(out + used, out_sz - used, "/%d:%d", (int)node->kind, idx);
    if (wr < 0) break;
    used += (size_t)wr;
    if (used >= out_sz) {
      used = out_sz - 1;
      break;
    }
  }
  out[used] = '\0';
}

static bool cogito_scroller_state_load(const CogitoNode* n, int* out_x, int* out_y) {
  if (!n) return false;
  char key_buf[512];
  cogito_scroller_state_make_key(n, key_buf, sizeof(key_buf));
  if (!key_buf[0]) return false;
  const char* key = key_buf;
  for (int i = 0; i < COGITO_SCROLLER_STATE_CAP; i++) {
    CogitoScrollerState* st = &cogito_scroller_states[i];
    if (!st->key) continue;
    if (strcmp(st->key, key) == 0) {
      if (out_x) *out_x = st->scroll_x;
      if (out_y) *out_y = st->scroll_y;
      return true;
    }
  }
  return false;
}

static void cogito_scroller_state_store(const CogitoNode* n) {
  if (!n) return;
  char key_buf[512];
  cogito_scroller_state_make_key(n, key_buf, sizeof(key_buf));
  if (!key_buf[0]) return;
  const char* key = key_buf;
  CogitoScrollerState* slot = NULL;
  CogitoScrollerState* oldest = &cogito_scroller_states[0];
  for (int i = 0; i < COGITO_SCROLLER_STATE_CAP; i++) {
    CogitoScrollerState* st = &cogito_scroller_states[i];
    if (!st->key) {
      slot = st;
      break;
    }
    if (strcmp(st->key, key) == 0) {
      slot = st;
      break;
    }
    if (st->stamp < oldest->stamp) oldest = st;
  }
  if (!slot) slot = oldest;
  if (!slot->key || strcmp(slot->key, key) != 0) {
    if (slot->key) free(slot->key);
    slot->key = cogito_scroller_state_dup(key);
  }
  slot->scroll_x = n->scroller.scroll_x;
  slot->scroll_y = n->scroller.scroll_y;
  slot->stamp = cogito_scroller_state_tick++;
}

/* Measure the natural content height of a node given a fixed width.
 * Used by scroller to determine overflow when the child uses vexpand. */
static int cogito_scroller_measure_child_h(CogitoNode* n, int width) {
  if (!n) return 0;
  switch (n->kind) {
    case COGITO_TEXTVIEW: {
      int pad = 8; /* matches the hardcoded internal pad in textview draw */
      int avail_w = width - pad * 2;
      if (avail_w < 1) avail_w = 1;
      const char* t = n->text ? n->text->data : "";
      if (!t[0]) return pad * 2;
      int size = cogito_node_font_size(n);
      CogitoColor dummy = {0, 0, 0, 0};
      int text_h = cogito_draw_text_wrapped(n, 0, 0, avail_w, t, dummy, size,
                                            cogito_node_bold(n), false, 0, false);
      return text_h + pad * 2;
    }
    case COGITO_WEBVIEW: {
      const char* html = (n->icon && n->icon->data) ? n->icon->data : "";
      int pad_h = n->padding_left + n->padding_right;
      int pad_v = n->padding_top + n->padding_bottom;
      if (!html[0]) return pad_v;
      int avail_w = width - pad_h;
      if (avail_w < 1) avail_w = 1;
      CogitoColor dummy = {0, 0, 0, 255};
      int content_h = cogito_webview_measure_content(n, html, avail_w, dummy);
      return content_h + pad_v;
    }
    case COGITO_VSTACK: {
      int inner_w = width - n->padding_left - n->padding_right;
      if (inner_w < 0) inner_w = 0;
      int total = n->padding_top + n->padding_bottom;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* child = n->children[i];
        int slot_w = inner_w - child->margin_left - child->margin_right;
        if (slot_w < 0) slot_w = 0;
        int child_w = child->hexpand ? slot_w : slot_w;
        int ch = cogito_scroller_measure_child_h(child, child_w);
        total += ch + child->margin_top + child->margin_bottom;
        if (i > 0) total += n->gap;
      }
      return total;
    }
    case COGITO_HSTACK: {
      int inner_w = width - n->padding_left - n->padding_right;
      if (inner_w < 0) inner_w = 0;
      int max_h = 0;
      for (size_t i = 0; i < n->len; i++) {
        int ch = cogito_scroller_measure_child_h(n->children[i], inner_w);
        if (ch > max_h) max_h = ch;
      }
      return max_h + n->padding_top + n->padding_bottom;
    }
    case COGITO_ZSTACK: {
      int inner_w = width - n->padding_left - n->padding_right;
      if (inner_w < 0) inner_w = 0;
      int max_h = 0;
      for (size_t i = 0; i < n->len; i++) {
        int ch = cogito_scroller_measure_child_h(n->children[i], inner_w);
        if (ch > max_h) max_h = ch;
      }
      return max_h + n->padding_top + n->padding_bottom;
    }
    default: {
      int cw = 0, ch = 0;
      cogito_intrinsic_size(n, &cw, &ch);
      return ch;
    }
  }
}

static void cogito_layout_scroller(CogitoNode* n, int x, int y) {
  int inner_x = x + n->padding_left;
  int inner_y = y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  if (n->len == 0) return;
  CogitoNode* c = n->children[0];
  int cw = 0;
  int ch = 0;
  cogito_intrinsic_size(c, &cw, &ch);
  if (!n->scroller.horz) {
    cw = inner_w;
  } else if (cw < inner_w) {
    cw = inner_w;
  }
  /* Re-measure content height given the actual width.
   * This lets text-wrapping nodes (textview, webview) report their natural
   * content height so the scroller knows when to show the scrollbar. */
  if (n->scroller.vert) {
    int measured_h = cogito_scroller_measure_child_h(c, cw);
    if (measured_h > ch) ch = measured_h;
  }
  if (!n->scroller.vert) {
    ch = inner_h;
  } else if (ch < inner_h) {
    ch = inner_h;
  }
  n->scroller.content_w = cw;
  n->scroller.content_h = ch;
  int max_x = cw - inner_w;
  int max_y = ch - inner_h;
  if (max_x < 0) max_x = 0;
  if (max_y < 0) max_y = 0;

  CogitoNode* sync_src = n->scroller.sync_with;
  if (sync_src && sync_src != n && sync_src->kind == COGITO_SCROLLER) {
    CogitoNode* src_win = cogito_node_window(sync_src);
    CogitoNode* dst_win = cogito_node_window(n);
    if (src_win && dst_win && src_win == dst_win) {
      int src_inner_w = sync_src->w - sync_src->padding_left - sync_src->padding_right;
      int src_inner_h = sync_src->h - sync_src->padding_top - sync_src->padding_bottom;
      if (src_inner_w < 0) src_inner_w = 0;
      if (src_inner_h < 0) src_inner_h = 0;
      int src_max_x = sync_src->scroller.content_w - src_inner_w;
      int src_max_y = sync_src->scroller.content_h - src_inner_h;
      if (src_max_x < 0) src_max_x = 0;
      if (src_max_y < 0) src_max_y = 0;

      if (n->scroller.horz && sync_src->scroller.horz) {
        if (src_max_x > 0 && max_x > 0) {
          double p = (double)sync_src->scroller.scroll_x / (double)src_max_x;
          if (p < 0.0) p = 0.0;
          if (p > 1.0) p = 1.0;
          n->scroller.scroll_x = (int)lround(p * (double)max_x);
        } else {
          n->scroller.scroll_x = 0;
        }
      }
      if (n->scroller.vert && sync_src->scroller.vert) {
        if (src_max_y > 0 && max_y > 0) {
          double p = (double)sync_src->scroller.scroll_y / (double)src_max_y;
          if (p < 0.0) p = 0.0;
          if (p > 1.0) p = 1.0;
          n->scroller.scroll_y = (int)lround(p * (double)max_y);
        } else {
          n->scroller.scroll_y = 0;
        }
      }
    }
  }

  if (!n->scroller.state_restored) {
    int saved_x = 0;
    int saved_y = 0;
    if (cogito_scroller_state_load(n, &saved_x, &saved_y)) {
      n->scroller.scroll_x = saved_x;
      n->scroller.scroll_y = saved_y;
    }
    n->scroller.state_restored = true;
  }
  if (n->scroller.scroll_x < 0) n->scroller.scroll_x = 0;
  if (n->scroller.scroll_y < 0) n->scroller.scroll_y = 0;
  if (n->scroller.scroll_x > max_x) n->scroller.scroll_x = max_x;
  if (n->scroller.scroll_y > max_y) n->scroller.scroll_y = max_y;
  cogito_scroller_state_store(n);
  cogito_layout_node(c, inner_x - n->scroller.scroll_x, inner_y - n->scroller.scroll_y, cw, ch);
  for (size_t i = 1; i < n->len; i++) {
    cogito_layout_node(n->children[i], inner_x, inner_y, 0, 0);
  }
}
