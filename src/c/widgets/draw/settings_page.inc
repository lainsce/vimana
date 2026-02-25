case COGITO_SETTINGS_PAGE: {
  // Draw children (VStack-like)
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i]);
  }
  break;
}
