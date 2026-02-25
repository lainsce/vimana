case COGITO_SWITCHBAR: {
	  bool hover = false, active = false;
	  cogito_node_hover_state(n, &hover, &active);
	  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
	  CogitoColor secondary_container = {0}, on_secondary_container = {0};
	  cogito_theme_get_state_layer_palette(NULL, NULL, &secondary_container, NULL,
	                                       NULL, NULL, NULL, &on_secondary_container,
	                                       NULL, NULL);
	  
	  // When checked/activated, use Secondary Container + On Secondary Container.
	  CogitoColor bg = n->checked 
	    ? secondary_container
	    : (s.has_bg ? s.bg : n->bg);
	  CogitoColor text_color = n->checked
	    ? on_secondary_container
	    : (s.has_text ? s.text : n->text_color);
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int radius = s.has_radius ? s.radius : 12;
  int border_w = n->checked ? 0 : 1;
  int bar_h = s.has_min_h ? s.min_h : 56;

  cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);

  // Draw bar background with rounded corners
  if (border_w > 0) {
    cogito_draw_rect(n->x, n->y, n->w, bar_h, bg, radius);
    cogito_draw_rect_lines(n->x, n->y, n->w, bar_h, border_color, radius, border_w);
  } else {
    cogito_draw_rect(n->x, n->y, n->w, bar_h, bg, radius);
  }
  // Draw label
  if (n->text && n->text->data && n->text->data[
    0
  ]) {
    int ts = cogito_node_font_size(n);
    int th = cogito_text_height_size(ts);
    int tx = n->x + 20;
    int ty = n->y + (bar_h - th) / 2;
    cogito_draw_text_size_node(n, tx, ty, n->text->data, text_color, ts, false);
  }
  // Draw switch - replicate switch.inc rendering for consistent look
  {
    int sw_w = 52;
    int sw_h = 28;
    int sw_x = n->x + n->w - sw_w - 20;
    int sw_y = n->y + (bar_h - sw_h) / 2;

    // Get themed colors (same as switch.inc)
    CogitoStyle track_s = cogito_switch_track_style(n->checked, hover, active);
    CogitoStyle knob_s = cogito_switch_knob_style(n->checked, hover, active);

    // Resolve track colors - exact switch.inc pattern
    CogitoColor track_off = n->track_set ? n->track_color : (track_s.has_track ? track_s.track : n->bg);
    CogitoColor track_on = n->track_on_set ? n->track_on_color : (track_s.has_track_on ? track_s.track_on : (track_s.has_track ? track_s.track : n->bg));
    if (track_s.has_bg) {
      track_off = track_s.bg;
      track_on = track_s.bg;
    }
    if (track_s.has_track_on) track_on = track_s.track_on;
    CogitoColor track = n->checked ? track_on : track_off;

    // Draw track
    cogito_draw_rect(sw_x, sw_y, sw_w, sw_h, track, sw_h / 2);

    // Resolve knob color - exact switch.inc pattern
    CogitoColor knob_col = n->knob_set ? n->knob_color : (knob_s.has_knob ? knob_s.knob : (knob_s.has_bg ? knob_s.bg : n->knob_color));

    // ---- Spring-driven knob position (mirrors switch.inc) ----
    int knob_w   = 32;
    int knob_h_r = hover ? sw_h : 24;
    if (knob_h_r > sw_h) knob_h_r = sw_h;
    int edge       = hover ? 0 : 2;
    int off_knob_x = sw_x + edge;
    int on_knob_x  = sw_x + sw_w - knob_w - edge;
    int center_t   = (n->checked ? on_knob_x : off_knob_x) + knob_w / 2;

    double dt = cogito_spring_dt;
    cogito_spring_tick(&n->spring_x, (double)center_t,
                       COGITO_SPRING_K_EXPRESSIVE, COGITO_SPRING_Z_EXPRESSIVE, dt);
    cogito_spring_tick(&n->spring_h, (double)knob_h_r,
                       COGITO_SPRING_K_FUNCTIONAL, COGITO_SPRING_Z_FUNCTIONAL, dt);

    int knob_h   = (int)(n->spring_h.x + 0.5);
    int center_x = (int)(n->spring_x.x + 0.5);
    int knob_x   = center_x - knob_w / 2;
    int knob_y   = sw_y + (sw_h - knob_h) / 2;
    if (knob_x < sw_x) knob_x = sw_x;
    if (knob_x > sw_x + sw_w - knob_w) knob_x = sw_x + sw_w - knob_w;
    int knob_r = knob_h / 2;
    cogito_draw_rect(knob_x, knob_y, knob_w, knob_h, knob_col, knob_r);

    if (cogito_spring_active(&n->spring_x, (double)center_t) ||
        cogito_spring_active(&n->spring_h, (double)knob_h_r)) {
      double et = cogito_now() + 0.6;
      if (et > cogito_anim_end_time) cogito_anim_end_time = et;
    }
  }
  // Draw children (only if checked/enabled)
  if (n->checked) {
    for (size_t i = 0; i < n->len; i++) {
      cogito_draw_node(n->children[i
      ]);
    }
  }
  break;
}
