case COGITO_IMAGE:
      // Respect an explicit size first; otherwise fall back to the
      // intrinsic image dimensions, and finally a reasonable default.
      if (n->min_w_set && n->min_h_set) {
        w = n->min_w;
        h = n->min_h;
      } else if (n->image.loaded && n->image.w > 0 && n->image.h > 0) {
        w = n->image.w;
        h = n->image.h;
      } else {
        w = 100;
        h = 100;
      }
      break;
    
