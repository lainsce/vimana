case COGITO_FAB: {
  bool nav_rail_expanded = (n->parent &&
    n->parent->kind == COGITO_NAV_RAIL &&
    n->parent->nav_rail.expanded);
  bool effective_extended = n->fab.extended || nav_rail_expanded;
  const char* label = (n->subtitle && n->subtitle->data && n->subtitle->data[0])
    ? n->subtitle->data
    : "Create";

  if (effective_extended) {
    // Extended FAB sizes: S(0)=56h, M(1)=80h, L(2)=96h
    int ext_h, ext_icon, ext_lpad, ext_gap, ext_rpad;
    if (n->fab.size == 0) {
      ext_h = 56; ext_icon = 24; ext_lpad = 16; ext_gap = 8; ext_rpad = 16;
    } else if (n->fab.size == 2) {
      ext_h = 96; ext_icon = 36; ext_lpad = 28; ext_gap = 16; ext_rpad = 28;
    } else {
      ext_h = 80; ext_icon = 28; ext_lpad = 26; ext_gap = 12; ext_rpad = 20;
    }
    int font_size = cogito_node_font_size(n);
    int label_w = cogito_text_width_size_node(n, label, font_size);
    w = ext_lpad + ext_icon + ext_gap + label_w + ext_rpad;
    // Extended FAB has minimum width of 80px
    if (w < 80) w = 80;
    h = ext_h;
  } else {
    // Standard circular FAB: S=56 (default), M=80, L=96
    int dim = (n->fab.size == 0) ? 56 : (n->fab.size == 2) ? 96 : 80;
    w = dim;
    h = dim;
  }
  break;
}
