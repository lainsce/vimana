case COGITO_FAB: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
  bool in_nav_rail = (n->parent && n->parent->kind == COGITO_NAV_RAIL);
  bool nav_rail_expanded = in_nav_rail && n->parent->nav_rail.expanded;
  bool effective_extended = n->fab.extended || nav_rail_expanded;
  const char* label = (n->subtitle && n->subtitle->data && n->subtitle->data[0])
    ? n->subtitle->data
    : "Create";
  CogitoStyle rail_style = {0};
  if (in_nav_rail) {
    rail_style = cogito_theme_resolve_node(n->parent);
  }

  // FAB size metrics: S(0), M(1), L(2)
  // Height: S=56, M=80, L=96
  // Corner radius: S=16, M=20, L=28
  // Icon size: S=24, M=28, L=36
  // Padding between icon and text (extended): S=8, M=12, L=16
  int fab_radius, fab_icon;
  int ext_lpad, ext_gap;
  if (n->fab.size == 0) {
    // Size S (default)
    fab_radius = 16; fab_icon = 24;
    ext_lpad = 16; ext_gap = 8;
  } else if (n->fab.size == 2) {
    // Size L
    fab_radius = 28; fab_icon = 36;
    ext_lpad = 28; ext_gap = 16;
  } else {
    // Size M
    fab_radius = 20; fab_icon = 28;
    ext_lpad = 26; ext_gap = 12;
  }

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = fab_radius;
  if (in_nav_rail) {
    bg = rail_style.has_selection ? rail_style.selection : n->parent->selection_color;
    text_color = cogito_on_color(bg);
    radius = 16;
  }

  // Animate colors
  if (!in_nav_rail) {
    cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);
  }

  // Draw box shadow (elevation)
  if (!in_nav_rail) {
    if (s.has_box_shadow && s.box_shadow.blur > 0) {
      cogito_draw_shadow_node_box(n, &s, n->x, n->y, n->w, n->h, radius, &s.box_shadow);
    } else if (n->shadow_set && n->shadow_level > 0) {
      cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
    } else {
      // Default FAB elevation
      cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, 3);
    }
  }

  // Draw elevated rounded background
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // State layer for hover/press
  if (hover || active) {
    CogitoColor state_layer = cogito_node_state_layer_seed(n);
    state_layer.a = active ? 31 : 20;
    cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, state_layer, radius);
  }

  // Draw ripple effect for Primary-colored FAB (clipped to widget bounds)
  if (!in_nav_rail) {
    if (cogito_ripple_update(n)) {
      float r_radius, r_alpha;
      cogito_ripple_get(n, &r_radius, &r_alpha);
      if (r_radius > 0 && r_alpha > 0) {
        CogitoColor ripple_color = text_color;
        ripple_color.a = (uint8_t)(255 * r_alpha);
        int cx = n->x + n->ripple_x;
        int cy = n->y + n->ripple_y;
        cogito_draw_circle_clipped_node(n, &s, radius, cx, cy, r_radius, ripple_color);
      }
    }
  }

  // Draw icon
  if (n->text && n->text->data[0]) {
    int icon_size = fab_icon;
    int icon_x = effective_extended ? (n->x + ext_lpad) : (n->x + (n->w - icon_size) / 2);
    int icon_y = n->y + (n->h - icon_size) / 2;

    cogito_draw_icon_fallback(n, n->text->data, icon_x, icon_y, icon_size, text_color);
  }

  // Draw label if extended
  if (effective_extended && label[0]) {
    int size = cogito_node_font_size(n);
    int text_h = cogito_text_height_size(size);
    int label_x = n->x + ext_lpad + fab_icon + ext_gap;
    int label_y = n->y + (n->h - text_h) / 2;
    cogito_draw_text_size_node(n, label_x, label_y, label, text_color, size, false);
  }
  break;
}
