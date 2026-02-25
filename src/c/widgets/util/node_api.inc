// Node binding API
// Property setters and container operations called from the Ergo runtime.

static void cogito_container_add(ErgoVal parentv, ErgoVal childv) {
  if (parentv.tag != EVT_OBJ || childv.tag != EVT_OBJ) ergo_trap("cogito.container_add expects objects");
  CogitoNode* parent = (CogitoNode*)parentv.as.p;
  CogitoNode* child = (CogitoNode*)childv.as.p;
  cogito_children_add(parent, child);
  cogito_apply_style_tree(child);
  if (cogito_backend_ready) {
    CogitoNode* win = cogito_node_window(parent);
    if (win) cogito_window_relayout(win);
  }
}

static void cogito_container_set_margins(ErgoVal nodev, ErgoVal top, ErgoVal right, ErgoVal bottom, ErgoVal left) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_margins expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->margin_top = (int)ergo_as_int(top);
  node->margin_right = (int)ergo_as_int(right);
  node->margin_bottom = (int)ergo_as_int(bottom);
  node->margin_left = (int)ergo_as_int(left);
  node->margin_set = true;
}

static void cogito_container_set_align(ErgoVal nodev, ErgoVal alignv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_align expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->align = (int)ergo_as_int(alignv);
}

static void cogito_container_set_halign(ErgoVal nodev, ErgoVal alignv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_halign expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  int col = (int)ergo_as_int(alignv);
  if (col < 0) col = 0;
  if (col > 2) col = 2;
  int row = node->align / 3;
  node->align = row * 3 + col;
}

static void cogito_container_set_valign(ErgoVal nodev, ErgoVal alignv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_valign expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  int row = (int)ergo_as_int(alignv);
  if (row < 0) row = 0;
  if (row > 2) row = 2;
  int col = node->align % 3;
  node->align = row * 3 + col;
}

static void cogito_container_set_hexpand(ErgoVal nodev, ErgoVal expandv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_hexpand expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->hexpand = ergo_as_bool(expandv);
}

static void cogito_container_set_vexpand(ErgoVal nodev, ErgoVal expandv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_vexpand expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->vexpand = ergo_as_bool(expandv);
}

static void cogito_container_set_gap(ErgoVal nodev, ErgoVal gapv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_gap expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;

  node->gap = (int)ergo_as_int(gapv);
  cogito_window_relayout(cogito_node_window(node));
}

static void cogito_container_set_padding(ErgoVal nodev, ErgoVal top, ErgoVal right, ErgoVal bottom, ErgoVal left) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_padding expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->padding_top = (int)ergo_as_int(top);
  node->padding_right = (int)ergo_as_int(right);
  node->padding_bottom = (int)ergo_as_int(bottom);
  node->padding_left = (int)ergo_as_int(left);
  node->padding_set = true;
}

static void cogito_node_set_disabled(ErgoVal nodev, ErgoVal onv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_disabled expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->disabled = ergo_as_bool(onv);
  if (n == cogito_focused && n->disabled) {
    cogito_focused = NULL;
    cogito_focus_visible = false;
  }
  cogito_window_relayout(cogito_node_window(n));
}

static bool cogito_node_supports_editable(const CogitoNode* n) {
  if (!n) return false;
  return n->kind == COGITO_TEXTFIELD || n->kind == COGITO_TEXTVIEW || n->kind == COGITO_SEARCHFIELD;
}

static void cogito_node_set_editable(ErgoVal nodev, ErgoVal onv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_editable expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  bool on = ergo_as_bool(onv);
  n->editable = on;
  if (!on && cogito_node_supports_editable(n)) {
    if (n == cogito_focused) {
      cogito_focused = NULL;
      cogito_focus_visible = false;
    }
    n->text_input.selecting = false;
    n->text_input.sel_end = n->text_input.sel_start;
  }
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_node_get_editable(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_get_editable expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (!cogito_node_supports_editable(n)) return EV_BOOL(false);
  return EV_BOOL(n->editable);
}

static void cogito_node_set_id(ErgoVal nodev, ErgoVal idv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_id expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* s = NULL;
  if (idv.tag == EVT_STR) {
    s = (ErgoStr*)idv.as.p;
    if (s) ergo_retain_val(EV_STR(s));
  } else if (idv.tag != EVT_NULL) {
    s = stdr_to_string(idv);
  }
  if (n->view_id) ergo_release_val(EV_STR(n->view_id));
  n->view_id = s;
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_pointer_capture_set(ErgoVal nodev) {
  if (nodev.tag == EVT_NULL) {
    cogito_pointer_capture = NULL;
    return;
  }
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.pointer_capture expects node or null");
  cogito_pointer_capture = (CogitoNode*)nodev.as.p;
}

static void cogito_pointer_capture_clear(void) {
  cogito_pointer_capture = NULL;
}

static void cogito_node_set_class(ErgoVal nodev, ErgoVal classv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_class expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* cs = NULL;
  bool temp = false;
  if (classv.tag == EVT_STR) {
    cs = (ErgoStr*)classv.as.p;
  } else if (classv.tag != EVT_NULL) {
    cs = stdr_to_string(classv);
    temp = true;
  }
  const char* s = cs ? cs->data : "";
  bool is_button = (n->kind == COGITO_BUTTON || n->kind == COGITO_ICONBTN);
  
  // Clear old class name
  if (n->class_name) {
    ergo_release_val(EV_STR(n->class_name));
    n->class_name = NULL;
  }
  
  n->font_mono = false;
  n->font_tabular = false;
  if (is_button) {
    n->button.outlined = false;
    if (n->kind == COGITO_BUTTON) {
      n->button.text_style = false;
    }
  }
  if (n->kind == COGITO_LABEL) n->label.cls = COGITO_LABEL_CLASS_NONE;
  if (s && s[
    0
  ]) {
    int label_cls = COGITO_LABEL_CLASS_NONE;
    if (n->kind == COGITO_LABEL) {
      label_cls = cogito_label_class_from_name(s);
    }
    if (label_cls != COGITO_LABEL_CLASS_NONE) {
      n->label.cls = label_cls;
    } else if (strcmp(s,
    "monospace") == 0 || strcmp(s,
    "mono") == 0) {
      n->font_mono = true;
    } else if (strcmp(s,
    "tabular") == 0 || strcmp(s,
    "tnum") == 0 || strcmp(s,
    "tabular-nums") == 0) {
      n->font_tabular = true;
    } else if (is_button && strcmp(s,
    "outlined") == 0) {
      n->button.outlined = true;
    } else if (is_button && strcmp(s,
    "text") == 0) {
      n->button.text_style = true;
    } else {
      // Store custom class name
      if (cs) {
        if (temp) {
          // stdr_to_string created this string (refcount 1).
          // Transfer ownership directly to the node -- no retain needed,
          // and suppress the temp release at the end.
          n->class_name = cs;
          temp = false;
        } else {
          // String owned by caller -- retain a separate reference for the node.
          n->class_name = cs;
          ergo_retain_val(EV_STR(cs));
        }
      }
    }
  }
  if (temp && cs) ergo_release_val(EV_STR(cs));
  cogito_apply_style_to_node(n);
  extern CogitoNode* cogito_active_window;
  if (cogito_active_window) {
    cogito_window_relayout(cogito_active_window);
  }
}

static void cogito_node_set_a11y_label(ErgoVal nodev, ErgoVal labelv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_a11y_label expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->a11y_label) {
    ergo_release_val(EV_STR(n->a11y_label));
    n->a11y_label = NULL;
  }
  if (labelv.tag == EVT_STR) {
    ErgoStr* ls = (ErgoStr*)labelv.as.p;
    if (ls) {
      n->a11y_label = ls;
      ergo_retain_val(EV_STR(ls));
    }
  } else if (labelv.tag != EVT_NULL) {
    n->a11y_label = stdr_to_string(labelv);
  }
  if (n->kind == COGITO_WINDOW) {
    cogito_apply_window_a11y_label(n);
  }
}

static void cogito_node_set_a11y_role(ErgoVal nodev, ErgoVal rolev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_a11y_role expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->a11y_role) {
    ergo_release_val(EV_STR(n->a11y_role));
    n->a11y_role = NULL;
  }
  if (rolev.tag == EVT_STR) {
    ErgoStr* rs = (ErgoStr*)rolev.as.p;
    if (rs) {
      n->a11y_role = rs;
      ergo_retain_val(EV_STR(rs));
    }
  } else if (rolev.tag != EVT_NULL) {
    n->a11y_role = stdr_to_string(rolev);
  }
}

__attribute__((unused))
static void cogito_node_set_tooltip_val(ErgoVal nodev, ErgoVal textv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_tooltip expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->tooltip) {
    ergo_release_val(EV_STR(n->tooltip));
    n->tooltip = NULL;
  }
  if (textv.tag == EVT_STR) {
    ErgoStr* ts = (ErgoStr*)textv.as.p;
    if (ts) {
      n->tooltip = ts;
      ergo_retain_val(EV_STR(ts));
    }
  } else if (textv.tag != EVT_NULL) {
    n->tooltip = stdr_to_string(textv);
  }
}

static void cogito_build(ErgoVal nodev, ErgoVal builder) {
  if (builder.tag != EVT_FN) ergo_trap("cogito.build expects function");
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.build expects object");
  ErgoVal arg = nodev;
  ergo_retain_val(arg);
  ErgoVal ret = ergo_call(builder,
  1, &arg);
  ergo_release_val(arg);
  ergo_release_val(ret);
}

static ErgoVal cogito_node_window_val(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_window expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  CogitoNode* win = cogito_node_window(n);
  if (win) return EV_OBJ(win);
  return EV_NULLV;
}
