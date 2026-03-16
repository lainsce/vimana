// Node binding API
// Property setters and container operations called from the Yis runtime.

static bool cogito_class_has_token_local(const char* class_name, const char* token) {
  if (!class_name || !token || !token[0]) return false;
  size_t token_len = strlen(token);
  const char* p = class_name;
  while (*p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (!*p) break;
    const char* start = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
    size_t len = (size_t)(p - start);
    if (len == token_len && strncmp(start, token, token_len) == 0) return true;
  }
  return false;
}

static void cogito_node_force_class_token(CogitoNode* n, const char* token) {
  if (!n || !token || !token[0]) return;
  const char* existing = (n->class_name && n->class_name->data) ? n->class_name->data : NULL;
  if (existing && cogito_class_has_token_local(existing, token)) return;

  if (!existing || !existing[0]) {
    if (n->class_name) {
      yis_release_val(YV_STR(n->class_name));
      n->class_name = NULL;
    }
    n->class_name = stdr_str_lit(token);
    return;
  }

  size_t need = strlen(existing) + 1 + strlen(token) + 1;
  char* merged = (char*)calloc(need, 1);
  if (!merged) return;
  snprintf(merged, need, "%s %s", existing, token);
  YisStr* cls = stdr_str_lit(merged);
  free(merged);
  if (n->class_name) yis_release_val(YV_STR(n->class_name));
  n->class_name = cls;
}

static void cogito_container_add(YisVal parentv, YisVal childv) {
  if (parentv.tag != EVT_OBJ || childv.tag != EVT_OBJ) yis_trap("cogito.container_add expects objects");
  CogitoNode* parent = (CogitoNode*)parentv.as.p;
  CogitoNode* child = (CogitoNode*)childv.as.p;

  if ((parent->kind == COGITO_BOTTOM_SHEET || parent->kind == COGITO_SIDE_SHEET) &&
      child->kind == COGITO_TEXTFIELD) {
    cogito_node_force_class_token(child, "outline");
  }

  cogito_children_add(parent, child);
  cogito_apply_style_tree(child);
  if (cogito_backend_ready) {
    CogitoNode* win = cogito_node_window(parent);
    if (win) cogito_window_relayout(win);
  }
}

static void cogito_container_set_margins(YisVal nodev, YisVal top, YisVal right, YisVal bottom, YisVal left) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.container_set_margins expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->margin_top = (int)yis_as_int(top);
  node->margin_right = (int)yis_as_int(right);
  node->margin_bottom = (int)yis_as_int(bottom);
  node->margin_left = (int)yis_as_int(left);
  node->margin_set = true;
}

static void cogito_container_set_align(YisVal nodev, YisVal alignv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.container_set_align expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->align = (int)yis_as_int(alignv);
}

static void cogito_container_set_halign(YisVal nodev, YisVal alignv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.container_set_halign expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  int col = (int)yis_as_int(alignv);
  if (col < 0) col = 0;
  if (col > 2) col = 2;
  int row = node->align / 3;
  node->align = row * 3 + col;
}

static void cogito_container_set_valign(YisVal nodev, YisVal alignv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.container_set_valign expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  int row = (int)yis_as_int(alignv);
  if (row < 0) row = 0;
  if (row > 2) row = 2;
  int col = node->align % 3;
  node->align = row * 3 + col;
}

static void cogito_container_set_hexpand(YisVal nodev, YisVal expandv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.container_set_hexpand expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->hexpand = yis_as_bool(expandv);
  node->hexpand_set = true;
}

static void cogito_container_set_hexpand_set(YisVal nodev, YisVal setv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.container_set_hexpand_set expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->hexpand_set = yis_as_bool(setv);
}

static void cogito_container_set_vexpand(YisVal nodev, YisVal expandv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.container_set_vexpand expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->vexpand = yis_as_bool(expandv);
  node->vexpand_set = true;
}

static void cogito_container_set_vexpand_set(YisVal nodev, YisVal setv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.container_set_vexpand_set expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->vexpand_set = yis_as_bool(setv);
}

static void cogito_scroller_set_min_content_width(YisVal scv, YisVal wv) {
  if (scv.tag != EVT_OBJ) yis_trap("cogito.scroller_set_min_content_width expects scroller");
  CogitoNode* n = (CogitoNode*)scv.as.p;
  if (!n || n->kind != COGITO_SCROLLER) yis_trap("cogito.scroller_set_min_content_width expects scroller");
  int w = (int)yis_as_int(wv);
  if (w < 0) w = 0;
  n->min_w = w;
  n->min_w_set = true;
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_scroller_set_min_content_height(YisVal scv, YisVal hv) {
  if (scv.tag != EVT_OBJ) yis_trap("cogito.scroller_set_min_content_height expects scroller");
  CogitoNode* n = (CogitoNode*)scv.as.p;
  if (!n || n->kind != COGITO_SCROLLER) yis_trap("cogito.scroller_set_min_content_height expects scroller");
  int h = (int)yis_as_int(hv);
  if (h < 0) h = 0;
  n->min_h = h;
  n->min_h_set = true;
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_scroller_set_scroll_y(YisVal scv, YisVal yv) {
  if (scv.tag != EVT_OBJ) yis_trap("cogito.scroller_set_scroll_y expects scroller");
  CogitoNode* n = (CogitoNode*)scv.as.p;
  if (!n || n->kind != COGITO_SCROLLER) yis_trap("cogito.scroller_set_scroll_y expects scroller");
  int y = (int)yis_as_int(yv);
  if (y < 0) y = 0;
  if (n->scroller.scroll_y == y) return;
  n->scroller.scroll_y = y;
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_container_set_gap(YisVal nodev, YisVal gapv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.container_set_gap expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;

  node->gap = (int)yis_as_int(gapv);
  cogito_window_relayout(cogito_node_window(node));
}

static void cogito_container_set_homogeneous(YisVal nodev, YisVal hv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.container_set_homogeneous expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->homogeneous = yis_as_bool(hv);
  cogito_window_relayout(cogito_node_window(node));
}

static void cogito_container_set_col_homogeneous(YisVal nodev, YisVal hv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.container_set_col_homogeneous expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->col_homogeneous = yis_as_bool(hv);
  cogito_window_relayout(cogito_node_window(node));
}

static void cogito_container_set_row_homogeneous(YisVal nodev, YisVal hv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.container_set_row_homogeneous expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->row_homogeneous = yis_as_bool(hv);
  cogito_window_relayout(cogito_node_window(node));
}

static void cogito_container_set_padding(YisVal nodev, YisVal top, YisVal right, YisVal bottom, YisVal left) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.container_set_padding expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->padding_top = (int)yis_as_int(top);
  node->padding_right = (int)yis_as_int(right);
  node->padding_bottom = (int)yis_as_int(bottom);
  node->padding_left = (int)yis_as_int(left);
  node->padding_set = true;
}

static void cogito_node_set_disabled(YisVal nodev, YisVal onv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_set_disabled expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->disabled = yis_as_bool(onv);
  if (n == cogito_focused && n->disabled) {
    cogito_focused = NULL;
    cogito_focus_visible = false;
  }
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_node_set_opacity(YisVal nodev, YisVal opacityv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_set_opacity expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  float v = opacityv.tag == EVT_INT ? (float)opacityv.as.i : (opacityv.tag == EVT_FLOAT ? (float)opacityv.as.f : 1.0f);
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  n->opacity = v;
  n->opacity_set = true;
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_node_set_visible(YisVal nodev, YisVal onv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_set_visible expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  bool visible = yis_as_bool(onv);
  n->hidden = !visible;

  if (!visible && cogito_focused) {
    CogitoNode* cur = cogito_focused;
    while (cur) {
      if (cur == n) {
        cogito_focused = NULL;
        cogito_focus_visible = false;
        break;
      }
      cur = cur->parent;
    }
  }

  cogito_window_relayout(cogito_node_window(n));
}

static YisVal cogito_node_get_visible(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_get_visible expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return YV_BOOL(!n->hidden);
}

static bool cogito_node_supports_editable(const CogitoNode* n) {
  if (!n) return false;
  return n->kind == COGITO_TEXTFIELD || n->kind == COGITO_TEXTVIEW || n->kind == COGITO_SEARCHFIELD;
}

static void cogito_node_set_editable(YisVal nodev, YisVal onv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_set_editable expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  bool on = yis_as_bool(onv);
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

static YisVal cogito_node_get_editable(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_get_editable expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (!cogito_node_supports_editable(n)) return YV_BOOL(false);
  return YV_BOOL(n->editable);
}

static void cogito_node_set_id(YisVal nodev, YisVal idv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_set_id expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* s = NULL;
  if (idv.tag == EVT_STR) {
    s = (YisStr*)idv.as.p;
    if (s) yis_retain_val(YV_STR(s));
  } else if (idv.tag != EVT_NULL) {
    s = stdr_to_string(idv);
  }
  if (n->view_id) yis_release_val(YV_STR(n->view_id));
  n->view_id = s;
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_pointer_capture_set(YisVal nodev) {
  if (nodev.tag == EVT_NULL) {
    cogito_pointer_capture = NULL;
    return;
  }
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.pointer_capture expects node or null");
  cogito_pointer_capture = (CogitoNode*)nodev.as.p;
}

static void cogito_pointer_capture_clear(void) {
  cogito_pointer_capture = NULL;
}

static void cogito_node_set_class(YisVal nodev, YisVal classv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_set_class expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  YisStr* cs = NULL;
  bool temp = false;
  if (classv.tag == EVT_STR) {
    cs = (YisStr*)classv.as.p;
  } else if (classv.tag != EVT_NULL) {
    cs = stdr_to_string(classv);
    temp = true;
  }
  const char* s = cs ? cs->data : "";
  // Skip if class name is unchanged
  const char* old = n->class_name ? n->class_name->data : "";
  if (strcmp(s, old) == 0) {
    if (temp && cs) yis_release_val(YV_STR(cs));
    return;
  }
  bool is_button = (n->kind == COGITO_BUTTON || n->kind == COGITO_ICONBTN);
  
  // Clear old class name
  if (n->class_name) {
    yis_release_val(YV_STR(n->class_name));
    n->class_name = NULL;
  }
  
  n->font_mono = false;
  n->font_tabular = false;
  n->font_emphasized = false;
  if (is_button) {
    n->button.outlined = false;
    if (n->kind == COGITO_BUTTON) {
      n->button.text_style = false;
    }
  }
  if (n->kind == COGITO_LABEL) n->label.cls = COGITO_LABEL_CLASS_NONE;
  if (s && s[0]) {
    bool has_ws = false;
    for (const char* q = s; *q; q++) {
      if (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') {
        has_ws = true;
        break;
      }
    }

    if (has_ws) {
      bool has_custom_token = false;
      const char* p = s;
      while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;

        const char* start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
        size_t tlen = (size_t)(p - start);
        if (tlen == 0) continue;

        char tok[64];
        size_t copy_len = tlen >= sizeof(tok) ? (sizeof(tok) - 1) : tlen;
        memcpy(tok, start, copy_len);
        tok[copy_len] = 0;

        bool recognized = false;

        if (n->kind == COGITO_LABEL) {
          int label_cls = cogito_label_class_from_name(tok);
          if (label_cls != COGITO_LABEL_CLASS_NONE) {
            n->label.cls = label_cls;
            recognized = true;
          }
        }

        if (!recognized && (strcmp(tok, "monospace") == 0 || strcmp(tok, "mono") == 0)) {
          n->font_mono = true;
          recognized = true;
        }
        if (!recognized && (strcmp(tok, "tabular") == 0 || strcmp(tok, "tnum") == 0 || strcmp(tok, "tabular-nums") == 0)) {
          n->font_tabular = true;
          recognized = true;
        }
        if (!recognized && strcmp(tok, "emphasized") == 0) {
          n->font_emphasized = true;
          recognized = true;
        }
        if (!recognized && is_button && strcmp(tok, "outlined") == 0) {
          n->button.outlined = true;
          recognized = true;
        }
        if (!recognized && is_button && strcmp(tok, "text") == 0) {
          n->button.text_style = true;
          recognized = true;
        }

        if (!recognized) has_custom_token = true;
      }

      if (cs && (has_custom_token || n->kind == COGITO_ICONBTN)) {
        if (temp) {
          n->class_name = cs;
          temp = false;
        } else {
          n->class_name = cs;
          yis_retain_val(YV_STR(cs));
        }
      }
    } else {
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
      } else if (strcmp(s, "emphasized") == 0) {
        n->font_emphasized = true;
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
            yis_retain_val(YV_STR(cs));
          }
        }
      }
    }
  }
  if (temp && cs) yis_release_val(YV_STR(cs));
  cogito_apply_style_to_node(n);
  extern CogitoNode* cogito_active_window;
  if (cogito_active_window) {
    cogito_window_relayout(cogito_active_window);
  }
}

static void cogito_node_set_a11y_label(YisVal nodev, YisVal labelv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_set_a11y_label expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->a11y_label) {
    yis_release_val(YV_STR(n->a11y_label));
    n->a11y_label = NULL;
  }
  if (labelv.tag == EVT_STR) {
    YisStr* ls = (YisStr*)labelv.as.p;
    if (ls) {
      n->a11y_label = ls;
      yis_retain_val(YV_STR(ls));
    }
  } else if (labelv.tag != EVT_NULL) {
    n->a11y_label = stdr_to_string(labelv);
  }
  if (n->kind == COGITO_WINDOW) {
    cogito_apply_window_a11y_label(n);
  }
}

static void cogito_node_set_a11y_role(YisVal nodev, YisVal rolev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_set_a11y_role expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->a11y_role) {
    yis_release_val(YV_STR(n->a11y_role));
    n->a11y_role = NULL;
  }
  if (rolev.tag == EVT_STR) {
    YisStr* rs = (YisStr*)rolev.as.p;
    if (rs) {
      n->a11y_role = rs;
      yis_retain_val(YV_STR(rs));
    }
  } else if (rolev.tag != EVT_NULL) {
    n->a11y_role = stdr_to_string(rolev);
  }
}

__attribute__((unused))
static void cogito_node_set_tooltip_val(YisVal nodev, YisVal textv) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_set_tooltip expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->tooltip) {
    yis_release_val(YV_STR(n->tooltip));
    n->tooltip = NULL;
  }
  if (textv.tag == EVT_STR) {
    YisStr* ts = (YisStr*)textv.as.p;
    if (ts) {
      n->tooltip = ts;
      yis_retain_val(YV_STR(ts));
    }
  } else if (textv.tag != EVT_NULL) {
    n->tooltip = stdr_to_string(textv);
  }
}

static void cogito_build(YisVal nodev, YisVal builder) {
  if (builder.tag != EVT_FN) yis_trap("cogito.build expects function");
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.build expects object");
  YisVal arg = nodev;
  yis_retain_val(arg);
  YisVal ret = yis_call(builder,
  1, &arg);
  yis_release_val(arg);
  yis_release_val(ret);
}

static YisVal cogito_node_window_val(YisVal nodev) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.node_window expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  CogitoNode* win = cogito_node_window(n);
  if (win) return YV_OBJ(win);
  return YV_NULLV;
}
