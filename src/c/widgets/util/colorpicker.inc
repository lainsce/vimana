// ColorPicker sizing constants - centralized to match DatePicker
enum {
  COGITO_COLORPICKER_FIELD_MIN_WIDTH = 220,
  COGITO_COLORPICKER_FIELD_HEIGHT    = 56,
  COGITO_COLORPICKER_PANEL_W         = 360,
  COGITO_COLORPICKER_PANEL_PAD       = 12,  // Match DATEPICKER_POP_SIDE
  COGITO_COLORPICKER_PANEL_TOP       = 20,  // Match DATEPICKER_POP_TOP
  COGITO_COLORPICKER_PANEL_HEADER_H  = 24,  // Match DATEPICKER_POP_HEADER_H
  COGITO_COLORPICKER_PANEL_HEADER_TO_ROWS_GAP = 24,
  COGITO_COLORPICKER_PANEL_LABEL_W   = 36,
  COGITO_COLORPICKER_PANEL_LABEL_GAP = 8,
  COGITO_COLORPICKER_PANEL_SLIDER_H  = 88,
  COGITO_COLORPICKER_PANEL_ROW_GAP   = 12,
  COGITO_COLORPICKER_PANEL_VALUE_GAP = 8,
  COGITO_COLORPICKER_PANEL_VALUE_W   = 36,
  COGITO_COLORPICKER_PANEL_FOOTER_H  = 36,  // Match DATEPICKER_POP_FOOTER_H
  COGITO_COLORPICKER_PANEL_FOOTER_GAP = 8,
  COGITO_COLORPICKER_PANEL_BOTTOM    = 12,  // Match DATEPICKER_POP_BOTTOM
};

static int cogito_colorpicker_panel_rows_top(const CogitoNode* n) {
  if (!n) return 0;
  return n->y + COGITO_COLORPICKER_PANEL_TOP + COGITO_COLORPICKER_PANEL_HEADER_H +
         COGITO_COLORPICKER_PANEL_HEADER_TO_ROWS_GAP;
}

static double cogito_colorpicker_clamp(double v, double lo, double hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static bool cogito_colorpicker_hit_rect_min48_local(int x, int y, int rx, int ry, int rw, int rh) {
  int hit_w = rw < 48 ? 48 : rw;
  int hit_h = rh < 48 ? 48 : rh;
  int hit_x = rx - (hit_w - rw) / 2;
  int hit_y = ry - (hit_h - rh) / 2;
  return cogito_hit_rect(x, y, hit_x, hit_y, hit_w, hit_h);
}

static void cogito_colorpicker_sync_hex(CogitoNode* n) {
  if (!n || n->kind != COGITO_COLORPICKER) return;
  CogitoColor c = cogito_hct_to_rgb(n->kd->colorpicker.h, n->kd->colorpicker.c, n->kd->colorpicker.t);
  char buf[8];
  cogito_color_to_hex(c, buf);
  cogito_text_set_from_buf(n, buf, strlen(buf));
}

// Trigger field layout: [swatch 28x28] [hex text] [icon button 44xh]
static void cogito_colorpicker_field_layout(
    CogitoNode* n,
    int* swatch_x, int* swatch_y, int* swatch_w, int* swatch_h,
    int* hex_x, int* hex_y, int* hex_w, int* hex_h,
    int* icon_x, int* icon_y, int* icon_w, int* icon_h) {
  int sw = 28, sh = 28;
  int pad_l = 12;
  int gap = 8;
  int icon_btn_w = 44;
  int sx = n->x + pad_l;
  int sy = n->y + (n->h - sh) / 2;
  int hx = sx + sw + gap;
  int hw = n->w - pad_l - sw - gap - icon_btn_w;
  if (hw < 0) hw = 0;
  int ix = n->x + n->w - icon_btn_w;
  if (swatch_x) *swatch_x = sx;
  if (swatch_y) *swatch_y = sy;
  if (swatch_w) *swatch_w = sw;
  if (swatch_h) *swatch_h = sh;
  if (hex_x) *hex_x = hx;
  if (hex_y) *hex_y = n->y;
  if (hex_w) *hex_w = hw;
  if (hex_h) *hex_h = n->h;
  if (icon_x) *icon_x = ix;
  if (icon_y) *icon_y = n->y;
  if (icon_w) *icon_w = icon_btn_w;
  if (icon_h) *icon_h = n->h;
}

// Panel footer layout: Cancel and OK buttons at the bottom
static void cogito_colorpicker_panel_footer_layout(
    CogitoNode* n,
    int* cancel_x, int* cancel_y, int* cancel_w, int* cancel_h,
    int* ok_x, int* ok_y, int* ok_w, int* ok_h) {
  int footer_h = COGITO_COLORPICKER_PANEL_FOOTER_H;
  int fy = n->y + n->h - COGITO_COLORPICKER_PANEL_BOTTOM - footer_h;
  int okbw = 44;
  int cxbw = 84;
  int btn_h = 32;
  int gap = 16;
  int bty = fy + (footer_h - btn_h) / 2;
  int ox = n->x + n->w - COGITO_COLORPICKER_PANEL_PAD - okbw;
  int cx = ox - gap - cxbw;
  if (cancel_x) *cancel_x = cx;
  if (cancel_y) *cancel_y = bty;
  if (cancel_w) *cancel_w = cxbw;
  if (cancel_h) *cancel_h = btn_h;
  if (ok_x) *ok_x = ox;
  if (ok_y) *ok_y = bty;
  if (ok_w) *ok_w = okbw;
  if (ok_h) *ok_h = btn_h;
}

static void cogito_colorpicker_slider_rect(CogitoNode* n, int idx, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (n && n->kind == COGITO_COLORPICKER && n->kd->colorpicker.popover_panel) {
    int row = 0;
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* c = n->children[i];
      if (!c || c->kind != COGITO_SLIDER) continue;
      if (row == idx) {
        if (out_x) *out_x = c->x;
        if (out_y) *out_y = c->y;
        if (out_w) *out_w = c->w;
        if (out_h) *out_h = c->h;
        return;
      }
      row++;
    }
  }

  int row_h = COGITO_COLORPICKER_PANEL_SLIDER_H;
  int slider_h = COGITO_COLORPICKER_PANEL_SLIDER_H;
  int row_y = cogito_colorpicker_panel_rows_top(n) + idx * (row_h + COGITO_COLORPICKER_PANEL_ROW_GAP);
  int x = n->x + COGITO_COLORPICKER_PANEL_PAD + COGITO_COLORPICKER_PANEL_LABEL_W + COGITO_COLORPICKER_PANEL_LABEL_GAP;
    int w = n->w - COGITO_COLORPICKER_PANEL_PAD * 2 -
      COGITO_COLORPICKER_PANEL_LABEL_W - COGITO_COLORPICKER_PANEL_LABEL_GAP -
      COGITO_COLORPICKER_PANEL_VALUE_GAP - COGITO_COLORPICKER_PANEL_VALUE_W;
  int y = row_y + (row_h - slider_h) / 2;
  if (w < 1) w = 1;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = w;
  if (out_h) *out_h = slider_h;
}

// Collect up to the first 3 slider children sorted by visual row (top to bottom).
static int cogito_colorpicker_panel_collect_sliders(CogitoNode* panel, CogitoNode* out_rows[3]) {
  if (out_rows) {
    out_rows[0] = NULL;
    out_rows[1] = NULL;
    out_rows[2] = NULL;
  }
  if (!panel || panel->kind != COGITO_COLORPICKER) return 0;

  CogitoNode* r0 = NULL;
  CogitoNode* r1 = NULL;
  CogitoNode* r2 = NULL;
  for (size_t i = 0; i < panel->len; i++) {
    CogitoNode* c = panel->children[i];
    if (!c || c->kind != COGITO_SLIDER) continue;

    if (!r0 || c->y < r0->y) {
      r2 = r1;
      r1 = r0;
      r0 = c;
    } else if (!r1 || c->y < r1->y) {
      r2 = r1;
      r1 = c;
    } else if (!r2 || c->y < r2->y) {
      r2 = c;
    }
  }

  int count = 0;
  if (r0) {
    if (out_rows) out_rows[count] = r0;
    count++;
  }
  if (r1) {
    if (out_rows) out_rows[count] = r1;
    count++;
  }
  if (r2) {
    if (out_rows) out_rows[count] = r2;
    count++;
  }
  return count;
}

// Reads H/C/T from panel sliders by row order: top=H, middle=C, bottom=T.
static bool cogito_colorpicker_panel_read_hct(CogitoNode* panel, double* out_h, double* out_c, double* out_t) {
  CogitoNode* rows[3] = { NULL, NULL, NULL };
  if (cogito_colorpicker_panel_collect_sliders(panel, rows) < 3) return false;
  if (out_h) *out_h = rows[0]->kd->slider.value;
  if (out_c) *out_c = rows[1]->kd->slider.value;
  if (out_t) *out_t = rows[2]->kd->slider.value;
  return true;
}

// Returns slider channel by visual row: top=0(H), middle=1(C), bottom=2(T).
static int cogito_colorpicker_panel_slider_channel(CogitoNode* panel, CogitoNode* slider) {
  if (!panel || panel->kind != COGITO_COLORPICKER || !slider || slider->kind != COGITO_SLIDER) return -1;
  CogitoNode* rows[3] = { NULL, NULL, NULL };
  int count = cogito_colorpicker_panel_collect_sliders(panel, rows);
  for (int i = 0; i < count && i < 3; i++) {
    if (rows[i] == slider) return i;
  }
  return -1;
}

static void cogito_colorpicker_panel_apply_hct_to_sliders(CogitoNode* panel) {
  if (!panel || panel->kind != COGITO_COLORPICKER) return;
  CogitoNode* rows[3] = { NULL, NULL, NULL };
  if (cogito_colorpicker_panel_collect_sliders(panel, rows) < 3) return;
  rows[0]->kd->slider.value = cogito_colorpicker_clamp(panel->kd->colorpicker.h, rows[0]->kd->slider.min, rows[0]->kd->slider.max);
  rows[0]->kd->slider.value2 = rows[0]->kd->slider.value;
  rows[1]->kd->slider.value = cogito_colorpicker_clamp(panel->kd->colorpicker.c, rows[1]->kd->slider.min, rows[1]->kd->slider.max);
  rows[1]->kd->slider.value2 = rows[1]->kd->slider.value;
  rows[2]->kd->slider.value = cogito_colorpicker_clamp(panel->kd->colorpicker.t, rows[2]->kd->slider.min, rows[2]->kd->slider.max);
  rows[2]->kd->slider.value2 = rows[2]->kd->slider.value;
}

// Sync panel H/C/T when one of its slider children changed via normal slider drag.
static bool cogito_colorpicker_panel_sync_from_slider(CogitoNode* slider) {
  if (!slider || slider->kind != COGITO_SLIDER || !slider->parent) return false;
  CogitoNode* panel = slider->parent;
  if (panel->kind != COGITO_COLORPICKER || !panel->kd->colorpicker.popover_panel) return false;
  int chan = cogito_colorpicker_panel_slider_channel(panel, slider);
  if (chan == 0) panel->kd->colorpicker.h = slider->kd->slider.value;
  else if (chan == 1) panel->kd->colorpicker.c = slider->kd->slider.value;
  else if (chan == 2) panel->kd->colorpicker.t = slider->kd->slider.value;
  else return false;
  if (!panel->kd->colorpicker.hex_editing) {
    cogito_colorpicker_sync_hex(panel);
  }
  return true;
}

// Hit-test panel rows directly (fallback when slider child is not the click target).
static int cogito_colorpicker_panel_hit_channel(CogitoNode* panel, int mx, int my) {
  if (!panel || panel->kind != COGITO_COLORPICKER || !panel->kd->colorpicker.popover_panel) return -1;
  for (int chan = 0; chan < 3; chan++) {
    int sx = 0, sy = 0, sw = 0, sh = 0;
    cogito_colorpicker_slider_rect(panel, chan, &sx, &sy, &sw, &sh);
    if (cogito_colorpicker_hit_rect_min48_local(mx, my, sx, sy, sw, sh)) return chan;
  }
  return -1;
}

// Mode-aware hex input rect — panel uses panel layout, trigger uses field layout
static bool cogito_colorpicker_hex_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!n) return false;
  if (n->kd->colorpicker.popover_panel) {
    if (out_x) *out_x = 0;
    if (out_y) *out_y = 0;
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    return false;
  } else {
    int hx = 0, hw = 0, hh = 0;
    cogito_colorpicker_field_layout(n, NULL, NULL, NULL, NULL, &hx, NULL, &hw, &hh, NULL, NULL, NULL, NULL);
    if (out_x) *out_x = hx;
    if (out_y) *out_y = n->y;
    if (out_w) *out_w = hw;
    if (out_h) *out_h = hh;
  }
  return true;
}

static bool cogito_colorpicker_set_from_mouse(CogitoNode* n, int channel, int mx) {
  int sx = 0, sy = 0, sw = 0, sh = 0;
  cogito_colorpicker_slider_rect(n, channel, &sx, &sy, &sw, &sh);
  double t = (sw > 0) ? ((double)(mx - sx) / (double)sw) : 0.0;
  if (t < 0.0) t = 0.0;
  if (t > 1.0) t = 1.0;
  double prev = 0.0;
  if (channel == 0) { prev = n->kd->colorpicker.h; n->kd->colorpicker.h = t * 360.0; }
  else if (channel == 1) { prev = n->kd->colorpicker.c; n->kd->colorpicker.c = t * 100.0; }
  else { prev = n->kd->colorpicker.t; n->kd->colorpicker.t = t * 100.0; }
  if (!n->kd->colorpicker.hex_editing) {
    cogito_colorpicker_sync_hex(n);
  }
  return prev != (channel == 0 ? n->kd->colorpicker.h : (channel == 1 ? n->kd->colorpicker.c : n->kd->colorpicker.t));
}
