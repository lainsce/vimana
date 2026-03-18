// Forward declarations for popover functions (implemented in interaction/popover.inc)
static CogitoNode* cogito_popover_new_obj(void);
static void cogito_popover_show(YisVal anchorv, YisVal popoverv);
static void cogito_popover_close(YisVal windowv);

static CogitoNode* cogito_colorpicker_new_obj(void) {
  CogitoNode* cp = cogito_node_new(COGITO_COLORPICKER);
  cp->kd->colorpicker.h = 236.0;
  cp->kd->colorpicker.c = 41.0;
  cp->kd->colorpicker.t = 40.0;
  cp->kd->colorpicker.popover_panel = false;
  cp->kd->colorpicker.owner = NULL;
  cogito_colorpicker_sync_hex(cp);
  return cp;
}
static YisVal cogito_colorpicker_new(void) {
  return YV_OBJ(cogito_colorpicker_new_obj());
}
static void cogito_colorpicker_on_change(YisVal nodev, YisVal handler) {
  if (nodev.tag != EVT_OBJ) yis_trap("cogito.colorpicker_on_change expects colorpicker");
  if (handler.tag != EVT_FN) yis_trap("cogito.colorpicker_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_COLORPICKER) yis_trap("cogito.colorpicker_on_change expects colorpicker");
  if (n->on_change) yis_release_val(YV_FN(n->on_change));
  n->on_change = (YisFn*)handler.as.p;
  yis_retain_val(YV_FN(n->on_change));
}

// Forward declaration (implemented in interaction/slider.inc, included before this)
static CogitoNode* cogito_slider_new_obj(double min, double max, double value);

// Creates a popover containing a colorpicker panel node with 3 slider children.
static CogitoNode* cogito_colorpicker_open_panel(CogitoNode* owner) {
  if (!owner || owner->kind != COGITO_COLORPICKER) return NULL;
  if (!owner->text || !owner->text->data || !owner->text->data[0]) {
    cogito_colorpicker_sync_hex(owner);
  }

  CogitoNode* pop = cogito_popover_new_obj();
  pop->padding_set = true;
  pop->padding_left = 0;
  pop->padding_top = 0;
  pop->padding_right = 0;
  pop->padding_bottom = 0;
  pop->gap = 0;
  pop->radius_set = true;
  pop->border_radius = 20;

  CogitoNode* panel = cogito_node_new(COGITO_COLORPICKER);
  panel->editable = false;
  panel->kd->colorpicker.popover_panel = true;
  panel->kd->colorpicker.owner = owner;
  panel->kd->colorpicker.h = owner->kd->colorpicker.h;
  panel->kd->colorpicker.c = owner->kd->colorpicker.c;
  panel->kd->colorpicker.t = owner->kd->colorpicker.t;
  panel->radius_set = true;
  panel->border_radius = 20;

  // Add H (0-360), C (0-100), T (0-100) sliders as children
  CogitoNode* sh = cogito_slider_new_obj(0.0, 360.0, owner->kd->colorpicker.h);
  CogitoNode* sc = cogito_slider_new_obj(0.0, 100.0, owner->kd->colorpicker.c);
  CogitoNode* st = cogito_slider_new_obj(0.0, 100.0, owner->kd->colorpicker.t);
  cogito_children_add(panel, sh);
  cogito_children_add(panel, sc);
  cogito_children_add(panel, st);
  yis_release_val(YV_OBJ(sh));
  yis_release_val(YV_OBJ(sc));
  yis_release_val(YV_OBJ(st));

  cogito_children_add(pop, panel);
  yis_release_val(YV_OBJ(panel));
  return pop;
}

// Shows the colorpicker panel as a popover anchored to the trigger node.
static void cogito_colorpicker_show_popover(CogitoNode* anchor) {
  if (!anchor || anchor->kind != COGITO_COLORPICKER) return;
  CogitoNode* win = cogito_node_window(anchor);
  if (!win) return;
  CogitoNode* pop = cogito_colorpicker_open_panel(anchor);
  if (!pop) return;
  cogito_popover_show(YV_OBJ(anchor), YV_OBJ(pop));
  yis_release_val(YV_OBJ(pop));
}

// Handles Cancel / OK footer button clicks in the panel.
// Returns true if consumed; slider clicks should be handled by the caller.
static bool cogito_colorpicker_hit_panel(CogitoNode* panel, int mx, int my) {
  if (!panel || panel->kind != COGITO_COLORPICKER || !panel->kd->colorpicker.popover_panel) return false;

  int cx = 0, cy = 0, cw = 0, ch = 0;
  int ox = 0, oy = 0, ow = 0, oh = 0;
  cogito_colorpicker_panel_footer_layout(panel, &cx, &cy, &cw, &ch, &ox, &oy, &ow, &oh);

  if (cogito_hit_rect_min48(mx, my, cx, cy, cw, ch)) {
    // Cancel: close popover, discard panel changes
    CogitoNode* win = cogito_node_window(panel);
    if (win) cogito_popover_close(YV_OBJ(win));
    return true;
  }

  if (cogito_hit_rect_min48(mx, my, ox, oy, ow, oh)) {
    // OK: commit panel H/C/T state to owner, fire onChange, close popover
    CogitoNode* owner = panel->kd->colorpicker.owner;
    CogitoNode* win = cogito_node_window(panel);
    if (owner && owner->kind == COGITO_COLORPICKER) {
      double new_h = panel->kd->colorpicker.h;
      double new_c = panel->kd->colorpicker.c;
      double new_t = panel->kd->colorpicker.t;
      if (new_h < 0.0 || new_h > 360.0 || new_c < 0.0 || new_c > 100.0 || new_t < 0.0 || new_t > 100.0) {
        (void)cogito_colorpicker_panel_read_hct(panel, &new_h, &new_c, &new_t);
      }
      bool changed = (owner->kd->colorpicker.h != new_h ||
                      owner->kd->colorpicker.c != new_c ||
                      owner->kd->colorpicker.t != new_t);
      owner->kd->colorpicker.h = new_h;
      owner->kd->colorpicker.c = new_c;
      owner->kd->colorpicker.t = new_t;
      cogito_colorpicker_sync_hex(owner);
      if (win) cogito_popover_close(YV_OBJ(win));
      if (changed) cogito_invoke_change(owner);
    } else {
      if (win) cogito_popover_close(YV_OBJ(win));
    }
    return true;
  }

  return false;
}

// Handles a click on the trigger node: opens popover (icon) or lets focus block handle hex edit.
static void cogito_colorpicker_click(CogitoNode* n, int mx, int my) {
  if (!n || n->kind != COGITO_COLORPICKER || n->kd->colorpicker.popover_panel) return;

  CogitoNode* win = cogito_node_window(n);
  if (win && win->popover_open && win->popover_anchor == n) {
    cogito_popover_close(YV_OBJ(win));
    return;
  }

  // Icon zone click → open panel popover
  int ix = 0, iy = 0, iw = 0, ih = 0;
  cogito_colorpicker_field_layout(n, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &ix, &iy, &iw, &ih);
  if (cogito_hit_rect_min48(mx, my, ix, iy, iw, ih)) {
    cogito_colorpicker_show_popover(n);
    return;
  }
  // Hex zone: hex_editing and caret are handled by the focus block in 15_run.inc
}
