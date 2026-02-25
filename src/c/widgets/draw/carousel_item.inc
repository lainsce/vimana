case COGITO_CAROUSEL_ITEM: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = s.has_bg ? s.bg : n->bg;
      CogitoColor border = s.has_border ? s.border : n->border_color;
      int radius = s.has_radius ? s.radius : n->border_radius;
      
      cogito_anim_apply(n, &s, &bg, NULL, &border, NULL);
      
      // Check if this carousel item has image children
      bool has_image_child = false;
      for (size_t i = 0; i < n->len; i++) {
        if (n->children[i]->kind == COGITO_IMAGE) { has_image_child = true; break; }
      }
      
      // When we have an image child, we skip drawing our own bg rectangle.
      // The image's corner mask needs the carousel item's theme bg color
      // so the rounded corners blend correctly with the item's surface.
      if (has_image_child) {
        // Pass carousel_item's theme bg to image children for corner masking.
        // The image needs to mask with the item's actual surface color,
        // not the window background behind the transparent carousel.
        CogitoColor item_bg = s.has_bg ? s.bg : n->bg;
        for (size_t i = 0; i < n->len; i++) {
          if (n->children[i]->kind == COGITO_IMAGE) {
            n->children[i]->bg = item_bg;
            n->children[i]->bg_set = true;
          }
        }
      } else if (n->bg_set || s.has_bg) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
      }
      
      // Draw children - clipping is handled by parent carousel.
      // Draw non-label children first; the label is rendered manually below.
      CogitoNode* label_child = NULL;
      CogitoNode* image_child = NULL;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        if (c->kind == COGITO_LABEL && c->class_name &&
            c->class_name->data && strstr(c->class_name->data, "carousel-item-label")) {
          label_child = c;
        } else {
          if (c->kind == COGITO_IMAGE) image_child = c;
          cogito_draw_node(c);
        }
      }
      // Draw the carousel-item-label at bottom-left, 16px from edge.
      // Skip if the item is too narrow for the label text, or label has no text.
      // Also skip if the label would be drawn outside the carousel bounds.
      if (label_child && label_child->text && label_child->text->data && label_child->text->data[0]) {
        int lbl_pad = 16;
        int lbl_size = label_child->font_size_set ? label_child->font_size : 21;
        bool lbl_bold = label_child->font_weight_set ? (label_child->font_weight >= 600) : true;
        int tw = cogito_text_width_size_node(label_child, label_child->text->data, lbl_size);
        int th = cogito_text_height_size(lbl_size);
        int lx = n->x + lbl_pad;
        int ly = n->y + n->h - lbl_pad - th;
        // Check if label is within carousel bounds (parent bounds)
        bool label_in_bounds = true;
        if (n->parent) {
          int carousel_x = n->parent->x;
          int carousel_y = n->parent->y;
          int carousel_w = n->parent->w;
          int carousel_h = n->parent->h;
          // Label text bounds
          int label_right = lx + tw;
          int label_bottom = ly + th;
          // Check if any part of the label is outside carousel bounds
          if (lx < carousel_x || label_right > carousel_x + carousel_w ||
              ly < carousel_y || label_bottom > carousel_y + carousel_h) {
            label_in_bounds = false;
          }
        }
        if (label_in_bounds && tw + lbl_pad * 2 <= n->w) {
          // Pick white or black based on the underlying image luminance.
          CogitoColor lbl_color;
          if (image_child && image_child->image.loaded) {
            lbl_color = (image_child->image.avg_lum < 0.55f)
              ? cogito_rgba(255, 255, 255, 255)
              : cogito_rgba(0, 0, 0, 255);
          } else {
            // No image — fall back to theme text color
            lbl_color = (s.has_text) ? s.text : cogito_rgba(255, 255, 255, 255);
          }
          // Subtle shadow for legibility over varied image content
          CogitoColor shadow = (lbl_color.r > 127)
            ? cogito_rgba(0, 0, 0, 90)
            : cogito_rgba(255, 255, 255, 90);
          cogito_draw_text_size_node(label_child, lx + 1, ly + 1, label_child->text->data, shadow, lbl_size, lbl_bold);
          cogito_draw_text_size_node(label_child, lx, ly, label_child->text->data, lbl_color, lbl_size, lbl_bold);
        }
      }
      break;
    }
