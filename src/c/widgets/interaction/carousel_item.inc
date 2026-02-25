// Helper: find the built-in carousel-item-label among children (may not be at index 0).
static CogitoNode* cogito_carousel_item_find_label(CogitoNode* item) {
  for (size_t i = 0; i < item->len; i++) {
    CogitoNode* c = item->children[i];
    if (c->kind == COGITO_LABEL && c->class_name &&
        c->class_name->data && strstr(c->class_name->data, "carousel-item-label")) {
      return c;
    }
  }
  return NULL;
}

static CogitoNode* cogito_carousel_item_new_obj(void) {
  CogitoNode* item = cogito_node_new(COGITO_CAROUSEL_ITEM);
  // Default border radius for carousel items (will be overridden by theme)
  item->border_radius = 16;
  
  // Add a built-in label as a child (drawn last = on top of other children)
  CogitoNode* label = cogito_node_new(COGITO_LABEL);
  label->text_align = 0;  // left alignment
  {
    ErgoStr* cls = (ErgoStr*)calloc(1, sizeof(ErgoStr));
    cls->len = 19;
    cls->data = (char*)malloc(20);
    memcpy(cls->data, "carousel-item-label", 19);
    cls->data[19] = '\0';
    cls->ref = 1;
    label->class_name = cls;
  }
  label->hexpand = true;
  // Apply theme styles so font_weight, text_color, etc. are baked into node fields
  cogito_apply_style_to_node(label);
  // Ensure font-size 21 for carousel-item-label (theme custom class)
  label->font_size = 21;
  label->font_size_set = true;
  label->font_weight = 700;
  label->font_weight_set = true;
  
  cogito_children_add(item, label);
  
  return item;
}

static ErgoVal cogito_carousel_item_new(void) {
  return EV_OBJ(cogito_carousel_item_new_obj());
}

// Set text on the built-in label
static void cogito_carousel_item_set_text(ErgoVal itemv, ErgoVal textv) {
  if (itemv.tag != EVT_OBJ) ergo_trap("cogito.carousel_item_set_text expects CarouselItem");
  CogitoNode* n = (CogitoNode*)itemv.as.p;
  CogitoNode* label = cogito_carousel_item_find_label(n);
  if (label) {
    ErgoStr* ts = stdr_to_string(textv);
    cogito_node_set_text(label, ts);
    cogito_window_relayout(cogito_node_window(n));
  }
}

// Set horizontal alignment on the carousel item's content.
// alignv: 0=left, 1=center, 2=right (mapped to align columns).
static void cogito_carousel_item_set_halign(ErgoVal itemv, ErgoVal alignv) {
  if (itemv.tag != EVT_OBJ) ergo_trap("cogito.carousel_item_set_halign expects CarouselItem");
  CogitoNode* n = (CogitoNode*)itemv.as.p;
  int h = (int)ergo_as_int(alignv);
  if (h < 0) h = 0;
  if (h > 2) h = 2;
  // align encodes row*3 + col; preserve current row, update column
  int cur_row = n->align / 3;
  n->align = cur_row * 3 + h;
  // Also set text_align on the built-in label for text rendering
  CogitoNode* label = cogito_carousel_item_find_label(n);
  if (label) {
    label->text_align = h;
  }
  cogito_window_relayout(cogito_node_window(n));
}

// Set vertical alignment on the carousel item's content.
// This sets the item's align property which controls child positioning
// in the zstack-like layout (0=top-left..8=bottom-right).
// alignv: 0=top, 1=center, 2=bottom (mapped to align rows 0,1,2).
static void cogito_carousel_item_set_valign(ErgoVal itemv, ErgoVal alignv) {
  if (itemv.tag != EVT_OBJ) ergo_trap("cogito.carousel_item_set_valign expects CarouselItem");
  CogitoNode* n = (CogitoNode*)itemv.as.p;
  int v = (int)ergo_as_int(alignv);
  if (v < 0) v = 0;
  if (v > 2) v = 2;
  // align encodes row*3 + col; preserve current column, update row
  int cur_col = n->align % 3;
  n->align = v * 3 + cur_col;
  cogito_window_relayout(cogito_node_window(n));
}
