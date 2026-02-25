static CogitoNode* cogito_badge_new_obj(int count) {
  CogitoNode* n = cogito_node_new(COGITO_BADGE);
  n->badge.count = count;
  if (count > 0) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", count);
    ErgoStr* s = stdr_str_lit(buf);
    cogito_node_set_text(n, s);
    ergo_release_val(EV_STR(s));
  }
  return n;
}

static ErgoVal cogito_badge_new(ErgoVal countv) {
  int count = 0;
  if (countv.tag == EVT_INT) {
    count = (int)countv.as.i;
  } else if (countv.tag == EVT_FLOAT) {
    count = (int)countv.as.f;
  }
  return EV_OBJ(cogito_badge_new_obj(count));
}

static void cogito_badge_set_count(ErgoVal badgev, ErgoVal countv) {
  if (badgev.tag != EVT_OBJ) ergo_trap("cogito.badge_set_count expects badge");
  CogitoNode* n = (CogitoNode*)badgev.as.p;
  int count = 0;
  if (countv.tag == EVT_INT) count = (int)countv.as.i;
  else if (countv.tag == EVT_FLOAT) count = (int)countv.as.f;
  n->badge.count = count;
  if (count > 0) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", count);
    ErgoStr* s = stdr_str_lit(buf);
    cogito_node_set_text(n, s);
    ergo_release_val(EV_STR(s));
  } else {
    cogito_node_set_text(n, NULL);
  }
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_badge_get_count(ErgoVal badgev) {
  if (badgev.tag != EVT_OBJ) ergo_trap("cogito.badge_get_count expects badge");
  CogitoNode* n = (CogitoNode*)badgev.as.p;
  return EV_INT(n->badge.count);
}
