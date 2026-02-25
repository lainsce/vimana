case COGITO_SETTINGS_LIST: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = s.has_radius ? s.radius : 12;
  int pad_t = s.has_padding_top ? s.padding_top : 8;

  // Draw card background
  if (s.has_bg || n->bg_set) {
    cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
  }
  // Draw title header
  if (n->text && n->text->data && n->text->data[
    0
  ]) {
    int ts = cogito_node_font_size(n);
    CogitoColor title_col = text_color;
    title_col.a = (uint8_t)(title_col.a * 0.6f);
    int tx = n->x + 16;
    int ty = n->y + pad_t;
    cogito_draw_text_size_node(n, tx, ty, n->text->data, title_col, ts, true);
  }
  // Draw children with dividers
  for (size_t i = 0; i < n->len; i++) {
    if (!cogito_rect_visible(n->children[i
    ]->x, n->children[i
    ]->y, n->children[i
    ]->w, n->children[i
    ]->h, n->x, n->y, n->w, n->h)) continue;
    cogito_draw_node(n->children[i
    ]);
    if (i + 1 < n->len) {
      CogitoColor div_color = text_color;
      div_color.a = (uint8_t)(div_color.a * 0.15f);
      int dy = n->children[i
      ]->y + n->children[i
      ]->h;
      cogito_draw_rect(n->x + 16, dy, n->w - 32,
      1, div_color,
      0);
    }
  }
  break;
}