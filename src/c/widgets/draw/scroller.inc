case COGITO_SCROLLER: {
      bool hover = false, active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = s.has_bg ? s.bg : n->bg;

      if (n->len > 0) {
        cogito_anim_apply(n, &s, &bg, NULL, NULL, NULL);
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg,
    0);

        if (cogito_backend && cogito_backend->begin_scissor) {
          cogito_backend->begin_scissor(n->x, n->y, n->w, n->h);
    }
        for (size_t i = 0; i < n->len; i++) {
          cogito_draw_node(n->children[i
      ]);
    }
        if (cogito_backend && cogito_backend->end_scissor) {
          cogito_backend->end_scissor();
    }
  }
  /* Overlay scrollbars: narrow (4px) when cursor over scroller but not on bar, wide (8px) when on bar; fade after 0.3s when cursor leaves scroller */
  {
        bool cursor_over_scroller = cogito_main_pointer_valid &&
            cogito_hit_rect(cogito_main_pointer_x, cogito_main_pointer_y, n->x, n->y, n->w, n->h);
        bool prev_bar_hovered = n->scroller.bar_hovered;
        bool cursor_over_sb = false;
        if (cursor_over_scroller && cogito_main_pointer_valid) {
          int hit_w = n->scroller.bar_hovered ? COGITO_SCROLLBAR_WIDTH_WIDE : COGITO_SCROLLBAR_WIDTH_NARROW;
          cursor_over_sb = cogito_scroller_hit_scrollbar(
              n, cogito_main_pointer_x, cogito_main_pointer_y, hit_w, hit_w) != COGITO_SCROLLBAR_HIT_NONE;
          if (!cursor_over_sb && n->scroller.bar_hovered) {
        // Fallback to narrow hit-test at the transition boundary.
            cursor_over_sb = cogito_scroller_hit_scrollbar(
                n, cogito_main_pointer_x, cogito_main_pointer_y,
                COGITO_SCROLLBAR_WIDTH_NARROW, COGITO_SCROLLBAR_WIDTH_NARROW) != COGITO_SCROLLBAR_HIT_NONE;
      }
    }
        n->scroller.bar_hovered = cursor_over_sb;
        int sb_w = n->scroller.bar_hovered ? COGITO_SCROLLBAR_WIDTH_WIDE : COGITO_SCROLLBAR_WIDTH_NARROW;
        int sb_h = sb_w;
        double now = cogito_now();
        bool bar_alpha_changed = false;
        if (cursor_over_scroller) {
          if (cursor_over_sb) {
            if (n->scroller.bar_alpha < 255) {
              n->scroller.bar_alpha = (uint8_t)(n->scroller.bar_alpha + 51) > 255 ? 255 : (n->scroller.bar_alpha + 51);
              bar_alpha_changed = true;
        }
      } else {
            if (n->scroller.bar_alpha < 200) {
              n->scroller.bar_alpha = (uint8_t)(n->scroller.bar_alpha + 40) > 200 ? 200 : (n->scroller.bar_alpha + 40);
              bar_alpha_changed = true;
        }
      }
          n->scroller.bar_hover_until = 0.0;
    } else {
          if (n->scroller.bar_alpha > 0 && n->scroller.bar_hover_until == 0.0) {
            n->scroller.bar_hover_until = now + 0.3;
      }
          if (n->scroller.bar_hover_until > 0.0 && now >= n->scroller.bar_hover_until) {
            n->scroller.bar_alpha = (uint8_t)((int)n->scroller.bar_alpha * 220 / 255);
            if (n->scroller.bar_alpha > 0) {
          // Request continued redraw via frame counter instead of queue_draw
          // Request continuous redraw for animation
              double anim_end = now + 0.016; // Request ~1 frame more of animation
              if (anim_end > cogito_anim_end_time) cogito_anim_end_time = anim_end;
        }
      }
    }
        if (bar_alpha_changed || prev_bar_hovered != n->scroller.bar_hovered) {
          double anim_end = now + 0.016;
          if (anim_end > cogito_anim_end_time) cogito_anim_end_time = anim_end;
    }
        if (n->scroller.bar_alpha > 0) {
      // Scrollbar uses on-surface tint derived from theme text, not hardcoded grey
          CogitoColor on_surface = s.has_text ? s.text : n->text_color;
          CogitoColor track = on_surface;
          track.a = (uint8_t)((int)n->scroller.bar_alpha * 30 / 255);
          CogitoColor thumb = on_surface;
          thumb.a = (uint8_t)((int)n->scroller.bar_alpha * 160 / 255);
          int vtx, vty, vtw, vth, vux, vuy, vuw, vuh;
          if (cogito_scroller_vert_scrollbar_rects(n, sb_w, &vtx, &vty, &vtw, &vth, &vux, &vuy, &vuw, &vuh)) {
            cogito_draw_rect(vtx, vty, vtw, vth, track,
        4);
            cogito_draw_rect(vux, vuy, vuw, vuh, thumb,
        4);
      }
          int htx, hty, htw, hth, hux, huy, huw, huh;
          if (cogito_scroller_horz_scrollbar_rects(n, sb_h, &htx, &hty, &htw, &hth, &hux, &huy, &huw, &huh)) {
            cogito_draw_rect(htx, hty, htw, hth, track,
        4);
            cogito_draw_rect(hux, huy, huw, huh, thumb,
        4);
      }
    }
  }
      break;
}
