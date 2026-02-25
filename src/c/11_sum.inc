// ---- SUM indentation-based parser ----

// Forward declarations for functions defined in 11_sum_cogito.inc
static void cogito_sum_add_styles(const char* parent, const char* child, CogitoStyle** styles, int* flags, int* count, int max);
static void cogito_sum_apply_property(CogitoStyle* style, const char* prop, const char** p, bool selection_mode);
static bool sum_system_is_dark(void);
static bool sum_system_accent(CogitoColor* out);
static int sum_system_ensor_scheme(void);  // 0=default, 1=vibrant, 2=muted, 3=monochrome, 4=salad
static double sum_system_font_weight(void);  // 0.0 = unset
static double sum_system_roundness(void);  // 0.0 = unset
static double sum_system_contrast(void);  // 0.0 = unset
static int sum_system_spacing(void);  // 0 = unset, 1=compact, 2=normal, 3=comfortable

typedef enum {
  SUM_DIAG_OFF = 0,
  SUM_DIAG_WARN,
  SUM_DIAG_STRICT
} SumDiagMode;

typedef struct {
  char name[64];
  char value[160];
} SumToken;

typedef struct {
  int indent_spaces;
  bool active;
} SumVariantFrame;

static SumDiagMode sum_diag_mode = SUM_DIAG_WARN;
static bool sum_diag_strict_failed = false;
static SumToken sum_tokens[128];
static int sum_tokens_len = 0;

static bool sum_env_truthy(const char* name) {
  const char* v = getenv(name);
  if (!v || !v[0]) return false;
  if (strcmp(v, "1") == 0 || strcmp(v, "true") == 0 || strcmp(v, "yes") == 0 || strcmp(v, "on") == 0) return true;
  return false;
}

static bool sum_system_high_contrast(void) {
  const char* v = getenv("COGITO_CONTRAST");
  if (!v || !v[0]) v = getenv("ERGO_CONTRAST");
  if (v && (strstr(v, "high") || strstr(v, "HIGH"))) return true;
  if (sum_env_truthy("COGITO_HIGH_CONTRAST") || sum_env_truthy("ERGO_HIGH_CONTRAST")) return true;
  return false;
}

static bool sum_system_reduced_motion(void) {
  if (sum_env_truthy("COGITO_REDUCED_MOTION") || sum_env_truthy("ERGO_REDUCED_MOTION")) return true;
  const char* v = getenv("COGITO_MOTION");
  if (!v || !v[0]) v = getenv("ERGO_MOTION");
  if (v && (strstr(v, "reduce") || strstr(v, "REDUCE") || strstr(v, "minimal"))) return true;
  return false;
}

static void sum_diag_emit(bool is_error, int line, const char* fmt, ...) {
  if (sum_diag_mode == SUM_DIAG_OFF) return;
  if (sum_diag_mode == SUM_DIAG_WARN && is_error) is_error = false;
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "%s: line %d: ", (is_error || sum_diag_mode == SUM_DIAG_STRICT) ? "error" : "warn", line);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  if (is_error || sum_diag_mode == SUM_DIAG_STRICT) {
    sum_diag_strict_failed = true;
  }
}

static void sum_trim_trailing(char* s) {
  if (!s) return;
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) {
    s[--n] = 0;
  }
}

static bool sum_buf_append_n(char** buf, size_t* len, size_t* cap, const char* s, size_t n) {
  if (!buf || !len || !cap || !s) return false;
  if (*len + n + 1 > *cap) {
    size_t next = *cap ? *cap * 2 : 1024;
    while (*len + n + 1 > next) next *= 2;
    char* p = (char*)realloc(*buf, next);
    if (!p) return false;
    *buf = p;
    *cap = next;
  }
  memcpy(*buf + *len, s, n);
  *len += n;
  (*buf)[*len] = 0;
  return true;
}

static bool sum_buf_append(char** buf, size_t* len, size_t* cap, const char* s) {
  return sum_buf_append_n(buf, len, cap, s, strlen(s));
}

static int sum_token_find(const char* name) {
  for (int i = 0; i < sum_tokens_len; i++) {
    if (strcmp(sum_tokens[i].name, name) == 0) return i;
  }
  return -1;
}

static void sum_token_set(const char* name, const char* value) {
  if (!name || !name[0] || !value) return;
  int i = sum_token_find(name);
  if (i < 0) {
    if (sum_tokens_len >= (int)(sizeof(sum_tokens) / sizeof(sum_tokens[0]))) return;
    i = sum_tokens_len++;
  }
  strncpy(sum_tokens[i].name, name, sizeof(sum_tokens[i].name) - 1);
  sum_tokens[i].name[sizeof(sum_tokens[i].name) - 1] = 0;
  strncpy(sum_tokens[i].value, value, sizeof(sum_tokens[i].value) - 1);
  sum_tokens[i].value[sizeof(sum_tokens[i].value) - 1] = 0;
}

static bool sum_eval_variant_condition(const char* expr, int line) {
  if (!expr) return false;
  while (*expr == ' ' || *expr == '\t') expr++;
  bool neg = false;
  if (*expr == '!') {
    neg = true;
    expr++;
    while (*expr == ' ' || *expr == '\t') expr++;
  } else if (strncmp(expr, "not ", 4) == 0) {
    neg = true;
    expr += 4;
    while (*expr == ' ' || *expr == '\t') expr++;
  }
  char cond[64];
  size_t ci = 0;
  while (*expr && *expr != ' ' && *expr != '\t') {
    if (ci + 1 < sizeof(cond)) cond[ci++] = *expr;
    expr++;
  }
  cond[ci] = 0;
  if (!cond[0]) {
    sum_diag_emit(false, line, "malformed @when condition");
    return false;
  }

  bool value = false;
  if (strcmp(cond, "dark") == 0) value = sum_system_is_dark();
  else if (strcmp(cond, "light") == 0) value = !sum_system_is_dark();
  else if (strcmp(cond, "high-contrast") == 0 || strcmp(cond, "highcontrast") == 0) value = sum_system_high_contrast();
  else if (strcmp(cond, "reduced-motion") == 0 || strcmp(cond, "reducedmotion") == 0) value = sum_system_reduced_motion();
  else if (strcmp(cond, "default") == 0 || strcmp(cond, "base") == 0) value = true;
  else {
    sum_diag_emit(false, line, "unknown @when condition '%s'", cond);
    return false;
  }
  return neg ? !value : value;
}

static void sum_color_to_hex_any(CogitoColor c, char out[16]) {
  if (c.a == 255) {
    snprintf(out, 16, "#%02x%02x%02x", c.r, c.g, c.b);
  } else {
    snprintf(out, 16, "#%02x%02x%02x%02x", c.r, c.g, c.b, c.a);
  }
}

static bool sum_eval_color_function(const char* expr, char* out, size_t out_cap, int line) {
  const char* p = expr;
  cogito_css_skip_ws(&p);
  if (strncmp(p, "alpha", 5) == 0) {
    p += 5;
    cogito_css_skip_ws(&p);
    if (*p != '(') {
      sum_diag_emit(false, line, "malformed alpha() expression");
      return false;
    }
    p++;
    CogitoColor c = cogito_rgba(0, 0, 0, 255);
    if (!cogito_css_parse_color(&p, &c)) {
      sum_diag_emit(false, line, "alpha() expects a color as first argument");
      return false;
    }
    cogito_css_skip_ws(&p);
    if (*p != ',') {
      sum_diag_emit(false, line, "alpha() missing comma");
      return false;
    }
    p++;
    cogito_css_skip_ws(&p);
    double a = 0.0;
    if (!cogito_css_read_number(&p, &a)) {
      sum_diag_emit(false, line, "alpha() expects number as second argument");
      return false;
    }
    bool percent = false;
    if (*p == '%') {
      percent = true;
      p++;
    }
    cogito_css_skip_ws(&p);
    if (*p != ')') {
      sum_diag_emit(false, line, "alpha() missing closing ')'");
      return false;
    }
    p++;
    cogito_css_skip_ws(&p);
    if (*p) {
      sum_diag_emit(false, line, "alpha() must be the full value");
      return false;
    }
    double factor = percent ? (a / 100.0) : (a <= 1.0 ? a : (a / 255.0));
    if (factor < 0.0) factor = 0.0;
    if (factor > 1.0) factor = 1.0;
    int na = (int)lround((double)c.a * factor);
    CogitoColor r = cogito_rgba(c.r, c.g, c.b, na);
    char hex[16];
    sum_color_to_hex_any(r, hex);
    snprintf(out, out_cap, "%s", hex);
    return true;
  }
  if (strncmp(p, "mix", 3) == 0) {
    p += 3;
    cogito_css_skip_ws(&p);
    if (*p != '(') {
      sum_diag_emit(false, line, "malformed mix() expression");
      return false;
    }
    p++;
    CogitoColor a = cogito_rgba(0, 0, 0, 255), b = cogito_rgba(0, 0, 0, 255);
    if (!cogito_css_parse_color(&p, &a)) {
      sum_diag_emit(false, line, "mix() expects color as first argument");
      return false;
    }
    cogito_css_skip_ws(&p);
    if (*p != ',') {
      sum_diag_emit(false, line, "mix() missing first comma");
      return false;
    }
    p++;
    if (!cogito_css_parse_color(&p, &b)) {
      sum_diag_emit(false, line, "mix() expects color as second argument");
      return false;
    }
    cogito_css_skip_ws(&p);
    if (*p != ',') {
      sum_diag_emit(false, line, "mix() missing second comma");
      return false;
    }
    p++;
    cogito_css_skip_ws(&p);
    double t = 0.0;
    if (!cogito_css_read_number(&p, &t)) {
      sum_diag_emit(false, line, "mix() expects number as third argument");
      return false;
    }
    if (*p == '%') {
      t = t / 100.0;
      p++;
    }
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    cogito_css_skip_ws(&p);
    if (*p != ')') {
      sum_diag_emit(false, line, "mix() missing closing ')'");
      return false;
    }
    p++;
    cogito_css_skip_ws(&p);
    if (*p) {
      sum_diag_emit(false, line, "mix() must be the full value");
      return false;
    }
    CogitoColor r = cogito_mix(a, b, (float)t);
    char hex[16];
    sum_color_to_hex_any(r, hex);
    snprintf(out, out_cap, "%s", hex);
    return true;
  }
  return false;
}

static void sum_expand_value(const char* in, char* out, size_t out_cap, int line) {
  char tmp[512];
  size_t wi = 0;
  const char* p = in ? in : "";
  while (*p && wi + 1 < sizeof(tmp)) {
    if (*p == '@') {
      const char* s = p + 1;
      char name[64];
      size_t ni = 0;
      while (*s && (isalnum((unsigned char)*s) || *s == '_' || *s == '-')) {
        if (ni + 1 < sizeof(name)) name[ni++] = *s;
        s++;
      }
      name[ni] = 0;
      if (ni > 0) {
        int idx = sum_token_find(name);
        if (idx >= 0) {
          const char* v = sum_tokens[idx].value;
          while (*v && wi + 1 < sizeof(tmp)) tmp[wi++] = *v++;
        } else {
          sum_diag_emit(false, line, "unknown theme token '@%s'", name);
        }
        p = s;
        continue;
      }
    }
    tmp[wi++] = *p++;
  }
  tmp[wi] = 0;

  char fn_eval[128];
  if (sum_eval_color_function(tmp, fn_eval, sizeof(fn_eval), line)) {
    snprintf(out, out_cap, "%s", fn_eval);
  } else {
    snprintf(out, out_cap, "%s", tmp);
  }
}

static char* sum_preprocess_source(const char* src, size_t* out_len) {
  char* out = NULL;
  size_t out_size = 0, out_cap = 0;
  SumVariantFrame stack[16];
  int depth = 0;
  int line_no = 1;
  const char* p = src;
  while (*p) {
    const char* ls = p;
    const char* le = ls;
    while (*le && *le != '\n') le++;
    size_t llen = (size_t)(le - ls);
    int spaces = 0;
    while (spaces < (int)llen && ls[spaces] == ' ') spaces++;

    char line[512];
    size_t use_len = llen >= sizeof(line) ? sizeof(line) - 1 : llen;
    memcpy(line, ls, use_len);
    line[use_len] = 0;
    sum_trim_trailing(line);
    char* semi = strchr(line, ';');
    if (semi) *semi = 0;
    const char* t = line;
    while (*t == ' ' || *t == '\t') t++;
    bool blank = (*t == 0);

    if (!blank) {
      while (depth > 0 && spaces <= stack[depth - 1].indent_spaces) depth--;
    }
    bool active = true;
    for (int i = 0; i < depth; i++) {
      if (!stack[i].active) {
        active = false;
        break;
      }
    }

    if (*t == '@') {
      if (strncmp(t, "@diagnostics", 12) == 0) {
        const char* c = strchr(t, ':');
        if (!c) sum_diag_emit(false, line_no, "malformed @diagnostics directive");
        else {
          c++;
          while (*c == ' ' || *c == '\t') c++;
          if (strncmp(c, "off", 3) == 0) sum_diag_mode = SUM_DIAG_OFF;
          else if (strncmp(c, "warn", 4) == 0) sum_diag_mode = SUM_DIAG_WARN;
          else if (strncmp(c, "strict", 6) == 0) sum_diag_mode = SUM_DIAG_STRICT;
          else sum_diag_emit(false, line_no, "unknown diagnostics mode '%s'", c);
        }
      } else if (strncmp(t, "@when", 5) == 0) {
        const char* expr = t + 5;
        while (*expr == ' ' || *expr == '\t') expr++;
        bool cond = sum_eval_variant_condition(expr, line_no);
        if (depth < (int)(sizeof(stack) / sizeof(stack[0]))) {
          stack[depth].indent_spaces = spaces;
          stack[depth].active = active && cond;
          depth++;
        } else {
          sum_diag_emit(false, line_no, "variant nesting too deep");
        }
      } else {
        const char* c = strchr(t, ':');
        if (c) {
          char name[64];
          size_t ni = 0;
          const char* np = t + 1;
          while (np < c && (isalnum((unsigned char)*np) || *np == '_' || *np == '-')) {
            if (ni + 1 < sizeof(name)) name[ni++] = *np;
            np++;
          }
          name[ni] = 0;
          if (!name[0]) {
            sum_diag_emit(false, line_no, "malformed token declaration");
          } else if (active) {
            char value[192];
            const char* vp = c + 1;
            while (*vp == ' ' || *vp == '\t') vp++;
            snprintf(value, sizeof(value), "%s", vp);
            sum_trim_trailing(value);
            sum_token_set(name, value);
          }
        } else if (active) {
          sum_diag_emit(false, line_no, "malformed directive '%s'", t);
        }
      }
    } else if (active) {
      int strip = depth * 2;
      int lead = spaces;
      if (strip > lead) strip = lead;
      const char* emit = ls + strip;
      size_t emit_len = llen - (size_t)strip;
      if (!sum_buf_append_n(&out, &out_size, &out_cap, emit, emit_len)) {
        free(out);
        return NULL;
      }
      if (*le == '\n') {
        if (!sum_buf_append(&out, &out_size, &out_cap, "\n")) {
          free(out);
          return NULL;
        }
      }
    }

    if (*le == '\n') {
      p = le + 1;
      line_no++;
    } else {
      p = le;
    }
  }
  if (!out) {
    out = (char*)malloc(1);
    if (!out) return NULL;
    out[0] = 0;
  }
  if (out_len) *out_len = out_size;
  return out;
}

static bool sum_line_is_blank_or_comment(const char* line, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char c = line[i];
    if (c == ';') return true;
    if (c != ' ' && c != '\t') return false;
  }
  return true;
}

static inline int sum_indent_level(const char *line, size_t len) {
  const unsigned char *p   = (const unsigned char *)line;
  const unsigned char *end = p + len;

  size_t spaces = 0;

  while (p < end) {
    unsigned char c = *p;
    if (c == ' ') {
      ++spaces;
      ++p;
      continue;
    }
    if (c == '\t') {
      ++p;
      continue;
    }
    break;
  }

  return (int)(spaces >> 1);
}

static void sum_parse(const char* src) {
  if (!src || !*src) return;
  cogito_style_version++;
  sum_tokens_len = 0;
  sum_diag_strict_failed = false;
  sum_diag_mode = SUM_DIAG_WARN;

  size_t pre_len = 0;
  char* pre = sum_preprocess_source(src, &pre_len);
  if (!pre) return;
  if (sum_diag_strict_failed && sum_diag_mode == SUM_DIAG_STRICT) {
    free(pre);
    return;
  }

  const char* p = pre;
  int line_no = 1;

  while (*p) {
    const char* line_start = p;
    const char* line_end = line_start;
    while (*line_end && *line_end != '\n') line_end++;
    size_t line_len = (size_t)(line_end - line_start);

    if (sum_line_is_blank_or_comment(line_start, line_len)) {
      p = *line_end ? line_end + 1 : line_end;
      line_no++;
      continue;
    }

    int indent = sum_indent_level(line_start, line_len);
    if (indent > 0) {
      p = *line_end ? line_end + 1 : line_end;
      line_no++;
      continue;
    }

    CogitoStyle* styles[32];
    int style_flags[32] = {0};
    int style_count = 0;
    while (*p) {
      const char* sel_start = p;
      const char* sel_end = sel_start;
      while (*sel_end && *sel_end != '\n') sel_end++;
      size_t sel_len = (size_t)(sel_end - sel_start);

      if (sum_line_is_blank_or_comment(sel_start, sel_len)) {
        p = *sel_end ? sel_end + 1 : sel_end;
        line_no++;
        continue;
      }

      int sel_indent = sum_indent_level(sel_start, sel_len);
      if (sel_indent > 0) break;

      char sel_line[256];
      if (sel_len >= sizeof(sel_line)) sel_len = sizeof(sel_line) - 1;
      memcpy(sel_line, sel_start, sel_len);
      sel_line[sel_len] = 0;
      char* semi = strchr(sel_line, ';');
      if (semi) *semi = 0;
      sum_trim_trailing(sel_line);

      const char* sp = sel_line;
      while (*sp) {
        while (*sp == ' ' || *sp == '\t') sp++;
        if (!*sp) break;

        char parent[64] = {0};
        size_t pi = 0;
        while (*sp && *sp != ' ' && *sp != '\t' && *sp != ',') {
          if (pi < sizeof(parent) - 1) parent[pi++] = *sp;
          sp++;
        }
        parent[pi] = 0;
        while (*sp == ' ' || *sp == '\t') sp++;

        char child[128] = {0};
        if (*sp && *sp != ',') {
          size_t ci = 0;
          while (*sp && *sp != ',' && *sp != ' ' && *sp != '\t') {
            if (ci < sizeof(child) - 1) child[ci++] = *sp;
            sp++;
          }
          child[ci] = 0;
        }

        if (style_count < (int)(sizeof(styles) / sizeof(styles[0]))) {
          cogito_sum_add_styles(parent, child[0] ? child : NULL, styles, style_flags, &style_count, (int)(sizeof(styles) / sizeof(styles[0])));
        }
        while (*sp == ' ' || *sp == '\t') sp++;
        if (*sp == ',') { sp++; continue; }
        break;
      }
      p = *sel_end ? sel_end + 1 : sel_end;
      line_no++;
    }

    if (style_count == 0) {
      while (*p) {
        const char* bl = p;
        const char* be = bl;
        while (*be && *be != '\n') be++;
        size_t blen = (size_t)(be - bl);
        if (sum_line_is_blank_or_comment(bl, blen)) {
          p = *be ? be + 1 : be;
          line_no++;
          continue;
        }
        if (sum_indent_level(bl, blen) == 0) break;
        p = *be ? be + 1 : be;
        line_no++;
      }
      continue;
    }

    bool ml_active = false;
    char ml_prop[64] = {0};
    char ml_val[512] = {0};
    int ml_line_no = 0;
    while (*p) {
      const char* dl = p;
      const char* de = dl;
      while (*de && *de != '\n') de++;
      size_t dlen = (size_t)(de - dl);

      if (sum_line_is_blank_or_comment(dl, dlen)) {
        p = *de ? de + 1 : de;
        line_no++;
        continue;
      }

      int di = sum_indent_level(dl, dlen);
      if (di == 0) break;

      const char* dc = dl;
      while (*dc == ' ' || *dc == '\t') dc++;

      char decl[256];
      size_t dremain = (size_t)(de - dc);
      if (dremain >= sizeof(decl)) dremain = sizeof(decl) - 1;
      memcpy(decl, dc, dremain);
      decl[dremain] = 0;
      char* dsemi = strchr(decl, ';');
      if (dsemi) *dsemi = 0;
      sum_trim_trailing(decl);

      if (ml_active) {
        if (decl[0]) {
          if (strlen(ml_val) + strlen(decl) + 2 < sizeof(ml_val)) {
            strcat(ml_val, " ");
            strcat(ml_val, decl);
          }
        }
        if (strchr(decl, ']')) {
          char expanded[256];
          sum_expand_value(ml_val, expanded, sizeof(expanded), ml_line_no);
          for (int i = 0; i < style_count; i++) {
            const char* v = expanded;
            cogito_sum_apply_property(styles[i], ml_prop, &v, style_flags[i] != 0);
          }
          ml_active = false;
          ml_prop[0] = 0;
          ml_val[0] = 0;
          ml_line_no = 0;
        }
        p = *de ? de + 1 : de;
        line_no++;
        continue;
      }

      char prop[64] = {0};
      const char* dp = decl;
      size_t ppi = 0;
      while (*dp && *dp != ':' && *dp != ' ' && *dp != '\t') {
        if (ppi < sizeof(prop) - 1) prop[ppi++] = *dp;
        dp++;
      }
      prop[ppi] = 0;
      while (*dp == ' ' || *dp == '\t') dp++;
      if (*dp != ':') {
        sum_diag_emit(false, line_no, "malformed declaration '%s' (expected ':')", decl);
        p = *de ? de + 1 : de;
        line_no++;
        continue;
      }
      dp++;
      while (*dp == ' ' || *dp == '\t') dp++;

      char val_buf[256];
      {
        size_t vi = 0;
        const char* vp = dp;
        while (*vp && vi < sizeof(val_buf) - 1) {
          if (vp[0] == 's' && vp[1] == 'p' && vi > 0 && (val_buf[vi - 1] >= '0' && val_buf[vi - 1] <= '9')) {
            vp += 2;
            continue;
          }
          val_buf[vi++] = *vp++;
        }
        val_buf[vi] = 0;
      }

      // Support multi-line bracket values, e.g.
      // box-shadow: [
      //   0 2 8 0 #0000001f
      // ]
      if (strchr(val_buf, '[') && !strchr(val_buf, ']')) {
        ml_active = true;
        snprintf(ml_prop, sizeof(ml_prop), "%s", prop);
        snprintf(ml_val, sizeof(ml_val), "%s", val_buf);
        ml_line_no = line_no;
        p = *de ? de + 1 : de;
        line_no++;
        continue;
      }

      char expanded[256];
      sum_expand_value(val_buf, expanded, sizeof(expanded), line_no);

      for (int i = 0; i < style_count; i++) {
        const char* v = expanded;
        cogito_sum_apply_property(styles[i], prop, &v, style_flags[i] != 0);
      }

      p = *de ? de + 1 : de;
      line_no++;
    }
    if (sum_diag_strict_failed && sum_diag_mode == SUM_DIAG_STRICT) break;
  }

  free(pre);
}

#define SUM_BRING_MAX_DEPTH 16

static char* sum_read_file_raw(const char* path, size_t* out_len) {
  if (!path || !path[0]) return NULL;
  FILE* f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  if (len < 0) { fclose(f); return NULL; }
  fseek(f, 0, SEEK_SET);
  char* buf = (char*)malloc((size_t)len + 1);
  if (!buf) { fclose(f); return NULL; }
  size_t got = fread(buf, 1, (size_t)len, f);
  fclose(f);
  buf[got] = 0;
  if (out_len) *out_len = got;
  return buf;
}

static bool sum_path_is_absolute(const char* path) {
  if (!path || !path[0]) return false;
#if defined(_WIN32)
  if ((isalpha((unsigned char)path[0]) && path[1] == ':') ||
      path[0] == '\\' || path[0] == '/') return true;
  return false;
#else
  return path[0] == '/';
#endif
}

static void sum_path_dirname(const char* path, char* out, size_t out_cap) {
  if (!out || out_cap == 0) return;
  out[0] = 0;
  if (!path || !path[0]) {
    snprintf(out, out_cap, ".");
    return;
  }
  const char* last = strrchr(path, '/');
#if defined(_WIN32)
  const char* back = strrchr(path, '\\');
  if (!last || (back && back > last)) last = back;
#endif
  if (!last) {
    snprintf(out, out_cap, ".");
    return;
  }
  size_t n = (size_t)(last - path);
  if (n == 0) {
    snprintf(out, out_cap, "/");
    return;
  }
  if (n >= out_cap) n = out_cap - 1;
  memcpy(out, path, n);
  out[n] = 0;
}

static void sum_path_join(const char* base_dir, const char* rel, char* out, size_t out_cap) {
  if (!out || out_cap == 0) return;
  out[0] = 0;
  if (!rel || !rel[0]) return;
  if (sum_path_is_absolute(rel) || !base_dir || !base_dir[0] || strcmp(base_dir, ".") == 0) {
    snprintf(out, out_cap, "%s", rel);
    return;
  }
  size_t bl = strlen(base_dir);
  bool has_sep = bl > 0 && (base_dir[bl - 1] == '/' || base_dir[bl - 1] == '\\');
  if (has_sep) snprintf(out, out_cap, "%s%s", base_dir, rel);
  else snprintf(out, out_cap, "%s/%s", base_dir, rel);
}

static void sum_path_normalize(const char* path, char* out, size_t out_cap) {
  if (!out || out_cap == 0) return;
  out[0] = 0;
  if (!path || !path[0]) return;
#if defined(_WIN32)
  char resolved[PATH_MAX];
  if (_fullpath(resolved, path, sizeof(resolved))) {
    snprintf(out, out_cap, "%s", resolved);
  } else {
    snprintf(out, out_cap, "%s", path);
  }
#else
  char resolved[PATH_MAX];
  if (realpath(path, resolved)) {
    snprintf(out, out_cap, "%s", resolved);
  } else {
    snprintf(out, out_cap, "%s", path);
  }
#endif
}

static bool sum_parse_bring_target(const char* line, char* out, size_t out_cap) {
  if (!line || !out || out_cap == 0) return false;
  out[0] = 0;
  if (strncmp(line, "@bring", 6) != 0) return false;
  char c = line[6];
  if (!(c == 0 || c == ':' || c == ' ' || c == '\t')) return false;
  const char* p = line + 6;
  while (*p == ' ' || *p == '\t') p++;
  if (*p == ':') {
    p++;
    while (*p == ' ' || *p == '\t') p++;
  }
  if (!*p) return false;
  if (*p == '"') {
    p++;
    size_t i = 0;
    while (*p && *p != '"') {
      if (i + 1 < out_cap) out[i++] = *p;
      p++;
    }
    if (*p != '"') return false;
    out[i] = 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return *p == 0 && out[0] != 0;
  }
  size_t i = 0;
  while (*p && *p != ' ' && *p != '\t') {
    if (i + 1 < out_cap) out[i++] = *p;
    p++;
  }
  out[i] = 0;
  while (*p == ' ' || *p == '\t') p++;
  return *p == 0 && out[0] != 0;
}

static bool sum_path_in_stack(const char* path, char stack[][PATH_MAX], int depth) {
  if (!path || !path[0]) return false;
  for (int i = 0; i < depth; i++) {
    if (strcmp(path, stack[i]) == 0) return true;
  }
  return false;
}

static bool sum_append_file_composed(const char* path, char stack[][PATH_MAX], int depth, char** out, size_t* out_len, size_t* out_cap) {
  if (!path || !path[0] || !stack || !out || !out_len || !out_cap) return false;
  if (depth >= SUM_BRING_MAX_DEPTH) {
    sum_diag_emit(false, 1, "@bring nesting exceeds max depth (%d): %s", SUM_BRING_MAX_DEPTH, path);
    return false;
  }

  char normalized[PATH_MAX];
  sum_path_normalize(path, normalized, sizeof(normalized));
  if (sum_path_in_stack(normalized, stack, depth)) {
    sum_diag_emit(false, 1, "@bring cycle detected: %s", normalized);
    return false;
  }
  snprintf(stack[depth], sizeof(stack[depth]), "%s", normalized);

  size_t src_len = 0;
  char* src = sum_read_file_raw(path, &src_len);
  if (!src) return false;

  char base_dir[PATH_MAX];
  sum_path_dirname(path, base_dir, sizeof(base_dir));

  int line_no = 1;
  const char* p = src;
  while (*p) {
    const char* ls = p;
    const char* le = ls;
    while (*le && *le != '\n') le++;
    size_t llen = (size_t)(le - ls);

    char line[512];
    size_t use_len = llen >= sizeof(line) ? sizeof(line) - 1 : llen;
    memcpy(line, ls, use_len);
    line[use_len] = 0;
    sum_trim_trailing(line);
    char* semi = strchr(line, ';');
    if (semi) *semi = 0;
    const char* t = line;
    while (*t == ' ' || *t == '\t') t++;

    char bring_rel[PATH_MAX];
    if (sum_parse_bring_target(t, bring_rel, sizeof(bring_rel))) {
      char bring_path[PATH_MAX];
      sum_path_join(base_dir, bring_rel, bring_path, sizeof(bring_path));
      if (!sum_append_file_composed(bring_path, stack, depth + 1, out, out_len, out_cap)) {
        sum_diag_emit(false, line_no, "failed to @bring '%s' from %s", bring_rel, path);
      }
    } else {
      bool looks_like_bring = strncmp(t, "@bring", 6) == 0 &&
        (t[6] == 0 || t[6] == ':' || t[6] == ' ' || t[6] == '\t');
      if (looks_like_bring) {
        sum_diag_emit(false, line_no, "malformed @bring directive");
      } else {
        if (!sum_buf_append_n(out, out_len, out_cap, ls, llen)) {
          free(src);
          return false;
        }
        if (*le == '\n') {
          if (!sum_buf_append_n(out, out_len, out_cap, "\n", 1)) {
            free(src);
            return false;
          }
        }
      }
    }

    if (*le == '\n') {
      p = le + 1;
      line_no++;
    } else {
      p = le;
    }
  }

  free(src);
  return true;
}

static char* sum_read_file(const char* path, size_t* out_len) {
  if (!path || !path[0]) return NULL;
  char* out = NULL;
  size_t out_size = 0;
  size_t out_cap = 0;
  char stack[SUM_BRING_MAX_DEPTH][PATH_MAX];
  if (!sum_append_file_composed(path, stack, 0, &out, &out_size, &out_cap)) {
    free(out);
    return NULL;
  }
  if (!out) {
    out = (char*)malloc(1);
    if (!out) return NULL;
    out[0] = 0;
  }
  if (out_len) *out_len = out_size;
  return out;
}

static const char* sum_default_path(void) {
  const char* stdlib = getenv("ERGO_STDLIB");
  if (stdlib && stdlib[0]) {
    static char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s/cogito_default.sum", stdlib);
    if (cogito_file_exists(buf)) return buf;
  }
  if (cogito_file_exists("cogito/cogito_default.sum")) return "cogito/cogito_default.sum";
  if (cogito_file_exists("cogito_default.sum")) return "cogito_default.sum";
  return NULL;
}

static const char* sum_dark_path(void) {
  const char* stdlib = getenv("ERGO_STDLIB");
  if (stdlib && stdlib[0]) {
    static char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s/cogito_dark.sum", stdlib);
    if (cogito_file_exists(buf)) return buf;
  }
  if (cogito_file_exists("cogito/cogito_dark.sum")) return "cogito/cogito_dark.sum";
  if (cogito_file_exists("cogito_dark.sum")) return "cogito_dark.sum";
  return NULL;
}

static bool sum_streq_ci(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    a++;
    b++;
  }
  return *a == 0 && *b == 0;
}

static bool sum_contains_ci(const char* haystack, const char* needle) {
  if (!haystack || !needle || !needle[0]) return false;
  size_t nlen = strlen(needle);
  for (const char* p = haystack; *p; p++) {
    size_t i = 0;
    while (i < nlen && p[i] &&
           tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
      i++;
    }
    if (i == nlen) return true;
  }
  return false;
}

static bool sum_env_dark_true(const char* name) {
  const char* v = getenv(name);
  if (!v || !v[0]) return false;
  if (sum_streq_ci(v, "1") || sum_streq_ci(v, "true") || sum_streq_ci(v, "yes") || sum_streq_ci(v, "on")) {
    return true;
  }
  if (sum_contains_ci(v, "dark")) return true;
  return false;
}

static bool sum_system_is_dark(void) {
  if (sum_env_dark_true("COGITO_DARK") || sum_env_dark_true("ERGO_DARK")) {
    return true;
  }
#if defined(__APPLE__)
  Class udc = (Class)objc_getClass("NSUserDefaults");
  if (!udc) return false;
  SEL sel_std = sel_registerName("standardUserDefaults");
  id defaults = ((id(*)(Class, SEL))objc_msgSend)(udc, sel_std);
  if (!defaults) return false;
  SEL sel_str = sel_registerName("stringForKey:");
  id key = cogito_nsstring("AppleInterfaceStyle");
  id style = ((id(*)(id, SEL, id))objc_msgSend)(defaults, sel_str, key);
  if (!style) return false;
  SEL sel_utf8 = sel_registerName("UTF8String");
  const char* s = ((const char*(*)(id, SEL))objc_msgSend)(style, sel_utf8);
  return s && strcmp(s, "Dark") == 0;
#elif defined(_WIN32)
  bool dark = false;
  DWORD val = 1;
  DWORD size = sizeof(val);
  if (RegGetValueA(HKEY_CURRENT_USER,
                   "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   "AppsUseLightTheme",
                   RRF_RT_REG_DWORD, NULL, &val, &size) == ERROR_SUCCESS && val == 0) {
    dark = true;
  }
  val = 1;
  size = sizeof(val);
  if (RegGetValueA(HKEY_CURRENT_USER,
                   "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   "SystemUsesLightTheme",
                   RRF_RT_REG_DWORD, NULL, &val, &size) == ERROR_SUCCESS && val == 0) {
    dark = true;
  }
  return dark;
#else
  const char* gtk = getenv("GTK_THEME");
  if (sum_contains_ci(gtk, "dark")) return true;
  const char* gnome = getenv("GNOME_THEME");
  if (sum_contains_ci(gnome, "dark")) return true;
  const char* color_scheme = getenv("COLORSCHEME");
  if (sum_contains_ci(color_scheme, "prefer-dark") || sum_contains_ci(color_scheme, "dark")) return true;
  const char* kde_scheme = getenv("KDE_COLOR_SCHEME");
  if (sum_contains_ci(kde_scheme, "dark")) return true;
  const char* qt_style = getenv("QT_STYLE_OVERRIDE");
  if (sum_contains_ci(qt_style, "dark")) return true;
  return false;
#endif
}

static bool sum_system_accent(CogitoColor* out) {
#if defined(__APPLE__)
  // Prefer AppleHighlightColor (string) to avoid accent index mismatches.
  Class udc = (Class)objc_getClass("NSUserDefaults");
  if (!udc) return false;
  SEL sel_std = sel_registerName("standardUserDefaults");
  id defaults = ((id(*)(Class, SEL))objc_msgSend)(udc, sel_std);
  if (!defaults) return false;
  SEL sel_str = sel_registerName("stringForKey:");
  id key_hl = cogito_nsstring("AppleHighlightColor");
  id hl = ((id(*)(id, SEL, id))objc_msgSend)(defaults, sel_str, key_hl);
  if (hl) {
    SEL sel_utf8 = sel_registerName("UTF8String");
    const char* s = ((const char*(*)(id, SEL))objc_msgSend)(hl, sel_utf8);
    if (s && *s) {
      char* end = NULL;
      double r = strtod(s, &end);
      if (end && end != s) {
        double g = strtod(end, &end);
        if (end) {
          double b = strtod(end, &end);
          if (r >= 0.0 && g >= 0.0 && b >= 0.0) {
            if (r > 1.0) r = 1.0;
            if (g > 1.0) g = 1.0;
            if (b > 1.0) b = 1.0;
            *out = cogito_rgba((int)(r * 255.0 + 0.5),
                               (int)(g * 255.0 + 0.5),
                               (int)(b * 255.0 + 0.5),
                               255);
            return true;
          }
        }
      }
    }
  }
  SEL sel_obj = sel_registerName("objectForKey:");
  id key = cogito_nsstring("AppleAccentColor");
  id num = ((id(*)(id, SEL, id))objc_msgSend)(defaults, sel_obj, key);
  if (!num) return false;
  SEL sel_int = sel_registerName("integerValue");
  int v = (int)((long(*)(id, SEL))objc_msgSend)(num, sel_int);
  CogitoColor c = cogito_rgba(10, 132, 255, 255);
  switch (v) {
    case -1: c = cogito_rgba(10, 132, 255, 255); break;   // blue
    case 0: c = cogito_rgba(142, 142, 147, 255); break;   // graphite
    case 1: c = cogito_rgba(191, 90, 242, 255); break;    // purple
    case 2: c = cogito_rgba(255, 55, 95, 255); break;     // pink
    case 3: c = cogito_rgba(255, 59, 48, 255); break;     // red
    case 4: c = cogito_rgba(255, 159, 10, 255); break;    // orange
    case 5: c = cogito_rgba(255, 214, 10, 255); break;    // yellow
    case 6: c = cogito_rgba(50, 215, 75, 255); break;     // green
    default: break;
  }
  *out = c;
  return true;
#elif defined(_WIN32)
  DWORD val = 0;
  DWORD size = sizeof(val);
  if (RegGetValueA(HKEY_CURRENT_USER,
                   "Software\\Microsoft\\Windows\\DWM",
                   "ColorizationColor",
                   RRF_RT_REG_DWORD, NULL, &val, &size) == ERROR_SUCCESS) {
    CogitoColor c = cogito_rgba((val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF, 255);
    *out = c;
    return true;
  }
  return false;
#else
  (void)out;
  return false;
#endif
}

// ---- Desktop Portal / System Properties (port of Desktop.vala + Portal.vala) ----

#if !defined(__APPLE__) && !defined(_WIN32)
// Linux: Try D-Bus portal first, then fall back to environment variables

// Parse accent color from portal D-Bus response (RGB array as "[ddd, ddd, ddd]")
static bool sum_parse_accent_color(const char* s, CogitoColor* out) {
  if (!s || s[0] != '[') return false;
  int r = 0, g = 0, b = 0;
  if (sscanf(s, "[%d, %d, %d]", &r, &g, &b) == 3 ||
      sscanf(s, "[%d,%d,%d]", &r, &g, &b) == 3) {
    if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
      *out = cogito_rgba(r, g, b, 255);
      return true;
    }
  }
  return false;
}

// Check if D-Bus portal is available and read settings
// This is a simplified version - full implementation would use GDBus
static bool sum_portal_read(const char* namespace, const char* key, char* buf, size_t bufsize) {
  // For now, we use a simple approach via gsettings or reading from portal files
  // Full D-Bus implementation would require GDBus linkage
  (void)namespace;
  (void)key;
  (void)buf;
  (void)bufsize;
  return false;
}
#endif

// Ensor scheme: 0=default, 1=vibrant, 2=muted, 3=monochrome, 4=salad
static int sum_system_ensor_scheme(void) {
#if defined(__APPLE__)
  // macOS: Check for custom ensor-scheme via defaults
  // Could be extended to read from a plist or environment variable
  /* future implementation */
  return 0;
#elif defined(_WIN32)
  // Windows: Could check registry for custom scheme
  (void)/* future implementation */;
  return 0;
#else
  // Linux: Check environment variables first
  const char* env = getenv("COGITO_ENSOR_SCHEME");
  if (env) {
    if (strcmp(env, "vibrant") == 0) return 1;
    if (strcmp(env, "muted") == 0) return 2;
    if (strcmp(env, "monochrome") == 0) return 3;
    if (strcmp(env, "salad") == 0) return 4;
    if (strcmp(env, "default") == 0) return 0;
  }
  // Check for portal D-Bus value (future)
  return 0;
#endif
}

// Font weight: 0.0 = unset, otherwise 100-900
static double sum_system_font_weight(void) {
#if defined(__APPLE__)
  // Could read from NSUserDefaults
  return 0.0;
#elif defined(_WIN32)
  // Could read from registry
  return 0.0;
#else
  const char* env = getenv("COGITO_FONT_WEIGHT");
  if (env) {
    double w = atof(env);
    if (w >= 100.0 && w <= 900.0) return w;
  }
  return 0.0;
#endif
}

// Roundness: 0.0 = unset, otherwise 0.0-2.0+ (multiplier)
static double sum_system_roundness(void) {
#if defined(__APPLE__)
  return 0.0;
#elif defined(_WIN32)
  return 0.0;
#else
  const char* env = getenv("COGITO_ROUNDNESS");
  if (env) {
    double r = atof(env);
    if (r >= 0.0) return r;
  }
  return 0.0;
#endif
}

// Contrast: 0.0 = unset, -1.0 to 1.0
static double sum_system_contrast(void) {
#if defined(__APPLE__)
  return 0.0;
#elif defined(_WIN32)
  return 0.0;
#else
  const char* env = getenv("COGITO_CONTRAST");
  if (env) {
    double c = atof(env);
    if (c >= -1.0 && c <= 1.0) return c;
  }
  return 0.0;
#endif
}

// Spacing: 0 = unset, 1=compact, 2=normal, 3=comfortable
static int sum_system_spacing(void) {
#if defined(__APPLE__)
  return 0;
#elif defined(_WIN32)
  return 0;
#else
  const char* env = getenv("COGITO_SPACING");
  if (env) {
    if (strcmp(env, "compact") == 0) return 1;
    if (strcmp(env, "normal") == 0) return 2;
    if (strcmp(env, "comfortable") == 0) return 3;
    int v = atoi(env);
    if (v >= 1 && v <= 3) return v;
  }
  return 0;
#endif
}

#include "ensor/ensor.inc"
// Forward declaration for tertiary color caching (defined in theme_colors.inc)
static void cogito_cache_tertiary_colors(CogitoEnsorScheme* scheme);
#include "widgets/util/sum_dynamic.inc"
#include "widgets/util/theme_colors.inc"
