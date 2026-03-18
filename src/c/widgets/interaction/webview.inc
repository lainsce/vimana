static CogitoNode* cogito_webview_new_obj(YisStr* url) {
  CogitoNode* n = cogito_node_new(COGITO_WEBVIEW);
  cogito_node_set_icon(n, url);
  n->kd->webview.open_external_on_click = false;
  return n;
}

static YisVal cogito_webview_new(YisVal urlv) {
  YisStr* url = stdr_to_string(urlv);
  return YV_OBJ(cogito_webview_new_obj(url));
}

static void cogito_webview_set_url(YisVal wvv, YisVal urlv) {
  if (wvv.tag != EVT_OBJ) yis_trap("cogito.webview_set_url expects webview");
  CogitoNode* n = (CogitoNode*)wvv.as.p;
  if (!n || n->kind != COGITO_WEBVIEW) yis_trap("cogito.webview_set_url expects webview");
  YisStr* url = stdr_to_string(urlv);
  cogito_node_set_icon(n, url);
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_webview_get_url(YisVal wvv) {
  if (wvv.tag != EVT_OBJ) yis_trap("cogito.webview_get_url expects webview");
  CogitoNode* n = (CogitoNode*)wvv.as.p;
  if (!n || n->kind != COGITO_WEBVIEW) yis_trap("cogito.webview_get_url expects webview");
  YisStr* s = n->icon ? n->icon : stdr_str_lit("");
  yis_retain_val(YV_STR(s));
  return YV_STR(s);
}

static void cogito_webview_set_open_external_on_click(YisVal wvv, YisVal onv) {
  if (wvv.tag != EVT_OBJ) yis_trap("cogito.webview_set_open_external_on_click expects webview");
  CogitoNode* n = (CogitoNode*)wvv.as.p;
  if (!n || n->kind != COGITO_WEBVIEW) yis_trap("cogito.webview_set_open_external_on_click expects webview");
  n->kd->webview.open_external_on_click = yis_as_bool(onv);
}

static YisVal cogito_webview_get_open_external_on_click(YisVal wvv) {
  if (wvv.tag != EVT_OBJ) yis_trap("cogito.webview_get_open_external_on_click expects webview");
  CogitoNode* n = (CogitoNode*)wvv.as.p;
  if (!n || n->kind != COGITO_WEBVIEW) yis_trap("cogito.webview_get_open_external_on_click expects webview");
  return YV_BOOL(n->kd->webview.open_external_on_click);
}

static YisVal cogito_webview_open(YisVal wvv) {
  if (wvv.tag != EVT_OBJ) yis_trap("cogito.webview_open expects webview");
  CogitoNode* n = (CogitoNode*)wvv.as.p;
  if (!n || n->kind != COGITO_WEBVIEW) yis_trap("cogito.webview_open expects webview");
  if (!n->icon || !n->icon->data || !n->icon->data[0]) return YV_BOOL(false);
  if (cogito_backend && cogito_backend->open_url) {
    cogito_backend->open_url(n->icon->data);
    return YV_BOOL(true);
  }
  return YV_BOOL(false);
}
