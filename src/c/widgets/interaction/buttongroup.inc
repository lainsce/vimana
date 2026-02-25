static CogitoNode* cogito_buttongroup_new_obj(void) {
  CogitoNode* s = cogito_node_new(COGITO_BUTTON_GROUP);
  s->selected = 0;
  s->gap = 2;
  // Initialize buttongroup properties with defaults (M size, round, standard)
  s->buttongroup.size = 2;    // Medium (M) by default
  s->buttongroup.shape = 0;    // Round by default
  s->buttongroup.connected = 0; // Standard (not connected) by default
  s->buttongroup.pressed_child = -1; // No child pressed initially
  return s;
}

static ErgoVal cogito_buttongroup_new(void) {
  return EV_OBJ(cogito_buttongroup_new_obj());
}

static void cogito_buttongroup_on_select(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.buttongroup_on_select expects buttongroup");
  if (handler.tag != EVT_FN) ergo_trap("cogito.buttongroup_on_select expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_BUTTON_GROUP) ergo_trap("cogito.buttongroup_on_select expects buttongroup");
  if (n->on_select) ergo_release_val(EV_FN(n->on_select));
  n->on_select = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_select));
}

// Handle click from a child button - update selection
static void cogito_buttongroup_child_clicked(CogitoNode* buttongroup, CogitoNode* child) {
  if (!buttongroup || buttongroup->kind != COGITO_BUTTON_GROUP) return;
  if (!child) return;
  
  // Find the child's index
  int idx = -1;
  for (size_t i = 0; i < buttongroup->len; i++) {
    if (buttongroup->children[i] == child) {
      idx = (int)i;
      break;
    }
  }
  
  if (idx < 0) return;
  
  int prev = buttongroup->selected;
  buttongroup->selected = idx;
  
  if (buttongroup->selected != prev && buttongroup->on_select) {
    ErgoVal arg = EV_INT(idx);
    cogito_invoke(buttongroup->on_select, 1, &arg);
  }
}

// Get the selected index for a child button (used by button draw code)
static int cogito_buttongroup_child_index(CogitoNode* child) {
  if (!child || !child->parent || child->parent->kind != COGITO_BUTTON_GROUP) return -1;
  
  for (size_t i = 0; i < child->parent->len; i++) {
    if (child->parent->children[i] == child) {
      return (int)i;
    }
  }
  return -1;
}

// Check if a child button is selected in its ButtonGroup
static bool cogito_buttongroup_child_is_selected(CogitoNode* child) {
  if (!child || !child->parent || child->parent->kind != COGITO_BUTTON_GROUP) return false;
  
  int idx = cogito_buttongroup_child_index(child);
  if (idx < 0) return false;
  
  return (child->parent->selected == idx);
}

// Get corner radii for a button in a connected ButtonGroup
// Returns true if in a connected buttongroup, false otherwise
static bool cogito_buttongroup_child_radii(CogitoNode* child, 
                                            int* out_tl, int* out_tr, 
                                            int* out_br, int* out_bl,
                                            int default_radius) {
  if (!child || !child->parent || child->parent->kind != COGITO_BUTTON_GROUP) {
    // Not in a buttongroup - use default radius for all corners
    if (out_tl) *out_tl = default_radius;
    if (out_tr) *out_tr = default_radius;
    if (out_br) *out_br = default_radius;
    if (out_bl) *out_bl = default_radius;
    return false;
  }
  
  CogitoNode* bg = child->parent;
  bool is_connected = (bg->buttongroup.connected == 1);
  bool is_rounded = (bg->buttongroup.shape == 0);
  
  int idx = cogito_buttongroup_child_index(child);
  int count = (int)bg->len;

  int outer_radius = default_radius;
  int inner_radius = 0;
  
  if (is_connected) {
    inner_radius = 4; // Slight edge between buttons
  } else {
    // Unconnected (standard) buttons: each button has its own rounded corners
    // For rounded shape: full circles
    // For square shape: use full outer radius (not 4px) - square shape uses full radii
    if (is_rounded) {
      inner_radius = default_radius; // Full circles
    } else {
      inner_radius = default_radius; // Square shape uses full radii when not connected
    }
  }
  
  if (count == 1) {
    // Single button - all corners rounded
    if (out_tl) *out_tl = outer_radius;
    if (out_tr) *out_tr = outer_radius;
    if (out_br) *out_br = outer_radius;
    if (out_bl) *out_bl = outer_radius;
  } else if (idx == 0) {
    // First button - rounded left, inner right
    if (out_tl) *out_tl = outer_radius;
    if (out_tr) *out_tr = inner_radius;
    if (out_br) *out_br = inner_radius;
    if (out_bl) *out_bl = outer_radius;
  } else if (idx == count - 1) {
    // Last button - inner left, rounded right
    if (out_tl) *out_tl = inner_radius;
    if (out_tr) *out_tr = outer_radius;
    if (out_br) *out_br = outer_radius;
    if (out_bl) *out_bl = inner_radius;
  } else {
    // Middle button - all inner
    if (out_tl) *out_tl = inner_radius;
    if (out_tr) *out_tr = inner_radius;
    if (out_br) *out_br = inner_radius;
    if (out_bl) *out_bl = inner_radius;
  }
  
  return true;
}

// ButtonGroup API functions for Ergo bindings
static void cogito_buttongroup_set_size(ErgoVal nodev, ErgoVal sizev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.buttongroup_set_size expects buttongroup");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_BUTTON_GROUP) ergo_trap("cogito.buttongroup_set_size expects buttongroup");
  int size = (int)ergo_as_int(sizev);
  if (size < 0) size = 0;
  if (size > 4) size = 4;
  n->buttongroup.size = size;
}

static ErgoVal cogito_buttongroup_get_size(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.buttongroup_get_size expects buttongroup");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_BUTTON_GROUP) ergo_trap("cogito.buttongroup_get_size expects buttongroup");
  return EV_INT(n->buttongroup.size);
}

static void cogito_buttongroup_set_shape(ErgoVal nodev, ErgoVal shapev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.buttongroup_set_shape expects buttongroup");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_BUTTON_GROUP) ergo_trap("cogito.buttongroup_set_shape expects buttongroup");
  int shape = (int)ergo_as_int(shapev);
  if (shape < 0) shape = 0;
  if (shape > 1) shape = 1;
  n->buttongroup.shape = shape;
}

static ErgoVal cogito_buttongroup_get_shape(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.buttongroup_get_shape expects buttongroup");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_BUTTON_GROUP) ergo_trap("cogito.buttongroup_get_shape expects buttongroup");
  return EV_INT(n->buttongroup.shape);
}

static void cogito_buttongroup_set_connected(ErgoVal nodev, ErgoVal connectedv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.buttongroup_set_connected expects buttongroup");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_BUTTON_GROUP) ergo_trap("cogito.buttongroup_set_connected expects buttongroup");
  n->buttongroup.connected = ergo_as_bool(connectedv) ? 1 : 0;
}

static ErgoVal cogito_buttongroup_get_connected(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.buttongroup_get_connected expects buttongroup");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_BUTTON_GROUP) ergo_trap("cogito.buttongroup_get_connected expects buttongroup");
  return EV_BOOL(n->buttongroup.connected == 1);
}