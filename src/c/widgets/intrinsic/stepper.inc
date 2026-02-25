case COGITO_STEPPER: {
      int size = cogito_node_font_size(n);
      char val_buf[32];
      if (n->stepper.step >= 1.0) {
        snprintf(val_buf, sizeof(val_buf), "%d", (int)n->stepper.max);
      } else {
        snprintf(val_buf, sizeof(val_buf), "%.1f", n->stepper.max);
      }
      int val_w = cogito_text_width_size_node(n, val_buf, size);
      if (val_w < 32) val_w = 32;
      h = text_h + 16;
      if (h < 40) h = 40;
      int btn_w = h;
      w = btn_w + val_w + 16 + btn_w;
      break;
    }

