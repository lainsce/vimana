case COGITO_GRID: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true;
  }
          if (s.has_border) { n->border_color = s.border; n->border_color_set = true;
  }
          if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true;
  }
      if (s.has_radius) n->border_radius = s.radius;
          if (n->bg_set || n->border_width > 0) {
            if (n->shadow_set && n->shadow_level > 0) {
              cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, n->border_radius, n->shadow_level);
    }
            cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, n->bg, n->border_radius);
            if (n->border_width > 0) {
              cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
    }
  }
      bool selectable = n->on_select || n->on_activate;
      bool has_selection = selectable && n->selected >= 0 && (size_t)n->selected < n->len;
      CogitoColor sel_bg = s.has_selection ? s.selection : n->selection_color;
      if (has_selection) {
        CogitoNode* sel = n->children[n->selected
    ];
        int inset = 2;
        int sr = 0;
        if (sel->border_radius > 0) sr = sel->border_radius + inset;
        if (cogito_rect_visible(sel->x - inset, sel->y - inset, sel->w + inset * 2, sel->h + inset * 2, n->x, n->y, n->w, n->h))
          cogito_draw_rect(sel->x - inset, sel->y - inset, sel->w + inset * 2, sel->h + inset * 2, sel_bg, sr);
  }
      for (size_t i = 0; i < n->len; i++) {
        if (!cogito_rect_visible(n->children[i
    ]->x, n->children[i
    ]->y, n->children[i
    ]->w, n->children[i
    ]->h, n->x, n->y, n->w, n->h)) continue;
        if (has_selection && (size_t)n->selected == i) {
          cogito_has_selection_text = true;
          cogito_selection_text = cogito_on_color(sel_bg);
          cogito_draw_node(n->children[i
      ]);
          cogito_has_selection_text = false;
    } else {
          cogito_draw_node(n->children[i
      ]);
    }
  }
      break;
}