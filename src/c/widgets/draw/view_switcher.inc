case COGITO_VIEWSWITCHER: {
      CogitoNode* active = cogito_view_switcher_active_child(n);
      if (active) cogito_draw_node(active);
      break;
    }
    
