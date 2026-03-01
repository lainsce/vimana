case COGITO_SLIDER: {
      bool hover = false;
      bool active = false;
      // If slider is being dragged, force active state
      if (n->slider.dragging) {
        hover = true;
        active = true;
      } else {
        cogito_node_hover_state(n, &hover, &active);
      }
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor inactive_track = s.has_bg ? s.bg : (s.has_track ? s.track : n->bg);
      CogitoColor active_track = s.has_text ? s.text : (s.has_knob ? s.knob : n->text_color);
      cogito_anim_apply(n, &s, &inactive_track, &active_track, NULL, NULL);

      int slider_size = (int)n->slider.size;
      if (slider_size < COGITO_SLIDER_SIZE_XS || slider_size > COGITO_SLIDER_SIZE_XL) {
        slider_size = COGITO_SLIDER_SIZE_XS;
      }

      const int dot_d = 4;
      const int endpoint_pad_x = 4;
      const int endpoint_pad_y = 6;
      const int thumb_w = 6;
      int thumb_w_active = thumb_w;
      if (active || n->slider.dragging) thumb_w_active = 2;
      int rail_h = 16;
      int thumb_h = 44;
      int track_r = 8;
      if (slider_size == COGITO_SLIDER_SIZE_S) {
        rail_h = 24;
      } else if (slider_size == COGITO_SLIDER_SIZE_M) {
        rail_h = 40;
        thumb_h = 52;
        track_r = 12;
      } else if (slider_size == COGITO_SLIDER_SIZE_L) {
        rail_h = 56;
        thumb_h = 68;
        track_r = 16;
      } else if (slider_size == COGITO_SLIDER_SIZE_XL) {
        rail_h = 96;
        thumb_h = 108;
        track_r = 28;
      }
      int rail_h_min = dot_d + endpoint_pad_y * 2;
      if (rail_h < rail_h_min) rail_h = rail_h_min;
      if (track_r < 2) track_r = 2;

      int thumb_max = n->h - 4;
      if (thumb_max < rail_h + 4) thumb_max = rail_h + 4;
      if (thumb_h > thumb_max) thumb_h = thumb_max;
      int thumb_r = thumb_w_active / 2;
      if (thumb_r < 2) thumb_r = 2;
      const int gap = 6;
      int pad = rail_h / 2 + 7;
      int cy = n->y + n->h / 2;
      int x0 = n->x + pad;
      int x1 = n->x + n->w - pad;
      if (x1 < x0) x1 = x0;
      int endpoint_center_inset_x = dot_d / 2 + endpoint_pad_x;
      int rx0 = x0 - endpoint_center_inset_x;
      int rx1 = x1 + endpoint_center_inset_x;

      double t0 = 0.0;
      double t1 = 0.0;
      double span = n->slider.max - n->slider.min;
      if (span > 0.0) {
        t0 = (n->slider.value - n->slider.min) / span;
        t1 = (n->slider.value2 - n->slider.min) / span;
      }
      if (t0 < 0.0) t0 = 0.0;
      if (t0 > 1.0) t0 = 1.0;
      if (t1 < 0.0) t1 = 0.0;
      if (t1 > 1.0) t1 = 1.0;

      int k0 = x0 + (int)lround((double)(x1 - x0) * t0);
      int k1 = x0 + (int)lround((double)(x1 - x0) * t1);
      if (k1 < k0) {
        int tmp = k0;
        k0 = k1;
        k1 = tmp;
      }

      int zero_k = x0 + (x1 - x0) / 2;
      if (span > 0.0 && n->slider.min < 0.0 && n->slider.max > 0.0) {
        double tz = (0.0 - n->slider.min) / span;
        if (tz < 0.0) tz = 0.0;
        if (tz > 1.0) tz = 1.0;
        zero_k = x0 + (int)lround((double)(x1 - x0) * tz);
      }

      int k0_left = k0 - thumb_w_active / 2;
      int k0_right = k0 + thumb_w_active / 2;
      int k1_left = k1 - thumb_w_active / 2;
      int k1_right = k1 + thumb_w_active / 2;
      int inactive_left_end = rx0;
      int inactive_right_start = rx1;
      int active_start = rx0;
      int active_end = rx0;
      bool draw_active = false;
      const int inner_r = 2;
      int inactive_left_rl = track_r;
      int inactive_left_rr = inner_r;
      int inactive_right_rl = inner_r;
      int inactive_right_rr = track_r;
      int active_rl = inner_r;
      int active_rr = inner_r;

      if (n->slider.mode == COGITO_SLIDER_MODE_RANGE) {
        inactive_left_end = k0_left - gap;
        inactive_right_start = k1_right + gap;
        active_start = k0_right + gap;
        active_end = k1_left - gap;
        draw_active = active_end > active_start;
      } else if (n->slider.mode == COGITO_SLIDER_MODE_CENTERED) {
        int center_gap = 2;
        if (k0 < zero_k) {
          inactive_left_end = k0_left - gap;
          inactive_right_start = zero_k + center_gap;
          active_start = k0_right + gap;
          active_end = zero_k - center_gap;
        } else if (k0 > zero_k) {
          inactive_left_end = zero_k - center_gap;
          inactive_right_start = k0_right + gap;
          active_start = zero_k + center_gap;
          active_end = k0_left - gap;
        } else {
          inactive_left_end = zero_k - center_gap;
          inactive_right_start = zero_k + center_gap;
        }
        draw_active = active_end > active_start;
      } else {
        inactive_left_end = k0_left - gap;
        inactive_right_start = k0_right + gap;
        active_start = rx0;
        active_end = inactive_left_end;
        active_rl = track_r;
        draw_active = active_end > active_start;
      }

      if (inactive_left_end > rx0) {
        int seg_w = inactive_left_end - rx0;
        cogito_draw_rect_radii_aa(rx0, cy - rail_h / 2, seg_w, rail_h, inactive_track,
                                  inactive_left_rl, inactive_left_rr, inactive_left_rr, inactive_left_rl);
      }
      if (inactive_right_start < rx1) {
        int seg_w = rx1 - inactive_right_start;
        cogito_draw_rect_radii_aa(inactive_right_start, cy - rail_h / 2, seg_w, rail_h, inactive_track,
                                  inactive_right_rl, inactive_right_rr, inactive_right_rr, inactive_right_rl);
      }
      if (draw_active) {
        int seg_w = active_end - active_start;
        cogito_draw_rect_radii_aa(active_start, cy - rail_h / 2, seg_w, rail_h, active_track,
                                  active_rl, active_rr, active_rr, active_rl);
      }

      if (n->slider.mode == COGITO_SLIDER_MODE_NORMAL &&
          slider_size >= COGITO_SLIDER_SIZE_M &&
          n->icon && n->icon->data && n->icon->data[0] &&
          draw_active) {
        int icon_size = 24;
        int icon_pad = 10;
        if (slider_size == COGITO_SLIDER_SIZE_L) icon_pad = 14;
        if (slider_size == COGITO_SLIDER_SIZE_XL) icon_pad = 20;
        int icon_x = rx0 + icon_pad;
        int icon_y = cy - icon_size / 2;
        int need_w = (icon_x - active_start) + icon_size + 8;
        if ((active_end - active_start) >= need_w) {
          CogitoColor icon_col = s.has_knob ? s.knob : n->knob_color;
          cogito_draw_icon_fallback(n, n->icon->data, icon_x, icon_y, icon_size, icon_col);
        }
      }

      CogitoColor endpoint_dot = active_track;
      endpoint_dot.a = endpoint_dot.a > 170 ? endpoint_dot.a : 170;
      cogito_draw_rect(x0 - dot_d / 2, cy - dot_d / 2, dot_d, dot_d, endpoint_dot, dot_d / 2);
      if (x1 != x0) {
        cogito_draw_rect(x1 - dot_d / 2, cy - dot_d / 2, dot_d, dot_d, endpoint_dot, dot_d / 2);
      }

      int state_x = k0;
      if (n->slider.mode == COGITO_SLIDER_MODE_RANGE) {
        state_x = (n->slider.active_handle == 2) ? k1 : k0;
      } else if (n->slider.mode == COGITO_SLIDER_MODE_CENTERED) {
        state_x = k0;
      }

      // ---- Spring animation for thumb size on hover/active ----
      double dt = cogito_spring_dt;
      double target_size = (hover || active || n->slider.dragging) ? 1.0 : 0.0;

      if (!n->spring_h.inited) {
        cogito_spring_snap(&n->spring_h, target_size);
      }

      bool was_expanded = (n->spring_h.target >= 0.5);
      bool is_expanded = (hover || active || n->slider.dragging);
      if (was_expanded != is_expanded) {
        cogito_spring_snap(&n->spring_h, is_expanded ? 0.0 : 1.0);
      }
      n->spring_h.target = target_size;

      // Fast spatial for thumb expansion (k=800, z=0.6)
      cogito_spring_tick(&n->spring_h, target_size, 800.0, 0.6, dt);
      float size_scale = (float)n->spring_h.x;
      if (size_scale < 0.0f) size_scale = 0.0f;
      if (size_scale > 1.5f) size_scale = 1.5f;

      // Apply scale to thumb dimensions
      int animated_thumb_w = thumb_w_active + (int)((thumb_w_active * 0.3f) * size_scale);
      int animated_thumb_h = thumb_h + (int)((thumb_h * 0.2f) * size_scale);
      int animated_thumb_r = animated_thumb_w / 2;
      if (animated_thumb_r < 2) animated_thumb_r = 2;

      if (hover || active || n->slider.dragging || size_scale > 0.01f) {
        CogitoColor state_layer = cogito_node_state_layer_seed(n);
        state_layer.a = active ? 31 : 20;
        const int state_pad = 6;
        int state_x0 = state_x - animated_thumb_w / 2 - state_pad;
        int state_y0 = cy - animated_thumb_h / 2 - state_pad;
        int state_w = animated_thumb_w + state_pad * 2;
        int state_h = animated_thumb_h + state_pad * 2;
        int state_r = animated_thumb_r + state_pad;
        if (state_r > state_w / 2) state_r = state_w / 2;
        cogito_draw_rect(state_x0, state_y0, state_w, state_h, state_layer, state_r);
      }

      int k0_anim_left = k0 - animated_thumb_w / 2;
      cogito_draw_rect(k0_anim_left, cy - animated_thumb_h / 2, animated_thumb_w, animated_thumb_h, active_track, animated_thumb_r);
      if (n->slider.mode == COGITO_SLIDER_MODE_RANGE) {
        int k1_anim_left = k1 - animated_thumb_w / 2;
        cogito_draw_rect(k1_anim_left, cy - animated_thumb_h / 2, animated_thumb_w, animated_thumb_h, active_track, animated_thumb_r);
      }

      if (cogito_spring_active(&n->spring_h, target_size)) {
        double end_time = cogito_now() + 0.6;
        if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
        cogito_window_relayout(cogito_node_window(n));
      }
      break;
    }
