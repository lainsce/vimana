case COGITO_APPBAR:
{
  cogito_appbar_apply_paired_surface(n);
  bool hover = false;
  bool active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
  CogitoColor bg = n->bg;
  CogitoColor border = n->border_color;
  CogitoColor text = s.has_text ? s.text : n->text_color;
  int border_w = n->border_width;
  int radius = n->border_radius;
  if (s.has_bg)
    bg = s.bg;
  if (s.has_border)
    border = s.border;
  if (s.has_border_width)
    border_w = s.border_width;
  if (s.has_radius)
    radius = s.radius;
  // Animate bg/text/border with spring (e.g. dark-mode toggle, theme switch)
  cogito_anim_apply(n, &s, &bg, &text, &border, NULL);
  bg = cogito_bin_scope_remap_color(bg);
  text = cogito_bin_scope_remap_color(text);
  border = cogito_bin_scope_remap_color(border);
  bool bottom_sheet_active = cogito_appbar_bottom_sheet_active(n);
  CogitoColor force_black = s.has_bg ? s.bg : bg;
  if (bottom_sheet_active)
  {
    bg = force_black;
    text = force_black;
    border = force_black;
    border_w = 0;
  }
  if (bottom_sheet_active || n->bg_set || s.has_bg || border_w > 0)
  {
    if (n->shadow_set && n->shadow_level >= 0)
    {
      cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
    }
    cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
    if (border_w > 0)
    {
      cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
    }
  }
  int btn = n->appbar.btn_size;
  int by = n->appbar.btn_y;
  int mx = 0, my = 0;
  bool down = false;
  (void)cogito_pointer_query(&mx, &my, &down, NULL);
  CogitoTrafficLightsState lights = {
      0};
  cogito_traffic_lights_compute(
      n->appbar.btn_close_x, n->appbar.btn_min_x, n->appbar.btn_max_x, by, btn,
      mx, my, down, &lights);

  CogitoColor fallback_btn = s.has_selection ? s.selection : n->selection_color;
  fallback_btn = cogito_bin_scope_remap_color(fallback_btn);
  CogitoColor fallback_border = border;
  if (!s.has_border)
    fallback_border = text;
  if (fallback_border.a > 80)
    fallback_border.a = 80;
  cogito_draw_traffic_lights_styled(
      &s, fallback_btn, fallback_border,
      n->appbar.btn_close_x, n->appbar.btn_min_x, n->appbar.btn_max_x,
      by, btn,
      &lights);
  int appbar_type = cogito_appbar_type(n);
  CogitoNode *title_widget = cogito_appbar_title_widget(n);
  if (appbar_type != COGITO_APPBAR_TYPE_SEARCH && n->text && !title_widget)
  {
    int title_cls = (appbar_type == COGITO_APPBAR_TYPE_LARGE) ? COGITO_LABEL_CLASS_DISPLAY : COGITO_LABEL_CLASS_TITLE;
    int tsz = cogito_appbar_title_font_size_for_type(appbar_type);
    int ty = n->appbar.title_y ? n->appbar.title_y : (n->y + 12);
    int tx = n->appbar.title_x ? n->appbar.title_x : (n->x + 16);
    CogitoColor tcol = cogito_label_class_color(title_cls, text);
    bool tbold = cogito_label_class_bold(title_cls, appbar_type != COGITO_APPBAR_TYPE_LARGE);
    bool use_serif = cogito_label_class_font_family_serif(title_cls, appbar_type == COGITO_APPBAR_TYPE_LARGE);
    bool prev_family_set = n->font_family_set;
    char prev_family[sizeof(n->font_family)];
    memcpy(prev_family, n->font_family, sizeof(prev_family));
    if (use_serif) {
      n->font_family_set = true;
      strncpy(n->font_family, "serif", sizeof(n->font_family) - 1);
      n->font_family[sizeof(n->font_family) - 1] = '\0';
    }
    cogito_draw_text_size_node(n, tx, ty, n->text->data, tcol, tsz, tbold, false);
    if (use_serif) {
      n->font_family_set = prev_family_set;
      memcpy(n->font_family, prev_family, sizeof(prev_family));
    }
  }
  if (appbar_type != COGITO_APPBAR_TYPE_SEARCH && n->subtitle)
  {
    int tsz = cogito_appbar_title_font_size_for_type(appbar_type);
    int t_h = cogito_text_height_size(tsz);
    int subtitle_cls = (appbar_type == COGITO_APPBAR_TYPE_SMALL) ? COGITO_LABEL_CLASS_CONTENT_TITLE : COGITO_LABEL_CLASS_SUBTITLE;
    int ssz = (appbar_type == COGITO_APPBAR_TYPE_SMALL)
      ? cogito_label_class_font_size(COGITO_LABEL_CLASS_CONTENT_TITLE, 16)
      : cogito_appbar_subtitle_font_size_for_type(appbar_type);
    int sy = n->appbar.subtitle_y ? n->appbar.subtitle_y : (n->y + 12 + t_h + 4);
    int tx = n->appbar.title_x ? n->appbar.title_x : (n->x + 16);
    CogitoColor scol = cogito_label_class_color(subtitle_cls, text);
    bool sbold = cogito_label_class_bold(subtitle_cls, false);
    cogito_draw_text_size_node(n, tx, sy, n->subtitle->data, scol, ssz, sbold, false);
  }
  for (size_t i = 0; i < n->len; i++)
  {
    CogitoNode *c = n->children[i];
    if (bottom_sheet_active && c)
    {
      CogitoColor prev_text = c->text_color;
      c->text_color = force_black;
      cogito_draw_node(c);
      c->text_color = prev_text;
    }
    else
    {
      cogito_draw_node(c);
    }
  }
  break;
}
