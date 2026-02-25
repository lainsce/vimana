case COGITO_DATEPICKER: {
  cogito_datepicker_ensure_date(n);
  bool hover = false;
  bool active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = n->datepicker.popover_panel
    ? cogito_theme_resolve_node(n)
    : cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text = s.has_text ? s.text : n->text_color;
  CogitoColor border = s.has_border ? s.border : n->border_color;
  CogitoColor sel = s.has_selection ? s.selection : n->selection_color;
  int border_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 0);
  int radius = s.has_radius ? s.radius : (n->datepicker.popover_panel ? 20 : 8);

  int mx = 0, my = 0;
  bool down = false;
  (void)cogito_pointer_query(&mx, &my, &down, NULL);

  if (n->datepicker.popover_panel) {
    CogitoDatePickerPanelLayout ly;
    cogito_datepicker_panel_layout(n, &ly);

    cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
    if (border_w > 0) {
      cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
    }

    uint8_t mode = cogito_datepicker_mode(n);
    bool calendar_mode = (mode == COGITO_DATEPICKER_MODE_CALENDAR);

    int base_size = cogito_node_font_size(n);
    int header_size = base_size;
    if (header_size < 14) header_size = 14;
    int week_size = base_size - 1;
    if (week_size < 13) week_size = 13;
    int day_size = base_size + 1;
    if (day_size < 15) day_size = 15;
    int action_size = base_size;
    if (action_size < 14) action_size = 14;

    CogitoColor text_muted = text;
    text_muted.a = 170;
    CogitoColor text_faded = text;
    text_faded.a = 112;
    CogitoColor outline = border;
    if (outline.a > 110) outline.a = 110;

    bool over_month_label = cogito_hit_rect(mx, my, ly.month_label_x, ly.month_label_y, ly.month_label_w, ly.month_label_h);
    bool over_year_label = cogito_hit_rect(mx, my, ly.year_label_x, ly.year_label_y, ly.year_label_w, ly.year_label_h);
    if (over_month_label || over_year_label) {
      CogitoColor hl = sel;
      hl.a = down ? 62 : 36;
      if (over_month_label) {
        cogito_draw_rect(ly.month_label_x, ly.month_label_y, ly.month_label_w, ly.month_label_h, hl, ly.month_label_h / 2);
      }
      if (over_year_label) {
        cogito_draw_rect(ly.year_label_x, ly.year_label_y, ly.year_label_w, ly.year_label_h, hl, ly.year_label_h / 2);
      }
    }

    if (calendar_mode) {
      bool over_month_prev = cogito_hit_rect(mx, my, ly.month_prev_x, ly.month_prev_y, ly.month_prev_w, ly.month_prev_h);
      bool over_month_next = cogito_hit_rect(mx, my, ly.month_next_x, ly.month_next_y, ly.month_next_w, ly.month_next_h);
      bool over_year_prev = cogito_hit_rect(mx, my, ly.year_prev_x, ly.year_prev_y, ly.year_prev_w, ly.year_prev_h);
      bool over_year_next = cogito_hit_rect(mx, my, ly.year_next_x, ly.year_next_y, ly.year_next_w, ly.year_next_h);

      CogitoColor nav_hl = sel;
      nav_hl.a = down ? 74 : 42;
      if (over_month_prev) cogito_draw_rect(ly.month_prev_x, ly.month_prev_y, ly.month_prev_w, ly.month_prev_h, nav_hl, ly.month_prev_h / 2);
      if (over_month_next) cogito_draw_rect(ly.month_next_x, ly.month_next_y, ly.month_next_w, ly.month_next_h, nav_hl, ly.month_next_h / 2);
      if (over_year_prev) cogito_draw_rect(ly.year_prev_x, ly.year_prev_y, ly.year_prev_w, ly.year_prev_h, nav_hl, ly.year_prev_h / 2);
      if (over_year_next) cogito_draw_rect(ly.year_next_x, ly.year_next_y, ly.year_next_w, ly.year_next_h, nav_hl, ly.year_next_h / 2);

      int nav_icon = 16;
      cogito_draw_icon_fallback(n, "sf:chevron.left",
                                ly.month_prev_x + (ly.month_prev_w - nav_icon) / 2,
                                ly.month_prev_y + (ly.month_prev_h - nav_icon) / 2,
                                nav_icon, text);
      cogito_draw_icon_fallback(n, "sf:chevron.right",
                                ly.month_next_x + (ly.month_next_w - nav_icon) / 2,
                                ly.month_next_y + (ly.month_next_h - nav_icon) / 2,
                                nav_icon, text);
      cogito_draw_icon_fallback(n, "sf:chevron.left",
                                ly.year_prev_x + (ly.year_prev_w - nav_icon) / 2,
                                ly.year_prev_y + (ly.year_prev_h - nav_icon) / 2,
                                nav_icon, text);
      cogito_draw_icon_fallback(n, "sf:chevron.right",
                                ly.year_next_x + (ly.year_next_w - nav_icon) / 2,
                                ly.year_next_y + (ly.year_next_h - nav_icon) / 2,
                                nav_icon, text);
    }

    const char* month_name = cogito_datepicker_month_short(n->datepicker.view_month);
    char year_buf[16];
    snprintf(year_buf, sizeof(year_buf), "%d", n->datepicker.view_year);

    int month_tw = cogito_text_width_size_node(n, month_name, header_size + 1);
    int year_tw = cogito_text_width_size_node(n, year_buf, header_size + 1);
    int label_th = cogito_text_height_size(header_size + 1);

    bool month_active = calendar_mode || mode == COGITO_DATEPICKER_MODE_MONTH_LIST;
    bool year_active = calendar_mode || mode == COGITO_DATEPICKER_MODE_YEAR_LIST;
    CogitoColor month_col = month_active ? text : text_muted;
    CogitoColor year_col = year_active ? text : text_muted;

    int drop_icon = 12;
    int month_block_w = month_tw + (month_active ? (6 + drop_icon) : 0);
    int year_block_w = year_tw + (year_active ? (6 + drop_icon) : 0);
    int month_tx = ly.month_label_x + (ly.month_label_w - month_block_w) / 2;
    int year_tx = ly.year_label_x + (ly.year_label_w - year_block_w) / 2;
    int label_ty = ly.month_label_y + (ly.month_label_h - label_th) / 2;

    cogito_draw_text_size_node(n, month_tx, label_ty, month_name, month_col, header_size + 1, true);
    if (month_active) {
      cogito_draw_icon_fallback(n, "sf:chevron.down",
                                month_tx + month_tw + 6,
                                ly.month_label_y + (ly.month_label_h - drop_icon) / 2,
                                drop_icon, month_col);
    }

    cogito_draw_text_size_node(n, year_tx, label_ty, year_buf, year_col, header_size + 1, true);
    if (year_active) {
      cogito_draw_icon_fallback(n, "sf:chevron.down",
                                year_tx + year_tw + 6,
                                ly.year_label_y + (ly.year_label_h - drop_icon) / 2,
                                drop_icon, year_col);
    }

    if (calendar_mode) {
      static const char* week_days[7] = {"S", "M", "T", "W", "T", "F", "S"};
      for (int col = 0; col < 7; col++) {
        int cx = ly.week_x + col * ly.cell_w;
        int tw = cogito_text_width_size_node(n, week_days[col], week_size);
        int th = cogito_text_height_size(week_size);
        cogito_draw_text_size_node(n,
                                   cx + (ly.cell_w - tw) / 2,
                                   ly.week_y + (ly.week_h - th) / 2,
                                   week_days[col], text_muted, week_size, false);
      }

      int today_y = 0, today_m = 0, today_d = 0;
      cogito_datepicker_today(&today_y, &today_m, &today_d);

      for (int idx = 0; idx < 42; idx++) {
        int row = idx / 7;
        int col = idx % 7;
        int cx = ly.grid_x + col * ly.cell_w;
        int cy = ly.grid_y + row * ly.cell_h;

        int y = 0, m = 0, d = 0;
        bool in_view = false;
        cogito_datepicker_panel_cell_date(n, idx, &y, &m, &d, &in_view);

        bool selected = cogito_datepicker_same_date(y, m, d,
                                                    n->datepicker.year,
                                                    n->datepicker.month,
                                                    n->datepicker.day);
        bool today = cogito_datepicker_same_date(y, m, d, today_y, today_m, today_d);
        bool over_cell = cogito_hit_rect(mx, my, cx, cy, ly.cell_w, ly.cell_h);

        int dot = ly.cell_h - 8;
        if (dot > ly.cell_w - 8) dot = ly.cell_w - 8;
        if (dot < 18) dot = 18;
        int dot_x = cx + (ly.cell_w - dot) / 2;
        int dot_y = cy + (ly.cell_h - dot) / 2;

        if (selected) {
          cogito_draw_rect(dot_x, dot_y, dot, dot, sel, dot / 2);
        } else if (over_cell) {
          CogitoColor hover_dot = sel;
          hover_dot.a = down ? 58 : 34;
          cogito_draw_rect(dot_x, dot_y, dot, dot, hover_dot, dot / 2);
        } else if (today) {
          cogito_draw_rect_lines(dot_x, dot_y, dot, dot, sel, dot / 2, 1);
        }

        char day_buf[8];
        snprintf(day_buf, sizeof(day_buf), "%d", d);
        CogitoColor day_col = text;
        if (!in_view) day_col = text_faded;
        if (today && !selected) day_col = sel;
        if (selected) day_col = cogito_on_color(sel);

        int tw = cogito_text_width_size_node(n, day_buf, day_size);
        int th = cogito_text_height_size(day_size);
        cogito_draw_text_size_node(n,
                                   cx + (ly.cell_w - tw) / 2,
                                   cy + (ly.cell_h - th) / 2,
                                   day_buf, day_col, day_size, false);
      }

      bool over_cancel = cogito_hit_rect(mx, my, ly.cancel_x, ly.cancel_y, ly.cancel_w, ly.cancel_h);
      bool over_ok = cogito_hit_rect(mx, my, ly.ok_x, ly.ok_y, ly.ok_w, ly.ok_h);

      CogitoColor sep = outline;
      cogito_draw_line(ly.footer_x,
                       ly.footer_y - COGITO_DATEPICKER_POP_FOOTER_GAP,
                       ly.footer_x + ly.footer_w,
                       ly.footer_y - COGITO_DATEPICKER_POP_FOOTER_GAP,
                       sep);

      CogitoColor action_hl = sel;
      action_hl.a = down ? 74 : 42;
      if (over_cancel) cogito_draw_rect(ly.cancel_x, ly.cancel_y, ly.cancel_w, ly.cancel_h, action_hl, 16);
      if (over_ok) cogito_draw_rect(ly.ok_x, ly.ok_y, ly.ok_w, ly.ok_h, action_hl, 16);

      int act_th = cogito_text_height_size(action_size + 1);
      int cw = cogito_text_width_size_node(n, "Cancel", action_size + 1);
      int ow = cogito_text_width_size_node(n, "OK", action_size + 1);
      cogito_draw_text_size_node(n,
                                 ly.cancel_x + (ly.cancel_w - cw) / 2,
                                 ly.cancel_y + (ly.cancel_h - act_th) / 2,
                                 "Cancel", sel, action_size + 1, true);
      cogito_draw_text_size_node(n,
                                 ly.ok_x + (ly.ok_w - ow) / 2,
                                 ly.ok_y + (ly.ok_h - act_th) / 2,
                                 "OK", sel, action_size + 1, true);
    } else {
      CogitoColor sep = outline;
      cogito_draw_line(ly.selector_x,
                       ly.selector_y - 8,
                       ly.selector_x + ly.selector_w,
                       ly.selector_y - 8,
                       sep);

      int rows = cogito_datepicker_selector_visible_rows(&ly);
      int total = cogito_datepicker_selector_total(mode);
      cogito_datepicker_selector_clamp_start(n, mode, rows);
      int start = n->datepicker.list_start;
      int selected_idx = cogito_datepicker_selector_selected_index(n, mode);

      for (int row = 0; row < rows; row++) {
        int idx = start + row;
        if (idx >= total) break;

        int iy = ly.selector_y + row * ly.selector_row_h;
        int ih = ly.selector_row_h;
        if (iy + ih > ly.selector_y + ly.selector_h) ih = ly.selector_y + ly.selector_h - iy;
        if (ih <= 0) break;

        bool selected = (idx == selected_idx);
        bool over_item = cogito_hit_rect(mx, my, ly.selector_x, iy, ly.selector_w, ih);

        if (selected) {
          CogitoColor row_sel = sel;
          row_sel.a = 44;
          cogito_draw_rect(ly.selector_x, iy, ly.selector_w, ih, row_sel, 0);
        } else if (over_item) {
          CogitoColor row_hover = sel;
          row_hover.a = down ? 28 : 18;
          cogito_draw_rect(ly.selector_x, iy, ly.selector_w, ih, row_hover, 0);
        }

        char label[32];
        if (mode == COGITO_DATEPICKER_MODE_MONTH_LIST) {
          snprintf(label, sizeof(label), "%s", cogito_datepicker_month_long(idx + 1));
        } else {
          snprintf(label, sizeof(label), "%d", COGITO_DATEPICKER_YEAR_MIN + idx);
        }

        int label_size = base_size + 2;
        if (label_size < 18) label_size = 18;
        int th = cogito_text_height_size(label_size);

        int check_size = 20;
        int text_x = ly.selector_x + 56;
        if (selected) {
          CogitoColor check_col = text;
          cogito_draw_icon_fallback(n,
                                    "checkmark",
                                    ly.selector_x + 20,
                                    iy + (ih - check_size) / 2,
                                    check_size,
                                    check_col);
        }

        CogitoColor row_text = selected ? text : text_muted;
        cogito_draw_text_size_node(n,
                                   text_x,
                                   iy + (ih - th) / 2,
                                   label,
                                   row_text,
                                   label_size,
                                   false);
      }

      int tx = 0, ty = 0, tw = 0, th = 0, ux = 0, uy = 0, uw = 0, uh = 0;
      if (cogito_datepicker_panel_selector_scrollbar_rect(n,
                                                          &tx, &ty, &tw, &th,
                                                          &ux, &uy, &uw, &uh)) {
        CogitoColor track = text_muted;
        track.a = 50;
        CogitoColor thumb = text_muted;
        thumb.a = 170;
        cogito_draw_rect(tx, ty, tw, th, track, tw / 2);
        cogito_draw_rect(ux, uy, uw, uh, thumb, uw / 2);
      }
    }
    break;
  }

  CogitoNode* win = cogito_node_window(n);
  bool open = (win && win->popover_open && win->popover_anchor == n);

  if (open) {
    border = sel;
    border_w = border_w < 2 ? 2 : border_w;
  } else if (border_w <= 0) {
    border_w = 1;
    border.a = 120;
  }

  int box_x = 0, box_y = 0, box_w = 0, box_h = 0;
  cogito_datepicker_field_box_rect(n, &box_x, &box_y, &box_w, &box_h);

  cogito_draw_rect_node(n, &s, box_x, box_y, box_w, box_h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines_node(n, &s, box_x, box_y, box_w, box_h, border, radius, border_w);
  }

  char value[32];
  cogito_datepicker_format_display(n->datepicker.year, n->datepicker.month, n->datepicker.day, value, sizeof(value));

  const char* label = (n->subtitle && n->subtitle->data && n->subtitle->data[0]) ? n->subtitle->data : "Date";

  int base_size = cogito_node_font_size(n);
  int label_size = base_size - 2;
  if (label_size < 11) label_size = 11;
  int value_size = base_size + 2;
  if (value_size < 16) value_size = 16;

  CogitoColor label_col = text;
  label_col.a = 165;

  int pad_x = 16;
  int icon_box = 32;
  int text_w = box_w - pad_x * 2 - icon_box - 8;
  if (text_w < 24) text_w = 24;

  cogito_draw_text_ellipsis(n, box_x + pad_x, box_y + 6, text_w, label, label_col, label_size, false, 0);

  int vth = cogito_text_height_size(value_size);
  int value_y = box_y + box_h - vth - 10;
  int min_value_y = box_y + 8 + cogito_text_height_size(label_size);
  if (value_y < min_value_y) value_y = min_value_y;
  cogito_draw_text_ellipsis(n, box_x + pad_x, value_y, text_w, value, text, value_size, false, 0);

  int icon_box_x = box_x + box_w - pad_x - icon_box;
  int icon_box_y = box_y + (box_h - icon_box) / 2;
  bool over_icon = cogito_hit_rect(mx, my, icon_box_x, icon_box_y, icon_box, icon_box);
  if (open || over_icon) {
    CogitoColor icon_hl = sel;
    icon_hl.a = (open || down) ? 62 : 40;
    cogito_draw_rect(icon_box_x, icon_box_y, icon_box, icon_box, icon_hl, icon_box / 2);
  }

  int icon_size = 18;
  CogitoColor icon_col = open ? sel : text;
  if (!open) icon_col.a = 190;
  cogito_draw_icon_fallback(n, "sf:calendar",
                            icon_box_x + (icon_box - icon_size) / 2,
                            icon_box_y + (icon_box - icon_size) / 2,
                            icon_size, icon_col);

  break;
}
