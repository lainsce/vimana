case COGITO_TABS: {
      int size = cogito_node_font_size(n);
      int th = cogito_text_height_size(size);
      const int icon_size = 24;
      bool has_any_icon = false;
      if (n->tab_icons) {
        for (size_t i = 0; i < n->tab_len; i++) {
          if (n->tab_icons[i] && n->tab_icons[i]->data && n->tab_icons[i]->data[0]) {
            has_any_icon = true;
            break;
          }
        }
      }
      w = 320;
      if (has_any_icon) {
        h = th + icon_size + 18;
        if (h < 64) h = 64;
      } else {
        h = 42;
      }
      break;
    }
    
