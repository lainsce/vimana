case COGITO_WEBVIEW: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      // Resolve base style first, then state overrides (hover only for now).
      CogitoStyle base = cogito_theme_resolve_node(n);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, false);
      CogitoColor bg = base.has_bg ? base.bg : n->bg;
      CogitoColor text_color = base.has_text ? base.text : n->text_color;
      int radius = base.has_radius ? base.radius : n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_radius) radius = s.radius;
      if (s.has_text) text_color = s.text;
      cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);
      if (text_color.a < 8) {
        if (cogito_cached_on_surface.a) text_color = cogito_cached_on_surface;
        else text_color = cogito_rgba(30, 30, 30, 255);
      }
      /* Compute visible rect: clamp to ancestor scroller viewport so the
       * background corners stay at the visible edges. */
      int wv_vis_x = n->x, wv_vis_y = n->y, wv_vis_w = n->w, wv_vis_h = n->h;
      bool wv_in_scroller = false;
      {
        CogitoNode* p = n->parent;
        while (p && p->kind != COGITO_SCROLLER) p = p->parent;
        if (p) {
          wv_in_scroller = true;
          int sv_l = p->x + p->padding_left;
          int sv_t = p->y + p->padding_top;
          int sv_r = p->x + p->w - p->padding_right;
          int sv_b = p->y + p->h - p->padding_bottom;
          if (wv_vis_x < sv_l) { wv_vis_w -= (sv_l - wv_vis_x); wv_vis_x = sv_l; }
          if (wv_vis_y < sv_t) { wv_vis_h -= (sv_t - wv_vis_y); wv_vis_y = sv_t; }
          if (wv_vis_x + wv_vis_w > sv_r) wv_vis_w = sv_r - wv_vis_x;
          if (wv_vis_y + wv_vis_h > sv_b) wv_vis_h = sv_b - wv_vis_y;
          if (wv_vis_w < 0) wv_vis_w = 0;
          if (wv_vis_h < 0) wv_vis_h = 0;
        }
      }
      if (n->bg_set) {
        int bg_tl = radius, bg_tr = radius, bg_br = radius, bg_bl = radius;
        cogito_resolve_node_radii(n, &s, radius, wv_vis_w, wv_vis_h, &bg_tl, &bg_tr, &bg_br, &bg_bl);
        if (wv_in_scroller) {
          if (wv_vis_y > n->y) { bg_tl = 0; bg_tr = 0; }
          if (wv_vis_y + wv_vis_h < n->y + n->h) { bg_br = 0; bg_bl = 0; }
        }
        cogito_draw_rect_radii_aa(wv_vis_x, wv_vis_y, wv_vis_w, wv_vis_h, bg, bg_tl, bg_tr, bg_br, bg_bl);
      }

      // Content area
      const char* html = (n->icon && n->icon->data) ? n->icon->data : "";
      if (!html[0]) break;

      int pad_l = n->padding_left;
      int pad_r = n->padding_right;
      int pad_t = n->padding_top;
      int pad_b = n->padding_bottom;
      int cx = n->x + pad_l;
      int cy = n->y + pad_t;
      int cw = n->w - pad_l - pad_r;
      int ch = n->h - pad_t - pad_b;
      if (cw <= 0 || ch <= 0) break;

      // Clip to content area, intersected with ancestor scroller viewport
      {
        int wx0 = cx, wy0 = cy, wx1 = cx + cw, wy1 = cy + ch;
        for (CogitoNode* pp = n->parent; pp; pp = pp->parent) {
          if (pp->kind == COGITO_SCROLLER) {
            int sx = pp->x + pp->padding_left;
            int sy = pp->y + pp->padding_top;
            int sx1 = sx + pp->w - pp->padding_left - pp->padding_right;
            int sy1 = sy + pp->h - pp->padding_top - pp->padding_bottom;
            if (wx0 < sx) wx0 = sx;
            if (wy0 < sy) wy0 = sy;
            if (wx1 > sx1) wx1 = sx1;
            if (wy1 > sy1) wy1 = sy1;
            break;
          }
        }
        int ww = wx1 - wx0, wh = wy1 - wy0;
        if (ww < 0) ww = 0;
        if (wh < 0) wh = 0;
        if (cogito_backend && cogito_backend->begin_scissor) {
          cogito_backend->begin_scissor(wx0, wy0, ww, wh);
        }
      }

      cogito_webview_draw_content(n, html, cx, cy, cw, text_color);

      if (cogito_backend && cogito_backend->end_scissor) {
        cogito_backend->end_scissor();
      }
      break;
    }
