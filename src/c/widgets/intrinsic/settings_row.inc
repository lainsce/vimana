case COGITO_SETTINGS_ROW: {
  int th = cogito_text_height();
  int child_w = 0, child_h = 0;
  if (n->len > 0) cogito_intrinsic_size(n->children[0], &child_w, &child_h);
  w = 400;
  h = th + 24;
  if (child_h + 12 > h) h = child_h + 12;
  if (h < 48) h = 48;
  break;
}
