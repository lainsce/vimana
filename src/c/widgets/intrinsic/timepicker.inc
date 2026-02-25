case COGITO_TIMEPICKER: {
  if (n->timepicker.popover_panel) {
    w = 328;
    // Keep structural vertical spacing when dial is hidden; only remove dial height.
    h = 24 + 20 + 80 + 36 + 48 + 24;
    if (n->timepicker.dial_visible) h += 256;
  } else {
    // Keep intrinsic size as outer size (including style padding).
    int pad_w = n->padding_left + n->padding_right;
    int pad_h = n->padding_top + n->padding_bottom;
    w = 220 - pad_w;
    h = 56 - pad_h;
    if (w < 0) w = 0;
    if (h < 0) h = 0;
  }
  break;
}
