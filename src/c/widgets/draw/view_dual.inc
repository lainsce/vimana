case COGITO_VIEW_DUAL: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
  CogitoColor text_col = s.has_text ? s.text : n->text_color;
  
  float ratio = n->view_dual.animating ? n->view_dual.anim_ratio : n->view_dual.ratio;
  ratio = cogito_view_dual_clamp_ratio(ratio);
  bool anim_to_left_edge = n->view_dual.animating && n->view_dual.anim_to <= 0.0f;
  bool anim_to_right_edge = n->view_dual.animating && n->view_dual.anim_to >= 1.0f;
  bool edge_left = (ratio <= COGITO_VIEW_DUAL_EDGE_RATIO_ZONE) || anim_to_left_edge;
  bool edge_right = (ratio >= (1.0f - COGITO_VIEW_DUAL_EDGE_RATIO_ZONE)) || anim_to_right_edge;

  int content_x = 0, content_y = 0, content_w = 0, content_h = 0;
  cogito_view_dual_content_bounds(n, &content_x, &content_y, &content_w, &content_h);
  
  // Handle visuals use the same intent region as runtime hit-testing.
  bool handle_visible = n->view_dual.show_handle;
  float handle_ratio = edge_left ? 0.0f : (edge_right ? 1.0f : ratio);
  bool handle_hover = false;
  bool handle_active = handle_visible && n->view_dual.dragging;
  int pmx = 0, pmy = 0;
  bool pdown = false, ppressed = false;
  if (handle_visible && cogito_pointer_query(&pmx, &pmy, &pdown, &ppressed)) {
    handle_hover = cogito_view_dual_hit_handle_intent_for_ratio(n, handle_ratio, pmx, pmy);
    if (handle_hover && (pdown || ppressed)) {
      handle_active = true;
    }
  }

  int handle_base_x = 0;
  int handle_base_w = 0;
  int intent_x = 0, intent_y = 0, intent_w = 0, intent_h = 0;
  cogito_view_dual_handle_targets_for_ratio(n, handle_ratio,
                                            &handle_base_x, &handle_base_w,
                                            &intent_x, &intent_y,
                                            &intent_w, &intent_h);

  int handle_draw_w = handle_active ? 12 : 4;
  int handle_h = handle_active ? 52 : 48;
  if (handle_h > content_h) handle_h = content_h;
  if (handle_h < 1) handle_h = 1;
  int handle_center_x = handle_base_x + handle_base_w / 2;
  int handle_x = handle_center_x - handle_draw_w / 2;
  int handle_min_x = content_x + COGITO_VIEW_DUAL_HANDLE_EDGE_INSET;
  int handle_max_x = content_x + content_w - COGITO_VIEW_DUAL_HANDLE_EDGE_INSET - handle_draw_w;
  if (handle_x < handle_min_x) handle_x = handle_min_x;
  if (handle_x > handle_max_x) handle_x = handle_max_x;
  int handle_y = content_y + (content_h - handle_h) / 2;

  // Draw only visible child when collapsed to an edge.
  if (edge_left) {
    if (n->len > 1) cogito_draw_node(n->children[1]);
  } else if (edge_right) {
    if (n->len > 0) cogito_draw_node(n->children[0]);
  } else {
    for (size_t i = 0; i < n->len && i < 2; i++) {
      cogito_draw_node(n->children[i]);
    }
  }

  // Draw resize handle in on-surface color for dark/light contrast.
  if (handle_visible) {
    CogitoColor handle_color = text_col;
    float alpha_mul = handle_active ? 0.72f : (handle_hover ? 0.58f : 0.46f);
    handle_color.a = (uint8_t)(text_col.a * alpha_mul);
    if (handle_color.a < 72) handle_color.a = 72;
    cogito_draw_rect(handle_x, handle_y, handle_draw_w, handle_h, handle_color, handle_draw_w / 2);
  }
  break;
}
