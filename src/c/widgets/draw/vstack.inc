case COGITO_VSTACK: {
			bool hover = false;
			bool active = false;
			cogito_node_hover_state(n, &hover, &active);
			CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
			bool inside_appbar = false;
			for (CogitoNode* p = n->parent; p; p = p->parent) {
				if (p->kind == COGITO_APPBAR) {
					inside_appbar = true;
					break;
				}
			}
			n->bg_set = false;
			if (!inside_appbar && s.has_bg) { n->bg = s.bg; n->bg_set = true; }
			n->border_color_set = false;
			if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
			n->border_width = 0;
			n->border_width_set = false;
			if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
			if (inside_appbar) {
				n->border_width = 0;
				n->border_width_set = false;
			}
			if (s.has_radius) n->border_radius = s.radius;
			if (n->bg_set || n->border_width > 0) {
				if (n->shadow_set && n->shadow_level >= 0) {
					cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, n->border_radius, n->shadow_level);
				}
				if (n->bg_set) {
					cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, n->bg, n->border_radius);
				}
				if (n->border_width > 0) {
					cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
				}
			}
			for (size_t i = 0; i < n->len; i++) {
				if (!cogito_rect_visible(n->children[i]->x, n->children[i]->y, n->children[i]->w, n->children[i]->h, n->x, n->y, n->w, n->h)) continue;
				cogito_draw_node(n->children[i]);
			}
			break;
		}

