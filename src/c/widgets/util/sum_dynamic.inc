// Dynamic theme generator
// Builds SUM source from accent color, dark/light mode, and Ensor scheme variant.

// Set the Ensor scheme variant and re-apply the current theme.
static void cogito_app_set_ensor_variant(ErgoVal app, ErgoVal variant) {
  int v = ergo_as_int(variant);
  if (v < 0) v = 0;
  if (v > 5) v = 5;
  if (v == cogito_theme_ensor_variant) return;
  cogito_theme_request_window_reveal();
  cogito_theme_ensor_variant = v;
  if (!cogito_active_window) {
    cogito_theme_refresh_deferred = true;
    return;
  }
  CogitoColor accent = cogito_app_effective_accent(app);
  cogito_theme_refresh_deferred = false;
  cogito_apply_theme(cogito_theme_dark, accent);
}

// Set theme contrast in [-1, 1] and re-apply current theme.
static void cogito_app_set_contrast(ErgoVal app, ErgoVal contrast) {
  double v = ergo_as_float(contrast);
  if (v < -1.0) v = -1.0;
  if (v > 1.0) v = 1.0;
  if (fabs(v - cogito_theme_contrast) < 0.000001) return;
  cogito_theme_contrast = v;
  if (!cogito_active_window) {
    cogito_theme_refresh_deferred = true;
    return;
  }
  CogitoColor accent = cogito_app_effective_accent(app);
  cogito_theme_refresh_deferred = false;
  cogito_apply_theme(cogito_theme_dark, accent);
}

static void cogito_sum_append(char* dst, size_t dst_cap, size_t* dst_off, const char* fmt, ...) {
  if (!dst || !dst_off || *dst_off >= dst_cap) return;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(dst + *dst_off, dst_cap - *dst_off, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  if ((size_t)n >= dst_cap - *dst_off) {
    *dst_off = dst_cap - 1;
  } else {
    *dst_off += (size_t)n;
  }
}

static inline double cogito_dynsum_now(void) {
#if defined(CLOCK_MONOTONIC)
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
  }
#endif
#if defined(TIME_UTC)
  struct timespec ts_utc;
  if (timespec_get(&ts_utc, TIME_UTC) == TIME_UTC) {
    return (double)ts_utc.tv_sec + (double)ts_utc.tv_nsec / 1000000000.0;
  }
#endif
  return 0.0;
}

static void sum_build_dynamic(bool dark, CogitoColor accent, char* out, size_t cap) {
  const char* profile_env = getenv("COGITO_THEME_PROFILE");
  bool profile = profile_env && profile_env[0] && profile_env[0] != '0';
  if (!profile) {
    const char* startup_profile_env = getenv("COGITO_STARTUP_PROFILE");
    profile = startup_profile_env && startup_profile_env[0] && startup_profile_env[0] != '0';
  }
  double t0 = profile ? cogito_dynsum_now() : 0.0;
  double t_after_scheme = t0;

  size_t off = 0;
  if (cap == 0) return;
  out[0] = 0;

  double h = 0.0, c = 0.0, t = 0.0;
  cogito_rgb_to_hct(accent, &h, &c, &t);

  // Build scheme using Ensor dynamic scheme
  CogitoEnsorScheme scheme = cogito_ensor_build_dynamic_scheme(dark, h, c, t, cogito_theme_ensor_variant, cogito_theme_contrast);
  t_after_scheme = profile ? cogito_dynsum_now() : t_after_scheme;

  // Cache tertiary colors for widgets to use (SUM path)
  cogito_cache_tertiary_colors(&scheme);

  // Hex buffer per color
  char p[8], op[8], pc[8], opc[8];
  char pf[8], pfd[8], opf[8], opfv[8];
  char sec[8], osec[8], secc[8], osecc[8];
  char sf[8], sfd[8], osf[8], osfv[8];
  char tert[8], otert[8], tertc[8], otertc[8];
  char tf[8], tfd[8], otf[8], otfv[8];
  char err[8], oerr[8], errc[8], oerrc[8];
  char bg[8], obg[8], surf[8], surft[8], ons[8];
  char outl[8], outlv[8];
  char surfvar[8], osurfvar[8];
  char surfdim[8], surfbrt[8];
  char sclo[8], scl[8], sc[8], sch[8], schi[8];
  char invsrf[8], invons[8], invp[8];
  char shad[8], scrm[8];

  cogito_color_to_hex(scheme.primary,              p);
  cogito_color_to_hex(scheme.on_primary,           op);
  cogito_color_to_hex(scheme.primary_container,    pc);
  cogito_color_to_hex(scheme.on_primary_container, opc);
  cogito_color_to_hex(scheme.primary_fixed,              pf);
  cogito_color_to_hex(scheme.primary_fixed_dim,          pfd);
  cogito_color_to_hex(scheme.on_primary_fixed,           opf);
  cogito_color_to_hex(scheme.on_primary_fixed_variant,   opfv);
  cogito_color_to_hex(scheme.secondary,              sec);
  cogito_color_to_hex(scheme.on_secondary,           osec);
  cogito_color_to_hex(scheme.secondary_container,    secc);
  cogito_color_to_hex(scheme.on_secondary_container, osecc);
  cogito_color_to_hex(scheme.secondary_fixed,            sf);
  cogito_color_to_hex(scheme.secondary_fixed_dim,        sfd);
  cogito_color_to_hex(scheme.on_secondary_fixed,         osf);
  cogito_color_to_hex(scheme.on_secondary_fixed_variant, osfv);
  cogito_color_to_hex(scheme.tertiary,              tert);
  cogito_color_to_hex(scheme.on_tertiary,           otert);
  cogito_color_to_hex(scheme.tertiary_container,    tertc);
  cogito_color_to_hex(scheme.on_tertiary_container, otertc);
  cogito_color_to_hex(scheme.tertiary_fixed,            tf);
  cogito_color_to_hex(scheme.tertiary_fixed_dim,        tfd);
  cogito_color_to_hex(scheme.on_tertiary_fixed,         otf);
  cogito_color_to_hex(scheme.on_tertiary_fixed_variant, otfv);
  cogito_color_to_hex(scheme.error,              err);
  cogito_color_to_hex(scheme.on_error,           oerr);
  cogito_color_to_hex(scheme.error_container,    errc);
  cogito_color_to_hex(scheme.on_error_container, oerrc);
  cogito_color_to_hex(scheme.background,         bg);
  cogito_color_to_hex(scheme.on_background,      obg);
  cogito_color_to_hex(scheme.surface,            surf);
  cogito_color_to_hex(scheme.surface_tint,       surft);
  cogito_color_to_hex(scheme.on_surface,         ons);
  cogito_color_to_hex(scheme.outline,            outl);
  cogito_color_to_hex(scheme.outline_variant,    outlv);
  cogito_color_to_hex(scheme.surface_variant,    surfvar);
  cogito_color_to_hex(scheme.on_surface_variant, osurfvar);
  cogito_color_to_hex(scheme.surface_dim,                  surfdim);
  cogito_color_to_hex(scheme.surface_bright,               surfbrt);
  cogito_color_to_hex(scheme.surface_container_lowest,     sclo);
  cogito_color_to_hex(scheme.surface_container_low,        scl);
  cogito_color_to_hex(scheme.surface_container,            sc);
  cogito_color_to_hex(scheme.surface_container_high,       sch);
  cogito_color_to_hex(scheme.surface_container_highest,    schi);
  cogito_color_to_hex(scheme.inverse_surface,    invsrf);
  cogito_color_to_hex(scheme.inverse_on_surface, invons);
  cogito_color_to_hex(scheme.inverse_primary,    invp);
  cogito_color_to_hex(scheme.shadow,             shad);
  cogito_color_to_hex(scheme.scrim,              scrm);

  // === SUM output ===
  cogito_sum_append(out, cap, &off,
  "*\n  color: %s\n  font-size: 14\n", ons);

  cogito_sum_append(out, cap, &off,
  ".surface\n  background: %s\n  color: %s\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  ".surface-container-high\n  background: %s\n  color: %s\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  ".surface-container-highest\n  background: %s\n  color: %s\n", schi, ons);
  cogito_sum_append(out, cap, &off,
  ".surface-container-low\n  background: %s\n  color: %s\n", scl, ons);
  cogito_sum_append(out, cap, &off,
  ".surface-container-lowest\n  background: %s\n  color: %s\n", sclo, ons);
  cogito_sum_append(out, cap, &off,
  ".surface-container\n  background: %s\n  color: %s\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  ".surface-bright\n  background: %s\n  color: %s\n", surfbrt, ons);
  cogito_sum_append(out, cap, &off,
  ".surface-dim\n  background: %s\n  color: %s\n", surfdim, ons);
  cogito_sum_append(out, cap, &off,
  ".surface-variant\n  background: %s\n  color: %s\n", surfvar, osurfvar);

  cogito_sum_append(out, cap, &off,
  ".inverse-primary\n  color: %s\n", invp);
  cogito_sum_append(out, cap, &off,
  ".inverse-primary-fill\n  background: %s\n  color: %s\n", invp, p);
  cogito_sum_append(out, cap, &off,
  ".inverse-surface\n  background: %s\n  color: %s\n", invsrf, invons);
  cogito_sum_append(out, cap, &off,
  ".on-inverse-surface\n  background: %s\n  color: %s\n", invons, invsrf);

  cogito_sum_append(out, cap, &off,
  ".error\n  background: %s\n  color: %s\n", err, oerr);
  cogito_sum_append(out, cap, &off,
  ".on-error\n  background: %s\n  color: %s\n", oerr, err);
  cogito_sum_append(out, cap, &off,
  ".error-container\n  background: %s\n  color: %s\n", errc, oerrc);
  cogito_sum_append(out, cap, &off,
  ".on-error-container\n  background: %s\n  color: %s\n", oerrc, errc);

  cogito_sum_append(out, cap, &off,
  ".primary\n  background: %s\n  color: %s\n", p, op);
  cogito_sum_append(out, cap, &off,
  ".on-primary\n  background: %s\n  color: %s\n", op, p);
  cogito_sum_append(out, cap, &off,
  ".primary-container\n  background: %s\n  color: %s\n", pc, opc);
  cogito_sum_append(out, cap, &off,
  ".on-primary-container\n  background: %s\n  color: %s\n", opc, pc);
  cogito_sum_append(out, cap, &off,
  ".primary-fixed\n  background: %s\n  color: %s\n", pf, opf);
  cogito_sum_append(out, cap, &off,
  ".primary-fixed-dim\n  background: %s\n  color: %s\n", pfd, opfv);
  cogito_sum_append(out, cap, &off,
  ".on-primary-fixed\n  background: %s\n  color: %s\n", opf, pf);
  cogito_sum_append(out, cap, &off,
  ".on-primary-fixed-variant\n  background: %s\n  color: %s\n", opfv, pfd);
  cogito_sum_append(out, cap, &off,
  ".secondary\n  background: %s\n  color: %s\n", sec, osec);
  cogito_sum_append(out, cap, &off,
  ".on-secondary\n  background: %s\n  color: %s\n", osec, sec);
  cogito_sum_append(out, cap, &off,
  ".secondary-container\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  ".on-secondary-container\n  background: %s\n  color: %s\n", osecc, secc);
  cogito_sum_append(out, cap, &off,
  ".secondary-fixed\n  background: %s\n  color: %s\n", sf, osf);
  cogito_sum_append(out, cap, &off,
  ".secondary-fixed-dim\n  background: %s\n  color: %s\n", sfd, osfv);
  cogito_sum_append(out, cap, &off,
  ".on-secondary-fixed\n  background: %s\n  color: %s\n", osf, sf);
  cogito_sum_append(out, cap, &off,
  ".on-secondary-fixed-variant\n  background: %s\n  color: %s\n", osfv, sfd);
  cogito_sum_append(out, cap, &off,
  ".tertiary\n  background: %s\n  color: %s\n", tert, otert);
  cogito_sum_append(out, cap, &off,
  ".on-tertiary\n  background: %s\n  color: %s\n", otert, tert);
  cogito_sum_append(out, cap, &off,
  ".tertiary-container\n  background: %s\n  color: %s\n", tertc, otertc);
  cogito_sum_append(out, cap, &off,
  ".on-tertiary-container\n  background: %s\n  color: %s\n", otertc, tertc);
  cogito_sum_append(out, cap, &off,
  ".tertiary-fixed\n  background: %s\n  color: %s\n", tf, otf);
  cogito_sum_append(out, cap, &off,
  ".tertiary-fixed-dim\n  background: %s\n  color: %s\n", tfd, otfv);
  cogito_sum_append(out, cap, &off,
  ".on-tertiary-fixed\n  background: %s\n  color: %s\n", otf, tf);
  cogito_sum_append(out, cap, &off,
  ".on-tertiary-fixed-variant\n  background: %s\n  color: %s\n", otfv, tfd);

  cogito_sum_append(out, cap, &off,
  ".on-surface\n  background: %s\n  color: %s\n", ons, surf);
  cogito_sum_append(out, cap, &off,
  ".on-surface-variant\n  background: %s\n  color: %s\n", osurfvar, surfvar);

  cogito_sum_append(out, cap, &off,
  ".outline\n  border-color: %s\n", outl);
  cogito_sum_append(out, cap, &off,
  ".outline-variant\n  border-color: %s\n", outlv);
  cogito_sum_append(out, cap, &off,
  ".outline-fill\n  background: %s\n  color: %s\n", outl, surf);
  cogito_sum_append(out, cap, &off,
  ".outline-variant-fill\n  background: %s\n  color: %s\n", outlv, surf);

  cogito_sum_append(out, cap, &off,
  ".scrim\n  background: %s\n", scrm);
  cogito_sum_append(out, cap, &off,
  ".shadow\n  background: %s\n", scrm);

  cogito_sum_append(out, cap, &off,
  ".big-display\n  color: %s\n  font-family: serif\n  font-size: 56\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".display\n  color: %s\n  font-family: serif\n  font-size: 36\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".view-title\n  color: %s\n  font-size: 24\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".view-subtitle\n  color: %s\n  font-size: 18\n  font-weight: bold\n", ons);
  cogito_sum_append(out, cap, &off,
  ".heading\n  color: %s\n  font-size: 16\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".content-title\n  color: %s\n  font-size: 16\n  font-weight: bold\n", ons);
  cogito_sum_append(out, cap, &off,
  ".content-subtitle\n  color: %s\n  font-size: 14\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".title\n  color: %s\n  font-size: 24\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".subtitle\n  color: %s\n  font-size: 18\n  font-weight: bold\n", ons);
  cogito_sum_append(out, cap, &off,
  ".body\n  color: %s\n  font-size: 14\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".caption\n  color: %s\n  font-size: 12\n  font-weight: bold\n", ons);

  cogito_sum_append(out, cap, &off,
  ".tabular\n  font-variant-numeric: tnum\n");
  cogito_sum_append(out, cap, &off,
  ".monospace\n  font-variant-monospace: monospace\n");

  cogito_sum_append(out, cap, &off,
  "about_window\n  background: %s\n  color: %s\n  border-radius: 26\n  padding: 24\n", sch, ons);
  cogito_sum_append(out, cap, &off,
  "active_indicator\n  color: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "appbar .iconbtn\n  background: transparent\n");
  cogito_sum_append(out, cap, &off,
  "appbar .window-close:active\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-close:hover\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-close\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-max:active\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-max:hover\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-max\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-min:active\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-min:hover\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-min\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar.sidebar\n  background: %s\n  color: %s\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "appbar.view\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "appbar\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "avatar\n  background: %s\n  color: %s\n  min-width: 64\n  min-height: 64\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "badge\n  background: %s\n  color: %s\n  font-size: 11\n  min-width: 10\n  min-height: 10\n", err, oerr);
  cogito_sum_append(out, cap, &off,
  "banner:selection\n  color: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "banner\n  background: %s\n  color: %s\n  padding: 16\n  min-height: 52\n  transition: 200 standard\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "bottom_sheet\n  background: %s\n  color: %s\n  border-radius: 28 28 0 0\n  padding: 16\n  transition: 300 emphasized\n", scl, ons);
  cogito_sum_append(out, cap, &off,
  "toolbar.floating_compact\n  border-radius: 999\n  elevation: 3\n  padding: 8\n");
  cogito_sum_append(out, cap, &off,
  "toolbar.floating\n  border-radius: 999\n  elevation: 3\n  padding: 8\n");
  cogito_sum_append(out, cap, &off,
  "toolbar\n  background: %s\n  color: %s\n  min-height: 64\n  padding: 8 16\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "bottom-nav:selection\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "bottom-nav\n  background: %s\n  color: %s\n  min-height: 80\n  padding: 0 8\n  transition: 220 standard\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "bottom-sheet\n  background: %s\n  color: %s\n  border-radius: 28 28 0 0\n  padding: 16\n  transition: 300 emphasized\n", scl, ons);
  cogito_sum_append(out, cap, &off,
  "side-sheet\n  background: %s\n  color: %s\n  border-radius: 16 0 0 16\n  padding: 16 24 24 24\n  transition: 300 emphasized\n", scl, ons);
  cogito_sum_append(out, cap, &off,
  "side-sheet.modal\n  background: %s\n  color: %s\n  border-radius: 16 0 0 16\n  padding: 24 24 24 24\n  transition: 300 emphasized\n", scl, ons);
  cogito_sum_append(out, cap, &off,
  "side-sheet.inset\n  background: %s\n  color: %s\n  border-radius: 16\n  padding: 16 24 24 24\n  transition: 300 emphasized\n", scl, ons);
  cogito_sum_append(out, cap, &off,
  "side-sheet.sidebar\n  background: %s\n  color: %s\n  border-radius: 0\n  border: 1 solid %s\n  padding: 24 24 24 24\n  transition: 300 emphasized\n", scl, ons, outl);
  cogito_sum_append(out, cap, &off,
  "button:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "button:focus, iconbtn:focus, checkbox:focus, switch:focus, slider:focus, dropdown:focus, chip:focus, buttongroup:focus, stepper:focus, fab:focus, nav_rail:focus, bottom_nav:focus, datepicker:focus, colorpicker:focus, timepicker:focus\n  border-color: %s\n  border-width: 2\n", p);
  cogito_sum_append(out, cap, &off,
  "button.outlined:disabled\n  background: transparent\n  color: %s52\n  border-color: %s52\n", ons, ons, ons);
  cogito_sum_append(out, cap, &off,
  "button.outlined\n  background: transparent\n  color: %s\n  border: 1 solid %s\n", p, outl);
  cogito_sum_append(out, cap, &off,
  "button.text:disabled\n  background: transparent\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "button.text\n  background: transparent\n  color: %s\n  transition: 150 standard\n", p);
  cogito_sum_append(out, cap, &off,
  "button.toolbar_format_active\n  background: %s\n  color: %s\n  min-width: 56\n  min-height: 56\n  padding: 0 16\n  box-shadow: none\n", p, op);
  cogito_sum_append(out, cap, &off,
  "button.toolbar_format\n  background: transparent\n  color: %s\n  min-width: 40\n  min-height: 48\n  padding: 0 8\n  box-shadow: none\n", p);
  cogito_sum_append(out, cap, &off,
  "button\n  background: %s\n  color: %s\n  font-weight: bold\n  transition: 180 standard\n", p, op);
  cogito_sum_append(out, cap, &off,
  "card:hover\n  elevation: 2\n");
  cogito_sum_append(out, cap, &off,
  "card\n  background: %s\n  color: %s\n  border-radius: 16\n  padding: 16\n  transition: 200 standard\n", scl, ons);
  cogito_sum_append(out, cap, &off,
  "carousel_item\n  background: %s\n  color: %s\n  border-radius: 26\n  padding: 16\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "carousel_item\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "carousel\n  background: %s\n  color: %s\n  min-height: 240\n  min-width: 400\n  border-radius: 26\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "checkbox .box:active\n  border-color: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "checkbox .box:hover\n  border-color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "checkbox .box\n  background: transparent\n  border-radius: 4\n  border: 2 solid %s\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "checkbox .check:active\n  color: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "checkbox .check:hover\n  color: %s\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "checkbox .check\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "checkbox:checked .box\n  background: %s\n  border: 0 none %s\n", p, p);
  cogito_sum_append(out, cap, &off,
  "checkbox:checked .check\n  color: %s\n", op);
  cogito_sum_append(out, cap, &off,
  "checkbox:checked\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "checkbox:disabled\n  color: %s52\n", ons);
  cogito_sum_append(out, cap, &off,
  "checkbox\n  color: %s\n  min-height: 24\n  min-width: 24\n  transition: 120 standard\n", ons);
  cogito_sum_append(out, cap, &off,
  "chip:checked\n  background: %s\n  color: %s\n  border: 0 none %s\n", secc, osecc, secc);
  cogito_sum_append(out, cap, &off,
  "chip:disabled\n  background: %s29\n  color: %s52\n  border-color: %s52\n", ons, ons, ons);
  cogito_sum_append(out, cap, &off,
  "chip\n  background: transparent\n  color: %s\n  border-radius: 8\n  min-height: 32\n  padding: 0 16\n  border: 1 solid %s\n  transition: 150 standard\n", ons, outlv);
  cogito_sum_append(out, cap, &off,
  "content_list label\n  background: transparent\n");
  cogito_sum_append(out, cap, &off,
  "content_list\n  background: %s\n  color: %s\n  padding: 0 16\n  min-height: 80\n", surfvar, ons);
  cogito_sum_append(out, cap, &off,
  "content_list=first\n  border-radius: 12 12 8 8\n");
  cogito_sum_append(out, cap, &off,
  "content_list=last\n  border-radius: 8 8 12 12\n");
  cogito_sum_append(out, cap, &off,
  "content_list=middle\n  border-radius: 8\n");
  cogito_sum_append(out, cap, &off,
  "content_list=single\n  border-radius: 12\n");
  cogito_sum_append(out, cap, &off,
  "datepicker, .datepicker\n  background: %s\n  color: %s\n  border: 1 solid %s\n  border-radius: 8\n  min-height: 56\n  min-width: 220\n  transition: 180 standard\n", sc, ons, outl);
  cogito_sum_append(out, cap, &off,
  "datepicker:hover, .datepicker:hover\n  background: %s\n", surfvar);
  cogito_sum_append(out, cap, &off,
  "datepicker:selection, .datepicker:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "dialog\n  background: %s\n  color: %s\n  border-radius: 28\n  min-width: 280\n  max-width: 560\n  min-height: 240\n  padding: 24\n", sch, ons);
  cogito_sum_append(out, cap, &off,
  "dialogslot\n  background: %s66\n  color: %s\n", scrm, ons);
  cogito_sum_append(out, cap, &off,
  "divider\n  background: %s\n  min-height: 1\n", outlv);
  cogito_sum_append(out, cap, &off,
  "dropdown:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "dropdown:hover\n  background: %s\n", surfvar);
  cogito_sum_append(out, cap, &off,
  "dropdown\n  background: %s\n  color: %s\n  border-radius: 4 4 0 0\n  min-height: 56\n  padding: 8 16\n  transition: 180 standard\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "empty_page\n  background: transparent\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "fab:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "fab.primary\n  background: %s\n  color: %s\n", pc, opc);
  cogito_sum_append(out, cap, &off,
  "fab.secondary\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "fab.tertiary\n  background: %s\n  color: %s\n", tertc, otertc);
  cogito_sum_append(out, cap, &off,
  "fab.surface\n  background: %s\n  color: %s\n", sch, p);
  cogito_sum_append(out, cap, &off,
  "fab\n  background: %s\n  color: %s\n  elevation: 2\n  transition: 180 standard\n", p, op);
  cogito_sum_append(out, cap, &off,
  "fixed\n  background: transparent\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "grid:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "grid\n  background: %s\n  color: %s\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "hstack\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "iconbtn:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "iconbtn.filled\n  background: %s\n  color: %s\n  transition: 150 standard\n", p, op);
  cogito_sum_append(out, cap, &off,
  "iconbtn.iconic\n  background: transparent\n  color: %s\n  transition: 150 standard\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "iconbtn.outlined\n  background: transparent\n  color: %s\n  border: 1 solid %s\n  transition: 150 standard\n", osurfvar, outl);
  cogito_sum_append(out, cap, &off,
  "iconbtn.tonal\n  background: %s\n  color: %s\n  transition: 150 standard\n", pc, opc);
  cogito_sum_append(out, cap, &off,
  "iconbtn\n  background: transparent\n  color: %s\n  transition: 150 standard\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "image\n  background: transparent\n");
  cogito_sum_append(out, cap, &off,
  "label.carousel-item-label\n  font-size: 21\n  font-weight: bold\n  color: #ffffff\n");
  cogito_sum_append(out, cap, &off,
  "label\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "list:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "list\n  background: transparent\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "menu item\n  border-radius: 0\n  padding: 8 12\n  min-height: 42\n");
  cogito_sum_append(out, cap, &off,
  "menu_button:hover\n  background: %s\n", surfvar);
  cogito_sum_append(out, cap, &off,
  "menu_button:active\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "menu_button:checked\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "menu_button\n  background: transparent\n  color: %s\n  padding: 8 12\n", ons);
  cogito_sum_append(out, cap, &off,
  "menu:selection\n  background: %s\n  color: %s\n", pc, opc);
  cogito_sum_append(out, cap, &off,
  "menu\n  background: %s\n  border-radius: 4\n  color: %s\n  elevation: 2\n  font-size: 14\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "nav-rail:selection\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "nav-rail\n  background: %s\n  color: %s\n  min-width: 96\n  padding: 4 0\n  transition: 220 standard\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "popover\n  background: %s\n  border-radius: 12\n  padding: 16\n  elevation: 2\n", sc);
  cogito_sum_append(out, cap, &off,
  "progress:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "progress\n  background: %s\n  border-radius: 100\n  track-height: 4\n  min-height: 12\n  min-width: 120\n  transition: 200 standard\n", outlv);
  cogito_sum_append(out, cap, &off,
  "radio .box:active\n  border-color: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "radio .box:hover\n  border-color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "radio .box\n  background: transparent\n  border-radius: 100\n  border: 2 solid %s\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "radio .check:active\n  color: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "radio .check:hover\n  color: %s\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "radio .check\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "radio:checked .box\n  background: %s\n  border: 0 none %s\n", p, p);
  cogito_sum_append(out, cap, &off,
  "radio:checked .check\n  color: %s\n", op);
  cogito_sum_append(out, cap, &off,
  "radio:checked\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "radio:disabled\n  color: %s52\n", ons);
  cogito_sum_append(out, cap, &off,
  "radio\n  color: %s\n  min-height: 24\n  min-width: 24\n  transition: 120 standard\n", ons);
  cogito_sum_append(out, cap, &off,
  "scroller.sidebar\n  background: %s\n  color: %s\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "scroller\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "searchfield:active\n  border-color: %s\n  border-width: 2\n", p);
  cogito_sum_append(out, cap, &off,
  "searchfield:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "searchfield:focus\n  border: 2 solid %s\n", p);
  cogito_sum_append(out, cap, &off,
  "searchfield:hover\n  background: %s\n  border-color: %s\n", surfvar, ons);
  cogito_sum_append(out, cap, &off,
  "searchfield:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "searchfield\n  background: %s\n  color: %s\n  border-radius: 999\n  min-height: 56\n  min-width: 240\n  padding: 6 16\n  transition: 180 standard\n", sch, ons);
  cogito_sum_append(out, cap, &off,
  "colorpicker\n  background: %s\n  color: %s\n  border-radius: 12\n  transition: 180 standard\n", sch, ons);
  cogito_sum_append(out, cap, &off,
  "colorpicker:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "buttongroup:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "buttongroup:selection\n  background: %s\n  color: %s\n", sec, osec);
  cogito_sum_append(out, cap, &off,
  "buttongroup\n  background: %s\n  color: %s\n  transition: 180 standard\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "settings_list\n  background: %s\n  color: %s\n  border-radius: 12\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "settings_page\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "settings_row\n  background: transparent\n  color: %s\n  min-height: 56\n  padding: 8 16\n", ons);
  cogito_sum_append(out, cap, &off,
  "settings_window\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "slider:disabled\n  color: %s52\n", ons);
  cogito_sum_append(out, cap, &off,
  "slider\n  background: %s\n  color: %s\n  track-height: 4\n  min-height: 48\n  min-width: 200\n  transition: 120 standard\n", surfvar, p);
  cogito_sum_append(out, cap, &off,
  "split_button\n  background: %s\n  color: %s\n", p, op);
  cogito_sum_append(out, cap, &off,
  "stepper:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "stepper\n  background: %s\n  color: %s\n  border-radius: 100\n  min-height: 42\n  min-width: 120\n  border: 1 solid %s\n  padding: 4 8\n  transition: 150 standard\n", surf, ons, outl);
  cogito_sum_append(out, cap, &off,
  "switch .knob:active\n  background: %s\n", op);
  cogito_sum_append(out, cap, &off,
  "switch .knob:hover\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "switch .knob\n  background: %s\n  min-width: 32\n  min-height: 24\n  elevation: 1\n", outl);
  cogito_sum_append(out, cap, &off,
  "switch .track:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "switch .track:hover\n  background: %s\n", outl);
  cogito_sum_append(out, cap, &off,
  "switch .track\n  background: %s\n  border: 2 solid %s\n", schi, outl);
  cogito_sum_append(out, cap, &off,
  "switch knob:active\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "switch knob:hover\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "switch knob\n  background: %s\n  min-width: 24\n  min-height: 24\n  elevation: 1\n", outl);
  cogito_sum_append(out, cap, &off,
  "switch track:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "switch track:hover\n  background: %s\n", schi);
  cogito_sum_append(out, cap, &off,
  "switch track\n  background: %s\n  border: 2 solid %s\n", schi, outl);
  cogito_sum_append(out, cap, &off,
  "switch:checked .track\n  background: %s\n  border-radius: 100\n  border: 0 none %s\n", p, p);
  cogito_sum_append(out, cap, &off,
  "switch:checked track\n  background: %s\n  border-radius: 100\n  border: 0 none %s\n", p, p);
  cogito_sum_append(out, cap, &off,
  "switch:disabled knob\n  background: %s52\n", ons);
  cogito_sum_append(out, cap, &off,
  "switch:disabled track\n  background: %s29\n", ons);
  cogito_sum_append(out, cap, &off,
  "switch:disabled\n  color: %s52\n", ons);
  cogito_sum_append(out, cap, &off,
  "switch\n  color: %s\n  min-height: 32\n  min-width: 52\n  transition: 100 standard\n", ons);
  cogito_sum_append(out, cap, &off,
  "switchbar\n  background: %s\n  color: %s\n  border: 1 solid %s\n  border-radius: 12\n  min-height: 56\n", surf, ons, outl);
  cogito_sum_append(out, cap, &off,
  "tabs:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "tabs\n  background: %s\n  color: %s\n  border-radius: 0\n  min-height: 48\n  transition: 180 standard\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "textfield:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "textfield:focus\n  border: 2 solid %s\n", p);
  cogito_sum_append(out, cap, &off,
  "textfield:hover\n  background: %s\n", surfvar);
  cogito_sum_append(out, cap, &off,
  "textfield:selection, .textfield:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "textfield\n  background: %s\n  color: %s\n  border-radius: 4 4 0 0\n  border: 0 none #000000\n  min-height: 56\n  padding: 8 16\n  transition: 180 standard\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "textview:focus\n  border: 2 solid %s\n", p);
  cogito_sum_append(out, cap, &off,
  "textview:selection, .textview:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "textview\n  background: %s\n  color: %s\n  border-radius: 8\n  min-height: 96\n  transition: 180 standard\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "timepicker:hover\n  background: %s\n", surfvar);
  cogito_sum_append(out, cap, &off,
  "timepicker:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "timepicker\n  background: %s\n  color: %s\n  border: 1 solid %s\n  border-radius: 8\n  min-height: 56\n  min-width: 220\n  transition: 180 standard\n", sc, ons, outl);
  cogito_sum_append(out, cap, &off,
  "tip_view\n  background: %s\n  color: %s\n  border-radius: 8\n  padding: 12\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "toast:selection\n  background: %s\n", invp);
  cogito_sum_append(out, cap, &off,
  "toast\n  background: %s\n  color: %s\n  border-radius: 999\n  min-height: 48\n  padding: 0 16\n  transition: 220 standard\n", invsrf, invons);
  cogito_sum_append(out, cap, &off,
  "toasts\n  padding: 12\n");
  cogito_sum_append(out, cap, &off,
  "toolbar.floating_compact\n  padding: 8\n");
  cogito_sum_append(out, cap, &off,
  "toolbar.floating\n  padding: 8\n");
  cogito_sum_append(out, cap, &off,
  "toolbar\n  background: %s\n  color: %s\n  min-height: 64\n  padding: 8 16\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "tooltip\n  background: %s\n  color: %s\n  border-radius: 6\n  font-size: 14\n  padding: 6 8\n", invsrf, invons);
  cogito_sum_append(out, cap, &off,
  "treeview\n  background: %s\n  color: %s\n  border-radius: 8\n  min-height: 180\n  min-width: 200\n  padding: 8\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "view_chooser\n  background: %s\n  color: %s\n  border-radius: 8\n  min-height: 48\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "view_dual\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "view_switcher\n  background: %s\n  color: %s\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "viewswitcher\n  background: %s\n  color: %s\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "vstack.sidebar\n  background: %s\n  color: %s\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "vstack\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "welcome_screen\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "window\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "zstack\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  ".about-window\n  background: %s\n  color: %s\n", sch, ons);
  cogito_sum_append(out, cap, &off,
  "label.about-window-title\n  color: %s\n  font-size: 28\n  font-weight: bold\n", ons);
  cogito_sum_append(out, cap, &off,
  "label.about-window-license\n  color: %s\n  font-size: 14\n", outl);
  cogito_sum_append(out, cap, &off,
  "hstack.about-window-actions\n  background: transparent\n  padding: 4 0 0 0\n");
  cogito_sum_append(out, cap, &off,
  "image.about-window-icon\n  min-width: 128\n  min-height: 128\n");
  cogito_sum_append(out, cap, &off,
  "menu_section\n  background: transparent\n");
  cogito_sum_append(out, cap, &off,
  "menu_section item\n  border-radius: 0\n  padding: 8 12\n  min-height: 42\n");
  cogito_sum_append(out, cap, &off,
  "switch .knob:checked\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "switch knob:checked\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "fab-menu\n  background: %s\n  color: %s\n", p, op);
  cogito_sum_append(out, cap, &off,
  "fab-menu.primary\n  background: %s\n  color: %s\n", pc, opc);
  cogito_sum_append(out, cap, &off,
  "fab-menu.secondary\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "fab-menu.tertiary\n  background: %s\n  color: %s\n", tertc, otertc);
  cogito_sum_append(out, cap, &off,
  "fab-menu.surface\n  background: %s\n  color: %s\n", sch, p);

  if (profile) {
    double t_end = cogito_dynsum_now();
    double ms_total = (t_end - t0) * 1000.0;
    double ms_scheme = (t_after_scheme - t0) * 1000.0;
    double ms_emit = (t_end - t_after_scheme) * 1000.0;
    fprintf(stderr,
            "[cogito-dynsum] total=%.2fms scheme=%.2fms emit=%.2fms\n",
            ms_total, ms_scheme, ms_emit);
    fflush(stderr);
  }
}
