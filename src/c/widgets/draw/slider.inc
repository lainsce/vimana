case COGITO_SLIDER: {
      bool hover = false;
      bool active = false;
      // If slider is being dragged, force active state
      if (n->kd->slider.dragging) {
        hover = true;
        active = true;
      } else {
        cogito_node_hover_state(n, &hover, &active);
      }
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoStyle track_s = cogito_slider_track_style(hover, active);
      CogitoStyle thumb_s = cogito_slider_thumb_style(hover, active);
      CogitoColor inactive_track = track_s.has_bg
        ? track_s.bg
        : (s.has_track
          ? s.track
          : (n->track_set
            ? n->track_color
            : (cogito_cached_surface_container_high.a ? cogito_cached_surface_container_high : n->bg)));
      CogitoColor active_track = thumb_s.has_bg
        ? thumb_s.bg
        : (s.has_text ? s.text : (s.has_knob ? s.knob : n->text_color));
      cogito_anim_apply(n, &s, &inactive_track, &active_track, NULL, NULL);
      inactive_track = cogito_bin_scope_remap_color(inactive_track);
      active_track = cogito_bin_scope_remap_color(active_track);

      CogitoColor container_bg = n->bg_set
        ? n->bg
        : (cogito_cached_surface_container.a ? cogito_cached_surface_container : cogito_cached_surface);
      CogitoColor surface_bg = cogito_cached_surface.a ? cogito_cached_surface : container_bg;
      container_bg = cogito_bin_scope_remap_color(container_bg);
      surface_bg = cogito_bin_scope_remap_color(surface_bg);
      bool require_stop_indicator = cogito_track_needs_stop_indicator(inactive_track, container_bg, surface_bg);

      bool cp_panel_slider = false;
      int cp_slider_channel = -1;
      CogitoNode* cp_panel = NULL;
      if (n->parent && n->parent->kind == COGITO_COLORPICKER && n->parent->kd->colorpicker.popover_panel) {
        cp_panel = n->parent;
        cp_slider_channel = cogito_colorpicker_panel_slider_channel(cp_panel, n);
        cp_panel_slider = (cp_slider_channel >= 0);
      }

      int slider_size = (int)n->kd->slider.size;
      if (slider_size < COGITO_SLIDER_SIZE_XS || slider_size > COGITO_SLIDER_SIZE_XL) {
        slider_size = COGITO_SLIDER_SIZE_XS;
      }

      const int dot_d = 4;
      const int endpoint_pad_x = 4;
      const int endpoint_pad_y = 6;
      const int thumb_w = 6;
      int thumb_w_active = thumb_w;
      if (active || n->kd->slider.dragging) thumb_w_active = 2;
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
      if (n->kd->slider.shape == COGITO_SLIDER_SHAPE_WAVY) {
        x0 += 4;
        x1 -= 4;
      }
      if (x1 < x0) x1 = x0;
      int endpoint_center_inset_x = dot_d / 2 + endpoint_pad_x;
      int rx0 = x0 - endpoint_center_inset_x;
      int rx1 = x1 + endpoint_center_inset_x;

      double t0 = 0.0;
      double t1 = 0.0;
      double span = n->kd->slider.max - n->kd->slider.min;
      if (span > 0.0) {
        t0 = (n->kd->slider.value - n->kd->slider.min) / span;
        t1 = (n->kd->slider.value2 - n->kd->slider.min) / span;
      }
      if (t0 < 0.0) t0 = 0.0;
      if (t0 > 1.0) t0 = 1.0;
      if (t1 < 0.0) t1 = 0.0;
      if (t1 > 1.0) t1 = 1.0;
      // RTL: invert track so value 0 is on right, max on left
      if (cogito_is_rtl()) { t0 = 1.0 - t0; t1 = 1.0 - t1; }

      int k0 = x0 + (int)lround((double)(x1 - x0) * t0);
      int k1 = x0 + (int)lround((double)(x1 - x0) * t1);
      if (k1 < k0) {
        int tmp = k0;
        k0 = k1;
        k1 = tmp;
      }

      int zero_k = x0 + (x1 - x0) / 2;
      if (span > 0.0 && n->kd->slider.min < 0.0 && n->kd->slider.max > 0.0) {
        double tz = (0.0 - n->kd->slider.min) / span;
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

      if (n->kd->slider.mode == COGITO_SLIDER_MODE_RANGE) {
        inactive_left_end = k0_left - gap;
        inactive_right_start = k1_right + gap;
        active_start = k0_right + gap;
        active_end = k1_left - gap;
        draw_active = active_end > active_start;
      } else if (n->kd->slider.mode == COGITO_SLIDER_MODE_CENTERED) {
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

      if (cp_panel_slider) {
        int gy = cy - rail_h / 2;
        int gh = rail_h;
        int gw = rx1 - rx0;
        // Keep hue strip positions aligned to the reference image while
        // generating colors in HCT instead of interpolating fixed RGBA stops.
        const double hue_track_max_deg = 300.0; // red -> ... -> magenta
        const double hue_track_chroma = 100.0;
        const double hue_track_tone = 50.0;
        double hue = cp_panel->kd->colorpicker.h;
        double tone = cp_panel->kd->colorpicker.t;
        if (tone < 0.0) tone = 0.0;
        if (tone > 100.0) tone = 100.0;
        CogitoColor chroma_gray = cogito_hct_to_rgb(hue, 0.0, tone);
        CogitoColor chroma_full = cogito_hct_to_rgb(hue, 100.0, tone);

        int seg_x[2] = {rx0, inactive_right_start};
        int seg_w[2] = {inactive_left_end - rx0, rx1 - inactive_right_start};
        int seg_tl[2] = {inactive_left_rl, inactive_right_rl};
        int seg_tr[2] = {inactive_left_rr, inactive_right_rr};
        int seg_br[2] = {inactive_left_rr, inactive_right_rr};
        int seg_bl[2] = {inactive_left_rl, inactive_right_rl};

        for (int seg = 0; seg < 2; seg++) {
          if (seg_w[seg] <= 0 || gh <= 0 || gw <= 0) continue;
          int sx = seg_x[seg];
          int sw = seg_w[seg];
          int tl = seg_tl[seg];
          int tr = seg_tr[seg];
          int br = seg_br[seg];
          int bl = seg_bl[seg];
          int bands = sw;
          if (bands < 1) bands = 1;

          for (int bi = 0; bi < bands; bi++) {
            int bx0 = sx + (bi * sw) / bands;
            int bx1 = sx + ((bi + 1) * sw) / bands;
            int bw = bx1 - bx0;
            if (bw <= 0) continue;

            double center_x = (double)bx0 + (double)bw * 0.5;
            double u = (gw > 1) ? ((center_x - (double)rx0) / (double)(gw - 1)) : 0.0;
            if (u < 0.0) u = 0.0;
            if (u > 1.0) u = 1.0;

            double u_l = (gw > 0) ? ((double)(bx0 - rx0) / (double)gw) : u;
            double u_r = (gw > 0) ? ((double)(bx1 - rx0) / (double)gw) : u;
            if (u_l < 0.0) u_l = 0.0;
            if (u_l > 1.0) u_l = 1.0;
            if (u_r < 0.0) u_r = 0.0;
            if (u_r > 1.0) u_r = 1.0;

            CogitoColor grad = inactive_track;
            if (cp_slider_channel == 0) {
              // Average neighboring hue samples per column to reduce visible
              // stepping from integer RGB quantization.
              double hue_deg_l = u_l * hue_track_max_deg;
              double hue_deg_r = u_r * hue_track_max_deg;
              if (hue_deg_l < 0.0) hue_deg_l = 0.0;
              if (hue_deg_l > hue_track_max_deg) hue_deg_l = hue_track_max_deg;
              if (hue_deg_r < 0.0) hue_deg_r = 0.0;
              if (hue_deg_r > hue_track_max_deg) hue_deg_r = hue_track_max_deg;
              CogitoColor grad_l = cogito_hct_to_rgb(hue_deg_l, hue_track_chroma, hue_track_tone);
              CogitoColor grad_r = cogito_hct_to_rgb(hue_deg_r, hue_track_chroma, hue_track_tone);
              grad = cogito_mix(grad_l, grad_r, 0.5f);
            } else if (cp_slider_channel == 1) {
              grad = cogito_mix(chroma_gray, chroma_full, (float)u);
            } else if (cp_slider_channel == 2) {
              CogitoColor black = {0, 0, 0, 255};
              CogitoColor white = {255, 255, 255, 255};
              grad = cogito_mix(black, white, (float)u);
            }

            int bt_l = (bi == 0) ? tl : 0;
            int bt_t = (bi == bands - 1) ? tr : 0;
            int bt_b = (bi == bands - 1) ? br : 0;
            int bt_bl = (bi == 0) ? bl : 0;
            cogito_draw_rect_radii_aa(bx0, gy, bw, gh, grad, bt_l, bt_t, bt_b, bt_bl);
          }
        }

        if (draw_active) {
          CogitoColor active_overlay = {255, 255, 255, 20}; // 8% white
          int seg_w = active_end - active_start;
          cogito_draw_rect_radii_aa(active_start, gy, seg_w, gh, active_overlay,
                                    active_rl, active_rr, active_rr, active_rl);
        }
      } else {
        bool wavy = (n->kd->slider.shape == COGITO_SLIDER_SHAPE_WAVY);

        double wave_thickness = (double)rail_h;
        double wave_amp = 0.0;
        double wave_period = 40.0;
        double wave_phase = 0.0;
        if (wavy) {
          if (slider_size == COGITO_SLIDER_SIZE_XL) {
            wave_thickness = 24.0; wave_amp = 3.0; wave_period = 120.0;
          } else if (slider_size == COGITO_SLIDER_SIZE_L) {
            wave_thickness = 16.0; wave_amp = 3.0; wave_period = 90.0;
          } else if (slider_size == COGITO_SLIDER_SIZE_M) {
            wave_thickness =  8.0; wave_amp = 3.0; wave_period = 80.0;
          } else if (slider_size == COGITO_SLIDER_SIZE_S) {
            wave_thickness =  4.0; wave_amp = 4.0; wave_period = 60.0;
          } else {
            wave_thickness =  4.0; wave_amp = 3.0; wave_period = 40.0;
          }

          // Animate wavy sliders: phase travels along X while amplitude
          // breathes up/down to create a lively vertical motion.
          double wt = cogito_now();
          wave_phase = wt * 4.5;
          double breathe = 0.5 + 0.5 * sin(wt * 2.8);
          wave_amp *= (0.55 + 0.45 * breathe);
        }

        int inactive_h  = wavy ? (int)lround(wave_thickness) : rail_h;
        if (inactive_h < 1) inactive_h = 1;
        int inactive_rl = wavy ? inactive_h / 2 : track_r;

        // In wavy NORMAL mode the left inactive segment covers the same region
        // as the active wave, so skip it. In RANGE/CENTERED modes the left
        // inactive is outside the active region and should always render.
        bool hide_inactive_left = wavy && (n->kd->slider.mode == COGITO_SLIDER_MODE_NORMAL);
        if (!hide_inactive_left && inactive_left_end > rx0) {
          int seg_w = inactive_left_end - rx0;
          cogito_draw_rect_radii_aa(rx0, cy - inactive_h / 2, seg_w, inactive_h, inactive_track,
                                    inactive_rl, inactive_rl, inactive_rl, inactive_rl);
        }
        if (inactive_right_start < rx1) {
          int seg_w = rx1 - inactive_right_start;
          cogito_draw_rect_radii_aa(inactive_right_start, cy - inactive_h / 2, seg_w, inactive_h, inactive_track,
                                    inactive_rl, inactive_rl, inactive_rl, inactive_rl);
        }
        if (draw_active) {
          int seg_w = active_end - active_start;
          if (wavy && seg_w >= 2) {
            cogito_draw_wavy_line_aa(active_start, active_end, cy,
                                     wave_thickness, wave_amp, wave_period, wave_phase, active_track);
            double wave_end = cogito_now() + 0.10;
            if (wave_end > cogito_anim_end_time) cogito_anim_end_time = wave_end;
          } else {
            cogito_draw_rect_radii_aa(active_start, cy - rail_h / 2, seg_w, rail_h, active_track,
                                      active_rl, active_rr, active_rr, active_rl);
          }
        }
      }

      bool draw_step_markers = false;
      bool draw_left_end_dot = false;
      bool draw_right_end_dot = false;
      if (n->kd->slider.show_end_dots) {
        if (n->kd->slider.mode == COGITO_SLIDER_MODE_NORMAL) {
          // Normal slider starts from the left side, so only show the opposite
          // endpoint marker on the right.
          draw_right_end_dot = true;
        } else {
          // Range and centered sliders keep both endpoint markers visible.
          draw_left_end_dot = true;
          draw_right_end_dot = true;
        }

        // Hide endpoint dots when a handle is already essentially on top of
        // that endpoint (outer 2% zones).
        const double end_dot_hide_t = 0.02;
        double left_handle_t = t0;
        double right_handle_t = t0;
        if (n->kd->slider.mode == COGITO_SLIDER_MODE_RANGE) {
          left_handle_t = t0;
          right_handle_t = t1;
          if (right_handle_t < left_handle_t) {
            double tmp = left_handle_t;
            left_handle_t = right_handle_t;
            right_handle_t = tmp;
          }
        }
        if (draw_left_end_dot && left_handle_t <= end_dot_hide_t) {
          draw_left_end_dot = false;
        }
        if (draw_right_end_dot && right_handle_t >= (1.0 - end_dot_hide_t)) {
          draw_right_end_dot = false;
        }
      }
      if (require_stop_indicator && n->kd->slider.mode == COGITO_SLIDER_MODE_NORMAL) {
        draw_right_end_dot = true;
      }

      if (!cp_panel_slider && span > 0.0 && n->kd->slider.show_step_dots) {
        double step = cogito_slider_step_normalized(n->kd->slider.step);
        if (step > 0.000001) {
          int step_count = (int)floor(span / step + 0.5);
          if (step_count >= 1 && step_count <= 256) {
            draw_step_markers = true;
            int marker_d = dot_d;
            CogitoColor marker_col = active_track;
            marker_col.a = marker_col.a > 170 ? marker_col.a : 170;
            int center_x = x0 + (x1 - x0) / 2;
            bool have_prev = false;
            int prev_x = 0;
            for (int i = 0; i <= step_count; i++) {
              if (i == 0 && !draw_left_end_dot) continue;
              if (i == step_count && !draw_right_end_dot) continue;
              double sv = n->kd->slider.min + (double)i * step;
              if (i == step_count || sv > n->kd->slider.max) sv = n->kd->slider.max;
              double tu = (sv - n->kd->slider.min) / span;
              if (tu < 0.0) tu = 0.0;
              if (tu > 1.0) tu = 1.0;
              int sx = x0 + (int)lround((double)(x1 - x0) * tu);
              if (n->kd->slider.mode == COGITO_SLIDER_MODE_CENTERED &&
                  i != 0 && i != step_count && abs(sx - center_x) <= 1) {
                continue;
              }
              if (have_prev && abs(sx - prev_x) < 2) continue;
              prev_x = sx;
              have_prev = true;
              cogito_draw_rect(sx - marker_d / 2, cy - marker_d / 2,
                               marker_d, marker_d, marker_col, marker_d / 2);
            }
          }
        }
      }

      if (n->kd->slider.mode == COGITO_SLIDER_MODE_NORMAL &&
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
          CogitoColor icon_col = thumb_s.has_bg ? thumb_s.bg : (s.has_knob ? s.knob : n->knob_color);
          cogito_draw_icon_fallback(n, n->icon->data, icon_x, icon_y, icon_size, icon_col);
        }
      }

      if (!cp_panel_slider && !draw_step_markers) {
        CogitoColor endpoint_dot = active_track;
        endpoint_dot.a = endpoint_dot.a > 170 ? endpoint_dot.a : 170;
        if (draw_left_end_dot) {
          cogito_draw_rect(x0 - dot_d / 2, cy - dot_d / 2, dot_d, dot_d, endpoint_dot, dot_d / 2);
        }
        if (draw_right_end_dot && x1 != x0) {
          cogito_draw_rect(x1 - dot_d / 2, cy - dot_d / 2, dot_d, dot_d, endpoint_dot, dot_d / 2);
        }
      }

      int state_x = k0;
      if (n->kd->slider.mode == COGITO_SLIDER_MODE_RANGE) {
        state_x = (n->kd->slider.active_handle == 2) ? k1 : k0;
      } else if (n->kd->slider.mode == COGITO_SLIDER_MODE_CENTERED) {
        state_x = k0;
      }

      // ---- Spring animation for thumb size on hover/active ----
      double dt = cogito_spring_dt;
      double target_size = (hover || active || n->kd->slider.dragging) ? 1.0 : 0.0;

      if (!n->spring_h.inited) {
        cogito_spring_snap(&n->spring_h, target_size);
      }

      bool was_expanded = (n->spring_h.target >= 0.5);
      bool is_expanded = (hover || active || n->kd->slider.dragging);
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

      if (hover || active || n->kd->slider.dragging || size_scale > 0.01f) {
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
      if (n->kd->slider.mode == COGITO_SLIDER_MODE_RANGE) {
        int k1_anim_left = k1 - animated_thumb_w / 2;
        cogito_draw_rect(k1_anim_left, cy - animated_thumb_h / 2, animated_thumb_w, animated_thumb_h, active_track, animated_thumb_r);
      }

      bool bubble_visible = n->kd->slider.show_value_bubble && (active || n->kd->slider.dragging);
      if (!cp_panel_slider && bubble_visible) {
        int bubble_h = 40;
        int value_size = cogito_node_font_size(n);
        if (value_size < 12) value_size = 12;
        if (value_size > 16) value_size = 16;

        CogitoColor bubble_bg = cogito_cached_inverse_surface.a
          ? cogito_cached_inverse_surface
          : (cogito_cached_surface_container_high.a ? cogito_cached_surface_container_high : n->bg);
        CogitoColor bubble_text = cogito_cached_inverse_on_surface.a
          ? cogito_cached_inverse_on_surface
          : (cogito_cached_on_surface.a ? cogito_cached_on_surface : cogito_on_color(bubble_bg));
        bubble_bg = cogito_bin_scope_remap_color(bubble_bg);
        bubble_text = cogito_bin_scope_remap_color(bubble_text);

        int handle_count = 1;
        int handle_idx[2] = {0, 0};
        if (n->kd->slider.mode == COGITO_SLIDER_MODE_RANGE) {
          handle_idx[0] = (n->kd->slider.active_handle == 2) ? 1 : 0;
        }
        int handle_x[2] = {k0, k1};
        double handle_val[2] = {n->kd->slider.value, n->kd->slider.value2};

        int bubble_y_base = cy - animated_thumb_h / 2 - 4 - bubble_h;

        for (int hi = 0; hi < handle_count; hi++) {
          int idx = handle_idx[hi];
          double bubble_value = handle_val[idx];
          double display_value = bubble_value * n->kd->slider.bubble_scale;
          char value_buf[96];
          value_buf[0] = '\0';

          char number_buf[48];
          double iv = round(display_value);
          if (fabs(display_value - iv) < 0.0005) {
            snprintf(number_buf, sizeof(number_buf), "%d", (int)iv);
          } else {
            snprintf(number_buf, sizeof(number_buf), "%.2f", display_value);
            size_t nlen = strlen(number_buf);
            while (nlen > 0 && number_buf[nlen - 1] == '0') {
              number_buf[--nlen] = '\0';
            }
            if (nlen > 0 && number_buf[nlen - 1] == '.') {
              number_buf[--nlen] = '\0';
            }
          }

          const char* fmt = (n->kd->slider.bubble_format && n->kd->slider.bubble_format->data)
            ? n->kd->slider.bubble_format->data
            : NULL;
          if (fmt && fmt[0]) {
            char time_buf[32];
            long long ms = llround(display_value);
            if (ms < 0) ms = 0;
            long long total_s = ms / 1000;
            long long hours = total_s / 3600;
            long long mins = (total_s % 3600) / 60;
            long long secs = total_s % 60;
            if (hours > 0) {
              snprintf(time_buf, sizeof(time_buf), "%lld:%02lld:%02lld", hours, mins, secs);
            } else {
              snprintf(time_buf, sizeof(time_buf), "%02lld:%02lld", mins, secs);
            }

            char int_buf[32];
            snprintf(int_buf, sizeof(int_buf), "%d", (int)llround(display_value));

            const char* marker = NULL;
            const char* replacement = NULL;
            const char* marker_pos = NULL;

            marker = "{time}";
            marker_pos = strstr(fmt, marker);
            if (marker_pos) {
              replacement = time_buf;
            } else {
              marker = "{int}";
              marker_pos = strstr(fmt, marker);
              if (marker_pos) {
                replacement = int_buf;
              } else {
                marker = "{value}";
                marker_pos = strstr(fmt, marker);
                if (marker_pos) {
                  replacement = number_buf;
                }
              }
            }

            if (marker_pos && replacement) {
              size_t prefix_len = (size_t)(marker_pos - fmt);
              const char* suffix = marker_pos + strlen(marker);
              snprintf(value_buf, sizeof(value_buf), "%.*s%s%s", (int)prefix_len, fmt, replacement, suffix);
            } else {
              snprintf(value_buf, sizeof(value_buf), "%s", fmt);
            }
          } else {
            snprintf(value_buf, sizeof(value_buf), "%s", number_buf);
          }

          int tw = cogito_text_width_size_node(n, value_buf, value_size);
          int th = cogito_text_height_size(value_size);
          int bubble_pad_x = 12;
          int bubble_w = tw + bubble_pad_x * 2;
          if (bubble_w < bubble_h) bubble_w = bubble_h;
          int bubble_r = bubble_h / 2;
          if (bubble_r > bubble_w / 2) bubble_r = bubble_w / 2;

          int bubble_x = handle_x[idx] - bubble_w / 2;
          int bubble_y = bubble_y_base;

          int min_bx = n->x + 2;
          int max_bx = n->x + n->w - bubble_w - 2;
          if (max_bx < min_bx) {
            bubble_x = min_bx;
          } else {
            if (bubble_x < min_bx) bubble_x = min_bx;
            if (bubble_x > max_bx) bubble_x = max_bx;
          }

          cogito_draw_rect(bubble_x, bubble_y, bubble_w, bubble_h, bubble_bg, bubble_r);

          int tx = bubble_x + (bubble_w - tw) / 2;
          int ty = bubble_y + (bubble_h - th) / 2;
          cogito_draw_text_size_node(n, tx, ty, value_buf, bubble_text, value_size, false, true);
        }
      }

      if (cogito_spring_active(&n->spring_h, target_size)) {
        double end_time = cogito_now() + 0.6;
        if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
        cogito_window_relayout(cogito_node_window(n));
      }
      break;
    }
