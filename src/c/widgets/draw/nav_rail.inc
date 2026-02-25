case COGITO_NAV_RAIL: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int rail_w = n->w > 0 ? n->w : COGITO_NAV_RAIL_COLLAPSED_WIDTH;
  int mx = 0, my = 0;
  (void)cogito_pointer_query(&mx, &my, NULL, NULL);
  // Draw background
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  cogito_draw_rect(n->x, n->y, rail_w, n->h, bg,
  0);

  CogitoNode* fab = NULL;
  for (size_t i = 0; i < n->len; i++) {
    if (n->children[i
    ] && n->children[i
    ]->kind == COGITO_FAB) {
      fab = n->children[i
      ];
      break;
    }
  }

  CogitoColor base_color = s.has_text ? s.text : n->text_color;
  if (n->nav_rail.show_toggle) {
    int tx = 0, ty = 0, tw = 0, th = 0;
    cogito_nav_rail_toggle_rect(n, &tx, &ty, &tw, &th);
    int toggle_icon_x = tx + (tw - COGITO_NAV_RAIL_SHARED_TOGGLE_ICON_SIZE) / 2;
    int toggle_icon_y = ty + (th - COGITO_NAV_RAIL_SHARED_TOGGLE_ICON_SIZE) / 2;
    int toggle_icon_size = COGITO_NAV_RAIL_SHARED_TOGGLE_ICON_SIZE;
    int line_left = toggle_icon_x + 4;
    int line_right = toggle_icon_x + toggle_icon_size - 4;
    int line_top = toggle_icon_y + 6;
    int line_mid = toggle_icon_y + toggle_icon_size / 2;
    int line_bot = toggle_icon_y + toggle_icon_size - 6;
    CogitoColor icon_stroke = base_color;

    if (cogito_backend && cogito_backend->draw_line) {
      cogito_backend->draw_line(line_left, line_top, line_right, line_top, icon_stroke,
      2);
      cogito_backend->draw_line(line_left, line_mid, line_right, line_mid, icon_stroke,
      2);
      cogito_backend->draw_line(line_left, line_bot, line_right, line_bot, icon_stroke,
      2);
    } else {
      cogito_draw_line(line_left, line_top, line_right, line_top, icon_stroke);
      cogito_draw_line(line_left, line_mid, line_right, line_mid, icon_stroke);
      cogito_draw_line(line_left, line_bot, line_right, line_bot, icon_stroke);
    }

    if (n->nav_rail.expanded) {
      int arrow_tip_x = toggle_icon_x + toggle_icon_size - 11;
      int arrow_back_x = toggle_icon_x + toggle_icon_size - 4;
      int arrow_top_y = line_mid - 4;
      int arrow_bot_y = line_mid + 4;
      CogitoColor toggle_bg = s.has_bg ? s.bg : n->bg;

      // Mask line intersections under the arrow so the arrow appears to cut through.
      int erase_x = arrow_tip_x - 1;
      int erase_w = (line_right - erase_x) + 1;
      if (erase_w < 1) erase_w = 1;
      cogito_draw_rect(erase_x, toggle_icon_y + 3, erase_w, toggle_icon_size - 6, toggle_bg,
      0);

      if (cogito_backend && cogito_backend->draw_line) {
        cogito_backend->draw_line(arrow_back_x, arrow_top_y, arrow_tip_x, line_mid, icon_stroke,
        2);
        cogito_backend->draw_line(arrow_tip_x, line_mid, arrow_back_x, arrow_bot_y, icon_stroke,
        2);
      } else {
        cogito_draw_line(arrow_back_x, arrow_top_y, arrow_tip_x, line_mid, icon_stroke);
        cogito_draw_line(arrow_tip_x, line_mid, arrow_back_x, arrow_bot_y, icon_stroke);
      }
    }
  }

  int icon_size = COGITO_NAV_RAIL_SHARED_ITEM_ICON_SIZE;
  int item_step = n->nav_rail.expanded ? COGITO_NAV_RAIL_EXPANDED_ITEM_STEP : COGITO_NAV_RAIL_COLLAPSED_ITEM_STEP;
  int item_y = cogito_nav_rail_items_start_y(n);
  int expanded_lane_w = rail_w - (COGITO_NAV_RAIL_EXPANDED_SIDE_PADDING * 2);
  if (expanded_lane_w < COGITO_NAV_RAIL_EXPANDED_INDICATOR_MIN_WIDTH) {
    expanded_lane_w = COGITO_NAV_RAIL_EXPANDED_INDICATOR_MIN_WIDTH;
  }
  int expanded_lane_x = n->x + (rail_w - expanded_lane_w) / 2;
  int expanded_content_x = expanded_lane_x + COGITO_NAV_RAIL_EXPANDED_CONTENT_INSET;

  for (size_t i = 0; i < n->nav_rail.len; i++) {
    bool selected = (int)i == n->nav_rail.selected;
    bool hover = cogito_hit_rect(mx, my, n->x, item_y, rail_w, item_step);
    CogitoColor hover_tint = s.has_text ? s.text : n->text_color;
    CogitoColor item_bg = selected
      ? (s.has_selection ? s.selection : n->selection_color)
      : (s.has_bg ? cogito_mix(s.bg, hover_tint,
    0.16f) : n->bg);
    CogitoColor content_color = base_color;
    if (selected) {
      CogitoColor sel_bg = s.has_selection ? s.selection : n->selection_color;
      content_color = cogito_on_color(sel_bg);
    }

    if (n->nav_rail.expanded) {
      int indicator_h = COGITO_NAV_RAIL_EXPANDED_INDICATOR_HEIGHT;
      int bg_y = item_y + (item_step - indicator_h) / 2;

      bool has_icon = (n->nav_rail.icons && n->nav_rail.icons[i
      ]);
      bool has_label = (n->nav_rail.labels && n->nav_rail.labels[i
      ]);
      int label_size = 16;
      int label_w = 0;
      if (has_label) {
        label_w = cogito_text_width_size_node(n, n->nav_rail.labels[i
        ]->data, label_size);
      }

      int body_w = 0;
      if (has_icon) body_w += icon_size;
      if (has_icon && has_label) body_w += COGITO_NAV_RAIL_EXPANDED_LABEL_GAP;
      if (has_label) body_w += label_w;
      if (body_w < icon_size) body_w = icon_size;

      int indicator_w = (COGITO_NAV_RAIL_EXPANDED_CONTENT_INSET * 2) + body_w;
      if (indicator_w < COGITO_NAV_RAIL_EXPANDED_INDICATOR_MIN_WIDTH) {
        indicator_w = COGITO_NAV_RAIL_EXPANDED_INDICATOR_MIN_WIDTH;
      }
      if (indicator_w > expanded_lane_w) indicator_w = expanded_lane_w;
      int bg_x = expanded_lane_x;
      if (selected || hover) {
        cogito_draw_rect(bg_x, bg_y, indicator_w, indicator_h, item_bg,
        99);
      }
      int content_x = expanded_content_x;
      if (has_icon) {
        int badge_value = (n->nav_rail.badges ? n->nav_rail.badges[i
        ] : 0);
        CogitoColor badge_bg = cogito_rgba(220,
        38,
        38,
        255);
        CogitoColor badge_fg = cogito_rgba(255,
        255,
        255,
        255);
        int icon_y = bg_y + (indicator_h - icon_size) / 2;
        int icon_x = content_x;
        cogito_draw_icon_fallback(n, n->nav_rail.icons[i
        ]->data, icon_x, icon_y, icon_size, content_color);
        if (badge_value != 0) {
          int anchor_x = icon_x + icon_size;
          int anchor_y = icon_y;
          if (badge_value < 0) {
            int d = 8;
            int bx = anchor_x - d / 2 + 2;
            int by = anchor_y - d / 2 + 2;
            cogito_draw_rect(bx, by, d, d, badge_bg, d / 2);
          } else {
            char buf[
              8
            ];
            if (badge_value > 99) snprintf(buf, sizeof(buf),
            "99+");
            else snprintf(buf, sizeof(buf),
            "%d", badge_value);
            int text_size = 10;
            int tw = cogito_text_width_size_node(n, buf, text_size);
            int bh = 14;
            int bw = tw + 8;
            if (bw < 14) bw = 14;
            int bx = anchor_x - bw / 2 + 2;
            int by = anchor_y - bh / 2 + 2;
            cogito_draw_rect(bx, by, bw, bh, badge_bg, bh / 2);
            int tx = bx + (bw - tw) / 2;
            int ty = by + (bh - cogito_text_height_size(text_size)) / 2;
            cogito_draw_text_size_node(n, tx, ty, buf, badge_fg, text_size, true);
          }
        }
        content_x += icon_size + COGITO_NAV_RAIL_EXPANDED_LABEL_GAP;
      }
      if (has_label) {
        int label_y = bg_y + (indicator_h - cogito_text_height_size(label_size)) / 2;
        cogito_draw_text_size_node(n, content_x, label_y, n->nav_rail.labels[i
        ]->data, content_color, label_size, false);
      }

      item_y += item_step;
    } else {
      int icon_x = n->x + (rail_w - icon_size) / 2;
      int icon_y = item_y + COGITO_NAV_RAIL_COLLAPSED_ICON_Y_OFFSET;
      int bg_x = n->x + (rail_w - COGITO_NAV_RAIL_COLLAPSED_INDICATOR_WIDTH) / 2;
      int bg_y = item_y;
      if (selected || hover) {
        cogito_draw_rect(
          bg_x,
          bg_y,
          COGITO_NAV_RAIL_COLLAPSED_INDICATOR_WIDTH,
          COGITO_NAV_RAIL_COLLAPSED_INDICATOR_HEIGHT,
          item_bg,
        99
        );
      }
      if (n->nav_rail.icons && n->nav_rail.icons[i
      ]) {
        int badge_value = (n->nav_rail.badges ? n->nav_rail.badges[i
        ] : 0);
        CogitoColor badge_bg = cogito_rgba(220,
        38,
        38,
        255);
        CogitoColor badge_fg = cogito_rgba(255,
        255,
        255,
        255);
        cogito_draw_icon_fallback(n, n->nav_rail.icons[i
        ]->data, icon_x, icon_y, icon_size, content_color);
        if (badge_value != 0) {
          int anchor_x = icon_x + icon_size;
          int anchor_y = icon_y;
          if (badge_value < 0) {
            int d = 8;
            int bx = anchor_x - d / 2 + 2;
            int by = anchor_y - d / 2 + 2;
            cogito_draw_rect(bx, by, d, d, badge_bg, d / 2);
          } else {
            char buf[
              8
            ];
            if (badge_value > 99) snprintf(buf, sizeof(buf),
            "99+");
            else snprintf(buf, sizeof(buf),
            "%d", badge_value);
            int text_size = 10;
            int tw = cogito_text_width_size_node(n, buf, text_size);
            int bh = 14;
            int bw = tw + 8;
            if (bw < 14) bw = 14;
            int bx = anchor_x - bw / 2 + 2;
            int by = anchor_y - bh / 2 + 2;
            cogito_draw_rect(bx, by, bw, bh, badge_bg, bh / 2);
            int tx = bx + (bw - tw) / 2;
            int ty = by + (bh - cogito_text_height_size(text_size)) / 2;
            cogito_draw_text_size_node(n, tx, ty, buf, badge_fg, text_size, true);
          }
        }
      }
      if (n->nav_rail.labels && n->nav_rail.labels[i
      ]) {
        // Cogito: Collapsed nav rail label — Label S = 12sp
        int size = 12;
        CogitoColor label_color = base_color;
        int label_w = cogito_text_width_size_node(n, n->nav_rail.labels[i
        ]->data, size);
        int label_x = n->x + (rail_w - label_w) / 2;
        int label_y = item_y + COGITO_NAV_RAIL_COLLAPSED_INDICATOR_HEIGHT + COGITO_NAV_RAIL_COLLAPSED_INDICATOR_LABEL_GAP;
        cogito_draw_text_size_node(n, label_x, label_y, n->nav_rail.labels[i
        ]->data, label_color, size, selected);
      }

      item_y += item_step;
    }
  }
  if (fab) cogito_draw_node(fab);
  break;
}
