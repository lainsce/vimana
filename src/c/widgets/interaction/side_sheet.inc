static void cogito_side_sheet_close_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!n || n->kind != COGITO_SIDE_SHEET) return;
  // Close button: in LTR at top-right of sheet; in RTL at top-left
  int cw = 24;
  int ch = 24;
  int cx = cogito_is_rtl() ? (n->x + 24) : (n->x + n->w - 24 - cw);
  int cy = n->y + 24;
  if (out_x) *out_x = cx;
  if (out_y) *out_y = cy;
  if (out_w) *out_w = cw;
  if (out_h) *out_h = ch;
}

static int cogito_side_sheet_min_width(CogitoNode* n) {
  if (!n) return 200;
  int handle_w = (n->kd->side_sheet.mode == COGITO_SIDE_SHEET_SIDEBAR)
    ? 0
    : COGITO_SIDE_SHEET_HANDLE_TOUCH_W;
  int pad_l = n->padding_left;
  int pad_r = n->padding_right;
  int title_w = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    title_w = cogito_text_width_size_node(n, n->text->data, ts);
  }
  // Keep title clear of the close button area (24 right pad + 24 button + 8 gap).
  int close_lane_w = 56;
  int edge_pad = pad_l > pad_r ? pad_l : pad_r;
  int header_w = handle_w + edge_pad + title_w + close_lane_w;

  // SideSheet content is vertically stacked, so min width is max row width, not sum.
  int content_w = 0;
  for (size_t i = 0; i < n->len; i++) {
    int cw = 0, ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    int row_w = cw + n->children[i]->margin_left + n->children[i]->margin_right;
    if (row_w > content_w) content_w = row_w;
  }

  int body_w = handle_w + pad_l + content_w + pad_r;
  int w = body_w > header_w ? body_w : header_w;
  if (w < 200) w = 200;
  return w;
}

static int cogito_side_sheet_left_inset(int host_x) {
  return host_x <= 0 ? 0 : 0; // TODO: maybe appbar inset?
}

static int cogito_side_sheet_settle_width(CogitoNode* n, int host_x, int host_w) {
  int min_w = cogito_side_sheet_min_width(n);
  if (host_w <= 0) return min_w;
  int left_inset = cogito_side_sheet_left_inset(host_x);
  int max_w = host_w - left_inset;
  if (max_w < 1) max_w = 1;
  if (max_w < min_w) return max_w;
  int settle_w = (int)lround((double)host_w * (double)COGITO_SIDE_SHEET_DEFAULT_RATIO);
  if (settle_w < min_w) settle_w = min_w;
  if (settle_w > max_w) settle_w = max_w;
  return settle_w;
}

static int cogito_side_sheet_target_width(CogitoNode* n, int host_x, int host_w) {
  int min_w = cogito_side_sheet_min_width(n);
  if (host_w <= 0) return min_w;
  int left_inset = cogito_side_sheet_left_inset(host_x);
  int max_w = host_w - left_inset;
  if (max_w < 1) max_w = 1;
  if (max_w < min_w) return max_w;
  int target_w = n->kd->side_sheet.desired_w > 0
    ? n->kd->side_sheet.desired_w
    : cogito_side_sheet_settle_width(n, host_x, host_w);
  if (target_w < min_w) target_w = min_w;
  if (target_w > max_w) target_w = max_w;
  return target_w;
}

static int cogito_side_sheet_snap_width(CogitoNode* n, int host_x, int host_w, int from_w) {
  int min_w = cogito_side_sheet_min_width(n);
  int settle_w = cogito_side_sheet_settle_width(n, host_x, host_w);
  int left_inset = cogito_side_sheet_left_inset(host_x);
  int max_w = host_w - left_inset;
  if (max_w < 1) max_w = 1;
  if (max_w < min_w) return max_w;

  int d_min = abs(from_w - min_w);
  int d_settle = abs(from_w - settle_w);
  int d_max = abs(from_w - max_w);
  if (d_max <= d_settle && d_max <= d_min) return max_w;
  if (d_min <= d_settle) return min_w;
  return settle_w;
}

static void cogito_side_sheet_handle_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!n) return;
  if (n->kd->side_sheet.mode == COGITO_SIDE_SHEET_SIDEBAR) {
    if (out_x) *out_x = n->x;
    if (out_y) *out_y = n->y;
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    return;
  }
  int w = COGITO_SIDE_SHEET_HANDLE_TOUCH_W;
  int h = 96;
  // In LTR sheet is on right → handle on left edge; in RTL sheet is on left → handle on right edge
  int x = cogito_is_rtl() ? (n->x + n->w - w) : n->x;
  int y = n->y + (n->h - h) / 2;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
}

void cogito_side_sheet_set_mode(CogitoNode* n, int mode) {
    if (!n || n->kind != COGITO_SIDE_SHEET) return;
  if (mode < COGITO_SIDE_SHEET_MODAL || mode > COGITO_SIDE_SHEET_SIDEBAR) {
    mode = COGITO_SIDE_SHEET_MODAL;
  }
    n->kd->side_sheet.mode = mode;
  n->kd->side_sheet.preferred_mode = mode;
    // Set class name for theme styling based on mode
    const char* class_name = NULL;
    switch (mode) {
        case 0: class_name = "modal"; break;
        case 1: class_name = "inset"; break;
        case 2: class_name = "sidebar"; break;
        default: class_name = "modal"; break;
    }
    if (class_name) {
        YisStr* cls = stdr_str_from_slice(class_name, strlen(class_name));
        if (n->class_name) yis_release_val(YV_STR(n->class_name));
        n->class_name = cls;
        yis_retain_val(YV_STR(cls));
    }
}

static CogitoNode* cogito_side_sheet_new_obj(YisStr* title) {
  CogitoNode* n = cogito_node_new(COGITO_SIDE_SHEET);
  if (title && title->data && title->data[0]) {
    cogito_node_set_text(n, title);
  }
  n->vexpand = true;
  n->gap = 8;
  n->kd->side_sheet.dragging = false;
  n->kd->side_sheet.expanded = false;
  n->kd->side_sheet.drag_start_x = 0;
  n->kd->side_sheet.drag_start_w = 0;
  n->kd->side_sheet.desired_w = 0;
  n->kd->side_sheet.mode = COGITO_SIDE_SHEET_MODAL;
  n->kd->side_sheet.preferred_mode = COGITO_SIDE_SHEET_MODAL;
  return n;
}

static YisVal cogito_side_sheet_new(YisVal titlev) {
  YisStr* ts = NULL;
  if (titlev.tag == EVT_STR) {
    ts = (YisStr*)titlev.as.p;
  } else if (titlev.tag != EVT_NULL) {
    ts = stdr_to_string(titlev);
  }
  CogitoNode* n = cogito_side_sheet_new_obj(ts);
  if (ts && titlev.tag != EVT_STR) yis_release_val(YV_STR(ts));
  return YV_OBJ(n);
}
