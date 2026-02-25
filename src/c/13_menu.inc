static void cogito_menu_close(void) {
  CogitoNode* owner = cogito_menu.owner;
  cogito_menu.open = false;
  cogito_menu.owner = NULL;
  cogito_menu_kbd_index = -1;
  if (owner) {
    CogitoNode* win = cogito_node_window(owner);
    if (win) cogito_overlay_focus_restore(win);
    cogito_a11y_emit_event("overlay-close", owner, "menu");
  }
}

static void cogito_menu_open_for(CogitoNode* n) {
  if (!n || n->menu_len == 0) return;
  cogito_overlay_focus_snapshot();
  int font_size = cogito_font_size();
  if (cogito_theme.menu.has_font_size && cogito_theme.menu.font_size > 0) {
    font_size = cogito_theme.menu.font_size;
  }
  CogitoStyle* item_style = &cogito_theme.menu_item;
  int pad_x = item_style->has_padding_left ? item_style->padding_left
              : (cogito_theme.menu.has_padding_left ? cogito_theme.menu.padding_left : 12);
  int pad_y = item_style->has_padding_top ? item_style->padding_top
              : (cogito_theme.menu.has_padding_top ? cogito_theme.menu.padding_top : 6);

  // Sections use gap by default; dividers are opt-in via menu_divider flag
  bool use_section = n->menu_section_after != NULL;
  bool use_divider = n->button.menu_divider;

  int item_h = font_size + pad_y * 2;
  int min_h = item_style->has_min_h ? item_style->min_h
              : (cogito_theme.menu.has_min_h ? cogito_theme.menu.min_h : 0);
  if (min_h > item_h) item_h = min_h;
  // Minimum 48px item height for pointer accessibility on all platforms.
  if (item_h < 48) item_h = 48;

  int menu_pad_top = 8;
  int menu_pad_bottom = 8;
  int item_gap = (n->button.menu_item_gap > 0) ? n->button.menu_item_gap :
                 (cogito_theme.menu.has_item_gap ? cogito_theme.menu.item_gap : 2);
  int section_gap = 2;
  int divider_h = use_divider ? 1 : 0;

  // Layout constants for icons and trailing content
  int icon_size = 20;
  int icon_text_gap = 8;
  int arrow_size = 16;

  // Detect if any item has leading icons or trailing content
  bool has_any_icon = false;
  int max_shortcut_w = 0;
  bool has_any_trailing = false;
  for (size_t i = 0; i < n->menu_len; i++) {
    if (n->menu_icons && n->menu_icons[i]) has_any_icon = true;
    if (n->menu_submenu && n->menu_submenu[i]) has_any_trailing = true;
    if (n->menu_shortcuts && n->menu_shortcuts[i]) {
      has_any_trailing = true;
      int sw = cogito_text_width_size(n->menu_shortcuts[i]->data, font_size);
      if (sw > max_shortcut_w) max_shortcut_w = sw;
    }
  }

  // Calculate width: label column + optional icon column + optional trailing column
  int max_label_w = 0;
  for (size_t i = 0; i < n->menu_len; i++) {
    const char* text = n->menu_labels[i] ? n->menu_labels[i]->data : "";
    int tw = cogito_text_width_size(text, font_size);
    if (tw > max_label_w) max_label_w = tw;
  }
  int w = pad_x + max_label_w + pad_x;
  if (has_any_icon) {
    w += icon_size + icon_text_gap; // icon column before label, replaces left pad_x
  }
  if (has_any_trailing) {
    // Gap between label and trailing content + trailing content + right padding
    int trailing_w = (max_shortcut_w > arrow_size) ? max_shortcut_w : arrow_size;
    w += icon_text_gap + trailing_w;
  }
  if (n->w > w) w = n->w;

  // Minimum menu width for accessibility
  if (w < 112) w = 112;
  
  // Calculate menu height with gaps and potential dividers
  int section_padding = 2; // 2px padding at top and bottom of each section
  int h = menu_pad_top;
  if (use_section) {
    // Calculate per-section heights
    int section_start = 0;
    for (size_t i = 0; i < n->menu_len; i++) {
      bool has_section_after = n->menu_section_after[i];
      if (has_section_after || i == n->menu_len - 1) {
        // Calculate section height
        int section_items = (int)i - section_start + 1;
        int section_h = section_items * item_h;
        // Add item gaps within section
         if (section_items > 1) {
           section_h += (section_items - 1) * item_gap;
         }
         // Add 2px padding at top and bottom of section
         section_h += section_padding * 2;
         // Minimum 48px per section for accessibility
         if (section_h < 48) section_h = 48;

         h += section_h;
        // Add section gap or divider after section (except last)
        if (has_section_after && i < n->menu_len - 1) {
          if (use_divider) {
            h += divider_h;
          } else {
            h += section_gap;
          }
        }
        section_start = (int)i + 1;
      }
    }
  } else {
    // Original calculation for non-sectioned menus
    for (size_t i = 0; i < n->menu_len; i++) {
      h += item_h;
      if (i < n->menu_len - 1) {
        h += item_gap;
      }
    }
  }
  h += menu_pad_bottom;
  
  cogito_menu.open = true;
  cogito_menu.owner = n;
  cogito_menu.item_h = item_h;
  cogito_menu.pad_x = pad_x;
  cogito_menu.pad_y = pad_y;
  cogito_menu.pad_top = menu_pad_top;
  cogito_menu.pad_bottom = menu_pad_bottom;
  cogito_menu.w = w;
  cogito_menu.h = h;
  int x = n->x;
  int y = n->y + n->h;
  
  // Get window size from backend
  int win_w = 800, win_h = 600;  // Default fallback
  if (cogito_backend && cogito_backend_ready && cogito_active_window) {
    // Use window size from active window
    win_w = cogito_active_window->w;
    win_h = cogito_active_window->h;
  }
  
  // Check if menu would go off right edge - try repositioning left
  if (x + w > win_w - 4) {
    int left_pos = n->x - w;
    if (left_pos >= 4) {
      // There's room on the left, use it
      x = left_pos;
    } else {
      // Not enough room on either side, clamp to window
      x = win_w - 4 - w;
      if (x < 4) x = 4;
    }
  }
  
  if (w > win_w - 8) w = win_w - 8;
  if (h > win_h - 8) h = win_h - 8;
  if (x + w > win_w - 4) x = win_w - 4 - w;
  if (x < 4) x = 4;
  
  // Check if menu would go off bottom - try repositioning above
  if (y + h > win_h - 4) {
    int above = n->y - h;
    if (above >= 4) {
      y = above;
    } else {
      // Not enough room above or below, clamp to window
      y = win_h - 4 - h;
      if (y < 4) y = 4;
    }
  }
  if (y < 4) y = 4;
  cogito_menu.w = w;
  cogito_menu.h = h;
  cogito_menu.x = x;
  cogito_menu.y = y;
  cogito_menu_kbd_index = (n->selected >= 0 && (size_t)n->selected < n->menu_len) ? n->selected : 0;
  cogito_a11y_emit_event("overlay-open", n, "menu");
}

static int cogito_menu_hit(const CogitoMenu* menu, int x, int y) {
  if (!menu || !menu->open) return -1;
  if (x < menu->x || y < menu->y || x > menu->x + menu->w || y > menu->y + menu->h) return -1;
  int rel_y = y - (menu->y + menu->pad_top);
  if (rel_y < 0) return -1;
  
  // Sections use gap by default; dividers are opt-in via menu_divider flag
  bool use_section = menu->owner->menu_section_after != NULL;
  bool use_divider = menu->owner->button.menu_divider;
  int item_gap = (menu->owner->button.menu_item_gap > 0) ? menu->owner->button.menu_item_gap : 
                 (cogito_theme.menu.has_item_gap ? cogito_theme.menu.item_gap : 2);
  int section_gap = 2;
  int divider_h = use_divider ? 1 : 0;
  int section_padding = 2; // 2px padding at top and bottom of each section
  
  int idx = -1;
  int current_y = use_section ? section_padding : 0;  // Start after section padding if using sections
  
  for (size_t i = 0; i < menu->owner->menu_len; i++) {
    if (rel_y >= current_y && rel_y < current_y + menu->item_h) {
      idx = (int)i;
      break;
    }
    current_y += menu->item_h + item_gap;
    // Check if this item has a section break after it
    bool has_section_after = use_section && menu->owner->menu_section_after[i];
    if (has_section_after && i < menu->owner->menu_len - 1) {
      // Add bottom padding of current section, gap/divider, and top padding of next section
      current_y += section_padding;  // Bottom padding of current section
      if (use_divider) {
        current_y += divider_h;
      } else {
        current_y += section_gap;
      }
      current_y += section_padding;  // Top padding of next section
    }
  }
  
  if (idx < 0 || (size_t)idx >= menu->owner->menu_len) {
    return -1;
  }

  return idx;
}

// FAB Menu popup geometry helpers.
// Items are M-size button pills (h=56, horizontal padding=24, gap=4) stacked
// above the FAB.  Item 0 is the bottom-most (closest to FAB).
static void cogito_fab_menu_geom(CogitoNode* n, int* out_item_w, int* out_item_x) {
  if (!n) {
    if (out_item_w) *out_item_w = 0;
    if (out_item_x) *out_item_x = 0;
    return;
  }
  int font_size = cogito_font_size();
  int pad_x = 24; // M-size button horizontal padding
  int max_text_w = 0;
  for (size_t i = 0; i < n->menu_len; i++) {
    const char* text = n->menu_labels[i] ? n->menu_labels[i]->data : "";
    int tw = cogito_text_width_size(text, font_size);
    if (tw > max_text_w) max_text_w = tw;
  }
  int item_w = max_text_w + pad_x * 2;
  if (item_w < n->w) item_w = n->w; // at least as wide as the FAB
  int item_x = n->x + n->w - item_w; // right-align with FAB right edge
  if (out_item_w) *out_item_w = item_w;
  if (out_item_x) *out_item_x = item_x;
}

// Returns the index of the FAB menu item hit at (mx, my), or -1.
// Item 0 = closest to FAB (bottom of stack).
static int cogito_fab_menu_item_hit(CogitoNode* n, int mx, int my) {
  if (!n || !n->popover_open || n->menu_len == 0) return -1;
  int item_h = 56; // M-size button height
  int item_gap = 4;
  int item_w = 0, item_x = 0;
  cogito_fab_menu_geom(n, &item_w, &item_x);
  for (int i = 0; i < (int)n->menu_len; i++) {
    int iy_top = n->y - 8 - (i + 1) * item_h - i * item_gap;
    if (cogito_hit_rect(mx, my, item_x, iy_top, item_w, item_h)) {
      return i;
    }
  }
  return -1;
}
