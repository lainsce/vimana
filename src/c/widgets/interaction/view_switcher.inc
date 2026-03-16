static CogitoNode* cogito_view_switcher_new_obj(void) {
  return cogito_node_new(COGITO_VIEWSWITCHER);
}
static YisVal cogito_view_switcher_new(void) {
  return YV_OBJ(cogito_view_switcher_new_obj());
}

// Find a view by ID in the children array
static CogitoNode* cogito_view_switcher_find_child(CogitoNode* n, YisStr* id) {
  if (!n || !id) return NULL;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if (c->view_id && cogito_str_eq(c->view_id, id)) return c;
  }
  return NULL;
}

// Find a builder by ID in the lazy builders array
static YisFn* cogito_view_switcher_find_builder(CogitoNode* n, YisStr* id, size_t* idx) {
  if (!n || !id) return NULL;
  for (size_t i = 0; i < n->view_builder_len; i++) {
    if (n->view_builder_ids[i] && cogito_str_eq(n->view_builder_ids[i], id)) {
      if (idx) *idx = i;
      return n->view_builders[i];
    }
  }
  return NULL;
}

// Add a lazy builder for a view ID
void cogito_view_switcher_add_lazy(CogitoNode* n, YisVal idv, YisVal builderv) {
  if (n->kind != COGITO_VIEWSWITCHER) yis_trap("cogito.view_switcher_add_lazy expects view_switcher");
  if (builderv.tag != EVT_FN) yis_trap("cogito.view_switcher_add_lazy expects function");
  
  YisStr* id = stdr_to_string(idv);
  
  // Check if we already have a builder for this ID
  if (cogito_view_switcher_find_builder(n, id, NULL)) {
    yis_release_val(YV_STR(id));
    return; // Already registered
  }
  
  // Grow arrays if needed
  if (n->view_builder_len >= n->view_builder_cap) {
    size_t new_cap = n->view_builder_cap == 0 ? 4 : n->view_builder_cap * 2;
    YisStr** new_ids = (YisStr**)realloc(n->view_builder_ids, new_cap * sizeof(YisStr*));
    YisFn** new_builders = (YisFn**)realloc(n->view_builders, new_cap * sizeof(YisFn*));
    if (!new_ids || !new_builders) {
      yis_release_val(YV_STR(id));
      yis_trap("cogito.view_switcher_add: out of memory");
    }
    n->view_builder_ids = new_ids;
    n->view_builders = new_builders;
    n->view_builder_cap = new_cap;
  }
  
  // Store the builder
  n->view_builder_ids[n->view_builder_len] = id;
  n->view_builders[n->view_builder_len] = (YisFn*)builderv.as.p;
  yis_retain_val(YV_STR(id));
  yis_retain_val(YV_FN((YisFn*)builderv.as.p));
  n->view_builder_len++;
  fflush(stderr);
}

// Build a view on-demand using its registered builder
static CogitoNode* cogito_view_switcher_build_view(CogitoNode* vs, YisStr* id) {
  size_t idx = 0;
  YisFn* builder = cogito_view_switcher_find_builder(vs, id, &idx);
  
  if (!builder) return NULL;
  
  // Call the builder function with the view_switcher as argument
  YisVal vsv = YV_OBJ(vs);
  yis_retain_val(vsv);
  YisVal ret = builder->fn(builder->env, 1, &vsv);
  yis_release_val(vsv);
  
  // The builder should return the view node (wrapped in a CogitoHandle)
  // CogitoHandle layout: YisObj base; void* ptr; int kind; ...
  // YisObj layout: int ref; void (*drop)(YisObj*);
  // So ptr is at offset: sizeof(int) + sizeof(void*) = typically 8 bytes on 64-bit
  CogitoNode* view = NULL;
  if (ret.tag == EVT_OBJ) {
    // Directly extract the ptr from CogitoHandle-like structure
    // The handle starts with YisObj (ref + drop), then ptr
    char* handle = (char*)ret.as.p;
    view = *(CogitoNode**)(handle + sizeof(YisObj));
    // Set the view_id on the newly created view
    if (view && !view->view_id) {
      view->view_id = id;
      yis_retain_val(YV_STR(id));
    }
    // Add the view as a child
    cogito_children_add(vs, view);
    // Apply styles to the new view tree
    cogito_apply_style_tree(view);
  }
  
  yis_release_val(ret);
  return view;
}

static void cogito_view_switcher_set_active(YisVal vsv, YisVal idv) {
  if (vsv.tag != EVT_OBJ) yis_trap("cogito.view_switcher_set_active expects view_switcher");
  CogitoNode* n = (CogitoNode*)vsv.as.p;
  if (n->kind != COGITO_VIEWSWITCHER) yis_trap("cogito.view_switcher_set_active expects view_switcher");
  YisStr* s = stdr_to_string(idv);
  
  // Check if view already exists, if not try to build it lazily
  if (!cogito_view_switcher_find_child(n, s)) {
    cogito_view_switcher_build_view(n, s);
  }
  
  if (n->view_active_id) yis_release_val(YV_STR(n->view_active_id));
  n->view_active_id = s;
  if (n->view_active_id) yis_retain_val(YV_STR(n->view_active_id));
  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_view_switcher_get_active(YisVal vsv) {
  if (vsv.tag != EVT_OBJ) yis_trap("cogito.view_switcher_get_active expects view_switcher");
  CogitoNode* n = (CogitoNode*)vsv.as.p;
  if (n->kind != COGITO_VIEWSWITCHER) yis_trap("cogito.view_switcher_get_active expects view_switcher");
  YisStr* s = n->view_active_id ? n->view_active_id : stdr_str_lit("");
  yis_retain_val(YV_STR(s));
  return YV_STR(s);
}
