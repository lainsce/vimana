case COGITO_COLORPICKER: {
      if (n->colorpicker.popover_panel && n->len > 0) {
        // Position 3 slider children below the header area.
        int sx = x + COGITO_COLORPICKER_PANEL_PAD + COGITO_COLORPICKER_PANEL_LABEL_W + COGITO_COLORPICKER_PANEL_LABEL_GAP;
        int sw = w - COGITO_COLORPICKER_PANEL_PAD * 2 -
                 COGITO_COLORPICKER_PANEL_LABEL_W - COGITO_COLORPICKER_PANEL_LABEL_GAP -
                 COGITO_COLORPICKER_PANEL_VALUE_GAP - COGITO_COLORPICKER_PANEL_VALUE_W;
        if (sw < 0) sw = 0;
        int sy = cogito_colorpicker_panel_rows_top(n);
        for (size_t i = 0; i < n->len; i++) {
          CogitoNode* c = n->children[i];
          if (c->kind != COGITO_SLIDER) continue;
          int ch = COGITO_COLORPICKER_PANEL_SLIDER_H;
          cogito_layout_node(c, sx, sy, sw, ch);
          sy += ch + COGITO_COLORPICKER_PANEL_ROW_GAP;
        }
      }
      break;
    }

