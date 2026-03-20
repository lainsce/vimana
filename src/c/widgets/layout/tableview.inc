// widgets/layout/tableview.inc — TableView layout
//
// Children are treated as rows. Each row's children are cells.
// TableView computes column widths (equal share or specified) and
// positions each cell accordingly. Supports checkbox column offset
// and pagination (only current page rows are laid out).

static void cogito_layout_tableview(CogitoNode* n, int x, int y) {
  CogitoTableViewData* tv = &n->kd->tableview;
  int cols = tv->col_count > 0 ? tv->col_count : 1;
  int inner_w = n->w - n->padding_left - n->padding_right;
  if (inner_w < 0) inner_w = 0;

  int checkbox_w = tv->show_checkboxes ? 48 : 0;
  int data_w = inner_w - checkbox_w;
  if (data_w < 0) data_w = 0;

  // Compute column widths
  int* cw = (int*)calloc((size_t)cols, sizeof(int));
  if (!cw) return;
  int total_specified = 0;
  int unspecified = 0;
  for (int c = 0; c < cols; c++) {
    if (tv->col_widths && tv->col_widths[c] > 0) {
      cw[c] = tv->col_widths[c];
      total_specified += cw[c];
    } else {
      unspecified++;
    }
  }
  if (unspecified > 0) {
    int remaining = data_w - total_specified;
    if (remaining < 0) remaining = 0;
    int per = remaining / unspecified;
    for (int c = 0; c < cols; c++) {
      if (!tv->col_widths || tv->col_widths[c] <= 0) {
        cw[c] = per;
      }
    }
  }

  int cx = x + n->padding_left + checkbox_w;
  int cy = y + n->padding_top;

  // Skip header area
  cy += tv->header_height;

  // Determine visible row range (pagination)
  int total_rows = (int)n->len;
  int row_start = 0;
  int row_end = total_rows;
  if (tv->rows_per_page > 0) {
    row_start = tv->page * tv->rows_per_page;
    row_end = row_start + tv->rows_per_page;
    if (row_start > total_rows) row_start = total_rows;
    if (row_end > total_rows) row_end = total_rows;
  }

  // Layout visible rows only; hide others
  for (int r = 0; r < total_rows; r++) {
    CogitoNode* row = n->children[r];
    if (r < row_start || r >= row_end) {
      row->w = 0;
      row->h = 0;
      continue;
    }
    if (cogito_node_is_hidden(row)) continue;
    row->x = cx;
    row->y = cy;
    row->w = data_w;
    row->h = tv->row_height;

    // Position cells within the row
    int cell_x = cx;
    for (size_t c = 0; c < row->len && (int)c < cols; c++) {
      CogitoNode* cell = row->children[c];
      if (cogito_node_is_hidden(cell)) continue;
      cell->x = cell_x;
      cell->y = cy;
      cell->w = cw[c];
      cell->h = tv->row_height;
      cogito_layout_node(cell, cell->x, cell->y, cell->w, cell->h);
      cell_x += cw[c];
    }

    cy += tv->row_height;
  }

  free(cw);
}
