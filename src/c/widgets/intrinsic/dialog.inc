case COGITO_DIALOG:
      w = 320;
      h = 200;
      {
        CogitoStyle ds = cogito_theme_resolve_node(n);
        int pad_l = 24;
        int pad_t = 24;
        int pad_r = 24;
        int pad_b = 24;
        if (n->padding_set) {
          pad_l = n->padding_left;
          pad_t = n->padding_top;
          pad_r = n->padding_right;
          pad_b = n->padding_bottom;
        } else {
          if (ds.has_padding) {
            pad_l = ds.padding_left;
            pad_t = ds.padding_top;
            pad_r = ds.padding_right;
            pad_b = ds.padding_bottom;
          }
          if (ds.has_padding_left) pad_l = ds.padding_left;
          if (ds.has_padding_top) pad_t = ds.padding_top;
          if (ds.has_padding_right) pad_r = ds.padding_right;
          if (ds.has_padding_bottom) pad_b = ds.padding_bottom;
        }
        int title_size = 20;
        int title_h = cogito_text_height_size(title_size);
        int title_w = (n->text && n->text->data && n->text->data[0]) ? cogito_text_width_size_node(n, n->text->data, title_size) : 0;
        int close_box = 24;
        int header_h = title_h > close_box ? title_h : close_box;
        int header_gap = 16;
        int section_gap = 24;
        int cw = 0;
        int ch = 0;
        for (size_t i = 0; i < n->len; i++) {
          int iw = 0;
          int ih = 0;
          cogito_intrinsic_size(n->children[i], &iw, &ih);
          int tw = iw + n->children[i]->margin_left + n->children[i]->margin_right;
          int th = ih + n->children[i]->margin_top + n->children[i]->margin_bottom;
          if (tw > cw) cw = tw;
          ch += th;
          if (i + 1 < n->len) ch += section_gap;
        }
        int min_header_total_w = 24 + title_w + 12 + close_box + 24;
        int min_header_cw = min_header_total_w - pad_l - pad_r;
        if (min_header_cw < 0) min_header_cw = 0;
        if (cw < min_header_cw) cw = min_header_cw;
        w = pad_l + cw + pad_r;
        h = pad_t + header_h + header_gap + ch + pad_b;
        if (w < 280) w = 280;
        if (w > 560) w = 560;
        if (h < 240) h = 240;
      }
      break;
    
