case COGITO_BOTTOM_NAV: {
  CogitoStyle s = cogito_theme_resolve_node(n);

  // Draw background
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, 0);

  CogitoNode* fab = NULL;
  for (size_t i = 0; i < n->len; i++) {
    if (n->children[i] && n->children[i]->kind == COGITO_FAB) {
      fab = n->children[i];
      break;
    }
  }

  if (n->kd->bottom_nav.len == 0) {
    if (fab) cogito_draw_node(fab);
    break;
  }

  // Auto-detect mode: Medium if container width > 600dp, else Compact
  bool medium_mode = n->w > 600;
  n->kd->bottom_nav.medium_mode = medium_mode;

  int reserved_right = 0;
  if (fab && fab->w > 0) reserved_right = fab->w + 18;
  int items_w = n->w - reserved_right;
  if (items_w < 1) items_w = n->w;
  int item_w = items_w / (int)n->kd->bottom_nav.len;
  if (item_w < 1) item_w = 1;

  int icon_size = medium_mode ? 24 : 24;
  int label_size = medium_mode ? 14 : 12;
  int mx = 0;
  int my = 0;
  bool button_down = false;
  bool have_mouse = cogito_pointer_query(&mx, &my, &button_down, NULL);

  // ---- Selection indicator animation using spring_w ----
  double dt = cogito_spring_dt;
  bool any_anim_active = false;
  int selected_idx = n->kd->bottom_nav.selected;

  if (!n->spring_w.inited) {
    cogito_spring_snap(&n->spring_w, (double)selected_idx);
  }

  bool was_selected = (int)(n->spring_w.target + 0.5) == selected_idx;
  if (!was_selected) {
    cogito_spring_snap(&n->spring_w, n->spring_w.x);
  }
  n->spring_w.target = (double)selected_idx;
  cogito_spring_tick(&n->spring_w, (double)selected_idx, 360.0, 0.8, dt);
  float visual_idx = (float)n->spring_w.x;

  if (cogito_spring_active(&n->spring_w, (double)selected_idx)) {
    any_anim_active = true;
  }

  if (any_anim_active) {
    double end_time = cogito_now() + 0.6;
    if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    cogito_window_relayout(cogito_node_window(n));
  }

  for (size_t i = 0; i < n->kd->bottom_nav.len; i++) {
    bool selected = (int)i == n->kd->bottom_nav.selected;
    bool hover = false;
    int visual_i = cogito_is_rtl() ? (int)(n->kd->bottom_nav.len - 1 - i) : (int)i;
    int item_x = n->x + visual_i * item_w;

    if (have_mouse && cogito_hit_rect(mx, my, item_x, n->y, item_w, n->h)) {
      hover = true;
    }

    // Get colors
    CogitoColor item_bg = selected
      ? (s.has_selection ? s.selection : n->selection_color)
      : (s.has_bg ? s.bg : n->bg);
    // hover state layer: on-surface at 8% opacity
    CogitoColor hover_state = cogito_node_state_layer_seed(n);
    hover_state.a = (hover && button_down) ? 28 : 20;
    CogitoColor text_color = s.has_text ? s.text : n->text_color;
    CogitoColor icon_color = s.has_text ? s.text : n->text_color;
    if (selected) {
      CogitoColor sel_bg = s.has_selection ? s.selection : n->selection_color;
      text_color = cogito_on_color(sel_bg);
      icon_color = cogito_on_color(sel_bg);
    }

    if (medium_mode) {
      // ---- MEDIUM MODE: Icon beside label, full-height pill ----
      int pill_pad = 12;
      int content_h = n->h - pill_pad * 2;

      // Draw content-hugging pill indicator with 24dp horizontal padding
      if (selected || hover) {
        float item_idx_f = (float)i;
        float dist_from_visual = item_idx_f - visual_idx;
        float anim_scale = 1.0f - (float)fabs(dist_from_visual);
        if (anim_scale < 0.0f) anim_scale = 0.0f;
        if (anim_scale > 1.0f) anim_scale = 1.0f;

        // Calculate content width (icon + gap + label)
        bool has_icon = n->kd->bottom_nav.icons && n->kd->bottom_nav.icons[i];
        bool has_label = n->kd->bottom_nav.labels && n->kd->bottom_nav.labels[i];
        int content_w = 0;
        if (has_icon) content_w += icon_size;
        if (has_icon && has_label) content_w += 8;
        if (has_label) {
          content_w += cogito_text_width_size_node(n, n->kd->bottom_nav.labels[i]->data, label_size);
        }

        // 24dp padding on both sides
        int pill_pad_h = 24;
        int target_pill_w = content_w + pill_pad_h * 2;

        if (selected) {
          // Animate pill width
          int min_pill_w = target_pill_w / 2;
          int pill_w = min_pill_w + (int)((target_pill_w - min_pill_w) * anim_scale);
          int pill_x = item_x + (item_w - pill_w) / 2;
          int pill_h = content_h;
          int pill_y = n->y + pill_pad;

          uint8_t alpha = (uint8_t)(255 * (0.5f + 0.5f * anim_scale));
          CogitoColor animated_bg = item_bg;
          animated_bg.a = (uint8_t)(animated_bg.a * alpha / 255);

          if (animated_bg.a > 10) {
            cogito_draw_rect(pill_x, pill_y, pill_w, pill_h, animated_bg, pill_h / 2);
          }
        } else if (hover) {
          // M3 hover state layer pill
          int pill_x = item_x + (item_w - target_pill_w) / 2;
          cogito_draw_rect(pill_x, n->y + pill_pad, target_pill_w, content_h, hover_state, content_h / 2);
        }
      }

      // Calculate content layout: icon + label horizontally centered
      bool has_icon = n->kd->bottom_nav.icons && n->kd->bottom_nav.icons[i];
      bool has_label = n->kd->bottom_nav.labels && n->kd->bottom_nav.labels[i];

      int content_w = 0;
      int icon_w = has_icon ? icon_size : 0;
      int label_w = 0;
      if (has_label) {
        label_w = cogito_text_width_size_node(n, n->kd->bottom_nav.labels[i]->data, label_size);
      }

      if (has_icon && has_label) {
        content_w = icon_w + 8 + label_w; // 8dp gap between icon and label
      } else {
        content_w = icon_w > label_w ? icon_w : label_w;
      }

      int content_x = item_x + (item_w - content_w) / 2;
      int center_y = n->y + n->h / 2;

      // Draw icon
      if (has_icon) {
        int icon_y = center_y - icon_size / 2;
        cogito_draw_icon_fallback(n, n->kd->bottom_nav.icons[i]->data, content_x, icon_y, icon_size, icon_color);
        content_x += icon_w + 8;
      }

      // Draw label
      if (has_label) {
        int th = cogito_text_height_size(label_size);
        int label_y = center_y - th / 2;
        cogito_draw_text_size_node(n, content_x, label_y, n->kd->bottom_nav.labels[i]->data, text_color, label_size, false, false);
      }

    } else {
      // ---- COMPACT MODE: Icon above label, small top pill ----
      if (selected || hover) {
        float item_idx_f = (float)i;
        float dist_from_visual = item_idx_f - visual_idx;
        float anim_scale = 1.0f - (float)fabs(dist_from_visual);
        if (anim_scale < 0.0f) anim_scale = 0.0f;
        if (anim_scale > 1.0f) anim_scale = 1.0f;
        float scale_mult = 0.85f + 0.30f * anim_scale;

        int base_indicator_w = 64;
        int base_indicator_h = 32;
        int animated_w = (int)(base_indicator_w * scale_mult);
        int animated_h = (int)(base_indicator_h * scale_mult);
        int indicator_x = item_x + (item_w - animated_w) / 2;
        int indicator_y = n->y + 8 + (base_indicator_h - animated_h) / 2;

        if (selected) {
          uint8_t alpha = (uint8_t)(255 * (0.5f + 0.5f * anim_scale));
          CogitoColor animated_bg = item_bg;
          animated_bg.a = (uint8_t)(animated_bg.a * alpha / 255);
          if (animated_bg.a > 10) {
            cogito_draw_rect(indicator_x, indicator_y, animated_w, animated_h, animated_bg, animated_h / 2);
          }
        } else if (hover) {
          cogito_draw_rect(indicator_x, n->y + 8, base_indicator_w, base_indicator_h, hover_state, 16);
        }
      }

      // Draw icon centered
      if (n->kd->bottom_nav.icons && n->kd->bottom_nav.icons[i]) {
        int icon_x = item_x + (item_w - icon_size) / 2;
        int icon_y = n->y + 12;
        cogito_draw_icon_fallback(n, n->kd->bottom_nav.icons[i]->data, icon_x, icon_y, icon_size, icon_color);
      }

      // Draw label below icon
      if (n->kd->bottom_nav.labels && n->kd->bottom_nav.labels[i]) {
        int label_w = cogito_text_width_size_node(n, n->kd->bottom_nav.labels[i]->data, label_size);
        int label_x = item_x + (item_w - label_w) / 2;
        int label_y = n->y + 12 + icon_size + 4;
        cogito_draw_text_size_node(n, label_x, label_y, n->kd->bottom_nav.labels[i]->data, text_color, label_size, false, false);
      }
    }
  }
  if (fab) cogito_draw_node(fab);
  break;
}
