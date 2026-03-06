case COGITO_VIEWSWITCHER: {
      CogitoNode* active = cogito_view_switcher_active_child(n);
      bool can_clip = cogito_backend && cogito_backend->begin_scissor && cogito_backend->end_scissor;
      if (active) {
        if (can_clip) cogito_backend->begin_scissor(n->x, n->y, n->w, n->h);
        cogito_draw_node(active);
        if (can_clip) cogito_backend->end_scissor();
      }
      break;
    }
    
