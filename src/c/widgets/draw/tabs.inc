case COGITO_TABS: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoStyle ind_s = cogito_tabs_indicator_style();
      CogitoColor bg = n->bg;
      if (s.has_bg) bg = s.bg;
      // Cogito: tab label — Title Small, 14sp
      CogitoColor text = s.has_text ? s.text : n->text_color;
      CogitoColor sel  = ind_s.has_bg ? ind_s.bg : (s.has_selection ? s.selection : n->selection_color);
      cogito_anim_apply(n, &s, &bg, &text, NULL, &sel);
      if (n->bg_set || s.has_bg) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, n->border_radius);
  }
      if (n->tab_len == 0) break;

      int slot_count = (int)n->tab_len;
      if (slot_count < 1) slot_count = 1;
      bool tabs_rtl = cogito_is_rtl();
      const int icon_size  = 24;
      const int icon_gap   = 4;
      // Cogito: active indicator — 3dp height, Full (1.5dp = 2dp) radius
      int indicator_h      = ind_s.has_min_h ? ind_s.min_h : 3;
      int indicator_radius = ind_s.has_radius ? ind_s.radius : 2;
      // Cogito Title Small = 14sp
      int size    = 14;
      int label_h = cogito_text_height_size(size);
      int indicator_target_w = 0;

      for (size_t i = 0; i < n->tab_len; i++) {
        int vi = tabs_rtl ? (int)(n->tab_len - 1 - i) : (int)i;
        int sx0 = n->x + (n->w * vi) / slot_count;
        int sx1 = n->x + (n->w * (vi + 1)) / slot_count;
        int sw  = sx1 - sx0;
        if (sw < 1) continue;

        const char* t    = n->tab_labels[i
    ] ? n->tab_labels[i
    ]->data : "";
        const char* icon = NULL;
        if (n->tab_icons && n->tab_icons[i
    ] && n->tab_icons[i
    ]->data && n->tab_icons[i
    ]->data[
      0
    ]) {
          icon = n->tab_icons[i
      ]->data;
    }
        bool has_icon    = icon != NULL;
        int block_h      = label_h + (has_icon ? (icon_size + icon_gap) : 0);
        int content_top  = n->y + (n->h - block_h) / 2;
        int label_y      = content_top + (has_icon ? (icon_size + icon_gap) : 0);
        bool is_sel      = (n->selected == (int)i);
        CogitoColor item_col = is_sel ? sel : text;

        if (has_icon) {
          int icon_x = sx0 + (sw - icon_size) / 2;
          cogito_draw_icon_fallback(n, icon, icon_x, content_top, icon_size, item_col);
    }

        int tw = cogito_text_width_size_node(n, t, size);
        int tx = sx0 + (sw - tw) / 2;
        cogito_draw_text_size_node(n, tx, label_y, t, item_col, size, is_sel, false);

        if (is_sel) {
          int inset = 2;
          if (tw <= inset * 2) inset = 0;
          int ind_w = tw - inset * 2;
          if (ind_w < 1) ind_w = 1;
          indicator_target_w = ind_w;
    }
  }
  // ---- Spring-driven indicator slide (semantic progress like FAB menu) ----
      if (n->selected >= 0 && (size_t)n->selected < n->tab_len && indicator_target_w > 0) {
        int indicator_y = n->y + n->h - indicator_h;
        double dt = cogito_spring_dt;

        // Use spring_x to store visual selected tab index (semantic progress)
        // spring_y stores visual width progress (0.0 = no width, 1.0 = full width)
        double target_idx = (double)n->selected;
        bool tab_changed = (n->tabs_anim_target != (float)target_idx);
        if (tab_changed) {
          // Snap to previous tab index, then animate to new one
          cogito_spring_snap(&n->spring_x, (double)n->tabs_anim_target);
          n->tabs_anim_target = (float)target_idx;
        }

        // Tick spring: Spatial Default for tab indicator (k=360, z=0.8)
        cogito_spring_tick(&n->spring_x, target_idx, 360.0, 0.8, dt);

        // Compute actual indicator position from visual tab index
        float visual_idx = (float)n->spring_x.x;
        int from_tab = (int)floorf(visual_idx);
        int to_tab = (int)ceilf(visual_idx);
        float t = visual_idx - from_tab; // 0.0 to 1.0 between tabs

        // Get positions for from_tab and to_tab
        int from_x = 0, from_w = 0, to_x = 0, to_w = 0;
        for (size_t i = 0; i < n->tab_len; i++) {
          int vi = tabs_rtl ? (int)(n->tab_len - 1 - i) : (int)i;
          int sx0 = n->x + (n->w * vi) / slot_count;
          int sx1 = n->x + (n->w * (vi + 1)) / slot_count;
          int sw = sx1 - sx0;
          if (sw < 1) continue;

          const char* tab_label = n->tab_labels[i] ? n->tab_labels[i]->data : "";
          int tw = cogito_text_width_size_node(n, tab_label, size);
          int inset = 2;
          if (tw <= inset * 2) inset = 0;
          int ind_w = tw - inset * 2;
          if (ind_w < 1) ind_w = 1;
          int tx = sx0 + (sw - tw) / 2;
          int ind_x = tx + inset;

          if ((int)i == from_tab) { from_x = ind_x; from_w = ind_w; }
          if ((int)i == to_tab) { to_x = ind_x; to_w = ind_w; }
        }

        // Interpolate between tabs
        int ind_x = (int)(from_x + t * (to_x - from_x) + 0.5f);
        int ind_w = (int)(from_w + t * (to_w - from_w) + 0.5f);

        if (ind_w < 1) ind_w = 1;
        if (ind_x < n->x) ind_x = n->x;
        if (ind_x + ind_w > n->x + n->w) ind_x = n->x + n->w - ind_w;
        cogito_draw_rect_radii_aa(ind_x, indicator_y, ind_w, indicator_h, sel,
                                  indicator_radius, indicator_radius, 0, 0);

        if (cogito_spring_active(&n->spring_x, target_idx)) {
          double end_time = cogito_now() + 0.6;
          if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
        }
      }
      break;
}
