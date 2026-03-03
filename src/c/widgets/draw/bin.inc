case COGITO_BIN: {
			bool hover = false;
			bool active = false;
			cogito_node_hover_state(n, &hover, &active);
			CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
			CogitoEnsorScheme local_scheme = {0};
			bool use_local_scheme = false;
			CogitoBinScopePalette from_scheme = {0};
			CogitoBinScopePalette to_scheme = {0};
			n->bg_set = false;
			if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
			if (n->parent && n->parent->kind == COGITO_DIALOG && n->parent->bg_set) {
				n->bg = n->parent->bg;
				n->bg_set = true;
			}
			n->border_color_set = false;
			if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
			n->border_width = 0;
			n->border_width_set = false;
			if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
			if (s.has_radius) n->border_radius = s.radius;

			if (n->bin_accent_set) {
				from_scheme.primary = cogito_cached_primary;
				from_scheme.on_primary = cogito_cached_on_primary;
				from_scheme.primary_container = cogito_cached_primary_container;
				from_scheme.on_primary_container = cogito_cached_on_primary_container;
				from_scheme.secondary = cogito_cached_secondary;
				from_scheme.on_secondary = cogito_cached_on_secondary;
				from_scheme.secondary_container = cogito_cached_secondary_container;
				from_scheme.on_secondary_container = cogito_cached_on_secondary_container;
				from_scheme.tertiary = cogito_cached_tertiary;
				from_scheme.on_tertiary = cogito_cached_on_tertiary;
				from_scheme.tertiary_container = cogito_cached_tertiary_container;
				from_scheme.on_tertiary_container = cogito_cached_on_tertiary_container;
				from_scheme.error = cogito_cached_error;
				from_scheme.on_error = cogito_cached_on_error;
				from_scheme.error_container = cogito_cached_error;
				from_scheme.on_error_container = cogito_cached_on_error;
				from_scheme.background = cogito_cached_surface;
				from_scheme.on_background = cogito_cached_on_surface;
				from_scheme.surface = cogito_cached_surface;
				from_scheme.on_surface = cogito_cached_on_surface;
				from_scheme.surface_tint = cogito_cached_primary;
				from_scheme.surface_container_lowest = cogito_cached_surface;
				from_scheme.surface_container_low = cogito_cached_surface;
				from_scheme.surface_container = cogito_cached_surface_container;
				from_scheme.surface_container_high = cogito_cached_surface_container_high;
				from_scheme.surface_container_highest = cogito_cached_surface_container_high;
				from_scheme.outline = cogito_cached_outline;
				from_scheme.outline_variant = cogito_cached_outline_variant;
				from_scheme.inverse_surface = cogito_cached_inverse_surface;
				from_scheme.inverse_on_surface = cogito_cached_inverse_on_surface;
				from_scheme.inverse_primary = cogito_cached_primary;
				from_scheme.surface_variant = cogito_cached_surface_container_high;
				from_scheme.on_surface_variant = cogito_cached_on_surface_variant;
				double h = 0.0;
				double c = 0.0;
				double t = 0.0;
				cogito_rgb_to_hct(n->bin_accent_color, &h, &c, &t);
				int variant = cogito_theme_ensor_variant;
				if (variant < COGITO_ENSOR_DEFAULT || variant > COGITO_ENSOR_CONTENT) {
					variant = COGITO_ENSOR_DEFAULT;
				}
				local_scheme = cogito_ensor_build_dynamic_scheme(
					cogito_theme_dark,
					h,
					c,
					t,
					(CogitoEnsorVariant)variant,
					cogito_theme_contrast
				);
				to_scheme.primary = local_scheme.primary;
				to_scheme.on_primary = local_scheme.on_primary;
				to_scheme.primary_container = local_scheme.primary_container;
				to_scheme.on_primary_container = local_scheme.on_primary_container;
				to_scheme.secondary = local_scheme.secondary;
				to_scheme.on_secondary = local_scheme.on_secondary;
				to_scheme.secondary_container = local_scheme.secondary_container;
				to_scheme.on_secondary_container = local_scheme.on_secondary_container;
				to_scheme.tertiary = local_scheme.tertiary;
				to_scheme.on_tertiary = local_scheme.on_tertiary;
				to_scheme.tertiary_container = local_scheme.tertiary_container;
				to_scheme.on_tertiary_container = local_scheme.on_tertiary_container;
				to_scheme.error = local_scheme.error;
				to_scheme.on_error = local_scheme.on_error;
				to_scheme.error_container = local_scheme.error_container;
				to_scheme.on_error_container = local_scheme.on_error_container;
				to_scheme.background = local_scheme.background;
				to_scheme.on_background = local_scheme.on_background;
				to_scheme.surface = local_scheme.surface;
				to_scheme.on_surface = local_scheme.on_surface;
				to_scheme.surface_tint = local_scheme.surface_tint;
				to_scheme.surface_container_lowest = local_scheme.surface_container_lowest;
				to_scheme.surface_container_low = local_scheme.surface_container_low;
				to_scheme.surface_container = local_scheme.surface_container;
				to_scheme.surface_container_high = local_scheme.surface_container_high;
				to_scheme.surface_container_highest = local_scheme.surface_container_highest;
				to_scheme.inverse_surface = local_scheme.inverse_surface;
				to_scheme.inverse_on_surface = local_scheme.inverse_on_surface;
				to_scheme.inverse_primary = local_scheme.inverse_primary;
				to_scheme.outline = local_scheme.outline;
				to_scheme.outline_variant = local_scheme.outline_variant;
				to_scheme.surface_variant = local_scheme.surface_variant;
				to_scheme.on_surface_variant = local_scheme.on_surface_variant;
				use_local_scheme = true;
				n->bg = local_scheme.surface_container;
				n->bg_set = true;
				if (n->border_width_set && n->border_width > 0 && !n->border_color_set) {
					n->border_color = local_scheme.outline_variant;
					n->border_color_set = true;
				}
			}

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
			bool had_bin_scope = cogito_bin_scope_active;
			CogitoBinScopePalette prev_from_scheme = cogito_bin_scope_from_scheme;
			CogitoBinScopePalette prev_to_scheme = cogito_bin_scope_to_scheme;
			bool palette_pushed = false;
			if (use_local_scheme) {
				cogito_bin_scope_active = true;
				cogito_bin_scope_from_scheme = from_scheme;
				cogito_bin_scope_to_scheme = to_scheme;
				palette_pushed = cogito_theme_palette_scope_push_scheme(&local_scheme);
			}
			for (size_t i = 0; i < n->len; i++) {
				cogito_draw_node(n->children[i]);
			}
			if (palette_pushed) cogito_theme_palette_scope_pop();
			cogito_bin_scope_active = had_bin_scope;
			cogito_bin_scope_from_scheme = prev_from_scheme;
			cogito_bin_scope_to_scheme = prev_to_scheme;
			break;
		}
