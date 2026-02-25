case COGITO_TREEVIEW: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor fg = s.has_text ? s.text : n->text_color;
  CogitoColor border = s.has_border ? s.border : n->border_color;
  int border_w = s.has_border_width ? s.border_width : n->border_width;
  CogitoColor sel_bg = s.has_selection ? s.selection : n->selection_color;
  cogito_anim_apply(n, &s, &bg, &fg, &border, &sel_bg);
  int radius = s.has_radius ? s.radius : 8;
  if (n->shadow_set && n->shadow_level > 0) {
    cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
  }
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
  }

  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  cogito_treeview_inner_rect(n, &inner_x, &inner_y, &inner_w, &inner_h);
  if (inner_w <= 0 || inner_h <= 0 || n->len == 0) break;

  size_t cap = cogito_treeview_visible_row_capacity(n);
  if (cap == 0) break;
  CogitoTreeRow* rows = (CogitoTreeRow*)calloc(cap, sizeof(CogitoTreeRow));
  if (!rows) break;
  size_t row_count = cogito_treeview_collect_visible_rows(n, rows, cap);
  int row_h = 0, pad = 0, indent_step = 0, chevron_w = 0, chevron_h = 0, chevron_gap = 0;
  cogito_treeview_metrics(&row_h, &pad, &indent_step, &chevron_w, &chevron_h, &chevron_gap);
  int max_y = inner_y + inner_h;
  bool clip = cogito_backend && cogito_backend->begin_scissor && cogito_backend->end_scissor;
  if (clip) cogito_backend->begin_scissor(inner_x, inner_y, inner_w, inner_h);
  for (size_t i = 0; i < row_count; i++) {
    int row_y = inner_y + (int)i * row_h;
    if (row_y >= max_y) break;
    CogitoNode* row_node = rows[i].node;
    if (!row_node) continue;
    int depth = rows[i].depth;
    bool selected = (n->selected >= 0 && (size_t)n->selected == i);
    if (selected) {
      cogito_draw_rect(inner_x + 1, row_y, inner_w - 2, row_h, sel_bg, 0);
    }
    int indent_x = inner_x + pad + depth * indent_step;
    CogitoColor row_fg = selected ? cogito_on_color(sel_bg) : fg;
    if (row_node->len > 0) {
      int cx = indent_x + 4;
      int cy = row_y + row_h / 2;
      if (row_node->tree_collapsed) {
        cogito_draw_line(cx - 1, cy - 3, cx + 2, cy, row_fg);
        cogito_draw_line(cx - 1, cy + 3, cx + 2, cy, row_fg);
      } else {
        cogito_draw_line(cx - 3, cy - 1, cx, cy + 2, row_fg);
        cogito_draw_line(cx + 3, cy - 1, cx, cy + 2, row_fg);
      }
    }
    int label_x = indent_x + (row_node->len > 0 ? (chevron_w + chevron_gap) : 0);
    if (label_x > inner_x + inner_w - 40) label_x = inner_x + inner_w - 40;
    char label[160];
    cogito_treeview_node_label(row_node, label, sizeof(label));
    int font_size = cogito_node_font_size(row_node);
    int text_h = cogito_text_height_size(font_size);
    int text_y = row_y + (row_h - text_h) / 2;
    if (text_y < row_y) text_y = row_y;
    cogito_draw_text_size_node(row_node, label_x, text_y, label, row_fg, font_size, false);
  }
  if (clip) cogito_backend->end_scissor();
  free(rows);
  break;
}
