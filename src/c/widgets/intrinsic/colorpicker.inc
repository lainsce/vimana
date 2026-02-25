case COGITO_COLORPICKER: {
      if (n->colorpicker.popover_panel) {
        // Panel: 3 XS sliders (48dp each) + 2 gaps (12dp) + top/bottom pad (16dp each) + footer (48dp)
        h = 16 + 3 * 48 + 2 * 12 + 16 + COGITO_COLORPICKER_PANEL_FOOTER_H;
        w = COGITO_COLORPICKER_PANEL_W;
      } else {
        // Trigger: compact field
        w = COGITO_COLORPICKER_FIELD_MIN_WIDTH;
        h = COGITO_COLORPICKER_FIELD_HEIGHT;
      }
      break;
    }

