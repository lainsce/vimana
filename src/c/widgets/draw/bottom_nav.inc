case COGITO_BOTTOM_NAV: {
  CogitoStyle s = cogito_theme_resolve_node(n);

  // Draw background
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, 0);

  CogitoNode* fab = NULL;
  for (size_t i = 0; i < n->len; i++) {
    if (n->children[i] && n->children[i]->kind == COGITO_FAB) {
      fab = n->children[i];
      break;
    }
  }

  if (n->bottom_nav.len == 0) {
    if (fab) cogito_draw_node(fab);
    break;
  }

  int reserved_right = 0;
  if (fab && fab->w > 0) reserved_right = fab->w + 18;
  int items_w = n->w - reserved_right;
  if (items_w < 1) items_w = n->w;
  int item_w = items_w / (int)n->bottom_nav.len;
  if (item_w < 1) item_w = 1;
  int icon_size = 24;
  int label_size = 12;
  int mx = 0;
  int my = 0;
  bool have_mouse = cogito_pointer_query(&mx, &my, NULL, NULL);

  for (size_t i = 0; i < n->bottom_nav.len; i++) {
    bool selected = (int)i == n->bottom_nav.selected;
    bool hover = false;

    int item_x = n->x + (int)i * item_w;

    // Check if mouse is over this item
    if (have_mouse && cogito_hit_rect(mx, my, item_x, n->y, item_w, n->h)) {
      hover = true;
    }

    // Draw selection/hover background
    if (selected || hover) {
      CogitoColor hover_tint = s.has_text ? s.text : n->text_color;
      CogitoColor item_bg = selected
        ? (s.has_selection ? s.selection : n->selection_color)
        : (s.has_bg ? cogito_mix(s.bg, hover_tint, 0.1f) : n->bg);
      // Bottom nav uses indicator pill above selected item
      if (selected) {
        int indicator_w = 64;
        int indicator_h = 32;
        int indicator_x = item_x + (item_w - indicator_w) / 2;
        int indicator_y = n->y + 8;
        cogito_draw_rect(indicator_x, indicator_y, indicator_w, indicator_h, item_bg, 16);
      }
    }

    // Draw icon
    if (n->bottom_nav.icons && n->bottom_nav.icons[i]) {
      CogitoColor icon_color = s.has_text ? s.text : n->text_color;
      if (selected) {
        CogitoColor sel_bg = s.has_selection ? s.selection : n->selection_color;
        icon_color = cogito_on_color(sel_bg);
      } else if (hover) {
        icon_color = s.has_text ? s.text : n->text_color;
      }
      int icon_x = item_x + (item_w - icon_size) / 2;
      int icon_y = n->y + 12;
      cogito_draw_icon_fallback(n, n->bottom_nav.icons[i]->data, icon_x, icon_y, icon_size, icon_color);
    }

    // Draw label below icon
    if (n->bottom_nav.labels && n->bottom_nav.labels[i]) {
      CogitoColor text_color = s.has_text ? s.text : n->text_color;
      if (selected) {
        CogitoColor sel_bg = s.has_selection ? s.selection : n->selection_color;
        text_color = cogito_on_color(sel_bg);
      }
      int label_w = cogito_text_width_size_node(n, n->bottom_nav.labels[i]->data, label_size);
      int label_x = item_x + (item_w - label_w) / 2;
      int label_y = n->y + 12 + icon_size + 4;
      cogito_draw_text_size_node(n, label_x, label_y, n->bottom_nav.labels[i]->data, text_color, label_size, false);
    }
  }
  if (fab) cogito_draw_node(fab);
  break;
}
