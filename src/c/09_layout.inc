#include "widgets/util/draw_queue.inc"

static void cogito_layout_node(CogitoNode* n, int x, int y, int w, int h);
static void cogito_window_relayout_now(CogitoNode* win);
static void cogito_window_relayout(CogitoNode* win);
static void cogito_relayout_subtree(CogitoNode* n);
static void cogito_dialog_close_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h);

static void cogito_distribute_axis_sizes(size_t count, const int* min_sizes, const bool* expand_flags, int available, int* out_sizes) {
  if (!out_sizes || !min_sizes || count == 0) return;
  if (available < 0) available = 0;

  int sum = 0;
  for (size_t i = 0; i < count; i++) {
    int v = min_sizes[i];
    if (v < 0) v = 0;
    out_sizes[i] = v;
    sum += v;
  }

  if (sum == available) return;

  if (sum < available) {
    int extra = available - sum;
    size_t expand_count = 0;
    for (size_t i = 0; i < count; i++) {
      if (expand_flags && expand_flags[i]) expand_count++;
    }
    if (expand_count == 0) return;
    // Distribute extra space evenly via division (O(n) instead of O(n*pixels))
    int per = extra / (int)expand_count;
    int rem = extra % (int)expand_count;
    int r = 0;
    for (size_t i = 0; i < count; i++) {
      if (expand_flags && expand_flags[i]) {
        out_sizes[i] += per + (r < rem ? 1 : 0);
        r++;
      }
    }
    return;
  }

  int overflow = sum - available;
  bool has_expand = false;
  for (size_t i = 0; i < count; i++) {
    if (expand_flags && expand_flags[i]) {
      has_expand = true;
      break;
    }
  }
  // Shrink expandable items first, then all items, using division (O(n) per pass)
  if (has_expand) {
    while (overflow > 0) {
      size_t sc = 0;
      for (size_t i = 0; i < count; i++) {
        if (expand_flags && expand_flags[i] && out_sizes[i] > 0) sc++;
      }
      if (sc == 0) break;
      int per = overflow / (int)sc;
      if (per < 1) per = 1;
      int rem = (per > 1) ? overflow % (int)sc : 0;
      int r = 0, removed = 0;
      for (size_t i = 0; i < count && removed < overflow; i++) {
        if (expand_flags && expand_flags[i] && out_sizes[i] > 0) {
          int take = per + (r < rem ? 1 : 0);
          r++;
          if (take > out_sizes[i]) take = out_sizes[i];
          out_sizes[i] -= take;
          removed += take;
        }
      }
      overflow -= removed;
      if (removed == 0) break;
    }
  }
  while (overflow > 0) {
    size_t sc = 0;
    for (size_t i = 0; i < count; i++) {
      if (out_sizes[i] > 0) sc++;
    }
    if (sc == 0) break;
    int per = overflow / (int)sc;
    if (per < 1) per = 1;
    int rem = (per > 1) ? overflow % (int)sc : 0;
    int r = 0, removed = 0;
    for (size_t i = 0; i < count && removed < overflow; i++) {
      if (out_sizes[i] > 0) {
        int take = per + (r < rem ? 1 : 0);
        r++;
        if (take > out_sizes[i]) take = out_sizes[i];
        out_sizes[i] -= take;
        removed += take;
      }
    }
    overflow -= removed;
    if (removed == 0) break;
  }
}

#include "widgets/layout/treeview.inc"
#include "widgets/layout/vstack.inc"
#include "widgets/layout/zstack.inc"
#include "widgets/layout/view_switcher.inc"
#include "widgets/layout/toasts.inc"
#include "widgets/layout/fixed.inc"
#include "widgets/layout/scroller.inc"
#include "widgets/layout/carousel.inc"
#include "widgets/layout/carousel_item.inc"
#include "widgets/layout/hstack.inc"
#include "widgets/layout/list.inc"
#include "widgets/layout/grid.inc"
#include "widgets/layout/window.inc"

static void cogito_layout_node(CogitoNode* n, int x, int y, int w, int h) {
  bool changed = (n->x != x || n->y != y || n->w != w || n->h != h);
  n->x = x;
  n->y = y;
  n->w = w;
  n->h = h;
  cogito_apply_size_constraints(n, &n->w, &n->h);
  if (changed || n->w != w || n->h != h) cogito_layout_mark_changed();
  switch (n->kind) {

    #include "widgets/layout/appbar.inc"

    #include "widgets/layout/dialog.inc"

    #include "widgets/layout/popover.inc"

    #include "widgets/layout/searchfield.inc"

    #include "widgets/layout/datepicker.inc"

    #include "widgets/layout/fab.inc"

    #include "widgets/layout/fab_menu.inc"

    #include "widgets/layout/stepper.inc"

    #include "widgets/layout/buttongroup.inc"

    #include "widgets/layout/colorpicker.inc"

    #include "widgets/layout/tooltip.inc"

    #include "widgets/layout/toolbar.inc"

    #include "widgets/layout/dialog_slot.inc"

    #include "widgets/layout/button.inc"

    #include "widgets/layout/iconbtn.inc"

    #include "widgets/layout/image.inc"

    #include "widgets/layout/label.inc"

    #include "widgets/layout/nav_rail.inc"

    #include "widgets/layout/bottom_nav.inc"

    #include "widgets/layout/checkbox.inc"

    #include "widgets/layout/chip.inc"

    #include "widgets/layout/switch.inc"

    #include "widgets/layout/textfield.inc"

    #include "widgets/layout/textview.inc"

    #include "widgets/layout/dropdown.inc"

    #include "widgets/layout/slider.inc"

    #include "widgets/layout/tabs.inc"

    #include "widgets/layout/progress.inc"

    #include "widgets/layout/toast.inc"

    #include "widgets/layout/divider.inc"

#include "widgets/layout/menu_section.inc"

    #include "widgets/layout/card.inc"

    #include "widgets/layout/avatar.inc"

    #include "widgets/layout/badge.inc"

    #include "widgets/layout/banner.inc"

    #include "widgets/layout/bottom_sheet.inc"

    #include "widgets/layout/side_sheet.inc"

    #include "widgets/layout/timepicker.inc"

    #include "widgets/layout/active_indicator.inc"

    #include "widgets/layout/switchbar.inc"

    #include "widgets/layout/content_list.inc"

    #include "widgets/layout/empty_page.inc"

    #include "widgets/layout/tip_view.inc"

    #include "widgets/layout/settings_window.inc"

    #include "widgets/layout/settings_page.inc"

    #include "widgets/layout/settings_list.inc"

    #include "widgets/layout/settings_row.inc"

    #include "widgets/layout/welcome_screen.inc"

    #include "widgets/layout/view_dual.inc"

    #include "widgets/layout/view_chooser.inc"

    #include "widgets/layout/about_window.inc"

    #include "widgets/layout/split_button.inc"

    #include "widgets/layout/drawing_area.inc"

    #include "widgets/layout/shape.inc"

    case COGITO_TREEVIEW:
      cogito_layout_treeview(n, x, y);
      break;

    case COGITO_VSTACK:
      cogito_layout_vstack(n, x, y);
      break;

    case COGITO_ZSTACK:
      cogito_layout_zstack(n, x, y);
      break;

    case COGITO_VIEWSWITCHER:
      cogito_layout_view_switcher(n, x, y);
      break;

    case COGITO_TOASTS:
      cogito_layout_toasts(n, x, y);
      break;

    case COGITO_FIXED:
      cogito_layout_fixed(n, x, y);
      break;

    case COGITO_SCROLLER:
      cogito_layout_scroller(n, x, y);
      break;

    case COGITO_CAROUSEL:
      cogito_layout_carousel(n, x, y);
      break;

    case COGITO_CAROUSEL_ITEM:
      cogito_layout_carousel_item(n, x, y);
      break;

    case COGITO_HSTACK:
      cogito_layout_hstack(n, x, y);
      break;

    case COGITO_LIST:
      cogito_layout_list(n, x, y);
      break;

    case COGITO_GRID:
      cogito_layout_grid(n, x, y);
      break;

    case COGITO_WINDOW:
      break;

    case COGITO_KIND_COUNT:
      break;
  }
}
