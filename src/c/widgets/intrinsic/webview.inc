case COGITO_WEBVIEW: {
      int pad_h = n->padding_left + n->padding_right;
      int pad_v = n->padding_top + n->padding_bottom;
      int base_w = n->min_w_set ? n->min_w : 360;
      int content_w = base_w + pad_h;
      if (content_w < 20) content_w = 20;

      const char* html = (n->icon && n->icon->data) ? n->icon->data : "";
      int content_h = 0;
      if (html[0]) {
        CogitoColor dummy = cogito_rgba(0, 0, 0, 255);
        content_h = cogito_webview_measure_content(n, html, content_w, dummy);
      }

      w = base_w;
      h = content_h + pad_v;
      if (n->min_h_set && h < n->min_h) h = n->min_h;
      if (h < 24) h = 24;
      break;
    }
