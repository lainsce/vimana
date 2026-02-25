case COGITO_SETTINGS_WINDOW: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;

  int sidebar_w = 200;

  // Draw sidebar background
  CogitoColor sidebar_bg = bg;
  sidebar_bg.a = (uint8_t)(sidebar_bg.a * 0.95f);
  cogito_draw_rect(n->x, n->y, sidebar_w, n->h, sidebar_bg, 0);

  // Draw sidebar title
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    if (ts < 16) ts = 16;
    cogito_draw_text_size_node(n, n->x + 16, n->y + 16, n->text->data, text_color, ts, true);
  }

  // Draw sidebar items (page titles)
  int item_h = 40;
  int iy = n->y + 48;
  int sel = n->selected >= 0 ? n->selected : 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* page = n->children[i];
    bool is_sel = ((int)i == sel);
    if (is_sel) {
      CogitoColor sel_bg = text_color;
      sel_bg.a = 20;
      cogito_draw_rect(n->x + 4, iy, sidebar_w - 8, item_h, sel_bg, 8);
    }
    // Use page title
    if (page->text && page->text->data && page->text->data[0]) {
      int ts = cogito_node_font_size(page);
      int th = cogito_text_height_size(ts);
      int ty = iy + (item_h - th) / 2;
      CogitoColor item_color = text_color;
      if (!is_sel) item_color.a = (uint8_t)(item_color.a * 0.6f);
      cogito_draw_text_size_node(n, n->x + 16, ty, page->text->data, item_color, ts, is_sel);
    }
    iy += item_h;
  }

  // Draw divider between sidebar and content
  CogitoColor div = text_color;
  div.a = (uint8_t)(div.a * 0.15f);
  cogito_draw_rect(n->x + sidebar_w - 1, n->y, 1, n->h, div, 0);

  // Draw active page content
  if (sel >= 0 && sel < (int)n->len) {
    cogito_draw_node(n->children[sel]);
  }
  break;
}
