static bool cogito_divider_orientation_vertical(ErgoVal orientationv) {
  ErgoStr* s = NULL;
  bool temp = false;
  bool vertical = false;
  if (orientationv.tag == EVT_STR) {
    s = (ErgoStr*)orientationv.as.p;
  } else if (orientationv.tag != EVT_NULL) {
    s = stdr_to_string(orientationv);
    temp = true;
  }
  if (s && s->data) {
    const char* o = s->data;
    vertical = strcmp(o, "vertical") == 0 ||
               strcmp(o, "v") == 0 ||
               strcmp(o, "column") == 0 ||
               strcmp(o, "y") == 0;
  }
  if (temp && s) ergo_release_val(EV_STR(s));
  return vertical;
}

static CogitoNode* cogito_divider_new_obj(bool vertical, bool inset) {
  CogitoNode* d = cogito_node_new(COGITO_DIVIDER);
  d->divider.vertical = vertical;
  d->divider.inset = inset;
  if (vertical) d->vexpand = true;
  else d->hexpand = true;
  return d;
}

static ErgoVal cogito_divider_new(ErgoVal orientationv, ErgoVal insetv) {
  bool vertical = cogito_divider_orientation_vertical(orientationv);
  bool inset = ergo_as_bool(insetv);
  return EV_OBJ(cogito_divider_new_obj(vertical, inset));
}
