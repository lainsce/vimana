static CogitoNode* cogito_avatar_new_obj(ErgoStr* text_or_icon) {
  CogitoNode* n = cogito_node_new(COGITO_AVATAR);
  if (text_or_icon && text_or_icon->data && text_or_icon->data[0]) {
    // If it looks like a file path or icon name, set as icon
    const char* s = text_or_icon->data;
    if (strchr(s, '/') || strchr(s, '.') || strncmp(s, "sf:", 3) == 0) {
      n->icon = text_or_icon;
      ergo_retain_val(EV_STR(text_or_icon));
    } else {
      cogito_node_set_text(n, text_or_icon);
    }
  }
  return n;
}

static ErgoVal cogito_avatar_new(ErgoVal textv) {
  ErgoStr* ts = NULL;
  if (textv.tag == EVT_STR) {
    ts = (ErgoStr*)textv.as.p;
  } else if (textv.tag != EVT_NULL) {
    ts = stdr_to_string(textv);
  }
  CogitoNode* n = cogito_avatar_new_obj(ts);
  if (ts && textv.tag != EVT_STR) ergo_release_val(EV_STR(ts));
  return EV_OBJ(n);
}

static void cogito_avatar_set_image(ErgoVal avatarv, ErgoVal pathv) {
  if (avatarv.tag != EVT_OBJ) ergo_trap("cogito.avatar_set_image expects avatar");
  CogitoNode* n = (CogitoNode*)avatarv.as.p;
  ErgoStr* path = stdr_to_string(pathv);
  if (n->icon) ergo_release_val(EV_STR(n->icon));
  n->icon = path;
  ergo_retain_val(EV_STR(path));
  n->image.loaded = false;
  n->image.texture = NULL;
  cogito_window_relayout(cogito_node_window(n));
}
