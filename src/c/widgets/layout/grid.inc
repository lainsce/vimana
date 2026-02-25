static void cogito_layout_grid(CogitoNode* n, int x, int y) {
  int cols = n->grid.cols > 0 ? n->grid.cols : 1;
  int gap_x = n->grid.gap_x;
  int gap_y = n->grid.gap_y;
  int inner_w = n->w - n->padding_left - n->padding_right;
  if (inner_w < 0) inner_w = 0;
  int cell_w = cols > 0 ? (inner_w - (cols - 1) * gap_x) / cols : 0;
  if (cell_w < 0) cell_w = 0;
  size_t count = n->len;
  if (count == 0) return;
  int rows_stack[64];
  int cols_out_stack[64];
  int span_x_stack[64];
  int span_y_stack[64];
  int* rows = NULL;
  int* cols_out = NULL;
  int* span_x = NULL;
  int* span_y = NULL;
  void* slot_heap = NULL;
  if (count <= 64) {
    rows = rows_stack;
    cols_out = cols_out_stack;
    span_x = span_x_stack;
    span_y = span_y_stack;
  } else {
    size_t rows_bytes = sizeof(int) * count;
    size_t cols_bytes = sizeof(int) * count;
    size_t sx_bytes = sizeof(int) * count;
    size_t sy_bytes = sizeof(int) * count;
    size_t total_bytes = rows_bytes + cols_bytes + sx_bytes + sy_bytes;
    slot_heap = calloc(1, total_bytes);
    if (!slot_heap) return;
    char* p = (char*)slot_heap;
    rows = (int*)p;
    p += rows_bytes;
    cols_out = (int*)p;
    p += cols_bytes;
    span_x = (int*)p;
    p += sx_bytes;
    span_y = (int*)p;
  }
  int row_cap = 4;
  int row_count = 0;
  bool *occ = (bool*)calloc((size_t)row_cap * (size_t)cols, sizeof(bool));
  if (!occ) {
    if (slot_heap) free(slot_heap);
    return;
  }
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int cs = c->grid_span_x > 0 ? c->grid_span_x : 1;
    int rs = c->grid_span_y > 0 ? c->grid_span_y : 1;
    if (cs > cols) cs = cols;
    int r = 0;
    int ccol = 0;
    bool placed = false;
    while (!placed) {
      if (r >= row_cap) {
        int new_cap = row_cap * 2;
        bool *next = (bool*)realloc(occ, (size_t)new_cap * (size_t)cols * sizeof(bool));
        if (!next) break;
        memset(next + (size_t)row_cap * (size_t)cols, 0, (size_t)(new_cap - row_cap) * (size_t)cols * sizeof(bool));
        occ = next;
        row_cap = new_cap;
      }
      for (ccol = 0; ccol + cs <= cols; ccol++) {
        bool ok = true;
        for (int rr = 0; rr < rs; rr++) {
          int row_idx = r + rr;
          if (row_idx >= row_cap) { ok = false; break; }
          for (int cc = 0; cc < cs; cc++) {
            if (occ[row_idx * cols + (ccol + cc)]) { ok = false; break; }
          }
          if (!ok) break;
        }
        if (ok) {
          for (int rr = 0; rr < rs; rr++) {
            int row_idx = r + rr;
            if (row_idx >= row_count) row_count = row_idx + 1;
            for (int cc = 0; cc < cs; cc++) {
              occ[row_idx * cols + (ccol + cc)] = true;
            }
          }
          placed = true;
          break;
        }
      }
      if (!placed) r++;
    }
    rows[i] = r;
    cols_out[i] = ccol;
    span_x[i] = cs;
    span_y[i] = rs;
  }
  int *row_heights = (int*)calloc((size_t)row_count, sizeof(int));
  bool *row_expand = (bool*)calloc((size_t)row_count, sizeof(bool));
  if (!row_heights || !row_expand) {
    if (slot_heap) free(slot_heap);
    free(occ);
    free(row_heights);
    free(row_expand);
    return;
  }
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int ch = 0;
    cogito_intrinsic_size(c, NULL, &ch);
    int rs = span_y[i] > 0 ? span_y[i] : 1;
    int per_row = rs > 0 ? (ch + rs - 1) / rs : ch;
    for (int rr = 0; rr < rs; rr++) {
      int row_idx = rows[i] + rr;
      if (row_idx >= row_count) break;
      if (per_row > row_heights[row_idx]) row_heights[row_idx] = per_row;
      if (c->vexpand) row_expand[row_idx] = true;
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
    if (any_expand) {
      int avail_h = n->h - n->padding_top - n->padding_bottom;
      if (avail_h < 0) avail_h = 0;
      int total_gap_h = row_count > 0 ? (row_count - 1) * gap_y : 0;
      int avail_row_h = avail_h - total_gap_h;
      if (avail_row_h < 0) avail_row_h = 0;
      int* row_out = (int*)calloc((size_t)row_count, sizeof(int));
      if (row_out) {
        cogito_distribute_axis_sizes((size_t)row_count, row_heights, row_expand, avail_row_h, row_out);
        for (int r = 0; r < row_count; r++) row_heights[r] = row_out[r];
        free(row_out);
      }
    }
  }
  int *row_y = (int*)calloc((size_t)row_count, sizeof(int));
  if (!row_y) {
    if (slot_heap) free(slot_heap);
    free(occ);
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
    int r = rows[i];
    int ccol = cols_out[i];
    int cs = span_x[i] > 0 ? span_x[i] : 1;
    int rs = span_y[i] > 0 ? span_y[i] : 1;
    int cell_x = base_x + ccol * (cell_w + gap_x);
    int cell_y = row_y[r];
    int cell_w_span = cell_w * cs + gap_x * (cs - 1);
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
    cogito_layout_node(c, cell_x + c->margin_left, cell_y + c->margin_top, cw, ch);
  }
  if (slot_heap) free(slot_heap);
  free(occ);
  free(row_heights);
  free(row_y);
  free(row_expand);
}
