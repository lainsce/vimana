static void cogito_draw_tooltip(CogitoNode* win, CogitoNode* target) {
  if (!win || !target || !target->tooltip || !target->tooltip->data) return;
  CogitoStyle s = cogito_theme_resolve(COGITO_TOOLTIP);
  CogitoColor bg = s.has_bg ? s.bg : cogito_rgba(30, 30, 30, 240);
  CogitoColor text = s.has_text ? s.text : cogito_rgba(255, 255, 255, 255);
  int text_l = cogito_luma(text);
  int bg_l = cogito_luma(bg);
  if (abs(text_l - bg_l) < 80) {
    text = cogito_on_color(bg);
  }
  CogitoColor border = s.has_border ? s.border : cogito_rgba(0, 0, 0, 0);
  int border_w = s.has_border_width ? s.border_width : 0;
  int radius = s.has_radius ? s.radius : 6;
  int font_size = s.has_font_size ? s.font_size : cogito_font_size();
  int pad_x = s.has_padding_left ? s.padding_left : 8;
  int pad_y = s.has_padding_top ? s.padding_top : 6;

  const char* t = target->tooltip->data;
  int tw = cogito_text_width_size_node(target, t, font_size);
  int th = cogito_text_height_size(font_size);
  int w = tw + pad_x * 2;
  int h = th + pad_y * 2;

  int mx = 0, my = 0;
  (void)cogito_pointer_query(&mx, &my, NULL, NULL);
  int x = mx + 12;
  int y = my + 16;
  if (x < 4) x = 4;
  if (x + w > win->w - 4) x = win->w - 4 - w;
  if (y + h > win->h - 4) y = win->h - 4 - h;
  if (y < 4) y = 4;

  cogito_draw_rect(x, y, w, h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines(x, y, w, h, border, radius, border_w);
  }
  cogito_draw_text_size_node(target, x + pad_x, y + pad_y, t, text, font_size, false);
}
