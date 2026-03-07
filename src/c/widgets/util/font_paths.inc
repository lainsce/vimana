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

static const char* cogito_guess_medium_font_path(const char* regular_path) {
  static char buf[PATH_MAX];
  bool has_regular = regular_path && regular_path[0];
  if (has_regular) {
    if (strstr(regular_path, "Medium") || strstr(regular_path, "medium")) {
      if (cogito_file_exists(regular_path)) return regular_path;
    }
    const struct { const char* find; const char* repl; } swaps[] = {
      {"-Regular", "-Medium"}, {"_Regular", "_Medium"}, {" Regular", " Medium"}, {"Regular", "Medium"},
      {"-Roman", "-Medium"}, {"_Roman", "_Medium"}, {" Roman", " Medium"}, {"Roman", "Medium"},
      {"-Book", "-Medium"}, {"_Book", "_Medium"}, {" Book", " Medium"}, {"Book", "Medium"},
      {"-Text", "-Medium"}, {"_Text", "_Medium"}, {" Text", " Medium"}, {"Text", "Medium"},
      {"-Light", "-Medium"}, {"_Light", "_Medium"}, {" Light", " Medium"}, {"Light", "Medium"},
      {"-regular", "-medium"}, {"_regular", "_medium"}, {" regular", " medium"}, {"regular", "medium"},
      {"-roman", "-medium"}, {"_roman", "_medium"}, {" roman", " medium"}, {"roman", "medium"},
      {"-book", "-medium"}, {"_book", "_medium"}, {" book", " medium"}, {"book", "medium"},
      {"-text", "-medium"}, {"_text", "_medium"}, {" text", " medium"}, {"text", "medium"},
      {"-light", "-medium"}, {"_light", "_medium"}, {" light", " medium"}, {"light", "medium"}
    };
    for (size_t i = 0; i < sizeof(swaps) / sizeof(swaps[0]); i++) {
      if (cogito_replace_once(regular_path, swaps[i].find, swaps[i].repl, buf, sizeof(buf))) {
        if (cogito_file_exists(buf)) return buf;
      }
    }
    const char* dot = strrchr(regular_path, '.');
    if (dot && dot != regular_path) {
      size_t base_len = (size_t)(dot - regular_path);
      if (base_len + 7 + strlen(dot) + 1 < sizeof(buf)) {
        memcpy(buf, regular_path, base_len);
        memcpy(buf + base_len, "-Medium", 7);
        strcpy(buf + base_len + 7, dot);
        if (cogito_file_exists(buf)) return buf;
      }
      if (base_len + 6 + strlen(dot) + 1 < sizeof(buf)) {
        memcpy(buf, regular_path, base_len);
        memcpy(buf + base_len, "Medium", 6);
        strcpy(buf + base_len + 6, dot);
        if (cogito_file_exists(buf)) return buf;
      }
    }
  }
  if (has_regular) {
    return NULL;
  }
#if defined(__APPLE__)
  const char* candidates[] = {
    "/System/Library/Fonts/HelveticaNeue.ttc",
    "/System/Library/Fonts/Helvetica.ttc"
  };
#elif defined(_WIN32)
  const char* candidates[] = {
    "C:\\Windows\\Fonts\\segoeui.ttf"
  };
#else
  const char* candidates[] = {
    "/usr/share/fonts/truetype/noto/NotoSans-Medium.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
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
    "C:\\Windows\\Fonts\\times.ttf",
    "C:\\Windows\\Fonts\\georgia.ttf"
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

// Resolve a font family name (e.g. "Roboto Flex") to a file path.
// If the input looks like a file path, it is returned as-is when valid.
#if defined(__APPLE__)
#include <CoreText/CoreText.h>
extern char* cogito_macos_list_font_names(void);

static char* cogito_list_font_family_names(void) {
  return cogito_macos_list_font_names();
}

static const char* cogito_resolve_font_by_name(const char* name) {
  static char resolved[PATH_MAX];
  if (!name || !name[0]) return NULL;
  // Treat as path if it starts with / or .
  if (name[0] == '/' || name[0] == '.') {
    if (cogito_file_exists(name)) return name;
    return NULL;
  }
  {
    const char* ext = strrchr(name, '.');
    if (ext && (strcmp(ext, ".ttf") == 0 || strcmp(ext, ".otf") == 0 ||
                strcmp(ext, ".ttc") == 0 || strcmp(ext, ".woff") == 0)) {
      if (cogito_file_exists(name)) return name;
      return NULL;
    }
  }
  CFStringRef cf_name = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
  if (!cf_name) return NULL;
  CFStringRef keys[] = { kCTFontFamilyNameAttribute };
  CFTypeRef values[] = { cf_name };
  CFDictionaryRef attrs = CFDictionaryCreate(kCFAllocatorDefault,
    (const void**)keys, (const void**)values, 1,
    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFRelease(cf_name);
  if (!attrs) return NULL;
  CTFontDescriptorRef desc = CTFontDescriptorCreateWithAttributes(attrs);
  CFRelease(attrs);
  if (!desc) return NULL;
  CTFontDescriptorRef match = CTFontDescriptorCreateMatchingFontDescriptor(desc, NULL);
  CFRelease(desc);
  if (!match) return NULL;
  CFURLRef url = (CFURLRef)CTFontDescriptorCopyAttribute(match, kCTFontURLAttribute);
  CFRelease(match);
  if (!url) return NULL;
  Boolean ok = CFURLGetFileSystemRepresentation(url, true, (UInt8*)resolved, sizeof(resolved));
  CFRelease(url);
  if (ok && resolved[0]) return resolved;
  return NULL;
}
#elif defined(__linux__) || defined(__FreeBSD__)
static char* cogito_list_font_family_names(void) {
  FILE* fp = popen("fc-list -f '%{family}\\n'", "r");
  if (!fp) return NULL;
  size_t cap = 8192;
  size_t len = 0;
  char* out = (char*)malloc(cap);
  if (!out) {
    pclose(fp);
    return NULL;
  }
  out[0] = '\0';
  char buf[1024];
  while (fgets(buf, sizeof(buf), fp)) {
    size_t chunk = strlen(buf);
    if (len + chunk + 1 > cap) {
      size_t next = cap * 2;
      while (len + chunk + 1 > next) next *= 2;
      char* grown = (char*)realloc(out, next);
      if (!grown) {
        free(out);
        pclose(fp);
        return NULL;
      }
      out = grown;
      cap = next;
    }
    memcpy(out + len, buf, chunk);
    len += chunk;
    out[len] = '\0';
  }
  pclose(fp);
  if (len == 0) {
    free(out);
    return NULL;
  }
  return out;
}

static const char* cogito_resolve_font_by_name(const char* name) {
  static char resolved[PATH_MAX];
  if (!name || !name[0]) return NULL;
  if (name[0] == '/' || name[0] == '.') {
    if (cogito_file_exists(name)) return name;
    return NULL;
  }
  {
    const char* ext = strrchr(name, '.');
    if (ext && (strcmp(ext, ".ttf") == 0 || strcmp(ext, ".otf") == 0 ||
                strcmp(ext, ".ttc") == 0 || strcmp(ext, ".woff") == 0)) {
      if (cogito_file_exists(name)) return name;
      return NULL;
    }
  }
  // Sanitize: only allow alnum, space, hyphen, underscore, period in name
  for (const char* p = name; *p; p++) {
    char c = *p;
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_' || c == '.'))
      return NULL;
  }
  char cmd[512];
  snprintf(cmd, sizeof(cmd), "fc-match -f '%%{file}' \"%s\"", name);
  FILE* fp = popen(cmd, "r");
  if (!fp) return NULL;
  resolved[0] = '\0';
  if (fgets(resolved, sizeof(resolved), fp)) {
    size_t len = strlen(resolved);
    if (len > 0 && resolved[len - 1] == '\n') resolved[len - 1] = '\0';
  }
  pclose(fp);
  if (resolved[0] && cogito_file_exists(resolved)) return resolved;
  return NULL;
}
#elif defined(_WIN32)
static char* cogito_list_font_family_names(void) {
  return NULL;
}

static const char* cogito_resolve_font_by_name(const char* name) {
  static char resolved[PATH_MAX];
  if (!name || !name[0]) return NULL;
  if (name[0] == '/' || name[0] == '.' || (name[0] && name[1] == ':')) {
    if (cogito_file_exists(name)) return name;
    return NULL;
  }
  {
    const char* ext = strrchr(name, '.');
    if (ext && (strcmp(ext, ".ttf") == 0 || strcmp(ext, ".otf") == 0 ||
                strcmp(ext, ".ttc") == 0 || strcmp(ext, ".woff") == 0)) {
      if (cogito_file_exists(name)) return name;
      return NULL;
    }
  }
  // Try common naming patterns in Windows Fonts directory
  const char* exts[] = { ".ttf", ".otf", ".ttc" };
  for (int e = 0; e < 3; e++) {
    snprintf(resolved, sizeof(resolved), "C:\\Windows\\Fonts\\%s%s", name, exts[e]);
    if (cogito_file_exists(resolved)) return resolved;
  }
  // Try with spaces removed
  char compact[256];
  size_t j = 0;
  for (const char* p = name; *p && j + 1 < sizeof(compact); p++) {
    if (*p != ' ') compact[j++] = *p;
  }
  compact[j] = '\0';
  for (int e = 0; e < 3; e++) {
    snprintf(resolved, sizeof(resolved), "C:\\Windows\\Fonts\\%s%s", compact, exts[e]);
    if (cogito_file_exists(resolved)) return resolved;
  }
  return NULL;
}
#else
static char* cogito_list_font_family_names(void) {
  return NULL;
}

static const char* cogito_resolve_font_by_name(const char* name) {
  (void)name;
  return NULL;
}
#endif
