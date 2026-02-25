case COGITO_BUTTON: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      
      // Check if this button is in a ButtonGroup
      bool in_buttongroup = n->parent && n->parent->kind == COGITO_BUTTON_GROUP;
      bool is_selected = in_buttongroup && cogito_buttongroup_child_is_selected(n);
      
      // Check for custom class
      bool has_custom_class = n->class_name && n->class_name->data && n->class_name->data[0];
      
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (n->parent && n->parent->kind == COGITO_APPBAR) {
        // Use cached pointer position instead of querying backend
        int mx = 0;
        int my = 0;
        bool down_state = false;
        bool have_mouse = cogito_pointer_query(&mx, &my, &down_state, NULL);
        bool over = have_mouse && cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
        bool down = over && down_state;
        if (down) {
          cogito_style_merge_into(&s, &cogito_theme.appbar_child_button_active, &s);
        } else if (over) {
          cogito_style_merge_into(&s, &cogito_theme.appbar_child_button_hover, &s);
        }
      }
      
      CogitoColor bg = n->bg;
      CogitoColor border = n->border_color;
      CogitoColor text = n->text_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      
      // Only apply circular override if NOT in a ButtonGroup (let buttongroup handle radius)
      if (!in_buttongroup) {
        // When pressed/active, use size-based pressed radius
        if (active) {
          radius = cogito_button_pressed_radius(n->button.size);
        } else {
          int circular = n->h / 2;
          if (circular > radius) radius = circular;
        }
      }
      
      if (s.has_text) text = s.text;
      else if (cogito_has_selection_text) text = cogito_selection_text;

      // ButtonGroup color handling: only apply if no explicit class is set
      // Check if button has an explicit class (filled, tonal, outlined, text)
      bool has_button_class = has_custom_class && (
        (n->class_name && (
          strstr(n->class_name->data, "filled") != NULL ||
          strstr(n->class_name->data, "tonal") != NULL ||
          strstr(n->class_name->data, "outlined") != NULL ||
          strstr(n->class_name->data, "text") != NULL
        ))
      );
      
      if (in_buttongroup && !has_button_class) {
        // ButtonGroup uses Secondary/On Secondary for selected, Secondary Container/On Secondary Container for no-state
        // Get the buttongroup node and its style
        CogitoNode* bg_node = n->parent;
        
        if (is_selected) {
          // Selected/Active: use Secondary color (On Secondary for text)
          // Check buttongroup's selection color first (from buttongroup:selection in SUM)
          if (bg_node->selection_set) {
            bg = bg_node->selection_color;
            text = cogito_on_color(bg);
          } else {
            // Use the button's checked style which should have secondary colors
            CogitoStyle checked_s = cogito_theme.per_kind_checked[COGITO_BUTTON];
            if (checked_s.has_bg) {
              bg = checked_s.bg;
              text = checked_s.has_text ? checked_s.text : cogito_on_color(bg);
            } else {
              // Fallback: try to use per_kind style
              CogitoStyle button_s = cogito_theme.per_kind[COGITO_BUTTON];
              bg = button_s.has_bg ? button_s.bg : cogito_rgba(0, 0, 0, 0);
              text = button_s.has_text ? button_s.text : bg_node->text_color;
            }
          }
        } else {
          // No-state: use Secondary Container color
          // Check buttongroup's bg color (from buttongroup selector in SUM)
          if (bg_node->bg_set) {
            bg = bg_node->bg;
            text = bg_node->text_color_set ? bg_node->text_color : cogito_on_color(bg);
          } else {
            // Fallback: use the button's base style (which is typically primary for buttons)
            // For secondary container, we need a lighter color
            // Use the button's hover style which often has container-like colors
            CogitoStyle hover_s = cogito_theme.per_kind_hover[COGITO_BUTTON];
            if (hover_s.has_bg) {
              bg = hover_s.bg;
              text = hover_s.has_text ? hover_s.text : bg_node->text_color;
            } else {
              // Use transparent (secondary container is typically surface-container-high which is light)
              bg = cogito_rgba(0, 0, 0, 0);
              text = bg_node->text_color;
            }
          }
        }
      }

      // Animate colors
      cogito_anim_apply(n, &s, &bg, &text, &border, NULL);
      
      // Get corner radii for ButtonGroup if applicable
      // Also apply buttongroup size/shape to radius if in buttongroup
      int rtl = radius, rtr = radius, rbr = radius, rbl = radius;
      if (in_buttongroup) {
        // Use buttongroup size/shape for radius if no explicit radius set
        if (!n->radius_set && !s.has_radius) {
          CogitoNode* buttongroup_node = n->parent;
          bool is_rounded = (buttongroup_node->buttongroup.shape == 0); // 0 = rounded (circle), 1 = pill (square)
          if (is_rounded) {
            // Rounded = circle shape
            radius = n->h / 2;
          } else {
            // Pill = square shape, use size-based radii
            int bg_size = (buttongroup_node->buttongroup.size >= 0 && buttongroup_node->buttongroup.size <= 4) ? buttongroup_node->buttongroup.size : 2;
            int buttongroup_radii[] = {4, 8, 8, 16, 20};
            radius = buttongroup_radii[bg_size];
          }
          // Apply radius to node so other code can use it
          n->border_radius = radius;
        }
        cogito_buttongroup_child_radii(n, &rtl, &rtr, &rbr, &rbl, radius);
      } else {
        // Plain buttons outside ButtonGroup: respect icon_shape option
        if (!n->radius_set && !s.has_radius) {
          if (n->button.icon_shape == 1) {
            // Squared button: use 16px radius
            radius = 16;
          } else {
            // Circle shape: use half height
            radius = n->h / 2;
          }
          // Apply radius to node so other code can use it
          n->border_radius = radius;
        }
      }
      
      if (n->bg_set || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
        }
        
        // Draw with individual corner radii if in ButtonGroup, otherwise use single radius
        if (in_buttongroup) {
          cogito_draw_rect_radii_aa(n->x, n->y, n->w, n->h, bg, rtl, rtr, rbr, rbl);
        } else {
          cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        }
        
        // State layer: hover=8%, active/pressed=12%.
        if (active || hover) {
          CogitoColor state = cogito_node_state_layer_seed(n);
          state.a = active ? 31 : 20;
          if (in_buttongroup) {
            cogito_draw_rect_radii_aa(n->x, n->y, n->w, n->h, state, rtl, rtr, rbr, rbl);
          } else {
            cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, state, radius);
          }
        }
        
        // Draw ripple effect for Primary-colored buttons (clipped to widget bounds)
        if (cogito_ripple_update(n)) {
          float r_radius, r_alpha;
          cogito_ripple_get(n, &r_radius, &r_alpha);
          if (r_radius > 0 && r_alpha > 0) {
            CogitoColor ripple_color = text;
            ripple_color.a = (uint8_t)(255 * r_alpha);
            int cx = n->x + n->ripple_x;
            int cy = n->y + n->ripple_y;
            if (in_buttongroup) {
              cogito_draw_circle_clipped_radii(cx, cy, r_radius, n->x, n->y, n->w, n->h, rtl, rtr, rbr, rbl, ripple_color);
            } else {
              cogito_draw_circle_clipped_node(n, &s, radius, cx, cy, r_radius, ripple_color);
            }
          }
        }
        
        if (border_w > 0) {
          if (in_buttongroup) {
            cogito_draw_rect_lines_radii_aa(n->x, n->y, n->w, n->h, border, rtl, rtr, rbr, rbl, border_w);
          } else {
            cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
          }
        }
      }
      
      if (n->text) {
        int size = cogito_node_font_size(n);
        int tw = cogito_text_width_size_node(n, n->text->data, size);
        int th = cogito_text_height_size(size);
        int tx = n->x + (n->w - tw) / 2;
        int ty = n->y + (n->h - th) / 2;
        cogito_draw_text_size_node(n, tx, ty, n->text->data, text, size, cogito_node_bold(n));
      }
      break;
    }
