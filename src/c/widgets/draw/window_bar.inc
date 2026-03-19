case COGITO_WINDOW_BAR:
{
  bool hover = false;
  bool active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
  CogitoColor bg = n->bg;
  CogitoColor border = n->border_color;
  int border_w = n->border_width;
  int radius = n->border_radius;
  if (s.has_bg) bg = s.bg;
  if (s.has_border) border = s.border;
  if (s.has_border_width) border_w = s.border_width;
  if (s.has_radius) radius = s.radius;
  cogito_anim_apply(n, &s, &bg, NULL, &border, NULL);
  bg = cogito_bin_scope_remap_color(bg);
  border = cogito_bin_scope_remap_color(border);
  if (n->bg_set || s.has_bg || border_w > 0) {
    cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
    if (border_w > 0) {
      cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
    }
  }
  // Draw AppMenu: icon + app name + chevron (centered in button rect)
  if (n->kd) {
    int amx = n->kd->window_bar.appmenu_x;
    int amy = n->kd->window_bar.appmenu_y;
    int amw = n->kd->window_bar.appmenu_w;
    int amh = n->kd->window_bar.appmenu_h;
    int icon_sz = 24;
    int gap = 6;
    int chevron_sz = 10;
    int font_sz = 13;
    // Resolve on-surface color for text/icons
    CogitoColor on_surf = s.has_text ? s.text : cogito_on_color(bg);
    on_surf = cogito_bin_scope_remap_color(on_surf);
    // Hover highlight on AppMenu area (suppressed during screenshot)
    bool am_hover = !cogito_screenshot_pending &&
                    cogito_hit_rect_min48(cogito_main_pointer_x, cogito_main_pointer_y,
                                          amx, amy, amw, amh);
    if (am_hover && !cogito_menu.open) {
      CogitoColor hover_bg = on_surf;
      hover_bg.a = 20; // ~8% state layer
      cogito_draw_rect_node(n, &s, amx, amy, amw, amh, hover_bg, 999);
    } else if (cogito_menu.open && cogito_menu.owner == n) {
      CogitoColor hover_bg = on_surf;
      hover_bg.a = 28; // ~11% state layer
      cogito_draw_rect_node(n, &s, amx, amy, amw, amh, hover_bg, 999);
    }
    // Measure content width
    const char* name = NULL;
    if (cogito_active_app && cogito_active_app->app_name &&
        cogito_active_app->app_name->data && cogito_active_app->app_name->data[0]) {
      name = cogito_active_app->app_name->data;
    }
    int text_w = name ? cogito_text_width_size(name, font_sz) : 0;
    int content_w = icon_sz + gap + text_w + gap + chevron_sz;
    // Center content horizontally within the button rect
    int cx = amx + (amw - content_w) / 2;
    // Vertically align with the traffic lights center
    int cy_center = amy + amh / 2; // fallback
    CogitoNode* win_node = cogito_node_window(n);
    CogitoNode* ab = win_node ? cogito_find_appbar(win_node) : NULL;
    if (ab && ab->kd) {
      int bsz = ab->kd->appbar.btn_size;
      if (bsz < 1) bsz = 14;
      cy_center = ab->kd->appbar.btn_y + bsz / 2;
    }
    // Draw monochrome app icon
    const char* icon_path = cogito_resolve_app_icon_path(cogito_active_app);
    if (icon_path) {
      CogitoTexture* tex = NULL;
      int tw = 0, th = 0;
      float avg_lum = 0.5f;
      if (cogito_image_load_file_texture(icon_path, icon_sz, icon_sz, &tex, &tw, &th, &avg_lum)) {
        CogitoRect src_r = {0, 0, tw, th};
        CogitoRect dst_r = {cx, cy_center - icon_sz / 2, icon_sz, icon_sz};
        CogitoColor tint = on_surf;
        if (cogito_backend && cogito_backend->draw_texture)
          cogito_backend->draw_texture(tex, src_r, dst_r, tint);
      }
    }
    cx += icon_sz + gap;
    // Draw app name
    if (name) {
      int text_h = cogito_text_height_size(font_sz);
      int ty = cy_center - text_h / 2;
      cogito_draw_text_size_node(n, cx, ty, name, on_surf, font_sz, true, false);
      cx += text_w + gap;
    }
    // Draw chevron down
    int chev_y = cy_center - chevron_sz / 2;
    cogito_draw_icon_fallback(n, "chevron.down", cx, chev_y, chevron_sz, on_surf);
  }
  break;
}
