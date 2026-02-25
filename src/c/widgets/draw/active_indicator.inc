case COGITO_ACTIVE_INDICATOR: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  CogitoColor color = s.has_text ? s.text : n->text_color;

  double now = cogito_now();
  if (n->indicator.last_time == 0.0) n->indicator.last_time = now;
  // Rotate at ~1.5 revolutions per second (forward direction)
  double dt = now - n->indicator.last_time;
  if (dt < 0) dt = 0; // Prevent negative time jumps
  n->indicator.angle += dt * 540.0; // degrees per second (positive for forward rotation)
  if (n->indicator.angle >= 360.0) n->indicator.angle -= 360.0;
  n->indicator.last_time = now;

  int cx = n->x + n->w / 2;
  int cy = n->y + n->h / 2;
  int nucleus_r = 3;
  int max_radius = (n->w < n->h ? n->w : n->h) / 2 - 4;
  if (max_radius < 12) max_radius = 12;

  // Draw nucleus with 4 circles (2 protons + 2 neutrons - helium-4)
  int nucleus_offset = 3;
  // Proton 1
  CogitoColor proton_color = color;
  proton_color.a = (uint8_t)(color.a * 0.95f);
  cogito_draw_rect(cx - nucleus_offset - nucleus_r, cy - nucleus_offset - nucleus_r, 
                   nucleus_r * 2, nucleus_r * 2, proton_color, nucleus_r);
  // Proton 2
  cogito_draw_rect(cx + nucleus_offset - nucleus_r, cy + nucleus_offset - nucleus_r, 
                   nucleus_r * 2, nucleus_r * 2, proton_color, nucleus_r);
  // Neutron 1 (slightly darker)
  CogitoColor neutron_color = color;
  neutron_color.a = (uint8_t)(color.a * 0.7f);
  cogito_draw_rect(cx + nucleus_offset - nucleus_r, cy - nucleus_offset - nucleus_r, 
                   nucleus_r * 2, nucleus_r * 2, neutron_color, nucleus_r);
  // Neutron 2
  cogito_draw_rect(cx - nucleus_offset - nucleus_r, cy + nucleus_offset - nucleus_r, 
                   nucleus_r * 2, nucleus_r * 2, neutron_color, nucleus_r);

  // Draw two concentric electron orbits (top-down view)
  int num_electrons = 2;
  for (int e = 0; e < num_electrons; e++) {
    // Each electron has its own orbit at different radius
    int electron_orbit_r = max_radius - e * 6 - 6;
    if (electron_orbit_r < 6) electron_orbit_r = 6;
    
    // Draw orbit path (circle)
    CogitoColor orbit_color = color;
    orbit_color.a = (uint8_t)(color.a * 0.1f);
    cogito_draw_rect(cx - electron_orbit_r, cy - electron_orbit_r, 
                     electron_orbit_r * 2, electron_orbit_r * 2, orbit_color, electron_orbit_r);
    
    // Draw electron on the orbit
    double electron_angle_rad = n->indicator.angle * 3.14159265358979 / 180.0;
    // Offset second electron by 180 degrees
    if (e == 1) electron_angle_rad += 3.14159265358979;
    
    float electron_x = (float)(cx + cos(electron_angle_rad) * electron_orbit_r);
    float electron_y = (float)(cy + sin(electron_angle_rad) * electron_orbit_r);
    int electron_r = 3;
    
    // Electron glow
    CogitoColor glow_color = color;
    glow_color.a = (uint8_t)(color.a * 0.1f);
    cogito_draw_rect((int)electron_x - electron_r - 1, (int)electron_y - electron_r - 1, 
                     electron_r * 2 + 2, electron_r * 2 + 2, glow_color, electron_r + 1);
    
    // Electron core
    CogitoColor electron_color = color;
    electron_color.a = color.a;
    cogito_draw_rect((int)electron_x - electron_r, (int)electron_y - electron_r, 
                     electron_r * 2, electron_r * 2, electron_color, electron_r);
  }

  // Request continuous redraw for animation
  double anim_end = now + 0.032;  // Request ~2 frames more of animation
  if (anim_end > cogito_anim_end_time) cogito_anim_end_time = anim_end;
  break;
}
