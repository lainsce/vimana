case COGITO_COLORPICKER: {
      if (n->colorpicker.popover_panel) {
        // Panel: DatePicker-like top/header rhythm + 3 slider rows + footer section.
        h = COGITO_COLORPICKER_PANEL_TOP +
            COGITO_COLORPICKER_PANEL_HEADER_H +
            COGITO_COLORPICKER_PANEL_HEADER_TO_ROWS_GAP +
         3 * COGITO_COLORPICKER_PANEL_SLIDER_H +
            2 * COGITO_COLORPICKER_PANEL_ROW_GAP +
            COGITO_COLORPICKER_PANEL_FOOTER_GAP +
            COGITO_COLORPICKER_PANEL_FOOTER_H +
            COGITO_COLORPICKER_PANEL_BOTTOM;
        w = COGITO_COLORPICKER_PANEL_W;
      } else {
        // Trigger: compact field
        w = COGITO_COLORPICKER_FIELD_MIN_WIDTH;
        h = COGITO_COLORPICKER_FIELD_HEIGHT;
      }
      break;
    }

