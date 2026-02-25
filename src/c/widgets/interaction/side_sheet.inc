static void cogito_side_sheet_close_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!n || n->kind != COGITO_SIDE_SHEET) return;
  // Close button is always 24px from top and right edges of the SHEET
  int cw = 24;
  int ch = 24;
  int cx = n->x + n->w - 24 - cw;
  int cy = n->y + 24;
  if (out_x) *out_x = cx;
  if (out_y) *out_y = cy;
  if (out_w) *out_w = cw;
  if (out_h) *out_h = ch;
}

static int cogito_side_sheet_min_width(CogitoNode* n) {
  if (!n) return 200;
  int pad_l = n->padding_left;
  int pad_r = n->padding_right;
  int gap = n->gap > 0 ? n->gap : 8;
  int title_w = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    title_w = cogito_text_width_size_node(n, n->text->data, ts) + gap;
  }
  int content_w = 0;
  for (size_t i = 0; i < n->len; i++) {
    int cw = 0, ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    content_w += cw + n->children[i]->margin_left + n->children[i]->margin_right;
    if (i > 0) content_w += gap;
  }
  int w = title_w + pad_l + content_w + pad_r;
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
  if (max_w < min_w) max_w = min_w;
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
  if (max_w < min_w) max_w = min_w;
  int target_w = n->side_sheet.desired_w > 0
    ? n->side_sheet.desired_w
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
  if (max_w < min_w) max_w = min_w;

  int d_min = abs(from_w - min_w);
  int d_settle = abs(from_w - settle_w);
  int d_max = abs(from_w - max_w);
  if (d_max <= d_settle && d_max <= d_min) return max_w;
  if (d_min <= d_settle) return min_w;
  return settle_w;
}

static void cogito_side_sheet_handle_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!n) return;
  int w = COGITO_SIDE_SHEET_HANDLE_TOUCH_W;
  int h = 96;
  int x = n->x;
  int y = n->y + (n->h - h) / 2;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
}

void cogito_side_sheet_set_mode(CogitoNode* n, int mode) {
    if (!n || n->kind != COGITO_SIDE_SHEET) return;
    n->side_sheet.mode = mode;
    // Set class name for theme styling based on mode
    const char* class_name = NULL;
    switch (mode) {
        case 0: class_name = "modal"; break;
        case 1: class_name = "inset"; break;
        case 2: class_name = "sidebar"; break;
        default: class_name = "modal"; break;
    }
    if (class_name) {
        ErgoStr* cls = stdr_str_from_slice(class_name, strlen(class_name));
        if (n->class_name) ergo_release_val(EV_STR(n->class_name));
        n->class_name = cls;
        ergo_retain_val(EV_STR(cls));
    }
}

static CogitoNode* cogito_side_sheet_new_obj(ErgoStr* title) {
  CogitoNode* n = cogito_node_new(COGITO_SIDE_SHEET);
  if (title && title->data && title->data[0]) {
    cogito_node_set_text(n, title);
  }
  n->vexpand = true;
  n->gap = 8;
  n->side_sheet.dragging = false;
  n->side_sheet.expanded = false;
  n->side_sheet.drag_start_x = 0;
  n->side_sheet.drag_start_w = 0;
  n->side_sheet.desired_w = 0;
  n->side_sheet.mode = COGITO_SIDE_SHEET_MODAL;
  return n;
}

static ErgoVal cogito_side_sheet_new(ErgoVal titlev) {
  ErgoStr* ts = NULL;
  if (titlev.tag == EVT_STR) {
    ts = (ErgoStr*)titlev.as.p;
  } else if (titlev.tag != EVT_NULL) {
    ts = stdr_to_string(titlev);
  }
  CogitoNode* n = cogito_side_sheet_new_obj(ts);
  if (ts && titlev.tag != EVT_STR) ergo_release_val(EV_STR(ts));
  return EV_OBJ(n);
}
