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

  // ---- Shadow elevation animation using spring_h ----
  double dt = cogito_spring_dt;
  double target_elev = (hover || active) ? 1.0 : 0.0;

  if (!n->spring_h.inited) {
    cogito_spring_snap(&n->spring_h, target_elev);
  }

  bool was_elevated = (n->spring_h.target >= 0.5);
  bool is_elevated = (hover || active);
  if (was_elevated != is_elevated) {
    cogito_spring_snap(&n->spring_h, is_elevated ? 0.0 : 1.0);
  }
  n->spring_h.target = target_elev;

  // Slow spatial for shadow (k=200, z=0.8) - smooth elevation change
  cogito_spring_tick(&n->spring_h, target_elev, 200.0, 0.8, dt);
  float elev_scale = (float)n->spring_h.x;
  if (elev_scale < 0.0f) elev_scale = 0.0f;
  if (elev_scale > 1.5f) elev_scale = 1.5f;

  // Calculate animated shadow level
  int base_shadow = shadow;
  int hover_shadow = shadow + 2;
  if (hover_shadow > 5) hover_shadow = 5;
  int animated_shadow = base_shadow + (int)((hover_shadow - base_shadow) * elev_scale);

  // Shadow
  if (animated_shadow >= 0) {
    cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, animated_shadow);
  }

  if (cogito_spring_active(&n->spring_h, target_elev)) {
    double end_time = cogito_now() + 0.6;
    if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    cogito_window_relayout(cogito_node_window(n));
  }

  // Background
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  bool can_clip = cogito_backend && cogito_backend->begin_scissor && cogito_backend->end_scissor;
  if (can_clip) {
    cogito_backend->begin_scissor(n->x, n->y, n->w, n->h);
  }

  // Header image (drawn inside card bg; shares card radius for top-corner clipping)
  if (n->card.header_image) {
    CogitoNode* img = n->card.header_image;
    img->border_radius = radius;
    img->radius_set = true;
    cogito_draw_node(img);
  }

  int img_h = n->card.header_image ? 160 : 0;
  CogitoStyle ts = cogito_theme_resolve_node(n);
  bool has_header_image = n->card.header_image != NULL;
  int header_content_gap = has_header_image ? 24 : 0;
  int default_pad = has_header_image ? 0 : 16;
  int pad_l = ts.has_padding_left ? ts.padding_left : default_pad;
  int pad_t = ts.has_padding_top  ? ts.padding_top  : default_pad;
  int cx    = n->x + pad_l;
  int cy    = n->y + img_h + (has_header_image ? header_content_gap : pad_t);

  // Title
  if (n->text && n->text->data && n->text->data[0]) {
    int tsz = cogito_label_class_font_size(COGITO_LABEL_CLASS_CONTENT_TITLE, 16);
    CogitoColor tcol = cogito_label_class_color(COGITO_LABEL_CLASS_CONTENT_TITLE, text_color);
    bool tbold = cogito_label_class_bold(COGITO_LABEL_CLASS_CONTENT_TITLE, true);
    cogito_draw_text_size_node(n, cx, cy, n->text->data, tcol, tsz, tbold, false);
    cy += cogito_text_height_size(tsz) + n->gap;
  }
  // Subtitle
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    int ssz = cogito_label_class_font_size(COGITO_LABEL_CLASS_CONTENT_SUBTITLE, 14);
    CogitoColor scol = cogito_label_class_color(COGITO_LABEL_CLASS_CONTENT_SUBTITLE, text_color);
    bool sbold = cogito_label_class_bold(COGITO_LABEL_CLASS_CONTENT_SUBTITLE, false);
    cogito_draw_text_size_node(n, cx, cy, n->subtitle->data, scol, ssz, sbold, false);
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

  if (can_clip) {
    cogito_backend->end_scissor();
  }

  // Border is drawn last so content never paints over it.
  if (border_w > 0) {
    cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border_color, radius, border_w);
  }
  break;
}

