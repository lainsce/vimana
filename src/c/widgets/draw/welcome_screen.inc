case COGITO_WELCOME_SCREEN: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = s.has_radius ? s.radius : 16;

  // Draw background
  int shadow = s.has_shadow ? s.shadow_level : 3;
  if (shadow > 0) {
    cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, shadow);
  }
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);

  int cx = n->x + n->w / 2;
  int cy = n->y + 40;

  // Draw app icon
  if (n->icon && n->icon->data && n->icon->data[0]) {
    int icon_size = 64;
    cogito_draw_icon_fallback(n, n->icon->data, cx - icon_size / 2, cy, icon_size, text_color);
    cy += icon_size + 16;
  }

  // Draw title
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = 22;
    int tw = cogito_text_width_size_node(n, n->text->data, ts);
    cogito_draw_text_size_node(n, cx - tw / 2, cy, n->text->data, text_color, ts, true);
    cy += cogito_text_height_size(ts) + 8;
  }

  // Draw description
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    int ds = cogito_node_font_size(n);
    CogitoColor desc_col = text_color;
    desc_col.a = (uint8_t)(desc_col.a * 0.7f);
    int dw = cogito_text_width_size_node(n, n->subtitle->data, ds);
    cogito_draw_text_size_node(n, cx - dw / 2, cy, n->subtitle->data, desc_col, ds, false);
    cy += cogito_text_height_size(ds) + 16;
  }

  // Draw feature list children
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i]);
  }

  // Draw action button at bottom
  if (n->action_text && n->action_text->data && n->action_text->data[0]) {
    int as = cogito_node_font_size(n);
    CogitoColor action_color = s.has_selection ? s.selection : text_color;
    int aw = cogito_text_width_size_node(n, n->action_text->data, as) + 48;
    int ah = 40;
    int ax = cx - aw / 2;
    int ay = n->y + n->h - 40 - ah;

    n->action_button_x = ax;
    n->action_button_y = ay;
    n->action_button_w = aw;
    n->action_button_h = ah;

    cogito_draw_rect(ax, ay, aw, ah, action_color, 20);
    CogitoColor btn_text = cogito_on_color(action_color);
    int tx = cx - (aw - 48) / 2;
    int ty = ay + (ah - cogito_text_height_size(as)) / 2;
    cogito_draw_text_size_node(n, tx, ty, n->action_text->data, btn_text, as, true);
  }
  break;
}
