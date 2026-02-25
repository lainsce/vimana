case COGITO_SHAPE: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor fill = s.has_bg ? s.bg : (n->bg_set ? n->bg : cogito_color(24, 24, 33, 255));
      if (n->shape.custom_color_set) {
        fill = n->shape.custom_color;
      }
      CogitoColor stroke = s.has_border ? s.border : (n->border_color_set ? n->border_color : fill);
      int stroke_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 0);
      if (n->shape.color_style > 0 && !n->shape.custom_color_set) {
        CogitoColor primary = cogito_color(103, 80, 164, 255);
        CogitoColor secondary = cogito_color(98, 91, 113, 255);
        CogitoColor tertiary = cogito_color(39, 116, 125, 255);

        if (cogito_theme.iconbtn_filled.has_bg && cogito_theme.iconbtn_filled.bg.a > 0) {
          primary = cogito_theme.iconbtn_filled.bg;
        } else if (cogito_theme.base.has_text && cogito_theme.base.text.a > 0) {
          primary = cogito_theme.base.text;
        }
        if (cogito_theme.iconbtn_tonal.has_bg && cogito_theme.iconbtn_tonal.bg.a > 0) {
          secondary = cogito_theme.iconbtn_tonal.bg;
        } else {
          secondary = primary;
        }
        if (cogito_theme.iconbtn_iconic.has_text && cogito_theme.iconbtn_iconic.text.a > 0) {
          tertiary = cogito_theme.iconbtn_iconic.text;
        } else if (cogito_theme.iconbtn_iconic.has_icon_color && cogito_theme.iconbtn_iconic.icon_color.a > 0) {
          tertiary = cogito_theme.iconbtn_iconic.icon_color;
        }

        // Prefer explicit SUM custom classes when present.
        const char* tone_name = (n->shape.color_style == 1)
            ? "primary"
            : ((n->shape.color_style == 2) ? "secondary" : "tertiary");
        for (int i = 0; i < cogito_theme.custom_class_count; i++) {
          if (strcmp(cogito_theme.custom_classes[i].name, tone_name) != 0) continue;
          if (cogito_theme.custom_classes[i].kind != COGITO_KIND_COUNT &&
              cogito_theme.custom_classes[i].kind != COGITO_SHAPE) continue;
          CogitoStyle cs = cogito_theme.custom_classes[i].style;
          if (cs.has_bg && cs.bg.a > 0) {
            if (n->shape.color_style == 1) primary = cs.bg;
            else if (n->shape.color_style == 2) secondary = cs.bg;
            else tertiary = cs.bg;
            break;
          }
          if (cs.has_text && cs.text.a > 0) {
            if (n->shape.color_style == 1) primary = cs.text;
            else if (n->shape.color_style == 2) secondary = cs.text;
            else tertiary = cs.text;
            break;
          }
        }

        CogitoColor pal_primary = cogito_color(0, 0, 0, 0);
        CogitoColor pal_secondary = cogito_color(0, 0, 0, 0);
        CogitoColor pal_tertiary = cogito_color(0, 0, 0, 0);
        cogito_theme_get_state_layer_palette(&pal_primary, &pal_secondary, NULL, &pal_tertiary, NULL, NULL,
                                             NULL, NULL, NULL, NULL);
        if (pal_primary.a > 0) primary = pal_primary;
        if (pal_secondary.a > 0) secondary = pal_secondary;
        if (pal_tertiary.a > 0) tertiary = pal_tertiary;
        if (primary.a == 0) primary = cogito_color(103, 80, 164, 255);
        if (secondary.a == 0) secondary = primary;
        if (tertiary.a == 0) tertiary = cogito_color(39, 116, 125, 255);
        if (n->shape.color_style == 1) fill = primary;
        else if (n->shape.color_style == 2) fill = secondary;
        else if (n->shape.color_style == 3) fill = tertiary;
        stroke = fill;
      }

      float px[COGITO_SHAPE_SVG_POLY_MAX];
      float py[COGITO_SHAPE_SVG_POLY_MAX];
      int point_count = n->shape.svg_poly_count;
      bool use_svg_poly = (point_count >= 3);
      if (!use_svg_poly) point_count = COGITO_SHAPE_VERTEX_COUNT;
      if (point_count > COGITO_SHAPE_SVG_POLY_MAX) point_count = COGITO_SHAPE_SVG_POLY_MAX;
      for (int i = 0; i < point_count; i++) {
        float vx = use_svg_poly ? n->shape.svg_poly[i * 2 + 0] : n->shape.vertices[i * 2 + 0];
        float vy = use_svg_poly ? n->shape.svg_poly[i * 2 + 1] : n->shape.vertices[i * 2 + 1];
        if (vx < 0.0f) vx = 0.0f;
        if (vx > 1.0f) vx = 1.0f;
        if (vy < 0.0f) vy = 0.0f;
        if (vy > 1.0f) vy = 1.0f;
        px[i] = (float)n->x + vx * (float)(n->w - 1);
        py[i] = (float)n->y + vy * (float)(n->h - 1);
      }

      bool pixel_shape = (n->shape.preset == COGITO_SHAPE_PIXEL_CIRCLE ||
                          n->shape.preset == COGITO_SHAPE_PIXEL_TRIANGLE);
      bool smooth_points = !pixel_shape && !use_svg_poly;
      bool antialias = !pixel_shape;
      if (antialias &&
          stroke_w > 0 &&
          stroke.r == fill.r &&
          stroke.g == fill.g &&
          stroke.b == fill.b &&
          stroke.a == fill.a) {
        // Avoid jagged overdraw from a same-color aliased outline on top of AA fill.
        stroke_w = 0;
      }
      cogito_draw_shape_polygon(px, py, point_count, fill, stroke, stroke_w, smooth_points, antialias);
      break;
    }
