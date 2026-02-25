case COGITO_ICONBTN: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      
      // Check if this icon button is in a ButtonGroup
      bool in_buttongroup = n->parent && n->parent->kind == COGITO_BUTTON_GROUP;
      bool is_selected = in_buttongroup && cogito_buttongroup_child_is_selected(n);
      
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      bool has_custom_class = n->class_name && n->class_name->data && n->class_name->data[0];
      bool iconic_class = n->class_name && n->class_name->data &&
                          strstr(n->class_name->data, "iconic") != NULL;
      bool is_iconic_variant = (n->button.icon_color_style == 3) || iconic_class;
      bool in_appbar = n->parent && n->parent->kind == COGITO_APPBAR;
      bool in_appbar_or_toolbar = n->parent && (n->parent->kind == COGITO_APPBAR || n->parent->kind == COGITO_TOOLBAR);
      int mx = 0;
      int my = 0;
      bool down_state = false;
      bool have_mouse = false;
      bool over = false;
      if (in_appbar_or_toolbar) {
        have_mouse = cogito_pointer_query(&mx, &my, &down_state, NULL);
        over = have_mouse && cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
      }
      
      if (in_appbar_or_toolbar && !is_iconic_variant) {
        bool down = over && down_state;
        if (down) {
          cogito_style_merge_into(&s, &cogito_theme.appbar_child_iconbtn_active, &s);
        } else if (over) {
          cogito_style_merge_into(&s, &cogito_theme.appbar_child_iconbtn_hover, &s);
        }
      }
      
      CogitoColor bg = n->bg;
      CogitoColor border = n->border_color;
      CogitoColor icon_color = n->text_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (in_appbar_or_toolbar && !is_iconic_variant) {
        bool down = over && down_state;
        if (over || down) {
          CogitoStyle overlay = down ? cogito_theme.appbar_child_iconbtn_active : cogito_theme.appbar_child_iconbtn_hover;
          if (overlay.has_bg) {
            bg = cogito_color_blend(bg, overlay.bg);
          }
        }
      }
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      // Effective shape:
      // - Outside ButtonGroup: use the IconButton shape.
      // - Inside ButtonGroup: use the group shape as baseline.
      // - If toggle is active and checked: invert (circle <-> square).
      int eff_shape = n->button.icon_shape;
      if (in_buttongroup && n->parent && n->parent->kind == COGITO_BUTTON_GROUP) {
        eff_shape = (n->parent->buttongroup.shape == 0) ? 0 : 1;
      }
      if (n->button.icon_toggle && n->checked) eff_shape = eff_shape ? 0 : 1;

      // Only apply default radius logic if NOT in a ButtonGroup (let buttongroup handle radius)
      if (!in_buttongroup) {
        if (in_appbar && !n->radius_set) {
          // AppBar IconBtn default shape still follows effective toggle-aware shape.
          if (eff_shape == 1) {
            if (n->button.size == COGITO_BUTTON_SIZE_XS) {
              radius = 12;
            } else if (n->button.size == COGITO_BUTTON_SIZE_S) {
              radius = 12;
            } else if (n->button.size == COGITO_BUTTON_SIZE_M) {
              radius = 16;
            } else if (n->button.size == COGITO_BUTTON_SIZE_L) {
              radius = 28;
            } else if (n->button.size == COGITO_BUTTON_SIZE_XL) {
              radius = 28;
            } else {
              radius = 16;
            }
          } else {
            int min_dim = n->w < n->h ? n->w : n->h;
            radius = min_dim / 2; // fully rounded = circle
          }
        } else if (s.has_radius) {
          radius = s.radius;
        } else {
          int min_dim = n->w < n->h ? n->w : n->h;
          if (eff_shape == 1) {
            if (n->button.size == COGITO_BUTTON_SIZE_XS) {
              radius = 12;
            } else if (n->button.size == COGITO_BUTTON_SIZE_S) {
              radius = 12;
            } else if (n->button.size == COGITO_BUTTON_SIZE_M) {
              radius = 16;
            } else if (n->button.size == COGITO_BUTTON_SIZE_L) {
              radius = 28;
            } else if (n->button.size == COGITO_BUTTON_SIZE_XL) {
              radius = 28;
            } else {
              radius = 16;
            }
          } else {
            radius = min_dim / 2; // fully rounded = circle
          }
        }
      }
      if (s.has_text) icon_color = s.text;
      else if (cogito_has_selection_text) icon_color = cogito_selection_text;

      // AppBar idle state: keep icons on primary for stronger affordance.
      if (in_appbar) {
        bool down = over && down_state;
        if (!hover && !active && !over && !down) {
          icon_color = cogito_theme.iconbtn_filled.bg;
        }
      }

      // ButtonGroup color handling: only apply if no explicit class is set
      // Check if icon button has an explicit class (filled, tonal, outlined, iconic)
      bool has_iconbtn_class = has_custom_class && (
        (n->class_name && (
          strstr(n->class_name->data, "filled") != NULL ||
          strstr(n->class_name->data, "tonal") != NULL ||
          strstr(n->class_name->data, "outlined") != NULL ||
          strstr(n->class_name->data, "iconic") != NULL
        ))
      );
      
      if (in_buttongroup && !has_iconbtn_class) {
        // ButtonGroup uses Secondary/On Secondary for selected, Secondary Container/On Secondary Container for no-state
        CogitoNode* bg_node = n->parent;
        
        if (is_selected) {
          // Selected/Active: use Secondary color (On Secondary for icon)
          // Check buttongroup's selection color first (from buttongroup:selection in SUM)
          if (bg_node->selection_set) {
            bg = bg_node->selection_color;
            icon_color = cogito_on_color(bg);
          } else {
            // Use the iconbtn's checked style which should have secondary colors
            CogitoStyle checked_s = cogito_theme.per_kind_checked[COGITO_ICONBTN];
            if (checked_s.has_bg) {
              bg = checked_s.bg;
              icon_color = checked_s.has_text ? checked_s.text : cogito_on_color(bg);
            } else {
              // Fallback: try to use per_kind style
              CogitoStyle iconbtn_s = cogito_theme.per_kind[COGITO_ICONBTN];
              bg = iconbtn_s.has_bg ? iconbtn_s.bg : cogito_rgba(0, 0, 0, 0);
              icon_color = iconbtn_s.has_text ? iconbtn_s.text : bg_node->text_color;
            }
          }
        } else {
          // No-state: use Secondary Container color
          // Check buttongroup's bg color (from buttongroup selector in SUM)
          if (bg_node->bg_set) {
            bg = bg_node->bg;
            icon_color = bg_node->text_color_set ? bg_node->text_color : cogito_on_color(bg);
          } else {
            // Fallback: use the iconbtn's base style
            // For secondary container, we need a lighter color
            // Use the iconbtn's hover style which often has container-like colors
            CogitoStyle hover_s = cogito_theme.per_kind_hover[COGITO_ICONBTN];
            if (hover_s.has_bg) {
              bg = hover_s.bg;
              icon_color = hover_s.has_text ? hover_s.text : bg_node->text_color;
            } else {
              // Use transparent (secondary container is typically surface-container-high which is light)
              bg = cogito_rgba(0, 0, 0, 0);
              icon_color = bg_node->text_color;
            }
          }
        }
      }

      cogito_anim_apply(n, &s, &bg, &icon_color, &border, NULL);
      
      // Get corner radii for ButtonGroup if applicable
      // Also apply buttongroup size/shape to radius if in buttongroup
      int rtl = radius, rtr = radius, rbr = radius, rbl = radius;
      if (in_buttongroup) {
        // Use buttongroup size/shape for radius if no explicit radius set
        if (!n->radius_set && !s.has_radius) {
          CogitoNode* buttongroup_node = n->parent;
          if (eff_shape == 0) {
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
        // Plain IconButtons outside ButtonGroup: respect effective shape (toggle-aware)
        // When pressed/active, use size-based pressed radius
        if (active) {
          radius = cogito_button_pressed_radius(n->button.size);
        } else if (!n->radius_set && !s.has_radius) {
          if (eff_shape == 1) {
            // Squared IconButton: use size-based radii (12, 12, 16, 28, 28)
            if (n->button.size == COGITO_BUTTON_SIZE_XS) {
              radius = 12;
            } else if (n->button.size == COGITO_BUTTON_SIZE_S) {
              radius = 12;
            } else if (n->button.size == COGITO_BUTTON_SIZE_M) {
              radius = 16;
            } else if (n->button.size == COGITO_BUTTON_SIZE_L) {
              radius = 28;
            } else if (n->button.size == COGITO_BUTTON_SIZE_XL) {
              radius = 28;
            } else {
              radius = 16;
            }
          } else {
            // Circle shape: use half of min dimension (fully rounded)
            int min_dim = n->w < n->h ? n->w : n->h;
            radius = min_dim / 2;
          }
          // Apply radius to node so other code can use it
          n->border_radius = radius;
        }
      }
      
      bool iconbtn_has_bg = n->bg_set || s.has_bg || (bg.a > 0);
      if (iconbtn_has_bg || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
        }
        
        // Draw with individual corner radii if in ButtonGroup, otherwise use single radius
        if (in_buttongroup) {
          cogito_draw_rect_radii_aa(n->x, n->y, n->w, n->h, bg, rtl, rtr, rbr, rbl);
        } else {
          cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        }
        
        // Draw ripple effect when iconbtn is active (showing Primary color; clipped to widget bounds)
        if (cogito_ripple_update(n)) {
          float r_radius, r_alpha;
          cogito_ripple_get(n, &r_radius, &r_alpha);
          if (r_radius > 0 && r_alpha > 0) {
            CogitoColor ripple_color = icon_color;
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
        
        if (border_w > 0 || n->button.outlined) {
          int draw_border_w = border_w;
          if (n->button.outlined && draw_border_w == 0) {
            draw_border_w = 1; // Default border width for outlined icon buttons
          }
          if (in_buttongroup) {
            cogito_draw_rect_lines_radii_aa(n->x, n->y, n->w, n->h, border, rtl, rtr, rbr, rbl, draw_border_w);
          } else {
            cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, draw_border_w);
          }
        }
      }

      if (active || hover) {
        CogitoColor state = cogito_node_state_layer_seed(n);
        state.a = active ? 31 : 20;
        if (in_buttongroup) {
          cogito_draw_rect_radii_aa(n->x, n->y, n->w, n->h, state, rtl, rtr, rbr, rbl);
        } else {
          cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, state, radius);
        }
      }
      
      if (n->text) {
        int btn_min = n->w < n->h ? n->w : n->h;
        int default_icon = 24;
        cogito_button_size_metrics((int)n->button.size, n->kind, NULL, NULL, &default_icon, NULL);
        // Icon sizes per size tier (from spec): XS=20, S=24, M=24, L=32, XL=40
        // cogito_button_size_metrics already returns these correctly via out_icon
        int target = s.has_icon_size ? s.icon_size : default_icon;
        if (target < 8) target = 8;
        if (btn_min > 0 && target > btn_min) target = btn_min;
        int ix = n->x + (n->w - target) / 2;
        int iy = n->y + (n->h - target) / 2;
        cogito_draw_icon_fallback(n, n->text->data, ix, iy, target, icon_color);
      }
      break;
    }
