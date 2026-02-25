case COGITO_DIALOG_SLOT:
      if (n->dialog && n->dialog_open) {
        CogitoNode* win = cogito_node_window(n);
        if (win && win->dialog && win->dialog != n) break;
        if (win && win->dialog == n && !cogito_dialog_slot_force_draw) break;
        CogitoStyle s = cogito_theme_resolve_node(n);
        CogitoColor scrim = s.has_bg ? s.bg : n->bg;
        if (scrim.a == 0) {
          CogitoStyle ws = cogito_theme_resolve(COGITO_WINDOW);
          scrim = ws.has_bg ? ws.bg : n->bg;
          scrim.a = 110;
        }
        if (win) {
          cogito_draw_rect(0, 0, win->w, win->h, scrim, 0);
        } else {
          cogito_draw_rect(n->x, n->y, n->w, n->h, scrim, 0);
        }
        cogito_draw_node(n->dialog);
      }
      break;
