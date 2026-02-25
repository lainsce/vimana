case COGITO_TIMEPICKER: {
  cogito_timepicker_ensure_time(n);
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = n->timepicker.popover_panel
    ? cogito_theme_resolve_node(n)
    : cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text = s.has_text ? s.text : n->text_color;
  CogitoColor border = s.has_border ? s.border : n->border_color;
  CogitoColor sel = s.has_selection ? s.selection : n->selection_color;
  int border_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 0);
  int radius = s.has_radius ? s.radius : (n->timepicker.popover_panel ? 24 : 8);

  if (n->timepicker.popover_panel) {
    CogitoTimePickerPanelLayout ly;
    cogito_timepicker_panel_layout(n, &ly);

    cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
    if (border_w > 0) {
      cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
    }

    int base_size = cogito_node_font_size(n);
    int header_size = base_size;
    if (header_size < 14) header_size = 14;
    int time_size = base_size + 26;
    if (time_size < 36) time_size = 36;
    int dial_size = base_size + 4;
    if (dial_size < 14) dial_size = 14;
    int footer_size = base_size;
    if (footer_size < 14) footer_size = 14;

    int mx = 0, my = 0;
    bool down = false;
    (void)cogito_pointer_query(&mx, &my, &down, NULL);

    cogito_draw_text_size_node(n, ly.header_x,
                               ly.header_y + (ly.header_h - cogito_text_height_size(header_size)) / 2,
                               "Select time", text, header_size, false);

    uint8_t mode = n->timepicker.mode;
    if (mode != COGITO_TIMEPICKER_MODE_MINUTE) mode = COGITO_TIMEPICKER_MODE_HOUR;
    bool is_pm = (n->timepicker.hour >= 12);
    int display_hour = cogito_timepicker_hour12_from_24(n->timepicker.hour);

    CogitoColor tertiary = {0}, tertiary_container = {0};
    cogito_theme_get_tertiary(&tertiary, NULL, &tertiary_container, NULL);
    CogitoColor box_bg = cogito_theme_dark
      ? cogito_mix(bg, cogito_rgba(255, 255, 255, 255), 0.10f)
      : cogito_mix(bg, cogito_rgba(0, 0, 0, 255), 0.06f);
    box_bg.a = 255;
    CogitoColor box_active_bg = cogito_mix(box_bg, sel, 0.30f);
    box_active_bg.a = 255;
    CogitoColor box_border = border;
    if (box_border.a > 170) box_border.a = 170;

    cogito_draw_rect(ly.hour_x, ly.hour_y, ly.hour_w, ly.hour_h,
                     mode == COGITO_TIMEPICKER_MODE_HOUR ? box_active_bg : box_bg, 10);
    cogito_draw_rect(ly.minute_x, ly.minute_y, ly.minute_w, ly.minute_h,
                     mode == COGITO_TIMEPICKER_MODE_MINUTE ? box_active_bg : box_bg, 10);
    cogito_draw_rect_lines(ly.hour_x, ly.hour_y, ly.hour_w, ly.hour_h, box_border, 10, 1);
    cogito_draw_rect_lines(ly.minute_x, ly.minute_y, ly.minute_w, ly.minute_h, box_border, 10, 1);

    char hbuf[8];
    char mbuf[8];
    snprintf(hbuf, sizeof(hbuf), "%02d", display_hour);
    snprintf(mbuf, sizeof(mbuf), "%02d", n->timepicker.minute);

    CogitoColor active_text = sel;
    CogitoColor inactive_text = text;
    int htw = cogito_text_width_size_node(n, hbuf, time_size);
    int mtw = cogito_text_width_size_node(n, mbuf, time_size);
    int tth = cogito_text_height_size(time_size);
    cogito_draw_text_size_node(n, ly.hour_x + (ly.hour_w - htw) / 2,
                               ly.hour_y + (ly.hour_h - tth) / 2,
                               hbuf, mode == COGITO_TIMEPICKER_MODE_HOUR ? active_text : inactive_text,
                               time_size, false);
    cogito_draw_text_size_node(n, ly.minute_x + (ly.minute_w - mtw) / 2,
                               ly.minute_y + (ly.minute_h - tth) / 2,
                               mbuf, mode == COGITO_TIMEPICKER_MODE_MINUTE ? active_text : inactive_text,
                               time_size, false);

    int colon_w = cogito_text_width_size_node(n, ":", time_size);
    cogito_draw_text_size_node(n, ly.colon_x - colon_w / 2,
                               ly.colon_y - tth / 2,
                               ":", text, time_size, true);

    CogitoColor seg_border = tertiary;
    if (seg_border.a > 190) seg_border.a = 190;
    CogitoColor am_bg = is_pm ? box_bg : tertiary_container;
    CogitoColor pm_bg = is_pm ? tertiary_container : box_bg;
    cogito_draw_rect(ly.ampm_x, ly.ampm_y, ly.ampm_w, ly.ampm_h, box_bg, 10);
    cogito_draw_rect(ly.ampm_x, ly.ampm_y, ly.ampm_w, ly.ampm_split_h, am_bg, 10);
    cogito_draw_rect(ly.ampm_x, ly.ampm_y + ly.ampm_split_h, ly.ampm_w, ly.ampm_h - ly.ampm_split_h, pm_bg, 10);
    cogito_draw_rect_lines(ly.ampm_x, ly.ampm_y, ly.ampm_w, ly.ampm_h, seg_border, 10, 1);
    cogito_draw_line(ly.ampm_x, ly.ampm_y + ly.ampm_split_h,
                     ly.ampm_x + ly.ampm_w, ly.ampm_y + ly.ampm_split_h, seg_border);

    int am_w = cogito_text_width_size_node(n, "AM", header_size + 4);
    int pm_w = cogito_text_width_size_node(n, "PM", header_size + 4);
    int am_th = cogito_text_height_size(header_size + 4);
    cogito_draw_text_size_node(n, ly.ampm_x + (ly.ampm_w - am_w) / 2,
                               ly.ampm_y + (ly.ampm_split_h - am_th) / 2,
                               "AM", !is_pm ? tertiary : inactive_text, header_size + 4, true);
    cogito_draw_text_size_node(n, ly.ampm_x + (ly.ampm_w - pm_w) / 2,
                               ly.ampm_y + ly.ampm_split_h + ((ly.ampm_h - ly.ampm_split_h) - am_th) / 2,
                               "PM", is_pm ? tertiary : inactive_text, header_size + 4, true);

    if (n->timepicker.dial_visible) {
      CogitoColor dial_bg = box_bg;
      CogitoColor dial_tint = sel;
      dial_tint.a = 26;
      cogito_draw_rect(ly.dial_x, ly.dial_y, ly.dial_d, ly.dial_d, dial_bg, ly.dial_r);
      cogito_draw_rect(ly.dial_x, ly.dial_y, ly.dial_d, ly.dial_d, dial_tint, ly.dial_r);

      for (int i = 0; i < 12; i++) {
        int value = 0;
        char lbuf[8];
        if (mode == COGITO_TIMEPICKER_MODE_HOUR) {
          value = i + 1;
          snprintf(lbuf, sizeof(lbuf), "%d", value);
        } else {
          value = i * 5;
          snprintf(lbuf, sizeof(lbuf), "%02d", value);
        }
        int px = 0, py = 0;
        cogito_timepicker_panel_value_point(&ly, mode, value, &px, &py);
        int tw = cogito_text_width_size_node(n, lbuf, dial_size);
        int th = cogito_text_height_size(dial_size);
        cogito_draw_text_size_node(n, px - tw / 2, py - th / 2, lbuf, text, dial_size, false);
      }

      int selected_value = (mode == COGITO_TIMEPICKER_MODE_MINUTE)
        ? n->timepicker.minute
        : cogito_timepicker_hour12_from_24(n->timepicker.hour);
      int hand_x = 0, hand_y = 0;
      cogito_timepicker_panel_value_point(&ly, mode, selected_value, &hand_x, &hand_y);
      CogitoColor hand = sel;
      hand.a = 220;
      cogito_draw_line(ly.dial_cx, ly.dial_cy, hand_x, hand_y, hand);
      cogito_draw_rect(ly.dial_cx - 4, ly.dial_cy - 4, 8, 8, hand, 4);
      cogito_draw_rect(hand_x - 24, hand_y - 24, 48, 48, sel, 24);

      char sbuf[8];
      if (mode == COGITO_TIMEPICKER_MODE_MINUTE) snprintf(sbuf, sizeof(sbuf), "%02d", n->timepicker.minute);
      else snprintf(sbuf, sizeof(sbuf), "%d", cogito_timepicker_hour12_from_24(n->timepicker.hour));
      int sw = cogito_text_width_size_node(n, sbuf, header_size + 6);
      int sh = cogito_text_height_size(header_size + 6);
      cogito_draw_text_size_node(n, hand_x - sw / 2, hand_y - sh / 2, sbuf, cogito_on_color(sel), header_size + 6, false);
    }

    bool over_cancel = cogito_hit_rect(mx, my, ly.cancel_x, ly.cancel_y, ly.cancel_w, ly.cancel_h);
    bool over_ok = cogito_hit_rect(mx, my, ly.ok_x, ly.ok_y, ly.ok_w, ly.ok_h);
    if (over_cancel || over_ok) {
      CogitoColor hl = sel;
      hl.a = down ? 80 : 44;
      if (over_cancel) cogito_draw_rect(ly.cancel_x, ly.cancel_y, ly.cancel_w, ly.cancel_h, hl, 16);
      if (over_ok) cogito_draw_rect(ly.ok_x, ly.ok_y, ly.ok_w, ly.ok_h, hl, 16);
    }

    CogitoColor action = sel;
    int cw = cogito_text_width_size_node(n, "Cancel", footer_size + 2);
    int ow = cogito_text_width_size_node(n, "OK", footer_size + 2);
    int ath = cogito_text_height_size(footer_size + 2);
    cogito_draw_text_size_node(n, ly.cancel_x + (ly.cancel_w - cw) / 2,
                               ly.cancel_y + (ly.cancel_h - ath) / 2,
                               "Cancel", action, footer_size + 2, true);
    cogito_draw_text_size_node(n, ly.ok_x + (ly.ok_w - ow) / 2,
                               ly.ok_y + (ly.ok_h - ath) / 2,
                               "OK", action, footer_size + 2, true);

    bool over_key_toggle = cogito_hit_rect(mx, my, ly.keyboard_x, ly.keyboard_y, ly.keyboard_w, ly.keyboard_h);
    bool key_down = over_key_toggle && down;
    CogitoStyle key_style = cogito_timepicker_iconbtn_style(over_key_toggle, key_down);
    CogitoColor key_bg = key_style.has_bg ? key_style.bg : bg;
    CogitoColor key_border = key_style.has_border ? key_style.border : border;
    int key_border_w = key_style.has_border_width ? key_style.border_width : 0;
    int key_radius = key_style.has_radius ? key_style.radius : (ly.keyboard_w / 2);
    CogitoColor key_col = key_style.has_text ? key_style.text : text;
    cogito_draw_rect(ly.keyboard_x, ly.keyboard_y, ly.keyboard_w, ly.keyboard_h, key_bg, key_radius);
    if (key_border_w > 0) {
      cogito_draw_rect_lines(ly.keyboard_x, ly.keyboard_y, ly.keyboard_w, ly.keyboard_h,
                             key_border, key_radius, key_border_w);
    }
    int key_icon = cogito_timepicker_iconbtn_icon_size(&key_style, ly.keyboard_w);
    int key_icon_x = ly.keyboard_x + (ly.keyboard_w - key_icon) / 2;
    int key_icon_y = ly.keyboard_y + (ly.keyboard_h - key_icon) / 2;
    cogito_draw_icon_fallback(n, n->timepicker.dial_visible ? "sf:keyboard" : "sf:clock",
                              key_icon_x, key_icon_y, key_icon, key_col);
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

  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
  }

  const char* label = (n->subtitle && n->subtitle->data && n->subtitle->data[0]) ? n->subtitle->data : "Select time";
  char value[32];
  cogito_timepicker_format_display(n->timepicker.hour, n->timepicker.minute, true, value, sizeof(value));

  int size = cogito_node_font_size(n);
  int label_size = size - 2;
  if (label_size < 11) label_size = 11;
  int value_size = size + 2;
  if (value_size < 16) value_size = 16;

  CogitoColor label_col = text;
  label_col.a = 160;
  int pad_x = 16;
  int icon_sz = 18;
  int icon_box_w = icon_sz + 12;
  int text_w = n->w - pad_x * 2 - icon_box_w;
  if (text_w < 24) text_w = 24;

  cogito_draw_text_ellipsis(n, n->x + pad_x, n->y + 8, text_w, label, label_col, label_size, false, 0);

  int vth = cogito_text_height_size(value_size);
  int value_y = n->y + n->h - vth - 8;
  cogito_draw_text_ellipsis(n, n->x + pad_x, value_y, text_w, value, text, value_size, false, 0);

  int icon_x = n->x + n->w - pad_x - icon_sz;
  int icon_y = n->y + (n->h - icon_sz) / 2;
  CogitoColor icon_col = sel;
  if (!open) {
    icon_col = text;
    icon_col.a = 190;
  }
  cogito_draw_icon_fallback(n, "sf:clock", icon_x, icon_y, icon_sz, icon_col);
  break;
}
