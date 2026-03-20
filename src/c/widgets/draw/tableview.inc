// widgets/draw/tableview.inc — TableView drawing
//
// Draws: header row with labels, row backgrounds (striped), grid lines,
// selection highlight, checkboxes, and pagination footer.

case COGITO_TABLEVIEW: {
  CogitoTableViewData* tv = &n->kd->tableview;
  CogitoStyle s = cogito_theme_resolve_node(n);
  int cols = tv->col_count > 0 ? tv->col_count : 1;
  int inner_w = n->w - n->padding_left - n->padding_right;
  if (inner_w < 0) inner_w = 0;

  int checkbox_w = tv->show_checkboxes ? 48 : 0;
  int data_w = inner_w - checkbox_w;
  if (data_w < 0) data_w = 0;

  // Compute column widths
  int* cw = (int*)calloc((size_t)cols, sizeof(int));
  if (!cw) break;
  {
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
        if (!tv->col_widths || tv->col_widths[c] <= 0) cw[c] = per;
      }
    }
  }

  int tx = n->x + n->padding_left;
  int ty = n->y + n->padding_top;
  int total_rows = (int)n->len;

  // Pagination calculations
  bool has_pagination = tv->rows_per_page > 0;
  int page_count = 1;
  int row_start = 0;
  int row_end = total_rows;
  if (has_pagination) {
    page_count = total_rows > 0 ? (total_rows + tv->rows_per_page - 1) / tv->rows_per_page : 1;
    if (page_count < 1) page_count = 1;
    row_start = tv->page * tv->rows_per_page;
    row_end = row_start + tv->rows_per_page;
    if (row_start > total_rows) row_start = total_rows;
    if (row_end > total_rows) row_end = total_rows;
  }
  int visible_count = row_end - row_start;

  // ---- Header background ----
  CogitoColor header_bg = s.has_bg ? s.bg : n->text_color;
  header_bg.a = 20;
  cogito_draw_rect_plain(tx, ty, inner_w, tv->header_height, header_bg);

  // ---- Header checkbox ----
  if (tv->show_checkboxes) {
    int box_size = 18;
    int box_x = tx + (checkbox_w - box_size) / 2;
    int box_y = ty + (tv->header_height - box_size) / 2;
    bool some_checked = tv->checked_count > 0;
    bool all_rows_checked = tv->checked_count == total_rows && total_rows > 0;
    if (some_checked) {
      // Filled accent box
      cogito_draw_rect(box_x, box_y, box_size, box_size, cogito_theme_accent, 4);
      CogitoColor mark = cogito_on_color(cogito_theme_accent);
      if (all_rows_checked) {
        // Full checkmark
        int m = box_size - 2;
        int cx2 = box_x + box_size / 2;
        int cy2 = box_y + box_size / 2;
        int ox = cx2 - m / 2;
        int oy = cy2 - m / 2;
        int sx = ox + (m * 2) / 16;
        int sy = oy + (m * 8) / 16;
        int mx2 = ox + (m * 6) / 16;
        int my2 = oy + (m * 12) / 16;
        int ex = ox + (m * 14) / 16;
        int ey = oy + (m * 3) / 16;
        cogito_draw_line(sx, sy, mx2, my2, mark);
        cogito_draw_line(mx2, my2, ex, ey, mark);
      } else {
        // Indeterminate dash
        int dash_y = box_y + box_size / 2;
        int dash_x1 = box_x + 4;
        int dash_x2 = box_x + box_size - 4;
        cogito_draw_line(dash_x1, dash_y, dash_x2, dash_y, mark);
      }
    } else {
      // Empty box outline
      CogitoColor border = s.has_text ? s.text : n->text_color;
      border.a = 80;
      cogito_draw_rect_lines(box_x, box_y, box_size, box_size, border, 4, 2);
    }
  }

  // ---- Header labels ----
  if (tv->headers) {
    int hx = tx + checkbox_w;
    for (int c = 0; c < cols; c++) {
      if (tv->headers[c] && tv->headers[c]->data) {
        CogitoColor hdr_color = s.has_text ? s.text : n->text_color;
        int text_x = hx + 8;
        int text_y = ty + (tv->header_height - 14) / 2;
        cogito_draw_text_size_node(n, text_x, text_y, tv->headers[c]->data, hdr_color, 13, true, false);
        // Sort indicator
        if (tv->sort_col == c) {
          const char* arrow = tv->sort_dir == 0 ? "\xe2\x96\xb2" : "\xe2\x96\xbc";
          int aw = 10;
          cogito_draw_text_size_node(n, hx + cw[c] - aw - 8, text_y, arrow, hdr_color, 10, false, false);
        }
      }
      hx += cw[c];
    }
  }

  // ---- Row backgrounds, checkboxes, and selection ----
  int ry = ty + tv->header_height;
  for (int r = row_start; r < row_end; r++) {
    CogitoNode* row = n->children[r];
    if (cogito_node_is_hidden(row)) continue;

    // Striped background
    if (tv->striped && ((r - row_start) % 2 == 1)) {
      CogitoColor stripe = n->text_color;
      stripe.a = 10;
      cogito_draw_rect_plain(tx, ry, inner_w, tv->row_height, stripe);
    }

    // Selection highlight
    if (r == tv->selected_row) {
      CogitoColor sel = cogito_theme_accent;
      sel.a = 46;
      cogito_draw_rect_plain(tx, ry, inner_w, tv->row_height, sel);
    }

    // Row checkbox
    if (tv->show_checkboxes) {
      int box_size = 18;
      int box_x = tx + (checkbox_w - box_size) / 2;
      int box_y = ry + (tv->row_height - box_size) / 2;
      bool checked = tv->row_checked && r < total_rows ? tv->row_checked[r] : false;
      if (checked) {
        cogito_draw_rect(box_x, box_y, box_size, box_size, cogito_theme_accent, 4);
        CogitoColor mark = cogito_on_color(cogito_theme_accent);
        int m = box_size - 2;
        int cx2 = box_x + box_size / 2;
        int cy2 = box_y + box_size / 2;
        int ox = cx2 - m / 2;
        int oy = cy2 - m / 2;
        int sx = ox + (m * 2) / 16;
        int sy = oy + (m * 8) / 16;
        int mx2 = ox + (m * 6) / 16;
        int my2 = oy + (m * 12) / 16;
        int ex = ox + (m * 14) / 16;
        int ey = oy + (m * 3) / 16;
        cogito_draw_line(sx, sy, mx2, my2, mark);
        cogito_draw_line(mx2, my2, ex, ey, mark);
      } else {
        CogitoColor border = s.has_text ? s.text : n->text_color;
        border.a = 80;
        cogito_draw_rect_lines(box_x, box_y, box_size, box_size, border, 4, 2);
      }
    }

    ry += tv->row_height;
  }

  // ---- Grid lines ----
  if (tv->show_grid) {
    CogitoColor grid_color = n->text_color;
    grid_color.a = 30;
    // Horizontal lines after header and each visible row
    int ly = ty + tv->header_height;
    cogito_draw_line(tx, ly, tx + inner_w, ly, grid_color);
    for (int r = 0; r < visible_count; r++) {
      ly += tv->row_height;
      cogito_draw_line(tx, ly, tx + inner_w, ly, grid_color);
    }
    // Vertical column separators (offset by checkbox column)
    int vx = tx + checkbox_w;
    for (int c = 0; c < cols - 1; c++) {
      vx += cw[c];
      cogito_draw_line(vx, ty, vx, ly, grid_color);
    }
  }

  // ---- Draw children (visible page rows only) ----
  for (int r = row_start; r < row_end; r++) {
    CogitoNode* row = n->children[r];
    if (cogito_node_is_hidden(row)) continue;
    for (size_t c = 0; c < row->len; c++) {
      cogito_draw_node(row->children[c]);
    }
  }

  // ---- Pagination footer ----
  if (has_pagination) {
    int footer_y = ty + tv->header_height + visible_count * tv->row_height;
    int footer_h = tv->pagination_height;
    CogitoColor text_color = s.has_text ? s.text : n->text_color;

    // Separator line
    CogitoColor sep = text_color;
    sep.a = 20;
    cogito_draw_line(tx, footer_y, tx + inner_w, footer_y, sep);

    // "Rows per page:" label
    CogitoColor label_color = text_color;
    label_color.a = 160;
    int label_y = footer_y + (footer_h - 13) / 2;
    cogito_draw_text_size_node(n, tx + 12, label_y, "Rows per page:", label_color, 12, false, false);

    // Page size dropdown button
    if (tv->page_size_option_count > 0) {
      int dd_x = tx + 120;
      int dd_y = footer_y + (footer_h - 28) / 2;
      int dd_w = 56;
      int dd_h = 28;
      CogitoColor dd_bg = text_color;
      dd_bg.a = tv->show_page_size_dropdown ? 20 : 10;
      cogito_draw_rect(dd_x, dd_y, dd_w, dd_h, dd_bg, 6);

      char buf[16];
      snprintf(buf, sizeof(buf), "%d", tv->rows_per_page);
      int tw = (int)strlen(buf) * 7;
      cogito_draw_text_size_node(n, dd_x + (dd_w - tw) / 2, dd_y + (dd_h - 12) / 2, buf, text_color, 12, false, false);

      // Dropdown chevron
      int chev_x = dd_x + dd_w - 16;
      int chev_y = dd_y + dd_h / 2;
      CogitoColor chev = text_color;
      chev.a = 120;
      cogito_draw_text_size_node(n, chev_x, chev_y - 6, "\xe2\x8c\x84", chev, 10, false, false); // ⌄

      // Dropdown popup
      if (tv->show_page_size_dropdown) {
        int popup_w = dd_w;
        int popup_h = tv->page_size_option_count * 28 + 4;
        int popup_x = dd_x;
        int popup_y = dd_y - popup_h - 2;
        CogitoColor popup_bg = {255, 255, 255, 240};
        cogito_draw_rect(popup_x, popup_y, popup_w, popup_h, popup_bg, 8);
        CogitoColor popup_border = text_color;
        popup_border.a = 20;
        cogito_draw_rect_lines(popup_x, popup_y, popup_w, popup_h, popup_border, 8, 1);

        for (int i = 0; i < tv->page_size_option_count; i++) {
          int opt_y = popup_y + 2 + i * 28;
          char opt_buf[16];
          snprintf(opt_buf, sizeof(opt_buf), "%d", tv->page_size_options[i]);
          CogitoColor opt_color = text_color;
          if (tv->page_size_options[i] == tv->rows_per_page) {
            opt_color = cogito_theme_accent;
          }
          cogito_draw_text_size_node(n, popup_x + 16, opt_y + 7, opt_buf, opt_color, 12, false, false);
        }
      }
    }

    // Page navigation (right-aligned)
    int btn_r = 16; // button radius
    int btn_gap = 4;
    int nav_right = tx + inner_w - 12;

    // Next button ">"
    int next_x = nav_right - btn_r * 2;
    int next_cy = footer_y + footer_h / 2;
    {
      CogitoColor btn_bg = text_color;
      btn_bg.a = tv->page < page_count - 1 ? 16 : 8;
      cogito_draw_rect(next_x, next_cy - btn_r, btn_r * 2, btn_r * 2, btn_bg, btn_r);
      CogitoColor arrow = text_color;
      arrow.a = tv->page < page_count - 1 ? 200 : 60;
      cogito_draw_text_size_node(n, next_x + btn_r - 3, next_cy - 6, "\xe2\x80\xba", arrow, 14, false, false); // ›
    }

    // Page number buttons (right to left from next button)
    int pnav_x = next_x - btn_gap;
    // Determine which pages to show
    int max_pg_btns = 5;
    int pg_start = 0;
    int pg_end = page_count;
    bool show_end_ellipsis = false;
    bool show_start_ellipsis = false;
    if (page_count > max_pg_btns + 2) {
      if (tv->page <= max_pg_btns / 2 + 1) {
        pg_end = max_pg_btns;
        show_end_ellipsis = true;
      } else if (tv->page >= page_count - max_pg_btns / 2 - 2) {
        pg_start = page_count - max_pg_btns;
        show_start_ellipsis = true;
      } else {
        pg_start = tv->page - max_pg_btns / 2;
        pg_end = pg_start + max_pg_btns;
        show_start_ellipsis = true;
        show_end_ellipsis = true;
      }
      if (pg_start < 0) pg_start = 0;
      if (pg_end > page_count) pg_end = page_count;
    }

    // Last page button (if ellipsis on right)
    if (show_end_ellipsis) {
      pnav_x -= btn_r * 2;
      char pbuf[8];
      snprintf(pbuf, sizeof(pbuf), "%d", page_count);
      int ptw = (int)strlen(pbuf) * 7;
      cogito_draw_text_size_node(n, pnav_x + btn_r - ptw / 2, next_cy - 6, pbuf, text_color, 12, false, false);
      pnav_x -= btn_gap;
      // Ellipsis
      pnav_x -= 20;
      CogitoColor dots = text_color;
      dots.a = 120;
      cogito_draw_text_size_node(n, pnav_x + 4, next_cy - 6, "...", dots, 12, false, false);
      pnav_x -= btn_gap;
    }

    // Page number buttons (right to left)
    for (int p = pg_end - 1; p >= pg_start; p--) {
      pnav_x -= btn_r * 2;
      bool is_current = p == tv->page;
      if (is_current) {
        cogito_draw_rect(pnav_x, next_cy - btn_r, btn_r * 2, btn_r * 2, cogito_theme_accent, btn_r);
        CogitoColor on = cogito_on_color(cogito_theme_accent);
        char pbuf[8];
        snprintf(pbuf, sizeof(pbuf), "%d", p + 1);
        int ptw = (int)strlen(pbuf) * 7;
        cogito_draw_text_size_node(n, pnav_x + btn_r - ptw / 2, next_cy - 6, pbuf, on, 12, true, false);
      } else {
        char pbuf[8];
        snprintf(pbuf, sizeof(pbuf), "%d", p + 1);
        int ptw = (int)strlen(pbuf) * 7;
        cogito_draw_text_size_node(n, pnav_x + btn_r - ptw / 2, next_cy - 6, pbuf, text_color, 12, false, false);
      }
      pnav_x -= btn_gap;
    }

    // Start ellipsis + first page
    if (show_start_ellipsis) {
      pnav_x -= 20;
      CogitoColor dots = text_color;
      dots.a = 120;
      cogito_draw_text_size_node(n, pnav_x + 4, next_cy - 6, "...", dots, 12, false, false);
      pnav_x -= btn_gap;
      pnav_x -= btn_r * 2;
      bool is_first_current = tv->page == 0;
      if (is_first_current) {
        cogito_draw_rect(pnav_x, next_cy - btn_r, btn_r * 2, btn_r * 2, cogito_theme_accent, btn_r);
        CogitoColor on = cogito_on_color(cogito_theme_accent);
        cogito_draw_text_size_node(n, pnav_x + btn_r - 3, next_cy - 6, "1", on, 12, true, false);
      } else {
        cogito_draw_text_size_node(n, pnav_x + btn_r - 3, next_cy - 6, "1", text_color, 12, false, false);
      }
      pnav_x -= btn_gap;
    }

    // Prev button "<"
    pnav_x -= btn_r * 2;
    {
      CogitoColor btn_bg = text_color;
      btn_bg.a = tv->page > 0 ? 16 : 8;
      cogito_draw_rect(pnav_x, next_cy - btn_r, btn_r * 2, btn_r * 2, btn_bg, btn_r);
      CogitoColor arrow = text_color;
      arrow.a = tv->page > 0 ? 200 : 60;
      cogito_draw_text_size_node(n, pnav_x + btn_r - 3, next_cy - 6, "\xe2\x80\xb9", arrow, 14, false, false); // ‹
    }
  }

  free(cw);
  break;
}
