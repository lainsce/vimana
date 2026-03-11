case COGITO_LABEL: {
      if (n->text) {
        const char* full_text = n->text->data ? n->text->data : "";
        const char* draw_text = full_text;
        int capped_len = -1;
        char trunc_stack[512];
        char* trunc_heap = NULL;
        if (n->label_max_width_chars_set) {
          capped_len = cogito_utf8_prefix_bytes_for_codepoints(full_text, n->label_max_width_chars);
          size_t len = (size_t)capped_len;
          if (len < sizeof(trunc_stack)) {
            memcpy(trunc_stack, full_text, len);
            trunc_stack[len] = '\0';
            draw_text = trunc_stack;
          } else {
            trunc_heap = (char*)calloc(len + 1, 1);
            if (trunc_heap) {
              memcpy(trunc_heap, full_text, len);
              trunc_heap[len] = '\0';
              draw_text = trunc_heap;
            }
          }
        }

        bool hover = false;
        bool active = false;
        cogito_node_hover_state(n, &hover, &active);
        int size = cogito_label_font_size(n);
        CogitoColor text = n->text_color;
        CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
        if (cogito_has_selection_text) {
          text = cogito_selection_text;
          if (text.a < 8) text = s.has_text ? s.text : n->text_color;
        }
        else if (s.has_text) text = s.text;
        int align = n->text_align;
        int content_x = n->x + n->padding_left;
        int content_y = n->y + n->padding_top;
        int content_w = n->w - n->padding_left - n->padding_right;
        int content_h = n->h - n->padding_top - n->padding_bottom;
        if (content_w < 0) content_w = 0;
        if (content_h < 0) content_h = 0;
        bool lbold = cogito_label_bold(n);
        bool lmedium = cogito_label_medium(n);
        if (n->text_wrap || n->label_line_chars_set) {
          cogito_draw_text_wrapped(n, content_x, content_y, content_w, draw_text, text, size, lbold, lmedium, align, true, 0);
        } else if (n->text_ellipsis) {
          int th = cogito_text_height_size(size);
          int ty = content_y + (content_h - th) / 2;
          cogito_draw_text_ellipsis(n, content_x, ty, content_w, draw_text, text, size, lbold, lmedium, align);
        } else if (n->text_fade || n->text_marquee) {
          int tw = cogito_text_width_size_node(n, draw_text, size);
          int th = cogito_text_height_size(size);
          int ty = content_y + (content_h - th) / 2;
          int tx = content_x;
          if (align == 1) tx = content_x + (content_w - tw) / 2;
          else if (align == 2) tx = content_x + (content_w - tw);
          bool overflows = tw > content_w;
          if (overflows && cogito_backend->begin_scissor && cogito_backend->end_scissor) {
            // Marquee scroll offset
            int scroll_offset = 0;
            if (n->text_marquee && overflows) {
              int max_scroll = tw - content_w;
              if (!n->marquee_active) {
                n->marquee_active = true;
                n->marquee_start = cogito_now();
                cogito_marquee_animating_count++;
              }
              // Cycle: 2s pause, 4s scroll right, 2s pause, 4s scroll back = 12s total
              double elapsed = cogito_now() - n->marquee_start;
              double cycle = fmod(elapsed, 12.0);
              if (cycle < 2.0) {
                scroll_offset = 0;
              } else if (cycle < 6.0) {
                double t = (cycle - 2.0) / 4.0;
                double et = t < 0.5 ? 2.0 * t * t : 1.0 - 0.5 * (2.0 - 2.0 * t) * (2.0 - 2.0 * t);
                scroll_offset = (int)(et * max_scroll);
              } else if (cycle < 8.0) {
                scroll_offset = max_scroll;
              } else {
                double t = (cycle - 8.0) / 4.0;
                double et = t < 0.5 ? 2.0 * t * t : 1.0 - 0.5 * (2.0 - 2.0 * t) * (2.0 - 2.0 * t);
                scroll_offset = max_scroll - (int)(et * max_scroll);
              }
              tx = content_x - scroll_offset;
            }
            cogito_backend->begin_scissor(content_x, n->y, content_w, n->h);
            cogito_draw_text_size_node(n, tx, ty, draw_text, text, size, lbold, lmedium);
            // Find ancestor background for gradient overlay
            CogitoColor fade_bg = cogito_color(255, 255, 255, 255);
            for (const CogitoNode* p = n->parent; p; p = p->parent) {
              if (p->bg_set && p->bg.a > 0) { fade_bg = p->bg; break; }
            }
            int fade_w = content_w / 5;
            if (fade_w < 16) fade_w = 16;
            if (fade_w > 48) fade_w = 48;
            // Fade intensity ramps with scroll offset for smooth transitions
            float right_fade_intensity = 1.0f;
            float left_fade_intensity = 0.0f;
            if (n->text_marquee && overflows) {
              int max_scroll = tw - content_w;
              // Ramp over the first/last fade_w pixels of scroll
              if (max_scroll > 0) {
                float ramp = (float)fade_w;
                right_fade_intensity = (scroll_offset < max_scroll)
                  ? fminf(1.0f, (float)(max_scroll - scroll_offset) / ramp) : 0.0f;
                left_fade_intensity = fminf(1.0f, (float)scroll_offset / ramp);
              }
            }
            // Right fade
            if (right_fade_intensity > 0.001f) {
              int fx = content_x + content_w - fade_w;
              for (int i = 0; i < fade_w; i++) {
                float ft = (float)i / (float)fade_w;
                ft = ft * ft * right_fade_intensity;
                CogitoColor c = fade_bg;
                c.a = (uint8_t)(fade_bg.a * ft);
                cogito_backend->draw_rect(fx + i, n->y, 1, n->h, c);
              }
            }
            // Left fade (marquee only)
            if (n->text_marquee && left_fade_intensity > 0.001f) {
              for (int i = 0; i < fade_w; i++) {
                float ft = (float)(fade_w - 1 - i) / (float)fade_w;
                ft = ft * ft * left_fade_intensity;
                CogitoColor c = fade_bg;
                c.a = (uint8_t)(fade_bg.a * ft);
                cogito_backend->draw_rect(content_x + i, n->y, 1, n->h, c);
              }
            }
            cogito_backend->end_scissor();
          } else {
            // Text fits or no scissor — stop marquee if active
            if (n->marquee_active) {
              n->marquee_active = false;
              cogito_marquee_animating_count--;
            }
            cogito_draw_text_size_node(n, tx, ty, draw_text, text, size, lbold, lmedium);
          }
        } else {
          int tw = cogito_text_width_size_node(n, draw_text, size);
          int th = cogito_text_height_size(size);
          int ty = content_y + (content_h - th) / 2;
          int tx = content_x;
          if (align == 1) tx = content_x + (content_w - tw) / 2;
          else if (align == 2) tx = content_x + (content_w - tw);
          cogito_draw_text_size_node(n, tx, ty, draw_text, text, size, lbold, lmedium);
        }
        if (trunc_heap) free(trunc_heap);
      }
      break;
    }
