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
  CogitoStyle badge_style = cogito_theme_resolve(COGITO_BADGE);
  CogitoColor badge_bg = badge_style.has_bg
    ? badge_style.bg
    : (cogito_cached_error.a
      ? cogito_cached_error
      : (s.has_selection ? s.selection : n->selection_color));
  CogitoColor badge_fg = badge_style.has_text
    ? badge_style.text
    : (cogito_cached_on_error.a
      ? cogito_cached_on_error
      : cogito_on_color(badge_bg));
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
      int arrow_tip_x, arrow_back_x;
      if (cogito_is_rtl()) {
        // RTL: arrow points right (collapse toward right wall)
        arrow_tip_x = toggle_icon_x + 11;
        arrow_back_x = toggle_icon_x + 4;
      } else {
        // LTR: arrow points left (collapse toward left wall)
        arrow_tip_x = toggle_icon_x + toggle_icon_size - 11;
        arrow_back_x = toggle_icon_x + toggle_icon_size - 4;
      }
      int arrow_top_y = line_mid - 4;
      int arrow_bot_y = line_mid + 4;
      CogitoColor toggle_bg = s.has_bg ? s.bg : n->bg;

      // Mask line intersections under the arrow so the arrow appears to cut through.
      int erase_x = cogito_is_rtl() ? arrow_back_x - 1 : (arrow_tip_x - 1);
      int erase_end = cogito_is_rtl() ? (arrow_tip_x + 1) : (line_right + 1);
      int erase_w = erase_end - erase_x;
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

  // ---- Selection indicator animation using per-item springs ----
  // Selected item: expand by 15% (to 1.15) then settle to 1.0
  // Deselected item: go straight to 0 while vanishing
  double dt = cogito_spring_dt;
  bool any_anim_active = false;

  for (size_t i = 0; i < n->nav_rail.len; i++) {
    if (!n->nav_rail.item_springs) break;
    bool selected = (int)i == n->nav_rail.selected;
    // Target: 1.0 for selected (normal width)
    // Target: 0.0 for deselected (vanished)
    double target = selected ? 1.0 : 0.0;
    CogitoSpring* spring = &n->nav_rail.item_springs[i];

    // On selection change, snap to starting position for animation
    bool was_selected = (spring->target >= 0.5);
    if (was_selected != selected) {
      // Selected: start at 1.15 (15% expansion), animate to 1.0
      // Deselected: start at 1.0 (normal), animate to 0.0
      cogito_spring_snap(spring, selected ? 1.15 : 1.0);
    }
    spring->target = target;

    // Tick spring: Default spatial for nav rail indicator (k=360, z=0.8)
    double k = 360.0;
    double z = 0.8;
    cogito_spring_tick(spring, target, k, z, dt);

    if (cogito_spring_active(spring, target)) {
      any_anim_active = true;
    }
  }

  if (any_anim_active) {
    double end_time = cogito_now() + 0.6;
    if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    cogito_window_relayout(cogito_node_window(n));
  }

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

      // Per-item hover state layer
      if (hover && !selected) {
        CogitoColor state = cogito_node_state_layer_seed(n);
        state.a = 20;
        cogito_draw_rect(expanded_lane_x, bg_y, expanded_lane_w, indicator_h, state, 99);
      }

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

      // Apply spring animation to indicator width
      float scale = n->nav_rail.item_springs ? (float)n->nav_rail.item_springs[i].x : (selected ? 1.0f : 0.0f);
      if (scale < 0.0f) scale = 0.0f;
      if (scale > 1.5f) scale = 1.5f;

      // Scale width: spring value is the scale multiplier
      int animated_w = (int)(indicator_w * scale + 0.5f);
      if (animated_w < 0) animated_w = 0;
      int bg_x = expanded_lane_x + (indicator_w - animated_w) / 2; // Center the expanded indicator

      if (scale > 0.01f || selected || hover) {
        // Fade out opacity as it shrinks below 0.85
        uint8_t alpha = (uint8_t)(255 * (scale < 0.85f ? scale / 0.85f : 1.0f));
        CogitoColor animated_bg = item_bg;
        animated_bg.a = (uint8_t)(animated_bg.a * alpha / 255);
        if (animated_bg.a > 10) {
          cogito_draw_rect(bg_x, bg_y, animated_w, indicator_h, animated_bg, 99);
        }
      }
      int content_x = expanded_content_x;
      bool rail_rtl = cogito_is_rtl();
      if (rail_rtl) {
        // RTL: start content from right edge of lane
        content_x = expanded_lane_x + expanded_lane_w - COGITO_NAV_RAIL_EXPANDED_CONTENT_INSET;
      }
      if (has_icon) {
        int badge_value = (n->nav_rail.badges ? n->nav_rail.badges[i
        ] : 0);
        int icon_y = bg_y + (indicator_h - icon_size) / 2;
        int icon_x = rail_rtl ? (content_x - icon_size) : content_x;
        cogito_draw_icon_fallback(n, n->nav_rail.icons[i
        ]->data, icon_x, icon_y, icon_size, content_color);
        if (badge_value != 0) {
          int anchor_x = rail_rtl ? icon_x : (icon_x + icon_size);
          int anchor_y = icon_y;
          if (badge_value < 0) {
            int d = 8;
            int bx = anchor_x - d / 2 + (rail_rtl ? -2 : 2);
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
            cogito_draw_text_size_node(n, tx, ty, buf, badge_fg, text_size, true, false);
          }
        }
        content_x += rail_rtl ? -(icon_size + COGITO_NAV_RAIL_EXPANDED_LABEL_GAP) : (icon_size + COGITO_NAV_RAIL_EXPANDED_LABEL_GAP);
      }
      if (has_label) {
        int label_y = bg_y + (indicator_h - cogito_text_height_size(label_size)) / 2;
        int lx = rail_rtl ? (content_x - label_w) : content_x;
        cogito_draw_text_size_node(n, lx, label_y, n->nav_rail.labels[i
        ]->data, content_color, label_size, false, false);
      }

      item_y += item_step;
    } else {
      int icon_x = n->x + (rail_w - icon_size) / 2;
      int icon_y = item_y + (n->nav_rail.no_label ? COGITO_NAV_RAIL_COLLAPSED_ICON_Y_OFFSET_NO_LABEL : COGITO_NAV_RAIL_COLLAPSED_ICON_Y_OFFSET);

      // Per-item hover state layer for collapsed mode (M3)
      if (hover && !selected) {
        int base_indicator_h_hover = n->nav_rail.no_label ? COGITO_NAV_RAIL_COLLAPSED_INDICATOR_NO_LABEL_HEIGHT : COGITO_NAV_RAIL_COLLAPSED_INDICATOR_HEIGHT;
        int base_indicator_w_hover = COGITO_NAV_RAIL_COLLAPSED_INDICATOR_WIDTH;
        int hover_x = n->x + (rail_w - base_indicator_w_hover) / 2;
        CogitoColor state = cogito_node_state_layer_seed(n);
        state.a = 20;
        cogito_draw_rect(hover_x, item_y, base_indicator_w_hover, base_indicator_h_hover, state, base_indicator_h_hover / 2);
      }

      // Apply spring animation to collapsed indicator
      float scale = n->nav_rail.item_springs ? (float)n->nav_rail.item_springs[i].x : (selected ? 1.0f : 0.0f);
      if (scale < 0.0f) scale = 0.0f;
      if (scale > 1.5f) scale = 1.5f;

      int base_indicator_w = COGITO_NAV_RAIL_COLLAPSED_INDICATOR_WIDTH;
      int base_indicator_h = n->nav_rail.no_label ? COGITO_NAV_RAIL_COLLAPSED_INDICATOR_NO_LABEL_HEIGHT : COGITO_NAV_RAIL_COLLAPSED_INDICATOR_HEIGHT;

      // Scale: spring value is the scale multiplier
      int animated_w = (int)(base_indicator_w * scale);
      int animated_h = (int)(base_indicator_h * scale);
      if (animated_w < 0) animated_w = 0;
      if (animated_h < 0) animated_h = 0;

      int bg_x = n->x + (rail_w - animated_w) / 2;
      int bg_y = item_y + (base_indicator_h - animated_h) / 2;

      if (scale > 0.01f || selected || hover) {
        // Fade out opacity as it shrinks below 0.85
        uint8_t alpha = (uint8_t)(255 * (scale < 0.85f ? scale / 0.85f : 1.0f));
        CogitoColor animated_bg = item_bg;
        animated_bg.a = (uint8_t)(animated_bg.a * alpha / 255);
        if (animated_bg.a > 10) {
          int radius = animated_h / 2;
          if (radius < 1) radius = 1;
          cogito_draw_rect(bg_x, bg_y, animated_w, animated_h, animated_bg, radius);
        }
      }
      if (n->nav_rail.icons && n->nav_rail.icons[i
      ]) {
        int badge_value = (n->nav_rail.badges ? n->nav_rail.badges[i
        ] : 0);
        cogito_draw_icon_fallback(n, n->nav_rail.icons[i
        ]->data, icon_x, icon_y, icon_size, content_color);
        if (badge_value != 0) {
          int anchor_x = cogito_is_rtl() ? icon_x : (icon_x + icon_size);
          int anchor_y = icon_y;
          if (badge_value < 0) {
            int d = 8;
            int bx = anchor_x - d / 2 + (cogito_is_rtl() ? -2 : 2);
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
            cogito_draw_text_size_node(n, tx, ty, buf, badge_fg, text_size, true, false);
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
        ]->data, label_color, size, selected, false);
      }

      item_y += item_step;
    }
  }
  // Draw vertical divider on inner edge (right in LTR, left in RTL)
  if (n->nav_rail.show_divider) {
    CogitoColor divider_color = s.has_border
      ? s.border
      : (cogito_cached_outline_variant.a
        ? cogito_cached_outline_variant
        : (cogito_cached_outline.a ? cogito_cached_outline : n->border_color));
    int divider_x = cogito_is_rtl() ? n->x : (n->x + rail_w - 1);
    cogito_draw_line(divider_x, n->y, divider_x, n->y + n->h, divider_color);
  }
  if (fab) cogito_draw_node(fab);
  break;
}
