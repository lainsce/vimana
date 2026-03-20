// VirtualList draw — render visible items with clipping.

static void cogito_draw_virtual_list(CogitoNode *n) {
  // Draw background if set
  if (n->bg_set) {
    cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
  }

  // Clip to widget bounds
  cogito_backend->begin_scissor(n->x, n->y, n->w, n->h);
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i]);
  }
  cogito_backend->end_scissor();

  // Draw border if set
  if (n->border_width > 0 && n->border_color_set) {
    cogito_draw_rect_lines_node(n, NULL, n->x, n->y, n->w, n->h,
                                 n->border_color, n->border_radius,
                                 n->border_width);
  }

  // Draw scrollbar indicator
  CogitoVirtualListData *vl = &n->kd->virtual_list;
  int total_h = vl->item_count * vl->item_height;
  if (total_h > vl->viewport_h && vl->viewport_h > 0) {
    int bar_w = 4;
    int bar_x = n->x + n->w - bar_w - 2;
    int bar_area_h = n->h - n->padding_top - n->padding_bottom;
    double ratio = (double)vl->viewport_h / (double)total_h;
    int bar_h = (int)(bar_area_h * ratio);
    if (bar_h < 20) bar_h = 20;
    double scroll_ratio = (total_h > vl->viewport_h)
      ? (double)vl->scroll_y / (double)(total_h - vl->viewport_h) : 0.0;
    int bar_y = n->y + n->padding_top + (int)((bar_area_h - bar_h) * scroll_ratio);

    CogitoColor bar_color = {128, 128, 128, 80};
    cogito_draw_rect(bar_x, bar_y, bar_w, bar_h, bar_color, 2);
  }
}
