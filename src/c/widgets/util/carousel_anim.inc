// Carousel animation helpers
// Scroll math macros for snap-to-item carousel behavior.

// Compute max_scroll for a carousel node (must match layout formula).
#define CAROUSEL_MAX_SCROLL(cr, out_max) do { \
  int _iw = (cr)->w - (cr)->padding_left - (cr)->padding_right; \
  if (_iw < 0) _iw = 0; \
  int _gaps = ((int)(cr)->len >= 3) ? 2 : ((int)(cr)->len - 1); \
  if (_gaps < 0) _gaps = 0; \
  int _avail = _iw - (_gaps * 8); \
  if (_avail < 0) _avail = 0; \
  int _ers = ((int)(cr)->len >= 3) ? ((int)(cr)->len + 5) : 7; \
  int _ecw = _ers * _avail / 7 + ((int)(cr)->len - 1) * 8; \
  (out_max) = _ecw - _iw; \
  if ((out_max) < 0) (out_max) = 0; \
} while(0)

// Compute the snap scroll_x for a given item index.
#define CAROUSEL_SNAP_SCROLL(cr, idx, max_scr, out_sx) do { \
  int _cnt = (int)(cr)->len; \
  if (_cnt > 1) { \
    (out_sx) = (int)((double)(idx) / (double)(_cnt - 1) * (double)(max_scr) + 0.5); \
    if ((out_sx) < 0) (out_sx) = 0; \
    if ((out_sx) > (max_scr)) (out_sx) = (max_scr); \
  } else { \
    (out_sx) = 0; \
  } \
} while(0)

// Snap-to-nearest-item: given a float scroll position, return the nearest index.
#define CAROUSEL_NEAREST_INDEX(cr, scroll_xf, max_scr, out_idx) do { \
  double _prog = ((max_scr) > 0) ? (scroll_xf) / (double)(max_scr) : 0.0; \
  if (_prog < 0.0) _prog = 0.0; \
  if (_prog > 1.0) _prog = 1.0; \
  (out_idx) = (int)(_prog * (double)((int)(cr)->len - 1) + 0.5); \
  if ((out_idx) < 0) (out_idx) = 0; \
  if ((out_idx) >= (int)(cr)->len) (out_idx) = (int)(cr)->len - 1; \
} while(0)

#define CAROUSEL_ANIM_DURATION (cogito_prefers_reduced_motion ? 0.0 : 0.35)
