case COGITO_BADGE: {
  CogitoStyle s = cogito_theme_resolve_node(n);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = n->h / 2;

  // Draw pill/circle background
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // Draw count text if present
  if (n->text && n->text->data && n->text->data[0]) {
    int size = 11;
    CogitoColor tc = cogito_on_color(bg);
    int tw = cogito_text_width_size_node(n, n->text->data, size);
    int th = cogito_text_height_size(size);
    int tx = n->x + (n->w - tw) / 2;
    int ty = n->y + (n->h - th) / 2;
    cogito_draw_text_size_node(n, tx, ty, n->text->data, tc, size, true);
    (void)text_color;
  }
  break;
}
