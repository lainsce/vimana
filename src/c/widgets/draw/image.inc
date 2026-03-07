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
        if (n->shadow_set && n->shadow_level >= 0) {
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
          float blur_sigma = n->blur_radius;
          if (blur_sigma > 0.0f && cogito_backend->render_target_create &&
              cogito_backend->set_render_target &&
              cogito_backend->texture_gaussian_blur &&
              cogito_backend->texture_destroy && draw_w > 0 && draw_h > 0) {
            int glow_pad_x = (int)(blur_sigma * 3.0f + 0.5f);
            int glow_pad_y = glow_pad_x;
            if (glow_pad_x < 1) glow_pad_x = 1;
            if (glow_pad_y < 1) glow_pad_y = 1;
            bool glow_stale = n->image.glow_texture == NULL ||
                              n->image.glow_source != n->image.texture ||
                              n->image.glow_w != draw_w ||
                              n->image.glow_h != draw_h ||
                              n->image.glow_sigma != blur_sigma;
            if (glow_stale) {
              if (n->image.glow_texture) {
                cogito_backend->texture_destroy(n->image.glow_texture);
                n->image.glow_texture = NULL;
              }
              n->image.glow_source = NULL;
              n->image.glow_w = 0;
              n->image.glow_h = 0;
              n->image.glow_sigma = 0.0f;

              int full_w = draw_w + glow_pad_x * 2;
              int full_h = draw_h + glow_pad_y * 2;
              // Work at half resolution for performance
              int half_w = full_w / 2;
              int half_h = full_h / 2;
              if (half_w < 16) half_w = 16;
              if (half_h < 16) half_h = 16;
              int half_pad_x = (glow_pad_x * half_w) / full_w;
              int half_pad_y = (glow_pad_y * half_h) / full_h;
              if (half_pad_x < 1) half_pad_x = 1;
              if (half_pad_y < 1) half_pad_y = 1;
              int half_img_w = half_w - half_pad_x * 2;
              int half_img_h = half_h - half_pad_y * 2;
              if (half_img_w < 1) half_img_w = 1;
              if (half_img_h < 1) half_img_h = 1;

              CogitoTexture* rt_pad = cogito_backend->render_target_create(half_w, half_h);
              if (rt_pad) {
                CogitoRect img_src = {src_x, src_y, src_w, src_h};
                cogito_backend->set_render_target(rt_pad);
                cogito_backend->clear(cogito_color(0, 0, 0, 0));

                // Scale radius to half-res image area
                int hr = 0;
                if (radius > 0 && half_img_w > 0 && half_img_h > 0) {
                  int rx = (radius * half_img_w + draw_w / 2) / draw_w;
                  int ry = (radius * half_img_h + draw_h / 2) / draw_h;
                  hr = rx < ry ? rx : ry;
                  if (hr < 1) hr = 1;
                  if (hr > half_img_w / 2) hr = half_img_w / 2;
                  if (hr > half_img_h / 2) hr = half_img_h / 2;
                }

                if (hr <= 0) {
                  CogitoRect img_dst = {half_pad_x, half_pad_y, half_img_w, half_img_h};
                  cogito_backend->draw_texture(n->image.texture, img_src, img_dst, white);
                } else {
                  // Row-segment rounded clipping at half-res
                  float hr_f = (float)hr;
                  int seg_start = 0, seg_inset = -1;
                  for (int row = 0; row <= half_img_h; row++) {
                    int inset = -1;
                    if (row < half_img_h) {
                      inset = 0;
                      if (row < hr) {
                        float yd = hr_f - ((float)row + 0.5f);
                        float ins = hr_f * hr_f - yd * yd;
                        if (ins < 0.0f) ins = 0.0f;
                        inset = (int)(hr_f - sqrtf(ins));
                      } else if (row >= half_img_h - hr) {
                        int br = half_img_h - 1 - row;
                        float yd = hr_f - ((float)br + 0.5f);
                        float ins = hr_f * hr_f - yd * yd;
                        if (ins < 0.0f) ins = 0.0f;
                        inset = (int)(hr_f - sqrtf(ins));
                      }
                    }
                    if (row == 0) { seg_inset = inset; continue; }
                    if (row < half_img_h && inset == seg_inset) continue;
                    int sh = row - seg_start;
                    int lw = half_img_w - seg_inset * 2;
                    if (sh > 0 && lw > 0) {
                      int sy0 = src_y + (seg_start * src_h) / half_img_h;
                      int sy1 = src_y + (row * src_h) / half_img_h;
                      int ssh = sy1 - sy0; if (ssh < 1) ssh = 1;
                      int sx0 = src_x + (seg_inset * src_w) / half_img_w;
                      int sx1 = src_x + ((half_img_w - seg_inset) * src_w) / half_img_w;
                      int ssw = sx1 - sx0; if (ssw < 1) ssw = 1;
                      CogitoRect ss = {sx0, sy0, ssw, ssh};
                      CogitoRect sd = {half_pad_x + seg_inset, half_pad_y + seg_start, lw, sh};
                      cogito_backend->draw_texture(n->image.texture, ss, sd, white);
                    }
                    seg_start = row;
                    seg_inset = inset;
                  }
                }

                cogito_backend->set_render_target(NULL);

                // Half the sigma for half-res, bilinear upscale adds smoothing
                float half_sigma = blur_sigma * 0.5f;
                if (half_sigma < 1.0f) half_sigma = 1.0f;
                CogitoTexture* blurred = cogito_backend->texture_gaussian_blur(rt_pad, half_sigma);
                cogito_backend->texture_destroy(rt_pad);

                if (blurred) {
                  n->image.glow_texture = blurred;
                  n->image.glow_source = n->image.texture;
                  n->image.glow_w = draw_w;
                  n->image.glow_h = draw_h;
                  n->image.glow_sigma = blur_sigma;
                }
              }
            }

            if (n->image.glow_texture) {
              CogitoRect glow_src = {0, 0, 0, 0};
              CogitoRect glow_dst = {
                draw_x - glow_pad_x,
                draw_y - glow_pad_y,
                draw_w + glow_pad_x * 2,
                draw_h + glow_pad_y * 2
              };
              CogitoColor glow_tint = {255, 255, 255, 136};
              cogito_backend->draw_texture(n->image.glow_texture, glow_src, glow_dst, glow_tint);
            }
          }

          if (radius <= 0) {
            CogitoRect src_rect = {src_x, src_y, src_w, src_h};
            CogitoRect dst_rect = {draw_x, draw_y, draw_w, draw_h};
            cogito_backend->draw_texture(n->image.texture, src_rect, dst_rect, white);
          } else {
            // Rounded clipping for textures with anti-aliased edges.
            // Use ceilf() for interior strips so they never extend past the
            // rounded rect boundary, then draw edge pixels at fractional alpha.
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
                  inset = (int)ceilf(r_f - sqrtf(inside));
                } else if (row >= draw_h - radius) {
                  int br = draw_h - 1 - row;
                  float y_dist = r_f - ((float)br + 0.5f);
                  float inside = r_f * r_f - y_dist * y_dist;
                  if (inside < 0.0f) inside = 0.0f;
                  inset = (int)ceilf(r_f - sqrtf(inside));
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
            // AA edge pixels along the corner arcs
            for (int row = 0; row < draw_h; row++) {
              float y_dist;
              bool in_corner = false;
              if (row < radius) {
                y_dist = r_f - ((float)row + 0.5f);
                in_corner = true;
              } else if (row >= draw_h - radius) {
                int br = draw_h - 1 - row;
                y_dist = r_f - ((float)br + 0.5f);
                in_corner = true;
              }
              if (!in_corner) continue;
              float ins_sq = r_f * r_f - y_dist * y_dist;
              if (ins_sq <= 0.0f) continue;
              float exact = r_f - sqrtf(ins_sq);
              int fl = (int)floorf(exact);
              float frac = exact - (float)fl;
              if (frac < 0.004f) continue; // pixel fully inside, already drawn
              float coverage = 1.0f - frac;
              uint8_t ea = (uint8_t)(coverage * 255.0f + 0.5f);
              if (ea < 1) continue;
              int sy = src_y + (row * src_h) / draw_h;
              int sh_e = (src_h + draw_h - 1) / draw_h;
              if (sh_e < 1) sh_e = 1;
              if (sy + sh_e > src_y + src_h) sh_e = src_y + src_h - sy;
              if (sh_e < 1) sh_e = 1;
              CogitoColor et = {255, 255, 255, ea};
              // Left edge
              if (fl >= 0 && fl < draw_w / 2) {
                int sx_l = src_x + (fl * src_w) / draw_w;
                int sw_l = (src_w + draw_w - 1) / draw_w;
                if (sw_l < 1) sw_l = 1;
                CogitoRect es = {sx_l, sy, sw_l, sh_e};
                CogitoRect ed = {draw_x + fl, draw_y + row, 1, 1};
                cogito_backend->draw_texture(n->image.texture, es, ed, et);
              }
              // Right edge
              int rc = draw_w - 1 - fl;
              if (rc >= draw_w / 2 && rc < draw_w) {
                int sx_r = src_x + (rc * src_w) / draw_w;
                int sw_r = (src_w + draw_w - 1) / draw_w;
                if (sw_r < 1) sw_r = 1;
                CogitoRect es = {sx_r, sy, sw_r, sh_e};
                CogitoRect ed = {draw_x + rc, draw_y + row, 1, 1};
                cogito_backend->draw_texture(n->image.texture, es, ed, et);
              }
            }
          }
        }
      } else if (n->icon && n->icon->data) {
        // Fallback to icon rendering if image failed to load
        CogitoColor icon_color;
        if (cogito_has_selection_text) {
          icon_color = cogito_selection_text;
          if (icon_color.a < 8) icon_color = s.has_text ? s.text : cogito_on_color(n->selection_color);
        } else {
          icon_color = s.has_text ? s.text : cogito_on_color(n->selection_color);
        }
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
