static CogitoNode* cogito_popover_new_obj(void);
static void cogito_popover_show(ErgoVal anchorv, ErgoVal popoverv);
static void cogito_popover_close(ErgoVal windowv);

static CogitoNode* cogito_datepicker_open_panel(CogitoNode* owner) {
  if (!owner || owner->kind != COGITO_DATEPICKER) return NULL;
  cogito_datepicker_ensure_date(owner);

  CogitoNode* pop = cogito_popover_new_obj();
  pop->padding_set = true;
  pop->padding_left = 0;
  pop->padding_top = 0;
  pop->padding_right = 0;
  pop->padding_bottom = 0;
  pop->gap = 0;
  pop->radius_set = true;
  pop->border_radius = 20;

  CogitoNode* panel = cogito_node_new(COGITO_DATEPICKER);
  panel->editable = false;
  panel->datepicker.popover_panel = true;
  panel->datepicker.owner = owner;
  panel->datepicker.year = owner->datepicker.year;
  panel->datepicker.month = owner->datepicker.month;
  panel->datepicker.day = owner->datepicker.day;
  panel->datepicker.view_year = owner->datepicker.year;
  panel->datepicker.view_month = owner->datepicker.month;
  panel->datepicker.list_start = 0;
  panel->datepicker.mode = COGITO_DATEPICKER_MODE_CALENDAR;
  panel->radius_set = true;
  panel->border_radius = 20;

  cogito_children_add(pop, panel);
  ergo_release_val(EV_OBJ(panel));
  return pop;
}

static void cogito_datepicker_show_popover(CogitoNode* anchor) {
  if (!anchor || anchor->kind != COGITO_DATEPICKER) return;
  CogitoNode* win = cogito_node_window(anchor);
  if (!win) return;

  CogitoNode* pop = cogito_datepicker_open_panel(anchor);
  if (!pop) return;
  cogito_popover_show(EV_OBJ(anchor), EV_OBJ(pop));
  ergo_release_val(EV_OBJ(pop));
}

static bool cogito_datepicker_hit_panel(CogitoNode* panel, int mx, int my) {
  if (!panel || panel->kind != COGITO_DATEPICKER || !panel->datepicker.popover_panel) return false;
  cogito_datepicker_ensure_date(panel);

  CogitoNode* owner = panel->datepicker.owner;
  CogitoNode* win = cogito_node_window(panel);
  CogitoDatePickerPanelLayout ly;
  cogito_datepicker_panel_layout(panel, &ly);

  uint8_t mode = cogito_datepicker_mode(panel);
  bool needs_relayout = false;

  bool over_month_label = cogito_hit_rect_min48(mx, my, ly.month_label_x, ly.month_label_y, ly.month_label_w, ly.month_label_h);
  bool over_year_label = cogito_hit_rect_min48(mx, my, ly.year_label_x, ly.year_label_y, ly.year_label_w, ly.year_label_h);

  if (mode == COGITO_DATEPICKER_MODE_CALENDAR) {
    if (cogito_hit_rect_min48(mx, my, ly.cancel_x, ly.cancel_y, ly.cancel_w, ly.cancel_h)) {
      if (win) cogito_popover_close(EV_OBJ(win));
      return true;
    }

    if (cogito_hit_rect_min48(mx, my, ly.ok_x, ly.ok_y, ly.ok_w, ly.ok_h)) {
      bool changed = false;
      if (owner && owner->kind == COGITO_DATEPICKER) {
        cogito_datepicker_ensure_date(owner);
        changed = !cogito_datepicker_same_date(owner->datepicker.year, owner->datepicker.month, owner->datepicker.day,
                                               panel->datepicker.year, panel->datepicker.month, panel->datepicker.day);
        owner->datepicker.year = panel->datepicker.year;
        owner->datepicker.month = panel->datepicker.month;
        owner->datepicker.day = panel->datepicker.day;
        owner->datepicker.view_year = owner->datepicker.year;
        owner->datepicker.view_month = owner->datepicker.month;
      }
      if (win) cogito_popover_close(EV_OBJ(win));
      if (changed && owner) cogito_invoke_change(owner);
      return true;
    }

    if (over_month_label) {
      cogito_datepicker_selector_set_mode(panel, COGITO_DATEPICKER_MODE_MONTH_LIST, &ly);
      needs_relayout = true;
    } else if (over_year_label) {
      cogito_datepicker_selector_set_mode(panel, COGITO_DATEPICKER_MODE_YEAR_LIST, &ly);
      needs_relayout = true;
    } else if (cogito_hit_rect_min48(mx, my, ly.month_prev_x, ly.month_prev_y, ly.month_prev_w, ly.month_prev_h)) {
      cogito_datepicker_shift_view_month(panel, -1);
      needs_relayout = true;
    } else if (cogito_hit_rect_min48(mx, my, ly.month_next_x, ly.month_next_y, ly.month_next_w, ly.month_next_h)) {
      cogito_datepicker_shift_view_month(panel, 1);
      needs_relayout = true;
    } else if (cogito_hit_rect_min48(mx, my, ly.year_prev_x, ly.year_prev_y, ly.year_prev_w, ly.year_prev_h)) {
      cogito_datepicker_shift_view_year(panel, -1);
      needs_relayout = true;
    } else if (cogito_hit_rect_min48(mx, my, ly.year_next_x, ly.year_next_y, ly.year_next_w, ly.year_next_h)) {
      cogito_datepicker_shift_view_year(panel, 1);
      needs_relayout = true;
    } else {
      int y = 0, m = 0, d = 0;
      bool in_view = false;
      if (cogito_datepicker_panel_hit_day(panel, mx, my, &y, &m, &d, &in_view)) {
        cogito_datepicker_set_selected(panel, y, m, d);
        panel->datepicker.view_year = y;
        panel->datepicker.view_month = m;
        needs_relayout = true;
        (void)in_view;
      }
    }
  } else {
    if (over_month_label) {
      if (mode == COGITO_DATEPICKER_MODE_MONTH_LIST) {
        cogito_datepicker_selector_set_mode(panel, COGITO_DATEPICKER_MODE_CALENDAR, &ly);
      } else {
        cogito_datepicker_selector_set_mode(panel, COGITO_DATEPICKER_MODE_MONTH_LIST, &ly);
      }
      needs_relayout = true;
    } else if (over_year_label) {
      if (mode == COGITO_DATEPICKER_MODE_YEAR_LIST) {
        cogito_datepicker_selector_set_mode(panel, COGITO_DATEPICKER_MODE_CALENDAR, &ly);
      } else {
        cogito_datepicker_selector_set_mode(panel, COGITO_DATEPICKER_MODE_YEAR_LIST, &ly);
      }
      needs_relayout = true;
    } else {
      int tx = 0, ty = 0, tw = 0, th = 0, ux = 0, uy = 0, uw = 0, uh = 0;
      if (cogito_datepicker_panel_selector_scrollbar_rect(panel,
                                                          &tx, &ty, &tw, &th,
                                                          &ux, &uy, &uw, &uh) &&
          cogito_hit_rect(mx, my, tx, ty, tw, th)) {
        if (cogito_datepicker_panel_selector_set_start_from_track(panel, my)) {
          needs_relayout = true;
        }
      } else {
        int idx = -1;
        if (cogito_datepicker_panel_hit_selector_item(panel, mx, my, &idx)) {
          cogito_datepicker_selector_apply_index(panel, mode, idx);
          cogito_datepicker_selector_set_mode(panel, COGITO_DATEPICKER_MODE_CALENDAR, &ly);
          needs_relayout = true;
        }
      }
    }
  }

  if (needs_relayout) {
    CogitoNode* relayout_win = win ? win : cogito_node_window(owner ? owner : panel);
    cogito_window_relayout(relayout_win);
    return true;
  }

  return cogito_hit_rect(mx, my, panel->x, panel->y, panel->w, panel->h);
}

static bool cogito_datepicker_wheel(CogitoNode* panel, int mx, int my, float wheel) {
  if (!panel || panel->kind != COGITO_DATEPICKER || !panel->datepicker.popover_panel) return false;
  if (wheel == 0.0f) return false;

  cogito_datepicker_ensure_date(panel);
  uint8_t mode = cogito_datepicker_mode(panel);
  if (mode == COGITO_DATEPICKER_MODE_CALENDAR) return false;

  CogitoDatePickerPanelLayout ly;
  cogito_datepicker_panel_layout(panel, &ly);
  if (!cogito_hit_rect(mx, my, ly.selector_x, ly.selector_y, ly.selector_w, ly.selector_h) &&
      !cogito_hit_rect(mx, my, ly.selector_scroll_x, ly.selector_scroll_y, ly.selector_scroll_w, ly.selector_scroll_h)) {
    return false;
  }

  int rows = cogito_datepicker_selector_visible_rows(&ly);
  int total = cogito_datepicker_selector_total(mode);
  if (total <= rows) return false;

  cogito_datepicker_selector_clamp_start(panel, mode, rows);
  int old_start = panel->datepicker.list_start;

  int notches = (int)lroundf(wheel);
  if (notches == 0) notches = (wheel > 0.0f) ? 1 : -1;
  panel->datepicker.list_start -= notches;
  cogito_datepicker_selector_clamp_start(panel, mode, rows);

  return panel->datepicker.list_start != old_start;
}

static bool cogito_datepicker_hit(CogitoNode* n, int mx, int my, bool clicked) {
  if (!n || n->kind != COGITO_DATEPICKER || !clicked) return false;

  if (n->datepicker.popover_panel) {
    return cogito_datepicker_hit_panel(n, mx, my);
  }

  CogitoNode* win = cogito_node_window(n);
  if (!win) return false;
  if (win->popover_open && win->popover_anchor == n) {
    cogito_popover_close(EV_OBJ(win));
    return true;
  }
  cogito_datepicker_show_popover(n);
  return true;
}

static CogitoNode* cogito_datepicker_new_obj(void) {
  CogitoNode* dp = cogito_node_new(COGITO_DATEPICKER);
  dp->editable = false;
  dp->datepicker.popover_panel = false;
  dp->datepicker.owner = NULL;
  dp->datepicker.mode = COGITO_DATEPICKER_MODE_CALENDAR;
  dp->datepicker.list_start = 0;
  cogito_datepicker_ensure_date(dp);
  return dp;
}

static ErgoVal cogito_datepicker_new(void) {
  return EV_OBJ(cogito_datepicker_new_obj());
}

static void cogito_datepicker_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.datepicker_on_change expects datepicker");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (!n || n->kind != COGITO_DATEPICKER) ergo_trap("cogito.datepicker_on_change expects datepicker");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  if (handler.tag == EVT_FN) {
    n->on_change = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(n->on_change));
  } else {
    n->on_change = NULL;
  }
}

static void cogito_datepicker_click(CogitoNode* n, int mx, int my) {
  (void)cogito_datepicker_hit(n, mx, my, true);
}
