static CogitoNode* cogito_find_tooltip_node(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_SCROLLER && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  if (n->kind == COGITO_DIALOG && !cogito_hit_node(n, x, y)) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* hit = cogito_find_tooltip_node(active, x, y);
      if (hit) return hit;
    }
  } else {
    for (size_t i = n->len; i > 0; i--) {
      CogitoNode* hit = cogito_find_tooltip_node(n->children[i - 1], x, y);
      if (hit) return hit;
    }
  }
  if (n->tooltip && n->tooltip->data && n->tooltip->data[0] && cogito_hit_node(n, x, y) && !n->disabled) {
    return n;
  }
  return NULL;
}
