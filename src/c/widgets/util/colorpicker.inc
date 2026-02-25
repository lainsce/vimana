// ColorPicker sizing constants
enum {
  COGITO_COLORPICKER_FIELD_MIN_WIDTH = 220,
  COGITO_COLORPICKER_FIELD_HEIGHT    = 56,
  COGITO_COLORPICKER_PANEL_W         = 360,
  COGITO_COLORPICKER_PANEL_PAD       = 16,
  COGITO_COLORPICKER_PANEL_FOOTER_H  = 48,
};

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
  CogitoColor c = cogito_hct_to_rgb(n->colorpicker.h, n->colorpicker.c, n->colorpicker.t);
  char buf[8];
  cogito_color_to_hex(c, buf);
  cogito_text_set_from_buf(n, buf, strlen(buf));
}

static void cogito_colorpicker_layout(CogitoNode* n, int* out_pad_l, int* out_pad_t, int* out_pad_r, int* out_pad_b,
                                      int* out_label_w, int* out_row_h, int* out_slider_h, int* out_gap,
                                      int* out_hex_x, int* out_hex_y, int* out_hex_w, int* out_hex_h) {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = s.has_padding_left ? s.padding_left : 10;
  int pad_t = s.has_padding_top ? s.padding_top : 10;
  int pad_r = s.has_padding_right ? s.padding_right : pad_l;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
  int size = cogito_node_font_size(n);
  int row_h = size + 10;
  int slider_h = 6;
  int gap = 8;
  int label_w = 12;
  int preview_w = row_h;
  int preview_gap = 8;
  int hex_x = n->x + pad_l + preview_w + preview_gap;
  int hex_y = n->y + pad_t + (row_h + gap) * 3;
  int hex_w = n->w - pad_l - pad_r - preview_w - preview_gap;
  int hex_h = row_h;
  if (out_pad_l) *out_pad_l = pad_l;
  if (out_pad_t) *out_pad_t = pad_t;
  if (out_pad_r) *out_pad_r = pad_r;
  if (out_pad_b) *out_pad_b = pad_b;
  if (out_label_w) *out_label_w = label_w;
  if (out_row_h) *out_row_h = row_h;
  if (out_slider_h) *out_slider_h = slider_h;
  if (out_gap) *out_gap = gap;
  if (out_hex_x) *out_hex_x = hex_x;
  if (out_hex_y) *out_hex_y = hex_y;
  if (out_hex_w) *out_hex_w = hex_w;
  if (out_hex_h) *out_hex_h = hex_h;
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
  int fy = n->y + n->h - footer_h;
  int btn_w = 80;
  int btn_h = 36;
  int btn_pad_r = 12;
  int gap = 8;
  int bty = fy + (footer_h - btn_h) / 2;
  int ox = n->x + n->w - btn_pad_r - btn_w;
  int cx = ox - gap - btn_w;
  if (cancel_x) *cancel_x = cx;
  if (cancel_y) *cancel_y = bty;
  if (cancel_w) *cancel_w = btn_w;
  if (cancel_h) *cancel_h = btn_h;
  if (ok_x) *ok_x = ox;
  if (ok_y) *ok_y = bty;
  if (ok_w) *ok_w = btn_w;
  if (ok_h) *ok_h = btn_h;
}

static void cogito_colorpicker_slider_rect(CogitoNode* n, int idx, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (n && n->kind == COGITO_COLORPICKER && n->colorpicker.popover_panel) {
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

  int pad_l = 0, pad_t = 0, pad_r = 0, label_w = 0, row_h = 0, slider_h = 0, gap = 0;
  cogito_colorpicker_layout(n, &pad_l, &pad_t, &pad_r, NULL, &label_w, &row_h, &slider_h, &gap, NULL, NULL, NULL, NULL);
  int row_y = n->y + pad_t + idx * (row_h + gap);
  int x = n->x + pad_l + label_w + 6;
  int w = n->w - pad_l - pad_r - label_w - 6;
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
  if (out_h) *out_h = rows[0]->slider.value;
  if (out_c) *out_c = rows[1]->slider.value;
  if (out_t) *out_t = rows[2]->slider.value;
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
  rows[0]->slider.value = cogito_colorpicker_clamp(panel->colorpicker.h, rows[0]->slider.min, rows[0]->slider.max);
  rows[0]->slider.value2 = rows[0]->slider.value;
  rows[1]->slider.value = cogito_colorpicker_clamp(panel->colorpicker.c, rows[1]->slider.min, rows[1]->slider.max);
  rows[1]->slider.value2 = rows[1]->slider.value;
  rows[2]->slider.value = cogito_colorpicker_clamp(panel->colorpicker.t, rows[2]->slider.min, rows[2]->slider.max);
  rows[2]->slider.value2 = rows[2]->slider.value;
}

// Sync panel H/C/T when one of its slider children changed via normal slider drag.
static bool cogito_colorpicker_panel_sync_from_slider(CogitoNode* slider) {
  if (!slider || slider->kind != COGITO_SLIDER || !slider->parent) return false;
  CogitoNode* panel = slider->parent;
  if (panel->kind != COGITO_COLORPICKER || !panel->colorpicker.popover_panel) return false;
  int chan = cogito_colorpicker_panel_slider_channel(panel, slider);
  if (chan == 0) panel->colorpicker.h = slider->slider.value;
  else if (chan == 1) panel->colorpicker.c = slider->slider.value;
  else if (chan == 2) panel->colorpicker.t = slider->slider.value;
  else return false;
  if (!panel->colorpicker.hex_editing) {
    cogito_colorpicker_sync_hex(panel);
  }
  return true;
}

// Hit-test panel rows directly (fallback when slider child is not the click target).
static int cogito_colorpicker_panel_hit_channel(CogitoNode* panel, int mx, int my) {
  if (!panel || panel->kind != COGITO_COLORPICKER || !panel->colorpicker.popover_panel) return -1;
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
  if (n->colorpicker.popover_panel) {
    int hx = 0, hy = 0, hw = 0, hh = 0;
    cogito_colorpicker_layout(n, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &hx, &hy, &hw, &hh);
    if (out_x) *out_x = hx;
    if (out_y) *out_y = hy;
    if (out_w) *out_w = hw;
    if (out_h) *out_h = hh;
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
  if (channel == 0) { prev = n->colorpicker.h; n->colorpicker.h = t * 360.0; }
  else if (channel == 1) { prev = n->colorpicker.c; n->colorpicker.c = t * 100.0; }
  else { prev = n->colorpicker.t; n->colorpicker.t = t * 100.0; }
  if (!n->colorpicker.hex_editing) {
    cogito_colorpicker_sync_hex(n);
  }
  return prev != (channel == 0 ? n->colorpicker.h : (channel == 1 ? n->colorpicker.c : n->colorpicker.t));
}
