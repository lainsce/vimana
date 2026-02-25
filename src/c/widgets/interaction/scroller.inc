static CogitoNode* cogito_scroller_new_obj(void) {
  return cogito_node_new(COGITO_SCROLLER);
}
static ErgoVal cogito_scroller_new(void) {
  return EV_OBJ(cogito_scroller_new_obj());
}
static void cogito_scroller_set_axes(ErgoVal scv, ErgoVal horzv, ErgoVal vertv) {
  if (scv.tag != EVT_OBJ) ergo_trap("cogito.scroller_set_axes expects scroller");
  CogitoNode* sc = (CogitoNode*)scv.as.p;
  sc->scroller.horz = ergo_as_bool(horzv);
  sc->scroller.vert = ergo_as_bool(vertv);
  cogito_window_relayout(cogito_node_window(sc));
}
static CogitoNode* cogito_find_scroller(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_SCROLLER && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* child = cogito_find_scroller(active, x, y);
      if (child) return child;
    }
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* child = cogito_find_scroller(n->children[i], x, y);
      if (child) return child;
    }
  }
  if (n->kind == COGITO_SCROLLER && cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return n;
  return NULL;
}

// Find the deepest COGITO_CAROUSEL under the cursor (for wheel scrolling).
static CogitoNode* cogito_find_carousel(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_CAROUSEL && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* child = cogito_find_carousel(n->children[i], x, y);
    if (child) return child;
  }
  if (n->kind == COGITO_CAROUSEL && cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return n;
  return NULL;
}
