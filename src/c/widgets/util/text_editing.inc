// Text editing utilities
// UTF-8 caret navigation and multi-line caret movement for text fields/views.

static int cogito_utf8_prev_index(const char* text, int len, int idx) {
  if (!text || len <= 0 || idx <= 0) return 0;
  if (idx > len) idx = len;
  idx--;
  while (idx > 0 && ((unsigned char)text[idx] & 0xC0) == 0x80) idx--;
  return idx;
}

static int cogito_utf8_next_index(const char* text, int len, int idx) {
  if (!text || len <= 0) return 0;
  if (idx < 0) idx = 0;
  if (idx >= len) return len;
  idx++;
  while (idx < len && ((unsigned char)text[idx] & 0xC0) == 0x80) idx++;
  return idx;
}

static bool cogito_text_move_caret_h(CogitoNode* n, int dir) {
  if (!n) return false;
  const char* text = n->text ? n->text->data : "";
  int len = (int)(n->text ? n->text->len : 0);
  int old_caret = n->text_input.caret;
  if (old_caret < 0) old_caret = 0;
  if (old_caret > len) old_caret = len;

  if (n->text_input.sel_start != n->text_input.sel_end) {
    int a = n->text_input.sel_start;
    int b = n->text_input.sel_end;
    if (a > b) { int t = a; a = b; b = t; }
    n->text_input.caret = (dir < 0) ? a : b;
  } else {
    n->text_input.caret = (dir < 0)
      ? cogito_utf8_prev_index(text, len, old_caret)
      : cogito_utf8_next_index(text, len, old_caret);
  }

  if (n->text_input.caret < 0) n->text_input.caret = 0;
  if (n->text_input.caret > len) n->text_input.caret = len;
  n->text_input.sel_start = n->text_input.caret;
  n->text_input.sel_end = n->text_input.caret;
  n->text_input.selecting = false;
  return n->text_input.caret != old_caret;
}

static bool cogito_textview_move_caret_v(CogitoNode* n, int dir) {
  if (!n || n->kind != COGITO_TEXTVIEW || dir == 0) return false;
  const char* t = n->text ? n->text->data : "";
  int len = (int)strlen(t);
  int caret = n->text_input.caret;
  if (caret < 0) caret = 0;
  if (caret > len) caret = len;
  int old_caret = caret;

  if (n->text_input.sel_start != n->text_input.sel_end) {
    int a = n->text_input.sel_start;
    int b = n->text_input.sel_end;
    if (a > b) { int tmp = a; a = b; b = tmp; }
    caret = (dir < 0) ? a : b;
  }

  int pad = 8;
  int size = cogito_node_font_size(n);
  int max_w = n->w - pad * 2;
  if (max_w < 1) max_w = 1;

  int idx = 0;
  int prev_start = -1;
  int prev_len = 0;
  while (idx <= len) {
    int line_start = idx;
    int line_len = 0;
    int last_space = -1;
    for (int i = idx; i < len; i++) {
      char c = t[i];
      if (c == '\n') {
        line_len = i - idx;
        idx = i + 1;
        break;
      }
      int w = cogito_text_width_size_n_node(n, t + idx, i - idx + 1, size);
      if (c == ' ') last_space = i;
      if (w > max_w) {
        if (last_space >= idx) {
          line_len = last_space - idx;
          idx = last_space + 1;
        } else {
          line_len = i - idx;
          idx = i;
        }
        break;
      }
      if (i == len - 1) {
        line_len = i - idx + 1;
        idx = len;
      }
    }

    int line_end = line_start + line_len;
    if (caret >= line_start && caret <= line_end) {
      int local = caret - line_start;
      int target_x = cogito_text_width_size_n_node(n, t + line_start, local, size);

      if (dir < 0) {
        if (prev_start < 0) {
          n->text_input.caret = line_start;
        } else {
          int prev_local = cogito_text_index_from_x(n, t + prev_start, size, target_x);
          if (prev_local > prev_len) prev_local = prev_len;
          n->text_input.caret = prev_start + prev_local;
        }
      } else {
        int next_start = idx;
        int next_len = 0;
        if (next_start >= len) {
          if (len > 0 && t[len - 1] == '\n') {
            n->text_input.caret = len;
          } else {
            n->text_input.caret = line_end;
          }
        } else {
          int next_last_space = -1;
          for (int i = next_start; i < len; i++) {
            char c = t[i];
            if (c == '\n') {
              next_len = i - next_start;
              break;
            }
            int w = cogito_text_width_size_n_node(n, t + next_start, i - next_start + 1, size);
            if (c == ' ') next_last_space = i;
            if (w > max_w) {
              if (next_last_space >= next_start) {
                next_len = next_last_space - next_start;
              } else {
                next_len = i - next_start;
              }
              break;
            }
            if (i == len - 1) {
              next_len = i - next_start + 1;
            }
          }
          int next_local = cogito_text_index_from_x(n, t + next_start, size, target_x);
          if (next_local > next_len) next_local = next_len;
          n->text_input.caret = next_start + next_local;
        }
      }
      break;
    }

    prev_start = line_start;
    prev_len = line_len;
    if (idx >= len) {
      if (caret == len && len > 0 && t[len - 1] == '\n') {
        if (dir < 0) {
          int target_x = 0;
          if (prev_start >= 0) {
            int prev_local = cogito_text_index_from_x(n, t + prev_start, size, target_x);
            if (prev_local > prev_len) prev_local = prev_len;
            n->text_input.caret = prev_start + prev_local;
          } else {
            n->text_input.caret = 0;
          }
        } else {
          n->text_input.caret = len;
        }
      }
      break;
    }
  }

  if (n->text_input.caret < 0) n->text_input.caret = 0;
  if (n->text_input.caret > len) n->text_input.caret = len;
  n->text_input.sel_start = n->text_input.caret;
  n->text_input.sel_end = n->text_input.caret;
  n->text_input.selecting = false;
  return n->text_input.caret != old_caret;
}
