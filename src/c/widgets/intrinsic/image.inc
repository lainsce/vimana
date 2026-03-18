case COGITO_IMAGE:
      // Respect an explicit size first; otherwise fall back to the
      // intrinsic image dimensions, and finally a reasonable default.
      if (n->min_w_set && n->min_h_set) {
        w = n->min_w;
        h = n->min_h;
      } else if (n->kd->image.loaded && n->kd->image.w > 0 && n->kd->image.h > 0) {
        w = n->kd->image.w;
        h = n->kd->image.h;
      } else {
        w = 100;
        h = 100;
      }
      break;
    
