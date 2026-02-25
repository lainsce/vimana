// Helper function to get tertiary colors for vibrant menus from Ensor scheme
static void cogito_menu_theme_tertiary(CogitoColor* out_color, CogitoColor* out_container, CogitoColor* on_container, CogitoColor* on_color) {
  cogito_theme_get_tertiary(out_color, on_color, out_container, on_container);
}
// Helper to count sections and find which section an item belongs to
static int cogito_menu_count_sections(CogitoNode* owner, int* section_of_item) {
  if (!owner || owner->menu_len == 0) return 0;
  bool use_section = owner->menu_section_after != NULL;
  if (!use_section) {
    for (size_t i = 0; i < owner->menu_len; i++) section_of_item[i] = 0;
    return 1; // Single section
  }
  int section_count = 1;
  section_of_item[0] = 0;
  for (size_t i = 1; i < owner->menu_len; i++) {
    if (owner->menu_section_after[i - 1]) section_count++;
    section_of_item[i] = section_count - 1;
  }
  return section_count;
}
// Helper to find first and last item indices for each section
static void cogito_menu_section_bounds(CogitoNode* owner, int section_count, int* first_item, int* last_item) {
  if (!owner || owner->menu_len == 0) return;
  bool use_section = owner->menu_section_after != NULL;
  for (int s = 0; s < section_count; s++) {
    first_item[s] = -1;
    last_item[s] = -1;
  }
  int current_section = 0;
  for (size_t i = 0; i < owner->menu_len; i++) {
    if (first_item[current_section] < 0) first_item[current_section] = (int)i;
    last_item[current_section] = (int)i;
    if (use_section && owner->menu_section_after[i] && i < owner->menu_len - 1) {
      current_section++;
    }
  }
}

static void cogito_draw_menu(void) {
  if (!cogito_menu.open || !cogito_menu.owner) return;

  bool vibrant = cogito_menu.owner->button.menu_vibrant;

  // Tertiary colors for toggled items (and vibrant mode)
  CogitoColor tertiary = {0}, tertiary_container = {0}, on_tertiary_container = {0}, on_tertiary = {0};
  cogito_menu_theme_tertiary(&tertiary, &tertiary_container, &on_tertiary_container, &on_tertiary);

  // Derive surface/on-surface/outline from theme accent via HCT
  double h_acc = 0.0, c_acc = 0.0, t_acc = 0.0;
  cogito_rgb_to_hct(cogito_theme_accent, &h_acc, &c_acc, &t_acc);
  double surf_chroma = c_acc > 4.0 ? 4.0 : c_acc;
  CogitoColor md3_surface   = cogito_hct_to_rgb(h_acc, surf_chroma, cogito_theme_dark ? 22.0 : 94.0);
  CogitoColor md3_on_surf   = cogito_hct_to_rgb(h_acc, surf_chroma, cogito_theme_dark ? 90.0 : 10.0);
  CogitoColor md3_outline   = cogito_hct_to_rgb(h_acc, 6.0, cogito_theme_dark ? 60.0 : 50.0);

  CogitoColor bg         = cogito_theme.menu.has_bg        ? cogito_theme.menu.bg        : md3_surface;
  CogitoColor border     = cogito_theme.menu.has_border    ? cogito_theme.menu.border    : md3_outline;
  CogitoColor text_color = cogito_theme.menu.has_text      ? cogito_theme.menu.text      : md3_on_surf;

  // Hover state layer: On Surface at 8% opacity
  CogitoColor hover_layer = md3_on_surf;
  hover_layer.a = 20; // ~8% of 255

  // Override with tertiary colors in vibrant mode
  if (vibrant) {
    bg         = tertiary_container;
    text_color = on_tertiary_container;
  }

  bool has_border = cogito_theme.menu.has_border || cogito_theme.menu.has_border_width;
  int border_w = cogito_theme.menu.has_border_width ? cogito_theme.menu.border_width : 0;
  int shadow_level = cogito_theme.menu.has_shadow ? cogito_theme.menu.shadow_level : 1;
  int font_size = cogito_font_size();
  if (cogito_theme.menu.has_font_size && cogito_theme.menu.font_size > 0) {
    font_size = cogito_theme.menu.font_size;
  }
  int x = cogito_menu.x;
  int y = cogito_menu.y;
  int w = cogito_menu.w;
  int h = cogito_menu.h;

  // Layout constants from design spec
  int icon_size = 20;       // Leading icon size
  int icon_text_gap = 8;    // Gap between icon and text
  int trailing_pad = 12;    // Right padding for trailing content

  // Detect if any item has a leading icon (to reserve column space)
  bool has_any_icon = false;
  for (size_t i = 0; i < cogito_menu.owner->menu_len; i++) {
    if (cogito_menu.owner->menu_icons && cogito_menu.owner->menu_icons[i]) has_any_icon = true;
  }

  // Text X offset: pad_x + optional icon column
  int text_x_offset = cogito_menu.pad_x;
  if (has_any_icon) {
    text_x_offset = cogito_menu.pad_x + icon_size + icon_text_gap;
  }

  // Sections use gap by default; dividers are opt-in
  bool use_section = cogito_menu.owner->menu_section_after != NULL;
  bool use_divider = cogito_menu.owner->button.menu_divider;
  int item_gap = (cogito_menu.owner->button.menu_item_gap > 0) ? cogito_menu.owner->button.menu_item_gap :
                 (cogito_theme.menu.has_item_gap ? cogito_theme.menu.item_gap : 2);
  int section_gap = 2;
  int divider_h = use_divider ? 1 : 0;

  // Count sections and determine section membership
  int max_items = (int)cogito_menu.owner->menu_len;
  int* section_of_item = (int*)malloc(max_items * sizeof(int));
  int* first_item_of_section = (int*)malloc((max_items + 1) * sizeof(int));
  int* last_item_of_section = (int*)malloc((max_items + 1) * sizeof(int));
  int section_count = cogito_menu_count_sections(cogito_menu.owner, section_of_item);
  cogito_menu_section_bounds(cogito_menu.owner, section_count, first_item_of_section, last_item_of_section);

  // Calculate section Y positions and heights
  int section_padding = 2;
  int* section_y = (int*)malloc((section_count + 1) * sizeof(int));
  int* section_h_arr = (int*)malloc((section_count + 1) * sizeof(int));
  int current_y_calc = y + cogito_menu.pad_top;
  for (int s = 0; s < section_count; s++) {
    section_y[s] = current_y_calc;
    int items_in_section = last_item_of_section[s] - first_item_of_section[s] + 1;
    int section_height = items_in_section * cogito_menu.item_h;
    if (items_in_section > 1) {
      section_height += (items_in_section - 1) * item_gap;
    }
    section_height += section_padding * 2;
    if (section_height < 48) section_height = 48;
    section_h_arr[s] = section_height;
    current_y_calc += section_height;
    if (s < section_count - 1) {
      if (use_divider) {
        current_y_calc += divider_h + item_gap;
      } else {
        current_y_calc += section_gap;
      }
    }
  }

  int inner_radius = 4;
  int outer_radius = 16;
  int item_sel_radius = 12; // Radius for hover/toggled corners at menu edges

  // Draw single shadow for entire menu
  if (shadow_level > 0 && section_count > 0) {
    cogito_draw_shadow_radii_level(x, y + cogito_menu.pad_top, w, h - cogito_menu.pad_top - cogito_menu.pad_bottom, outer_radius, outer_radius, outer_radius, outer_radius, shadow_level);
  }
  // Draw section backgrounds
  for (int s = 0; s < section_count; s++) {
    int tl = inner_radius, tr = inner_radius, br = inner_radius, bl = inner_radius;
    if (section_count == 1) {
      tl = tr = br = bl = outer_radius;
    } else if (s == 0) {
      tl = tr = outer_radius;
    } else if (s == section_count - 1) {
      br = bl = outer_radius;
    }
    cogito_draw_rect_radii_aa(x, section_y[s], w, section_h_arr[s], bg, tl, tr, br, bl);
  }
  // Draw section borders if needed
  if (has_border && border_w > 0) {
    CogitoColor border_color = vibrant ? tertiary : border;
    for (int s = 0; s < section_count; s++) {
      int tl = inner_radius, tr = inner_radius, br = inner_radius, bl = inner_radius;
      if (section_count == 1) {
        tl = tr = br = bl = outer_radius;
      } else if (s == 0) {
        tl = tr = outer_radius;
      } else if (s == section_count - 1) {
        br = bl = outer_radius;
      }
      cogito_draw_rect_lines_radii(x, section_y[s], w, section_h_arr[s], border_color, tl, tr, br, bl, border_w);
    }
  }

  if (cogito_menu.owner->kind == COGITO_DROPDOWN && w > 2) {
    int seam_y = -1;
    if (y == cogito_menu.owner->y + cogito_menu.owner->h) seam_y = y;
    if (y + h == cogito_menu.owner->y) seam_y = y + h - 1;
    if (seam_y >= y && seam_y < y + h) {
      CogitoColor seam_color = vibrant ? tertiary : border;
      cogito_draw_line(x + 1, seam_y, x + w - 2, seam_y, seam_color);
    }
  }
  int mx = 0, my = 0;
  (void)cogito_pointer_query(&mx, &my, NULL, NULL);
  int hover = cogito_menu_hit(&cogito_menu, mx, my);
  if (hover < 0 && cogito_menu_kbd_index >= 0 && cogito_menu.owner &&
      (size_t)cogito_menu_kbd_index < cogito_menu.owner->menu_len) {
    hover = cogito_menu_kbd_index;
  }

  // Insets for hover/toggled highlight
  int sel_inset_edge = 4;
  int sel_inset_between = 2;
  int sel_inset_section_edge = 2;

  // Calculate item Y positions
  int* item_y = (int*)malloc(max_items * sizeof(int));
  for (int s = 0; s < section_count; s++) {
    int item_idx = first_item_of_section[s];
    int item_y_pos = section_y[s] + section_padding;
    int items_count = last_item_of_section[s] - first_item_of_section[s] + 1;
    for (int j = 0; j < items_count; j++) {
      item_y[item_idx + j] = item_y_pos;
      item_y_pos += cogito_menu.item_h + item_gap;
    }
  }

  for (size_t i = 0; i < cogito_menu.owner->menu_len; i++) {
    int current_item_y = item_y[i];
    int section_idx = section_of_item[i];
    bool is_first_in_section = (i == (size_t)first_item_of_section[section_idx]);
    bool is_last_in_section = (i == (size_t)last_item_of_section[section_idx]);
    bool is_first_overall = (section_idx == 0 && is_first_in_section);
    bool is_last_overall = (section_idx == section_count - 1 && is_last_in_section);

    bool is_toggled = cogito_menu.owner->menu_toggled && cogito_menu.owner->menu_toggled[i];
    bool is_hovered = ((int)i == hover);

    // Draw highlight background for toggled or hovered items
    if (is_toggled || is_hovered) {
      int hl_x = x + sel_inset_edge;
      int hl_w = w - sel_inset_edge * 2;
      int hl_y = current_item_y;
      int hl_h = cogito_menu.item_h;

      // Apply insets based on position within section
      if (is_first_in_section) {
        hl_y += sel_inset_section_edge;
        hl_h -= sel_inset_section_edge;
      } else {
        hl_y += sel_inset_between;
        hl_h -= sel_inset_between;
      }
      if (is_last_in_section) {
        hl_h -= sel_inset_section_edge;
      } else {
        hl_h -= sel_inset_between;
      }

      int hl_tl, hl_tr, hl_br, hl_bl;
      CogitoColor hl_color;

      if (is_toggled) {
        // Toggled: Tertiary Container, uniform 12dp radius
        hl_color = tertiary_container;
        hl_tl = hl_tr = hl_br = hl_bl = item_sel_radius;
      } else {
        // Hovered: On Surface at 8%, position-aware radius
        hl_color = hover_layer;
        hl_tl = hl_tr = hl_br = hl_bl = inner_radius;
        // Corners meeting menu outer radius get item_sel_radius
        if (is_first_overall) {
          hl_tl = hl_tr = item_sel_radius;
        }
        if (is_last_overall) {
          hl_br = hl_bl = item_sel_radius;
        }
      }

      cogito_draw_rect_radii_aa(hl_x, hl_y, hl_w, hl_h, hl_color, hl_tl, hl_tr, hl_br, hl_bl);
    }

    // Determine text color for this item
    CogitoColor item_text_color;
    if (is_toggled) {
      item_text_color = on_tertiary_container;
    } else {
      // Hovered or normal: On Surface
      item_text_color = text_color;
    }

    const char* text = cogito_menu.owner->menu_labels[i] ? cogito_menu.owner->menu_labels[i]->data : "";
    int th = cogito_text_height_size(font_size);
    int ty = current_item_y + (cogito_menu.item_h - th) / 2;

    // Draw leading icon if present
    if (has_any_icon) {
      ErgoStr* icon_str = cogito_menu.owner->menu_icons ? cogito_menu.owner->menu_icons[i] : NULL;
      if (icon_str) {
        int icon_y = current_item_y + (cogito_menu.item_h - icon_size) / 2;
        cogito_draw_icon_fallback(cogito_menu.owner, icon_str->data, x + cogito_menu.pad_x, icon_y, icon_size, item_text_color);
      }
    }

    // Draw label text
    cogito_draw_text_size_node(cogito_menu.owner, x + text_x_offset, ty, text, item_text_color, font_size, false);

    // Draw trailing content: shortcut text or submenu arrow
    bool is_submenu = cogito_menu.owner->menu_submenu && cogito_menu.owner->menu_submenu[i];
    ErgoStr* shortcut = cogito_menu.owner->menu_shortcuts ? cogito_menu.owner->menu_shortcuts[i] : NULL;
    if (is_submenu) {
      int arrow_size = 16;
      int arrow_x = x + w - trailing_pad - arrow_size;
      int arrow_y = current_item_y + (cogito_menu.item_h - arrow_size) / 2;
      cogito_draw_icon_fallback(cogito_menu.owner, "chevron.right", arrow_x, arrow_y, arrow_size, item_text_color);
    } else if (shortcut) {
      int sc_w = cogito_text_width_size(shortcut->data, font_size);
      int sc_x = x + w - trailing_pad - sc_w;
      cogito_draw_text_size_node(cogito_menu.owner, sc_x, ty, shortcut->data, item_text_color, font_size, false);
    }

    // Draw divider if this item has a section break after it
    bool has_section_after = use_section && cogito_menu.owner->menu_section_after[i];
    if (use_divider && has_section_after && i < cogito_menu.owner->menu_len - 1) {
      int divider_y = current_item_y + cogito_menu.item_h + item_gap;
      CogitoColor divider_color = vibrant ? tertiary : (cogito_theme.menu.has_border ? cogito_theme.menu.border : md3_outline);
      int divider_inset = 12;
      cogito_draw_line(x + divider_inset, divider_y, x + w - divider_inset, divider_y, divider_color);
    }
  }

  free(item_y);
  free(section_of_item);
  free(first_item_of_section);
  free(last_item_of_section);
  free(section_y);
  free(section_h_arr);
}
