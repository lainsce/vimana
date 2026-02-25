static CogitoNode* cogito_menu_button_new_obj(ErgoStr* icon) {
  CogitoNode* n = cogito_node_new(COGITO_ICONBTN);
  n->button.icon_shape = 0; // Default to circle (matches spec)
  n->button.size = COGITO_BUTTON_SIZE_M; // Default to M size
  n->button.icon_width = 1; // Default to square (default width)
  cogito_node_set_text(n, icon);
  return n;
}
static ErgoVal cogito_menu_button_new(ErgoVal iconv) {
  ErgoStr* ts = stdr_to_string(iconv);
  CogitoNode* n = cogito_menu_button_new_obj(ts);
  return EV_OBJ(n);
}
