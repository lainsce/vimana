case COGITO_VIEW_CHOOSER: {
  int fs = cogito_node_font_size(n);
  int th = cogito_text_height_size(fs);
  w = 200;
  // Measure widest label for better intrinsic width
  if (n->tab_labels && n->tab_len > 0) {
    int max_tw = 0;
    for (int li = 0; li < (int)n->tab_len; li++) {
      if (n->tab_labels[li]) {
        int tw = cogito_text_width_size_node(n, n->tab_labels[li]->data, fs);
        if (tw > max_tw) max_tw = tw;
      }
    }
    // label padding (16) + icon space (12 + icon_size ~22 + gap 8)
    int label_w = max_tw + 16 + 42;
    if (label_w > w) w = label_w;
  }
  h = th + 16;
  if (h < 36) h = 36;
  break;
}
