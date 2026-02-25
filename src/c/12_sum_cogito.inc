static int cogito_sum_parse_transition_ease(const char* ease) {
  if (!ease || !ease[0]) return -1;
  if (strcmp(ease, "linear") == 0) return 0;
  if (strcmp(ease, "standard") == 0) return 1;
  if (strcmp(ease, "ease") == 0) return 1;
  if (strcmp(ease, "ease-in") == 0) return 2;
  if (strcmp(ease, "ease-out") == 0) return 3;
  if (strcmp(ease, "ease-in-out") == 0) return 4;
  if (strcmp(ease, "emphasized") == 0) return 5;
  if (strcmp(ease, "emphasized-out") == 0) return 6;
  return 0;
}

static bool cogito_sum_read_duration_ms(const char** p, int* out_ms) {
  if (!p || !*p || !out_ms) return false;
  const char* s = *p;
  cogito_css_skip_ws(&s);
  char* end = NULL;
  double v = strtod(s, &end);
  if (end == s) return false;
  s = end;
  cogito_css_skip_ws(&s);
  bool seconds = false;
  if (s[0] == 'm' && s[1] == 's') {
    s += 2;
  } else if (s[0] == 's' && !isalnum((unsigned char)s[1]) && s[1] != '_' && s[1] != '-') {
    seconds = true;
    s += 1;
  }
  int ms = (int)lround(seconds ? (v * 1000.0) : v);
  if (ms < 0) ms = 0;
  *out_ms = ms;
  *p = s;
  return true;
}

__attribute__((unused))
static bool cogito_sum_read_bool(const char** p, bool* out) {
  if (!p || !*p || !out) return false;
  const char* s = *p;
  cogito_css_skip_ws(&s);
  char ident[24] = {0};
  if (cogito_css_read_ident(&s, ident, sizeof(ident))) {
    if (strcmp(ident, "true") == 0 || strcmp(ident, "yes") == 0 || strcmp(ident, "on") == 0) {
      *out = true;
      *p = s;
      return true;
    }
    if (strcmp(ident, "false") == 0 || strcmp(ident, "no") == 0 || strcmp(ident, "off") == 0) {
      *out = false;
      *p = s;
      return true;
    }
  }
  double v = 0.0;
  if (cogito_css_read_number(&s, &v)) {
    *out = v != 0.0;
    *p = s;
    return true;
  }
  return false;
}

void cogito_sum_apply_property(CogitoStyle* style, const char* prop, const char** p, bool selection_mode) {
  if (selection_mode) {
    if (strcmp(prop, "background") == 0 || strcmp(prop, "background-color") == 0 ||
        strcmp(prop, "selection-color") == 0 || strcmp(prop, "selection-background") == 0 ||
        strcmp(prop, "highlight-color") == 0) {
      CogitoColor c;
      if (cogito_css_parse_color(p, &c)) { style->has_selection = true; style->selection = c; }
      return;
    }
  } else if (strcmp(prop, "selection-color") == 0 || strcmp(prop, "selection-background") == 0 || strcmp(prop, "highlight-color") == 0) {
    return;
  }
  if (strcmp(prop, "background") == 0 || strcmp(prop, "background-color") == 0) {
    CogitoColor c;
    if (cogito_css_parse_color(p, &c)) { style->has_bg = true; style->bg = c; }
    return;
  }
  if (strcmp(prop, "icon-size") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_icon_size = true; style->icon_size = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "icon-color") == 0 || strcmp(prop, "icon-tint") == 0) {
    CogitoColor c;
    if (cogito_css_parse_color(p, &c)) { style->has_icon_color = true; style->icon_color = c; }
    return;
  }
  if (strcmp(prop, "transition") == 0) {
    const char* s = *p;
    for (int i = 0; i < 2; i++) {
      int ms = 0;
      if (cogito_sum_read_duration_ms(&s, &ms)) {
        style->has_transition_ms = true;
        style->transition_ms = ms;
        continue;
      }
      cogito_css_skip_ws(&s);
      char ease[32] = {0};
      if (cogito_css_read_ident(&s, ease, sizeof(ease))) {
        style->has_transition_ease = true;
        style->transition_ease = cogito_sum_parse_transition_ease(ease);
        continue;
      }
      break;
    }
    *p = s;
    return;
  }
  if (strcmp(prop, "transition-duration") == 0) {
    int ms = 0;
    if (cogito_sum_read_duration_ms(p, &ms)) {
      style->has_transition_ms = true;
      style->transition_ms = ms;
    }
    return;
  }
  if (strcmp(prop, "transition-easing") == 0 || strcmp(prop, "transition-timing-function") == 0) {
    const char* s = *p;
    cogito_css_skip_ws(&s);
    char ease[32] = {0};
    cogito_css_read_ident(&s, ease, sizeof(ease));
    *p = s;
    if (ease[0]) {
      style->has_transition_ease = true;
      style->transition_ease = cogito_sum_parse_transition_ease(ease);
    }
    return;
  }
  if (strcmp(prop, "color") == 0 || strcmp(prop, "text-color") == 0) {
    CogitoColor c;
    if (cogito_css_parse_color(p, &c)) { style->has_text = true; style->text = c; }
    return;
  }
  if (strcmp(prop, "border-color") == 0) {
    CogitoColor c;
    if (cogito_css_parse_color(p, &c)) { style->has_border = true; style->border = c; }
    return;
  }
  if (strcmp(prop, "border-width") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_border_width = true; style->border_width = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "border-radius") == 0 || strcmp(prop, "radius") == 0) {
    int vals[4] = {0, 0, 0, 0};
    int count = cogito_css_parse_ints(p, vals, 4);
    if (count == 1) {
      cogito_style_set_radius(style, vals[0], vals[0], vals[0], vals[0]);
      style->has_radius = true;
      style->radius = vals[0];
    } else if (count == 2) {
      cogito_style_set_radius(style, vals[0], vals[1], vals[0], vals[1]);
    } else if (count == 3) {
      cogito_style_set_radius(style, vals[0], vals[1], vals[2], vals[1]);
    } else if (count >= 4) {
      cogito_style_set_radius(style, vals[0], vals[1], vals[2], vals[3]);
    }
    return;
  }
  if (strcmp(prop, "font-size") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_font_size = true; style->font_size = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "letter-spacing") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_letter_spacing = true; style->letter_spacing = (float)v; }
    return;
  }
  if (strcmp(prop, "min-width") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_min_w = true; style->min_w = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "min-height") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_min_h = true; style->min_h = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "max-width") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_max_w = true; style->max_w = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "max-height") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_max_h = true; style->max_h = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-size") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_size = true; style->appbar_btn_size = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-gap") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_gap = true; style->appbar_btn_gap = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-top") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_pad_top = true; style->appbar_btn_pad_top = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-right") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_pad_right = true; style->appbar_btn_pad_right = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-close-color") == 0) {
    CogitoColor c;
    if (cogito_css_parse_color(p, &c)) { style->has_appbar_btn_close_color = true; style->appbar_btn_close_color = c; }
    return;
  }
  if (strcmp(prop, "appbar-btn-min-color") == 0) {
    CogitoColor c;
    if (cogito_css_parse_color(p, &c)) { style->has_appbar_btn_min_color = true; style->appbar_btn_min_color = c; }
    return;
  }
  if (strcmp(prop, "appbar-btn-max-color") == 0) {
    CogitoColor c;
    if (cogito_css_parse_color(p, &c)) { style->has_appbar_btn_max_color = true; style->appbar_btn_max_color = c; }
    return;
  }
  if (strcmp(prop, "appbar-btn-border-color") == 0) {
    CogitoColor c;
    if (cogito_css_parse_color(p, &c)) { style->has_appbar_btn_border_color = true; style->appbar_btn_border_color = c; }
    return;
  }
  if (strcmp(prop, "appbar-btn-border-width") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_border_width = true; style->appbar_btn_border_width = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "font-weight") == 0) {
    double v = 0;
    const char* s = *p;
    char ident[32];
    if (cogito_css_read_ident(&s, ident, sizeof(ident))) {
      if (strcmp(ident, "bold") == 0) { style->has_font_weight = true; style->font_weight = 700; *p = s; return; }
      if (strcmp(ident, "normal") == 0) { style->has_font_weight = true; style->font_weight = 400; *p = s; return; }
    }
    if (cogito_css_read_number(p, &v)) { style->has_font_weight = true; style->font_weight = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "font-variant-numeric") == 0) {
    const char* s = *p;
    char ident[64];
    bool tabular = false;
    while (cogito_css_read_ident(&s, ident, sizeof(ident))) {
      if (strcmp(ident, "tabular-nums") == 0 || strcmp(ident, "tnum") == 0) {
        tabular = true;
      }
      while (*s && (isspace((unsigned char)*s) || *s == ',')) s++;
    }
    style->has_font_tabular = true;
    style->font_tabular = tabular;
    *p = s;
    return;
  }
  if (strcmp(prop, "font-family") == 0) {
    const char* s = *p;
    char ident[64];
    char family[64];
    bool family_set = false;
    bool parsed_any = false;
    bool mono = false;
    bool serif = false;
    while (*s) {
      while (*s && (isspace((unsigned char)*s) || *s == ',')) s++;
      if (!*s) break;

      ident[0] = '\0';
      if (*s == '"' || *s == '\'') {
        char quote = *s++;
        size_t i = 0;
        while (*s && *s != quote && i + 1 < sizeof(ident)) ident[i++] = *s++;
        ident[i] = '\0';
        if (*s == quote) s++;
      } else {
        if (!cogito_css_read_ident(&s, ident, sizeof(ident))) break;
      }

      if (!ident[0]) continue;
      parsed_any = true;
      if (!family_set) {
        strncpy(family, ident, sizeof(family) - 1);
        family[sizeof(family) - 1] = '\0';
        family_set = true;
      }
      if (strcmp(ident, "monospace") == 0 || strcmp(ident, "mono") == 0) {
        mono = true;
      } else if (strcmp(ident, "serif") == 0) {
        serif = true;
      }
    }
    if (family_set) {
      if (serif) {
        strncpy(family, "serif", sizeof(family) - 1);
        family[sizeof(family) - 1] = '\0';
      }
      strncpy(style->font_family, family, sizeof(style->font_family) - 1);
      style->font_family[sizeof(style->font_family) - 1] = '\0';
      style->has_font_family = true;
    }
    if (parsed_any) {
      style->has_font_mono = true;
      style->font_mono = mono;
    }
    *p = s;
    return;
  }
  if (strcmp(prop, "padding") == 0) {
    int vals[4] = {0, 0, 0, 0};
    int count = cogito_css_parse_ints(p, vals, 4);
    if (count == 1) {
      cogito_style_set_padding(style, vals[0], vals[0], vals[0], vals[0]);
    } else if (count == 2) {
      cogito_style_set_padding(style, vals[0], vals[1], vals[0], vals[1]);
    } else if (count == 3) {
      cogito_style_set_padding(style, vals[0], vals[1], vals[2], vals[1]);
    } else if (count >= 4) {
      cogito_style_set_padding(style, vals[0], vals[1], vals[2], vals[3]);
    }
    return;
  }
  if (strcmp(prop, "padding-left") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_padding_left = true; style->padding_left = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "padding-top") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_padding_top = true; style->padding_top = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "padding-right") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_padding_right = true; style->padding_right = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "padding-bottom") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_padding_bottom = true; style->padding_bottom = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "margin") == 0) {
    int vals[4] = {0, 0, 0, 0};
    int count = cogito_css_parse_ints(p, vals, 4);
    if (count == 1) {
      cogito_style_set_margin(style, vals[0], vals[0], vals[0], vals[0]);
    } else if (count == 2) {
      cogito_style_set_margin(style, vals[0], vals[1], vals[0], vals[1]);
    } else if (count == 3) {
      cogito_style_set_margin(style, vals[0], vals[1], vals[2], vals[1]);
    } else if (count >= 4) {
      cogito_style_set_margin(style, vals[0], vals[1], vals[2], vals[3]);
    }
    return;
  }
  if (strcmp(prop, "margin-left") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_left = true; style->margin_left = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "margin-top") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_top = true; style->margin_top = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "margin-right") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_right = true; style->margin_right = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "margin-bottom") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_bottom = true; style->margin_bottom = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "item-padding") == 0 || strcmp(prop, "menu-item-padding") == 0) {
    int vals[4] = {0, 0, 0, 0};
    int count = cogito_css_parse_ints(p, vals, 4);
    if (count == 1) {
      style->has_padding = true;
      style->padding_left = vals[0];
      style->padding_top = vals[0];
      style->padding_right = vals[0];
      style->padding_bottom = vals[0];
    } else if (count == 2) {
      style->has_padding = true;
      style->padding_top = vals[0];
      style->padding_bottom = vals[0];
      style->padding_left = vals[1];
      style->padding_right = vals[1];
    } else if (count == 3) {
      style->has_padding = true;
      style->padding_top = vals[0];
      style->padding_left = vals[1];
      style->padding_right = vals[1];
      style->padding_bottom = vals[2];
    } else if (count >= 4) {
      style->has_padding = true;
      style->padding_top = vals[0];
      style->padding_right = vals[1];
      style->padding_bottom = vals[2];
      style->padding_left = vals[3];
    }
    return;
  }
  if (strcmp(prop, "item-height") == 0 || strcmp(prop, "menu-item-height") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_top = true; style->margin_top = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "box-shadow") == 0) {
    cogito_css_skip_ws(p);
    if (**p == '[') {
      (*p)++;
      cogito_css_skip_ws(p);
    }
    double dx = 0, dy = 0, blur = 0, spread = 0;
    if (!cogito_css_read_number(p, &dx)) return;
    if (!cogito_css_read_number(p, &dy)) return;
    const char* save = *p;
    cogito_css_skip_ws(&save);
    if (save[0] >= '0' && save[0] <= '9') {
      *p = save;
      cogito_css_read_number(p, &blur);
    }
    save = *p;
    cogito_css_skip_ws(&save);
    if (save[0] >= '0' && save[0] <= '9') {
      *p = save;
      cogito_css_read_number(p, &spread);
    }
    CogitoColor c = {0, 0, 0, 255};
    if (!cogito_css_parse_color(p, &c)) return;
    bool inset = false;
    char kw[16];
    cogito_css_skip_ws(p);
    const char* kw_start = *p;
    if (cogito_css_read_ident(p, kw, sizeof(kw))) {
      if (strcmp(kw, "inset") == 0) {
        inset = true;
      } else {
        *p = kw_start;
      }
    }
    cogito_css_skip_ws(p);
    if (**p == ']') (*p)++;
    style->has_box_shadow = true;
    style->box_shadow.dx = (float)dx;
    style->box_shadow.dy = (float)dy;
    style->box_shadow.blur = (float)blur;
    style->box_shadow.spread = (float)spread;
    style->box_shadow.color = c;
    style->box_shadow.inset = inset;
    return;
  }
  if (strcmp(prop, "elevation") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) {
      int level = (int)lround(v);
      if (level < 0) level = 0;
      if (level > 5) level = 5;
      style->has_shadow = true;
      style->shadow_level = level;
    }
    return;
  }
  if (strcmp(prop, "track-height") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_track_height = true; style->track_height = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "track-color") == 0 || strcmp(prop, "track") == 0) {
    CogitoColor c;
    if (cogito_css_parse_color(p, &c)) { style->has_track = true; style->track = c; }
    return;
  }
  if (strcmp(prop, "knob-color") == 0 || strcmp(prop, "knob") == 0) {
    CogitoColor c;
    if (cogito_css_parse_color(p, &c)) { style->has_knob = true; style->knob = c; }
    return;
  }
  if (strcmp(prop, "knob-width") == 0 || strcmp(prop, "knob-w") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_min_w = true; style->min_w = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "knob-height") == 0 || strcmp(prop, "knob-h") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_min_h = true; style->min_h = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "check-color") == 0 || strcmp(prop, "check") == 0) {
    CogitoColor c;
    if (cogito_css_parse_color(p, &c)) { style->has_check = true; style->check = c; }
    return;
  }
  if (strcmp(prop, "font") == 0) {
    cogito_css_skip_ws(p);
    style->font_family[0] = '\0';
    if (**p == '"') {
      (*p)++;
      size_t i = 0;
      while (**p && **p != '"' && i < sizeof(style->font_family) - 1) {
        style->font_family[i++] = **p;
        (*p)++;
      }
      style->font_family[i] = '\0';
      if (**p == '"') (*p)++;
      style->has_font_family = true;
    } else {
      char fam[64];
      if (cogito_css_read_ident(p, fam, sizeof(fam))) {
        strncpy(style->font_family, fam, sizeof(style->font_family) - 1);
        style->font_family[sizeof(style->font_family) - 1] = '\0';
        style->has_font_family = true;
      }
    }
    double sz;
    if (cogito_css_read_number(p, &sz)) {
      style->has_font_size = true;
      style->font_size = (int)lround(sz);
    }
    cogito_css_skip_ws(p);
    const char* ws = *p;
    char wident[32];
    if (cogito_css_read_ident(&ws, wident, sizeof(wident))) {
      if (strcmp(wident, "bold") == 0) { style->has_font_weight = true; style->font_weight = 700; *p = ws; }
      else if (strcmp(wident, "normal") == 0) { style->has_font_weight = true; style->font_weight = 400; *p = ws; }
    } else {
      double wv;
      if (cogito_css_read_number(p, &wv)) {
        style->has_font_weight = true;
        style->font_weight = (int)lround(wv);
      }
    }
    return;
  }
   if (strcmp(prop, "opacity") == 0) {
     double v;
     if (cogito_css_read_number(p, &v)) {
       if (v < 0.0) v = 0.0;
       if (v > 1.0) v = 1.0;
       style->has_opacity = true;
       style->opacity = (float)v;
     }
     return;
   }
   if (strcmp(prop, "divider") == 0) {
     style->has_divider = true;
     style->divider = true;
     return;
   }
   if (strcmp(prop, "item-gap") == 0 || strcmp(prop, "menu-item-gap") == 0) {
     double v;
     if (cogito_css_read_number(p, &v)) {
       style->has_item_gap = true;
       style->item_gap = (int)lround(v);
     }
     return;
   }
  if (strcmp(prop, "border") == 0) {
    double bw;
    if (cogito_css_read_number(p, &bw)) {
      style->has_border_width = true;
      style->border_width = (int)lround(bw);
    }
    cogito_css_skip_ws(p);
    char bstyle[16];
    if (cogito_css_read_ident(p, bstyle, sizeof(bstyle))) {
      style->has_border_style = true;
      if (strcmp(bstyle, "none") == 0) style->border_style = COGITO_BORDER_NONE;
      else if (strcmp(bstyle, "solid") == 0) style->border_style = COGITO_BORDER_SOLID;
      else if (strcmp(bstyle, "dashed") == 0) style->border_style = COGITO_BORDER_DASHED;
      else if (strcmp(bstyle, "dotted") == 0) style->border_style = COGITO_BORDER_DOTTED;
      else style->border_style = COGITO_BORDER_SOLID;
    }
    CogitoColor bc;
    if (cogito_css_parse_color(p, &bc)) {
      style->has_border = true;
      style->border = bc;
    }
    return;
  }
}

static bool cogito_sum_selector_kind(const char* sel, CogitoKind* out_kind, bool* out_base, int* out_label_class, bool* out_menu, int* out_class, char* out_custom_class, size_t custom_class_cap) {
  if (!sel || !sel[0]) return false;
  char buf[64];
  size_t n = strlen(sel);
  if (n >= sizeof(buf)) n = sizeof(buf) - 1;
  memcpy(buf, sel, n);
  buf[n] = 0;
  char* class_part = NULL;
  if (buf[0] == '.') {
    class_part = buf + 1;
    buf[0] = 0;
  } else {
    char* dot = strchr(buf, '.');
    if (dot) {
      *dot = 0;
      class_part = dot + 1;
    }
  }
  const char* s = buf;
  if (*s == '#') s++;
  if (strcmp(s, "*") == 0) {
    if (out_base) *out_base = true;
    return true;
  }
  if (class_part) {
    bool found_special_class = false;
    if (strcmp(class_part, "monospace") == 0 || strcmp(class_part, "mono") == 0) {
      if (out_class) *out_class = COGITO_STYLE_CLASS_MONO;
      found_special_class = true;
    } else if (strcmp(class_part, "tabular") == 0 || strcmp(class_part, "tnum") == 0 || strcmp(class_part, "tabular-nums") == 0) {
      if (out_class) *out_class = COGITO_STYLE_CLASS_TABULAR;
      found_special_class = true;
    } else if (strcmp(class_part, "outlined") == 0) {
      if (out_class) *out_class = COGITO_STYLE_CLASS_OUTLINED;
      found_special_class = true;
    } else if (strcmp(class_part, "text") == 0) {
      if (out_class) *out_class = COGITO_STYLE_CLASS_TEXT;
      found_special_class = true;
    } else if (strcmp(class_part, "filled") == 0) {
      if (out_class) *out_class = COGITO_STYLE_CLASS_FILLED;
      found_special_class = true;
    } else if (strcmp(class_part, "tonal") == 0) {
      if (out_class) *out_class = COGITO_STYLE_CLASS_TONAL;
      found_special_class = true;
    } else if (strcmp(class_part, "iconic") == 0) {
      if (out_class) *out_class = COGITO_STYLE_CLASS_ICONIC;
      found_special_class = true;
    }
    if (found_special_class) {
      // Continue to parse widget type (e.g., iconbtn.outlined)
      // But return early if no widget type prefix (class-only selector like .outlined)
      if (s[0] == 0) {
        if (out_kind) *out_kind = COGITO_KIND_COUNT; // sentinel: matches any widget kind
        return true;
      }
    } else {
      int cls = cogito_label_class_from_name(class_part);
      if (cls != COGITO_LABEL_CLASS_NONE) {
        if (s[0] == 0 || strcmp(s, "label") == 0) {
          if (out_label_class) *out_label_class = cls;
          if (out_kind) *out_kind = COGITO_LABEL;
          return true;
        }
      }
      if (out_custom_class && custom_class_cap > 0) {
        strncpy(out_custom_class, class_part, custom_class_cap - 1);
        out_custom_class[custom_class_cap - 1] = 0;
      }
      // Class-only selector (e.g. ".primary"): no widget type prefix → universal class
      if (s[0] == 0) {
        if (out_kind) *out_kind = COGITO_KIND_COUNT; // sentinel: matches any widget kind
        return true;
      }
    }
  }
  if (strcmp(s, "appbar") == 0) { if (out_kind) *out_kind = COGITO_APPBAR; return true; }
  if (strcmp(s, "bottom-nav") == 0 || strcmp(s, "bottom_nav") == 0) { if (out_kind) *out_kind = COGITO_BOTTOM_NAV; return true; }
  if (strcmp(s, "button") == 0) { if (out_kind) *out_kind = COGITO_BUTTON; return true; }
  if (strcmp(s, "carousel") == 0) { if (out_kind) *out_kind = COGITO_CAROUSEL; return true; }
  if (strcmp(s, "carousel_item") == 0) { if (out_kind) *out_kind = COGITO_CAROUSEL_ITEM; return true; }
  if (strcmp(s, "checkbox") == 0) { if (out_kind) *out_kind = COGITO_CHECKBOX; return true; }
  if (strcmp(s, "chip") == 0) { if (out_kind) *out_kind = COGITO_CHIP; return true; }
  if (strcmp(s, "colorpicker") == 0) { if (out_kind) *out_kind = COGITO_COLORPICKER; return true; }
  if (strcmp(s, "datepicker") == 0) { if (out_kind) *out_kind = COGITO_DATEPICKER; return true; }
  if (strcmp(s, "dialog") == 0) { if (out_kind) *out_kind = COGITO_DIALOG; return true; }
  if (strcmp(s, "dialogslot") == 0 || strcmp(s, "dialog-slot") == 0 || strcmp(s, "dialog_slot") == 0) {
    if (out_kind) *out_kind = COGITO_DIALOG_SLOT;
    return true;
  }
  if (strcmp(s, "divider") == 0) { if (out_kind) *out_kind = COGITO_DIVIDER; return true; }
  if (strcmp(s, "card") == 0) { if (out_kind) *out_kind = COGITO_CARD; return true; }
  if (strcmp(s, "avatar") == 0) { if (out_kind) *out_kind = COGITO_AVATAR; return true; }
  if (strcmp(s, "badge") == 0) { if (out_kind) *out_kind = COGITO_BADGE; return true; }
  if (strcmp(s, "banner") == 0) { if (out_kind) *out_kind = COGITO_BANNER; return true; }
  if (strcmp(s, "bottom-sheet") == 0 || strcmp(s, "bottom_sheet") == 0 || strcmp(s, "bottomsheet") == 0) { if (out_kind) *out_kind = COGITO_BOTTOM_SHEET; return true; }
  if (strcmp(s, "side-sheet") == 0 || strcmp(s, "side_sheet") == 0 || strcmp(s, "sidesheet") == 0) { if (out_kind) *out_kind = COGITO_SIDE_SHEET; return true; }
  if (strcmp(s, "timepicker") == 0 || strcmp(s, "time-picker") == 0 || strcmp(s, "time_picker") == 0) { if (out_kind) *out_kind = COGITO_TIMEPICKER; return true; }
  if (strcmp(s, "active-indicator") == 0 || strcmp(s, "active_indicator") == 0 || strcmp(s, "activeindicator") == 0) { if (out_kind) *out_kind = COGITO_ACTIVE_INDICATOR; return true; }
  if (strcmp(s, "switchbar") == 0 || strcmp(s, "switch-bar") == 0 || strcmp(s, "switch_bar") == 0) { if (out_kind) *out_kind = COGITO_SWITCHBAR; return true; }
  if (strcmp(s, "content-list") == 0 || strcmp(s, "content_list") == 0 || strcmp(s, "contentlist") == 0) { if (out_kind) *out_kind = COGITO_CONTENT_LIST; return true; }
  if (strcmp(s, "empty-page") == 0 || strcmp(s, "empty_page") == 0 || strcmp(s, "emptypage") == 0) { if (out_kind) *out_kind = COGITO_EMPTY_PAGE; return true; }
  if (strcmp(s, "tip-view") == 0 || strcmp(s, "tip_view") == 0 || strcmp(s, "tipview") == 0) { if (out_kind) *out_kind = COGITO_TIP_VIEW; return true; }
  if (strcmp(s, "settings-window") == 0 || strcmp(s, "settings_window") == 0 || strcmp(s, "settingswindow") == 0) { if (out_kind) *out_kind = COGITO_SETTINGS_WINDOW; return true; }
  if (strcmp(s, "settings-page") == 0 || strcmp(s, "settings_page") == 0 || strcmp(s, "settingspage") == 0) { if (out_kind) *out_kind = COGITO_SETTINGS_PAGE; return true; }
  if (strcmp(s, "settings-list") == 0 || strcmp(s, "settings_list") == 0 || strcmp(s, "settingslist") == 0) { if (out_kind) *out_kind = COGITO_SETTINGS_LIST; return true; }
  if (strcmp(s, "settings-row") == 0 || strcmp(s, "settings_row") == 0 || strcmp(s, "settingsrow") == 0) { if (out_kind) *out_kind = COGITO_SETTINGS_ROW; return true; }
  if (strcmp(s, "welcome-screen") == 0 || strcmp(s, "welcome_screen") == 0 || strcmp(s, "welcomescreen") == 0) { if (out_kind) *out_kind = COGITO_WELCOME_SCREEN; return true; }
  if (strcmp(s, "view-dual") == 0 || strcmp(s, "view_dual") == 0 || strcmp(s, "viewdual") == 0) { if (out_kind) *out_kind = COGITO_VIEW_DUAL; return true; }
  if (strcmp(s, "view-chooser") == 0 || strcmp(s, "view_chooser") == 0 || strcmp(s, "viewchooser") == 0) { if (out_kind) *out_kind = COGITO_VIEW_CHOOSER; return true; }
  if (strcmp(s, "about-window") == 0 || strcmp(s, "about_window") == 0 || strcmp(s, "aboutwindow") == 0) { if (out_kind) *out_kind = COGITO_ABOUT_WINDOW; return true; }
  if (strcmp(s, "split-button") == 0 || strcmp(s, "split_button") == 0 || strcmp(s, "splitbutton") == 0) { if (out_kind) *out_kind = COGITO_SPLIT_BUTTON; return true; }
  if (strcmp(s, "drawing-area") == 0 || strcmp(s, "drawing_area") == 0 || strcmp(s, "drawingarea") == 0) { if (out_kind) *out_kind = COGITO_DRAWING_AREA; return true; }
  if (strcmp(s, "shape") == 0) { if (out_kind) *out_kind = COGITO_SHAPE; return true; }
  if (strcmp(s, "dropdown") == 0) { if (out_kind) *out_kind = COGITO_DROPDOWN; return true; }
  if (strcmp(s, "fab") == 0) { if (out_kind) *out_kind = COGITO_FAB; return true; }
  if (strcmp(s, "fab-menu") == 0 || strcmp(s, "fab_menu") == 0 || strcmp(s, "fabmenu") == 0) { if (out_kind) *out_kind = COGITO_FAB_MENU; return true; }
  if (strcmp(s, "fixed") == 0) { if (out_kind) *out_kind = COGITO_FIXED; return true; }
  if (strcmp(s, "grid") == 0) { if (out_kind) *out_kind = COGITO_GRID; return true; }
  if (strcmp(s, "hstack") == 0) { if (out_kind) *out_kind = COGITO_HSTACK; return true; }
  if (strcmp(s, "iconbtn") == 0) { if (out_kind) *out_kind = COGITO_ICONBTN; return true; }
  if (strcmp(s, "image") == 0) { if (out_kind) *out_kind = COGITO_IMAGE; return true; }
  if (strcmp(s, "label") == 0) { if (out_kind) *out_kind = COGITO_LABEL; return true; }
  if (strcmp(s, "list") == 0) { if (out_kind) *out_kind = COGITO_LIST; return true; }
  if (strcmp(s, "menu") == 0) { if (out_menu) *out_menu = true; return true; }
  if (strcmp(s, "menu-section") == 0 || strcmp(s, "menu_section") == 0 || strcmp(s, "menusection") == 0) { if (out_kind) *out_kind = COGITO_MENU_SECTION; return true; }
  if (strcmp(s, "nav-rail") == 0) { if (out_kind) *out_kind = COGITO_NAV_RAIL; return true; }
  if (strcmp(s, "popover") == 0) { if (out_kind) *out_kind = COGITO_POPOVER; return true; }
  if (strcmp(s, "progress") == 0) { if (out_kind) *out_kind = COGITO_PROGRESS; return true; }
  if (strcmp(s, "scroller") == 0) { if (out_kind) *out_kind = COGITO_SCROLLER; return true; }
  if (strcmp(s, "searchfield") == 0) { if (out_kind) *out_kind = COGITO_SEARCHFIELD; return true; }
  if (strcmp(s, "buttongroup") == 0) { if (out_kind) *out_kind = COGITO_BUTTON_GROUP; return true; }
  if (strcmp(s, "slider") == 0) { if (out_kind) *out_kind = COGITO_SLIDER; return true; }
  if (strcmp(s, "stepper") == 0) { if (out_kind) *out_kind = COGITO_STEPPER; return true; }
  if (strcmp(s, "switch") == 0) { if (out_kind) *out_kind = COGITO_SWITCH; return true; }
  if (strcmp(s, "tabs") == 0) { if (out_kind) *out_kind = COGITO_TABS; return true; }
  if (strcmp(s, "textfield") == 0) { if (out_kind) *out_kind = COGITO_TEXTFIELD; return true; }
  if (strcmp(s, "textview") == 0) { if (out_kind) *out_kind = COGITO_TEXTVIEW; return true; }
  if (strcmp(s, "toast") == 0) { if (out_kind) *out_kind = COGITO_TOAST; return true; }
  if (strcmp(s, "toasts") == 0) { if (out_kind) *out_kind = COGITO_TOASTS; return true; }
  if (strcmp(s, "toolbar") == 0) {
    if (out_kind) *out_kind = COGITO_TOOLBAR;
    return true;
  }
  if (strcmp(s, "tooltip") == 0) { if (out_kind) *out_kind = COGITO_TOOLTIP; return true; }
  if (strcmp(s, "treeview") == 0) { if (out_kind) *out_kind = COGITO_TREEVIEW; return true; }
  if (strcmp(s, "viewswitcher") == 0) { if (out_kind) *out_kind = COGITO_VIEWSWITCHER; return true; }
  if (strcmp(s, "vstack") == 0) { if (out_kind) *out_kind = COGITO_VSTACK; return true; }
  if (strcmp(s, "window") == 0) { if (out_kind) *out_kind = COGITO_WINDOW; return true; }
  if (strcmp(s, "zstack") == 0) { if (out_kind) *out_kind = COGITO_ZSTACK; return true; }
  return false;
}

static bool cogito_sum_parse_token(const char* tok, char* base, size_t base_cap, char* state, size_t state_cap) {
  if (!tok || !tok[0]) return false;
  while (*tok == '.' || *tok == '#') tok++;
  if (!*tok) return false;
  char buf[128];
  size_t len = strlen(tok);
  if (len >= sizeof(buf)) len = sizeof(buf) - 1;
  memcpy(buf, tok, len);
  buf[len] = 0;
  char* state_ptr = NULL;
  char* dot = strchr(buf, '.');
  char* colon = strchr(buf, ':');
  char* eq = strchr(buf, '=');
  // For element position syntax (content-list=first), treat = as state delimiter
  // Priority: colon > dot > eq (but eq only if no colon/dot)
  if (colon) state_ptr = colon;
  else if (dot) state_ptr = dot;
  else if (eq) state_ptr = eq;
  if (state_ptr) {
    *state_ptr = 0;
    state_ptr++;
  }
  if (base && base_cap) {
    strncpy(base, buf, base_cap - 1);
    base[base_cap - 1] = 0;
  }
  if (state && state_cap) {
    state[0] = 0;
    if (state_ptr && state_ptr[0]) {
      strncpy(state, state_ptr, state_cap - 1);
      state[state_cap - 1] = 0;
    }
  }
  return true;
}

static bool cogito_sum_selector_has_class(const char* sel, const char* name) {
  if (!sel || !name || !name[0]) return false;
  const char* p = sel;
  while (*p) {
    if (*p == '.') {
      p++;
      const char* start = p;
      while (*p && *p != '.' && *p != ':' && *p != '#' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != ',') p++;
      size_t len = (size_t)(p - start);
      if (len > 0 && strlen(name) == len && strncmp(start, name, len) == 0) return true;
    } else {
      p++;
    }
  }
  return false;
}

static CogitoCustomClassStyle* cogito_get_or_create_custom_class(const char* name, CogitoKind kind) {
  if (!name || !name[0]) return NULL;
  for (int i = 0; i < cogito_theme.custom_class_count; i++) {
    if (strcmp(cogito_theme.custom_classes[i].name, name) == 0 &&
        cogito_theme.custom_classes[i].kind == kind) {
      return &cogito_theme.custom_classes[i];
    }
  }
  if (cogito_theme.custom_class_count >= COGITO_MAX_CUSTOM_CLASSES) return NULL;
  CogitoCustomClassStyle* entry = &cogito_theme.custom_classes[cogito_theme.custom_class_count++];
  strncpy(entry->name, name, sizeof(entry->name) - 1);
  entry->name[sizeof(entry->name) - 1] = '\0';
  entry->kind = kind;
  cogito_style_clear(&entry->style);
  cogito_style_clear(&entry->hover);
  cogito_style_clear(&entry->active);
  cogito_style_clear(&entry->focused);
  cogito_style_clear(&entry->checked);
  cogito_style_clear(&entry->disabled);
  return entry;
}

void cogito_sum_add_styles(const char* parent, const char* child, CogitoStyle** styles, int* flags, int* count, int max) {
  if (!count || !styles || max <= 0) return;
  if (!child || !child[0]) {
    bool is_base = false;
    bool is_menu = false;
    CogitoKind kind = COGITO_WINDOW;
    int label_class = COGITO_LABEL_CLASS_NONE;
    int class_kind = COGITO_STYLE_CLASS_NONE;
    char buf[64];
    char state[16] = {0};
    const char* sel = parent;
    char custom_class_buf[64] = {0};
    if (parent) {
      size_t n = strlen(parent);
      if (n >= sizeof(buf)) n = sizeof(buf) - 1;
      memcpy(buf, parent, n);
      buf[n] = 0;
      char* colon = strchr(buf, ':');
      char* eq = strchr(buf, '=');
      if (colon) {
        *colon = 0;
        strncpy(state, colon + 1, sizeof(state) - 1);
        state[sizeof(state) - 1] = 0;
        cogito_trim(state);
      } else if (eq) {
        // Handle element position syntax: content-list=first
        *eq = 0;
        strncpy(state, eq + 1, sizeof(state) - 1);
        state[sizeof(state) - 1] = 0;
        cogito_trim(state);
      }
      sel = buf;
    }
    if (cogito_sum_selector_kind(sel, &kind, &is_base, &label_class, &is_menu, &class_kind, custom_class_buf, sizeof(custom_class_buf))) {
      CogitoStyle* style = NULL;
      bool hover = strcmp(state, "hover") == 0;
      bool active = strcmp(state, "active") == 0;
      bool focused = strcmp(state, "focus") == 0 || strcmp(state, "focused") == 0;
      bool checked = strcmp(state, "checked") == 0;
      bool disabled = strcmp(state, "disabled") == 0;
      if (class_kind == COGITO_STYLE_CLASS_NONE && label_class == COGITO_LABEL_CLASS_NONE && 
          !is_menu && !is_base && custom_class_buf[0] != '\0') {
        CogitoCustomClassStyle* entry = cogito_get_or_create_custom_class(custom_class_buf, kind);
        if (entry) {
          if (checked) style = &entry->checked;
          else if (disabled) style = &entry->disabled;
          else if (active) style = &entry->active;
          else if (focused) style = &entry->focused;
          else if (hover) style = &entry->hover;
          else style = &entry->style;
        }
      } else if (class_kind != COGITO_STYLE_CLASS_NONE) {
        if (class_kind == COGITO_STYLE_CLASS_MONO) {
          if (checked) style = &cogito_theme.class_mono_checked;
          else if (disabled) style = &cogito_theme.class_mono_disabled;
          else if (active) style = &cogito_theme.class_mono_active;
          else if (focused) style = &cogito_theme.class_mono_focused;
          else if (hover) style = &cogito_theme.class_mono_hover;
          else style = &cogito_theme.class_mono;
        } else if (class_kind == COGITO_STYLE_CLASS_TABULAR) {
          if (checked) style = &cogito_theme.class_tabular_checked;
          else if (disabled) style = &cogito_theme.class_tabular_disabled;
          else if (active) style = &cogito_theme.class_tabular_active;
          else if (focused) style = &cogito_theme.class_tabular_focused;
          else if (hover) style = &cogito_theme.class_tabular_hover;
          else style = &cogito_theme.class_tabular;
        } else if (class_kind == COGITO_STYLE_CLASS_OUTLINED) {
          if (kind == COGITO_ICONBTN) {
            if (active) style = &cogito_theme.iconbtn_outlined_active;
            else if (hover) style = &cogito_theme.iconbtn_outlined_hover;
            else style = &cogito_theme.iconbtn_outlined;
          } else {
            if (disabled) style = &cogito_theme.button_outlined_disabled;
            else if (active) style = &cogito_theme.button_outlined_active;
            else if (focused) style = &cogito_theme.button_outlined_focused;
            else if (hover) style = &cogito_theme.button_outlined_hover;
            else style = &cogito_theme.button_outlined;
          }
        } else if (class_kind == COGITO_STYLE_CLASS_TEXT) {
          if (disabled) style = &cogito_theme.button_text_disabled;
          else if (active) style = &cogito_theme.button_text_active;
          else if (focused) style = &cogito_theme.button_text_focused;
          else if (hover) style = &cogito_theme.button_text_hover;
          else style = &cogito_theme.button_text;
        } else if (class_kind == COGITO_STYLE_CLASS_FILLED) {
          if (active) style = &cogito_theme.iconbtn_filled_active;
          else if (hover) style = &cogito_theme.iconbtn_filled_hover;
          else style = &cogito_theme.iconbtn_filled;
        } else if (class_kind == COGITO_STYLE_CLASS_TONAL) {
          if (active) style = &cogito_theme.iconbtn_tonal_active;
          else if (hover) style = &cogito_theme.iconbtn_tonal_hover;
          else style = &cogito_theme.iconbtn_tonal;
        } else if (class_kind == COGITO_STYLE_CLASS_ICONIC) {
          if (active) style = &cogito_theme.iconbtn_iconic_active;
          else if (hover) style = &cogito_theme.iconbtn_iconic_hover;
          else style = &cogito_theme.iconbtn_iconic;
        }
      } else if (label_class != COGITO_LABEL_CLASS_NONE) {
        if (checked) style = &cogito_theme.label_class_checked[label_class];
        else if (disabled) style = &cogito_theme.label_class_disabled[label_class];
        else if (active) style = &cogito_theme.label_class_active[label_class];
        else if (focused) style = &cogito_theme.label_class_focused[label_class];
        else if (hover) style = &cogito_theme.label_class_hover[label_class];
        else style = &cogito_theme.label_class[label_class];
      } else if (kind == COGITO_CONTENT_LIST && state[0] &&
                 (strcmp(state, "first") == 0 || strcmp(state, "last") == 0 ||
                  strcmp(state, "middle") == 0 || strcmp(state, "single") == 0)) {
        // Handle element position selectors: content-list=first
        if (strcmp(state, "first") == 0) style = &cogito_theme.content_list_item_first;
        else if (strcmp(state, "last") == 0) style = &cogito_theme.content_list_item_last;
        else if (strcmp(state, "middle") == 0) style = &cogito_theme.content_list_item_middle;
        else if (strcmp(state, "single") == 0) style = &cogito_theme.content_list_item_single;
      } else if (is_menu) {
        style = &cogito_theme.menu;
      } else {
        if (is_base) {
          style = &cogito_theme.base;
        } else if (checked) {
          style = &cogito_theme.per_kind_checked[kind];
        } else if (disabled) {
          style = &cogito_theme.per_kind_disabled[kind];
        } else if (active) {
          style = &cogito_theme.per_kind_active[kind];
        } else if (focused) {
          style = &cogito_theme.per_kind_focused[kind];
        } else if (hover) {
          style = &cogito_theme.per_kind_hover[kind];
        } else {
          style = &cogito_theme.per_kind[kind];
        }
      }
      if (style && *count < max) {
        styles[(*count)] = style;
        if (flags && strcmp(state, "selection") == 0) flags[(*count)] = 1;
        (*count)++;
      }
    }
    return;
  }
  char parent_base[32] = {0};
  char child_base[32] = {0};
  char child_state[16] = {0};
  char parent_state[16] = {0};
  if (!cogito_sum_parse_token(parent, parent_base, sizeof(parent_base), parent_state, sizeof(parent_state))) return;
  if (!cogito_sum_parse_token(child, child_base, sizeof(child_base), child_state, sizeof(child_state))) return;
  bool hover = strcmp(child_state, "hover") == 0;
  bool active = strcmp(child_state, "active") == 0;
  bool checked_state = strcmp(parent_state, "checked") == 0;
  bool parent_radio = strcmp(parent_base, "radio") == 0 || cogito_sum_selector_has_class(parent, "radio");
  bool parent_checkbox = strcmp(parent_base, "checkbox") == 0 || cogito_sum_selector_has_class(parent, "checkbox");
  bool parent_switch = strcmp(parent_base, "switch") == 0 || cogito_sum_selector_has_class(parent, "switch");
  if ((parent_checkbox || parent_radio) && strcmp(child_base, "check") == 0) {
    CogitoStyle* style = NULL;
    if (parent_radio) {
      if (checked_state) style = &cogito_theme.radio_check_checked;
      else if (hover) style = &cogito_theme.radio_check_hover;
      else if (active) style = &cogito_theme.radio_check_active;
      else style = &cogito_theme.radio_check;
    } else {
      if (checked_state) style = &cogito_theme.checkbox_check_checked;
      else if (hover) style = &cogito_theme.checkbox_check_hover;
      else if (active) style = &cogito_theme.checkbox_check_active;
      else style = &cogito_theme.checkbox_check;
    }
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if ((parent_checkbox || parent_radio) && strcmp(child_base, "box") == 0) {
    CogitoStyle* style = NULL;
    if (parent_radio) {
      if (checked_state) style = &cogito_theme.radio_box_checked;
      else if (hover) style = &cogito_theme.radio_box_hover;
      else if (active) style = &cogito_theme.radio_box_active;
      else style = &cogito_theme.radio_box;
    } else {
      if (checked_state) style = &cogito_theme.checkbox_box_checked;
      else if (hover) style = &cogito_theme.checkbox_box_hover;
      else if (active) style = &cogito_theme.checkbox_box_active;
      else style = &cogito_theme.checkbox_box;
    }
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if (parent_switch && strcmp(child_base, "track") == 0) {
    CogitoStyle* style = NULL;
    if (checked_state) style = &cogito_theme.switch_track_checked;
    else if (hover) style = &cogito_theme.switch_track_hover;
    else if (active) style = &cogito_theme.switch_track_active;
    else style = &cogito_theme.switch_track;
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if (parent_switch && strcmp(child_base, "knob") == 0) {
    CogitoStyle* style = NULL;
    if (checked_state) style = &cogito_theme.switch_knob_checked;
    else if (hover) style = &cogito_theme.switch_knob_hover;
    else if (active) style = &cogito_theme.switch_knob_active;
    else style = &cogito_theme.switch_knob;
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if (strcmp(parent_base, "menu") == 0) {
    if (strcmp(child_base, "item") == 0) {
      if (*count < max) {
        styles[(*count)] = &cogito_theme.menu_item;
        if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
        (*count)++;
      }
    }
    return;
  }
  if ((strcmp(parent_base, "content-list") == 0 || strcmp(parent_base, "content_list") == 0 || strcmp(parent_base, "contentlist") == 0) &&
      strcmp(child_base, "label") == 0) {
    if (*count < max) {
      styles[(*count)] = &cogito_theme.content_list_label;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if (strcmp(parent_base, "appbar") != 0) return;

  if (strcmp(child_base, "button") == 0) {
    CogitoStyle* style = NULL;
    if (hover) style = &cogito_theme.appbar_child_button_hover;
    else if (active) style = &cogito_theme.appbar_child_button_active;
    else style = &cogito_theme.appbar_child_button;
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if (strcmp(child_base, "iconbtn") == 0 || strcmp(child_base, "icon-button") == 0) {
    CogitoStyle* style = NULL;
    if (hover) style = &cogito_theme.appbar_child_iconbtn_hover;
    else if (active) style = &cogito_theme.appbar_child_iconbtn_active;
    else style = &cogito_theme.appbar_child_iconbtn;
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  CogitoStyle* all = NULL;
  if (hover) all = &cogito_theme.appbar_win_btn_hover_all;
  else if (active) all = &cogito_theme.appbar_win_btn_active_all;
  else all = &cogito_theme.appbar_win_btn_all;

  if (strcmp(child_base, "window-button") == 0) {
    if (*count < max) {
      styles[(*count)] = all;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  int idx = -1;
  if (strcmp(child_base, "window-close") == 0) idx = 0;
  else if (strcmp(child_base, "window-min") == 0) idx = 1;
  else if (strcmp(child_base, "window-max") == 0) idx = 2;
  if (idx >= 0) {
    CogitoStyle* style = NULL;
    if (hover) style = &cogito_theme.appbar_win_btn_hover[idx];
    else if (active) style = &cogito_theme.appbar_win_btn_active[idx];
    else style = &cogito_theme.appbar_win_btn[idx];
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
}

static void cogito_apply_sum_inline(const char* src) {
  if (!src || !src[0]) return;
  sum_parse(src);
}

static void cogito_apply_sum_file_inline(const char* path) {
  size_t len = 0;
  char* src = sum_read_file(path, &len);
  if (!src) return;
  sum_parse(src);
  free(src);
}

static void cogito_apply_default_sum(void) {
  size_t len = 0;
  const char* path = sum_default_path();
  char* src = sum_read_file(path, &len);
  if (!src) return;
  sum_parse(src);
  free(src);
}

static bool cogito_sum_resolve_user_path(const char* path, char* resolved, size_t resolved_cap) {
  if (!path || !path[0] || !resolved || resolved_cap == 0) return false;
  resolved[0] = 0;
  if (sum_path_is_absolute(path)) {
    snprintf(resolved, resolved_cap, "%s", path);
    return cogito_file_exists(resolved);
  }

  const char* rel_path = path;
  const char* script_dir = cogito_get_script_dir();
  if (script_dir && script_dir[0]) {
    sum_path_join(script_dir, rel_path, resolved, resolved_cap);
    if (cogito_file_exists(resolved)) return true;
  }

  snprintf(resolved, resolved_cap, "%s", rel_path);
  return cogito_file_exists(resolved);
}

static void cogito_load_sum_file(const char* path) {
  char resolved_path[PATH_MAX];
  const char* load_path = path;
  if (cogito_sum_resolve_user_path(path, resolved_path, sizeof(resolved_path))) {
    load_path = resolved_path;
  }
  size_t len = 0;
  char* src = sum_read_file(load_path, &len);
  if (!src) {
    if (cogito_debug_enabled()) {
      fprintf(stderr, "cogito: sum not found: %s\n", path ? path : "");
      fflush(stderr);
    }
    return;
  }
  if (cogito_user_sum) free(cogito_user_sum);
  cogito_user_sum = src;
  extern CogitoNode* cogito_active_window;
  if (cogito_active_window) {
    cogito_apply_theme(cogito_theme_dark, cogito_theme_accent);
    if (cogito_active_window->auto_size) {
      int dw = 0;
      int dh = 0;
      cogito_window_desired_size(cogito_active_window, &dw, &dh);
      if (dw > 0 && dh > 0) {
        cogito_active_window->w = dw;
        cogito_active_window->h = dh;
        if (cogito_backend_ready && cogito_backend && cogito_active_backend_window) {
          cogito_backend->window_set_size(cogito_active_backend_window, dw, dh);
        }
      }
    }
    cogito_window_relayout(cogito_active_window);
  } else {
    cogito_theme_refresh_deferred = true;
  }
}

static void cogito_load_sum(ErgoVal pathv) {
  ErgoStr* ps = stdr_to_string(pathv);
  if (ps && ps->data) {
    char resolved_path[PATH_MAX];
    const char* load_path = ps->data;
    if (cogito_sum_resolve_user_path(ps->data, resolved_path, sizeof(resolved_path))) {
      load_path = resolved_path;
    }
    size_t len = 0;
    char* src = sum_read_file(load_path, &len);
    if (src) {
      if (cogito_user_sum) free(cogito_user_sum);
      cogito_user_sum = src;
      if (cogito_active_window) {
        cogito_apply_theme(cogito_theme_dark, cogito_theme_accent);
      } else {
        cogito_theme_refresh_deferred = true;
      }
    }
  }
  if (ps) ergo_release_val(EV_STR(ps));
}

static void cogito_load_sum_source(const char* src) {
  if (!src) return;
  size_t len = strlen(src);
  char* copy = (char*)malloc(len + 1);
  if (!copy) return;
  memcpy(copy, src, len);
  copy[len] = '\0';
  if (cogito_user_sum) free(cogito_user_sum);
  cogito_user_sum = copy;
  if (cogito_active_window) {
    cogito_apply_theme(cogito_theme_dark, cogito_theme_accent);
  } else {
    cogito_theme_refresh_deferred = true;
  }
}

static inline double cogito_sum_profile_now(void) {
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

static void cogito_apply_theme(bool dark, CogitoColor accent) {
  const char* theme_profile_env = getenv("COGITO_THEME_PROFILE");
  bool theme_profile = theme_profile_env && theme_profile_env[0] && theme_profile_env[0] != '0';
  if (!theme_profile) {
    const char* startup_profile_env = getenv("COGITO_STARTUP_PROFILE");
    theme_profile = startup_profile_env && startup_profile_env[0] && startup_profile_env[0] != '0';
  }
  double tp0 = theme_profile ? cogito_sum_profile_now() : 0.0;
  double tp_after_defaults = tp0;
  double tp_after_default_sum = tp0;
  double tp_after_dark_sum = tp0;
  double tp_after_dynamic_build = tp0;
  double tp_after_dynamic_parse = tp0;
  double tp_after_user_sum = tp0;
  double tp_after_style_tree = tp0;
  bool had_prev_theme = cogito_theme_initialized;
  bool reveal_requested = cogito_theme_window_reveal_pending;
  cogito_theme_window_reveal_pending = false;
  int reveal_x = 0;
  int reveal_y = 0;
  CogitoColor reveal_old_bg = {0, 0, 0, 0};
  CogitoColor reveal_old_variant_bg = {0, 0, 0, 0};
  bool start_reveal = false;
  extern CogitoNode* cogito_active_window;
  CogitoNode* reveal_win = cogito_active_window;
  if (had_prev_theme && reveal_requested && reveal_win &&
      reveal_win->kind == COGITO_WINDOW && reveal_win->w > 0 && reveal_win->h > 0) {
    reveal_old_bg = reveal_win->bg;
    {
      CogitoStyle old_button = cogito_theme_resolve(COGITO_BUTTON);
      reveal_old_variant_bg = old_button.has_bg ? old_button.bg : reveal_old_bg;
    }
    if (!cogito_theme_take_action_origin(&reveal_x, &reveal_y)) {
      if (!cogito_pointer_query(&reveal_x, &reveal_y, NULL, NULL)) {
        reveal_x = reveal_win->x + reveal_win->w / 2;
        reveal_y = reveal_win->y + reveal_win->h / 2;
      }
    }
    start_reveal = true;
  } else if (reveal_requested) {
    // Drop stale origin when there is no drawable window for reveal.
    (void)cogito_theme_take_action_origin(NULL, NULL);
  }

  cogito_theme_initialized = true;
  cogito_sum_default_loaded = true;
  cogito_theme_set_defaults();
  tp_after_defaults = theme_profile ? cogito_sum_profile_now() : tp_after_defaults;
  cogito_apply_default_sum();
  tp_after_default_sum = theme_profile ? cogito_sum_profile_now() : tp_after_default_sum;
  if (dark) {
    cogito_apply_sum_file_inline(sum_dark_path());
  }
  tp_after_dark_sum = theme_profile ? cogito_sum_profile_now() : tp_after_dark_sum;
  char dyn[65536];
  sum_build_dynamic(dark, accent, dyn, sizeof(dyn));
  tp_after_dynamic_build = theme_profile ? cogito_sum_profile_now() : tp_after_dynamic_build;
  if (getenv("COGITO_THEME_DEBUG")) {
    fprintf(stderr, "Cogito dynamic SUM:\\n%s\\n", dyn);
  }
  cogito_apply_sum_inline(dyn);
  tp_after_dynamic_parse = theme_profile ? cogito_sum_profile_now() : tp_after_dynamic_parse;
  if (cogito_user_sum && cogito_user_sum[0]) {
    cogito_apply_sum_inline(cogito_user_sum);
  }
  tp_after_user_sum = theme_profile ? cogito_sum_profile_now() : tp_after_user_sum;
  cogito_theme_dark = dark;
  cogito_theme_accent = accent;
  if (cogito_active_window) {
    cogito_apply_style_tree(cogito_active_window);
    tp_after_style_tree = theme_profile ? cogito_sum_profile_now() : tp_after_style_tree;
    if (start_reveal && reveal_win == cogito_active_window) {
      CogitoColor new_bg = cogito_active_window->bg;
      CogitoColor new_variant_bg = new_bg;
      {
        CogitoStyle new_button = cogito_theme_resolve(COGITO_BUTTON);
        if (new_button.has_bg) new_variant_bg = new_button.bg;
      }
      CogitoColor reveal_from = reveal_old_bg;
      CogitoColor reveal_to = new_bg;
      int bg_delta =
          abs((int)new_bg.r - (int)reveal_old_bg.r) +
          abs((int)new_bg.g - (int)reveal_old_bg.g) +
          abs((int)new_bg.b - (int)reveal_old_bg.b) +
          abs((int)new_bg.a - (int)reveal_old_bg.a);
      if (bg_delta < 24) {
        reveal_from = reveal_old_variant_bg;
        reveal_to = new_variant_bg;
      }
      if (reveal_from.r != reveal_to.r || reveal_from.g != reveal_to.g ||
          reveal_from.b != reveal_to.b || reveal_from.a != reveal_to.a) {
        cogito_window_theme_reveal_start(cogito_active_window, reveal_x, reveal_y, reveal_from, reveal_to);
      }
    }
    cogito_window_relayout(cogito_active_window);
  }
  if (theme_profile) {
    double t_end = cogito_sum_profile_now();
    double ms_total = (t_end - tp0) * 1000.0;
    double ms_defaults = (tp_after_defaults - tp0) * 1000.0;
    double ms_default_sum = (tp_after_default_sum - tp_after_defaults) * 1000.0;
    double ms_dark_sum = (tp_after_dark_sum - tp_after_default_sum) * 1000.0;
    double ms_dynamic_build = (tp_after_dynamic_build - tp_after_dark_sum) * 1000.0;
    double ms_dynamic_parse = (tp_after_dynamic_parse - tp_after_dynamic_build) * 1000.0;
    double ms_user_sum = (tp_after_user_sum - tp_after_dynamic_parse) * 1000.0;
    double ms_style_tree = (tp_after_style_tree - tp_after_user_sum) * 1000.0;
    double ms_post_style = (t_end - tp_after_style_tree) * 1000.0;
    fprintf(stderr,
            "[cogito-theme] total=%.2fms defaults=%.2fms default_sum=%.2fms dark_sum=%.2fms dynamic_build=%.2fms dynamic_parse=%.2fms user_sum=%.2fms style_tree=%.2fms post_style=%.2fms\n",
            ms_total, ms_defaults, ms_default_sum, ms_dark_sum, ms_dynamic_build,
            ms_dynamic_parse, ms_user_sum, ms_style_tree, ms_post_style);
    fflush(stderr);
  }
}
