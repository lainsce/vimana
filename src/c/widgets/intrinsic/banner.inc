case COGITO_BANNER: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_t = s.has_padding_top ? s.padding_top : 16;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
  int size = cogito_node_font_size(n);
  int th = cogito_text_height_size(size);
  w = 324;
  h = pad_t + th + pad_b;
  if (h < 52) h = 52;
  break;
}
