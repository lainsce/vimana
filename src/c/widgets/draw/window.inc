case COGITO_WINDOW: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s_win = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg_target = s_win.has_bg ? s_win.bg : n->bg;

      bool reveal_active = false;
      float reveal_t = 1.0f;
      double now = cogito_now();
      if (n->window_theme_reveal_active && n->window_theme_reveal_duration > 0.0 &&
          n->window_theme_reveal_max_radius > 0.0f) {
        double t = (now - n->window_theme_reveal_start) / n->window_theme_reveal_duration;
        if (t >= 1.0) {
          n->window_theme_reveal_active = false;
          n->window_theme_reveal_radius = 0.0f;
        } else if (t >= 0.0) {
          float tf = (float)t;
          float eased = 1.0f - (1.0f - tf) * (1.0f - tf) * (1.0f - tf);
          n->window_theme_reveal_radius = eased * n->window_theme_reveal_max_radius;
          reveal_t = tf;
          reveal_active = true;
          double end_time = now + 0.032;
          if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
        }
      }
      CogitoColor bg = bg_target;
      cogito_anim_apply(n, &s_win, &bg, NULL, NULL, NULL);
      cogito_draw_rect(n->x, n->y, n->w, n->h, bg, 0);
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      if (reveal_active) {
        float overlay_fade = 1.0f - reveal_t;
        int overlay_alpha_i = (int)roundf(120.0f * overlay_fade);
        if (overlay_alpha_i > 0) {
          if (overlay_alpha_i > 255) overlay_alpha_i = 255;
          CogitoColor from = n->window_theme_reveal_old_bg;
          CogitoColor to = n->window_theme_reveal_new_bg;
          from.a = (uint8_t)overlay_alpha_i;
          to.a = (uint8_t)overlay_alpha_i;
          cogito_draw_rect(n->x, n->y, n->w, n->h, from, 0);
          cogito_draw_circle_clipped(n->window_theme_reveal_origin_x,
                                     n->window_theme_reveal_origin_y,
                                     n->window_theme_reveal_radius,
                                     n->x, n->y, n->w, n->h,
                                     to);
        }
      }
      if (n->kind == COGITO_WINDOW && n->dialog) {
        if (n->dialog->kind == COGITO_DIALOG_SLOT) {
          cogito_dialog_slot_force_draw = true;
          cogito_draw_node(n->dialog);
          cogito_dialog_slot_force_draw = false;
        } else if (n->dialog_open) {
          bool hover = false, active = false;
          cogito_node_hover_state(n, &hover, &active);
          CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
          CogitoColor scrim = s.has_bg ? s.bg : n->bg;
          cogito_draw_rect(0, 0, n->w, n->h, scrim, 0);
          cogito_draw_node(n->dialog);
        }
      }
      // Draw popover on top if open
      if (n->kind == COGITO_WINDOW && n->popover_open && n->popover) {
        cogito_draw_node(n->popover);
      }
      // Draw side sheet on top if open
      if (n->kind == COGITO_WINDOW && n->side_sheet_open && n->side_sheet_node) {
        // Skip scrim for sidebar mode - it's docked next to overlay
        if (n->side_sheet_node->side_sheet.mode != COGITO_SIDE_SHEET_SIDEBAR) {
            // Draw scrim for modal/inset modes
            bool hover = false, active = false;
            cogito_node_hover_state(n, &hover, &active);
            CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
            CogitoColor scrim = s.has_bg ? s.bg : n->bg;
            scrim.a = 128;
            cogito_draw_rect(0, 0, n->w, n->h, scrim, 0);
        }
        cogito_draw_node(n->side_sheet_node);
      }
      // Draw toasts in a final overlay pass so they are never clipped by containers.
      cogito_draw_toasts_overlay(n);
      // Draw FAB Menus in a final overlay pass so they always float above all content.
      cogito_draw_fab_overlay(n);
      break;
}
