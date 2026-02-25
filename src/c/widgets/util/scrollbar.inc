/* Shared scrollbar geometry and hit-test for overlay scrollbars.
 * Uses sb_width for the scrollbar thickness (4 when not hovered, 8 when hovered). */

typedef enum {
  COGITO_SCROLLBAR_HIT_NONE = 0,
  COGITO_SCROLLBAR_HIT_VERT_TRACK,
  COGITO_SCROLLBAR_HIT_VERT_THUMB,
  COGITO_SCROLLBAR_HIT_HORZ_TRACK,
  COGITO_SCROLLBAR_HIT_HORZ_THUMB
} CogitoScrollbarHit;

#define COGITO_SCROLLBAR_WIDTH_NARROW  4
#define COGITO_SCROLLBAR_WIDTH_WIDE    8
#define COGITO_SCROLLBAR_MIN_THUMB     24

/* Compute vertical scrollbar track and thumb rects. Returns true if vertical bar is visible.
 * track/thumb out rects: x, y, w, h. sb_width is the bar thickness (4 or 8). */
static bool cogito_scroller_vert_scrollbar_rects(CogitoNode* n, int sb_width,
    int* track_x, int* track_y, int* track_w, int* track_h,
    int* thumb_x, int* thumb_y, int* thumb_w, int* thumb_h) {
  if (!n || !n->scroller.vert || n->scroller.content_h <= 0) return false;
  int inner_y = n->y + n->padding_top;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_h <= 0) return false;
  int max_y = n->scroller.content_h - inner_h;
  if (max_y <= 0) return false;
  *track_x = n->x + n->w - sb_width;
  *track_y = inner_y;
  *track_w = sb_width;
  *track_h = inner_h;
  int th_h = (inner_h * inner_h) / n->scroller.content_h;
  if (th_h < COGITO_SCROLLBAR_MIN_THUMB) th_h = COGITO_SCROLLBAR_MIN_THUMB;
  if (th_h > inner_h) th_h = inner_h;
  *thumb_x = *track_x;
  *thumb_w = sb_width;
  *thumb_h = th_h;
  *thumb_y = *track_y + (int)((long)(*track_h - th_h) * (long)n->scroller.scroll_y / (long)max_y);
  if (*thumb_y < *track_y) *thumb_y = *track_y;
  if (*thumb_y + *thumb_h > *track_y + *track_h) *thumb_y = *track_y + *track_h - *thumb_h;
  return true;
}

static bool cogito_scroller_horz_scrollbar_rects(CogitoNode* n, int sb_height,
    int* track_x, int* track_y, int* track_w, int* track_h,
    int* thumb_x, int* thumb_y, int* thumb_w, int* thumb_h) {
  if (!n || !n->scroller.horz || n->scroller.content_w <= 0) return false;
  int inner_x = n->x + n->padding_left;
  int inner_w = n->w - n->padding_left - n->padding_right;
  if (inner_w <= 0) return false;
  int max_x = n->scroller.content_w - inner_w;
  if (max_x <= 0) return false;
  *track_x = inner_x;
  *track_y = n->y + n->h - sb_height;
  *track_w = inner_w;
  *track_h = sb_height;
  int th_w = (inner_w * inner_w) / n->scroller.content_w;
  if (th_w < COGITO_SCROLLBAR_MIN_THUMB) th_w = COGITO_SCROLLBAR_MIN_THUMB;
  if (th_w > inner_w) th_w = inner_w;
  *thumb_y = *track_y;
  *thumb_h = sb_height;
  *thumb_w = th_w;
  *thumb_x = *track_x + (int)((long)(*track_w - th_w) * (long)n->scroller.scroll_x / (long)max_x);
  if (*thumb_x < *track_x) *thumb_x = *track_x;
  if (*thumb_x + *thumb_w > *track_x + *track_w) *thumb_x = *track_x + *track_w - *thumb_w;
  return true;
}

/* Hit-test (mx, my) against scrollbar. Use sb_width/sb_height for bar size (narrow=4 for testing hover). */
static CogitoScrollbarHit cogito_scroller_hit_scrollbar(CogitoNode* n, int mx, int my, int sb_width, int sb_height) {
  if (!n) return COGITO_SCROLLBAR_HIT_NONE;
  int vtx, vty, vtw, vth, vux, vuy, vuw, vuh;
  if (cogito_scroller_vert_scrollbar_rects(n, sb_width, &vtx, &vty, &vtw, &vth, &vux, &vuy, &vuw, &vuh)) {
    if (cogito_hit_rect(mx, my, vux, vuy, vuw, vuh)) return COGITO_SCROLLBAR_HIT_VERT_THUMB;
    if (cogito_hit_rect(mx, my, vtx, vty, vtw, vth)) return COGITO_SCROLLBAR_HIT_VERT_TRACK;
  }
  int htx, hty, htw, hth, hux, huy, huw, huh;
  if (cogito_scroller_horz_scrollbar_rects(n, sb_height, &htx, &hty, &htw, &hth, &hux, &huy, &huw, &huh)) {
    if (cogito_hit_rect(mx, my, hux, huy, huw, huh)) return COGITO_SCROLLBAR_HIT_HORZ_THUMB;
    if (cogito_hit_rect(mx, my, htx, hty, htw, hth)) return COGITO_SCROLLBAR_HIT_HORZ_TRACK;
  }
  return COGITO_SCROLLBAR_HIT_NONE;
}

/* Get vertical scrollbar max_scroll and travel (track_h - thumb_h) for drag. Returns true if vertical bar is active. */
static bool cogito_scroller_vert_drag_params(CogitoNode* n, int* out_max_scroll, int* out_travel) {
  if (!n || !n->scroller.vert || n->scroller.content_h <= 0) return false;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_h <= 0) return false;
  int max_y = n->scroller.content_h - inner_h;
  if (max_y <= 0) return false;
  int thumb_h = (inner_h * inner_h) / n->scroller.content_h;
  if (thumb_h < COGITO_SCROLLBAR_MIN_THUMB) thumb_h = COGITO_SCROLLBAR_MIN_THUMB;
  if (thumb_h > inner_h) thumb_h = inner_h;
  *out_max_scroll = max_y;
  *out_travel = inner_h - thumb_h;
  if (*out_travel < 1) *out_travel = 1;
  return true;
}

static bool cogito_scroller_horz_drag_params(CogitoNode* n, int* out_max_scroll, int* out_travel) {
  if (!n || !n->scroller.horz || n->scroller.content_w <= 0) return false;
  int inner_w = n->w - n->padding_left - n->padding_right;
  if (inner_w <= 0) return false;
  int max_x = n->scroller.content_w - inner_w;
  if (max_x <= 0) return false;
  int thumb_w = (inner_w * inner_w) / n->scroller.content_w;
  if (thumb_w < COGITO_SCROLLBAR_MIN_THUMB) thumb_w = COGITO_SCROLLBAR_MIN_THUMB;
  if (thumb_w > inner_w) thumb_w = inner_w;
  *out_max_scroll = max_x;
  *out_travel = inner_w - thumb_w;
  if (*out_travel < 1) *out_travel = 1;
  return true;
}

/* Compute scroll position so thumb center is at mouse position (for track click). Vert: my, horz: mx. */
static int cogito_scroller_track_to_scroll(int click_pos, int track_start, int track_size, int thumb_size, int max_scroll) {
  int travel = track_size - thumb_size;
  if (travel <= 0) return 0;
  int center = click_pos - track_start - thumb_size / 2;
  if (center < 0) center = 0;
  if (center > travel) center = travel;
  return (int)((long)center * (long)max_scroll / (long)travel);
}
