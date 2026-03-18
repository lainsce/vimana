case COGITO_DIVIDER:
      if (n->kd->divider.vertical) {
        w = n->kd->divider.wavy ? 12 : 1;
        h = 24;
      } else {
        w = 24;
        h = n->kd->divider.wavy ? 12 : 1;
      }
      break;
    
