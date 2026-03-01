case COGITO_SWITCH: {
      // ---- Configurable insets for switch handle ----
      // These control the distance of the handle from track edges
      int INSET_X_UNCHECKED = 8;    // Distance from left when unchecked
      int INSET_X_CHECKED = 4;      // Distance from right when checked
      int INSET_Y_UNCHECKED = 8;    // Distance from top/bottom when unchecked
      int INSET_Y_CHECKED = 4;      // Distance from top/bottom when checked
      int INSET_PRESSED = 2;        // Distance from all sides when pressed

      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      int text_w = 0;
      if (n->text) text_w = cogito_text_width(n->text->data);
      int intrinsic_w = text_w + 40;
      int use_w = n->w > 0 ? n->w : intrinsic_w;
      if (use_w < 24) use_w = 24;
      int min_track = n->min_w_set ? n->min_w : 52;
      if (min_track < 32) min_track = 32;
      int track_w = use_w - (n->text ? (text_w + 8) : 0);
      if (track_w < min_track) track_w = min_track;
      if (track_w > use_w) track_w = use_w;
      int text_h = n->text ? cogito_text_height() : 0;
      int intrinsic_h = text_h + 4;
      if (intrinsic_h < 32) intrinsic_h = 32; // Cogito: Switch track 32dp height
      int draw_h = n->h > 0 ? n->h : intrinsic_h;
      int h = (n->vexpand && draw_h > intrinsic_h) ? draw_h : intrinsic_h;
      if (h > draw_h) h = draw_h;
      if (h > track_w / 2) h = track_w / 2;
      if (h < 4) h = 4;
      int w = track_w;
      int cy = n->y + (draw_h - h) / 2;
      int cx = n->x;
      bool over = hover;
      bool down = active;
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoStyle track_s = cogito_switch_track_style(n->checked, over, down);
      CogitoStyle knob_s = cogito_switch_knob_style(n->checked, over, down);

      CogitoColor track_off = n->track_set ? n->track_color : (track_s.has_track ? track_s.track : n->bg);
      CogitoColor track_on  = n->track_on_set ? n->track_on_color : (track_s.has_track ? track_s.track : n->bg);
      if (track_s.has_bg) { track_off = track_s.bg; track_on = track_s.bg; }
      if (track_s.has_track_on) track_on = track_s.track_on;
      CogitoColor track = n->checked ? track_on : track_off;
      // Knob color: Outline for unchecked, On Primary for checked
      CogitoColor knob_col;
      if (n->knob_set) {
        knob_col = n->knob_color;
      } else if (n->checked) {
        // Checked: use On Primary (white/light on primary track)
        knob_col = cogito_cached_on_primary;
      } else {
        // Unchecked: use Outline color token
        knob_col = cogito_cached_outline;
      }

      // Animate track color via spring
      cogito_anim_apply(n, &track_s, &track, &knob_col, NULL, NULL);

      // Draw pill-shaped track
      cogito_draw_rect(cx, cy, w, h, track, h / 2);
      if (track_s.has_border_width && track_s.border_width > 0 && track_s.has_border && track_s.border.a > 0) {
        cogito_draw_rect_lines(cx, cy, w, h, track_s.border, h / 2, track_s.border_width);
      }
      // ---- Knob layout targets ----
      // Switch handle sizing:
      // - Unchecked, unpressed: 26dp width
      // - Checked, unpressed: 32dp width
      // - Both, pressed: 34dp width
      int knob_w_unchecked = 26;
      int knob_w_checked = 32;
      int knob_w_pressed = 34;
      int knob_h_normal = n->checked ? 24 : 16;  // Height stays as before
      int knob_h_pressed = 28;
      int knob_w_target = down ? knob_w_pressed : (n->checked ? knob_w_checked : knob_w_unchecked);
      int knob_h_target = down ? knob_h_pressed : knob_h_normal;
      if (knob_w_target > w - 4) knob_w_target = w - 4;
      if (knob_h_target > h - 4) knob_h_target = h - 4;

      // Calculate handle position using configured insets
      int track_cy = cy + h / 2;

      // When pressed, both states use INSET_PRESSED
      int inset_x = down ? INSET_PRESSED : (n->checked ? INSET_X_CHECKED : INSET_X_UNCHECKED);
      int inset_y = down ? INSET_PRESSED : (n->checked ? INSET_Y_CHECKED : INSET_Y_UNCHECKED);

      int off_x = cx + inset_x;
      int on_x = cx + w - knob_w_target - inset_x;
      if (on_x < off_x) on_x = off_x;

      int center_target = (n->checked ? on_x : off_x) + knob_w_target / 2;
      (void)(track_cy - knob_h_target / 2); // ky_target not used; ky derived from spring

      // ---- Spring-based knob position / size ----
  // Use spring_x for horizontal center, spring_h for knob height.
  // Expressive profile: slight physical bounce on toggle — satisfying confirmation.
      double dt = cogito_spring_dt;
      cogito_spring_tick(&n->spring_x, (double)center_target,
                         COGITO_SPRING_K_EXPRESSIVE, COGITO_SPRING_Z_EXPRESSIVE, dt);
      cogito_spring_tick(&n->spring_h, (double)knob_h_target,
                         COGITO_SPRING_K_FUNCTIONAL, COGITO_SPRING_Z_FUNCTIONAL, dt);

      int knob_w   = knob_w_target;
      int knob_h   = (int)(n->spring_h.x + 0.5);
      int center_x = (int)(n->spring_x.x + 0.5);

      if (knob_h < 4)  knob_h = 4;
      if (knob_h > h)  knob_h = h;
      int kx = center_x - knob_w / 2;

      // Calculate Y position respecting top/bottom insets
      int ky = cy + inset_y;

      // Clamp to track bounds
      if (kx < cx) kx = cx;
      int max_kx = cx + w - knob_w;
      if (max_kx < cx) max_kx = cx;
      if (kx > max_kx) kx = max_kx;
      if (ky < cy) ky = cy;
      int max_ky = cy + h - knob_h;
      if (max_ky < cy) max_ky = cy;
      if (ky > max_ky) ky = max_ky;

      // Stable corner radius (based on knob width so it doesn't shrink horizontally)
      int knob_r = knob_w / 2;
      if (knob_r < 1) knob_r = 1;
      if (knob_r > knob_h / 2) knob_r = knob_h / 2;
      cogito_draw_rect(kx, ky, knob_w, knob_h, knob_col, knob_r);

      // Keep animation ticking while springs are active
      if (cogito_spring_active(&n->spring_x, (double)center_target) ||
          cogito_spring_active(&n->spring_h, (double)knob_h_target)) {
        double end_time = cogito_now() + 0.6;
        if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    }

      if (n->text) {
        int th = cogito_text_height();
        int ty = n->y + (draw_h - th) / 2;
        CogitoColor label_text = n->text_color;
        if (cogito_has_selection_text) label_text = cogito_selection_text;
        else if (s.has_text) label_text = s.text;
        int text_x = n->x + w + 8;
        int text_avail = use_w - (text_x - n->x);
        if (text_avail > 0) {
          cogito_draw_text_ellipsis(n, text_x, ty, text_avail, n->text->data, label_text, cogito_node_font_size(n),
      false,
      0);
    }
  }
      break;
}
