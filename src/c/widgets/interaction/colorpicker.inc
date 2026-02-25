// Forward declarations for popover functions (implemented in interaction/popover.inc)
static CogitoNode* cogito_popover_new_obj(void);
static void cogito_popover_show(ErgoVal anchorv, ErgoVal popoverv);
static void cogito_popover_close(ErgoVal windowv);

static CogitoNode* cogito_colorpicker_new_obj(void) {
  CogitoNode* cp = cogito_node_new(COGITO_COLORPICKER);
  cp->colorpicker.h = 236.0;
  cp->colorpicker.c = 41.0;
  cp->colorpicker.t = 40.0;
  cp->colorpicker.popover_panel = false;
  cp->colorpicker.owner = NULL;
  cogito_colorpicker_sync_hex(cp);
  return cp;
}
static ErgoVal cogito_colorpicker_new(void) {
  return EV_OBJ(cogito_colorpicker_new_obj());
}
static void cogito_colorpicker_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.colorpicker_on_change expects colorpicker");
  if (handler.tag != EVT_FN) ergo_trap("cogito.colorpicker_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_COLORPICKER) ergo_trap("cogito.colorpicker_on_change expects colorpicker");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}

// Forward declaration (implemented in interaction/slider.inc, included before this)
static CogitoNode* cogito_slider_new_obj(double min, double max, double value);

// Creates a popover containing a colorpicker panel node with 3 slider children.
static CogitoNode* cogito_colorpicker_open_panel(CogitoNode* owner) {
  if (!owner || owner->kind != COGITO_COLORPICKER) return NULL;

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
  panel->colorpicker.popover_panel = true;
  panel->colorpicker.owner = owner;
  panel->colorpicker.h = owner->colorpicker.h;
  panel->colorpicker.c = owner->colorpicker.c;
  panel->colorpicker.t = owner->colorpicker.t;
  panel->radius_set = true;
  panel->border_radius = 20;

  // Add H (0-360), C (0-100), T (0-100) sliders as children
  CogitoNode* sh = cogito_slider_new_obj(0.0, 360.0, owner->colorpicker.h);
  CogitoNode* sc = cogito_slider_new_obj(0.0, 100.0, owner->colorpicker.c);
  CogitoNode* st = cogito_slider_new_obj(0.0, 100.0, owner->colorpicker.t);
  cogito_children_add(panel, sh);
  cogito_children_add(panel, sc);
  cogito_children_add(panel, st);
  ergo_release_val(EV_OBJ(sh));
  ergo_release_val(EV_OBJ(sc));
  ergo_release_val(EV_OBJ(st));

  cogito_children_add(pop, panel);
  ergo_release_val(EV_OBJ(panel));
  return pop;
}

// Shows the colorpicker panel as a popover anchored to the trigger node.
static void cogito_colorpicker_show_popover(CogitoNode* anchor) {
  if (!anchor || anchor->kind != COGITO_COLORPICKER) return;
  CogitoNode* win = cogito_node_window(anchor);
  if (!win) return;
  CogitoNode* pop = cogito_colorpicker_open_panel(anchor);
  if (!pop) return;
  cogito_popover_show(EV_OBJ(anchor), EV_OBJ(pop));
  ergo_release_val(EV_OBJ(pop));
}

// Handles Cancel / OK footer button clicks in the panel.
// Returns true if consumed; slider clicks should be handled by the caller.
static bool cogito_colorpicker_hit_panel(CogitoNode* panel, int mx, int my) {
  if (!panel || panel->kind != COGITO_COLORPICKER || !panel->colorpicker.popover_panel) return false;

  int cx = 0, cy = 0, cw = 0, ch = 0;
  int ox = 0, oy = 0, ow = 0, oh = 0;
  cogito_colorpicker_panel_footer_layout(panel, &cx, &cy, &cw, &ch, &ox, &oy, &ow, &oh);

  if (cogito_hit_rect_min48(mx, my, cx, cy, cw, ch)) {
    // Cancel: close popover, discard panel changes
    CogitoNode* win = cogito_node_window(panel);
    if (win) cogito_popover_close(EV_OBJ(win));
    return true;
  }

  if (cogito_hit_rect_min48(mx, my, ox, oy, ow, oh)) {
    // OK: commit panel H/C/T state to owner, fire onChange, close popover
    CogitoNode* owner = panel->colorpicker.owner;
    CogitoNode* win = cogito_node_window(panel);
    if (owner && owner->kind == COGITO_COLORPICKER) {
      double new_h = panel->colorpicker.h;
      double new_c = panel->colorpicker.c;
      double new_t = panel->colorpicker.t;
      if (new_h < 0.0 || new_h > 360.0 || new_c < 0.0 || new_c > 100.0 || new_t < 0.0 || new_t > 100.0) {
        (void)cogito_colorpicker_panel_read_hct(panel, &new_h, &new_c, &new_t);
      }
      bool changed = (owner->colorpicker.h != new_h ||
                      owner->colorpicker.c != new_c ||
                      owner->colorpicker.t != new_t);
      owner->colorpicker.h = new_h;
      owner->colorpicker.c = new_c;
      owner->colorpicker.t = new_t;
      cogito_colorpicker_sync_hex(owner);
      if (win) cogito_popover_close(EV_OBJ(win));
      if (changed) cogito_invoke_change(owner);
    } else {
      if (win) cogito_popover_close(EV_OBJ(win));
    }
    return true;
  }

  return false;
}

// Handles a click on the trigger node: opens popover (icon) or lets focus block handle hex edit.
static void cogito_colorpicker_click(CogitoNode* n, int mx, int my) {
  if (!n || n->kind != COGITO_COLORPICKER || n->colorpicker.popover_panel) return;

  CogitoNode* win = cogito_node_window(n);
  if (win && win->popover_open && win->popover_anchor == n) {
    cogito_popover_close(EV_OBJ(win));
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
