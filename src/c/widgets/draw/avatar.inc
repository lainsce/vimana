case COGITO_AVATAR: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = n->w / 2; // Always circular

  cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);

  // Draw circular background
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // Draw image if loaded, otherwise initials
  if (n->icon && n->icon->data && n->icon->data[0]) {
    // Try to load and draw image
    if (!n->image.loaded) {
      CogitoTexture* tex = NULL;
      if (cogito_image_load_file_texture(n->icon->data, n->w, n->h,
                                         &tex, &n->image.w, &n->image.h, &n->image.avg_lum)) {
        n->image.texture = tex;
      }
      n->image.loaded = true;
    }
    if (n->image.texture) {
      // Draw image clipped to circle (cover mode)
      int iw = n->image.w;
      int ih = n->image.h;
      float scale_w = (float)n->w / (float)iw;
      float scale_h = (float)n->h / (float)ih;
      float scale = scale_w > scale_h ? scale_w : scale_h;
      int src_w = (int)((float)n->w / scale);
      int src_h = (int)((float)n->h / scale);
      int src_x = (iw - src_w) / 2;
      int src_y = (ih - src_h) / 2;
      if (cogito_backend && cogito_backend->draw_texture) {
        CogitoRect src_rect = {src_x, src_y, src_w, src_h};
        CogitoRect dst_rect = {n->x, n->y, n->w, n->h};
        CogitoColor white = {255, 255, 255, 255};
        cogito_backend->draw_texture(n->image.texture, src_rect, dst_rect, white);
      }
      // Draw circle border on top to clip visual
      CogitoColor border_color = s.has_border ? s.border : n->border_color;
      int border_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 0);
      if (border_w > 0) {
        cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border_color, radius, border_w);
      }
    } else {
      // Fallback to icon
      int icon_size = n->w / 2;
      int ix = n->x + (n->w - icon_size) / 2;
      int iy = n->y + (n->h - icon_size) / 2;
      CogitoColor icon_col = cogito_on_color(bg);
      cogito_draw_icon_fallback(n, n->icon->data, ix, iy, icon_size, icon_col);
    }
  } else if (n->text && n->text->data && n->text->data[0]) {
    // Draw initials (first character, uppercased)
    char initials[4] = {0};
    const char* t = n->text->data;
    if (t[0] >= 'a' && t[0] <= 'z') {
      initials[0] = t[0] - 32;
    } else {
      initials[0] = t[0];
    }
    int size = n->w / 2;
    if (size < 10) size = 10;
    CogitoColor init_color = cogito_on_color(bg);
    int tw = cogito_text_width_size_node(n, initials, size);
    int th = cogito_text_height_size(size);
    int tx = n->x + (n->w - tw) / 2;
    int ty = n->y + (n->h - th) / 2;
    cogito_draw_text_size_node(n, tx, ty, initials, init_color, size, true);
  } else {
    // Empty avatar - draw person icon
    int icon_size = n->w / 2;
    int ix = n->x + (n->w - icon_size) / 2;
    int iy = n->y + (n->h - icon_size) / 2;
    CogitoColor icon_col = cogito_on_color(bg);
    cogito_draw_icon_fallback(n, "person", ix, iy, icon_size, icon_col);
  }
  break;
}
