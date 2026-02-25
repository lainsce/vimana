case COGITO_TABS: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = n->bg;
      if (s.has_bg) bg = s.bg;
      // Cogito: tab label — Title Small, 14sp
      CogitoColor text = s.has_text ? s.text : n->text_color;
      CogitoColor sel  = s.has_selection ? s.selection : n->selection_color;
      cogito_anim_apply(n, &s, &bg, &text, NULL, &sel);
      if (n->bg_set || s.has_bg) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, n->border_radius);
  }
      if (n->tab_len == 0) break;

      const int slot_count = 4;
      const int icon_size  = 24;
      const int icon_gap   = 4;
      // Cogito: active indicator — 3dp height, Full (1.5dp = 2dp) radius
      const int indicator_h      = 3;
      const int indicator_radius = 2;
      // Cogito Title Small = 14sp
      int size    = 14;
      int label_h = cogito_text_height_size(size);
      int indicator_target_x = 0;
      int indicator_target_w = 0;

      for (size_t i = 0; i < n->tab_len; i++) {
        int sx0 = n->x + (n->w * (int)i) / slot_count;
        int sx1 = n->x + (n->w * ((int)i + 1)) / slot_count;
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
        cogito_draw_text_size_node(n, tx, label_y, t, item_col, size, is_sel);

        if (is_sel) {
          int inset = 2;
          if (tw <= inset * 2) inset = 0;
          int ind_w = tw - inset * 2;
          if (ind_w < 1) ind_w = 1;
          indicator_target_w = ind_w;
          indicator_target_x = tx + inset;
    }
  }
  // ---- Spring-driven indicator slide (Expressive: slight bounce on tab change) ----
      if (n->selected >= 0 && (size_t)n->selected < n->tab_len && indicator_target_w > 0) {
        int indicator_y = n->y + n->h - indicator_h;
        double dt = cogito_spring_dt;

        cogito_spring_tick(&n->spring_x, (double)indicator_target_x,
                           COGITO_SPRING_K_EXPRESSIVE, COGITO_SPRING_Z_EXPRESSIVE, dt);
        cogito_spring_tick(&n->spring_w, (double)indicator_target_w,
                           COGITO_SPRING_K_FUNCTIONAL, COGITO_SPRING_Z_FUNCTIONAL, dt);

        int ind_x = (int)(n->spring_x.x + 0.5);
        int ind_w = (int)(n->spring_w.x + 0.5);

        if (ind_w < 1) ind_w = 1;
        if (ind_x < n->x) ind_x = n->x;
        if (ind_x + ind_w > n->x + n->w) ind_x = n->x + n->w - ind_w;
        if (ind_x < n->x) ind_x = n->x;
        cogito_draw_rect_radii_aa(ind_x, indicator_y, ind_w, indicator_h, sel,
                                  indicator_radius, indicator_radius,
    0,
    0);

        if (cogito_spring_active(&n->spring_x, (double)indicator_target_x) ||
            cogito_spring_active(&n->spring_w, (double)indicator_target_w)) {
          double end_time = cogito_now() + 0.6;
          if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    }
  }
      break;
}