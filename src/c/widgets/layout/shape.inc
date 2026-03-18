case COGITO_SHAPE:
  if (n->kd->shape.clip_children) {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* c = n->children[i];
      int cw = n->w;
      int ch = n->h;
      cogito_layout_node(c, n->x, n->y, cw, ch);
    }
  }
  break;
