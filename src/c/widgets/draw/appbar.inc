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
  int appbar_type = cogito_appbar_type(n);
  CogitoNode *title_widget = cogito_appbar_title_widget(n);
  int text_right_bound = n->x + n->w - 16;
  if (!title_widget) {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* c = n->children[i];
      if (cogito_node_is_hidden(c)) continue;
      if (cogito_appbar_is_leading_child(c)) continue;
      int right = c->x - 12;
      if (right < text_right_bound) text_right_bound = right;
    }
  }
  if (appbar_type != COGITO_APPBAR_TYPE_SEARCH && n->text && !title_widget)
  {
    int title_cls = (appbar_type == COGITO_APPBAR_TYPE_LARGE) ? COGITO_LABEL_CLASS_DISPLAY : COGITO_LABEL_CLASS_TITLE;
    int tsz = cogito_appbar_title_font_size_for_type(appbar_type);
    int ty = n->appbar.title_y ? n->appbar.title_y : (n->y + 12);
    int tx = n->appbar.title_x ? n->appbar.title_x : (n->x + 16);
    int max_w = text_right_bound - tx;
    CogitoColor tcol = cogito_label_class_color(title_cls, text);
    bool tbold = cogito_label_class_bold(title_cls, false);
    bool tmedium = !tbold && cogito_label_class_medium(title_cls, appbar_type != COGITO_APPBAR_TYPE_LARGE);
    bool use_serif = cogito_label_class_font_family_serif(title_cls, false);
    bool prev_family_set = n->font_family_set;
    char prev_family[sizeof(n->font_family)];
    memcpy(prev_family, n->font_family, sizeof(prev_family));
    if (use_serif) {
      n->font_family_set = true;
      strncpy(n->font_family, "serif", sizeof(n->font_family) - 1);
      n->font_family[sizeof(n->font_family) - 1] = '\0';
    }
    if (max_w > 0) {
      cogito_draw_text_ellipsis(n, tx, ty, max_w, n->text->data, tcol, tsz, tbold, tmedium, 0);
    }
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
    int max_w = text_right_bound - tx;
    CogitoColor scol = cogito_label_class_color(subtitle_cls, text);
    bool sbold = cogito_label_class_bold(subtitle_cls, false);
    bool smedium = !sbold && cogito_label_class_medium(subtitle_cls, appbar_type == COGITO_APPBAR_TYPE_SMALL);
    if (max_w > 0) {
      cogito_draw_text_ellipsis(n, tx, sy, max_w, n->subtitle->data, scol, ssz, sbold, smedium, 0);
    }
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
