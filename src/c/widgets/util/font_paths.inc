// Font path discovery utilities
// Locates system fonts for regular, bold, and monospace typefaces.

static const char* cogito_default_font_path(void) {
#if defined(__APPLE__)
  const char* candidates[] = {
    "/System/Library/Fonts/SFNS.ttf",
    "/System/Library/Fonts/SFNSText.ttf",
    "/System/Library/Fonts/SFNSDisplay.ttf",
    "/System/Library/Fonts/Helvetica.ttc"
  };
#elif defined(_WIN32)
  const char* candidates[] = {
    "C:\\\\Windows\\\\Fonts\\\\segoeui.ttf"
  };
#else
  const char* candidates[] = {
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
  };
#endif
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    if (cogito_file_exists(candidates[i])) return candidates[i];
  }
  return NULL;
}

static const char* cogito_replace_once(const char* path, const char* find, const char* repl, char* out, size_t cap) {
  if (!path || !find || !repl || !out || cap == 0) return NULL;
  const char* pos = strstr(path, find);
  if (!pos) return NULL;
  size_t prefix = (size_t)(pos - path);
  size_t find_len = strlen(find);
  size_t repl_len = strlen(repl);
  size_t tail_len = strlen(pos + find_len);
  if (prefix + repl_len + tail_len + 1 > cap) return NULL;
  memcpy(out, path, prefix);
  memcpy(out + prefix, repl, repl_len);
  memcpy(out + prefix + repl_len, pos + find_len, tail_len + 1);
  return out;
}

static const char* cogito_guess_bold_font_path(const char* regular_path) {
  static char buf[PATH_MAX];
  bool has_regular = regular_path && regular_path[0];
  if (has_regular) {
    if (strstr(regular_path, "Bold") || strstr(regular_path, "bold")) {
      if (cogito_file_exists(regular_path)) return regular_path;
    }
    const struct { const char* find; const char* repl; } swaps[] = {
      {"-Regular", "-Bold"}, {"_Regular", "_Bold"}, {" Regular", " Bold"}, {"Regular", "Bold"},
      {"-Roman", "-Bold"}, {"_Roman", "_Bold"}, {" Roman", " Bold"}, {"Roman", "Bold"},
      {"-Book", "-Bold"}, {"_Book", "_Bold"}, {" Book", " Bold"}, {"Book", "Bold"},
      {"-Medium", "-Bold"}, {"_Medium", "_Bold"}, {" Medium", " Bold"}, {"Medium", "Bold"},
      {"-Light", "-Bold"}, {"_Light", "_Bold"}, {" Light", " Bold"}, {"Light", "Bold"},
      {"-Text", "-Bold"}, {"_Text", "_Bold"}, {" Text", " Bold"}, {"Text", "Bold"},
      {"-regular", "-bold"}, {"_regular", "_bold"}, {" regular", " bold"}, {"regular", "bold"},
      {"-roman", "-bold"}, {"_roman", "_bold"}, {" roman", " bold"}, {"roman", "bold"},
      {"-book", "-bold"}, {"_book", "_bold"}, {" book", " bold"}, {"book", "bold"},
      {"-medium", "-bold"}, {"_medium", "_bold"}, {" medium", " bold"}, {"medium", "bold"},
      {"-light", "-bold"}, {"_light", "_bold"}, {" light", " bold"}, {"light", "bold"},
      {"-text", "-bold"}, {"_text", "_bold"}, {" text", " bold"}, {"text", "bold"}
    };
    for (size_t i = 0; i < sizeof(swaps) / sizeof(swaps[0]); i++) {
      if (cogito_replace_once(regular_path, swaps[i].find, swaps[i].repl, buf, sizeof(buf))) {
        if (cogito_file_exists(buf)) return buf;
      }
    }
    const char* dot = strrchr(regular_path, '.');
    if (dot && dot != regular_path) {
      size_t base_len = (size_t)(dot - regular_path);
      if (base_len + 6 + strlen(dot) + 1 < sizeof(buf)) {
        memcpy(buf, regular_path, base_len);
        memcpy(buf + base_len, "-Bold", 5);
        strcpy(buf + base_len + 5, dot);
        if (cogito_file_exists(buf)) return buf;
      }
      if (base_len + 5 + strlen(dot) + 1 < sizeof(buf)) {
        memcpy(buf, regular_path, base_len);
        memcpy(buf + base_len, "Bold", 4);
        strcpy(buf + base_len + 4, dot);
        if (cogito_file_exists(buf)) return buf;
      }
    }
  }
  if (has_regular) {
    // Avoid mixing unrelated font families when we already have a base font.
    return NULL;
  }
#if defined(__APPLE__)
  const char* candidates[] = {
    "/System/Library/Fonts/HelveticaNeue.ttc",
    "/System/Library/Fonts/Helvetica.ttc"
  };
#elif defined(_WIN32)
  const char* candidates[] = {
    "C:\\\\Windows\\\\Fonts\\\\segoeuib.ttf"
  };
#else
  const char* candidates[] = {
    "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"
  };
#endif
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    if (cogito_file_exists(candidates[i])) return candidates[i];
  }
  return NULL;
}

static const char* cogito_guess_mono_font_path(const char* regular_path) {
  if (regular_path && regular_path[0]) {
    if (strstr(regular_path, "SFNS.ttf")) {
      if (cogito_file_exists("/System/Library/Fonts/SFNSMono.ttf")) return "/System/Library/Fonts/SFNSMono.ttf";
    }
    if (strstr(regular_path, "SFNSItalic.ttf")) {
      if (cogito_file_exists("/System/Library/Fonts/SFNSMonoItalic.ttf")) return "/System/Library/Fonts/SFNSMonoItalic.ttf";
    }
    if (strstr(regular_path, "SFNSRounded.ttf")) {
      if (cogito_file_exists("/System/Library/Fonts/SFNSMono.ttf")) return "/System/Library/Fonts/SFNSMono.ttf";
    }
    if (strstr(regular_path, "SFCompact")) {
      if (cogito_file_exists("/System/Library/Fonts/SFNSMono.ttf")) return "/System/Library/Fonts/SFNSMono.ttf";
    }
  }
#if defined(__APPLE__)
  const char* candidates[] = {
    "/System/Library/Fonts/SFNSMono.ttf",
    "/System/Library/Fonts/Menlo.ttc"
  };
#elif defined(_WIN32)
  const char* candidates[] = {
    "C:\\\\Windows\\\\Fonts\\\\consola.ttf"
  };
#else
  const char* candidates[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf"
  };
#endif
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    if (cogito_file_exists(candidates[i])) return candidates[i];
  }
  return NULL;
}

static const char* cogito_guess_serif_font_path(const char* regular_path) {
  static char buf[PATH_MAX];
  bool has_regular = regular_path && regular_path[0];
  if (has_regular) {
    const struct { const char* find; const char* repl; } swaps[] = {
      {"Sans", "Serif"},
      {"sans", "serif"}
    };
    for (size_t i = 0; i < sizeof(swaps) / sizeof(swaps[0]); i++) {
      if (cogito_replace_once(regular_path, swaps[i].find, swaps[i].repl, buf, sizeof(buf))) {
        if (cogito_file_exists(buf)) return buf;
      }
    }
  }
#if defined(__APPLE__)
  const char* candidates[] = {
    "/System/Library/Fonts/Supplemental/Times New Roman.ttf",
    "/System/Library/Fonts/Supplemental/Georgia.ttf",
    "/System/Library/Fonts/NewYork.ttf",
    "/System/Library/Fonts/Times.ttc"
  };
#elif defined(_WIN32)
  const char* candidates[] = {
    "C:\\\\Windows\\\\Fonts\\\\times.ttf",
    "C:\\\\Windows\\\\Fonts\\\\georgia.ttf"
  };
#else
  const char* candidates[] = {
    "/usr/share/fonts/truetype/noto/NotoSerif-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf"
  };
#endif
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    if (cogito_file_exists(candidates[i])) return candidates[i];
  }
  return NULL;
}
