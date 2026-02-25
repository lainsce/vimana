case COGITO_BUTTON_GROUP: {
      if (n->len == 0) break;
      
      // ---- Press animation: tick springs and trigger relayout ----
      // Detect which child is being pressed
      int pressed_idx = -1;
      int mx = 0, my = 0;
      bool btn_down = false;
      bool have_mouse = cogito_pointer_query(&mx, &my, &btn_down, NULL);
      if (have_mouse && btn_down && !n->disabled) {
        for (size_t i = 0; i < n->len; i++) {
          CogitoNode* c = n->children[i];
          if (!c->disabled && cogito_hit_rect(mx, my, c->x, c->y, c->w, c->h)) {
            pressed_idx = (int)i;
            break;
          }
        }
      }
      
      // Update pressed child tracking
      n->buttongroup.pressed_child = pressed_idx;
      
      // Spring animation for width expansion (k=1400, z=0.9)
      double dt = cogito_spring_dt;
      double k_spring = 1400.0;
      double z_spring = 0.9;
      bool any_anim_active = false;
      
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        double target_mult = 1.0;
        if ((int)i == pressed_idx) {
          target_mult = 1.15; // 15% expansion
        } else if (pressed_idx >= 0 && ((int)i == pressed_idx - 1 || (int)i == pressed_idx + 1)) {
          // Adjacent buttons shrink proportionally
          int num_adjacent = 0;
          if (pressed_idx > 0) num_adjacent++;
          if (pressed_idx < (int)n->len - 1) num_adjacent++;
          if (num_adjacent > 0) {
            // Total expansion is 15%, distribute shrink among adjacent
            target_mult = 1.0 - (0.15 / num_adjacent);
          }
        }
        
        // Initialize spring to 1.0 (neutral) if not yet initialized
        // This ensures animation starts from neutral position, not target
        if (!c->spring_w.inited) {
          cogito_spring_snap(&c->spring_w, 1.0);
        }
        
        // Tick spring animation
        cogito_spring_tick(&c->spring_w, target_mult, k_spring, z_spring, dt);
        
        // Check if animation is active
        if (cogito_spring_active(&c->spring_w, target_mult)) {
          any_anim_active = true;
        }
      }
      
      // If animation is active, extend animation time and trigger relayout
      if (any_anim_active) {
        double end_time = cogito_now() + 0.6;
        if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
        // Trigger relayout for next frame
        cogito_window_relayout(cogito_node_window(n));
      }
      
      // Draw children
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;
    }
