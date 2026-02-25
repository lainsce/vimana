case COGITO_CAROUSEL: {
      if (n->len > 0) {
        // Draw background first
        CogitoStyle s = cogito_theme_resolve_node_state(n, false, false);
        CogitoColor bg = s.has_bg ? s.bg : n->bg;
        if (s.has_bg || n->bg_set) {
          int radius = s.has_radius ? s.radius : 0;
          cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        }
        
        // Apply scissor clipping for carousel content
        // Clip to carousel's exact bounds - this ensures items don't render outside
        if (cogito_backend && cogito_backend->begin_scissor) {
          // Clamp scissor to positive coordinates to avoid SDL issues
          int scissor_x = n->x < 0 ? 0 : n->x;
          int scissor_y = n->y < 0 ? 0 : n->y;
          int scissor_w = n->w;
          int scissor_h = n->h;
          cogito_backend->begin_scissor(scissor_x, scissor_y, scissor_w, scissor_h);
        }
        for (size_t i = 0; i < n->len; i++) {
          cogito_draw_node(n->children[i]);
        }
        if (cogito_backend && cogito_backend->end_scissor) {
          cogito_backend->end_scissor();
        }
        
        // Draw rounded-corner masks on the carousel boundary.
        // The scissor clip is rectangular, so partially-scrolled items at the
        // edges produce hard cuts.  Overlay corner masks to restore rounding.
        int cr_radius = s.has_radius ? s.radius : n->border_radius;
        if (cr_radius > 0) {
          // Find the visible color behind the carousel (skip transparent parents)
          CogitoColor cr_mask = {255, 255, 255, 255};
          CogitoNode* p = n->parent;
          while (p) {
            if (p->bg_set && p->bg.a > 0) { cr_mask = p->bg; break; }
            p = p->parent;
          }
          int cx = n->x, cy = n->y, cw = n->w, ch = n->h;
          #define COGITO_CR_AA_COV(c) ((c) <= 0.05f ? 0.0f : ((c) >= 1.0f ? 1.0f : powf((c), 0.7f)))
          float cr_rf = (float)cr_radius;
          // Top-left
          for (int dy = 0; dy < cr_radius; dy++) {
            float yd = cr_rf - ((float)dy + 0.5f);
            float ins = cr_rf * cr_rf - yd * yd;
            if (ins < 0.0f) ins = 0.0f;
            float xe = cr_rf - sqrtf(ins);
            int sw = (int)xe;
            float fr = xe - (float)sw;
            if (sw > 0) cogito_draw_rect(cx, cy + dy, sw, 1, cr_mask, 0);
            if (fr > 0.01f && sw < cw) {
              float cv = COGITO_CR_AA_COV(fr);
              CogitoColor aa = cr_mask; aa.a = (uint8_t)(cr_mask.a * cv + 0.5f);
              if (aa.a > 0) cogito_draw_rect(cx + sw, cy + dy, 1, 1, aa, 0);
            }
          }
          // Top-right
          for (int dy = 0; dy < cr_radius; dy++) {
            float yd = cr_rf - ((float)dy + 0.5f);
            float ins = cr_rf * cr_rf - yd * yd;
            if (ins < 0.0f) ins = 0.0f;
            float xe = cr_rf - sqrtf(ins);
            int sw = (int)xe;
            float fr = xe - (float)sw;
            if (sw > 0) cogito_draw_rect(cx + cw - sw, cy + dy, sw, 1, cr_mask, 0);
            if (fr > 0.01f && sw < cw) {
              float cv = COGITO_CR_AA_COV(fr);
              CogitoColor aa = cr_mask; aa.a = (uint8_t)(cr_mask.a * cv + 0.5f);
              if (aa.a > 0) cogito_draw_rect(cx + cw - sw - 1, cy + dy, 1, 1, aa, 0);
            }
          }
          // Bottom-left
          for (int dy = 0; dy < cr_radius; dy++) {
            float yd = (float)(dy + 1) - 0.5f;
            float ins = cr_rf * cr_rf - yd * yd;
            if (ins < 0.0f) ins = 0.0f;
            float xe = cr_rf - sqrtf(ins);
            int sw = (int)xe;
            float fr = xe - (float)sw;
            if (sw > 0) cogito_draw_rect(cx, cy + ch - cr_radius + dy, sw, 1, cr_mask, 0);
            if (fr > 0.01f && sw < cw) {
              float cv = COGITO_CR_AA_COV(fr);
              CogitoColor aa = cr_mask; aa.a = (uint8_t)(cr_mask.a * cv + 0.5f);
              if (aa.a > 0) cogito_draw_rect(cx + sw, cy + ch - cr_radius + dy, 1, 1, aa, 0);
            }
          }
          // Bottom-right
          for (int dy = 0; dy < cr_radius; dy++) {
            float yd = (float)(dy + 1) - 0.5f;
            float ins = cr_rf * cr_rf - yd * yd;
            if (ins < 0.0f) ins = 0.0f;
            float xe = cr_rf - sqrtf(ins);
            int sw = (int)xe;
            float fr = xe - (float)sw;
            if (sw > 0) cogito_draw_rect(cx + cw - sw, cy + ch - cr_radius + dy, sw, 1, cr_mask, 0);
            if (fr > 0.01f && sw < cw) {
              float cv = COGITO_CR_AA_COV(fr);
              CogitoColor aa = cr_mask; aa.a = (uint8_t)(cr_mask.a * cv + 0.5f);
              if (aa.a > 0) cogito_draw_rect(cx + cw - sw - 1, cy + ch - cr_radius + dy, 1, 1, aa, 0);
            }
          }
          #undef COGITO_CR_AA_COV
        }
      }
      break;
    }
