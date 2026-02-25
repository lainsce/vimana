case COGITO_TOOLBAR: {
      bool hover = false;
      bool active = false;
      bool floating = cogito_toolbar_is_floating(n);
      bool vibrant = floating && n->toolbar.vibrant;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      
      // Get tertiary container color for vibrant mode from Ensor scheme
      CogitoColor tertiary_container = {0};
      if (vibrant) {
        cogito_theme_get_tertiary(NULL, NULL, &tertiary_container, NULL);
      }
      
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      
      // Override with tertiary container color in vibrant mode
      if (vibrant) {
        n->bg = tertiary_container;
        n->bg_set = true;
      }
      
      // No border on floating toolbars
      n->border_width = 0;
      
      if (s.has_radius) {
        n->border_radius = s.radius;
      } else if (floating) {
        n->border_radius = n->h / 2;
      }
      if (n->bg_set) {
        int shadow_level = 0;
        if (n->shadow_set && n->shadow_level > 0) {
          shadow_level = n->shadow_level;
        } else if (floating) {
          shadow_level = 1;
        }
        if (shadow_level > 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, n->border_radius, shadow_level);
        }
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, n->bg, n->border_radius);
      }
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        if (!c || c->w <= 0 || c->h <= 0) continue;
        cogito_draw_node(c);
      }
      break;
    }
    
