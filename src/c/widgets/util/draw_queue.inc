// Draw queue: only redraw when something requested it (queue_draw) or layout actually changed.
// Used by the run loop to avoid redraws when idle; relayout calls queue_draw only when geometry changed.

static CogitoNode* cogito_draw_pending_window = NULL;
static bool cogito_layout_did_change = false;
static bool cogito_relayout_deferred = false;
static CogitoNode* cogito_relayout_pending_window = NULL;
static bool cogito_draw_damage_valid = false;
static int cogito_draw_damage_x = 0;
static int cogito_draw_damage_y = 0;
static int cogito_draw_damage_w = 0;
static int cogito_draw_damage_h = 0;
static bool cogito_draw_clip_active = false;
static int cogito_draw_clip_x = 0;
static int cogito_draw_clip_y = 0;
static int cogito_draw_clip_w = 0;
static int cogito_draw_clip_h = 0;

static void cogito_damage_add_rect(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  if (!cogito_draw_damage_valid) {
    cogito_draw_damage_x = x;
    cogito_draw_damage_y = y;
    cogito_draw_damage_w = w;
    cogito_draw_damage_h = h;
    cogito_draw_damage_valid = true;
    return;
  }
  int x0 = cogito_draw_damage_x < x ? cogito_draw_damage_x : x;
  int y0 = cogito_draw_damage_y < y ? cogito_draw_damage_y : y;
  int r0 = cogito_draw_damage_x + cogito_draw_damage_w;
  int b0 = cogito_draw_damage_y + cogito_draw_damage_h;
  int r1 = x + w;
  int b1 = y + h;
  int x1 = r0 > r1 ? r0 : r1;
  int y1 = b0 > b1 ? b0 : b1;
  cogito_draw_damage_x = x0;
  cogito_draw_damage_y = y0;
  cogito_draw_damage_w = x1 - x0;
  cogito_draw_damage_h = y1 - y0;
}

static void cogito_queue_draw(CogitoNode* win) {
  if (!win) return;
  cogito_draw_pending_window = win;
  cogito_draw_damage_valid = false;
  if (win->w > 0 && win->h > 0) {
    cogito_damage_add_rect(0, 0, win->w, win->h);
  }
}

static void cogito_queue_draw_rect(CogitoNode* win, int x, int y, int w, int h) {
  if (!win) return;
  cogito_draw_pending_window = win;
  cogito_damage_add_rect(x, y, w, h);
}

static bool cogito_draw_pending(CogitoNode* win) {
  return win && (cogito_draw_pending_window == win);
}

static void cogito_draw_clear_pending(CogitoNode* win) {
  if (cogito_draw_pending_window == win) {
    cogito_draw_pending_window = NULL;
    cogito_draw_damage_valid = false;
  }
}

static void cogito_layout_mark_changed(void) {
  cogito_layout_did_change = true;
}

static void cogito_layout_begin(void) {
  cogito_layout_did_change = false;
}

static bool cogito_layout_changed(void) {
  return cogito_layout_did_change;
}

static bool cogito_draw_pending_damage(CogitoNode* win, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!win || cogito_draw_pending_window != win || !cogito_draw_damage_valid) return false;
  if (out_x) *out_x = cogito_draw_damage_x;
  if (out_y) *out_y = cogito_draw_damage_y;
  if (out_w) *out_w = cogito_draw_damage_w;
  if (out_h) *out_h = cogito_draw_damage_h;
  return true;
}

static void cogito_draw_set_clip(bool active, int x, int y, int w, int h) {
  cogito_draw_clip_active = active;
  if (active) {
    cogito_draw_clip_x = x;
    cogito_draw_clip_y = y;
    cogito_draw_clip_w = w;
    cogito_draw_clip_h = h;
  } else {
    cogito_draw_clip_x = 0;
    cogito_draw_clip_y = 0;
    cogito_draw_clip_w = 0;
    cogito_draw_clip_h = 0;
  }
}

static void cogito_relayout_defer_begin(void) {
  cogito_relayout_deferred = true;
  cogito_relayout_pending_window = NULL;
}

static void cogito_relayout_defer_request(CogitoNode* win) {
  if (!win) return;
  cogito_relayout_pending_window = win;
}

static CogitoNode* cogito_relayout_defer_take(void) {
  CogitoNode* win = cogito_relayout_pending_window;
  cogito_relayout_pending_window = NULL;
  return win;
}

static void cogito_relayout_defer_end(void) {
  cogito_relayout_deferred = false;
  cogito_relayout_pending_window = NULL;
}
