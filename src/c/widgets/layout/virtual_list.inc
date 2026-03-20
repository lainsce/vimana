// VirtualList — virtualized vertical list with fixed item height.
// Only visible items + a small buffer are materialized as child nodes.
// Items are created on-demand via a builder callback.

// Forward declaration for function defined in 10_interaction.inc
static void cogito_clear_children(CogitoNode* node);

static CogitoNode *cogito_virtual_list_new_obj(void) {
  CogitoNode *n = cogito_node_new(COGITO_VIRTUAL_LIST);
  n->kd->virtual_list.item_count = 0;
  n->kd->virtual_list.item_height = 48;
  n->kd->virtual_list.scroll_y = 0;
  n->kd->virtual_list.viewport_h = 0;
  n->kd->virtual_list.first_visible = 0;
  n->kd->virtual_list.visible_count = 0;
  n->kd->virtual_list.prev_first = -1;
  n->kd->virtual_list.prev_count = -1;
  n->kd->virtual_list.builder = NULL;
  return n;
}

static YisVal cogito_virtual_list_new(void) {
  return YV_OBJ(cogito_virtual_list_new_obj());
}

static void cogito_virtual_list_set_item_count(CogitoNode *n, int count) {
  if (!n || n->kind != COGITO_VIRTUAL_LIST) return;
  n->kd->virtual_list.item_count = count < 0 ? 0 : count;
}

static void cogito_virtual_list_set_item_height(CogitoNode *n, int h) {
  if (!n || n->kind != COGITO_VIRTUAL_LIST) return;
  n->kd->virtual_list.item_height = h > 0 ? h : 1;
}

static void cogito_virtual_list_set_builder(CogitoNode *n, YisFn *builder) {
  if (!n || n->kind != COGITO_VIRTUAL_LIST) return;
  if (n->kd->virtual_list.builder)
    yis_release_val(YV_FN(n->kd->virtual_list.builder));
  n->kd->virtual_list.builder = builder;
  if (builder)
    yis_retain_val(YV_FN(builder));
}

// Scroll callback — set from Yis or C
static void cogito_virtual_list_scroll(CogitoNode *n, int delta_y) {
  if (!n || n->kind != COGITO_VIRTUAL_LIST) return;
  CogitoVirtualListData *vl = &n->kd->virtual_list;
  int max_scroll = vl->item_count * vl->item_height - vl->viewport_h;
  if (max_scroll < 0) max_scroll = 0;
  vl->scroll_y += delta_y;
  if (vl->scroll_y < 0) vl->scroll_y = 0;
  if (vl->scroll_y > max_scroll) vl->scroll_y = max_scroll;
}

// Layout: compute visible range and rebuild children
static void cogito_layout_virtual_list(CogitoNode *n) {
  CogitoVirtualListData *vl = &n->kd->virtual_list;

  int pad_t = n->padding_top;
  int pad_b = n->padding_bottom;
  int pad_l = cogito_padding_start(n);
  int pad_r = cogito_padding_end(n);
  int inner_w = n->w - pad_l - pad_r;
  int inner_h = n->h - pad_t - pad_b;
  vl->viewport_h = inner_h;

  if (vl->item_count <= 0 || vl->item_height <= 0 || !vl->builder) {
    cogito_clear_children(n);
    return;
  }

  // Clamp scroll
  int total_h = vl->item_count * vl->item_height;
  int max_scroll = total_h - inner_h;
  if (max_scroll < 0) max_scroll = 0;
  if (vl->scroll_y > max_scroll) vl->scroll_y = max_scroll;
  if (vl->scroll_y < 0) vl->scroll_y = 0;

  // Compute visible range with buffer
  int buffer = 2; // extra items above/below viewport
  int first = vl->scroll_y / vl->item_height - buffer;
  if (first < 0) first = 0;
  int last = (vl->scroll_y + inner_h) / vl->item_height + buffer;
  if (last >= vl->item_count) last = vl->item_count - 1;
  int count = last - first + 1;

  vl->first_visible = first;
  vl->visible_count = count;

  // Rebuild children only if range changed
  bool needs_rebuild = (first != vl->prev_first || count != vl->prev_count
                        || (int)n->len != count);

  if (needs_rebuild) {
    cogito_clear_children(n);
    for (int i = first; i <= last; i++) {
      YisVal idx = YV_INT(i);
      YisVal result = vl->builder->fn(vl->builder->env, 1, &idx);
      if (result.tag == EVT_OBJ && result.as.p) {
        CogitoNode *child = (CogitoNode *)result.as.p;
        cogito_children_add(n, child);
        yis_release_val(result); // children_add retained
      }
    }
    vl->prev_first = first;
    vl->prev_count = count;
  }

  // Position children
  int cy = n->y + pad_t - vl->scroll_y + first * vl->item_height;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode *child = n->children[i];
    child->x = n->x + pad_l;
    child->y = cy;
    child->w = inner_w;
    child->h = vl->item_height;
    cogito_layout_node(child, child->x, child->y, child->w, child->h);
    cy += vl->item_height;
  }
}

// Measure: total content height
static int cogito_measure_virtual_list_h(CogitoNode *n) {
  if (!n || n->kind != COGITO_VIRTUAL_LIST) return 0;
  return n->kd->virtual_list.item_count * n->kd->virtual_list.item_height;
}
