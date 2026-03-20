// widgets/interaction/tableview.inc — TableView creation and API

CogitoNode* cogito_tableview_new_obj(int cols) {
  CogitoNode* n = cogito_node_new(COGITO_TABLEVIEW);
  n->kd->tableview.col_count = cols > 0 ? cols : 1;
  n->kd->tableview.selected_row = -1;
  n->kd->tableview.header_height = 36;
  n->kd->tableview.row_height = 40;
  n->kd->tableview.sort_col = -1;
  n->kd->tableview.pagination_height = 48;
  return n;
}

void cogito_tableview_set_headers(CogitoNode* n, YisStr** headers, int count) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  CogitoTableViewData* tv = &n->kd->tableview;
  // Free old headers
  if (tv->headers) {
    for (int i = 0; i < tv->col_count; i++) {
      if (tv->headers[i]) yis_release_val(YV_STR(tv->headers[i]));
    }
    free(tv->headers);
  }
  tv->headers = (YisStr**)calloc((size_t)count, sizeof(YisStr*));
  for (int i = 0; i < count && i < tv->col_count; i++) {
    tv->headers[i] = headers[i];
    if (headers[i]) yis_retain_val(YV_STR(headers[i]));
  }
}

void cogito_tableview_set_col_width(CogitoNode* n, int col, int width) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  CogitoTableViewData* tv = &n->kd->tableview;
  if (!tv->col_widths) {
    tv->col_widths = (int*)calloc((size_t)tv->col_count, sizeof(int));
  }
  if (col >= 0 && col < tv->col_count) {
    tv->col_widths[col] = width;
  }
}

int cogito_tableview_get_selected(CogitoNode* n) {
  if (!n || n->kind != COGITO_TABLEVIEW) return -1;
  return n->kd->tableview.selected_row;
}

void cogito_tableview_set_selected(CogitoNode* n, int row) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  n->kd->tableview.selected_row = row;
}

void cogito_tableview_set_show_grid(CogitoNode* n, bool show) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  n->kd->tableview.show_grid = show;
}

void cogito_tableview_set_striped(CogitoNode* n, bool striped) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  n->kd->tableview.striped = striped;
}

void cogito_tableview_set_row_height(CogitoNode* n, int h) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  n->kd->tableview.row_height = h > 0 ? h : 40;
}

void cogito_tableview_set_header_height(CogitoNode* n, int h) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  n->kd->tableview.header_height = h > 0 ? h : 36;
}

void cogito_tableview_add_row(CogitoNode* n, CogitoNode* row) {
  if (!n || n->kind != COGITO_TABLEVIEW || !row) return;
  CogitoTableViewData* tv = &n->kd->tableview;
  // Grow row_checked array to accommodate the new row
  if (tv->show_checkboxes) {
    size_t new_len = n->len + 1;
    bool* rc = (bool*)realloc(tv->row_checked, new_len * sizeof(bool));
    if (rc) {
      rc[n->len] = false;
      tv->row_checked = rc;
    }
  }
  cogito_children_add(n, row);
}

void cogito_tableview_clear(CogitoNode* n) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  CogitoTableViewData* tv = &n->kd->tableview;
  cogito_clear_children(n);
  free(tv->row_checked);
  tv->row_checked = NULL;
  tv->checked_count = 0;
  tv->selected_row = -1;
  tv->page = 0;
}

// --- Checkbox API ---

void cogito_tableview_set_checkboxes(CogitoNode* n, bool show) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  CogitoTableViewData* tv = &n->kd->tableview;
  tv->show_checkboxes = show;
  if (show && !tv->row_checked && n->len > 0) {
    tv->row_checked = (bool*)calloc(n->len, sizeof(bool));
  }
}

void cogito_tableview_check_row(CogitoNode* n, int row, bool checked) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  CogitoTableViewData* tv = &n->kd->tableview;
  if (row < 0 || row >= (int)n->len) return;
  if (!tv->row_checked) return;
  bool was = tv->row_checked[row];
  tv->row_checked[row] = checked;
  if (checked && !was) tv->checked_count++;
  else if (!checked && was) tv->checked_count--;
}

bool cogito_tableview_is_row_checked(CogitoNode* n, int row) {
  if (!n || n->kind != COGITO_TABLEVIEW) return false;
  CogitoTableViewData* tv = &n->kd->tableview;
  if (row < 0 || row >= (int)n->len || !tv->row_checked) return false;
  return tv->row_checked[row];
}

void cogito_tableview_check_all(CogitoNode* n, bool checked) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  CogitoTableViewData* tv = &n->kd->tableview;
  if (!tv->row_checked && n->len > 0) {
    tv->row_checked = (bool*)calloc(n->len, sizeof(bool));
  }
  if (!tv->row_checked) return;
  for (size_t i = 0; i < n->len; i++) {
    tv->row_checked[i] = checked;
  }
  tv->checked_count = checked ? (int)n->len : 0;
}

int cogito_tableview_get_checked_count(CogitoNode* n) {
  if (!n || n->kind != COGITO_TABLEVIEW) return 0;
  return n->kd->tableview.checked_count;
}

// --- Pagination API ---

void cogito_tableview_set_page(CogitoNode* n, int page) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  CogitoTableViewData* tv = &n->kd->tableview;
  int total = (int)n->len;
  int max_page = 0;
  if (tv->rows_per_page > 0 && total > 0) {
    max_page = (total - 1) / tv->rows_per_page;
  }
  tv->page = page < 0 ? 0 : (page > max_page ? max_page : page);
}

int cogito_tableview_get_page(CogitoNode* n) {
  if (!n || n->kind != COGITO_TABLEVIEW) return 0;
  return n->kd->tableview.page;
}

void cogito_tableview_set_rows_per_page(CogitoNode* n, int count) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  CogitoTableViewData* tv = &n->kd->tableview;
  tv->rows_per_page = count > 0 ? count : 0;
  // Re-clamp page
  if (tv->rows_per_page > 0) {
    int total = (int)n->len;
    int max_page = total > 0 ? (total - 1) / tv->rows_per_page : 0;
    if (tv->page > max_page) tv->page = max_page;
  }
}

int cogito_tableview_get_rows_per_page(CogitoNode* n) {
  if (!n || n->kind != COGITO_TABLEVIEW) return 0;
  return n->kd->tableview.rows_per_page;
}

int cogito_tableview_get_page_count(CogitoNode* n) {
  if (!n || n->kind != COGITO_TABLEVIEW) return 1;
  CogitoTableViewData* tv = &n->kd->tableview;
  if (tv->rows_per_page <= 0) return 1;
  int total = (int)n->len;
  if (total <= 0) return 1;
  return (total + tv->rows_per_page - 1) / tv->rows_per_page;
}

void cogito_tableview_set_page_sizes(CogitoNode* n, int* sizes, int count) {
  if (!n || n->kind != COGITO_TABLEVIEW) return;
  CogitoTableViewData* tv = &n->kd->tableview;
  free(tv->page_size_options);
  tv->page_size_options = NULL;
  tv->page_size_option_count = 0;
  if (sizes && count > 0) {
    tv->page_size_options = (int*)malloc((size_t)count * sizeof(int));
    if (tv->page_size_options) {
      for (int i = 0; i < count; i++) tv->page_size_options[i] = sizes[i];
      tv->page_size_option_count = count;
    }
  }
}

// --- Click handling ---

static bool cogito_tableview_handle_click(CogitoNode* n, int mx, int my) {
  if (!n || n->kind != COGITO_TABLEVIEW) return false;
  CogitoTableViewData* tv = &n->kd->tableview;
  int tx = n->x + n->padding_left;
  int ty = n->y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int checkbox_w = tv->show_checkboxes ? 48 : 0;
  int header_bottom = ty + tv->header_height;

  // Pagination area
  bool has_pagination = tv->rows_per_page > 0;
  int total_rows = (int)n->len;
  int page_count = has_pagination ? ((total_rows + tv->rows_per_page - 1) / tv->rows_per_page) : 1;
  if (page_count < 1) page_count = 1;
  int visible_rows = has_pagination ? tv->rows_per_page : total_rows;
  int data_bottom = header_bottom + visible_rows * tv->row_height;

  // Click in pagination footer area
  if (has_pagination && my >= data_bottom) {
    int footer_y = data_bottom;
    int footer_h = tv->pagination_height;
    if (my < footer_y + footer_h) {
      // "Rows per page" dropdown toggle (left area, first 200px)
      if (mx >= tx && mx < tx + 200 && tv->page_size_option_count > 0) {
        // Check if dropdown popup is showing
        if (tv->show_page_size_dropdown) {
          // Check click on a dropdown option
          int opt_y = footer_y - tv->page_size_option_count * 28;
          for (int i = 0; i < tv->page_size_option_count; i++) {
            int oy = opt_y + i * 28;
            if (my >= oy && my < oy + 28 && mx >= tx + 100 && mx < tx + 180) {
              tv->rows_per_page = tv->page_size_options[i];
              tv->page = 0;
              tv->show_page_size_dropdown = false;
              return true;
            }
          }
          tv->show_page_size_dropdown = false;
          return true;
        }
        // Toggle dropdown open
        if (mx >= tx + 100 && mx < tx + 180) {
          tv->show_page_size_dropdown = !tv->show_page_size_dropdown;
          return true;
        }
      }
      // Page navigation (right area)
      int nav_x = tx + inner_w - 220;
      int btn_size = 32;
      // Prev button
      if (mx >= nav_x && mx < nav_x + btn_size) {
        if (tv->page > 0) tv->page--;
        return true;
      }
      nav_x += btn_size + 4;
      // Page number buttons (show up to 7 page buttons)
      int max_visible = 7;
      int start_p = 0;
      if (page_count > max_visible) {
        start_p = tv->page - max_visible / 2;
        if (start_p < 0) start_p = 0;
        if (start_p + max_visible > page_count) start_p = page_count - max_visible;
      }
      int end_p = start_p + (page_count < max_visible ? page_count : max_visible);
      for (int p = start_p; p < end_p; p++) {
        if (mx >= nav_x && mx < nav_x + btn_size) {
          tv->page = p;
          return true;
        }
        nav_x += btn_size + 4;
      }
      // Next button
      if (mx >= nav_x && mx < nav_x + btn_size) {
        if (tv->page < page_count - 1) tv->page++;
        return true;
      }
      return true;
    }
    return false;
  }

  // Close page size dropdown on any other click
  if (tv->show_page_size_dropdown) {
    tv->show_page_size_dropdown = false;
  }

  int row_start = has_pagination ? tv->page * tv->rows_per_page : 0;

  // Header checkbox click
  if (tv->show_checkboxes && my >= ty && my < header_bottom && mx >= tx && mx < tx + checkbox_w) {
    bool any_checked = tv->checked_count > 0;
    cogito_tableview_check_all(n, !any_checked);
    // Fire on_check with -1 for select-all
    if (n->on_check) {
      YisVal args[1] = { YV_INT(-1) };
      yis_call(YV_FN(n->on_check), 1, args);
    }
    return true;
  }

  // Row area click
  if (my >= header_bottom) {
    int rel_y = my - header_bottom;
    if (tv->row_height <= 0) return false;
    int vis_row = rel_y / tv->row_height;
    int abs_row = row_start + vis_row;
    if (abs_row < 0 || abs_row >= total_rows) return false;

    // Row checkbox click
    if (tv->show_checkboxes && mx >= tx && mx < tx + checkbox_w) {
      bool checked = tv->row_checked ? tv->row_checked[abs_row] : false;
      cogito_tableview_check_row(n, abs_row, !checked);
      if (n->on_check) {
        YisVal args[1] = { YV_INT(abs_row) };
        yis_call(YV_FN(n->on_check), 1, args);
      }
      return true;
    }

    // Normal row selection
    tv->selected_row = abs_row;
    if (n->on_select) {
      YisVal args[1] = { YV_INT(abs_row) };
      yis_call(YV_FN(n->on_select), 1, args);
    }
    return true;
  }
  return false;
}
