static CogitoNode* cogito_badge_new_obj(int count) {
  CogitoNode* n = cogito_node_new(COGITO_BADGE);
  n->badge.count = count;
  if (count > 0) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", count);
    YisStr* s = stdr_str_lit(buf);
    cogito_node_set_text(n, s);
    yis_release_val(YV_STR(s));
  }
  return n;
}

static YisVal cogito_badge_new(YisVal countv) {
  int count = 0;
  if (countv.tag == EVT_INT) {
    count = (int)countv.as.i;
  } else if (countv.tag == EVT_FLOAT) {
    count = (int)countv.as.f;
  }
  return YV_OBJ(cogito_badge_new_obj(count));
}

static void cogito_badge_set_count(YisVal badgev, YisVal countv) {
  if (badgev.tag != EVT_OBJ) yis_trap("cogito.badge_set_count expects badge");
  CogitoNode* n = (CogitoNode*)badgev.as.p;
  int count = 0;
  if (countv.tag == EVT_INT) count = (int)countv.as.i;
  else if (countv.tag == EVT_FLOAT) count = (int)countv.as.f;
  n->badge.count = count;
  if (count > 0) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", count);
    YisStr* s = stdr_str_lit(buf);
    cogito_node_set_text(n, s);
    yis_release_val(YV_STR(s));
  } else {
    cogito_node_set_text(n, NULL);
  }
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_badge_get_count(YisVal badgev) {
  if (badgev.tag != EVT_OBJ) yis_trap("cogito.badge_get_count expects badge");
  CogitoNode* n = (CogitoNode*)badgev.as.p;
  return YV_INT(n->badge.count);
}
