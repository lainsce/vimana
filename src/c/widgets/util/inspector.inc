static int cogito_inspector_hit_test(CogitoWindow* win, int x, int y, void* user) {
  (void)win;
  enum { COGITO_WINDOW_DRAG_HIT_PAD = 8 };
  const int* params = (const int*)user;
  int appbar_h = (params && params[0] > 0) ? params[0] : 52;
  int margin = (params && params[1] > 0) ? params[1] : 0;
  int btn = (params && params[2] > 0) ? params[2] : 14;
  int gap = (params && params[3] > 0) ? params[3] : 9;
  int pad_x = (params && params[4] > 0) ? params[4] : 9;
  int pad_top = (params && params[5] >= 0) ? params[5] : 9;
  int by = margin + pad_top;
  if (by + btn > margin + appbar_h) by = margin + (appbar_h - btn) / 2;
  int close_x = margin + pad_x;
  int min_x = close_x + btn + gap;
  int max_x = min_x + btn + gap;
  if (cogito_hit_rect(x, y, close_x, by, btn, btn) ||
      cogito_hit_rect(x, y, min_x, by, btn, btn) ||
      cogito_hit_rect(x, y, max_x, by, btn, btn)) {
    return 0; // normal, so clicks on traffic lights are delivered to UI handlers.
  }
  // Cogito hit-test enum values: 0 = normal, 1 = draggable.
  return (y >= (margin - COGITO_WINDOW_DRAG_HIT_PAD) &&
          y < (margin + appbar_h + COGITO_WINDOW_DRAG_HIT_PAD) &&
          x >= (margin - COGITO_WINDOW_DRAG_HIT_PAD)) ? 1 : 0;
}

static void cogito_inspector_color_hex(CogitoColor c, char* out, size_t cap) {
  if (!out || cap == 0) return;
  if (c.a == 255) snprintf(out, cap, "#%02X%02X%02X", (unsigned int)c.r, (unsigned int)c.g, (unsigned int)c.b);
  else snprintf(out, cap, "#%02X%02X%02X%02X", (unsigned int)c.r, (unsigned int)c.g, (unsigned int)c.b, (unsigned int)c.a);
}

static void cogito_inspector_panel_rect(CogitoNode* win, int* out_x, int* out_y, int* out_w, int* out_h, int* out_tree_w) {
  if (!win) return;
  int view_x = cogito_inspector_view_x;
  int view_y = cogito_inspector_view_y;
  int view_w = cogito_inspector_view_w > 0 ? cogito_inspector_view_w : win->w;
  int view_h = cogito_inspector_view_h > 0 ? cogito_inspector_view_h : win->h;
  if (view_w < 0) view_w = 0;
  if (view_h < 0) view_h = 0;
  int panel_w = view_w / 2;
  if (panel_w < 320) panel_w = view_w;
  if (panel_w > 520) panel_w = 520;
  int x = view_x + view_w - panel_w;
  if (x < 0) x = 0;
  int y = view_y;
  int h = view_h > COGITO_INSPECTOR_STATUS_H ? view_h - COGITO_INSPECTOR_STATUS_H : view_h;
  int tree_w = (panel_w * 45) / 100;
  if (tree_w < 170) tree_w = panel_w / 2;
  if (tree_w > panel_w - 120) tree_w = panel_w - 120;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = panel_w;
  if (out_h) *out_h = h;
  if (out_tree_w) *out_tree_w = tree_w;
}

static void cogito_inspector_tree_metrics(int* out_header_h, int* out_row_h, int* out_pad) {
  if (out_header_h) *out_header_h = 32;
  if (out_row_h) *out_row_h = 22;
  if (out_pad) *out_pad = 6;
}

static bool cogito_inspector_is_collapsed(CogitoNode* n) {
  if (!n) return false;
  for (int i = 0; i < cogito_inspector_collapsed_count; i++) {
    if (cogito_inspector_collapsed_nodes[i] == n) {
      return cogito_inspector_collapsed_values[i];
    }
  }
  return false;
}

static void cogito_inspector_set_collapsed(CogitoNode* n, bool collapsed) {
  if (!n) return;
  for (int i = 0; i < cogito_inspector_collapsed_count; i++) {
    if (cogito_inspector_collapsed_nodes[i] == n) {
      if (collapsed) {
        cogito_inspector_collapsed_values[i] = true;
      } else {
        for (int j = i; j + 1 < cogito_inspector_collapsed_count; j++) {
          cogito_inspector_collapsed_nodes[j] = cogito_inspector_collapsed_nodes[j + 1];
          cogito_inspector_collapsed_values[j] = cogito_inspector_collapsed_values[j + 1];
        }
        cogito_inspector_collapsed_count--;
      }
      return;
    }
  }
  if (collapsed && cogito_inspector_collapsed_count < COGITO_INSPECTOR_MAX_NODES) {
    cogito_inspector_collapsed_nodes[cogito_inspector_collapsed_count] = n;
    cogito_inspector_collapsed_values[cogito_inspector_collapsed_count] = true;
    cogito_inspector_collapsed_count++;
  }
}

static void cogito_inspector_collect_nodes(CogitoNode* n, int depth) {
  if (!n) return;
  if (cogito_inspector_node_count >= COGITO_INSPECTOR_MAX_NODES) return;
  cogito_inspector_nodes[cogito_inspector_node_count] = n;
  cogito_inspector_depths[cogito_inspector_node_count] = depth;
  cogito_inspector_node_count++;
  if (n->len == 0 || cogito_inspector_is_collapsed(n)) return;
  for (size_t i = 0; i < n->len; i++) {
    cogito_inspector_collect_nodes(n->children[i], depth + 1);
  }
}

static void cogito_inspector_rebuild_tree(CogitoNode* win) {
  cogito_inspector_node_count = 0;
  cogito_inspector_collect_nodes(win, 0);
}

static void cogito_inspector_node_label(CogitoNode* n, char* out, size_t cap) {
  if (!out || cap == 0) return;
  if (!n) {
    snprintf(out, cap, "<null>");
    return;
  }
  const char* kind = cogito_kind_name(n->kind);
  const char* cls = (n->class_name && n->class_name->data && n->class_name->data[0]) ? n->class_name->data : NULL;
  if (cls) snprintf(out, cap, "%s.%s", kind, cls);
  else snprintf(out, cap, "%s", kind);
}

static CogitoNode* cogito_inspector_pick_node_recursive(CogitoNode* n, int x, int y) {
  if (!n || !cogito_hit_node(n, x, y)) return NULL;
  for (size_t i = n->len; i > 0; i--) {
    CogitoNode* hit = cogito_inspector_pick_node_recursive(n->children[i - 1], x, y);
    if (hit) return hit;
  }
  return n;
}

/* Returns true if wheel was consumed (scrolled widget tree). local_x, local_y are in inspector content coords. */
static bool cogito_inspector_handle_wheel(CogitoNode* win, int local_x, int local_y, float wheel_delta) {
  if (!cogito_inspector_open || !win || wheel_delta == 0.0f) return false;
  int px = 0, py = 0, pw = 0, ph = 0, tree_w = 0;
  cogito_inspector_panel_rect(win, &px, &py, &pw, &ph, &tree_w);
  int header_h = 0;
  int row_h = 0;
  int pad = 0;
  cogito_inspector_tree_metrics(&header_h, &row_h, &pad);
  int list_y = py + header_h + 4;
  int list_h = ph - (header_h + 8);
  if (list_h < 0) list_h = 0;
  if (!cogito_hit_rect(local_x, local_y, px, list_y, tree_w, list_h)) return false;
  cogito_inspector_rebuild_tree(win);
  int total_tree_h = cogito_inspector_node_count * row_h;
  int max_scroll = total_tree_h > list_h ? total_tree_h - list_h : 0;
  int delta = (int)(wheel_delta * 32.0f);
  cogito_inspector_tree_scroll_y -= delta;
  if (cogito_inspector_tree_scroll_y > max_scroll) cogito_inspector_tree_scroll_y = max_scroll;
  if (cogito_inspector_tree_scroll_y < 0) cogito_inspector_tree_scroll_y = 0;
  return true;
}

static bool cogito_inspector_handle_click(CogitoNode* win, int x, int y) {
  if (!cogito_inspector_open || !win) return false;
  int px = 0, py = 0, pw = 0, ph = 0, tree_w = 0;
  cogito_inspector_panel_rect(win, &px, &py, &pw, &ph, &tree_w);
  if (!cogito_hit_rect(x, y, px, py, pw, ph)) return false;

  cogito_inspector_rebuild_tree(win);
  int header_h = 0;
  int row_h = 0;
  int pad = 0;
  cogito_inspector_tree_metrics(&header_h, &row_h, &pad);
  int list_y = py + header_h + 4;
  int list_h = ph - (header_h + 8);
  if (list_h < 0) list_h = 0;
  int total_tree_h = cogito_inspector_node_count * row_h;
  int max_scroll = total_tree_h > list_h ? total_tree_h - list_h : 0;
  if (cogito_inspector_tree_scroll_y > max_scroll) cogito_inspector_tree_scroll_y = max_scroll;
  if (cogito_inspector_tree_scroll_y < 0) cogito_inspector_tree_scroll_y = 0;
  if (cogito_hit_rect(x, y, px, list_y, tree_w, list_h)) {
    int row = (y - list_y + cogito_inspector_tree_scroll_y) / row_h;
    if (row >= 0 && row < cogito_inspector_node_count) {
      CogitoNode* n = cogito_inspector_nodes[row];
      int depth = cogito_inspector_depths[row];
      int row_y = list_y + row * row_h;
      int indent_x = px + pad + depth * 12;
      int chevron_x = indent_x;
      int chevron_y = row_y + (row_h - 12) / 2;
      int chevron_w = 10;
      int chevron_h = 12;
      if (n && n->len > 0 && cogito_hit_rect(x, y, chevron_x, chevron_y, chevron_w, chevron_h)) {
        cogito_inspector_set_collapsed(n, !cogito_inspector_is_collapsed(n));
      } else {
        cogito_inspector_selected = n;
      }
    }
  }
  return true;
}

static void cogito_draw_inspector_highlight(void) {
  if (!cogito_inspector_open || !cogito_inspector_selected) return;
  CogitoNode* n = cogito_inspector_selected;
  if (n->w <= 0 || n->h <= 0) return;
  CogitoColor red = cogito_rgba(220, 44, 44, 255);
  cogito_draw_rect_lines(n->x - 1, n->y - 1, n->w + 2, n->h + 2, red, n->border_radius, 2);
}

static int cogito_inspector_draw_prop_kv_row(CogitoNode* win, int x, int y, int w,
                                             const char* key, const char* value,
                                             CogitoColor key_color, CogitoColor value_color,
                                             CogitoColor row_bg, CogitoColor row_border,
                                             int text_size, int row_pad) {
  if (!win || !key || !value || w <= 0) return y;
  int row_h = cogito_text_height_size(text_size) + row_pad * 2;
  if (row_h < 16) row_h = 16;
  cogito_draw_rect(x, y, w, row_h, row_bg, 4);
  cogito_draw_rect_lines(x, y, w, row_h, row_border, 4, 1);

  int key_x = x + row_pad;
  int key_w = cogito_text_width_size_node(win, key, text_size);
  int value_w = cogito_text_width_size_node(win, value, text_size);
  int value_x = x + w - row_pad - value_w;
  int min_value_x = key_x + key_w + 10;
  if (value_x < min_value_x) value_x = min_value_x;

  cogito_draw_text_size_node(win, key_x, y + row_pad, key, key_color, text_size, false);
  cogito_draw_text_size_node(win, value_x, y + row_pad, value, value_color, text_size, false);
  return y + row_h + 4;
}

static int cogito_inspector_draw_geometry_state_widget(CogitoNode* win, int x, int y, int w,
                                                       int geom_w, int geom_h,
                                                       bool hover, bool active, bool focused, bool disabled,
                                                       CogitoColor text, CogitoColor muted) {
  if (!win || w <= 0) return y;
  const int label_size = 12;
  const int label_h = cogito_text_height_size(label_size);
  if (geom_w < 1) geom_w = 1;
  if (geom_h < 1) geom_h = 1;

  bool representation = (geom_w > 300 || geom_h > 300);
  int box_w = geom_w;
  int box_h = geom_h;
  if (representation) {
    float sx = 140.0f / (float)geom_w;
    float sy = 140.0f / (float)geom_h;
    float s = sx < sy ? sx : sy;
    if (s > 1.0f) s = 1.0f;
    box_w = (int)lroundf((float)geom_w * s);
    box_h = (int)lroundf((float)geom_h * s);
    if (box_w < 1) box_w = 1;
    if (box_h < 1) box_h = 1;
  }

  int canvas_pad = 8;
  int canvas_x = x + canvas_pad;
  int canvas_w = w - canvas_pad * 2;
  if (canvas_w < 16) canvas_w = 16;
  int box_x = x + (w - box_w) / 2;
  int top_y = y;
  int box_y = top_y + label_h + 4;

  char val[32];
  snprintf(val, sizeof(val), "w: %d%s", geom_w, representation ? " (repr)" : "");
  int tw = cogito_text_width_size_node(win, val, label_size);
  int wx = box_x + (box_w - tw) / 2;
  if (wx < canvas_x) wx = canvas_x;
  if (wx + tw > canvas_x + canvas_w) wx = canvas_x + canvas_w - tw;
  cogito_draw_text_size_node(win, wx, top_y, val, muted, label_size, false);

  CogitoColor box_bg = cogito_rgba(46, 204, 113, 50);
  CogitoColor box_border = cogito_rgba(22, 120, 66, 210);
  if (cogito_backend && cogito_backend->begin_scissor && cogito_backend->end_scissor) {
    cogito_backend->begin_scissor(canvas_x, box_y, canvas_w, box_h);
    cogito_draw_rect(box_x, box_y, box_w, box_h, box_bg, 0);
    cogito_draw_rect_lines(box_x, box_y, box_w, box_h, box_border, 0, 2);
    cogito_backend->end_scissor();
  } else {
    cogito_draw_rect(box_x, box_y, box_w, box_h, box_bg, 0);
    cogito_draw_rect_lines(box_x, box_y, box_w, box_h, box_border, 0, 2);
  }

  snprintf(val, sizeof(val), "h: %d", geom_h);
  int vh = cogito_text_height_size(label_size);
  int hw = cogito_text_width_size_node(win, val, label_size);
  int hx = x + w - hw - 2;
  int hy = box_y + (box_h - vh) / 2;
  cogito_draw_text_size_node(win, hx, hy, val, muted, label_size, false);

  int sy = box_y + box_h + 8;
  bool any = false;
  const char* states[4] = {"hover", "active", "focused", "disabled"};
  bool flags[4] = {hover, active, focused, disabled};
  for (int i = 0; i < 4; i++) {
    if (!flags[i]) continue;
    any = true;
    int sw = cogito_text_width_size_node(win, states[i], label_size);
    cogito_draw_text_size_node(win, x + (w - sw) / 2, sy, states[i], text, label_size, true);
    sy += label_h + 2;
  }
  if (!any) {
    const char* none = "no active state";
    int sw = cogito_text_width_size_node(win, none, label_size);
    cogito_draw_text_size_node(win, x + (w - sw) / 2, sy, none, muted, label_size, false);
    sy += label_h + 2;
  }
  return sy + 2;
}

static void cogito_draw_inspector_panel(CogitoNode* win) {
  if (!cogito_inspector_open || !win) return;
  cogito_inspector_rebuild_tree(win);
  if (!cogito_inspector_selected) cogito_inspector_selected = win;

  int px = 0, py = 0, pw = 0, ph = 0, tree_w = 0;
  cogito_inspector_panel_rect(win, &px, &py, &pw, &ph, &tree_w);
  int header_h = 0;
  int row_h = 0;
  int pad = 0;
  cogito_inspector_tree_metrics(&header_h, &row_h, &pad);
  const int text_size = 13;

  CogitoColor bg = cogito_rgba(18, 21, 26, 255);
  CogitoColor panel = cogito_rgba(23, 26, 32, 255);
  CogitoColor border = cogito_rgba(75, 82, 95, 255);
  CogitoColor text = cogito_rgba(220, 226, 235, 255);
  CogitoColor muted = cogito_rgba(160, 170, 184, 255);
  CogitoColor selected_bg = cogito_rgba(45, 74, 124, 255);
  CogitoColor prop_row_bg = cogito_rgba(28, 33, 40, 255);
  CogitoColor prop_row_bg_alt = cogito_rgba(32, 37, 46, 255);
  CogitoColor prop_row_border = cogito_rgba(75, 82, 95, 128);

  int status_y = py + ph;
  int status_h = COGITO_INSPECTOR_STATUS_H;
  cogito_draw_rect(px, py, pw, ph + status_h, bg, 0);
  cogito_draw_rect(px, py, tree_w, ph + status_h, panel, 0);
  cogito_draw_line(px + tree_w, py, px + tree_w, py + ph + status_h, border);
  cogito_draw_line(px, py + header_h, px + pw, py + header_h, border);
  cogito_draw_rect(px, status_y, pw, status_h, prop_row_bg, 0);
  cogito_draw_line(px, status_y, px + pw, status_y, border);
  cogito_draw_text_size_node(win, px + pad, status_y + (status_h - cogito_text_height_size(12)) / 2, COGITO_USER_VERSION, muted, 12, false);

  cogito_draw_text_size_node(win, px + pad, py + header_h - 16, "Widget Tree", muted, 13, false);
  cogito_draw_text_size_node(win, px + tree_w + pad, py + header_h - 16, "Selected Node", muted, 13, false);

  int list_y = py + header_h + 4;
  int list_h = ph - (header_h + 8);
  if (list_h < 0) list_h = 0;
  int visible_rows = list_h / row_h + 2;
  if (visible_rows < 0) visible_rows = 0;
  int total_tree_h = cogito_inspector_node_count * row_h;
  int max_scroll = total_tree_h > list_h ? total_tree_h - list_h : 0;
  if (cogito_inspector_tree_scroll_y > max_scroll) cogito_inspector_tree_scroll_y = max_scroll;
  if (cogito_inspector_tree_scroll_y < 0) cogito_inspector_tree_scroll_y = 0;
  int first_row = cogito_inspector_tree_scroll_y / row_h;
  int y_offset = cogito_inspector_tree_scroll_y % row_h;
  for (int i = 0; i < visible_rows; i++) {
    int row_index = first_row + i;
    if (row_index >= cogito_inspector_node_count) break;
    int row_y = list_y - y_offset + i * row_h;
    if (row_y + row_h < list_y || row_y >= list_y + list_h) continue;
    CogitoNode* n = cogito_inspector_nodes[row_index];
    int depth = cogito_inspector_depths[row_index];
    if (n == cogito_inspector_selected) {
      cogito_draw_rect(px + 1, row_y, tree_w - 2, row_h, selected_bg, 0);
    }
    int indent_x = px + pad + depth * 12;
    int label_x = indent_x;
    if (n && n->len > 0) {
      bool collapsed = cogito_inspector_is_collapsed(n);
      int cx = indent_x + 4;
      int cy = row_y + row_h / 2;
      if (collapsed) {
        cogito_draw_line(cx - 1, cy - 3, cx + 2, cy, muted);
        cogito_draw_line(cx - 1, cy + 3, cx + 2, cy, muted);
      } else {
        cogito_draw_line(cx - 3, cy - 1, cx, cy + 2, muted);
        cogito_draw_line(cx + 3, cy - 1, cx, cy + 2, muted);
      }
      label_x = indent_x + 12;
    }
    char label[128];
    cogito_inspector_node_label(n, label, sizeof(label));
    if (label_x > px + tree_w - 60) label_x = px + tree_w - 60;
    cogito_draw_text_size_node(win, label_x, row_y + 2, label, text, text_size, false);
  }
  /* Vertical scrollbar for widget tree */
  if (max_scroll > 0 && list_h > 0) {
    int sb_x = px + tree_w - 8;
    int sb_w = 6;
    int thumb_h = list_h * list_h / total_tree_h;
    if (thumb_h < 20) thumb_h = 20;
    if (thumb_h > list_h) thumb_h = list_h;
    int thumb_y = list_y + (int)((long)(list_h - thumb_h) * (long)cogito_inspector_tree_scroll_y / (long)(max_scroll > 0 ? max_scroll : 1));
    if (thumb_y < list_y) thumb_y = list_y;
    if (thumb_y + thumb_h > list_y + list_h) thumb_y = list_y + list_h - thumb_h;
    cogito_draw_rect(sb_x, list_y, sb_w, list_h, prop_row_bg, 2);
    cogito_draw_rect(sb_x, thumb_y, sb_w, thumb_h, border, 2);
  }

  CogitoNode* s = cogito_inspector_selected;
  if (!s) return;
  int rx = px + tree_w + pad;
  int rw = pw - tree_w - pad * 2;
  if (rw < 40) rw = 40;
  if (cogito_backend && cogito_backend->begin_scissor) {
    cogito_backend->begin_scissor(px + tree_w, py, rw + pad, ph);
  }
  int ry = py + header_h + 6;
  const int row_pad = 6;
  bool row_alt = false;

  char line[192];
  char value[192];
  char cbuf1[20], cbuf2[20], cbuf3[20], cbuf4[20];

  cogito_inspector_node_label(s, line, sizeof(line));
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "node", line, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, 13, row_pad
  );
  row_alt = !row_alt;

  bool hover = false, active = false;
  if (!s->disabled && cogito_backend_ready && cogito_backend) {
    cogito_node_hover_state(s, &hover, &active);
  }
  bool focused = (s == cogito_focused);
  bool disabled = s->disabled;

  cogito_inspector_color_hex(s->bg, cbuf1, sizeof(cbuf1));
  cogito_inspector_color_hex(s->text_color, cbuf2, sizeof(cbuf2));
  cogito_inspector_color_hex(s->border_color, cbuf3, sizeof(cbuf3));
  cogito_inspector_color_hex(s->selection_color, cbuf4, sizeof(cbuf4));

  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "background", cbuf1, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "color", cbuf2, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "border-color", cbuf3, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "selection-color", cbuf4, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  snprintf(value, sizeof(value), "%d", s->border_width);
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "border-width", value, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  snprintf(value, sizeof(value), "%d", s->border_radius);
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "border-radius", value, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  snprintf(value, sizeof(value), "%d %d %d %d", s->padding_top, s->padding_right, s->padding_bottom, s->padding_left);
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "padding", value, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  snprintf(value, sizeof(value), "%d %d %d %d", s->margin_top, s->margin_right, s->margin_bottom, s->margin_left);
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "margin", value, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  snprintf(value, sizeof(value), "%.2f", s->opacity);
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "opacity", value, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );

  ry += 6;
  (void)cogito_inspector_draw_geometry_state_widget(
    win, rx, ry, rw,
    s->w, s->h,
    hover, active, focused, disabled,
    text, muted
  );
  if (cogito_backend && cogito_backend->end_scissor) {
    cogito_backend->end_scissor();
  }
}
