static CogitoNode* cogito_avatar_new_obj(YisStr* text_or_icon) {
  CogitoNode* n = cogito_node_new(COGITO_AVATAR);
  if (text_or_icon && text_or_icon->data && text_or_icon->data[0]) {
    // If it looks like a file path or icon name, set as icon
    const char* s = text_or_icon->data;
    if (strchr(s, '/') || strchr(s, '.') || strncmp(s, "sf:", 3) == 0) {
      n->icon = text_or_icon;
      yis_retain_val(YV_STR(text_or_icon));
    } else {
      cogito_node_set_text(n, text_or_icon);
    }
  }
  return n;
}

static YisVal cogito_avatar_new(YisVal textv) {
  YisStr* ts = NULL;
  if (textv.tag == EVT_STR) {
    ts = (YisStr*)textv.as.p;
  } else if (textv.tag != EVT_NULL) {
    ts = stdr_to_string(textv);
  }
  CogitoNode* n = cogito_avatar_new_obj(ts);
  if (ts && textv.tag != EVT_STR) yis_release_val(YV_STR(ts));
  return YV_OBJ(n);
}

static void cogito_avatar_set_image(YisVal avatarv, YisVal pathv) {
  if (avatarv.tag != EVT_OBJ) yis_trap("cogito.avatar_set_image expects avatar");
  CogitoNode* n = (CogitoNode*)avatarv.as.p;
  YisStr* path = stdr_to_string(pathv);
  if (n->icon) yis_release_val(YV_STR(n->icon));
  n->icon = path;
  yis_retain_val(YV_STR(path));
  n->kd->image.loaded = false;
  n->kd->image.texture = NULL;
  cogito_window_relayout(cogito_node_window(n));
}
