static void cogito_layout_grid(CogitoNode* n, int x, int y) {
  int cols = n->grid.cols > 0 ? n->grid.cols : 1;
  int gap_x = n->grid.gap_x;
  int gap_y = n->grid.gap_y;
  int inner_w = n->w - n->padding_left - n->padding_right;
  if (inner_w < 0) inner_w = 0;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_h < 0) inner_h = 0;
  size_t count = n->len;
  if (count == 0) return;
  int row_count = 0;
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    if (!c || c->hidden) continue;
    int col = c->grid_pos_set ? c->grid_col : 0;
    int row = c->grid_pos_set ? c->grid_row : 0;
    int cs = c->grid_span_x > 0 ? c->grid_span_x : 1;
    int rs = c->grid_span_y > 0 ? c->grid_span_y : 1;
    if (col < 0) col = 0;
    if (row < 0) row = 0;
    if (col >= cols) col = cols - 1;
    if (cs > cols) cs = cols;
    if (col + cs > cols) cs = cols - col;
    if (cs < 1) cs = 1;
    int end_row = row + rs;
    if (end_row > row_count) row_count = end_row;
  }
  if (row_count < 1) {
    for (size_t i = 0; i < count; i++) {
      CogitoNode* c = n->children[i];
      if (!c || c->hidden) continue;
      cogito_layout_node(c, x, y, 0, 0);
    }
    return;
  }

  // Calculate column minimum widths and expand flags
  int *col_min_w = (int*)calloc((size_t)cols, sizeof(int));
  bool *col_expand = (bool*)calloc((size_t)cols, sizeof(bool));
  if (!col_min_w || !col_expand) {
    free(col_min_w);
    free(col_expand);
    return;
  }
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    if (!c || c->hidden) continue;
    int cw = 0;
    cogito_intrinsic_size(c, &cw, NULL);
    int col = c->grid_pos_set ? c->grid_col : 0;
    int cs = c->grid_span_x > 0 ? c->grid_span_x : 1;
    if (col < 0) col = 0;
    if (col >= cols) col = cols - 1;
    if (cs > cols) cs = cols;
    if (col + cs > cols) cs = cols - col;
    if (cs < 1) cs = 1;
    int per_col = cs > 0 ? (cw + cs - 1) / cs : cw;
    for (int cc = 0; cc < cs; cc++) {
      int col_idx = col + cc;
      if (col_idx >= cols) break;
      if (per_col > col_min_w[col_idx]) col_min_w[col_idx] = per_col;
      if (cogito_layout_effective_hexpand(c)) col_expand[col_idx] = true;
    }
  }
  // Distribute column widths
  int total_gap_w = cols > 0 ? (cols - 1) * gap_x : 0;
  int avail_col_w = inner_w - total_gap_w;
  if (avail_col_w < 0) avail_col_w = 0;
  int* col_widths = (int*)calloc((size_t)cols, sizeof(int));
  if (col_widths) {
    cogito_distribute_axis_sizes((size_t)cols, col_min_w, col_expand, avail_col_w, col_widths);
  } else {
    free(col_min_w);
    free(col_expand);
    return;
  }

  // Calculate row heights and expand flags
  int *row_heights = (int*)calloc((size_t)row_count, sizeof(int));
  bool *row_expand = (bool*)calloc((size_t)row_count, sizeof(bool));
  if (!row_heights || !row_expand) {
    free(col_min_w);
    free(col_expand);
    free(col_widths);
    free(row_heights);
    free(row_expand);
    return;
  }
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    if (!c || c->hidden) continue;
    int ch = 0;
    cogito_intrinsic_size(c, NULL, &ch);
    int row = c->grid_pos_set ? c->grid_row : 0;
    int rs = c->grid_span_y > 0 ? c->grid_span_y : 1;
    if (row < 0) row = 0;
    if (row >= row_count) row = row_count - 1;
    if (row + rs > row_count) rs = row_count - row;
    if (rs < 1) rs = 1;
    int per_row = rs > 0 ? (ch + rs - 1) / rs : ch;
    for (int rr = 0; rr < rs; rr++) {
      int row_idx = row + rr;
      if (row_idx >= row_count) break;
      if (per_row > row_heights[row_idx]) row_heights[row_idx] = per_row;
      if (cogito_layout_effective_vexpand(c)) row_expand[row_idx] = true;
    }
  }
  if (row_expand) {
    bool any_expand = false;
    for (int r = 0; r < row_count; r++) {
      if (row_expand[r]) {
        any_expand = true;
        break;
      }
    }
    // If grid itself has vexpand true but no children do, treat all rows as expandable
    // Zero out row heights so space is distributed equally (like 1fr), not biased by content
    if (!any_expand && n->vexpand) {
      any_expand = true;
      for (int r = 0; r < row_count; r++) {
        row_expand[r] = true;
        row_heights[r] = 0;
      }
    }
    if (any_expand) {
      int total_gap_h = row_count > 0 ? (row_count - 1) * gap_y : 0;
      int avail_row_h = inner_h - total_gap_h;
      if (avail_row_h < 0) avail_row_h = 0;
      int* row_out = (int*)calloc((size_t)row_count, sizeof(int));
      if (row_out) {
        cogito_distribute_axis_sizes((size_t)row_count, row_heights, row_expand, avail_row_h, row_out);
        for (int r = 0; r < row_count; r++) row_heights[r] = row_out[r];
        free(row_out);
      }
    }
  }

  // Similarly for horizontal expand
  if (col_expand) {
    bool any_hexpand = false;
    for (int c = 0; c < cols; c++) {
      if (col_expand[c]) {
        any_hexpand = true;
        break;
      }
    }
    // If grid itself has hexpand true but no children do, treat all columns as expandable
    // Zero out col minimums so space is distributed equally (like 1fr), not biased by content
    if (!any_hexpand && n->hexpand) {
      any_hexpand = true;
      for (int c = 0; c < cols; c++) {
        col_expand[c] = true;
        col_min_w[c] = 0;
      }
    }
    if (any_hexpand) {
      int total_gap_w = cols > 0 ? (cols - 1) * gap_x : 0;
      int avail_col_w = inner_w - total_gap_w;
      if (avail_col_w < 0) avail_col_w = 0;
      int* col_out = (int*)calloc((size_t)cols, sizeof(int));
      if (col_out) {
        cogito_distribute_axis_sizes((size_t)cols, col_min_w, col_expand, avail_col_w, col_out);
        for (int c = 0; c < cols; c++) col_widths[c] = col_out[c];
        free(col_out);
      }
    }
  }

  int *row_y = (int*)calloc((size_t)row_count, sizeof(int));
  if (!row_y) {
    free(col_min_w);
    free(col_expand);
    free(col_widths);
    free(row_heights);
    free(row_expand);
    return;
  }
  int cy = y + n->padding_top;
  for (int r = 0; r < row_count; r++) {
    row_y[r] = cy;
    cy += row_heights[r] + gap_y;
  }

  int base_x = x + n->padding_left;
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    if (!c || c->hidden) continue;
    int r = c->grid_pos_set ? c->grid_row : 0;
    int ccol = c->grid_pos_set ? c->grid_col : 0;
    int cs = c->grid_span_x > 0 ? c->grid_span_x : 1;
    int rs = c->grid_span_y > 0 ? c->grid_span_y : 1;
    if (ccol < 0) ccol = 0;
    if (r < 0) r = 0;
    if (ccol >= cols) ccol = cols - 1;
    if (r >= row_count) r = row_count - 1;
    if (cs > cols) cs = cols;
    if (ccol + cs > cols) cs = cols - ccol;
    if (cs < 1) cs = 1;
    if (r + rs > row_count) rs = row_count - r;
    if (rs < 1) rs = 1;

    int cell_x = base_x;
    for (int cc = 0; cc < ccol; cc++) {
      cell_x += col_widths[cc] + gap_x;
    }
    int cell_y = row_y[r];
    int cell_w_span = 0;
    for (int cc = 0; cc < cs; cc++) {
      int col_idx = ccol + cc;
      if (col_idx >= cols) break;
      cell_w_span += col_widths[col_idx];
      if (cc + 1 < cs) cell_w_span += gap_x;
    }
    int cell_h_span = 0;
    for (int rr = 0; rr < rs; rr++) {
      int row_idx = r + rr;
      if (row_idx >= row_count) break;
      cell_h_span += row_heights[row_idx];
      if (rr + 1 < rs) cell_h_span += gap_y;
    }
    int cw = cell_w_span - c->margin_left - c->margin_right;
    int ch = cell_h_span - c->margin_top - c->margin_bottom;
    if (cw < 0) cw = 0;
    if (ch < 0) ch = 0;

    // Keep children inside their computed cell even when their min-size constraints are larger.
    bool prev_max_w_set = c->max_w_set;
    bool prev_max_h_set = c->max_h_set;
    int prev_max_w = c->max_w;
    int prev_max_h = c->max_h;
    bool forced_max_w = false;
    bool forced_max_h = false;
    if (cw > 0 && (!c->max_w_set || c->max_w <= 0 || c->max_w > cw)) {
      c->max_w_set = true;
      c->max_w = cw;
      forced_max_w = true;
    }
    if (ch > 0 && (!c->max_h_set || c->max_h <= 0 || c->max_h > ch)) {
      c->max_h_set = true;
      c->max_h = ch;
      forced_max_h = true;
    }

    cogito_layout_node(c, cell_x + c->margin_left, cell_y + c->margin_top, cw, ch);

    if (forced_max_w) {
      c->max_w_set = prev_max_w_set;
      c->max_w = prev_max_w;
    }
    if (forced_max_h) {
      c->max_h_set = prev_max_h_set;
      c->max_h = prev_max_h;
    }
  }

  free(col_min_w);
  free(col_expand);
  free(col_widths);
  free(row_heights);
  free(row_y);
  free(row_expand);
}
