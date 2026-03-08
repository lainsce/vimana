case COGITO_SIDE_SHEET: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = s.has_radius ? s.radius : (n->radius_set ? n->border_radius : 16);

  cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);

  // Draw modal scrim when the sheet is overlaid in ZStack/Window (not for sidebar mode).
  if (n->side_sheet.mode == COGITO_SIDE_SHEET_MODAL || n->side_sheet.mode == COGITO_SIDE_SHEET_INSET) {
    CogitoNode* host = n->parent;
    if (host && host->kind != COGITO_DIALOG_SLOT &&
        (host->kind == COGITO_ZSTACK || host->kind == COGITO_WINDOW)) {
      CogitoStyle slot_style = cogito_theme_resolve(COGITO_DIALOG_SLOT);
      CogitoColor scrim = slot_style.has_bg ? slot_style.bg : n->bg;
      if (scrim.a == 0) {
        CogitoStyle ws = cogito_theme_resolve(COGITO_WINDOW);
        scrim = ws.has_bg ? ws.bg : n->bg;
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
  }

  int r_x = n->x, r_y = n->y, r_w = n->w, r_h = n->h;
  if (n->side_sheet.mode == COGITO_SIDE_SHEET_INSET) {
      r_x += 16;
      r_y += 16;
      r_w -= 32;
      r_h -= 32;
  }
  // Draw docked sheet surface.
  cogito_draw_rect_node(n, &s, r_x, r_y, r_w, r_h, bg, radius);

  // Draw border on inner edge: right edge in RTL, left edge in LTR
  if (n->side_sheet.mode == COGITO_SIDE_SHEET_SIDEBAR) {
      CogitoStyle outline_style = cogito_theme_resolve(COGITO_SIDE_SHEET);
      CogitoColor border_color = n->border_color;
      if (outline_style.has_bg) {
          border_color = outline_style.bg;
      } else {
          CogitoStyle ws = cogito_theme_resolve(COGITO_WINDOW);
          if (ws.has_bg) border_color = ws.bg;
      }
      int bx = cogito_is_rtl() ? (r_x + r_w - 1) : r_x;
      cogito_draw_rect(bx, r_y, 1, r_h, border_color, 0);
  }

  // Draw title (clipped so it does not overlap the close button)
  bool sheet_rtl = cogito_is_rtl();
  int handle_w = (n->side_sheet.mode == COGITO_SIDE_SHEET_SIDEBAR)
    ? 0
    : COGITO_SIDE_SHEET_HANDLE_TOUCH_W;
  if (n->text && n->text->data && n->text->data[0]) {
    int pad_l = n->padding_left;
    int pad_r = n->padding_right;
    int pad_t = n->padding_top;
    int size = cogito_node_font_size(n);
    // Reserve space for the close button (24px pad + 24px box + 8px gap = 56px)
    int btn_reserve = 24 + 24 + 8;
    int title_x, max_title_w;
    if (sheet_rtl) {
      // RTL: close button on left, title on right
      title_x = r_x + btn_reserve;
      max_title_w = r_x + r_w - handle_w - pad_r - title_x;
    } else {
      // LTR: close button on right, title on left
      title_x = r_x + handle_w + pad_l;
      max_title_w = (r_x + r_w - btn_reserve) - title_x;
    }
    if (max_title_w > 0) {
      cogito_draw_text_ellipsis(n, title_x, r_y + pad_t + 12,
                                max_title_w, n->text->data, text_color, size,
      true, false,
      sheet_rtl ? 2 : 0);
    }
  }

  // Close button: left side in RTL, right side in LTR
  int cw = 24;
  int ch = 24;
  int cx = sheet_rtl ? (r_x + 24) : (r_x + r_w - 24 - cw);
  int cy = r_y + 24;
  if (cw > 0 && ch > 0) {
    int mx = 0, my = 0;
    bool down = false;
    (void)cogito_pointer_query(&mx, &my, &down, NULL);
    bool over = cogito_hit_rect(mx, my, cx, cy, cw, ch);
    CogitoColor cbg = text_color;
    cbg.a = 36;
    if (over) cbg.a = down ? 72 : 48;
    int cr = cw / 2;
    if (cr < 4) cr = 4;
    cogito_draw_rect(cx, cy, cw, ch, cbg, cr);
    int icon_size = cw - 10;
    if (icon_size < 10) icon_size = 10;
    int ix = cx + (cw - icon_size) / 2;
    int iy = cy + (ch - icon_size) / 2;
    cogito_draw_icon_fallback(n,
    "xmark", ix, iy, icon_size, text_color);
  }
  // Draw children
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i]);
  }
  break;
}
