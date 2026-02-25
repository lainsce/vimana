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

      int *rows = (int*)calloc(count, sizeof(int));
      int *cols_out = (int*)calloc(count, sizeof(int));
      int *span_x = (int*)calloc(count, sizeof(int));
      int *span_y = (int*)calloc(count, sizeof(int));
      if (!rows || !cols_out || !span_x || !span_y) {
        free(rows);
        free(cols_out);
        free(span_x);
        free(span_y);
        w = 0;
        h = 0;
        break;
      }

      int row_cap = 4;
      int row_count = 0;
      bool *occ = (bool*)calloc((size_t)row_cap * (size_t)cols, sizeof(bool));
      if (!occ) {
        free(rows);
        free(cols_out);
        free(span_x);
        free(span_y);
        w = 0;
        h = 0;
        break;
      }

      for (size_t i = 0; i < count; i++) {
        CogitoNode* c = n->children[i];
        int cs = c->grid_span_x > 0 ? c->grid_span_x : 0;
        int rs = c->grid_span_y > 0 ? c->grid_span_y : 0;
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
      int *col_widths = (int*)calloc((size_t)cols, sizeof(int));
      if (!row_heights || !col_widths) {
        free(rows);
        free(cols_out);
        free(span_x);
        free(span_y);
        free(occ);
        free(row_heights);
        free(col_widths);
        w = 0;
        h = 0;
        break;
      }

      for (size_t i = 0; i < count; i++) {
        CogitoNode* c = n->children[i];
        int iw = 0;
        int ih = 0;
        cogito_intrinsic_size(c, &iw, &ih);
        int cs = span_x[i] > 0 ? span_x[i] : 0;
        int rs = span_y[i] > 0 ? span_y[i] : 0;
        int per_col = cs > 0 ? (iw + cs - 1) / cs : iw;
        int per_row = rs > 0 ? (ih + rs - 1) / rs : ih;

        for (int cc = 0; cc < cs; cc++) {
          int col_idx = cols_out[i] + cc;
          if (col_idx >= cols) break;
          if (per_col > col_widths[col_idx]) col_widths[col_idx] = per_col;
        }
        for (int rr = 0; rr < rs; rr++) {
          int row_idx = rows[i] + rr;
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

      free(rows);
      free(cols_out);
      free(span_x);
      free(span_y);
      free(occ);
      free(row_heights);
      free(col_widths);
      break;
    }
    
