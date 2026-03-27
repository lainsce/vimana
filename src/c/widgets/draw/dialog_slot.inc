case COGITO_DIALOG_SLOT:
      if (n->dialog && (n->dialog_open || n->dialog->overlay_anim_active)) {
        CogitoNode* win = cogito_node_window(n);
        if (win && win->dialog && win->dialog != n) break;
        if (win && win->dialog == n && !cogito_dialog_slot_force_draw) break;
        float p = cogito_overlay_anim_value(n->dialog, n->dialog_open);
        if (n->dialog->kind != COGITO_WELCOME_SCREEN) {
          CogitoStyle s = cogito_theme_resolve_node(n);
          CogitoColor scrim = s.has_bg ? s.bg : n->bg;
          if (scrim.a == 0) {
            CogitoStyle ws = cogito_theme_resolve(COGITO_WINDOW);
            scrim = ws.has_bg ? ws.bg : n->bg;
            scrim.a = 82;
          }
          scrim.a = (uint8_t)lroundf((float)scrim.a * p);
          if (win) {
            cogito_draw_rect(0, 0, win->w, win->h, scrim, 0);
          } else {
            cogito_draw_rect(n->x, n->y, n->w, n->h, scrim, 0);
          }
        }
        if (n->dialog->kind == COGITO_WELCOME_SCREEN) {
          cogito_overlay_draw_active = true;
          cogito_draw_node(n->dialog);
          cogito_overlay_draw_active = false;
        } else {
          if (p >= 0.999f) {
            cogito_overlay_draw_active = true;
            cogito_draw_node(n->dialog);
            cogito_overlay_draw_active = false;
          } else {
            int clip_h = (int)lroundf((float)n->dialog->h * p);
            if (clip_h > 0) {
              bool can_clip = cogito_backend && cogito_backend->begin_scissor && cogito_backend->end_scissor;
              if (can_clip) cogito_backend->begin_scissor(n->dialog->x, n->dialog->y, n->dialog->w, clip_h);
              cogito_overlay_draw_active = true;
              cogito_draw_node(n->dialog);
              cogito_overlay_draw_active = false;
              if (can_clip) cogito_backend->end_scissor();
            }
          }
        }
      }
      break;
