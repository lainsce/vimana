case COGITO_COLORPICKER: {
      if (n->colorpicker.popover_panel && n->len > 0) {
        // Position 3 slider children vertically inside the panel.
        // Layout: left pad=16, label_area=20, label_gap=8 → slider starts at x+44
        int pad = 16;
        int label_area = 20;
        int label_gap = 8;
        int row_gap = 12;
        int sx = x + pad + label_area + label_gap;
        int sw = w - pad - label_area - label_gap - pad;
        if (sw < 0) sw = 0;
        int sy = y + pad;
        for (size_t i = 0; i < n->len; i++) {
          CogitoNode* c = n->children[i];
          if (c->kind != COGITO_SLIDER) continue;
          int cw = 0, ch = 0;
          cogito_intrinsic_size(c, &cw, &ch);
          cogito_layout_node(c, sx, sy, sw, ch);
          sy += ch + row_gap;
        }
      }
      break;
    }

