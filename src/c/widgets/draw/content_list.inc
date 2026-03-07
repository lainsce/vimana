case COGITO_CONTENT_LIST: {
  bool hover = false;
  bool active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int border_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 1);
  CogitoColor sel_bg = s.has_selection ? s.selection : n->selection_color;
  cogito_anim_apply(n, &s, &bg, NULL, NULL, &sel_bg);

  // Draw overall background with base radius
  int r = s.has_radius ? s.radius : 12;
  // ContentList backgrounds are item-centric: only fill the container when it has no children.
  if ((s.has_bg || n->bg_set) && n->len == 0) {
    cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, r);
  }
  // Get element position styles from theme and remap to active Bin scope.
  CogitoStyle first_style = cogito_theme.content_list_item_first;
  CogitoStyle last_style = cogito_theme.content_list_item_last;
  CogitoStyle middle_style = cogito_theme.content_list_item_middle;
  CogitoStyle single_style = cogito_theme.content_list_item_single;
  cogito_apply_bin_scope_to_style(&first_style);
  cogito_apply_bin_scope_to_style(&last_style);
  cogito_apply_bin_scope_to_style(&middle_style);
  cogito_apply_bin_scope_to_style(&single_style);

  bool has_selection = n->selected >= 0 && (size_t)n->selected < n->len;
  bool plain_list = n->class_name && n->class_name->data &&
                    (cogito_class_has_token(n->class_name->data, "plain") ||
                     cogito_class_has_token(n->class_name->data, "transparent"));

  // Determine unselected item background based on parent class
  bool in_sidebar = n->parent && n->parent->class_name &&
                    n->parent->class_name->data &&
                    strcmp(n->parent->class_name->data, "sidebar") == 0;
  CogitoColor unsel_bg = in_sidebar ? cogito_cached_surface_container_high
                                    : cogito_cached_surface_container;
  if (plain_list) unsel_bg = cogito_rgba(0, 0, 0, 0);

  // Draw children with proper rounded corners based on position
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* child = n->children[i];
    if (!cogito_rect_visible(child->x, child->y, child->w, child->h, n->x, n->y, n->w, n->h)) continue;

    // Determine element position and get corresponding style
    CogitoStyle* item_style = NULL;
    int tl = 0, tr = 0, br = 0, bl = 0;

    if (n->len == 1) {
      item_style = &single_style;
      tl = tr = br = bl = 12;
    } else if (i == 0) {
      item_style = &first_style;
      tl = tr = 12;
      br = bl = 8;
    } else if (i == n->len - 1) {
      item_style = &last_style;
      tl = tr = 8;
      br = bl = 12;
    } else {
      item_style = &middle_style;
      tl = tr = br = bl = 8;
    }
    // Override fallback radii with any explicit per-position style radius values.
    if (item_style->has_radius) {
      tl = tr = br = bl = item_style->radius;
    }
    if (item_style->has_radius_tl) tl = item_style->radius_tl;
    if (item_style->has_radius_tr) tr = item_style->radius_tr;
    if (item_style->has_radius_br) br = item_style->radius_br;
    if (item_style->has_radius_bl) bl = item_style->radius_bl;
    if (child->w > 0 && child->h > 0) {
      cogito_clamp_corner_radii(child->w, child->h, &tl, &tr, &br, &bl);
    }

    bool is_selected = has_selection && (size_t)n->selected == i;

    if (is_selected) {
        int sr = child->h / 2;
        CogitoColor sel_bg = cogito_cached_secondary_container;
        cogito_draw_rect_radii_aa(child->x, child->y, child->w, child->h, sel_bg, sr, sr, sr, sr);
        cogito_has_selection_text = true;
        CogitoColor sel_text = cogito_cached_on_secondary_container;
        if (sel_text.a == 0) {
          sel_text = cogito_cached_on_surface;
          if (sel_text.a == 0) {
            sel_text = cogito_on_color(sel_bg);
          }
        }
        cogito_selection_text = sel_text;
        cogito_draw_node(child);
        cogito_has_selection_text = false;
    } else {
      // Draw unselected item background with position-specific rounded corners
      if (unsel_bg.a > 0) {
        cogito_draw_rect_radii_aa(child->x, child->y, child->w, child->h, unsel_bg, tl, tr, br, bl);
      }
      // Draw the child
      cogito_draw_node(child);
    }

    // Draw divider between items (not after last, not around selected items)
    if (i + 1 < n->len && !is_selected && !(has_selection && (size_t)n->selected == i + 1)) {
      CogitoColor div_color = border_color;
      // Check if middle style has custom divider color
      if (middle_style.has_border) {
        div_color = middle_style.border;
      }
      div_color.a = (uint8_t)(div_color.a * 0.3f);
      int dy = child->y + child->h;
      int div_margin = middle_style.has_padding_left ? middle_style.padding_left : 16;
      int div_w = n->w - (div_margin * 2);
      if (div_w > 0) {
        cogito_draw_rect(n->x + div_margin, dy, div_w,
        1, div_color,
        0);
      }
    }
  }
  if (border_w > 0 && (s.has_border || n->border_width_set)) {
    cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border_color, r, border_w);
  }
  break;
}
