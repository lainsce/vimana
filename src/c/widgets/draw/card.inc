case COGITO_CARD: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg           = s.has_bg          ? s.bg           : n->bg;
  CogitoColor text_color   = s.has_text         ? s.text         : n->text_color;
  CogitoColor border_color = s.has_border       ? s.border       : n->border_color;
  int border_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 0);
  // Cogito Expressive: Card — L=16dp default (elevated). Overrideable via theme or node.
  int radius = s.has_radius ? s.radius : (n->radius_set ? n->border_radius : 16);
  int shadow = s.has_shadow ? s.shadow_level : n->shadow_level;

  // Variant overrides (only when not already set by theme/node)
  if (!s.has_border_width && !n->border_width_set) {
    if (n->card.variant == 2) border_w = 1; // outlined
  }
  if (!n->shadow_set) {
    if (n->card.variant == 1 || n->card.variant == 2) shadow = 0;
  }

  cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);

  // Shadow
  if (shadow > 0) {
    cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, shadow);
  }
  // Background
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // Header image (drawn inside card bg; shares card radius for top-corner clipping)
  if (n->card.header_image) {
    CogitoNode* img = n->card.header_image;
    img->border_radius = radius;
    img->radius_set = true;
    cogito_draw_node(img);
  }

  // State layer for interactive cards (on_click set): hover 8%, press 12%
  if (n->on_click) {
    CogitoColor state = cogito_node_state_layer_seed(n);
    if (active)     state.a = 31;
    else if (hover) state.a = 20;
    else            state.a = 0;
    if (state.a > 0) {
      cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, state, radius);
    }
  }
  // Border
  if (border_w > 0) {
    cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border_color, radius, border_w);
  }

  int img_h = n->card.header_image ? 160 : 0;
  CogitoStyle ts = cogito_theme_resolve_node(n);
  int pad_l = ts.has_padding_left ? ts.padding_left : 16;
  int pad_t = ts.has_padding_top  ? ts.padding_top  : 16;
  int sz    = cogito_node_font_size(n);
  int cx    = n->x + pad_l;
  int cy    = n->y + img_h + pad_t;

  // Title
  if (n->text && n->text->data && n->text->data[0]) {
    cogito_draw_text_size_node(n, cx, cy, n->text->data, text_color, sz, true, false);
    cy += cogito_text_height_size(sz) + n->gap;
  }
  // Subtitle (body-medium 14dp, slightly dimmed)
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    CogitoColor sub_col = text_color;
    sub_col.a = (uint8_t)(sub_col.a * 0.7f);
    cogito_draw_text_size_node(n, cx, cy, n->subtitle->data, sub_col, 14, false, false);
  }

  // Children (skip overflow_btn — drawn last on top)
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if (c == n->card.overflow_btn) continue;
    cogito_draw_node(c);
  }

  // Overflow button (drawn on top of everything)
  if (n->card.overflow_btn) {
    cogito_draw_node(n->card.overflow_btn);
  }
  break;
}

