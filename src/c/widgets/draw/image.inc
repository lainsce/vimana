case COGITO_IMAGE: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      
      // Read style values into locals without mutating node state
      CogitoColor bg = s.has_bg ? s.bg : n->bg;
      bool has_bg = s.has_bg || n->bg_set;
      CogitoColor border = s.has_border ? s.border : n->border_color;
      int border_w = s.has_border_width ? s.border_width : n->border_width;
      // User-set radius takes priority over theme
      int radius = n->radius_set ? n->border_radius : (s.has_radius ? s.radius : n->border_radius);
      // Images inside carousel_items should use the parent's border-radius
      if (radius == 0 && n->parent && n->parent->kind == COGITO_CAROUSEL_ITEM) {
        CogitoStyle ps = cogito_theme_resolve_node(n->parent);
        if (ps.has_radius) radius = ps.radius;
        else if (n->parent->border_radius > 0) radius = n->parent->border_radius;
      }
      if (radius > n->w / 2) radius = n->w / 2;
      if (radius > n->h / 2) radius = n->h / 2;
      
      // Load image from file path if not yet loaded (fallback if layout didn't load it)
      if (n->icon && n->icon->data && n->icon->data[0] && !n->image.loaded) {
        CogitoTexture* tex = NULL;
        int tw = 0, th = 0;
        if (cogito_image_load_file_texture(n->icon->data, n->w, n->h,
                                           &tex, &tw, &th, &n->image.avg_lum)) {
          n->image.texture = tex;
          n->image.w = tw;
          n->image.h = th;
          n->image.loaded = true;
          CogitoNode* win = cogito_node_window(n);
          if (win) {
            cogito_window_relayout(win);
          }
        } else {
          n->image.loaded = true; // Mark as attempted to avoid retrying
        }
      }
      
      // Draw background and shadow
      if (has_bg || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
        }
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      
      // Draw the loaded image texture
      if (n->image.texture && n->image.w > 0 && n->image.h > 0) {
        // Calculate draw dimensions - fit the image to the widget size (cover mode)
        int draw_w = n->w;
        int draw_h = n->h;
        int draw_x = n->x;
        int draw_y = n->y;
        
        // Scale to fill while maintaining aspect ratio (cover mode)
        float img_aspect = (float)n->image.w / (float)n->image.h;
        float widget_aspect = (n->h > 0) ? (float)n->w / (float)n->h : 1.0f;
        int src_x = 0, src_y = 0, src_w = n->image.w, src_h = n->image.h;
        
        if (img_aspect > widget_aspect && widget_aspect > 0) {
          // Image is wider - crop sides
          src_w = (int)(n->image.h * widget_aspect);
          src_x = (n->image.w - src_w) / 2;
        } else if (img_aspect < widget_aspect && img_aspect > 0) {
          // Image is taller - crop top/bottom
          src_h = (int)(n->image.w / widget_aspect);
          src_y = (n->image.h - src_h) / 2;
        }
        
        // --- Parallax effect for images inside carousel items ---
        // Shift the source rect horizontally so the rounded item acts as a
        // peephole into a wider scene.  The shift is proportional to the
        // item's offset from the carousel center.
        if (n->parent && n->parent->kind == COGITO_CAROUSEL_ITEM) {
          CogitoNode* ci = n->parent;          // carousel item
          CogitoNode* carousel = ci->parent;   // carousel
          if (carousel && carousel->kind == COGITO_CAROUSEL && carousel->w > 0) {
            int item_cx = ci->x + ci->w / 2;
            int car_cx  = carousel->x + carousel->w / 2;
            float ratio = (float)(item_cx - car_cx) / (float)(carousel->w / 2);
            // Strength: shift up to 20% of the visible source width
            float strength = 0.20f * (float)src_w;
            int shift = (int)(ratio * strength);
            src_x += shift;
            // Clamp so we don't read outside the texture
            if (src_x < 0) src_x = 0;
            if (src_x + src_w > n->image.w) src_x = n->image.w - src_w;
          }
        }

        CogitoColor white = {255, 255, 255, 255};
        if (cogito_backend && cogito_backend->draw_texture) {
          if (radius <= 0) {
            CogitoRect src_rect = {src_x, src_y, src_w, src_h};
            CogitoRect dst_rect = {draw_x, draw_y, draw_w, draw_h};
            cogito_backend->draw_texture(n->image.texture, src_rect, dst_rect, white);
          } else {
            // Rounded clipping for textures:
            // batch contiguous rows with the same inset to reduce draw calls.
            float r_f = (float)radius;
            int seg_start = 0;
            int seg_inset = -1;
            for (int row = 0; row <= draw_h; row++) {
              int inset = -1;
              if (row < draw_h) {
                inset = 0;
                if (row < radius) {
                  float y_dist = r_f - ((float)row + 0.5f);
                  float inside = r_f * r_f - y_dist * y_dist;
                  if (inside < 0.0f) inside = 0.0f;
                  inset = (int)(r_f - sqrtf(inside));
                } else if (row >= draw_h - radius) {
                  int br = draw_h - 1 - row;
                  float y_dist = r_f - ((float)br + 0.5f);
                  float inside = r_f * r_f - y_dist * y_dist;
                  if (inside < 0.0f) inside = 0.0f;
                  inset = (int)(r_f - sqrtf(inside));
                }
              }
              if (row == 0) {
                seg_inset = inset;
                continue;
              }
              if (row < draw_h && inset == seg_inset) continue;

              int seg_end = row;
              int seg_h = seg_end - seg_start;
              int line_w = draw_w - seg_inset * 2;
              if (seg_h > 0 && line_w > 0) {
                int src_y0 = src_y + (seg_start * src_h) / draw_h;
                int src_y1 = src_y + (seg_end * src_h) / draw_h;
                int src_h_seg = src_y1 - src_y0;
                if (src_h_seg < 1) src_h_seg = 1;

                int src_x0 = src_x + (seg_inset * src_w) / draw_w;
                int src_x1 = src_x + ((draw_w - seg_inset) * src_w) / draw_w;
                int src_w_seg = src_x1 - src_x0;
                if (src_w_seg < 1) src_w_seg = 1;

                CogitoRect src_seg = {src_x0, src_y0, src_w_seg, src_h_seg};
                CogitoRect dst_seg = {draw_x + seg_inset, draw_y + seg_start, line_w, seg_h};
                cogito_backend->draw_texture(n->image.texture, src_seg, dst_seg, white);
              }

              seg_start = row;
              seg_inset = inset;
            }
          }
        }
      } else if (n->icon && n->icon->data) {
        // Fallback to icon rendering if image failed to load
        CogitoColor icon_color = cogito_has_selection_text ? cogito_selection_text : (s.has_text ? s.text : cogito_on_color(n->selection_color));
        int target = s.has_icon_size ? s.icon_size : (n->w < n->h ? n->w : n->h);
        if (target <= 0) target = 16;
        int min_dim = n->w < n->h ? n->w : n->h;
        if (min_dim > 0 && target > min_dim) target = min_dim;
        int ix = n->x + (n->w - target) / 2;
        int iy = n->y + (n->h - target) / 2;
        cogito_draw_icon_fallback(n, n->icon->data, ix, iy, target, icon_color);
      }
      break;
    }
