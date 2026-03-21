    case COGITO_VIDEO_VIEW: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = s.has_bg ? s.bg : n->bg;
      bool has_bg = s.has_bg || n->bg_set;
      int radius = n->radius_set ? n->border_radius
                 : (s.has_radius ? s.radius : n->border_radius);
      if (radius > n->w / 2) radius = n->w / 2;
      if (radius > n->h / 2) radius = n->h / 2;

      if (has_bg) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
      }

      // Update texture from GStreamer frame
      n->kd->video_view.texture =
          cogito_gst_update_video_texture(n->kd->video_view.texture);

      if (n->kd->video_view.texture &&
          n->kd->video_view.video_w > 0 && n->kd->video_view.video_h > 0) {
        int iw = n->kd->video_view.video_w;
        int ih = n->kd->video_view.video_h;
        float img_aspect = (float)iw / (float)ih;
        float wgt_aspect = (n->h > 0) ? (float)n->w / (float)n->h : 1.0f;
        int draw_x = n->x, draw_y = n->y, draw_w = n->w, draw_h = n->h;
        int src_x = 0, src_y = 0, src_w = iw, src_h = ih;

        if (n->kd->video_view.fit == 1) {
          // Contain: scale to fit entirely within widget
          float scale = ((float)n->w / (float)iw < (float)n->h / (float)ih)
                      ? (float)n->w / (float)iw
                      : (float)n->h / (float)ih;
          draw_w = (int)(iw * scale);
          draw_h = (int)(ih * scale);
          draw_x = n->x + (n->w - draw_w) / 2;
          draw_y = n->y + (n->h - draw_h) / 2;
        } else {
          // Cover: crop source to fill widget
          if (img_aspect > wgt_aspect && wgt_aspect > 0) {
            src_w = (int)(ih * wgt_aspect);
            src_x = (iw - src_w) / 2;
          } else if (img_aspect < wgt_aspect && img_aspect > 0) {
            src_h = (int)(iw / wgt_aspect);
            src_y = (ih - src_h) / 2;
          }
        }

        if (radius > 0 && cogito_backend->begin_scissor) {
          cogito_backend->begin_scissor(n->x, n->y, n->w, n->h);
        }
        if (cogito_backend && cogito_backend->draw_texture) {
          CogitoRect src_rect = {src_x, src_y, src_w, src_h};
          CogitoRect dst_rect = {draw_x, draw_y, draw_w, draw_h};
          CogitoColor white = {255, 255, 255, 255};
          cogito_backend->draw_texture(n->kd->video_view.texture,
                                       src_rect, dst_rect, white);
        }
        if (radius > 0 && cogito_backend->end_scissor) {
          cogito_backend->end_scissor();
        }
      }
      break;
    }
