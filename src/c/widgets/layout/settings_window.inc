case COGITO_SETTINGS_WINDOW: {
  // HStack: sidebar (200px) + content area
  int sidebar_w = 200;
  int content_x = x + sidebar_w;
  int content_w = n->w - sidebar_w;
  if (content_w < 0) content_w = 0;

  // Active page is determined by n->selected
  int sel = n->selected >= 0 ? n->selected : 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if ((int)i == sel) {
      int cw = 0, ch = 0;
      cogito_intrinsic_size(c, &cw, &ch);
      cogito_layout_node(c, content_x, y, content_w, n->h);
    } else {
      // Off-screen
      cogito_layout_node(c, -9999, -9999, 0, 0);
    }
  }
  break;
}
