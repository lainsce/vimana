case COGITO_ZSTACK: {
			bool hover = false;
			bool active = false;
			cogito_node_hover_state(n, &hover, &active);
			CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
			n->bg_set = false;
			if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
			n->border_color_set = false;
			if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
			n->border_width = 0;
			n->border_width_set = false;
			if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
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
			// Check if any child is a visible sheet overlay (modal)
			bool zstack_has_sheet = false;
			for (size_t i = n->len; i > 0; i--) {
				CogitoNode* c = n->children[i - 1];
				if (c && (c->kind == COGITO_BOTTOM_SHEET || c->kind == COGITO_SIDE_SHEET)) {
					zstack_has_sheet = true;
					break;
				}
			}
			bool saved_suppressed = cogito_hover_suppressed;
			if (zstack_has_sheet) cogito_hover_suppressed = true;
			for (size_t i = 0; i < n->len; i++) {
				CogitoNode* c = n->children[i];
				bool is_sheet = c && (c->kind == COGITO_BOTTOM_SHEET || c->kind == COGITO_SIDE_SHEET);
				if (zstack_has_sheet && is_sheet) {
					cogito_overlay_draw_active = true;
					cogito_draw_node(c);
					cogito_overlay_draw_active = false;
				} else {
					cogito_draw_node(c);
				}
			}
			cogito_hover_suppressed = saved_suppressed;
			break;
		}

