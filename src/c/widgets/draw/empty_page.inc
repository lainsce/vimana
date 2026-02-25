case COGITO_EMPTY_PAGE: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  CogitoColor bg = s.has_bg ? s.bg : n->bg;

  if (s.has_bg) {
    int radius = s.has_radius ? s.radius : 0;
    cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
  }

  int cx = n->x + n->w / 2;
  int cy_cursor = n->y + n->h / 3; // Start from upper third

  // Draw icon
  if (n->icon && n->icon->data && n->icon->data[0]) {
    int icon_size = 128;
    int ix = cx - icon_size / 2;
    CogitoColor icon_col = text_color;
    icon_col.a = (uint8_t)(icon_col.a * 0.32f);
    cogito_draw_icon_fallback(n, n->icon->data, ix, cy_cursor, icon_size, icon_col);
    cy_cursor += icon_size + 12;
  }

  // Draw title
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    if (ts < 18) ts = 18;
    int tw = cogito_text_width_size_node(n, n->text->data, ts);
    int tx = cx - tw / 2;
    cogito_draw_text_size_node(n, tx, cy_cursor, n->text->data, text_color, ts, true);
    cy_cursor += cogito_text_height_size(ts) + 6;
  }

  // Draw description
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    int ds = cogito_node_font_size(n);
    CogitoColor desc_col = text_color;
    desc_col.a = (uint8_t)(desc_col.a * 0.7f);
    int dw = cogito_text_width_size_node(n, n->subtitle->data, ds);
    int dx = cx - dw / 2;
    cogito_draw_text_size_node(n, dx, cy_cursor, n->subtitle->data, desc_col, ds, false);
    cy_cursor += cogito_text_height_size(ds) + 12;
  }

  // Draw action button
  if (n->action_text && n->action_text->data && n->action_text->data[0]) {
    int as = cogito_node_font_size(n);
    CogitoColor action_color = s.has_selection ? s.selection : text_color;
    int aw = cogito_text_width_size_node(n, n->action_text->data, as) + 32;
    int ah = 56;
    int ax = cx - aw / 2;
    int ay = cy_cursor;

    n->action_button_x = ax;
    n->action_button_y = ay;
    n->action_button_w = aw;
    n->action_button_h = ah;

    // Button background
    CogitoColor btn_bg = action_color;
    btn_bg.a = 32;
    cogito_draw_rect(ax, ay, aw, ah, btn_bg, 999);

    int tx = cx - (aw - 32) / 2;
    int ty = ay + (ah - cogito_text_height_size(as)) / 2;
    cogito_draw_text_size_node(n, tx, ty, n->action_text->data, action_color, as, true);
  }
  break;
}
