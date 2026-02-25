case COGITO_BOTTOM_NAV:
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        if (!c) continue;
        if (c->kind == COGITO_FAB) {
          int cw = 0, ch = 0;
          cogito_intrinsic_size(c, &cw, &ch);
          cogito_layout_node(c, n->x, n->y, cw, ch);
        } else {
          cogito_layout_node(c, n->x, n->y, 0, 0);
        }
      }
      break;
