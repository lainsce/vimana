static CogitoNode* cogito_card_new_obj(YisStr* title) {
  CogitoNode* n = cogito_node_new(COGITO_CARD);
  if (title && title->data && title->data[0]) {
    cogito_node_set_text(n, title);
  }
  n->gap = 8;
  n->shadow_level = 1;
  return n;
}

static YisVal cogito_card_new(YisVal titlev) {
  YisStr* ts = NULL;
  if (titlev.tag == EVT_STR) {
    ts = (YisStr*)titlev.as.p;
  } else if (titlev.tag != EVT_NULL) {
    ts = stdr_to_string(titlev);
  }
  CogitoNode* n = cogito_card_new_obj(ts);
  if (ts && titlev.tag != EVT_STR) yis_release_val(EV_STR(ts));
  return EV_OBJ(n);
}
