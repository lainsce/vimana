case COGITO_BOTTOM_SHEET: {
  CogitoStyle s = cogito_theme_resolve_node(n);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = s.has_radius ? s.radius : (n->radius_set ? n->border_radius : 28);

  cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);

  // Draw modal scrim when the sheet is overlaid in ZStack/Window.
  CogitoNode* host = n->parent;
  if (host && host->kind != COGITO_DIALOG_SLOT &&
      (host->kind == COGITO_ZSTACK || host->kind == COGITO_WINDOW)) {
    CogitoStyle slot_style = cogito_theme_resolve(COGITO_DIALOG_SLOT);
    CogitoColor scrim = slot_style.has_bg ? slot_style.bg : cogito_cached_surface;
    if (scrim.a == 0) {
      CogitoStyle ws = cogito_theme_resolve(COGITO_WINDOW);
      if (ws.has_bg) scrim = ws.bg;
      else if (cogito_cached_surface.a) scrim = cogito_cached_surface;
      else scrim = cogito_rgba(0, 0, 0, 255);
      scrim.a = 82;
    }
    CogitoNode* win = cogito_node_window(n);
    bool window_overlay = (host->kind == COGITO_WINDOW);
    if (!window_overlay && host->kind == COGITO_ZSTACK && win && host->parent == win) {
      window_overlay = true;
    }
    int sx = window_overlay ? 0 : host->x;
    int sy = window_overlay ? 0 : host->y;
    int sw = window_overlay && win ? win->w : host->w;
    int sh = window_overlay && win ? win->h : host->h;
    if (sw > 0 && sh > 0) {
      cogito_draw_rect(sx, sy, sw, sh, scrim, 0);
    }
  }

  // Draw docked sheet surface.
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // ---- Drag handle animation using spring_w ----
  double dt = cogito_spring_dt;
  double target_expand = (n->kd->bottom_sheet.dragging || n->kd->bottom_sheet.expanded) ? 1.0 : 0.0;

  if (!n->spring_w.inited) {
    cogito_spring_snap(&n->spring_w, target_expand);
  }

  bool was_expanded = (n->spring_w.target >= 0.5);
  bool is_expanded = (n->kd->bottom_sheet.dragging || n->kd->bottom_sheet.expanded);
  if (was_expanded != is_expanded) {
    cogito_spring_snap(&n->spring_w, is_expanded ? target_expand : n->spring_w.x);
  }
  n->spring_w.target = target_expand;

  // Default spatial for handle (k=360, z=0.8)
  cogito_spring_tick(&n->spring_w, target_expand, 360.0, 0.8, dt);
  float expand_scale = (float)n->spring_w.x;
  if (expand_scale < 0.0f) expand_scale = 0.0f;
  if (expand_scale > 1.5f) expand_scale = 1.5f;

  // Animate handle width from 32 to 44 based on state
  int base_handle_w = 32;
  int expanded_handle_w = 44;
  int handle_w = base_handle_w + (int)((expanded_handle_w - base_handle_w) * expand_scale);
  int handle_h = 4 + (int)(2 * expand_scale); // 4 to 6
  if (n->kd->bottom_sheet.dragging && handle_h < 6) handle_h = 6;

  int handle_x = n->x + (n->w - handle_w) / 2;
  int handle_y = n->y + 12;
  CogitoColor handle_color = text_color;
  handle_color.a = 100;
  cogito_draw_rect(handle_x, handle_y, handle_w, handle_h, handle_color, handle_h / 2);

  if (cogito_spring_active(&n->spring_w, target_expand)) {
    double end_time = cogito_now() + 0.6;
    if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    cogito_window_relayout(cogito_node_window(n));
  }

  // Draw title
  int content_y = n->y + 28;
  if (n->text && n->text->data && n->text->data[0]) {
    int pad_l = n->padding_left;
    int pad_t = n->padding_top;
    int size = cogito_node_font_size(n);
    int th = cogito_text_height_size(size);
    int gap = n->gap > 0 ? n->gap : 8;
    cogito_draw_text_size_node(n, n->x + pad_l, content_y + pad_t,
                               n->text->data, text_color, size, true, false);
    (void)th;
    (void)gap;
  }

  // Draw children
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i]);
  }
  break;
}
