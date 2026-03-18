static int cogito_bottom_sheet_min_height(CogitoNode* n) {
  if (!n) return 100;
  int pad_t = n->padding_top;
  int pad_b = n->padding_bottom;
  int handle_h = 28;
  int gap = n->gap > 0 ? n->gap : 8;
  int title_h = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    title_h = cogito_text_height_size(ts) + gap;
  }
  int content_h = 0;
  int action_h = 0;
  int action_count = 0;
  int content_count = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    bool is_action = c->view_id && c->view_id->data && strcmp(c->view_id->data, "bottom-sheet-action") == 0;
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (is_action) {
      action_h += ch + c->margin_top + c->margin_bottom;
      if (action_count > 0) action_h += gap;
      action_count++;
    } else {
      content_h += ch + c->margin_top + c->margin_bottom;
      if (content_count > 0) content_h += gap;
      content_count++;
    }
  }
  int h = handle_h + title_h + pad_t + content_h + pad_b;
  if (action_count > 0) {
    h += gap + action_h;
  }
  if (h < 100) h = 100;
  return h;
}

static int cogito_bottom_sheet_top_inset(int host_y) {
  return host_y <= 0 ? cogito_appbar_titlebar_inset() : 0;
}

static int cogito_bottom_sheet_settle_height(CogitoNode* n, int host_y, int host_h) {
  int min_h = cogito_bottom_sheet_min_height(n);
  if (host_h <= 0) return min_h;
  int top_inset = cogito_bottom_sheet_top_inset(host_y);
  int max_h = host_h - top_inset;
  if (max_h < min_h) max_h = min_h;
  int settle_h = (int)lround((double)host_h * (double)COGITO_BOTTOM_SHEET_DEFAULT_RATIO);
  if (settle_h < min_h) settle_h = min_h;
  if (settle_h > max_h) settle_h = max_h;
  return settle_h;
}

static int cogito_bottom_sheet_target_height(CogitoNode* n, int host_y, int host_h) {
  int min_h = cogito_bottom_sheet_min_height(n);
  if (host_h <= 0) return min_h;
  int top_inset = cogito_bottom_sheet_top_inset(host_y);
  int max_h = host_h - top_inset;
  if (max_h < min_h) max_h = min_h;
  int target_h = n->kd->bottom_sheet.desired_h > 0
    ? n->kd->bottom_sheet.desired_h
    : cogito_bottom_sheet_settle_height(n, host_y, host_h);
  if (target_h < min_h) target_h = min_h;
  if (target_h > max_h) target_h = max_h;
  return target_h;
}

static int cogito_bottom_sheet_snap_height(CogitoNode* n, int host_y, int host_h, int from_h) {
  int min_h = cogito_bottom_sheet_min_height(n);
  int settle_h = cogito_bottom_sheet_settle_height(n, host_y, host_h);
  int top_inset = cogito_bottom_sheet_top_inset(host_y);
  int max_h = host_h - top_inset;
  if (max_h < min_h) max_h = min_h;

  int d_min = abs(from_h - min_h);
  int d_settle = abs(from_h - settle_h);
  int d_max = abs(from_h - max_h);
  if (d_max <= d_settle && d_max <= d_min) return max_h;
  if (d_min <= d_settle) return min_h;
  return settle_h;
}

static void cogito_bottom_sheet_handle_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!n) return;
  int w = 96;
  int h = COGITO_BOTTOM_SHEET_HANDLE_TOUCH_H;
  int x = n->x + (n->w - w) / 2;
  int y = n->y;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
}

static CogitoNode* cogito_bottom_sheet_new_obj(YisStr* title) {
  CogitoNode* n = cogito_node_new(COGITO_BOTTOM_SHEET);
  if (title && title->data && title->data[0]) {
    cogito_node_set_text(n, title);
  }
  n->hexpand = true;
  n->gap = 8;
  n->kd->bottom_sheet.dragging = false;
  n->kd->bottom_sheet.expanded = false;
  n->kd->bottom_sheet.drag_start_y = 0;
  n->kd->bottom_sheet.drag_start_h = 0;
  n->kd->bottom_sheet.desired_h = 0;
  return n;
}

static YisVal cogito_bottom_sheet_new(YisVal titlev) {
  YisStr* ts = NULL;
  if (titlev.tag == EVT_STR) {
    ts = (YisStr*)titlev.as.p;
  } else if (titlev.tag != EVT_NULL) {
    ts = stdr_to_string(titlev);
  }
  CogitoNode* n = cogito_bottom_sheet_new_obj(ts);
  if (ts && titlev.tag != EVT_STR) yis_release_val(YV_STR(ts));
  return YV_OBJ(n);
}
