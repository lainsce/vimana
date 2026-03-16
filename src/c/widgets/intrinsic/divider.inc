case COGITO_DIVIDER:
      if (n->divider.vertical) {
        w = n->divider.wavy ? 12 : 1;
        h = 24;
      } else {
        w = 24;
        h = n->divider.wavy ? 12 : 1;
      }
      break;
    
