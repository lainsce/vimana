case COGITO_PROGRESS: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor track = s.has_bg ? s.bg : (n->bg_set ? n->bg : (s.has_track ? s.track : n->bg));
      CogitoColor fill = s.has_selection ? s.selection : (s.has_text ? s.text : (s.has_knob ? s.knob : n->text_color));
      cogito_anim_apply(n, &s, &track, &fill, NULL, NULL);

      int thickness = n->track_height_set ? n->track_height : (s.has_track_height ? s.track_height : 4);
      if (thickness < 1) thickness = 1;
      int cap = thickness / 2;
      if (cap < 1) cap = 1;
      bool indeterminate = n->progress.indeterminate;
      bool circular = n->progress.circular;
      bool wavy = (n->progress.shape == COGITO_PROGRESS_SHAPE_WAVY);

      double now = cogito_now();

      if (circular) {
        int min_side = n->w < n->h ? n->w : n->h;
        int outer_r = min_side / 2 - 1;
        if (outer_r < 2) break;
        if (thickness > outer_r) thickness = outer_r;
        cap = thickness / 2;
        if (cap < 1) cap = 1;
        int stroke_r = outer_r - cap;
        if (stroke_r < 1) stroke_r = 1;

        int cx = n->x + n->w / 2;
        int cy = n->y + n->h / 2;
        const double rad_to_deg = 57.2957795130823208768;
        const double start_deg = -90.0;
        const double sweep_deg = 360.0;
        const double end_deg = start_deg + sweep_deg;
        double wave_phase = 0.0;
        double wave_amp = (double)thickness * 0.24;
        if (wave_amp < 0.6) wave_amp = 0.6;

        double seg_a0 = start_deg;
        double seg_a1 = start_deg;
        if (indeterminate) {
          double chunk = sweep_deg * 0.30;
          double phase = fmod(now * 210.0, sweep_deg);
          seg_a0 = start_deg + phase;
          seg_a1 = seg_a0 + chunk;
        } else {
          double t = n->progress.value;
          if (t < 0.0) t = 0.0;
          if (t > 1.0) t = 1.0;
          seg_a1 = start_deg + sweep_deg * t;
        }

        double boundary_sep = thickness < 6 ? 6.0 : 8.0;
        if (stroke_r > 0) {
          double half_t = (double)thickness * 0.5;
          double ratio = (half_t + 0.35) / (double)stroke_r;
          if (ratio > 1.0) ratio = 1.0;
          double cap_ext = asin(ratio) * rad_to_deg;
          double pad_px = (double)thickness * 0.35 + 0.8;
          double pad_arc = (pad_px / (double)stroke_r) * rad_to_deg;
          double needed = cap_ext * 2.0 + pad_arc;
          if (needed > boundary_sep) boundary_sep = needed;
        }

        if (!wavy) {
          wave_amp = 0.0;
        }

        double active_s[2];
        double active_e[2];
        int active_count = 0;
        if (indeterminate && seg_a1 > end_deg) {
          double a0 = seg_a0;
          double a1 = end_deg;
          if (a1 > a0) {
            active_s[active_count] = a0;
            active_e[active_count] = a1;
            active_count++;
          }
          a0 = start_deg;
          a1 = start_deg + (seg_a1 - end_deg);
          if (a1 > a0) {
            active_s[active_count] = a0;
            active_e[active_count] = a1;
            active_count++;
          }
        } else if (seg_a1 > seg_a0) {
          active_s[active_count] = seg_a0;
          active_e[active_count] = seg_a1;
          active_count++;
        }

        for (int i = 0; i < active_count; i++) {
          if (active_s[i] < start_deg) active_s[i] = start_deg;
          if (active_e[i] > end_deg) active_e[i] = end_deg;
        }
        if (active_count == 2 && active_s[1] < active_s[0]) {
          double ts = active_s[0];
          double te = active_e[0];
          active_s[0] = active_s[1];
          active_e[0] = active_e[1];
          active_s[1] = ts;
          active_e[1] = te;
        }

        double cut_s[4];
        double cut_e[4];
        int cut_count = 0;
        for (int i = 0; i < active_count; i++) {
          double s = (active_s[i] - start_deg) - boundary_sep;
          double e = (active_e[i] - start_deg) + boundary_sep;

          while (s < 0.0) {
            s += sweep_deg;
            e += sweep_deg;
          }
          while (s >= sweep_deg) {
            s -= sweep_deg;
            e -= sweep_deg;
          }

          if (e <= s) continue;
          if (e <= sweep_deg) {
            if (cut_count < 4) {
              cut_s[cut_count] = s;
              cut_e[cut_count] = e;
              cut_count++;
            }
          } else {
            if (cut_count < 4) {
              cut_s[cut_count] = s;
              cut_e[cut_count] = sweep_deg;
              cut_count++;
            }
            if (cut_count < 4) {
              cut_s[cut_count] = 0.0;
              cut_e[cut_count] = e - sweep_deg;
              cut_count++;
            }
          }
        }

        for (int i = 0; i < cut_count; i++) {
          for (int j = i + 1; j < cut_count; j++) {
            if (cut_s[j] < cut_s[i]) {
              double ts = cut_s[i];
              double te = cut_e[i];
              cut_s[i] = cut_s[j];
              cut_e[i] = cut_e[j];
              cut_s[j] = ts;
              cut_e[j] = te;
            }
          }
        }

        double merged_s[4];
        double merged_e[4];
        int merged_count = 0;
        for (int i = 0; i < cut_count; i++) {
          double s = cut_s[i];
          double e = cut_e[i];
          if (e <= s) continue;
          if (merged_count > 0 && s <= merged_e[merged_count - 1]) {
            if (e > merged_e[merged_count - 1]) merged_e[merged_count - 1] = e;
          } else if (merged_count < 4) {
            merged_s[merged_count] = s;
            merged_e[merged_count] = e;
            merged_count++;
          }
        }

        if (active_count == 0) {
          cogito_draw_ring_arc_aa(cx, cy, (double)stroke_r, (double)thickness, start_deg, end_deg, track, false, 0.0, 0.0);
        } else {
          double cursor = 0.0;
          for (int i = 0; i < merged_count; i++) {
            if (merged_s[i] > cursor) {
              double t0 = start_deg + cursor;
              double t1 = start_deg + merged_s[i];
              cogito_draw_ring_arc_aa(cx, cy, (double)stroke_r, (double)thickness, t0, t1, track, false, 0.0, 0.0);
            }
            if (merged_e[i] > cursor) cursor = merged_e[i];
          }
          if (cursor < sweep_deg) {
            double t0 = start_deg + cursor;
            double t1 = start_deg + sweep_deg;
            cogito_draw_ring_arc_aa(cx, cy, (double)stroke_r, (double)thickness, t0, t1, track, false, 0.0, 0.0);
          }
        }

        for (int i = 0; i < active_count; i++) {
          double a0 = active_s[i];
          double a1 = active_e[i];
          if (a1 <= a0) continue;
          cogito_draw_ring_arc_aa(cx, cy, (double)stroke_r, (double)thickness, a0, a1, fill, wavy, wave_phase, wave_amp);
        }
      } else {
        int x0 = n->x;
        int x1 = n->x + n->w;
        if (x1 <= x0) break;
        int cy = n->y + n->h / 2;
        int boundary_gap = cap * 2 + 1;
        if (boundary_gap < 2) boundary_gap = 2;

        int active_start = x0;
        int active_end = x0;
        bool has_active = false;
        if (indeterminate) {
          int width = x1 - x0;
          int chunk = (int)lround((double)width * 0.38);
          if (chunk < thickness * 4) chunk = thickness * 4;
          if (chunk > width) chunk = width;
          double travel = (double)(width + chunk + 1);
          double pos = fmod(now * 260.0, travel) - (double)chunk;
          active_start = x0 + (int)floor(pos);
          active_end = active_start + chunk;
          if (active_start < x0) active_start = x0;
          if (active_end > x1) active_end = x1;
          has_active = active_end > active_start;
        } else {
          double t = n->progress.value;
          if (t < 0.0) t = 0.0;
          if (t > 1.0) t = 1.0;
          active_start = x0;
          active_end = x0 + (int)lround((double)(x1 - x0) * t);
          if (active_end > x1) active_end = x1;
          has_active = active_end > active_start;
        }

        if (!has_active) {
          cogito_draw_rect(x0, cy - thickness / 2, x1 - x0, thickness, track, cap);
        } else {
          int left_end = active_start - boundary_gap;
          if (left_end > x0) {
            cogito_draw_rect(x0, cy - thickness / 2, left_end - x0, thickness, track, cap);
          }
          int right_start = active_end + boundary_gap;
          if (right_start < x1) {
            cogito_draw_rect(right_start, cy - thickness / 2, x1 - right_start, thickness, track, cap);
          }

          int draw_a0 = active_start;
          int draw_a1 = active_end;
          if (draw_a0 < x0) draw_a0 = x0;
          if (draw_a1 > x1) draw_a1 = x1;

          if (draw_a1 > draw_a0) {
            if (!wavy) {
              cogito_draw_rect(draw_a0, cy - thickness / 2, draw_a1 - draw_a0, thickness, fill, cap);
            } else {
              int span = draw_a1 - draw_a0;
              if (span < 2) {
                cogito_draw_rect(draw_a0, cy - thickness / 2, span, thickness, fill, cap);
              } else {
                double amp = (double)thickness * 0.38;
                if (amp < 1.0) amp = 1.0;
                double period = (double)thickness * 3.2;
                if (period < 12.0) period = 12.0;
                double phase = indeterminate ? now * 8.0 : 0.0;
                cogito_draw_wavy_line_aa(draw_a0, draw_a1, cy,
                                         (double)thickness,
                                         amp, period, phase,
                                         fill);
              }
            }
          }
        }
      }

      if (indeterminate) {
        double anim_end = now + 0.032;
        if (anim_end > cogito_anim_end_time) cogito_anim_end_time = anim_end;
      }
      break;
    }
