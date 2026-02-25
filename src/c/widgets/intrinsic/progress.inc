case COGITO_PROGRESS: {
      CogitoStyle s = cogito_theme_resolve_node(n);
      int thickness = n->track_height_set ? n->track_height : (s.has_track_height ? s.track_height : 4);
      if (thickness < 1) thickness = 1;
      if (n->progress.circular) {
        int d = thickness * 8;
        if (d < 28) d = 28;
        w = d;
        h = d;
      } else {
        w = 160;
        h = thickness + 6;
        if (h < 10) h = 10;
      }
      break;
    }
    
