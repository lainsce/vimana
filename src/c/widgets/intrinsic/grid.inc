case COGITO_GRID: {
      int cols = n->grid.cols > 0 ? n->grid.cols : 1;
      int gap_x = n->grid.gap_x;
      int gap_y = n->grid.gap_y;
      size_t count = n->len;
      if (count == 0) {
        w = 0;
        h = 0;
        break;
      }

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
        w = 0;
        h = 0;
        break;
      }

      int *row_heights = (int*)calloc((size_t)row_count, sizeof(int));
      int *col_widths = (int*)calloc((size_t)cols, sizeof(int));
      if (!row_heights || !col_widths) {
        free(row_heights);
        free(col_widths);
        w = 0;
        h = 0;
        break;
      }

      for (size_t i = 0; i < count; i++) {
        CogitoNode* c = n->children[i];
        if (!c || c->hidden) continue;
        int iw = 0;
        int ih = 0;
        cogito_intrinsic_size(c, &iw, &ih);
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
        if (row + rs > row_count) rs = row_count - row;
        if (rs < 1) rs = 1;
        int per_col = cs > 0 ? (iw + cs - 1) / cs : iw;
        int per_row = rs > 0 ? (ih + rs - 1) / rs : ih;

        for (int cc = 0; cc < cs; cc++) {
          int col_idx = col + cc;
          if (col_idx >= cols) break;
          if (per_col > col_widths[col_idx]) col_widths[col_idx] = per_col;
        }
        for (int rr = 0; rr < rs; rr++) {
          int row_idx = row + rr;
          if (row_idx >= row_count) break;
          if (per_row > row_heights[row_idx]) row_heights[row_idx] = per_row;
        }
      }

      int sum_w = 0;
      for (int c = 0; c < cols; c++) sum_w += col_widths[c];
      int sum_h = 0;
      for (int r = 0; r < row_count; r++) sum_h += row_heights[r];

      int total_gap_w = cols > 0 ? (cols - 1) * gap_x : 0;
      int total_gap_h = row_count > 0 ? (row_count - 1) * gap_y : 0;

      w = sum_w + total_gap_w;
      h = sum_h + total_gap_h;
      if (w < 0) w = 0;
      if (h < 0) h = 0;
      free(row_heights);
      free(col_widths);
      break;
    }
    
