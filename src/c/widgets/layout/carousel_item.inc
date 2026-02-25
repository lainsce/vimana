static void cogito_layout_carousel_item(CogitoNode* n, int x, int y) {
  // CarouselItem is laid out by its parent carousel with specific dimensions
  // Layout children using zstack-like behavior (stacked on top of each other)
  // Images are laid out flush to item edges; labels get padding + alignment.
  int inner_x = x + n->padding_left;
  int inner_y = y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  
  int align = n->align;
  if (align < 0 || align > 8) align = 0;
  int row = align / 3;
  int col = align % 3;
  
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    
    // Images are laid out flush to carousel item edges (no padding)
    if (c->kind == COGITO_IMAGE) {
      cogito_layout_node(c, x, y, n->w, n->h);
      continue;
    }
    
    int cw = 0;
    int ch = 0;
    int ox = 0;
    int oy = 0;
    int avail_w = inner_w - c->margin_left - c->margin_right;
    int avail_h = inner_h - c->margin_top - c->margin_bottom;
    if (avail_w < 0) avail_w = 0;
    if (avail_h < 0) avail_h = 0;
    
    cogito_intrinsic_size(c, &cw, &ch);
    
    // Handle expand flags
    if (c->hexpand) cw = avail_w;
    if (c->vexpand) ch = avail_h;
    
    // Apply alignment
    if (col == 1) ox = (avail_w - cw) / 2;
    else if (col == 2) ox = avail_w - cw;
    if (row == 1) oy = (avail_h - ch) / 2;
    else if (row == 2) oy = avail_h - ch;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    
    cogito_layout_node(c, inner_x + c->margin_left + ox, inner_y + c->margin_top + oy, cw, ch);
  }
}
