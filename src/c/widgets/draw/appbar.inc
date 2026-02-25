case COGITO_APPBAR: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg     = n->bg;
      CogitoColor border = n->border_color;
      CogitoColor text   = s.has_text ? s.text : n->text_color;
      int border_w = n->border_width;
      int radius   = n->border_radius;
      if (s.has_bg)           bg       = s.bg;
      if (s.has_border)       border   = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius)       radius   = s.radius;
      // Animate bg/text/border with spring (e.g. dark-mode toggle, theme switch)
      cogito_anim_apply(n, &s, &bg, &text, &border, NULL);
      bool bottom_sheet_active = cogito_appbar_bottom_sheet_active(n);
      CogitoColor force_black = cogito_rgba(0,
  0,
  0,
  255);
      if (bottom_sheet_active) {
        bg = force_black;
        text = force_black;
        border = force_black;
        border_w = 0;
  }
      if (bottom_sheet_active || n->bg_set || s.has_bg || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
    }
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
    }
  }
	      int btn = n->appbar.btn_size;
	      int by = n->appbar.btn_y;
	      int mx = 0, my = 0;
      bool down = false;
      (void)cogito_pointer_query(&mx, &my, &down, NULL);
      CogitoTrafficLightsState lights = {
    0
  };
      cogito_traffic_lights_compute(
        n->appbar.btn_close_x, n->appbar.btn_min_x, n->appbar.btn_max_x, by, btn,
        mx, my, down, &lights
      );

      CogitoColor fallback_btn = s.has_selection ? s.selection : n->selection_color;
      CogitoColor fallback_border = border;
      if (!s.has_border) fallback_border = text;
      if (fallback_border.a > 80) fallback_border.a = 80;
      cogito_draw_traffic_lights_styled(
        &s, fallback_btn, fallback_border,
        n->appbar.btn_close_x, n->appbar.btn_min_x, n->appbar.btn_max_x,
        by, btn,
        &lights
      );
      CogitoNode* title_widget = cogito_appbar_title_widget(n);
      if (n->text && !title_widget) {
        int tsz = cogito_label_class_font_size(COGITO_LABEL_CLASS_TITLE,
    24);
        int ty = n->appbar.title_y ? n->appbar.title_y : (n->y + 12);
        CogitoColor tcol = cogito_label_class_color(COGITO_LABEL_CLASS_TITLE, text);
        bool tbold = cogito_label_class_bold(COGITO_LABEL_CLASS_TITLE, true);
        cogito_draw_text_size_node(n, n->x + 16, ty, n->text->data, tcol, tsz, tbold);
  }
      if (n->subtitle) {
        int tsz = cogito_label_class_font_size(COGITO_LABEL_CLASS_TITLE,
    24);
        int t_h = cogito_text_height_size(tsz);
        int ssz = cogito_label_class_font_size(COGITO_LABEL_CLASS_SUBTITLE,
    18);
        int sy = n->appbar.subtitle_y ? n->appbar.subtitle_y : (n->y + 12 + t_h + 4);
        CogitoColor scol = cogito_label_class_color(COGITO_LABEL_CLASS_SUBTITLE, text);
        bool sbold = cogito_label_class_bold(COGITO_LABEL_CLASS_SUBTITLE, false);
        cogito_draw_text_size_node(n, n->x + 16, sy, n->subtitle->data, scol, ssz, sbold);
  }
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i
    ];
        if (bottom_sheet_active && c) {
          CogitoColor prev_text = c->text_color;
          c->text_color = force_black;
          cogito_draw_node(c);
          c->text_color = prev_text;
    } else {
          cogito_draw_node(c);
    }
  }
      break;
}
