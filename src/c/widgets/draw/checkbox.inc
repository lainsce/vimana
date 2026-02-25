case COGITO_CHECKBOX: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      int text_w = 0;
      if (n->text) text_w = cogito_text_width(n->text->data);
      int text_h = n->text ? cogito_text_height() : 16;
      int intrinsic_w = text_w + 32;
      int draw_h = n->h > 0 ? n->h : (text_h + 4);
      if (draw_h < 12) draw_h = 12;
      int use_w = n->w > 0 ? n->w : intrinsic_w;
      if (use_w < 12) use_w = 12;
      // Cogito: Checkbox box — fixed 18×18dp, XS=4dp corner radius
      int avail = use_w - (n->text ? (text_w + 6) : 0);
      if (avail < 12) avail = 12;
      int box = 18;
      if (box > draw_h) box = draw_h;
      if (box > avail) box = avail;
      if (box < 12) box = 12;
      int cy = n->y + (draw_h - box) / 2;
      bool over = hover;
      bool down = active;
      bool is_radio = (n->group.tag != EVT_NULL);
      CogitoStyle box_style = cogito_checkbox_box_style(is_radio, n->checked, over, down);
      CogitoStyle check_style = cogito_checkbox_check_style(is_radio, n->checked, over, down);
      if (!box_style.has_transition_ms && s.has_transition_ms) {
        box_style.has_transition_ms = true;
        box_style.transition_ms = s.transition_ms;
  }
      if (!box_style.has_transition_ease && s.has_transition_ease) {
        box_style.has_transition_ease = true;
        box_style.transition_ease = s.transition_ease;
  }
      if (!check_style.has_transition_ms && s.has_transition_ms) {
        check_style.has_transition_ms = true;
        check_style.transition_ms = s.transition_ms;
  }
      if (!check_style.has_transition_ease && s.has_transition_ease) {
        check_style.has_transition_ease = true;
        check_style.transition_ease = s.transition_ease;
  }
      CogitoColor resolved_text = s.has_text ? s.text : n->text_color;

      // Get box colors
      CogitoColor box_bg = box_style.has_bg ? box_style.bg : n->bg;
      CogitoColor box_border = box_style.has_border ? box_style.border : resolved_text;
      int box_border_w = box_style.has_border_width ? box_style.border_width : 0;
      if (!n->checked && box_border_w < 2) box_border_w = 2;

      // Check mark color from theme, fallback to contrast against box background or text color
      CogitoColor check_base = box_bg;
      if (check_base.a == 0) check_base = box_border;
      CogitoColor check = check_style.has_check ? check_style.check : (n->checked ? cogito_on_color(check_base) : resolved_text);
      cogito_anim_apply(n, &box_style, &box_bg, &check, &box_border, NULL);

	      if (is_radio) {
	        int box_r = box_style.has_radius ? box_style.radius : (box / 2);
	        if (box_r < 1) box_r = 1;
	        if (box_r > box / 2) box_r = box / 2;

	        // Draw radio box
	        if (box_bg.a > 10) {
	          cogito_draw_rect(n->x, cy, box, box, box_bg, box_r);
    }
    // Draw radio border
	        if (box_border_w > 0) {
	          cogito_draw_rect_lines(n->x, cy, box, box, box_border, box_r, box_border_w);
    }
    // Draw inner dot when checked
	        if (n->checked) {
	          int r = box / 4;
	          if (r < 3) r = 3;
	          int dot_size = r * 2;
	          int dot_x = n->x + box / 2 - r;
	          int dot_y = cy + box / 2 - r;
	          int dot_r = r;
	          if (dot_r < 1) dot_r = 1;
	          cogito_draw_rect(dot_x, dot_y, dot_size, dot_size, check, dot_r);
    }
  } else {
    // Checkbox
        int box_radius = box_style.has_radius ? box_style.radius : 4; // XS token

        // Draw checkbox box
        if (box_bg.a > 10) {
          cogito_draw_rect(n->x, cy, box, box, box_bg, box_radius);
    }
    // Draw checkbox border
        if (box_border_w > 0) {
          cogito_draw_rect_lines(n->x, cy, box, box, box_border, box_radius, box_border_w);
    }
    // Checkmark: cleaner Cogito geometry — left tick + right stroke
        if (n->checked) {
          int mark = box - 2;
          if (mark < 6) mark = 6;
          int x0 = n->x + (box - mark) / 2;
          int y0 = cy + (box - mark) / 2;
          // Start → mid (lower-left to center-bottom)  
          int sx = x0 + (mark * 2) / 16;
          int sy = y0 + (mark * 8) / 16;
          int mx = x0 + (mark * 6) / 16;
          int my = y0 + (mark * 12) / 16;
          // mid → end (center-bottom to upper-right)
          int ex = x0 + (mark * 14) / 16;
          int ey = y0 + (mark * 3) / 16;
          cogito_draw_line(sx, sy, mx, my, check);
          cogito_draw_line(mx, my, ex, ey, check);
    }
  }
      if (n->text) {
        int th = cogito_text_height();
        int ty = n->y + (draw_h - th) / 2;
        CogitoColor label_text = n->text_color;
        if (cogito_has_selection_text) label_text = cogito_selection_text;
        else if (s.has_text) label_text = s.text;
        int text_x = n->x + box + 6;
        int text_avail = use_w - (text_x - n->x);
        if (text_avail > 0) {
          cogito_draw_text_ellipsis(n, text_x, ty, text_avail, n->text->data, label_text, cogito_node_font_size(n),
      false,
      0);
    }
  }
      break;
}