case COGITO_DRAWING_AREA: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = s.has_bg ? s.bg : (n->bg_set ? n->bg : cogito_theme.base.bg);
      int radius = n->radius_set ? n->border_radius : (s.has_radius ? s.radius : n->border_radius);
      if (radius < 0) radius = 0;
      cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
      CogitoColor stroke = s.has_text ? s.text : (n->text_color_set ? n->text_color : cogito_color(0, 0, 0, 255));
      int pc = n->drawing_area.point_count;
      for (int i = 1; i < pc; i++) {
        int x0 = n->drawing_area.points_x[i - 1];
        int y0 = n->drawing_area.points_y[i - 1];
        int x1 = n->drawing_area.points_x[i];
        int y1 = n->drawing_area.points_y[i];
        if (x0 < 0 || y0 < 0 || x1 < 0 || y1 < 0) continue;
        cogito_draw_line(n->x + x0, n->y + y0, n->x + x1, n->y + y1, stroke);
      }
      if (n->on_draw) {
        CogitoNode* prev_canvas = cogito_canvas_active;
        cogito_canvas_active = n;
        ErgoVal args[3] = {EV_OBJ(n), EV_INT(n->w), EV_INT(n->h)};
        cogito_invoke(n->on_draw, 3, args);
        cogito_canvas_active = prev_canvas;
      }
      break;
    }
