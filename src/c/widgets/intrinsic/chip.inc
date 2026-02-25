case COGITO_CHIP: {
  int font_size = cogito_node_font_size(n);
  const char* t = n->text ? n->text->data : "";
  int text_w    = cogito_text_width_size_node(n, t, font_size);

  // Cogito: 12dp leading, 12dp trailing (or icon-zone + 4dp when selected)
  int lead_pad  = n->chip.selected ? (32 + 4) : 12; // icon_zone(h≈32) + gap
  int trail_pad = 12;
  if (n->chip.has_close) trail_pad = 18 + 8; // close button zone + gap
  w = lead_pad + text_w + trail_pad;
  h = 32; // Cogito: Chip height = 32dp
  break;
}