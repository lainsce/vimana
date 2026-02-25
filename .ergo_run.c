// ---- Ergo runtime (minimal) ----
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#else
int isatty(int);
int fileno(FILE*);
#endif

static int ergo_stdout_isatty = 0;

static bool cogito_debug_enabled(void);
static const char* cogito_font_path_active = NULL;

static void ergo_runtime_init(void) {
#if defined(__APPLE__)
  if (cogito_debug_enabled()) {
    fprintf(stderr, "cogito: runtime_init\n");
    fflush(stderr);
  }
#endif
#if defined(_WIN32)
  ergo_stdout_isatty = _isatty(_fileno(stdout));
#else
  ergo_stdout_isatty = isatty(fileno(stdout));
#endif
  if (!ergo_stdout_isatty) {
    setvbuf(stdout, NULL, _IOFBF, 1 << 16);
  }
}

typedef enum {
  EVT_NULL,
  EVT_INT,
  EVT_FLOAT,
  EVT_BOOL,
  EVT_STR,
  EVT_ARR,
  EVT_OBJ,
  EVT_FN
} ErgoTag;

typedef struct ErgoVal ErgoVal;

typedef struct ErgoStr {
  int ref;
  size_t len;
  char* data;
} ErgoStr;

typedef struct ErgoArr {
  int ref;
  size_t len;
  size_t cap;
  ErgoVal* items;
} ErgoArr;

typedef struct ErgoObj {
  int ref;
  void (*drop)(struct ErgoObj*);
} ErgoObj;

typedef struct ErgoFn {
  int ref;
  int arity;
  ErgoVal (*fn)(void* env, int argc, ErgoVal* argv);
  void* env;
  int env_size;
} ErgoFn;

struct ErgoVal {
  ErgoTag tag;
  union {
    int64_t i;
    double f;
    bool b;
    void* p;
  } as;
};

#define YV_NULLV ((ErgoVal){.tag=EVT_NULL})
#define YV_INT(x) ((ErgoVal){.tag=EVT_INT, .as.i=(int64_t)(x)})
#define YV_FLOAT(x) ((ErgoVal){.tag=EVT_FLOAT, .as.f=(double)(x)})
#define YV_BOOL(x) ((ErgoVal){.tag=EVT_BOOL, .as.b=(x)?true:false})
#define YV_STR(x) ((ErgoVal){.tag=EVT_STR, .as.p=(x)})
#define YV_ARR(x) ((ErgoVal){.tag=EVT_ARR, .as.p=(x)})
#define YV_OBJ(x) ((ErgoVal){.tag=EVT_OBJ, .as.p=(x)})
#define YV_FN(x) ((ErgoVal){.tag=EVT_FN, .as.p=(x)})

static void ergo_trap(const char* msg) {
  fprintf(stderr, "runtime error: %s\n", msg ? msg : "unknown error");
  fprintf(stderr, "  (run with debugger for stack trace)\n");
  abort();
}

static void ergo_retain_val(ErgoVal v);
static void ergo_release_val(ErgoVal v);

// Static constant strings (ref=INT32_MAX means never freed)
static ErgoStr ergo_static_empty    = { INT32_MAX, 0, "" };
static ErgoStr ergo_static_null     = { INT32_MAX, 4, "null" };
static ErgoStr ergo_static_true     = { INT32_MAX, 4, "true" };
static ErgoStr ergo_static_false    = { INT32_MAX, 5, "false" };
static ErgoStr ergo_static_array    = { INT32_MAX, 7, "[array]" };
static ErgoStr ergo_static_object   = { INT32_MAX, 8, "[object]" };
static ErgoStr ergo_static_function = { INT32_MAX, 10, "[function]" };
static ErgoStr ergo_static_unknown  = { INT32_MAX, 3, "<?>" };

static ErgoStr* stdr_str_lit(const char* s) {
  size_t n = strlen(s);
  ErgoStr* st = (ErgoStr*)malloc(sizeof(ErgoStr) + n + 1);
  st->ref = 1;
  st->len = n;
  st->data = (char*)(st + 1);
  memcpy(st->data, s, n + 1);
  return st;
}

static ErgoStr* stdr_str_from_parts(int n, ErgoVal* parts);
static ErgoStr* stdr_to_string(ErgoVal v);
static ErgoStr* stdr_str_from_slice(const char* s, size_t len);
static ErgoArr* stdr_arr_new(int n);
static void ergo_arr_add(ErgoArr* a, ErgoVal v);
static ErgoVal ergo_arr_get(ErgoArr* a, int64_t idx);
static void ergo_arr_set(ErgoArr* a, int64_t idx, ErgoVal v);
static ErgoVal ergo_arr_remove(ErgoArr* a, int64_t idx);

static ErgoVal stdr_str_at(ErgoVal v, int64_t idx) {
  if (v.tag != EVT_STR) ergo_trap("str_at expects string");
  ErgoStr* s = (ErgoStr*)v.as.p;
  if (idx < 0 || (size_t)idx >= s->len) return YV_STR(&ergo_static_empty);
  return YV_STR(stdr_str_from_slice(s->data + idx, 1));
}

static int stdr_len(ErgoVal v) {
  if (v.tag == EVT_STR) return (int)((ErgoStr*)v.as.p)->len;
  if (v.tag == EVT_ARR) return (int)((ErgoArr*)v.as.p)->len;
  return 0;
}

static bool stdr_is_null(ErgoVal v) { return v.tag == EVT_NULL; }

static void stdr_write(ErgoVal v) {
  ErgoStr* s = stdr_to_string(v);
  fwrite(s->data, 1, s->len, stdout);
  if (ergo_stdout_isatty) fflush(stdout);
  ergo_release_val(YV_STR(s));
}

static void writef(ErgoVal fmt, int argc, ErgoVal* argv) {
  if (fmt.tag != EVT_STR) ergo_trap("writef expects string");
  ErgoStr* s = (ErgoStr*)fmt.as.p;
  size_t i = 0;
  size_t seg = 0;
  int argi = 0;
  while (i < s->len) {
    if (i + 1 < s->len && s->data[i] == '{' && s->data[i + 1] == '}') {
      if (i > seg) fwrite(s->data + seg, 1, i - seg, stdout);
      if (argi < argc) {
        ErgoStr* ps = stdr_to_string(argv[argi++]);
        fwrite(ps->data, 1, ps->len, stdout);
        ergo_release_val(YV_STR(ps));
      }
      i += 2;
      seg = i;
      continue;
    }
    i++;
  }
  if (i > seg) fwrite(s->data + seg, 1, i - seg, stdout);
  if (ergo_stdout_isatty) fflush(stdout);
}

static void stdr_writef_args(ErgoVal fmt, ErgoVal args) {
  if (args.tag != EVT_ARR) ergo_trap("writef expects args tuple");
  ErgoArr* a = (ErgoArr*)args.as.p;
  writef(fmt, (int)a->len, a->items);
}

static ErgoStr* stdr_read_line(void) {
  size_t cap = 128;
  size_t len = 0;
  char* buf = (char*)malloc(cap);
  if (!buf) ergo_trap("out of memory");
  int c;
  while ((c = fgetc(stdin)) != EOF) {
    if (c == '\n') break;
    if (len + 1 >= cap) {
      cap *= 2;
      buf = (char*)realloc(buf, cap);
      if (!buf) ergo_trap("out of memory");
    }
    buf[len++] = (char)c;
  }
  if (len > 0 && buf[len - 1] == '\r') len--;
  ErgoStr* s = (ErgoStr*)malloc(sizeof(ErgoStr) + len + 1);
  if (!s) ergo_trap("out of memory");
  s->ref = 1;
  s->len = len;
  s->data = (char*)(s + 1);
  memcpy(s->data, buf, len);
  s->data[len] = 0;
  free(buf);
  return s;
}

static ErgoVal stdr_read_text_file(ErgoVal pathv) {
  if (pathv.tag != EVT_STR) ergo_trap("read_text_file expects string path");
  ErgoStr* path = (ErgoStr*)pathv.as.p;
  FILE* f = fopen(path->data, "rb");
  if (!f) return YV_NULLV;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return YV_NULLV;
  }
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return YV_NULLV;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return YV_NULLV;
  }
  size_t len = (size_t)sz;
  ErgoStr* out = (ErgoStr*)malloc(sizeof(ErgoStr) + len + 1);
  if (!out) {
    fclose(f);
    ergo_trap("out of memory");
  }
  out->data = (char*)(out + 1);
  size_t n = 0;
  if (len > 0) n = fread(out->data, 1, len, f);
  fclose(f);
  if (n != len) {
    free(out);
    return YV_NULLV;
  }
  out->ref = 1;
  out->len = len;
  out->data[len] = 0;
  return YV_STR(out);
}

static ErgoVal stdr_write_text_file(ErgoVal pathv, ErgoVal textv) {
  if (pathv.tag != EVT_STR) ergo_trap("write_text_file expects string path");
  if (textv.tag != EVT_STR) ergo_trap("write_text_file expects string text");
  ErgoStr* path = (ErgoStr*)pathv.as.p;
  ErgoStr* text = (ErgoStr*)textv.as.p;
  FILE* f = fopen(path->data, "wb");
  if (!f) return YV_BOOL(false);
  size_t n = 0;
  if (text->len > 0) n = fwrite(text->data, 1, text->len, f);
  bool ok = (n == text->len) && (fclose(f) == 0);
  return YV_BOOL(ok);
}

static ErgoVal stdr_capture_shell_first_line(const char* cmd) {
  if (!cmd || !cmd[0]) return YV_NULLV;
#if defined(_WIN32)
  FILE* p = _popen(cmd, "r");
#else
  FILE* p = popen(cmd, "r");
#endif
  if (!p) return YV_NULLV;
  char buf[4096];
  if (!fgets(buf, sizeof(buf), p)) {
#if defined(_WIN32)
    _pclose(p);
#else
    pclose(p);
#endif
    return YV_NULLV;
  }
#if defined(_WIN32)
  _pclose(p);
#else
  pclose(p);
#endif
  size_t len = strlen(buf);
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) len--;
  if (len == 0) return YV_NULLV;
  return YV_STR(stdr_str_from_slice(buf, len));
}

static ErgoVal stdr_open_file_dialog(ErgoVal promptv, ErgoVal extv) {
  if (promptv.tag != EVT_STR) ergo_trap("open_file_dialog expects prompt string");
  if (extv.tag != EVT_STR) ergo_trap("open_file_dialog expects extension string");
  ErgoStr* prompt = (ErgoStr*)promptv.as.p;
  ErgoStr* ext = (ErgoStr*)extv.as.p;
#if defined(__APPLE__)
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "osascript -e 'set _p to POSIX path of (choose file of type {\"%s\"} with prompt \"%s\")' -e 'return _p' 2>/dev/null",
           ext ? ext->data : "", prompt ? prompt->data : "");
  return stdr_capture_shell_first_line(cmd);
#else
  (void)prompt;
  (void)ext;
  return YV_NULLV;
#endif
}

static ErgoVal stdr_save_file_dialog(ErgoVal promptv, ErgoVal default_namev, ErgoVal extv) {
  if (promptv.tag != EVT_STR) ergo_trap("save_file_dialog expects prompt string");
  if (default_namev.tag != EVT_STR) ergo_trap("save_file_dialog expects default_name string");
  if (extv.tag != EVT_STR) ergo_trap("save_file_dialog expects extension string");
  ErgoStr* prompt = (ErgoStr*)promptv.as.p;
  ErgoStr* def = (ErgoStr*)default_namev.as.p;
  ErgoStr* ext = (ErgoStr*)extv.as.p;
#if defined(__APPLE__)
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "osascript -e 'set _p to POSIX path of (choose file name with prompt \"%s\" default name \"%s\")' -e 'return _p' 2>/dev/null",
           prompt ? prompt->data : "", def ? def->data : "");
  ErgoVal out = stdr_capture_shell_first_line(cmd);
  (void)ext;
  return out;
#else
  (void)prompt;
  (void)def;
  (void)ext;
  return YV_NULLV;
#endif
}

static size_t stdr_find_sub(const char* s, size_t slen, const char* sub, size_t sublen, size_t start) {
  if (sublen == 0) return start;
  if (start > slen) return (size_t)-1;
  for (size_t i = start; i + sublen <= slen; i++) {
    if (memcmp(s + i, sub, sublen) == 0) return i;
  }
  return (size_t)-1;
}

static void stdr_trim_span(const char* s, size_t len, size_t* out_start, size_t* out_len) {
  size_t a = 0;
  while (a < len && (s[a] == ' ' || s[a] == '\t')) a++;
  size_t b = len;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) b--;
  *out_start = a;
  *out_len = b - a;
}

static ErgoStr* stdr_str_from_slice(const char* s, size_t len) {
  ErgoStr* st = (ErgoStr*)malloc(sizeof(ErgoStr) + len + 1);
  if (!st) ergo_trap("out of memory");
  st->ref = 1;
  st->len = len;
  st->data = (char*)(st + 1);
  if (len > 0) memcpy(st->data, s, len);
  st->data[len] = 0;
  return st;
}

static int64_t stdr_parse_int_slice(const char* s, size_t len) {
  if (len == 0) return 0;
  char stack[64];
  char* tmp = (len < sizeof(stack)) ? stack : (char*)malloc(len + 1);
  if (!tmp) ergo_trap("out of memory");
  memcpy(tmp, s, len);
  tmp[len] = 0;
  char* end = NULL;
  long long v = strtoll(tmp, &end, 10);
  if (tmp != stack) free(tmp);
  if (end == tmp) return 0;
  return (int64_t)v;
}

static double stdr_parse_float_slice(const char* s, size_t len) {
  if (len == 0) return 0.0;
  char stack[64];
  char* tmp = (len < sizeof(stack)) ? stack : (char*)malloc(len + 1);
  if (!tmp) ergo_trap("out of memory");
  memcpy(tmp, s, len);
  tmp[len] = 0;
  char* end = NULL;
  double v = strtod(tmp, &end);
  if (tmp != stack) free(tmp);
  if (end == tmp) return 0.0;
  return v;
}

static bool stdr_parse_bool_slice(const char* s, size_t len) {
  if (len == 1) {
    if (s[0] == '1') return true;
    if (s[0] == '0') return false;
  }
  if (len == 4) {
    return ((s[0] == 't' || s[0] == 'T') &&
            (s[1] == 'r' || s[1] == 'R') &&
            (s[2] == 'u' || s[2] == 'U') &&
            (s[3] == 'e' || s[3] == 'E'));
  }
  return false;
}

static ErgoVal stdr_readf_parse(ErgoVal fmt, ErgoVal line, ErgoVal args) {
  if (fmt.tag != EVT_STR) ergo_trap("readf expects string format");
  if (line.tag != EVT_STR) ergo_trap("readf expects string input");
  if (args.tag != EVT_ARR) ergo_trap("readf expects args tuple");

  ErgoStr* fs = (ErgoStr*)fmt.as.p;
  ErgoStr* ls = (ErgoStr*)line.as.p;
  ErgoArr* a = (ErgoArr*)args.as.p;

  const char* f = fs->data;
  size_t flen = fs->len;
  const char* s = ls->data;
  size_t slen = ls->len;

  int segs = 1;
  for (size_t i = 0; i + 1 < flen; i++) {
    if (f[i] == '{' && f[i + 1] == '}') {
      segs++;
      i++;
    }
  }

  const char* stack_ptrs[16];
  size_t stack_lens[16];
  const char** seg_ptrs = (segs <= 16) ? stack_ptrs : (const char**)malloc(sizeof(char*) * segs);
  size_t* seg_lens = (segs <= 16) ? stack_lens : (size_t*)malloc(sizeof(size_t) * segs);
  if (!seg_ptrs || !seg_lens) ergo_trap("out of memory");

  size_t seg_start = 0;
  int seg_idx = 0;
  for (size_t i = 0; i + 1 < flen; i++) {
    if (f[i] == '{' && f[i + 1] == '}') {
      seg_ptrs[seg_idx] = f + seg_start;
      seg_lens[seg_idx] = i - seg_start;
      seg_idx++;
      i++;
      seg_start = i + 1;
    }
  }
  seg_ptrs[seg_idx] = f + seg_start;
  seg_lens[seg_idx] = flen - seg_start;

  int placeholders = segs - 1;

  size_t spos = 0;
  if (seg_lens[0] > 0) {
    size_t found = stdr_find_sub(s, slen, seg_ptrs[0], seg_lens[0], 0);
    if (found != (size_t)-1) spos = found + seg_lens[0];
  }

  ErgoArr* out = stdr_arr_new((int)a->len);

  for (size_t i = 0; i < a->len; i++) {
    size_t cap_start = spos;
    size_t cap_len = 0;
    if ((int)i < placeholders) {
      size_t found = stdr_find_sub(s, slen, seg_ptrs[i + 1], seg_lens[i + 1], spos);
      if (found == (size_t)-1) {
        cap_len = slen - spos;
        spos = slen;
      } else {
        cap_len = found - spos;
        spos = found + seg_lens[i + 1];
      }
    }

    size_t trim_start = 0;
    size_t trim_len = cap_len;
    stdr_trim_span(s + cap_start, cap_len, &trim_start, &trim_len);
    const char* cap = (cap_len > 0) ? (s + cap_start + trim_start) : "";

    ErgoVal hint = a->items[i];
    ErgoVal v;
    if (hint.tag == EVT_INT) {
      v = YV_INT(stdr_parse_int_slice(cap, trim_len));
    } else if (hint.tag == EVT_FLOAT) {
      v = YV_FLOAT(stdr_parse_float_slice(cap, trim_len));
    } else if (hint.tag == EVT_BOOL) {
      v = YV_BOOL(stdr_parse_bool_slice(cap, trim_len));
    } else if (hint.tag == EVT_STR) {
      v = YV_STR(stdr_str_from_slice(cap, trim_len));
    } else {
      v = YV_STR(stdr_str_from_slice(cap, trim_len));
    }
    ergo_arr_add(out, v);
  }

  if (seg_ptrs != stack_ptrs) free(seg_ptrs);
  if (seg_lens != stack_lens) free(seg_lens);

  return YV_ARR(out);
}

static ErgoStr* stdr_to_string(ErgoVal v) {
  char buf[64];
  if (v.tag == EVT_NULL) return &ergo_static_null;
  if (v.tag == EVT_BOOL) return v.as.b ? &ergo_static_true : &ergo_static_false;
  if (v.tag == EVT_INT) {
    snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i);
    return stdr_str_lit(buf);
  }
  if (v.tag == EVT_FLOAT) {
    snprintf(buf, sizeof(buf), "%.6f", v.as.f);
    return stdr_str_lit(buf);
  }
  if (v.tag == EVT_STR) {
    ergo_retain_val(v);
    return (ErgoStr*)v.as.p;
  }
  if (v.tag == EVT_ARR) return &ergo_static_array;
  if (v.tag == EVT_OBJ) return &ergo_static_object;
  if (v.tag == EVT_FN) return &ergo_static_function;
  return &ergo_static_unknown;
}

static ErgoStr* stdr_str_from_parts(int n, ErgoVal* parts) {
  size_t total = 0;
  ErgoStr* stack_strs[16];
  ErgoStr** strs = (n <= 16) ? stack_strs : (ErgoStr**)malloc(sizeof(ErgoStr*) * (size_t)n);
  for (int i = 0; i < n; i++) {
    strs[i] = stdr_to_string(parts[i]);
    total += strs[i]->len;
  }
  ErgoStr* out = (ErgoStr*)malloc(sizeof(ErgoStr) + total + 1);
  out->ref = 1;
  out->len = total;
  out->data = (char*)(out + 1);
  size_t off = 0;
  for (int i = 0; i < n; i++) {
    memcpy(out->data + off, strs[i]->data, strs[i]->len);
    off += strs[i]->len;
    ergo_release_val(YV_STR(strs[i]));
  }
  out->data[total] = 0;
  if (strs != stack_strs) free(strs);
  return out;
}

static void ergo_retain_val(ErgoVal v) {
  if (v.tag == EVT_STR) { int* r = &((ErgoStr*)v.as.p)->ref; if (*r != INT32_MAX) (*r)++; }
  else if (v.tag == EVT_ARR) ((ErgoArr*)v.as.p)->ref++;
  else if (v.tag == EVT_OBJ) ((ErgoObj*)v.as.p)->ref++;
  else if (v.tag == EVT_FN) ((ErgoFn*)v.as.p)->ref++;
}

static void ergo_release_val(ErgoVal v) {
  if (v.tag == EVT_STR) {
    ErgoStr* s = (ErgoStr*)v.as.p;
    if (s->ref == INT32_MAX) return;
    if (--s->ref == 0) {
      if (s->data != (char*)(s + 1)) free(s->data);
      free(s);
    }
  } else if (v.tag == EVT_ARR) {
    ErgoArr* a = (ErgoArr*)v.as.p;
    if (--a->ref == 0) {
      for (size_t i = 0; i < a->len; i++) ergo_release_val(a->items[i]);
      free(a->items);
      free(a);
    }
  } else if (v.tag == EVT_OBJ) {
    ErgoObj* o = (ErgoObj*)v.as.p;
    if (--o->ref == 0) {
      if (o->drop) o->drop(o);
      free(o);
    }
  } else if (v.tag == EVT_FN) {
    ErgoFn* f = (ErgoFn*)v.as.p;
    if (--f->ref == 0) {
      if (f->env && f->env_size > 0) {
        ErgoVal* caps = (ErgoVal*)f->env;
        for (int i = 0; i < f->env_size; i++) ergo_release_val(caps[i]);
        free(f->env);
      }
      free(f);
    }
  }
}

static ErgoVal ergo_move(ErgoVal* slot) {
  ErgoVal v = *slot;
  *slot = YV_NULLV;
  return v;
}

static void ergo_move_into(ErgoVal* slot, ErgoVal v) {
  ergo_release_val(*slot);
  *slot = v;
}

static int64_t ergo_as_int(ErgoVal v) {
  if (v.tag == EVT_INT) return v.as.i;
  if (v.tag == EVT_BOOL) return v.as.b ? 1 : 0;
  if (v.tag == EVT_FLOAT) return (int64_t)v.as.f;
  ergo_trap("type mismatch: expected int");
  return 0;
}

static double ergo_as_float(ErgoVal v) {
  if (v.tag == EVT_FLOAT) return v.as.f;
  if (v.tag == EVT_INT) return (double)v.as.i;
  ergo_trap("type mismatch: expected float");
  return 0.0;
}

static bool ergo_as_bool(ErgoVal v) {
  if (v.tag == EVT_BOOL) return v.as.b;
  if (v.tag == EVT_NULL) return false;
  if (v.tag == EVT_INT) return v.as.i != 0;
  if (v.tag == EVT_FLOAT) return v.as.f != 0.0;
  if (v.tag == EVT_STR) return ((ErgoStr*)v.as.p)->len != 0;
  if (v.tag == EVT_ARR) return ((ErgoArr*)v.as.p)->len != 0;
  return true;
}

static ErgoVal ergo_add(ErgoVal a, ErgoVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(ergo_as_float(a) + ergo_as_float(b));
  return YV_INT(ergo_as_int(a) + ergo_as_int(b));
}

static ErgoVal ergo_sub(ErgoVal a, ErgoVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(ergo_as_float(a) - ergo_as_float(b));
  return YV_INT(ergo_as_int(a) - ergo_as_int(b));
}

static ErgoVal ergo_mul(ErgoVal a, ErgoVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(ergo_as_float(a) * ergo_as_float(b));
  return YV_INT(ergo_as_int(a) * ergo_as_int(b));
}

static ErgoVal ergo_div(ErgoVal a, ErgoVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(ergo_as_float(a) / ergo_as_float(b));
  return YV_INT(ergo_as_int(a) / ergo_as_int(b));
}

static ErgoVal ergo_mod(ErgoVal a, ErgoVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) ergo_trap("% expects integer");
  return YV_INT(ergo_as_int(a) % ergo_as_int(b));
}

static ErgoVal ergo_neg(ErgoVal a) {
  if (a.tag == EVT_FLOAT) return YV_FLOAT(-a.as.f);
  return YV_INT(-ergo_as_int(a));
}

static ErgoVal ergo_eq(ErgoVal a, ErgoVal b) {
  if (a.tag != b.tag) return YV_BOOL(false);
  switch (a.tag) {
    case EVT_NULL: return YV_BOOL(true);
    case EVT_BOOL: return YV_BOOL(a.as.b == b.as.b);
    case EVT_INT: return YV_BOOL(a.as.i == b.as.i);
    case EVT_FLOAT: return YV_BOOL(a.as.f == b.as.f);
    case EVT_STR: {
      ErgoStr* sa = (ErgoStr*)a.as.p;
      ErgoStr* sb = (ErgoStr*)b.as.p;
      if (sa->len != sb->len) return YV_BOOL(false);
      return YV_BOOL(memcmp(sa->data, sb->data, sa->len) == 0);
    }
    default: return YV_BOOL(a.as.p == b.as.p);
  }
}

static ErgoVal ergo_ne(ErgoVal a, ErgoVal b) {
  ErgoVal v = ergo_eq(a, b);
  return YV_BOOL(!v.as.b);
}

static ErgoVal ergo_lt(ErgoVal a, ErgoVal b) { return YV_BOOL(ergo_as_float(a) < ergo_as_float(b)); }
static ErgoVal ergo_le(ErgoVal a, ErgoVal b) { return YV_BOOL(ergo_as_float(a) <= ergo_as_float(b)); }
static ErgoVal ergo_gt(ErgoVal a, ErgoVal b) { return YV_BOOL(ergo_as_float(a) > ergo_as_float(b)); }
static ErgoVal ergo_ge(ErgoVal a, ErgoVal b) { return YV_BOOL(ergo_as_float(a) >= ergo_as_float(b)); }

static ErgoArr* stdr_arr_new(int n) {
  ErgoArr* a = (ErgoArr*)malloc(sizeof(ErgoArr));
  a->ref = 1;
  a->len = 0;
  a->cap = (n > 0) ? (size_t)n : 4;
  a->items = (ErgoVal*)malloc(sizeof(ErgoVal) * a->cap);
  return a;
}

static void ergo_arr_add(ErgoArr* a, ErgoVal v) {
  if (a->len >= a->cap) {
    a->cap *= 2;
    a->items = (ErgoVal*)realloc(a->items, sizeof(ErgoVal) * a->cap);
  }
  a->items[a->len++] = v;
}

static ErgoVal ergo_arr_get(ErgoArr* a, int64_t idx) {
  if (idx < 0 || (size_t)idx >= a->len) return YV_NULLV;
  ErgoVal v = a->items[idx];
  ergo_retain_val(v);
  return v;
}

static void ergo_arr_set(ErgoArr* a, int64_t idx, ErgoVal v) {
  if (idx < 0 || (size_t)idx >= a->len) return;
  ergo_release_val(a->items[idx]);
  a->items[idx] = v;
}

static ErgoVal ergo_arr_remove(ErgoArr* a, int64_t idx) {
  if (idx < 0 || (size_t)idx >= a->len) return YV_NULLV;
  ErgoVal v = a->items[idx];
  for (size_t i = (size_t)idx; i + 1 < a->len; i++) {
    a->items[i] = a->items[i + 1];
  }
  a->len--;
  return v;
}

static ErgoObj* ergo_obj_new(size_t size, void (*drop)(ErgoObj*)) {
  ErgoObj* o = (ErgoObj*)malloc(size);
  o->ref = 1;
  o->drop = drop;
  return o;
}

static ErgoFn* ergo_fn_new(ErgoVal (*fn)(void* env, int argc, ErgoVal* argv), int arity) {
  ErgoFn* f = (ErgoFn*)malloc(sizeof(ErgoFn));
  f->ref = 1;
  f->arity = arity;
  f->fn = fn;
  f->env = NULL;
  f->env_size = 0;
  return f;
}

static ErgoFn* ergo_fn_new_with_env(ErgoVal (*fn)(void* env, int argc, ErgoVal* argv), int arity, void* env, int env_size) {
  ErgoFn* f = (ErgoFn*)malloc(sizeof(ErgoFn));
  f->ref = 1;
  f->arity = arity;
  f->fn = fn;
  f->env = env;
  f->env_size = env_size;
  return f;
}

static ErgoVal ergo_call(ErgoVal fval, int argc, ErgoVal* argv) {
  if (fval.tag != EVT_FN) ergo_trap("call expects function");
  ErgoFn* f = (ErgoFn*)fval.as.p;
  if (f->arity >= 0 && f->arity != argc) ergo_trap("arity mismatch");
  return f->fn(f->env, argc, argv);
}

// ---- Cogito GUI (shared library bindings) ----
// Injected by codegen when the program imports `cogito`.
// ---- Cogito bindings (shared library) ----
#include <cogito.h>

#undef cogito_about_window_new
#undef cogito_about_window_set_description
#undef cogito_about_window_set_icon
#undef cogito_about_window_set_issue_url
#undef cogito_about_window_set_website
#undef cogito_active_indicator_new
#undef cogito_app_free
#undef cogito_app_get_icon
#undef cogito_app_new
#undef cogito_app_run
#undef cogito_app_copy_to_clipboard
#undef cogito_app_set_accent_color
#undef cogito_app_set_dark_mode
#undef cogito_app_set_accent_from_image
#undef cogito_app_set_app_name
#undef cogito_app_set_appid
#undef cogito_app_set_contrast
#undef cogito_app_set_ensor_variant
#undef cogito_app_set_icon
#undef cogito_appbar_add_button
#undef cogito_appbar_new
#undef cogito_appbar_set_controls
#undef cogito_appbar_set_subtitle
#undef cogito_appbar_set_title
#undef cogito_toolbar_new
#undef cogito_button_add_menu
#undef cogito_button_add_menu_section
#undef cogito_button_get_size
#undef cogito_button_new
#undef cogito_button_set_size
#undef cogito_button_set_text
#undef cogito_carousel_item_set_halign
#undef cogito_carousel_item_set_text
#undef cogito_carousel_item_set_valign
#undef cogito_checkbox_get_checked
#undef cogito_checkbox_new
#undef cogito_checkbox_on_change
#undef cogito_checkbox_set_checked
#undef cogito_colorpicker_new
#undef cogito_colorpicker_on_change
#undef cogito_colorpicker_set_hex
#undef cogito_colorpicker_get_hex
#undef cogito_content_list_new
#undef cogito_datepicker_new
#undef cogito_datepicker_on_change
#undef cogito_dialog_new
#undef cogito_dialog_slot_clear
#undef cogito_dialog_slot_new
#undef cogito_dialog_slot_show
#undef cogito_dropdown_get_selected
#undef cogito_dropdown_new
#undef cogito_dropdown_on_change
#undef cogito_dropdown_set_items
#undef cogito_dropdown_set_selected
#undef cogito_empty_page_new
#undef cogito_empty_page_set_action
#undef cogito_empty_page_set_description
#undef cogito_empty_page_set_icon
#undef cogito_find_children
#undef cogito_find_parent
#undef cogito_fixed_set_pos
#undef cogito_grid_new_with_cols
#undef cogito_grid_on_activate
#undef cogito_grid_on_select
#undef cogito_grid_set_align
#undef cogito_grid_set_gap
#undef cogito_grid_set_span
#undef cogito_iconbtn_add_menu
#undef cogito_iconbtn_add_menu_section
#undef cogito_iconbtn_new
#undef cogito_image_new
#undef cogito_image_set_icon
#undef cogito_drawing_area_new
#undef cogito_drawing_area_on_press
#undef cogito_drawing_area_on_drag
#undef cogito_drawing_area_on_release
#undef cogito_drawing_area_get_x
#undef cogito_drawing_area_get_y
#undef cogito_drawing_area_get_pressed
#undef cogito_drawing_area_clear
#undef cogito_drawing_area_on_draw
#undef cogito_canvas_set_color
#undef cogito_canvas_set_line_width
#undef cogito_canvas_line
#undef cogito_canvas_rect
#undef cogito_canvas_fill_rect
#undef cogito_shape_new
#undef cogito_shape_set_preset
#undef cogito_shape_get_preset
#undef cogito_shape_set_size
#undef cogito_shape_get_size
#undef cogito_shape_set_color
#undef cogito_shape_set_color_style
#undef cogito_shape_get_color_style
#undef cogito_shape_set_vertex
#undef cogito_shape_get_vertex_x
#undef cogito_shape_get_vertex_y
#undef cogito_timer_set_timeout
#undef cogito_timer_set_interval
#undef cogito_timer_set_timeout_ex
#undef cogito_timer_set_interval_ex
#undef cogito_timer_set_timeout_for
#undef cogito_timer_set_interval_for
#undef cogito_timer_set_timeout_for_ex
#undef cogito_timer_set_interval_for_ex
#undef cogito_timer_clear
#undef cogito_timer_clear_for
#undef cogito_timer_clear_all
#undef cogito_label_new
#undef cogito_label_set_align
#undef cogito_label_set_ellipsis
#undef cogito_label_set_text
#undef cogito_label_set_wrap
#undef cogito_list_on_activate
#undef cogito_list_on_select
#undef cogito_load_sum_file
#undef cogito_menu_button_new
#undef cogito_menu_set_icon
#undef cogito_menu_set_shortcut
#undef cogito_menu_set_submenu
#undef cogito_menu_set_toggled
#undef cogito_node_add
#undef cogito_node_build
#undef cogito_node_free
#undef cogito_node_get_editable
#undef cogito_node_get_text
#undef cogito_node_new
#undef cogito_node_on_activate
#undef cogito_node_on_change
#undef cogito_node_on_click
#undef cogito_node_on_select
#undef cogito_node_remove
#undef cogito_node_set_a11y_label
#undef cogito_node_set_a11y_role
#undef cogito_node_set_align
#undef cogito_node_set_class
#undef cogito_node_set_disabled
#undef cogito_node_set_editable
#undef cogito_node_set_halign
#undef cogito_node_set_id
#undef cogito_node_set_margins
#undef cogito_node_set_padding
#undef cogito_node_set_text
#undef cogito_node_set_tooltip
#undef cogito_node_set_valign
#undef cogito_node_window
#undef cogito_open_url
#undef cogito_pointer_capture
#undef cogito_pointer_release
#undef cogito_progress_get_circular
#undef cogito_progress_get_indeterminate
#undef cogito_progress_get_thickness
#undef cogito_progress_get_value
#undef cogito_progress_get_wavy
#undef cogito_progress_new
#undef cogito_progress_set_circular
#undef cogito_progress_set_indeterminate
#undef cogito_progress_set_thickness
#undef cogito_progress_set_value
#undef cogito_progress_set_wavy
#undef cogito_rebuild_active_window
#undef cogito_scroller_set_axes
#undef cogito_searchfield_get_text
#undef cogito_searchfield_new
#undef cogito_searchfield_on_change
#undef cogito_searchfield_set_text
#undef cogito_buttongroup_new
#undef cogito_buttongroup_on_select
#undef cogito_buttongroup_set_size
#undef cogito_buttongroup_get_size
#undef cogito_buttongroup_set_shape
#undef cogito_buttongroup_get_shape
#undef cogito_buttongroup_set_connected
#undef cogito_buttongroup_get_connected
#undef cogito_settings_list_new
#undef cogito_settings_page_new
#undef cogito_settings_row_new
#undef cogito_settings_window_new
#undef cogito_slider_get_centered
#undef cogito_slider_get_range_end
#undef cogito_slider_get_range_start
#undef cogito_slider_get_size
#undef cogito_slider_get_value
#undef cogito_slider_new
#undef cogito_slider_on_change
#undef cogito_slider_range_new
#undef cogito_slider_set_centered
#undef cogito_slider_set_icon
#undef cogito_slider_set_range
#undef cogito_slider_set_range_end
#undef cogito_slider_set_range_start
#undef cogito_slider_set_size
#undef cogito_slider_set_value
#undef cogito_split_button_add_menu
#undef cogito_split_button_add_menu_section
#undef cogito_split_button_new
#undef cogito_split_button_set_size
#undef cogito_split_button_set_variant
#undef cogito_stepper_get_value
#undef cogito_stepper_new
#undef cogito_stepper_on_change
#undef cogito_stepper_set_value
#undef cogito_switch_get_checked
#undef cogito_switch_new
#undef cogito_switch_on_change
#undef cogito_switch_set_checked
#undef cogito_switchbar_get_checked
#undef cogito_switchbar_new
#undef cogito_switchbar_on_change
#undef cogito_switchbar_set_checked
#undef cogito_tabs_bind
#undef cogito_tabs_get_selected
#undef cogito_tabs_new
#undef cogito_tabs_on_change
#undef cogito_tabs_set_ids
#undef cogito_tabs_set_items
#undef cogito_tabs_set_selected
#undef cogito_textfield_get_hint
#undef cogito_textfield_get_text
#undef cogito_textfield_new
#undef cogito_textfield_on_change
#undef cogito_textfield_set_hint
#undef cogito_textfield_set_text
#undef cogito_textview_get_text
#undef cogito_textview_new
#undef cogito_textview_on_change
#undef cogito_textview_set_text
#undef cogito_tip_view_new
#undef cogito_tip_view_set_title
#undef cogito_toast_new
#undef cogito_toast_on_click
#undef cogito_toast_set_action
#undef cogito_toasts_new
#undef cogito_treeview_new
#undef cogito_view_chooser_bind
#undef cogito_view_chooser_new
#undef cogito_view_chooser_set_items
#undef cogito_view_dual_new
#undef cogito_view_dual_set_ratio
#undef cogito_view_switcher_new
#undef cogito_view_switcher_set_active
#undef cogito_welcome_screen_new
#undef cogito_welcome_screen_set_action
#undef cogito_welcome_screen_set_description
#undef cogito_welcome_screen_set_icon
#undef cogito_window_clear_dialog
#undef cogito_window_free
#undef cogito_window_new
#undef cogito_window_set_a11y_label
#undef cogito_window_set_autosize
#undef cogito_window_set_builder
#undef cogito_window_set_dialog
#undef cogito_window_set_side_sheet
#undef cogito_side_sheet_set_mode
#undef cogito_window_clear_side_sheet
#undef cogito_window_set_resizable
#undef cogito_fab_menu_set_size
#undef cogito_fab_menu_set_color
#undef cogito_card_set_subtitle
#undef cogito_card_on_click
#undef cogito_card_set_variant
#undef cogito_card_set_header_image
#undef cogito_card_add_action
#undef cogito_card_add_overflow

static bool cogito_debug_enabled(void) {
  const char* env = getenv("COGITO_DEBUG");
  return env && env[0] && env[0] != '0';
}

typedef enum {
  COGITO_HANDLE_APP = 1,
  COGITO_HANDLE_WINDOW,
  COGITO_HANDLE_NODE,
  COGITO_HANDLE_STATE
} CogitoHandleKind;

typedef struct CogitoHandle {
  ErgoObj base;
  void* ptr;
  int kind;
  ErgoVal on_click;
  ErgoVal on_change;
  ErgoVal on_select;
  ErgoVal on_activate;
  ErgoVal on_action;
  ErgoVal on_draw;
  ErgoVal builder;
} CogitoHandle;

typedef struct CogitoHandleEntry {
  cogito_node* node;
  CogitoHandle* handle;
  struct CogitoHandleEntry* next;
} CogitoHandleEntry;

typedef struct CogitoMenuHandler {
  ErgoVal fn;
} CogitoMenuHandler;

static void __cogito_button_on_click(ErgoVal btnv, ErgoVal handler);

// Forward declaration for internal ErgoVal-based function
extern void cogito_view_switcher_add_lazy_ergo(cogito_node* view_switcher, ErgoVal id, ErgoVal builder);

typedef struct CogitoState {
  ErgoObj base;
  ErgoVal value;
} CogitoState;

static CogitoHandleEntry* cogito_handle_entries = NULL;

static CogitoHandle* cogito_handle_lookup(cogito_node* node) {
  for (CogitoHandleEntry* e = cogito_handle_entries; e; e = e->next) {
    if (e->node == node) return e->handle;
  }
  return NULL;
}

static void cogito_handle_register(cogito_node* node, CogitoHandle* handle) {
  CogitoHandleEntry* e = (CogitoHandleEntry*)malloc(sizeof(*e));
  e->node = node;
  e->handle = handle;
  e->next = cogito_handle_entries;
  cogito_handle_entries = e;
}

static void cogito_handle_unregister(cogito_node* node) {
  CogitoHandleEntry** cur = &cogito_handle_entries;
  while (*cur) {
    CogitoHandleEntry* e = *cur;
    if (e->node == node) {
      *cur = e->next;
      free(e);
      return;
    }
    cur = &e->next;
  }
}

static void cogito_handle_drop(ErgoObj* o) {
  CogitoHandle* h = (CogitoHandle*)o;
  if (!h) return;
  if (h->on_click.tag != EVT_NULL) ergo_release_val(h->on_click);
  if (h->on_change.tag != EVT_NULL) ergo_release_val(h->on_change);
  if (h->on_select.tag != EVT_NULL) ergo_release_val(h->on_select);
  if (h->on_activate.tag != EVT_NULL) ergo_release_val(h->on_activate);
  if (h->on_action.tag != EVT_NULL) ergo_release_val(h->on_action);
  if (h->on_draw.tag != EVT_NULL) ergo_release_val(h->on_draw);
  if (h->builder.tag != EVT_NULL) ergo_release_val(h->builder);
  if (h->kind == COGITO_HANDLE_WINDOW || h->kind == COGITO_HANDLE_NODE) {
    cogito_handle_unregister((cogito_node*)h->ptr);
  }
  h->ptr = NULL;
}

static CogitoHandle* cogito_handle_new(void* ptr, int kind) {
  CogitoHandle* h = (CogitoHandle*)ergo_obj_new(sizeof(CogitoHandle), cogito_handle_drop);
  h->ptr = ptr;
  h->kind = kind;
  h->on_click = YV_NULLV;
  h->on_change = YV_NULLV;
  h->on_select = YV_NULLV;
  h->on_activate = YV_NULLV;
  h->on_action = YV_NULLV;
  h->on_draw = YV_NULLV;
  h->builder = YV_NULLV;
  return h;
}

static ErgoVal cogito_wrap_node(cogito_node* node, int kind) {
  if (!node) return YV_NULLV;
  CogitoHandle* h = cogito_handle_lookup(node);
  if (!h) {
    h = cogito_handle_new(node, kind);
    cogito_handle_register(node, h);
  }
  return YV_OBJ(h);
}

static CogitoHandle* cogito_handle_from_val(ErgoVal v, const char* what) {
  if (v.tag != EVT_OBJ) ergo_trap(what);
  return (CogitoHandle*)v.as.p;
}

static cogito_app* cogito_app_from_val(ErgoVal v) {
  CogitoHandle* h = cogito_handle_from_val(v,
  "cogito.app expects app");
  return (cogito_app*)h->ptr;
}

static cogito_window* cogito_window_from_val(ErgoVal v) {
  CogitoHandle* h = cogito_handle_from_val(v,
  "cogito.window expects window");
  return (cogito_window*)h->ptr;
}

static cogito_node* cogito_node_from_val(ErgoVal v) {
  CogitoHandle* h = cogito_handle_from_val(v,
  "cogito.node expects node");
  return (cogito_node*)h->ptr;
}
// Non-static version for use by view_switcher.inc
cogito_node* cogito_unwrap_handle(ErgoVal v) {
  if (v.tag != EVT_OBJ) return NULL;
  CogitoHandle* h = (CogitoHandle*)v.as.p;
  return (cogito_node*)h->ptr;
}

static const char* cogito_required_cstr(ErgoVal v, ErgoStr** tmp) {
  if (v.tag == EVT_NULL) return "";
  if (v.tag == EVT_STR) return ((ErgoStr*)v.as.p)->data;
  ErgoStr* s = stdr_to_string(v);
  if (tmp) *tmp = s;
  return s ? s->data : "";
}

static const char* cogito_optional_cstr(ErgoVal v, ErgoStr** tmp) {
  if (v.tag == EVT_NULL) return NULL;
  if (v.tag == EVT_STR) return ((ErgoStr*)v.as.p)->data;
  ErgoStr* s = stdr_to_string(v);
  if (tmp) *tmp = s;
  return s ? s->data : NULL;
}

static void cogito_set_handler(CogitoHandle* h, ErgoVal* slot, ErgoVal handler) {
  bool had = slot->tag != EVT_NULL;
  if (had) ergo_release_val(*slot);
  *slot = handler;
  bool has = handler.tag != EVT_NULL;
  if (has) ergo_retain_val(handler);
  if (h) {
    if (!had && has) ergo_retain_val(YV_OBJ(h));
    if (had && !has) ergo_release_val(YV_OBJ(h));
  }
}

static void cogito_invoke_node_handler(ErgoVal handler, cogito_node* node) {
  if (handler.tag != EVT_FN) return;
  ErgoVal arg = cogito_wrap_node(node, COGITO_HANDLE_NODE);
  ergo_retain_val(arg);
  ErgoVal ret = ergo_call(handler,
  1, &arg);
  ergo_release_val(arg);
  ergo_release_val(ret);
}

static void cogito_invoke_index_handler(ErgoVal handler, int idx) {
  if (handler.tag != EVT_FN) return;
  ErgoVal arg = YV_INT(idx);
  ErgoVal ret = ergo_call(handler,
  1, &arg);
  ergo_release_val(ret);
}

static void cogito_invoke_draw_handler(ErgoVal handler, cogito_node* node,
                                       int width, int height) {
  if (handler.tag != EVT_FN) return;
  ErgoVal args[3] = {cogito_wrap_node(node, COGITO_HANDLE_NODE), YV_INT(width),
                     YV_INT(height)};
  ergo_retain_val(args[0]);
  ErgoVal ret = ergo_call(handler, 3, args);
  ergo_release_val(args[0]);
  ergo_release_val(ret);
}

static void cogito_cb_click(cogito_node* node, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_node_handler(h->on_click, node);
}

static void cogito_cb_change(cogito_node* node, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_node_handler(h->on_change, node);
}

static void cogito_cb_action(cogito_node* node, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_node_handler(h->on_action, node);
}

static void cogito_cb_draw(cogito_node* node, int width, int height, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_draw_handler(h->on_draw, node, width, height);
}

static void cogito_cb_select(cogito_node* node, int idx, void* user) {
  (void)node;
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_index_handler(h->on_select, idx);
}

static void cogito_cb_activate(cogito_node* node, int idx, void* user) {
  (void)node;
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_index_handler(h->on_activate, idx);
}

static void cogito_cb_builder(cogito_node* node, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_node_handler(h->builder, node);
}

static CogitoMenuHandler* cogito_menu_handler_new(ErgoVal handler) {
  CogitoMenuHandler* mh = (CogitoMenuHandler*)calloc(1, sizeof(*mh));
  mh->fn = handler;
  if (handler.tag != EVT_NULL) ergo_retain_val(handler);
  return mh;
}

static void cogito_cb_menu(cogito_node* node, void* user) {
  CogitoMenuHandler* mh = (CogitoMenuHandler*)user;
  if (!mh) return;
  cogito_invoke_node_handler(mh->fn, node);
}

static void cogito_state_drop(ErgoObj* o) {
  CogitoState* s = (CogitoState*)o;
  if (s->value.tag != EVT_NULL) {
    ergo_release_val(s->value);
    s->value = YV_NULLV;
  }
}

static ErgoVal cogito_state_new_val(ErgoVal initial) {
  CogitoState* s = (CogitoState*)ergo_obj_new(sizeof(CogitoState), cogito_state_drop);
  s->value = initial;
  if (initial.tag != EVT_NULL) ergo_retain_val(initial);
  return YV_OBJ(s);
}

static ErgoVal cogito_state_get_val(ErgoVal sv) {
  if (sv.tag != EVT_OBJ) ergo_trap("cogito.state_get expects state");
  CogitoState* s = (CogitoState*)sv.as.p;
  ErgoVal v = s->value;
  if (v.tag != EVT_NULL) ergo_retain_val(v);
  return v;
}

static void cogito_state_set_val(ErgoVal sv, ErgoVal nv) {
  if (sv.tag != EVT_OBJ) ergo_trap("cogito.state_set expects state");
  CogitoState* s = (CogitoState*)sv.as.p;
  if (s->value.tag != EVT_NULL) ergo_release_val(s->value);
  s->value = nv;
  if (nv.tag != EVT_NULL) ergo_retain_val(nv);
  cogito_rebuild_active_window();
}

static ErgoVal __cogito_app(void) {
  cogito_app* app = cogito_app_new();
  CogitoHandle* h = cogito_handle_new(app, COGITO_HANDLE_APP);
  return YV_OBJ(h);
}

static void __cogito_app_set_appid(ErgoVal appv, ErgoVal idv) {
  cogito_app* app = cogito_app_from_val(appv);
  ErgoStr* tmp = NULL;
  const char* id = cogito_optional_cstr(idv, &tmp);
  if (id) cogito_app_set_appid(app, id);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_app_set_app_name(ErgoVal appv, ErgoVal namev) {
  cogito_app* app = cogito_app_from_val(appv);
  ErgoStr* tmp = NULL;
  const char* name = cogito_optional_cstr(namev, &tmp);
  if (name) cogito_app_set_app_name(app, name);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_app_set_accent_color(ErgoVal appv, ErgoVal colorv, ErgoVal overridev) {
  cogito_app* app = cogito_app_from_val(appv);
  ErgoStr* tmp = NULL;
  const char* color = cogito_optional_cstr(colorv, &tmp);
  bool ov = overridev.tag == EVT_BOOL ? overridev.as.b : false;
  if (color) cogito_app_set_accent_color(app, color, ov);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_app_set_dark_mode(ErgoVal appv, ErgoVal darkv, ErgoVal follow_systemv) {
  cogito_app* app = cogito_app_from_val(appv);
  bool dark = darkv.tag == EVT_BOOL ? darkv.as.b : ergo_as_bool(darkv);
  bool follow_system = follow_systemv.tag == EVT_BOOL ? follow_systemv.as.b : false;
  if (app) cogito_app_set_dark_mode(app, dark, follow_system);
}

static ErgoVal __cogito_app_set_accent_from_image(ErgoVal appv, ErgoVal pathv, ErgoVal follow_systemv) {
  cogito_app* app = cogito_app_from_val(appv);
  ErgoStr* tmp = NULL;
  const char* path = cogito_optional_cstr(pathv, &tmp);
  bool follow_system = follow_systemv.tag == EVT_BOOL ? follow_systemv.as.b : false;
  const char* hex = (app && path && path[0]) ? cogito_app_set_accent_from_image(app, path, follow_system) : NULL;
  if (tmp) ergo_release_val(YV_STR(tmp));
  if (!hex || !hex[0]) return YV_NULLV;
  return YV_STR(stdr_str_lit(hex));
}

static void __cogito_app_set_ensor_variant(ErgoVal appv, ErgoVal variantv) {
  cogito_app* app = cogito_app_from_val(appv);
  if (app) {
    cogito_app_set_ensor_variant(app, (int)ergo_as_int(variantv));
  }
}

static void __cogito_app_set_contrast(ErgoVal appv, ErgoVal contrastv) {
  cogito_app* app = cogito_app_from_val(appv);
  if (app) {
    cogito_app_set_contrast(app, ergo_as_float(contrastv));
  }
}

static void __cogito_app_set_icon(ErgoVal appv, ErgoVal pathv) {
  cogito_app* app = cogito_app_from_val(appv);
  ErgoStr* tmp = NULL;
  const char* path = cogito_optional_cstr(pathv, &tmp);
  if (path) cogito_app_set_icon(app, path);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static ErgoVal __cogito_app_get_icon(ErgoVal appv) {
  cogito_app* app = cogito_app_from_val(appv);
  const char* path = cogito_app_get_icon(app);
  return YV_STR(stdr_str_lit(path ? path : ""));
}

static ErgoVal __cogito_app_copy_to_clipboard(ErgoVal appv, ErgoVal textv) {
  cogito_app* app = cogito_app_from_val(appv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_optional_cstr(textv, &tmp);
  bool ok = false;
  if (app && text) ok = cogito_app_copy_to_clipboard(app, text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

static ErgoVal __cogito_window(ErgoVal titlev, ErgoVal wv, ErgoVal hv) {
  ErgoStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  int w = (int)ergo_as_int(wv);
  int h = (int)ergo_as_int(hv);
  cogito_window* win = cogito_window_new(title, w, h);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node((cogito_node*)win, COGITO_HANDLE_WINDOW);
}

static void __cogito_window_set_resizable(ErgoVal winv, ErgoVal onv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_window_set_resizable(win, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_window_set_autosize(ErgoVal winv, ErgoVal onv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_window_set_autosize(win, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_window_set_a11y_label(ErgoVal winv, ErgoVal labelv) {
  cogito_window* win = cogito_window_from_val(winv);
  ErgoStr* tmp = NULL;
  const char* label = cogito_optional_cstr(labelv, &tmp);
  if (label) cogito_window_set_a11y_label(win, label);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_window_set_builder(ErgoVal winv, ErgoVal builder) {
  cogito_window* win = cogito_window_from_val(winv);
  CogitoHandle* h = (CogitoHandle*)winv.as.p;
  if (builder.tag != EVT_FN) {
    cogito_set_handler(h, &h->builder, builder);
    cogito_window_set_builder(win, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->builder, builder);
  cogito_window_set_builder(win, cogito_cb_builder, h);
}

static ErgoVal __cogito_label(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_label_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_button(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_button_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_iconbtn(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_iconbtn_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_fab(ErgoVal iconv) {
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_node* n = cogito_fab_new(icon);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

// Forward declarations for FAB Menu
struct CogitoNode* cogito_fab_menu_new(const char* icon);
extern void cogito_fab_menu_add_item(ErgoVal fab_menuv, ErgoVal iconv, ErgoVal action);

static void __cogito_fab_set_size(ErgoVal fabv, ErgoVal sizev) {
  cogito_node* fab = cogito_node_from_val(fabv);
  int size = (int)ergo_as_int(sizev);
  if (size < 0) size = 0;
  if (size > 2) size = 2;
  cogito_fab_set_size(fab, size);
}

static void __cogito_fab_set_color(ErgoVal fabv, ErgoVal colorv) {
  cogito_node* fab = cogito_node_from_val(fabv);
  int color = (int)ergo_as_int(colorv);
  if (color < 0) color = 0;
  if (color > 3) color = 3;
  cogito_fab_set_color(fab, color);
}

static void __cogito_fab_menu_set_color(ErgoVal fabv, ErgoVal colorv) {
  cogito_node* fab = cogito_node_from_val(fabv);
  int color = (int)ergo_as_int(colorv);
  if (color < 0) color = 0;
  if (color > 3) color = 3;
  cogito_fab_menu_set_color(fab, color);
}

static void __cogito_fab_set_extended(ErgoVal fabv, ErgoVal extendedv, ErgoVal labelv) {
  cogito_node* fab = cogito_node_from_val(fabv);
  bool extended = ergo_as_bool(extendedv);
  ErgoStr* tmp = NULL;
  const char* label = cogito_optional_cstr(labelv, &tmp);
  cogito_fab_set_extended(fab, extended, label ? label : "");
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_fab_on_click(ErgoVal fabv, ErgoVal handler) {
  cogito_node* fab = cogito_node_from_val(fabv);
  CogitoHandle* h = (CogitoHandle*)fabv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_fab_on_click(fab, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_fab_on_click(fab, cogito_cb_click, h);
}

static ErgoVal __cogito_fab_menu(ErgoVal iconv) {
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_node* n = cogito_fab_menu_new(icon);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_fab_menu_add_item(ErgoVal fabv, ErgoVal iconv, ErgoVal actionv) {
  cogito_node* node = cogito_node_from_val(fabv);
  cogito_fab_menu_add_item(YV_OBJ(node), iconv, actionv);
}

static ErgoVal __cogito_chip(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_chip_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_chip_set_selected(ErgoVal chipv, ErgoVal sel) {
  cogito_node* n = cogito_node_from_val(chipv);
  cogito_chip_set_selected(n, ergo_as_bool(sel));
}

static ErgoVal __cogito_chip_get_selected(ErgoVal chipv) {
  cogito_node* n = cogito_node_from_val(chipv);
  return YV_BOOL(cogito_chip_get_selected(n));
}

static void __cogito_chip_set_closable(ErgoVal chipv, ErgoVal closable) {
  cogito_node* n = cogito_node_from_val(chipv);
  cogito_chip_set_closable(n, ergo_as_bool(closable));
}

static void __cogito_chip_on_click(ErgoVal chipv, ErgoVal handler) {
  cogito_node* chip = cogito_node_from_val(chipv);
  CogitoHandle* h = (CogitoHandle*)chipv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_chip_on_click(chip, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_chip_on_click(chip, cogito_cb_click, h);
}

static void __cogito_chip_on_close(ErgoVal chipv, ErgoVal handler) {
  cogito_node* chip = cogito_node_from_val(chipv);
  CogitoHandle* h = (CogitoHandle*)chipv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handler);
    cogito_chip_on_close(chip, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_action, handler);
  cogito_chip_on_close(chip, cogito_cb_action, h);
}
// --- Divider ---
static ErgoVal __cogito_divider(ErgoVal orientationv, ErgoVal insetv) {
  ErgoStr* tmp = NULL;
  const char* orientation = NULL;
  if (orientationv.tag != EVT_NULL) {
    orientation = cogito_required_cstr(orientationv, &tmp);
  }
  bool inset = ergo_as_bool(insetv);
  cogito_node* n = cogito_divider_new(orientation, inset);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- Card ---
static ErgoVal __cogito_card(ErgoVal titlev) {
  ErgoStr* tmp = NULL;
  const char* title = NULL;
  if (titlev.tag != EVT_NULL) {
    title = cogito_required_cstr(titlev, &tmp);
  }
  cogito_node* n = cogito_card_new(title);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_card_set_subtitle(ErgoVal cardv, ErgoVal subtitlev) {
  cogito_node* n = cogito_node_from_val(cardv);
  ErgoStr* ts = NULL;
  if (subtitlev.tag == EVT_STR) {
    ts = (ErgoStr*)subtitlev.as.p;
  } else if (subtitlev.tag != EVT_NULL) {
    ts = stdr_to_string(subtitlev);
  }
  cogito_node_set_subtitle(n, ts);
  if (ts && subtitlev.tag != EVT_STR) ergo_release_val(YV_STR(ts));
  cogito_window_relayout(cogito_node_window(n));
}

static void __cogito_card_on_click(ErgoVal cardv, ErgoVal handler) {
  cogito_node* n = cogito_node_from_val(cardv);
  CogitoHandle* h = (CogitoHandle*)cardv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_node_on_click(n, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_node_on_click(n, cogito_cb_click, h);
}

static void __cogito_card_set_variant(ErgoVal cardv, ErgoVal variantv) {
  cogito_node* n = cogito_node_from_val(cardv);
  int v = (int)ergo_as_int(variantv);
  n->card.variant = (uint8_t)(v < 0 ? 0 : v > 2 ? 2 : v);
  if (!n->shadow_set) {
    n->shadow_level = (n->card.variant == 0) ? 1 : 0;
  }
  cogito_window_relayout(cogito_node_window(n));
}

static void __cogito_card_set_header_image(ErgoVal cardv, ErgoVal urlv) {
  cogito_node* n = cogito_node_from_val(cardv);
  if (n->card.header_image) {
    ergo_release_val(YV_OBJ(n->card.header_image));
    n->card.header_image = NULL;
  }
  if (urlv.tag != EVT_NULL) {
    ErgoVal img_val = cogito_image_new(urlv);
    if (img_val.tag == EVT_OBJ) {
      n->card.header_image = (cogito_node*)img_val.as.p;
      // img_val already holds the reference; do not release
    }
  }
  cogito_window_relayout(cogito_node_window(n));
}

static void __cogito_card_add_action(ErgoVal cardv, ErgoVal btnv) {
  cogito_node* card = cogito_node_from_val(cardv);
  if (btnv.tag != EVT_OBJ) return;
  cogito_node* btn = cogito_node_from_val(btnv);
  if (card->card.action_len + 1 > card->card.action_cap) {
    size_t next = card->card.action_cap == 0 ? 4 : card->card.action_cap * 2;
    card->card.actions = (cogito_node**)realloc(card->card.actions, sizeof(cogito_node*) * next);
    card->card.action_cap = next;
  }
  card->card.actions[card->card.action_len++] = btn;
  cogito_children_add(card, btn);
  cogito_apply_style_tree(btn);
  cogito_window_relayout(cogito_node_window(card));
}

static void __cogito_card_add_overflow(ErgoVal cardv, ErgoVal labelv, ErgoVal handler) {
  cogito_node* card = cogito_node_from_val(cardv);
  if (!card->card.overflow_btn) {
    ErgoStr* icon_str = stdr_str_lit("more-vert-symbolic");
    cogito_node* btn = cogito_iconbtn_new_obj(icon_str);
    ergo_release_val(YV_STR(icon_str));
    card->card.overflow_btn = btn;
    cogito_children_add(card, btn);
    cogito_apply_style_tree(btn);
  }
  ErgoStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  CogitoMenuHandler* mh = handler.tag == EVT_FN ? cogito_menu_handler_new(handler) : NULL;
  cogito_iconbtn_add_menu(card->card.overflow_btn, label, mh ? cogito_cb_menu : NULL, mh);
  if (tmp) ergo_release_val(YV_STR(tmp));
  cogito_window_relayout(cogito_node_window(card));
}

// --- Avatar ---
static ErgoVal __cogito_avatar(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = NULL;
  if (textv.tag != EVT_NULL) {
    text = cogito_required_cstr(textv, &tmp);
  }
  cogito_node* n = cogito_avatar_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_avatar_set_image(ErgoVal avatarv, ErgoVal pathv) {
  cogito_node* n = cogito_node_from_val(avatarv);
  ErgoStr* tmp = NULL;
  const char* path = cogito_required_cstr(pathv, &tmp);
  cogito_avatar_set_image(n, path);
  if (tmp) ergo_release_val(YV_STR(tmp));
}
// --- Badge ---
static ErgoVal __cogito_badge(ErgoVal countv) {
  int count = 0;
  if (countv.tag == EVT_INT) count = (int)countv.as.i;
  else if (countv.tag == EVT_FLOAT) count = (int)countv.as.f;
  cogito_node* n = cogito_badge_new(count);
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_badge_set_count(ErgoVal badgev, ErgoVal countv) {
  cogito_node* n = cogito_node_from_val(badgev);
  int count = 0;
  if (countv.tag == EVT_INT) count = (int)countv.as.i;
  else if (countv.tag == EVT_FLOAT) count = (int)countv.as.f;
  cogito_badge_set_count(n, count);
}

static ErgoVal __cogito_badge_get_count(ErgoVal badgev) {
  cogito_node* n = cogito_node_from_val(badgev);
  return YV_INT(cogito_badge_get_count(n));
}
// --- Banner ---
static ErgoVal __cogito_banner(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_banner_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_banner_set_action(ErgoVal bannerv, ErgoVal textv, ErgoVal handlerv) {
  cogito_node* banner = cogito_node_from_val(bannerv);
  CogitoHandle* h = (CogitoHandle*)bannerv.as.p;
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  if (handlerv.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_banner_set_action(banner, text, NULL, NULL);
  } else {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_banner_set_action(banner, text, cogito_cb_action, h);
  }
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_banner_set_icon(ErgoVal bannerv, ErgoVal iconv) {
  cogito_node* banner = cogito_node_from_val(bannerv);
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_banner_set_icon(banner, icon);
  if (tmp) ergo_release_val(YV_STR(tmp));
}
// --- BottomSheet ---
static ErgoVal __cogito_bottom_sheet(ErgoVal titlev) {
  ErgoStr* tmp = NULL;
  const char* title = NULL;
  if (titlev.tag != EVT_NULL) {
    title = cogito_required_cstr(titlev, &tmp);
  }
  cogito_node* n = cogito_bottom_sheet_new(title);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_side_sheet(ErgoVal titlev) {
  ErgoStr* tmp = NULL;
  const char* title = NULL;
  if (titlev.tag != EVT_NULL) {
    title = cogito_required_cstr(titlev, &tmp);
  }
  cogito_node* n = cogito_side_sheet_new(title);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_side_sheet_set_mode(ErgoVal sheetv, ErgoVal modev) {
  cogito_node* n = cogito_node_from_val(sheetv);
  cogito_side_sheet_set_mode(n, (int)ergo_as_int(modev));
}
// --- TimePicker ---
static ErgoVal __cogito_timepicker(void) {
  cogito_node* n = cogito_timepicker_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_timepicker_on_change(ErgoVal tpv, ErgoVal handler) {
  cogito_node* tp = cogito_node_from_val(tpv);
  CogitoHandle* h = (CogitoHandle*)tpv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_timepicker_on_change(tp, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_timepicker_on_change(tp, cogito_cb_change, h);
}

static ErgoVal __cogito_timepicker_get_hour(ErgoVal tpv) {
  cogito_node* n = cogito_node_from_val(tpv);
  return YV_INT(cogito_timepicker_get_hour(n));
}

static ErgoVal __cogito_timepicker_get_minute(ErgoVal tpv) {
  cogito_node* n = cogito_node_from_val(tpv);
  return YV_INT(cogito_timepicker_get_minute(n));
}

static void __cogito_timepicker_set_time(ErgoVal tpv, ErgoVal hourv, ErgoVal minv) {
  cogito_node* n = cogito_node_from_val(tpv);
  int hour = 0, minute = 0;
  if (hourv.tag == EVT_INT) hour = (int)hourv.as.i;
  else if (hourv.tag == EVT_FLOAT) hour = (int)hourv.as.f;
  if (minv.tag == EVT_INT) minute = (int)minv.as.i;
  else if (minv.tag == EVT_FLOAT) minute = (int)minv.as.f;
  cogito_timepicker_set_time(n, hour, minute);
}
// --- ActiveIndicator ---
static ErgoVal __cogito_active_indicator(void) {
  cogito_node* n = cogito_active_indicator_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- SwitchBar ---
static ErgoVal __cogito_switchbar(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_switchbar_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static ErgoVal __cogito_switchbar_get_checked(ErgoVal sbv) {
  cogito_node* n = cogito_node_from_val(sbv);
  return YV_BOOL(cogito_switchbar_get_checked(n));
}
static void __cogito_switchbar_set_checked(ErgoVal sbv, ErgoVal val) {
  cogito_node* n = cogito_node_from_val(sbv);
  cogito_switchbar_set_checked(n, ergo_as_bool(val));
}
static void __cogito_switchbar_on_change(ErgoVal sbv, ErgoVal handler) {
  cogito_node* sb = cogito_node_from_val(sbv);
  CogitoHandle* h = (CogitoHandle*)sbv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_switchbar_on_change(sb, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_switchbar_on_change(sb, cogito_cb_change, h);
}
// --- DrawingArea ---
static ErgoVal __cogito_drawing_area(void) {
  cogito_node* n = cogito_drawing_area_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_drawing_area_on_press(ErgoVal dav, ErgoVal handler) {
  cogito_node* da = cogito_node_from_val(dav);
  CogitoHandle* h = (CogitoHandle*)dav.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_drawing_area_on_press(da, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_drawing_area_on_press(da, cogito_cb_click, h);
}
static void __cogito_drawing_area_on_drag(ErgoVal dav, ErgoVal handler) {
  cogito_node* da = cogito_node_from_val(dav);
  CogitoHandle* h = (CogitoHandle*)dav.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_drawing_area_on_drag(da, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_drawing_area_on_drag(da, cogito_cb_change, h);
}
static void __cogito_drawing_area_on_release(ErgoVal dav, ErgoVal handler) {
  cogito_node* da = cogito_node_from_val(dav);
  CogitoHandle* h = (CogitoHandle*)dav.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handler);
    cogito_drawing_area_on_release(da, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_action, handler);
  cogito_drawing_area_on_release(da, cogito_cb_action, h);
}
static void __cogito_drawing_area_on_draw(ErgoVal dav, ErgoVal handler) {
  cogito_node* da = cogito_node_from_val(dav);
  CogitoHandle* h = (CogitoHandle*)dav.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_draw, handler);
    cogito_drawing_area_on_draw(da, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_draw, handler);
  cogito_drawing_area_on_draw(da, cogito_cb_draw, h);
}
static ErgoVal __cogito_drawing_area_get_x(ErgoVal dav) {
  cogito_node* da = cogito_node_from_val(dav);
  return YV_INT(cogito_drawing_area_get_x(da));
}
static ErgoVal __cogito_drawing_area_get_y(ErgoVal dav) {
  cogito_node* da = cogito_node_from_val(dav);
  return YV_INT(cogito_drawing_area_get_y(da));
}
static ErgoVal __cogito_drawing_area_get_pressed(ErgoVal dav) {
  cogito_node* da = cogito_node_from_val(dav);
  return YV_BOOL(cogito_drawing_area_get_pressed(da));
}
static void __cogito_drawing_area_clear(ErgoVal dav) {
  cogito_node* da = cogito_node_from_val(dav);
  cogito_drawing_area_clear(da);
}
static void __cogito_canvas_set_color(ErgoVal dav, ErgoVal colorv) {
  cogito_node* da = cogito_node_from_val(dav);
  ErgoStr* tmp = NULL;
  const char* color = cogito_required_cstr(colorv, &tmp);
  cogito_canvas_set_color(da, color);
  if (tmp) ergo_release_val(YV_STR(tmp));
}
static void __cogito_canvas_set_line_width(ErgoVal dav, ErgoVal widthv) {
  cogito_node* da = cogito_node_from_val(dav);
  cogito_canvas_set_line_width(da, (int)ergo_as_int(widthv));
}
static void __cogito_canvas_line(ErgoVal dav, ErgoVal x1v, ErgoVal y1v,
                                 ErgoVal x2v, ErgoVal y2v) {
  cogito_node* da = cogito_node_from_val(dav);
  cogito_canvas_line(da, (int)ergo_as_int(x1v), (int)ergo_as_int(y1v),
                     (int)ergo_as_int(x2v), (int)ergo_as_int(y2v));
}
static void __cogito_canvas_rect(ErgoVal dav, ErgoVal xv, ErgoVal yv,
                                 ErgoVal wv, ErgoVal hv) {
  cogito_node* da = cogito_node_from_val(dav);
  cogito_canvas_rect(da, (int)ergo_as_int(xv), (int)ergo_as_int(yv),
                     (int)ergo_as_int(wv), (int)ergo_as_int(hv));
}
static void __cogito_canvas_fill_rect(ErgoVal dav, ErgoVal xv, ErgoVal yv,
                                      ErgoVal wv, ErgoVal hv) {
  cogito_node* da = cogito_node_from_val(dav);
  cogito_canvas_fill_rect(da, (int)ergo_as_int(xv), (int)ergo_as_int(yv),
                          (int)ergo_as_int(wv), (int)ergo_as_int(hv));
}
// --- Shape ---
static ErgoVal __cogito_shape(ErgoVal presetv) {
  cogito_node* n = cogito_shape_new((int)ergo_as_int(presetv));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_shape_set_preset(ErgoVal shapev, ErgoVal presetv) {
  cogito_node* n = cogito_node_from_val(shapev);
  cogito_shape_set_preset(n, (int)ergo_as_int(presetv));
}
static ErgoVal __cogito_shape_get_preset(ErgoVal shapev) {
  cogito_node* n = cogito_node_from_val(shapev);
  return YV_INT(cogito_shape_get_preset(n));
}
static void __cogito_shape_set_size(ErgoVal shapev, ErgoVal sizev) {
  cogito_node* n = cogito_node_from_val(shapev);
  cogito_shape_set_size(n, (int)ergo_as_int(sizev));
}
static ErgoVal __cogito_shape_get_size(ErgoVal shapev) {
  cogito_node* n = cogito_node_from_val(shapev);
  return YV_INT(cogito_shape_get_size(n));
}
static void __cogito_shape_set_color(ErgoVal shapev, ErgoVal colorv) {
  cogito_node* n = cogito_node_from_val(shapev);
  if (colorv.tag == EVT_INT || colorv.tag == EVT_FLOAT) {
    cogito_shape_set_color_style(n, (int)ergo_as_int(colorv));
    return;
  }
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(colorv, &tmp);
  cogito_shape_set_color(n, text);
  if (tmp) ergo_release_val(YV_STR(tmp));
}
static void __cogito_shape_set_color_style(ErgoVal shapev, ErgoVal stylev) {
  cogito_node* n = cogito_node_from_val(shapev);
  cogito_shape_set_color_style(n, (int)ergo_as_int(stylev));
}
static ErgoVal __cogito_shape_get_color_style(ErgoVal shapev) {
  cogito_node* n = cogito_node_from_val(shapev);
  return YV_INT(cogito_shape_get_color_style(n));
}
static void __cogito_shape_set_vertex(ErgoVal shapev, ErgoVal idxv, ErgoVal xv, ErgoVal yv) {
  cogito_node* n = cogito_node_from_val(shapev);
  cogito_shape_set_vertex(n, (int)ergo_as_int(idxv), (float)ergo_as_float(xv),
                          (float)ergo_as_float(yv));
}
static ErgoVal __cogito_shape_get_vertex_x(ErgoVal shapev, ErgoVal idxv) {
  cogito_node* n = cogito_node_from_val(shapev);
  return YV_FLOAT((double)cogito_shape_get_vertex_x(n, (int)ergo_as_int(idxv)));
}
static ErgoVal __cogito_shape_get_vertex_y(ErgoVal shapev, ErgoVal idxv) {
  cogito_node* n = cogito_node_from_val(shapev);
  return YV_FLOAT((double)cogito_shape_get_vertex_y(n, (int)ergo_as_int(idxv)));
}
// --- ContentList ---
static ErgoVal __cogito_content_list(void) {
  cogito_node* n = cogito_content_list_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- EmptyPage ---
static ErgoVal __cogito_empty_page(ErgoVal titlev) {
  ErgoStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_node* n = cogito_empty_page_new(title);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_empty_page_set_description(ErgoVal epv, ErgoVal descv) {
  cogito_node* n = cogito_node_from_val(epv);
  ErgoStr* tmp = NULL;
  const char* desc = cogito_required_cstr(descv, &tmp);
  cogito_empty_page_set_description(n, desc);
  if (tmp) ergo_release_val(YV_STR(tmp));
}
static void __cogito_empty_page_set_icon(ErgoVal epv, ErgoVal iconv) {
  cogito_node* n = cogito_node_from_val(epv);
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_empty_page_set_icon(n, icon);
  if (tmp) ergo_release_val(YV_STR(tmp));
}
static void __cogito_empty_page_set_action(ErgoVal epv, ErgoVal textv, ErgoVal handlerv) {
  cogito_node* ep = cogito_node_from_val(epv);
  CogitoHandle* h = (CogitoHandle*)epv.as.p;
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  if (handlerv.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_empty_page_set_action(ep, text, NULL, NULL);
  } else {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_empty_page_set_action(ep, text, cogito_cb_action, h);
  }
  if (tmp) ergo_release_val(YV_STR(tmp));
}
// --- TipView ---
static ErgoVal __cogito_tip_view(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_tip_view_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_tip_view_set_title(ErgoVal tvv, ErgoVal titlev) {
  cogito_node* n = cogito_node_from_val(tvv);
  ErgoStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_tip_view_set_title(n, title);
  if (tmp) ergo_release_val(YV_STR(tmp));
}
// --- SettingsWindow ---
static ErgoVal __cogito_settings_window(ErgoVal titlev) {
  ErgoStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_node* n = cogito_settings_window_new(title);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- SettingsPage ---
static ErgoVal __cogito_settings_page(ErgoVal titlev) {
  ErgoStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_node* n = cogito_settings_page_new(title);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- SettingsList ---
static ErgoVal __cogito_settings_list(ErgoVal titlev) {
  ErgoStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_node* n = cogito_settings_list_new(title);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- SettingsRow ---
static ErgoVal __cogito_settings_row(ErgoVal labelv) {
  ErgoStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  cogito_node* n = cogito_settings_row_new(label);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- WelcomeScreen ---
static ErgoVal __cogito_welcome_screen(ErgoVal titlev) {
  ErgoStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_node* n = cogito_welcome_screen_new(title);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_welcome_screen_set_description(ErgoVal wsv, ErgoVal descv) {
  cogito_node* n = cogito_node_from_val(wsv);
  ErgoStr* tmp = NULL;
  const char* desc = cogito_required_cstr(descv, &tmp);
  cogito_welcome_screen_set_description(n, desc);
  if (tmp) ergo_release_val(YV_STR(tmp));
}
static void __cogito_welcome_screen_set_icon(ErgoVal wsv, ErgoVal iconv) {
  cogito_node* n = cogito_node_from_val(wsv);
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_welcome_screen_set_icon(n, icon);
  if (tmp) ergo_release_val(YV_STR(tmp));
}
static void __cogito_welcome_screen_set_action(ErgoVal wsv, ErgoVal textv, ErgoVal handlerv) {
  cogito_node* ws = cogito_node_from_val(wsv);
  CogitoHandle* h = (CogitoHandle*)wsv.as.p;
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  if (handlerv.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_welcome_screen_set_action(ws, text, NULL, NULL);
  } else {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_welcome_screen_set_action(ws, text, cogito_cb_action, h);
  }
  if (tmp) ergo_release_val(YV_STR(tmp));
}
// --- ViewDual ---
static ErgoVal __cogito_view_dual(void) {
  cogito_node* n = cogito_view_dual_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_view_dual_set_ratio(ErgoVal vdv, ErgoVal ratiov) {
  cogito_node* n = cogito_node_from_val(vdv);
  double ratio = ergo_as_float(ratiov);
  cogito_view_dual_set_ratio(n, ratio);
}
// --- ViewChooser ---
static ErgoVal __cogito_view_chooser(void) {
  cogito_node* n = cogito_view_chooser_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_view_chooser_set_items(ErgoVal vcv, ErgoVal arrv) {
  cogito_node* n = cogito_node_from_val(vcv);
  if (arrv.tag != EVT_ARR) ergo_trap("view_chooser_set_items expects array");
  ErgoArr* a = (ErgoArr*)arrv.as.p;
  size_t len = (size_t)a->len;
  const char** items = (const char**)malloc(sizeof(const char*) * len);
  ErgoStr** tmps = (ErgoStr**)malloc(sizeof(ErgoStr*) * len);
  for (size_t i = 0; i < len; i++) {
    ErgoVal v = ergo_arr_get(a, (int64_t)i);
    tmps[i
    ] = NULL;
    items[i
    ] = cogito_required_cstr(v, &tmps[i
    ]);
  }
  cogito_view_chooser_set_items(n, items, len);
  for (size_t i = 0; i < len; i++) {
    if (tmps[i
    ]) ergo_release_val(YV_STR(tmps[i
    ]));
  }
  free(items);
  free(tmps);
}
static void __cogito_view_chooser_bind(ErgoVal vcv, ErgoVal vsv) {
  cogito_node* vc = cogito_node_from_val(vcv);
  cogito_node* vs = cogito_node_from_val(vsv);
  cogito_view_chooser_bind(vc, vs);
}
// --- AboutWindow ---
static ErgoVal __cogito_about_window(ErgoVal namev, ErgoVal versionv) {
  ErgoStr* tmp1 = NULL;
  ErgoStr* tmp2 = NULL;
  const char* name = cogito_required_cstr(namev, &tmp1);
  const char* version = cogito_required_cstr(versionv, &tmp2);
  cogito_node* n = cogito_about_window_new(name, version);
  if (tmp1) ergo_release_val(YV_STR(tmp1));
  if (tmp2) ergo_release_val(YV_STR(tmp2));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_about_window_set_icon(ErgoVal awv, ErgoVal iconv) {
  cogito_node* n = cogito_node_from_val(awv);
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_about_window_set_icon(n, icon);
  if (tmp) ergo_release_val(YV_STR(tmp));
}
static void __cogito_about_window_set_description(ErgoVal awv, ErgoVal descv) {
  cogito_node* n = cogito_node_from_val(awv);
  ErgoStr* tmp = NULL;
  const char* desc = cogito_required_cstr(descv, &tmp);
  cogito_about_window_set_description(n, desc);
  if (tmp) ergo_release_val(YV_STR(tmp));
}
static void __cogito_about_window_set_website(ErgoVal awv, ErgoVal urlv) {
  cogito_node* n = cogito_node_from_val(awv);
  ErgoStr* tmp = NULL;
  const char* url = cogito_required_cstr(urlv, &tmp);
  cogito_about_window_set_website(n, url);
  if (tmp) ergo_release_val(YV_STR(tmp));
}
static void __cogito_about_window_set_issue_url(ErgoVal awv, ErgoVal urlv) {
  cogito_node* n = cogito_node_from_val(awv);
  ErgoStr* tmp = NULL;
  const char* url = cogito_required_cstr(urlv, &tmp);
  cogito_about_window_set_issue_url(n, url);
  if (tmp) ergo_release_val(YV_STR(tmp));
}
// --- MenuButton ---
static ErgoVal __cogito_menu_button(ErgoVal iconv) {
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_node* n = cogito_menu_button_new(icon);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- SplitButton ---
static ErgoVal __cogito_split_button(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_split_button_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_split_button_add_menu(ErgoVal btnv, ErgoVal labelv, ErgoVal handlerv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  CogitoHandle* h = (CogitoHandle*)btnv.as.p;
  ErgoStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  if (handlerv.tag == EVT_FN) {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_split_button_add_menu(btn, label, cogito_cb_action, h);
  } else {
    cogito_split_button_add_menu(btn, label, NULL, NULL);
  }
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_split_button_add_menu_section(ErgoVal btnv, ErgoVal labelv, ErgoVal handlerv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  CogitoHandle* h = (CogitoHandle*)btnv.as.p;
  ErgoStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  if (handlerv.tag == EVT_FN) {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_split_button_add_menu_section(btn, label, cogito_cb_action, h);
  } else {
    cogito_split_button_add_menu_section(btn, label, NULL, NULL);
  }
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_split_button_set_size(ErgoVal btnv, ErgoVal sizev) {
  cogito_node* btn = cogito_node_from_val(btnv);
  int size = (int)ergo_as_int(sizev);
  cogito_split_button_set_size(btn, size);
}

static void __cogito_split_button_set_variant(ErgoVal btnv, ErgoVal variantv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  int variant = (int)ergo_as_int(variantv);
  cogito_split_button_set_variant(btn, variant);
}

static ErgoVal __cogito_image(ErgoVal iconv) {
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_node* n = cogito_image_new(icon);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_image_set_icon(ErgoVal imgv, ErgoVal iconv) {
  cogito_node* n = cogito_node_from_val(imgv);
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_image_set_icon(n, icon);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_image_set_source(ErgoVal imgv, ErgoVal sourcev) {
  cogito_node* n = cogito_node_from_val(imgv);
  ErgoStr* tmp = NULL;
  const char* source = cogito_required_cstr(sourcev, &tmp);
  cogito_image_set_source(n, source);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_image_set_size(ErgoVal imgv, ErgoVal wv, ErgoVal hv) {
  cogito_node* n = cogito_node_from_val(imgv);
  int w = (int)ergo_as_int(wv);
  int h = (int)ergo_as_int(hv);
  cogito_image_set_size(n, w, h);
}

static void __cogito_image_set_radius(ErgoVal imgv, ErgoVal rv) {
  cogito_node* n = cogito_node_from_val(imgv);
  int r = (int)ergo_as_int(rv);
  cogito_image_set_radius(n, r);
}

static void __cogito_image_set_alt_text(ErgoVal imgv, ErgoVal altv) {
  cogito_node* n = cogito_node_from_val(imgv);
  ErgoStr* tmp = NULL;
  const char* alt = cogito_optional_cstr(altv, &tmp);
  if (alt) cogito_image_set_alt_text(n, alt);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static ErgoVal __cogito_appbar(ErgoVal titlev, ErgoVal subtitlev) {
  ErgoStr* ttmp = NULL;
  ErgoStr* stmp = NULL;
  const char* title = cogito_required_cstr(titlev, &ttmp);
  const char* subtitle = cogito_required_cstr(subtitlev, &stmp);
  cogito_node* n = cogito_appbar_new(title, subtitle);
  if (ttmp) ergo_release_val(YV_STR(ttmp));
  if (stmp) ergo_release_val(YV_STR(stmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_appbar_add_button(ErgoVal appbarv, ErgoVal iconv, ErgoVal handler) {
  cogito_node* appbar = cogito_node_from_val(appbarv);
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_node* btn = cogito_appbar_add_button(appbar, icon, NULL, NULL);
  if (tmp) ergo_release_val(YV_STR(tmp));
  ErgoVal btnv = cogito_wrap_node(btn, COGITO_HANDLE_NODE);
  if (handler.tag == EVT_FN) __cogito_button_on_click(btnv, handler);
  return btnv;
}

static void __cogito_appbar_set_controls(ErgoVal appbarv, ErgoVal layoutv) {
  cogito_node* appbar = cogito_node_from_val(appbarv);
  ErgoStr* tmp = NULL;
  const char* layout = cogito_optional_cstr(layoutv, &tmp);
  if (layout) cogito_appbar_set_controls(appbar, layout);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_appbar_set_title(ErgoVal appbarv, ErgoVal titlev) {
  cogito_node* appbar = cogito_node_from_val(appbarv);
  ErgoStr* tmp = NULL;
  const char* title = cogito_optional_cstr(titlev, &tmp);
  cogito_appbar_set_title(appbar, title ? title : "");
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_appbar_set_subtitle(ErgoVal appbarv, ErgoVal subtitlev) {
  cogito_node* appbar = cogito_node_from_val(appbarv);
  ErgoStr* tmp = NULL;
  const char* subtitle = cogito_optional_cstr(subtitlev, &tmp);
  cogito_appbar_set_subtitle(appbar, subtitle ? subtitle : "");
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static ErgoVal __cogito_dialog(ErgoVal titlev) {
  ErgoStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_node* n = cogito_dialog_new(title);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_dialog_close(ErgoVal dialogv) {
  cogito_node* dialog = cogito_node_from_val(dialogv);
  cogito_dialog_close(dialog);
}

static void __cogito_dialog_remove(ErgoVal dialogv) {
  cogito_node* dialog = cogito_node_from_val(dialogv);
  cogito_dialog_remove(dialog);
}

static ErgoVal __cogito_dialog_slot(void) {
  cogito_node* n = cogito_dialog_slot_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_dialog_slot_show(ErgoVal slotv, ErgoVal dialogv) {
  cogito_node* slot = cogito_node_from_val(slotv);
  cogito_node* dialog = cogito_node_from_val(dialogv);
  cogito_dialog_slot_show(slot, dialog);
}

static void __cogito_dialog_slot_clear(ErgoVal slotv) {
  cogito_node* slot = cogito_node_from_val(slotv);
  cogito_dialog_slot_clear(slot);
}

static void __cogito_window_set_dialog(ErgoVal winv, ErgoVal dialogv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_node* dialog = cogito_node_from_val(dialogv);
  cogito_window_set_dialog(win, dialog);
}

static void __cogito_window_clear_dialog(ErgoVal winv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_window_clear_dialog(win);
}

static void __cogito_window_set_side_sheet(ErgoVal winv, ErgoVal side_sheetv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_node* side_sheet = cogito_node_from_val(side_sheetv);
  cogito_window_set_side_sheet(win, side_sheet);
}

static void __cogito_window_clear_side_sheet(ErgoVal winv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_window_clear_side_sheet(win);
}

static ErgoVal __cogito_node_window(ErgoVal nodev) {
  cogito_node* n = cogito_node_from_val(nodev);
  cogito_window* win = cogito_node_window(n);
  return cogito_wrap_node((cogito_node*)win, COGITO_HANDLE_WINDOW);
}

static ErgoVal __cogito_find_parent(ErgoVal nodev) {
  cogito_node* n = cogito_node_from_val(nodev);
  cogito_node* p = cogito_node_get_parent(n);
  if (!p) return YV_NULLV;
  return cogito_wrap_node(p, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_find_children(ErgoVal nodev) {
  cogito_node* n = cogito_node_from_val(nodev);
  size_t count = cogito_node_get_child_count(n);
  ErgoArr* arr = stdr_arr_new((int)count);
  for (size_t i = 0; i < count; i++) {
    cogito_node* child = cogito_node_get_child(n, i);
    ergo_arr_add(arr, cogito_wrap_node(child, COGITO_HANDLE_NODE));
  }
  return YV_ARR(arr);
}

static void __cogito_label_set_class(ErgoVal labelv, ErgoVal classv) {
  cogito_node* n = cogito_node_from_val(labelv);
  ErgoStr* tmp = NULL;
  const char* cls = cogito_optional_cstr(classv, &tmp);
  if (cls) cogito_node_set_class(n, cls);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_label_set_text(ErgoVal labelv, ErgoVal textv) {
  cogito_node* n = cogito_node_from_val(labelv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_optional_cstr(textv, &tmp);
  if (text) cogito_node_set_text(n, text);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_node_set_class(ErgoVal nodev, ErgoVal classv) {
  cogito_node* n = cogito_node_from_val(nodev);
  ErgoStr* tmp = NULL;
  const char* cls = cogito_optional_cstr(classv, &tmp);
  if (cls) cogito_node_set_class(n, cls);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_node_set_a11y_label(ErgoVal nodev, ErgoVal labelv) {
  cogito_node* n = cogito_node_from_val(nodev);
  ErgoStr* tmp = NULL;
  const char* label = cogito_optional_cstr(labelv, &tmp);
  if (label) cogito_node_set_a11y_label(n, label);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_node_set_a11y_role(ErgoVal nodev, ErgoVal rolev) {
  cogito_node* n = cogito_node_from_val(nodev);
  ErgoStr* tmp = NULL;
  const char* role = cogito_optional_cstr(rolev, &tmp);
  if (role) cogito_node_set_a11y_role(n, role);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_node_set_tooltip(ErgoVal nodev, ErgoVal textv) {
  cogito_node* n = cogito_node_from_val(nodev);
  ErgoStr* tmp = NULL;
  const char* text = cogito_optional_cstr(textv, &tmp);
  if (text) cogito_node_set_tooltip(n, text);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_pointer_capture(ErgoVal nodev) {
  if (nodev.tag == EVT_NULL) {
    cogito_pointer_release();
    return;
  }
  cogito_node* n = cogito_node_from_val(nodev);
  cogito_pointer_capture(n);
}

static void __cogito_pointer_release(void) {
  cogito_pointer_release();
}

static void __cogito_label_set_wrap(ErgoVal labelv, ErgoVal onv) {
  cogito_node* n = cogito_node_from_val(labelv);
  cogito_label_set_wrap(n, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_label_set_ellipsis(ErgoVal labelv, ErgoVal onv) {
  cogito_node* n = cogito_node_from_val(labelv);
  cogito_label_set_ellipsis(n, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_label_set_align(ErgoVal labelv, ErgoVal alignv) {
  cogito_node* n = cogito_node_from_val(labelv);
  cogito_label_set_align(n, (int)ergo_as_int(alignv));
}

static ErgoVal __cogito_checkbox(ErgoVal textv, ErgoVal groupv) {
  ErgoStr* ttmp = NULL;
  ErgoStr* gtmp = NULL;
  const char* text = cogito_required_cstr(textv, &ttmp);
  const char* group = cogito_optional_cstr(groupv, &gtmp);
  cogito_node* n = cogito_checkbox_new(text, group);
  if (ttmp) ergo_release_val(YV_STR(ttmp));
  if (gtmp) ergo_release_val(YV_STR(gtmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_switch(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_switch_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_textfield(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_textfield_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_textview(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_textview_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_searchfield(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_searchfield_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_searchfield_set_text(ErgoVal sfv, ErgoVal textv) {
  cogito_node* sf = cogito_node_from_val(sfv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_searchfield_set_text(sf, text);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static ErgoVal __cogito_searchfield_get_text(ErgoVal sfv) {
  cogito_node* sf = cogito_node_from_val(sfv);
  const char* text = cogito_searchfield_get_text(sf);
  return YV_STR(stdr_str_lit(text ? text : ""));
}

static void __cogito_searchfield_on_change(ErgoVal sfv, ErgoVal handler) {
  cogito_node* sf = cogito_node_from_val(sfv);
  CogitoHandle* h = (CogitoHandle*)sfv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_searchfield_on_change(sf, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_searchfield_on_change(sf, cogito_cb_change, h);
}

static ErgoVal __cogito_dropdown(void) {
  cogito_node* n = cogito_dropdown_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_datepicker(void) {
  cogito_node* n = cogito_datepicker_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_datepicker_on_change(ErgoVal dpv, ErgoVal handler) {
  cogito_node* dp = cogito_node_from_val(dpv);
  CogitoHandle* h = (CogitoHandle*)dpv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_datepicker_on_change(dp, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_datepicker_on_change(dp, cogito_cb_change, h);
}

static ErgoVal __cogito_stepper(ErgoVal minv, ErgoVal maxv, ErgoVal valuev, ErgoVal stepv) {
  cogito_node* n = cogito_stepper_new(ergo_as_float(minv), ergo_as_float(maxv), ergo_as_float(valuev), ergo_as_float(stepv));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_slider(ErgoVal minv, ErgoVal maxv, ErgoVal valuev) {
  cogito_node* n = cogito_slider_new(ergo_as_float(minv), ergo_as_float(maxv), ergo_as_float(valuev));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_slider_range(ErgoVal minv, ErgoVal maxv, ErgoVal startv, ErgoVal endv) {
  cogito_node* n = cogito_slider_range_new(ergo_as_float(minv), ergo_as_float(maxv), ergo_as_float(startv), ergo_as_float(endv));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_tabs(void) {
  cogito_node* n = cogito_tabs_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_nav_rail(void) {
  cogito_node* n = cogito_nav_rail_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_buttongroup(void) {
  cogito_node* n = cogito_buttongroup_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_view_switcher(void) {
  cogito_node* n = cogito_view_switcher_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_progress(ErgoVal valuev) {
  cogito_node* n = cogito_progress_new(ergo_as_float(valuev));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_treeview(void) {
  cogito_node* n = cogito_treeview_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_colorpicker(void) {
  cogito_node* n = cogito_colorpicker_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_colorpicker_on_change(ErgoVal cpv, ErgoVal handler) {
  cogito_node* cp = cogito_node_from_val(cpv);
  CogitoHandle* h = (CogitoHandle*)cpv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_colorpicker_on_change(cp, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_colorpicker_on_change(cp, cogito_cb_change, h);
}

static void __cogito_colorpicker_set_hex(ErgoVal cpv, ErgoVal hexv) {
  cogito_node* cp = cogito_node_from_val(cpv);
  ErgoStr* tmp = NULL;
  const char* hex = cogito_optional_cstr(hexv, &tmp);
  if (hex) cogito_colorpicker_set_hex(cp, hex);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static ErgoVal __cogito_colorpicker_get_hex(ErgoVal cpv) {
  cogito_node* cp = cogito_node_from_val(cpv);
  const char* hex = cogito_colorpicker_get_hex(cp);
  return YV_STR(stdr_str_lit(hex ? hex : ""));
}

static ErgoVal __cogito_toasts(void) {
  cogito_node* n = cogito_toasts_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_toast(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_toast_new(text);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_toolbar(void) {
  cogito_node* n = cogito_toolbar_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_toolbar_set_vibrant(ErgoVal toolbarv, ErgoVal vibrantv) {
  cogito_node* toolbar = cogito_node_from_val(toolbarv);
  cogito_toolbar_set_vibrant(toolbar, ergo_as_bool(vibrantv));
}

static ErgoVal __cogito_toolbar_get_vibrant(ErgoVal toolbarv) {
  cogito_node* toolbar = cogito_node_from_val(toolbarv);
  return YV_BOOL(cogito_toolbar_get_vibrant(toolbar));
}

static void __cogito_toolbar_set_vertical(ErgoVal toolbarv, ErgoVal verticalv) {
  cogito_node* toolbar = cogito_node_from_val(toolbarv);
  cogito_toolbar_set_vertical(toolbar, ergo_as_bool(verticalv));
}

static ErgoVal __cogito_toolbar_get_vertical(ErgoVal toolbarv) {
  cogito_node* toolbar = cogito_node_from_val(toolbarv);
  return YV_BOOL(cogito_toolbar_get_vertical(toolbar));
}

static ErgoVal __cogito_vstack(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_VSTACK), COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_hstack(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_HSTACK), COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_zstack(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_ZSTACK), COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_fixed(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_FIXED), COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_scroller(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_SCROLLER), COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_carousel(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_CAROUSEL), COGITO_HANDLE_NODE);
}

static void __cogito_carousel_set_active_index(ErgoVal carouselv, ErgoVal idxv) {
  cogito_node* carousel = cogito_node_from_val(carouselv);
  cogito_carousel_set_active_index(carousel, (int)ergo_as_int(idxv));
}

static ErgoVal __cogito_carousel_get_active_index(ErgoVal carouselv) {
  cogito_node* carousel = cogito_node_from_val(carouselv);
  int idx = cogito_carousel_get_active_index(carousel);
  return YV_INT(idx);
}

static ErgoVal __cogito_carousel_item(void) {
  return cogito_wrap_node(cogito_carousel_item_new(), COGITO_HANDLE_NODE);
}

static void __cogito_carousel_item_set_text(ErgoVal itemv, ErgoVal textv) {
  cogito_node* item = cogito_node_from_val(itemv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_carousel_item_set_text(item, text);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_carousel_item_set_halign(ErgoVal itemv, ErgoVal alignv) {
  cogito_node* item = cogito_node_from_val(itemv);
  cogito_carousel_item_set_halign(item, (int)ergo_as_int(alignv));
}

static void __cogito_carousel_item_set_valign(ErgoVal itemv, ErgoVal alignv) {
  cogito_node* item = cogito_node_from_val(itemv);
  cogito_carousel_item_set_valign(item, (int)ergo_as_int(alignv));
}

static ErgoVal __cogito_list(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_LIST), COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_grid(ErgoVal cols) {
  return cogito_wrap_node(cogito_grid_new_with_cols((int)ergo_as_int(cols)), COGITO_HANDLE_NODE);
}

static void __cogito_container_add(ErgoVal parentv, ErgoVal childv) {
  cogito_node* parent = cogito_node_from_val(parentv);
  cogito_node* child = cogito_node_from_val(childv);
  cogito_node_add(parent, child);
}

static void __cogito_container_set_margins(ErgoVal nodev, ErgoVal top, ErgoVal right, ErgoVal bottom, ErgoVal left) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_margins(node, (int)ergo_as_int(top), (int)ergo_as_int(right), (int)ergo_as_int(bottom), (int)ergo_as_int(left));
}

static void __cogito_container_set_align(ErgoVal nodev, ErgoVal align) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_align(node, (int)ergo_as_int(align));
}

static void __cogito_container_set_halign(ErgoVal nodev, ErgoVal align) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_halign(node, (int)ergo_as_int(align));
}

static void __cogito_container_set_valign(ErgoVal nodev, ErgoVal align) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_valign(node, (int)ergo_as_int(align));
}

static void __cogito_container_set_hexpand(ErgoVal nodev, ErgoVal expand) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_hexpand(node, ergo_as_bool(expand));
}

static void __cogito_container_set_vexpand(ErgoVal nodev, ErgoVal expand) {
  cogito_node* node = cogito_node_from_val(nodev);
  bool expand_bool = ergo_as_bool(expand);
  cogito_node_set_vexpand(node, expand_bool);
}

static void __cogito_container_set_gap(ErgoVal nodev, ErgoVal gap) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_gap(node, (int)ergo_as_int(gap));
}

static void __cogito_container_set_padding(ErgoVal nodev, ErgoVal top, ErgoVal right, ErgoVal bottom, ErgoVal left) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_padding(node, (int)ergo_as_int(top), (int)ergo_as_int(right), (int)ergo_as_int(bottom), (int)ergo_as_int(left));
}

static void __cogito_fixed_set_pos(ErgoVal fixedv, ErgoVal childv, ErgoVal xv, ErgoVal yv) {
  cogito_node* fixed = cogito_node_from_val(fixedv);
  cogito_node* child = cogito_node_from_val(childv);
  cogito_fixed_set_pos(fixed, child, (int)ergo_as_int(xv), (int)ergo_as_int(yv));
}

static void __cogito_scroller_set_axes(ErgoVal scv, ErgoVal hv, ErgoVal vv) {
  cogito_node* sc = cogito_node_from_val(scv);
  bool h = hv.tag == EVT_BOOL ? hv.as.b : false;
  bool v = vv.tag == EVT_BOOL ? vv.as.b : false;
  cogito_scroller_set_axes(sc, h, v);
}

static void __cogito_grid_set_gap(ErgoVal gridv, ErgoVal xv, ErgoVal yv) {
  cogito_node* grid = cogito_node_from_val(gridv);
  cogito_grid_set_gap(grid, (int)ergo_as_int(xv), (int)ergo_as_int(yv));
}

static void __cogito_grid_set_span(ErgoVal childv, ErgoVal col_span, ErgoVal row_span) {
  cogito_node* child = cogito_node_from_val(childv);
  cogito_grid_set_span(child, (int)ergo_as_int(col_span), (int)ergo_as_int(row_span));
}

static void __cogito_grid_set_align(ErgoVal childv, ErgoVal halign, ErgoVal valign) {
  cogito_node* child = cogito_node_from_val(childv);
  cogito_grid_set_align(child, (int)ergo_as_int(halign), (int)ergo_as_int(valign));
}

static void __cogito_node_set_disabled(ErgoVal nodev, ErgoVal onv) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_disabled(node, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_node_set_editable(ErgoVal nodev, ErgoVal onv) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_editable(node, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static ErgoVal __cogito_node_get_editable(ErgoVal nodev) {
  cogito_node* node = cogito_node_from_val(nodev);
  return YV_BOOL(cogito_node_get_editable(node));
}

static void __cogito_node_set_id(ErgoVal nodev, ErgoVal idv) {
  cogito_node* node = cogito_node_from_val(nodev);
  ErgoStr* tmp = NULL;
  const char* id = cogito_optional_cstr(idv, &tmp);
  if (id) cogito_node_set_id(node, id);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_button_set_text(ErgoVal btnv, ErgoVal textv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_button_set_text(btn, text);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_button_set_size(ErgoVal btnv, ErgoVal sizev) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_size(btn, (int)ergo_as_int(sizev));
}

static ErgoVal __cogito_button_get_size(ErgoVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_button_get_size(btn));
}

static void __cogito_button_on_click(ErgoVal btnv, ErgoVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  CogitoHandle* h = (CogitoHandle*)btnv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_node_on_click(btn, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_node_on_click(btn, cogito_cb_click, h);
}

static void __cogito_button_add_menu(ErgoVal btnv, ErgoVal labelv, ErgoVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  ErgoStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  CogitoMenuHandler* mh = handler.tag == EVT_FN ? cogito_menu_handler_new(handler) : NULL;
  cogito_button_add_menu(btn, label, mh ? cogito_cb_menu : NULL, mh);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_button_add_menu_section(ErgoVal btnv, ErgoVal labelv, ErgoVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  ErgoStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  CogitoMenuHandler* mh = handler.tag == EVT_FN ? cogito_menu_handler_new(handler) : NULL;
  cogito_button_add_menu_section(btn, label, mh ? cogito_cb_menu : NULL, mh);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_iconbtn_add_menu(ErgoVal btnv, ErgoVal labelv, ErgoVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  ErgoStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  CogitoMenuHandler* mh = handler.tag == EVT_FN ? cogito_menu_handler_new(handler) : NULL;
  cogito_iconbtn_add_menu(btn, label, mh ? cogito_cb_menu : NULL, mh);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_iconbtn_add_menu_section(ErgoVal btnv, ErgoVal labelv, ErgoVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  ErgoStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  CogitoMenuHandler* mh = handler.tag == EVT_FN ? cogito_menu_handler_new(handler) : NULL;
  cogito_iconbtn_add_menu_section(btn, label, mh ? cogito_cb_menu : NULL, mh);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

// Menu item property setters (work on any node with menu items)
// Use public API (cogito_menu_set_*) so these compile against the opaque cogito_node.
static void __cogito_menu_set_icon(ErgoVal nodev, ErgoVal iconv) {
  cogito_node* n = cogito_node_from_val(nodev);
  if (!n) return;
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_menu_set_icon(n, icon);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_menu_set_shortcut(ErgoVal nodev, ErgoVal shortcutv) {
  cogito_node* n = cogito_node_from_val(nodev);
  if (!n) return;
  ErgoStr* tmp = NULL;
  const char* shortcut = cogito_required_cstr(shortcutv, &tmp);
  cogito_menu_set_shortcut(n, shortcut);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_menu_set_submenu(ErgoVal nodev, ErgoVal submenuv) {
  cogito_node* n = cogito_node_from_val(nodev);
  if (!n) return;
  cogito_menu_set_submenu(n, ergo_as_bool(submenuv));
}

static void __cogito_menu_set_toggled(ErgoVal nodev, ErgoVal toggledv) {
  cogito_node* n = cogito_node_from_val(nodev);
  if (!n) return;
  cogito_menu_set_toggled(n, ergo_as_bool(toggledv));
}

// Icon button setters/getters
static void __cogito_iconbtn_set_shape(ErgoVal btnv, ErgoVal shapev) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_iconbtn_set_shape(btn, (int)ergo_as_int(shapev));
}

static ErgoVal __cogito_iconbtn_get_shape(ErgoVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_iconbtn_get_shape(btn));
}

static void __cogito_iconbtn_set_color_style(ErgoVal btnv, ErgoVal stylev) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_iconbtn_set_color_style(btn, (int)ergo_as_int(stylev));
}

static ErgoVal __cogito_iconbtn_get_color_style(ErgoVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_iconbtn_get_color_style(btn));
}

static void __cogito_iconbtn_set_width(ErgoVal btnv, ErgoVal widthv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_iconbtn_set_width(btn, (int)ergo_as_int(widthv));
}

static ErgoVal __cogito_iconbtn_get_width(ErgoVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_iconbtn_get_width(btn));
}

static void __cogito_iconbtn_set_toggle(ErgoVal btnv, ErgoVal togglev) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_iconbtn_set_toggle(btn, ergo_as_bool(togglev));
}

static ErgoVal __cogito_iconbtn_get_toggle(ErgoVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_BOOL(cogito_iconbtn_get_toggle(btn));
}

static void __cogito_iconbtn_set_checked(ErgoVal btnv, ErgoVal checkedv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_iconbtn_set_checked(btn, ergo_as_bool(checkedv));
}

static ErgoVal __cogito_iconbtn_get_checked(ErgoVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_BOOL(cogito_iconbtn_get_checked(btn));
}

static void __cogito_iconbtn_set_size(ErgoVal btnv, ErgoVal sizev) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_iconbtn_set_size(btn, (int)ergo_as_int(sizev));
}

static ErgoVal __cogito_iconbtn_get_size(ErgoVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_iconbtn_get_size(btn));
}

static void __cogito_iconbtn_set_menu_divider(ErgoVal btnv, ErgoVal dividerv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_menu_divider(btn, ergo_as_bool(dividerv));
}

static ErgoVal __cogito_iconbtn_get_menu_divider(ErgoVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_BOOL(cogito_button_get_menu_divider(btn));
}

static void __cogito_iconbtn_set_menu_item_gap(ErgoVal btnv, ErgoVal gapv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_menu_item_gap(btn, (int)ergo_as_int(gapv));
}

static ErgoVal __cogito_iconbtn_get_menu_item_gap(ErgoVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_button_get_menu_item_gap(btn));
}

static void __cogito_button_set_menu_divider(ErgoVal btnv, ErgoVal dividerv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_menu_divider(btn, ergo_as_bool(dividerv));
}

static ErgoVal __cogito_button_get_menu_divider(ErgoVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_BOOL(cogito_button_get_menu_divider(btn));
}

static void __cogito_button_set_menu_item_gap(ErgoVal btnv, ErgoVal gapv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_menu_item_gap(btn, (int)ergo_as_int(gapv));
}

static ErgoVal __cogito_button_get_menu_item_gap(ErgoVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_button_get_menu_item_gap(btn));
}

static void __cogito_iconbtn_set_menu_vibrant(ErgoVal btnv, ErgoVal vibrantv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_menu_vibrant(btn, ergo_as_bool(vibrantv));
}

static ErgoVal __cogito_iconbtn_get_menu_vibrant(ErgoVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_BOOL(cogito_button_get_menu_vibrant(btn));
}

static void __cogito_button_set_menu_vibrant(ErgoVal btnv, ErgoVal vibrantv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_menu_vibrant(btn, ergo_as_bool(vibrantv));
}

static ErgoVal __cogito_button_get_menu_vibrant(ErgoVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_BOOL(cogito_button_get_menu_vibrant(btn));
}

static void __cogito_iconbtn_on_click(ErgoVal btnv, ErgoVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  CogitoHandle* h = (CogitoHandle*)btnv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_node_on_click(btn, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_node_on_click(btn, cogito_cb_click, h);
}

static void __cogito_checkbox_set_checked(ErgoVal cbv, ErgoVal checkedv) {
  cogito_node* cb = cogito_node_from_val(cbv);
  cogito_checkbox_set_checked(cb, checkedv.tag == EVT_BOOL ? checkedv.as.b : false);
}

static ErgoVal __cogito_checkbox_get_checked(ErgoVal cbv) {
  cogito_node* cb = cogito_node_from_val(cbv);
  return YV_BOOL(cogito_checkbox_get_checked(cb));
}

static void __cogito_switch_set_checked(ErgoVal swv, ErgoVal checkedv) {
  cogito_node* sw = cogito_node_from_val(swv);
  cogito_switch_set_checked(sw, checkedv.tag == EVT_BOOL ? checkedv.as.b : false);
}

static ErgoVal __cogito_switch_get_checked(ErgoVal swv) {
  cogito_node* sw = cogito_node_from_val(swv);
  return YV_BOOL(cogito_switch_get_checked(sw));
}

static void __cogito_checkbox_on_change(ErgoVal cbv, ErgoVal handler) {
  cogito_node* cb = cogito_node_from_val(cbv);
  CogitoHandle* h = (CogitoHandle*)cbv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_checkbox_on_change(cb, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_checkbox_on_change(cb, cogito_cb_change, h);
}

static void __cogito_switch_on_change(ErgoVal swv, ErgoVal handler) {
  cogito_node* sw = cogito_node_from_val(swv);
  CogitoHandle* h = (CogitoHandle*)swv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_switch_on_change(sw, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_switch_on_change(sw, cogito_cb_change, h);
}

static void __cogito_textfield_set_text(ErgoVal tfv, ErgoVal textv) {
  cogito_node* tf = cogito_node_from_val(tfv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_textfield_set_text(tf, text);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static ErgoVal __cogito_textfield_get_text(ErgoVal tfv) {
  cogito_node* tf = cogito_node_from_val(tfv);
  const char* text = cogito_textfield_get_text(tf);
  return YV_STR(stdr_str_lit(text ? text : ""));
}

static void __cogito_textfield_set_hint(ErgoVal tfv, ErgoVal hintv) {
  cogito_node* tf = cogito_node_from_val(tfv);
  ErgoStr* tmp = NULL;
  const char* hint = cogito_required_cstr(hintv, &tmp);
  cogito_textfield_set_hint(tf, hint);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static ErgoVal __cogito_textfield_get_hint(ErgoVal tfv) {
  cogito_node* tf = cogito_node_from_val(tfv);
  const char* hint = cogito_textfield_get_hint(tf);
  return YV_STR(stdr_str_lit(hint ? hint : ""));
}

static void __cogito_textfield_on_change(ErgoVal tfv, ErgoVal handler) {
  cogito_node* tf = cogito_node_from_val(tfv);
  CogitoHandle* h = (CogitoHandle*)tfv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_textfield_on_change(tf, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_textfield_on_change(tf, cogito_cb_change, h);
}

static void __cogito_textview_set_text(ErgoVal tvv, ErgoVal textv) {
  cogito_node* tv = cogito_node_from_val(tvv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_textview_set_text(tv, text);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static ErgoVal __cogito_textview_get_text(ErgoVal tvv) {
  cogito_node* tv = cogito_node_from_val(tvv);
  const char* text = cogito_textview_get_text(tv);
  return YV_STR(stdr_str_lit(text ? text : ""));
}

static void __cogito_textview_on_change(ErgoVal tvv, ErgoVal handler) {
  cogito_node* tv = cogito_node_from_val(tvv);
  CogitoHandle* h = (CogitoHandle*)tvv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_textview_on_change(tv, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_textview_on_change(tv, cogito_cb_change, h);
}

static void __cogito_dropdown_set_items(ErgoVal ddv, ErgoVal itemsv) {
  cogito_node* dd = cogito_node_from_val(ddv);
  if (itemsv.tag != EVT_ARR) ergo_trap("cogito.dropdown_set_items expects array");
  ErgoArr* arr = (ErgoArr*)itemsv.as.p;
  size_t count = arr->len;
  const char** items = (const char**)calloc(count, sizeof(char*));
  ErgoStr** temps = (ErgoStr**)calloc(count, sizeof(ErgoStr*));
  for (size_t i = 0; i < count; i++) {
    items[i
    ] = cogito_required_cstr(arr->items[i
    ], &temps[i
    ]);
  }
  cogito_dropdown_set_items(dd, items, count);
  for (size_t i = 0; i < count; i++) {
    if (temps[i
    ]) ergo_release_val(YV_STR(temps[i
    ]));
  }
  free(temps);
  free(items);
}

static void __cogito_dropdown_set_selected(ErgoVal ddv, ErgoVal idxv) {
  cogito_node* dd = cogito_node_from_val(ddv);
  cogito_dropdown_set_selected(dd, (int)ergo_as_int(idxv));
}

static ErgoVal __cogito_dropdown_get_selected(ErgoVal ddv) {
  cogito_node* dd = cogito_node_from_val(ddv);
  return YV_INT(cogito_dropdown_get_selected(dd));
}

static void __cogito_dropdown_on_change(ErgoVal ddv, ErgoVal handler) {
  cogito_node* dd = cogito_node_from_val(ddv);
  CogitoHandle* h = (CogitoHandle*)ddv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_dropdown_on_change(dd, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_dropdown_on_change(dd, cogito_cb_change, h);
}

static void __cogito_slider_set_value(ErgoVal slv, ErgoVal valuev) {
  cogito_node* sl = cogito_node_from_val(slv);
  cogito_slider_set_value(sl, ergo_as_float(valuev));
}

static ErgoVal __cogito_slider_get_value(ErgoVal slv) {
  cogito_node* sl = cogito_node_from_val(slv);
  return YV_FLOAT(cogito_slider_get_value(sl));
}

static void __cogito_slider_set_size(ErgoVal slv, ErgoVal sizev) {
  cogito_node* sl = cogito_node_from_val(slv);
  cogito_slider_set_size(sl, (int)ergo_as_int(sizev));
}

static ErgoVal __cogito_slider_get_size(ErgoVal slv) {
  cogito_node* sl = cogito_node_from_val(slv);
  return YV_INT(cogito_slider_get_size(sl));
}

static void __cogito_slider_set_icon(ErgoVal slv, ErgoVal iconv) {
  cogito_node* sl = cogito_node_from_val(slv);
  if (iconv.tag == EVT_NULL) {
    cogito_slider_set_icon(sl, NULL);
    return;
  }
  if (iconv.tag == EVT_STR) {
    ErgoStr* s = (ErgoStr*)iconv.as.p;
    cogito_slider_set_icon(sl, s ? s->data : NULL);
    return;
  }
  ErgoStr* s = stdr_to_string(iconv);
  cogito_slider_set_icon(sl, s ? s->data : NULL);
  if (s) ergo_release_val(YV_STR(s));
}

static void __cogito_slider_set_centered(ErgoVal slv, ErgoVal onv) {
  cogito_node* sl = cogito_node_from_val(slv);
  cogito_slider_set_centered(sl, ergo_as_bool(onv));
}

static ErgoVal __cogito_slider_get_centered(ErgoVal slv) {
  cogito_node* sl = cogito_node_from_val(slv);
  return YV_BOOL(cogito_slider_get_centered(sl));
}

static void __cogito_slider_set_range(ErgoVal slv, ErgoVal startv, ErgoVal endv) {
  cogito_node* sl = cogito_node_from_val(slv);
  cogito_slider_set_range(sl, ergo_as_float(startv), ergo_as_float(endv));
}

static void __cogito_slider_set_range_start(ErgoVal slv, ErgoVal startv) {
  cogito_node* sl = cogito_node_from_val(slv);
  cogito_slider_set_range_start(sl, ergo_as_float(startv));
}

static void __cogito_slider_set_range_end(ErgoVal slv, ErgoVal endv) {
  cogito_node* sl = cogito_node_from_val(slv);
  cogito_slider_set_range_end(sl, ergo_as_float(endv));
}

static ErgoVal __cogito_slider_get_range_start(ErgoVal slv) {
  cogito_node* sl = cogito_node_from_val(slv);
  return YV_FLOAT(cogito_slider_get_range_start(sl));
}

static ErgoVal __cogito_slider_get_range_end(ErgoVal slv) {
  cogito_node* sl = cogito_node_from_val(slv);
  return YV_FLOAT(cogito_slider_get_range_end(sl));
}

static void __cogito_slider_on_change(ErgoVal slv, ErgoVal handler) {
  cogito_node* sl = cogito_node_from_val(slv);
  CogitoHandle* h = (CogitoHandle*)slv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_slider_on_change(sl, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_slider_on_change(sl, cogito_cb_change, h);
}

static void __cogito_stepper_set_value(ErgoVal stv, ErgoVal valuev) {
  cogito_node* st = cogito_node_from_val(stv);
  cogito_stepper_set_value(st, ergo_as_float(valuev));
}

static ErgoVal __cogito_stepper_get_value(ErgoVal stv) {
  cogito_node* st = cogito_node_from_val(stv);
  return YV_FLOAT(cogito_stepper_get_value(st));
}

static void __cogito_stepper_on_change(ErgoVal stv, ErgoVal handler) {
  cogito_node* st = cogito_node_from_val(stv);
  CogitoHandle* h = (CogitoHandle*)stv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_stepper_on_change(st, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_stepper_on_change(st, cogito_cb_change, h);
}

static void __cogito_buttongroup_on_select(ErgoVal segv, ErgoVal handler) {
  cogito_node* seg = cogito_node_from_val(segv);
  CogitoHandle* h = (CogitoHandle*)segv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_buttongroup_on_select(seg, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_buttongroup_on_select(seg, cogito_cb_select, h);
}

static void __cogito_buttongroup_set_size(ErgoVal bgv, ErgoVal sizev) {
  cogito_node* bg = cogito_node_from_val(bgv);
  int size = (int)ergo_as_int(sizev);
  if (size < 0) size = 0;
  if (size > 4) size = 4;
  cogito_buttongroup_set_size(bg, size);
}

static ErgoVal __cogito_buttongroup_get_size(ErgoVal bgv) {
  cogito_node* bg = cogito_node_from_val(bgv);
  return YV_INT(cogito_buttongroup_get_size(bg));
}

static void __cogito_buttongroup_set_shape(ErgoVal bgv, ErgoVal shapev) {
  cogito_node* bg = cogito_node_from_val(bgv);
  int shape = (int)ergo_as_int(shapev);
  if (shape < 0) shape = 0;
  if (shape > 1) shape = 1;
  cogito_buttongroup_set_shape(bg, shape);
}

static ErgoVal __cogito_buttongroup_get_shape(ErgoVal bgv) {
  cogito_node* bg = cogito_node_from_val(bgv);
  return YV_INT(cogito_buttongroup_get_shape(bg));
}

static void __cogito_buttongroup_set_connected(ErgoVal bgv, ErgoVal connectedv) {
  cogito_node* bg = cogito_node_from_val(bgv);
  cogito_buttongroup_set_connected(bg, ergo_as_bool(connectedv));
}

static ErgoVal __cogito_buttongroup_get_connected(ErgoVal bgv) {
  cogito_node* bg = cogito_node_from_val(bgv);
  return YV_BOOL(cogito_buttongroup_get_connected(bg));
}

static void __cogito_tabs_set_items(ErgoVal tabsv, ErgoVal itemsv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  if (itemsv.tag != EVT_ARR) ergo_trap("cogito.tabs_set_items expects array");
  ErgoArr* arr = (ErgoArr*)itemsv.as.p;
  size_t count = arr->len;
  const char** items = (const char**)calloc(count, sizeof(char*));
  ErgoStr** temps = (ErgoStr**)calloc(count, sizeof(ErgoStr*));
  for (size_t i = 0; i < count; i++) {
    items[i
    ] = cogito_required_cstr(arr->items[i
    ], &temps[i
    ]);
  }
  cogito_tabs_set_items(tabs, items, count);
  for (size_t i = 0; i < count; i++) {
    if (temps[i
    ]) ergo_release_val(YV_STR(temps[i
    ]));
  }
  free(temps);
  free(items);
}

static void __cogito_tabs_set_ids(ErgoVal tabsv, ErgoVal itemsv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  if (itemsv.tag != EVT_ARR) ergo_trap("cogito.tabs_set_ids expects array");
  ErgoArr* arr = (ErgoArr*)itemsv.as.p;
  size_t count = arr->len;
  const char** items = (const char**)calloc(count, sizeof(char*));
  ErgoStr** temps = (ErgoStr**)calloc(count, sizeof(ErgoStr*));
  for (size_t i = 0; i < count; i++) {
    items[i
    ] = cogito_required_cstr(arr->items[i
    ], &temps[i
    ]);
  }
  cogito_tabs_set_ids(tabs, items, count);
  for (size_t i = 0; i < count; i++) {
    if (temps[i
    ]) ergo_release_val(YV_STR(temps[i
    ]));
  }
  free(temps);
  free(items);
}

static void __cogito_tabs_set_selected(ErgoVal tabsv, ErgoVal idxv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  cogito_tabs_set_selected(tabs, (int)ergo_as_int(idxv));
}

static ErgoVal __cogito_tabs_get_selected(ErgoVal tabsv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  return YV_INT(cogito_tabs_get_selected(tabs));
}

static void __cogito_tabs_on_change(ErgoVal tabsv, ErgoVal handler) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  CogitoHandle* h = (CogitoHandle*)tabsv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_tabs_on_change(tabs, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_tabs_on_change(tabs, cogito_cb_change, h);
}

static void __cogito_tabs_bind(ErgoVal tabsv, ErgoVal viewv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  cogito_node* view = cogito_node_from_val(viewv);
  cogito_tabs_bind(tabs, view);
}

static void __cogito_nav_rail_set_items(ErgoVal railv, ErgoVal labelsv, ErgoVal iconsv) {
  cogito_node* rail = cogito_node_from_val(railv);
  if (labelsv.tag != EVT_ARR) ergo_trap("cogito.nav_rail_set_items expects array of labels");
  ErgoArr* labels = (ErgoArr*)labelsv.as.p;
  size_t count = labels->len;

  const char** label_strs = (const char**)calloc(count, sizeof(char*));
  ErgoStr** label_temps = (ErgoStr**)calloc(count, sizeof(ErgoStr*));
  for (size_t i = 0; i < count; i++) {
    label_strs[i
    ] = cogito_required_cstr(labels->items[i
    ], &label_temps[i
    ]);
  }

  const char** icon_strs = NULL;
  ErgoStr** icon_temps = NULL;
  if (iconsv.tag == EVT_ARR) {
    ErgoArr* icons = (ErgoArr*)iconsv.as.p;
    size_t icon_count = icons->len;
    icon_strs = (const char**)calloc(icon_count, sizeof(char*));
    icon_temps = (ErgoStr**)calloc(icon_count, sizeof(ErgoStr*));
    for (size_t i = 0; i < icon_count; i++) {
      icon_strs[i
      ] = cogito_required_cstr(icons->items[i
      ], &icon_temps[i
      ]);
    }
    cogito_nav_rail_set_items(rail, label_strs, icon_strs, icon_count < count ? icon_count : count);
    for (size_t i = 0; i < icon_count; i++) {
      if (icon_temps[i
      ]) ergo_release_val(YV_STR(icon_temps[i
      ]));
    }
    free(icon_temps);
    free(icon_strs);
  } else {
    cogito_nav_rail_set_items(rail, label_strs, NULL, count);
  }

  for (size_t i = 0; i < count; i++) {
    if (label_temps[i
    ]) ergo_release_val(YV_STR(label_temps[i
    ]));
  }
  free(label_temps);
  free(label_strs);
}

static void __cogito_nav_rail_set_badges(ErgoVal railv, ErgoVal badgesv) {
  cogito_node* rail = cogito_node_from_val(railv);
  if (badgesv.tag == EVT_NULL) {
    cogito_nav_rail_set_badges(rail, NULL,
    0);
    return;
  }
  if (badgesv.tag != EVT_ARR) ergo_trap("cogito.nav_rail_set_badges expects array");
  ErgoArr* badges = (ErgoArr*)badgesv.as.p;
  size_t count = badges->len;
  int* vals = (int*)calloc(count, sizeof(int));
  for (size_t i = 0; i < count; i++) {
    ErgoVal bv = badges->items[i
    ];
    int v = 0;
    switch (bv.tag) {
      case EVT_NULL:
        v = 0;
        break;
      case EVT_BOOL:
        v = bv.as.b ? -1 : 0;
        break;
      case EVT_INT:
        v = (int)bv.as.i;
        if (v < 0) v = -1;
        break;
      case EVT_FLOAT: {
        int rounded = (int)(bv.as.f >= 0.0 ? bv.as.f + 0.5 : bv.as.f - 0.5);
        v = rounded < 0 ? -1 : rounded;
        break;
      }
      default:
        free(vals);
        ergo_trap("cogito.nav_rail_set_badges expects int/bool/null entries");
    }
    vals[i] = v;
  }
  cogito_nav_rail_set_badges(rail, vals, count);
  free(vals);
}

static void __cogito_nav_rail_set_toggle(ErgoVal railv, ErgoVal onv) {
  cogito_node* rail = cogito_node_from_val(railv);
  cogito_nav_rail_set_toggle(rail, ergo_as_bool(onv));
}

static void __cogito_nav_rail_set_selected(ErgoVal railv, ErgoVal idxv) {
  cogito_node* rail = cogito_node_from_val(railv);
  cogito_nav_rail_set_selected(rail, (int)ergo_as_int(idxv));
}

static ErgoVal __cogito_nav_rail_get_selected(ErgoVal railv) {
  cogito_node* rail = cogito_node_from_val(railv);
  return YV_INT(cogito_nav_rail_get_selected(rail));
}

static ErgoVal __cogito_bottom_nav(void) {
  cogito_node* n = cogito_bottom_nav_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_nav_rail_on_change(ErgoVal railv, ErgoVal handler) {
  cogito_node* rail = cogito_node_from_val(railv);
  CogitoHandle* h = (CogitoHandle*)railv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_nav_rail_on_change(rail, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_nav_rail_on_change(rail, cogito_cb_select, h);
}

static void __cogito_bottom_nav_set_items(ErgoVal navv, ErgoVal labelsv, ErgoVal iconsv) {
  cogito_node* nav = cogito_node_from_val(navv);
  if (labelsv.tag != EVT_ARR) ergo_trap("cogito.bottom_nav_set_items expects array of labels");
  ErgoArr* labels = (ErgoArr*)labelsv.as.p;
  size_t count = labels->len;
  const char** label_strs = (const char**)calloc(count, sizeof(char*));
  ErgoStr** label_temps = (ErgoStr**)calloc(count, sizeof(ErgoStr*));
  for (size_t i = 0; i < count; i++) {
    label_strs[i] = cogito_required_cstr(labels->items[i], &label_temps[i]);
  }
  const char** icon_strs = NULL;
  ErgoStr** icon_temps = NULL;
  if (iconsv.tag == EVT_ARR) {
    ErgoArr* icons = (ErgoArr*)iconsv.as.p;
    size_t icon_count = icons->len;
    icon_strs = (const char**)calloc(icon_count, sizeof(char*));
    icon_temps = (ErgoStr**)calloc(icon_count, sizeof(ErgoStr*));
    for (size_t i = 0; i < icon_count; i++) {
      icon_strs[i] = cogito_required_cstr(icons->items[i], &icon_temps[i]);
    }
    cogito_bottom_nav_set_items(nav, label_strs, icon_strs, icon_count < count ? icon_count : count);
    for (size_t i = 0; i < icon_count; i++) {
      if (icon_temps[i]) ergo_release_val(YV_STR(icon_temps[i]));
    }
    free(icon_temps);
    free(icon_strs);
  } else {
    cogito_bottom_nav_set_items(nav, label_strs, NULL, count);
  }
  for (size_t i = 0; i < count; i++) {
    if (label_temps[i]) ergo_release_val(YV_STR(label_temps[i]));
  }
  free(label_temps);
  free(label_strs);
}

static void __cogito_bottom_nav_set_selected(ErgoVal navv, ErgoVal idxv) {
  cogito_node* nav = cogito_node_from_val(navv);
  cogito_bottom_nav_set_selected(nav, (int)ergo_as_int(idxv));
}

static ErgoVal __cogito_bottom_nav_get_selected(ErgoVal navv) {
  cogito_node* nav = cogito_node_from_val(navv);
  return YV_INT(cogito_bottom_nav_get_selected(nav));
}

static void __cogito_bottom_nav_on_change(ErgoVal navv, ErgoVal handler) {
  cogito_node* nav = cogito_node_from_val(navv);
  CogitoHandle* h = (CogitoHandle*)navv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_bottom_nav_on_change(nav, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_bottom_nav_on_change(nav, cogito_cb_select, h);
}

static void __cogito_view_switcher_set_active(ErgoVal viewv, ErgoVal idv) {
  cogito_node* view = cogito_node_from_val(viewv);
  ErgoStr* tmp = NULL;
  const char* id = cogito_required_cstr(idv, &tmp);
  cogito_view_switcher_set_active(view, id);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_view_switcher_add(ErgoVal vsv, ErgoVal idv, ErgoVal builderv) {
  cogito_node* nvs = cogito_node_from_val(vsv);
  cogito_view_switcher_add_lazy_ergo(nvs, idv, builderv);
}

static void __cogito_view_switcher_add_lazy(ErgoVal vsv, ErgoVal idv, ErgoVal builderv) {
  __cogito_view_switcher_add(vsv, idv, builderv);
}

static void __cogito_progress_set_value(ErgoVal pv, ErgoVal valuev) {
  cogito_node* p = cogito_node_from_val(pv);
  cogito_progress_set_value(p, ergo_as_float(valuev));
}

static ErgoVal __cogito_progress_get_value(ErgoVal pv) {
  cogito_node* p = cogito_node_from_val(pv);
  return YV_FLOAT(cogito_progress_get_value(p));
}

static void __cogito_progress_set_indeterminate(ErgoVal pv, ErgoVal onv) {
  cogito_node* p = cogito_node_from_val(pv);
  cogito_progress_set_indeterminate(p, ergo_as_bool(onv));
}

static ErgoVal __cogito_progress_get_indeterminate(ErgoVal pv) {
  cogito_node* p = cogito_node_from_val(pv);
  return YV_BOOL(cogito_progress_get_indeterminate(p));
}

static void __cogito_progress_set_thickness(ErgoVal pv, ErgoVal pxv) {
  cogito_node* p = cogito_node_from_val(pv);
  cogito_progress_set_thickness(p, (int)ergo_as_int(pxv));
}

static ErgoVal __cogito_progress_get_thickness(ErgoVal pv) {
  cogito_node* p = cogito_node_from_val(pv);
  return YV_INT(cogito_progress_get_thickness(p));
}

static void __cogito_progress_set_wavy(ErgoVal pv, ErgoVal onv) {
  cogito_node* p = cogito_node_from_val(pv);
  cogito_progress_set_wavy(p, ergo_as_bool(onv));
}

static ErgoVal __cogito_progress_get_wavy(ErgoVal pv) {
  cogito_node* p = cogito_node_from_val(pv);
  return YV_BOOL(cogito_progress_get_wavy(p));
}

static void __cogito_progress_set_circular(ErgoVal pv, ErgoVal onv) {
  cogito_node* p = cogito_node_from_val(pv);
  cogito_progress_set_circular(p, ergo_as_bool(onv));
}

static ErgoVal __cogito_progress_get_circular(ErgoVal pv) {
  cogito_node* p = cogito_node_from_val(pv);
  return YV_BOOL(cogito_progress_get_circular(p));
}

static void __cogito_toast_set_text(ErgoVal tv, ErgoVal textv) {
  cogito_node* t = cogito_node_from_val(tv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_toast_set_text(t, text);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_toast_on_click(ErgoVal tv, ErgoVal handler) {
  cogito_node* t = cogito_node_from_val(tv);
  CogitoHandle* h = (CogitoHandle*)tv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_toast_on_click(t, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_toast_on_click(t, cogito_cb_click, h);
}

static void __cogito_toast_set_action(ErgoVal tv, ErgoVal textv, ErgoVal handler) {
  cogito_node* t = cogito_node_from_val(tv);
  CogitoHandle* h = (CogitoHandle*)tv.as.p;
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handler);
    cogito_toast_set_action(t, text, NULL, NULL);
    if (tmp) ergo_release_val(YV_STR(tmp));
    return;
  }
  cogito_set_handler(h, &h->on_action, handler);
  cogito_toast_set_action(t, text, cogito_cb_action, h);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_list_on_select(ErgoVal listv, ErgoVal handler) {
  cogito_node* list = cogito_node_from_val(listv);
  CogitoHandle* h = (CogitoHandle*)listv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_list_on_select(list, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_list_on_select(list, cogito_cb_select, h);
}

static void __cogito_list_on_activate(ErgoVal listv, ErgoVal handler) {
  cogito_node* list = cogito_node_from_val(listv);
  CogitoHandle* h = (CogitoHandle*)listv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_activate, handler);
    cogito_list_on_activate(list, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_activate, handler);
  cogito_list_on_activate(list, cogito_cb_activate, h);
}

static void __cogito_grid_on_select(ErgoVal gridv, ErgoVal handler) {
  cogito_node* grid = cogito_node_from_val(gridv);
  CogitoHandle* h = (CogitoHandle*)gridv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_grid_on_select(grid, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_grid_on_select(grid, cogito_cb_select, h);
}

static void __cogito_grid_on_activate(ErgoVal gridv, ErgoVal handler) {
  cogito_node* grid = cogito_node_from_val(gridv);
  CogitoHandle* h = (CogitoHandle*)gridv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_activate, handler);
    cogito_grid_on_activate(grid, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_activate, handler);
  cogito_grid_on_activate(grid, cogito_cb_activate, h);
}

static void __cogito_build(ErgoVal nodev, ErgoVal builder) {
  if (builder.tag != EVT_FN) ergo_trap("cogito.build expects function");
  ErgoVal arg = nodev;
  ergo_retain_val(arg);
  ErgoVal ret = ergo_call(builder, 1, &arg);
  ergo_release_val(arg);
  ergo_release_val(ret);
}

static ErgoVal __cogito_state_new(ErgoVal initial) { return cogito_state_new_val(initial);
}
static ErgoVal __cogito_state_get(ErgoVal state) { return cogito_state_get_val(state);
}
static void __cogito_state_set(ErgoVal state, ErgoVal value) { cogito_state_set_val(state, value);
}

static void __cogito_timer_cb_call_ergo(void* user) {
  ErgoFn* fn = (ErgoFn*)user;
  if (!fn) return;
  ErgoVal ret = ergo_call(YV_FN(fn), 0, NULL);
  ergo_release_val(ret);
}

static void __cogito_timer_cb_release_ergo(void* user) {
  ErgoFn* fn = (ErgoFn*)user;
  if (!fn) return;
  ergo_release_val(YV_FN(fn));
}

static ErgoVal __cogito_timer_timeout(ErgoVal delayv, ErgoVal handlerv) {
  if (handlerv.tag != EVT_FN) ergo_trap("cogito.set_timeout expects function");
  int64_t delay = ergo_as_int(delayv);
  if (delay < 0) delay = 0;
  ErgoFn* fn = (ErgoFn*)handlerv.as.p;
  ergo_retain_val(YV_FN(fn));
  uint64_t id = cogito_timer_set_timeout_ex((uint32_t)delay,
                                            __cogito_timer_cb_call_ergo, fn,
                                            __cogito_timer_cb_release_ergo);
  if (id == 0) ergo_release_val(YV_FN(fn));
  return YV_INT((int64_t)id);
}

static ErgoVal __cogito_timer_interval(ErgoVal delayv, ErgoVal handlerv) {
  if (handlerv.tag != EVT_FN) ergo_trap("cogito.set_interval expects function");
  int64_t delay = ergo_as_int(delayv);
  if (delay < 0) delay = 0;
  ErgoFn* fn = (ErgoFn*)handlerv.as.p;
  ergo_retain_val(YV_FN(fn));
  uint64_t id = cogito_timer_set_interval_ex((uint32_t)delay,
                                             __cogito_timer_cb_call_ergo, fn,
                                             __cogito_timer_cb_release_ergo);
  if (id == 0) ergo_release_val(YV_FN(fn));
  return YV_INT((int64_t)id);
}

static ErgoVal __cogito_timer_timeout_for(ErgoVal ownerv, ErgoVal delayv, ErgoVal handlerv) {
  if (handlerv.tag != EVT_FN) ergo_trap("cogito.set_timeout_for expects function");
  cogito_node* owner = cogito_node_from_val(ownerv);
  int64_t delay = ergo_as_int(delayv);
  if (delay < 0) delay = 0;
  ErgoFn* fn = (ErgoFn*)handlerv.as.p;
  ergo_retain_val(YV_FN(fn));
  uint64_t id = cogito_timer_set_timeout_for_ex(
      owner, (uint32_t)delay, __cogito_timer_cb_call_ergo, fn,
      __cogito_timer_cb_release_ergo);
  if (id == 0) ergo_release_val(YV_FN(fn));
  return YV_INT((int64_t)id);
}

static ErgoVal __cogito_timer_interval_for(ErgoVal ownerv, ErgoVal delayv, ErgoVal handlerv) {
  if (handlerv.tag != EVT_FN) ergo_trap("cogito.set_interval_for expects function");
  cogito_node* owner = cogito_node_from_val(ownerv);
  int64_t delay = ergo_as_int(delayv);
  if (delay < 0) delay = 0;
  ErgoFn* fn = (ErgoFn*)handlerv.as.p;
  ergo_retain_val(YV_FN(fn));
  uint64_t id = cogito_timer_set_interval_for_ex(
      owner, (uint32_t)delay, __cogito_timer_cb_call_ergo, fn,
      __cogito_timer_cb_release_ergo);
  if (id == 0) ergo_release_val(YV_FN(fn));
  return YV_INT((int64_t)id);
}

static ErgoVal __cogito_timer_cancel(ErgoVal timer_id_v) {
  cogito_timer_id id = (cogito_timer_id)ergo_as_int(timer_id_v);
  bool ok = cogito_timer_clear(id);
  return YV_BOOL(ok);
}

static void __cogito_timer_cancel_for(ErgoVal ownerv) {
  cogito_node* owner = cogito_node_from_val(ownerv);
  cogito_timer_clear_for(owner);
}

static void __cogito_timer_cancel_all(void) {
  cogito_timer_clear_all();
}

static void __cogito_run(ErgoVal appv, ErgoVal winv) {
  cogito_app* app = cogito_app_from_val(appv);
  cogito_window* win = cogito_window_from_val(winv);
  cogito_app_run(app, win);
}

static void __cogito_set_ensor_variant(ErgoVal appv, ErgoVal variantv) {
  cogito_app* app = cogito_app_from_val(appv);
  if (app) cogito_app_set_ensor_variant(app, (int)ergo_as_int(variantv));
}

static void __cogito_load_sum(ErgoVal pathv) {
  ErgoStr* tmp = NULL;
  const char* path = cogito_required_cstr(pathv, &tmp);
  cogito_load_sum_file(path);
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static void __cogito_set_script_dir(ErgoVal dirv) {
  ErgoStr* tmp = NULL;
  const char* dir = cogito_optional_cstr(dirv, &tmp);
  if (dir && dir[0]) cogito_set_script_dir(dir); // Call the actual cogito library function
  if (tmp) ergo_release_val(YV_STR(tmp));
}

static ErgoVal __cogito_open_url(ErgoVal urlv) {
  ErgoStr* tmp = NULL;
  const char* url = cogito_optional_cstr(urlv, &tmp);
  bool ok = false;
  if (url && url[0]) ok = cogito_open_url(url);
  if (tmp) ergo_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}
// ---- Codegen aliases ----
#define cogito_app_new __cogito_app
#define cogito_app_set_appid __cogito_app_set_appid
#define cogito_app_set_app_name __cogito_app_set_app_name
#define cogito_app_set_accent_color __cogito_app_set_accent_color
#define cogito_app_set_dark_mode __cogito_app_set_dark_mode
#define cogito_app_set_accent_from_image __cogito_app_set_accent_from_image
#define cogito_app_set_contrast __cogito_app_set_contrast
#define cogito_app_set_ensor_variant __cogito_app_set_ensor_variant
#define cogito_app_set_icon __cogito_app_set_icon
#define cogito_app_get_icon __cogito_app_get_icon
#define cogito_open_url __cogito_open_url
#define cogito_window_new __cogito_window
#define cogito_window_set_resizable __cogito_window_set_resizable
#define cogito_window_set_autosize __cogito_window_set_autosize
#define cogito_window_set_a11y_label __cogito_window_set_a11y_label
#define cogito_window_set_builder __cogito_window_set_builder
#define cogito_button_new __cogito_button
#define cogito_iconbtn_new __cogito_iconbtn
#define cogito_fab_new __cogito_fab
#define cogito_label_new __cogito_label
#define cogito_dialog_new __cogito_dialog
#define cogito_dialog_slot_new __cogito_dialog_slot
#define cogito_image_new __cogito_image
#define cogito_checkbox_new __cogito_checkbox
#define cogito_switch_new __cogito_switch
#define cogito_textfield_new __cogito_textfield
#define cogito_searchfield_new __cogito_searchfield
#define cogito_textview_new __cogito_textview
#define cogito_dropdown_new __cogito_dropdown
#define cogito_datepicker_new __cogito_datepicker
#define cogito_stepper_new __cogito_stepper
#define cogito_slider_new __cogito_slider
#define cogito_slider_range_new __cogito_slider_range
#define cogito_tabs_new __cogito_tabs
#define cogito_nav_rail_new __cogito_nav_rail
#define cogito_bottom_nav_new __cogito_bottom_nav
#define cogito_buttongroup_new __cogito_buttongroup
#define cogito_view_switcher_new __cogito_view_switcher
#define cogito_progress_new __cogito_progress
#define cogito_treeview_new __cogito_treeview
#define cogito_colorpicker_new __cogito_colorpicker
#define cogito_colorpicker_set_hex __cogito_colorpicker_set_hex
#define cogito_colorpicker_get_hex __cogito_colorpicker_get_hex
#define cogito_toasts_new __cogito_toasts
#define cogito_toast_new __cogito_toast
#define cogito_appbar_new __cogito_appbar
#define cogito_toolbar_new __cogito_toolbar
#define cogito_vstack_new __cogito_vstack
#define cogito_hstack_new __cogito_hstack
#define cogito_zstack_new __cogito_zstack
#define cogito_fixed_new __cogito_fixed
#define cogito_scroller_new __cogito_scroller
#define cogito_carousel_new __cogito_carousel
#define cogito_carousel_item_new __cogito_carousel_item
#define cogito_carousel_item_set_text __cogito_carousel_item_set_text
#define cogito_carousel_item_set_halign __cogito_carousel_item_set_halign
#define cogito_carousel_item_set_valign __cogito_carousel_item_set_valign
#define cogito_carousel_set_active_index __cogito_carousel_set_active_index
#define cogito_carousel_get_active_index __cogito_carousel_get_active_index
#define cogito_list_new __cogito_list
#define cogito_grid_new __cogito_grid
#define cogito_container_add __cogito_container_add
#define cogito_container_set_margins __cogito_container_set_margins
#define cogito_container_set_align __cogito_container_set_align
#define cogito_container_set_halign __cogito_container_set_halign
#define cogito_container_set_valign __cogito_container_set_valign
#define cogito_container_set_hexpand __cogito_container_set_hexpand
#define cogito_container_set_vexpand __cogito_container_set_vexpand
#define cogito_container_set_gap __cogito_container_set_gap
#define cogito_dialog_slot_show __cogito_dialog_slot_show
#define cogito_dialog_slot_clear __cogito_dialog_slot_clear
#define cogito_container_set_padding __cogito_container_set_padding
#define cogito_fixed_set_pos __cogito_fixed_set_pos
#define cogito_scroller_set_axes __cogito_scroller_set_axes
#define cogito_grid_set_gap __cogito_grid_set_gap
#define cogito_grid_set_span __cogito_grid_set_span
#define cogito_grid_set_align __cogito_grid_set_align
#define cogito_label_set_class __cogito_label_set_class
#define cogito_label_set_text __cogito_label_set_text
#define cogito_label_set_wrap __cogito_label_set_wrap
#define cogito_label_set_ellipsis __cogito_label_set_ellipsis
#define cogito_label_set_align __cogito_label_set_align
#define cogito_node_set_disabled __cogito_node_set_disabled
#define cogito_node_set_editable __cogito_node_set_editable
#define cogito_node_get_editable __cogito_node_get_editable
#define cogito_node_set_id __cogito_node_set_id
#define cogito_node_set_class __cogito_node_set_class
#define cogito_node_set_a11y_label __cogito_node_set_a11y_label
#define cogito_node_set_a11y_role __cogito_node_set_a11y_role
#define cogito_node_set_tooltip_val __cogito_node_set_tooltip
#define cogito_app_set_appid __cogito_app_set_appid
#define cogito_app_set_app_name __cogito_app_set_app_name
#define cogito_app_set_accent_color __cogito_app_set_accent_color
#define cogito_app_set_dark_mode __cogito_app_set_dark_mode
#define cogito_app_set_accent_from_image __cogito_app_set_accent_from_image
#define cogito_app_set_contrast __cogito_app_set_contrast
#define cogito_app_set_ensor_variant __cogito_app_set_ensor_variant
#define cogito_app_set_icon __cogito_app_set_icon
#define cogito_app_get_icon __cogito_app_get_icon
#define cogito_pointer_capture_set __cogito_pointer_capture
#define cogito_pointer_capture_clear __cogito_pointer_release
#define cogito_view_switcher_set_active __cogito_view_switcher_set_active
#define cogito_view_switcher_add __cogito_view_switcher_add
#define cogito_textfield_set_text __cogito_textfield_set_text
#define cogito_textfield_get_text __cogito_textfield_get_text
#define cogito_textfield_set_hint __cogito_textfield_set_hint
#define cogito_textfield_get_hint __cogito_textfield_get_hint
#define cogito_searchfield_set_text __cogito_searchfield_set_text
#define cogito_searchfield_get_text __cogito_searchfield_get_text
#define cogito_searchfield_on_change __cogito_searchfield_on_change
#define cogito_textfield_on_change __cogito_textfield_on_change
#define cogito_textview_set_text __cogito_textview_set_text
#define cogito_textview_get_text __cogito_textview_get_text
#define cogito_textview_on_change __cogito_textview_on_change
#define cogito_datepicker_on_change __cogito_datepicker_on_change
#define cogito_dropdown_set_items __cogito_dropdown_set_items
#define cogito_dropdown_set_selected __cogito_dropdown_set_selected
#define cogito_dropdown_get_selected __cogito_dropdown_get_selected
#define cogito_dropdown_on_change __cogito_dropdown_on_change
#define cogito_slider_set_value __cogito_slider_set_value
#define cogito_slider_get_value __cogito_slider_get_value
#define cogito_slider_set_size __cogito_slider_set_size
#define cogito_slider_get_size __cogito_slider_get_size
#define cogito_slider_set_icon __cogito_slider_set_icon
#define cogito_slider_set_centered __cogito_slider_set_centered
#define cogito_slider_get_centered __cogito_slider_get_centered
#define cogito_slider_set_range __cogito_slider_set_range
#define cogito_slider_set_range_start __cogito_slider_set_range_start
#define cogito_slider_set_range_end __cogito_slider_set_range_end
#define cogito_slider_get_range_start __cogito_slider_get_range_start
#define cogito_slider_get_range_end __cogito_slider_get_range_end
#define cogito_slider_on_change __cogito_slider_on_change
#define cogito_colorpicker_on_change __cogito_colorpicker_on_change
#define cogito_tabs_set_items __cogito_tabs_set_items
#define cogito_tabs_set_ids __cogito_tabs_set_ids
#define cogito_tabs_set_selected __cogito_tabs_set_selected
#define cogito_tabs_get_selected __cogito_tabs_get_selected
#define cogito_tabs_on_change __cogito_tabs_on_change
#define cogito_tabs_bind __cogito_tabs_bind
#define cogito_nav_rail_set_items __cogito_nav_rail_set_items
#define cogito_nav_rail_set_badges __cogito_nav_rail_set_badges
#define cogito_nav_rail_set_toggle __cogito_nav_rail_set_toggle
#define cogito_nav_rail_set_selected __cogito_nav_rail_set_selected
#define cogito_nav_rail_get_selected __cogito_nav_rail_get_selected
#define cogito_nav_rail_on_change __cogito_nav_rail_on_change
#define cogito_nav_rail_set_no_label __cogito_nav_rail_set_no_label
#define cogito_nav_rail_get_no_label __cogito_nav_rail_get_no_label
#define cogito_bottom_nav_set_items __cogito_bottom_nav_set_items
#define cogito_bottom_nav_set_selected __cogito_bottom_nav_set_selected
#define cogito_bottom_nav_get_selected __cogito_bottom_nav_get_selected
#define cogito_bottom_nav_on_change __cogito_bottom_nav_on_change
#define cogito_progress_set_value __cogito_progress_set_value
#define cogito_progress_get_value __cogito_progress_get_value
#define cogito_progress_set_indeterminate __cogito_progress_set_indeterminate
#define cogito_progress_get_indeterminate __cogito_progress_get_indeterminate
#define cogito_progress_set_thickness __cogito_progress_set_thickness
#define cogito_progress_get_thickness __cogito_progress_get_thickness
#define cogito_progress_set_wavy __cogito_progress_set_wavy
#define cogito_progress_get_wavy __cogito_progress_get_wavy
#define cogito_progress_set_circular __cogito_progress_set_circular
#define cogito_progress_get_circular __cogito_progress_get_circular
#define cogito_toast_set_text __cogito_toast_set_text
#define cogito_toast_on_click __cogito_toast_on_click
#define cogito_toast_set_action __cogito_toast_set_action
#define cogito_window_set_autosize __cogito_window_set_autosize
#define cogito_window_set_resizable __cogito_window_set_resizable
#define cogito_window_set_dialog __cogito_window_set_dialog
#define cogito_window_clear_dialog __cogito_window_clear_dialog
#define cogito_window_set_side_sheet __cogito_window_set_side_sheet
#define cogito_window_clear_side_sheet __cogito_window_clear_side_sheet
#define cogito_side_sheet_set_mode __cogito_side_sheet_set_mode
#define cogito_node_window_val __cogito_node_window
#define cogito_find_parent __cogito_find_parent
#define cogito_find_children __cogito_find_children
#define cogito_build __cogito_build
#define cogito_window_set_builder __cogito_window_set_builder
#define cogito_state_new __cogito_state_new
#define cogito_state_get __cogito_state_get
#define cogito_state_set __cogito_state_set
#define cogito_button_set_text __cogito_button_set_text
#define cogito_button_set_size __cogito_button_set_size
#define cogito_button_get_size __cogito_button_get_size
#define cogito_image_set_icon __cogito_image_set_icon
#define cogito_image_set_size __cogito_image_set_size
#define cogito_image_set_radius __cogito_image_set_radius
#define cogito_image_set_alt_text __cogito_image_set_alt_text
#define cogito_checkbox_set_checked __cogito_checkbox_set_checked
#define cogito_checkbox_get_checked __cogito_checkbox_get_checked
#define cogito_switch_set_checked __cogito_switch_set_checked
#define cogito_switch_get_checked __cogito_switch_get_checked
#define cogito_checkbox_on_change __cogito_checkbox_on_change
#define cogito_switch_on_change __cogito_switch_on_change
#define cogito_list_on_select __cogito_list_on_select
#define cogito_list_on_activate __cogito_list_on_activate
#define cogito_grid_on_select __cogito_grid_on_select
#define cogito_grid_on_activate __cogito_grid_on_activate
void __cogito_button_on_click(ErgoVal btn, ErgoVal handler);
#define cogito_button_on_click __cogito_button_on_click
#define cogito_button_add_menu __cogito_button_add_menu
#define cogito_button_add_menu_section __cogito_button_add_menu_section
#define cogito_iconbtn_add_menu __cogito_iconbtn_add_menu
#define cogito_iconbtn_add_menu_section __cogito_iconbtn_add_menu_section
#define cogito_iconbtn_set_shape __cogito_iconbtn_set_shape
#define cogito_iconbtn_get_shape __cogito_iconbtn_get_shape
#define cogito_iconbtn_set_color_style __cogito_iconbtn_set_color_style
#define cogito_iconbtn_get_color_style __cogito_iconbtn_get_color_style
#define cogito_iconbtn_set_width __cogito_iconbtn_set_width
#define cogito_iconbtn_get_width __cogito_iconbtn_get_width
#define cogito_iconbtn_set_toggle __cogito_iconbtn_set_toggle
#define cogito_iconbtn_get_toggle __cogito_iconbtn_get_toggle
#define cogito_iconbtn_set_checked __cogito_iconbtn_set_checked
#define cogito_iconbtn_get_checked __cogito_iconbtn_get_checked
#define cogito_iconbtn_set_size __cogito_iconbtn_set_size
#define cogito_iconbtn_get_size __cogito_iconbtn_get_size
#define cogito_iconbtn_on_click __cogito_iconbtn_on_click
#define cogito_fab_set_size __cogito_fab_set_size
#define cogito_fab_set_color __cogito_fab_set_color
#define cogito_fab_set_extended __cogito_fab_set_extended
#define cogito_fab_on_click __cogito_fab_on_click
#define cogito_fab_menu_new __cogito_fab_menu
#define cogito_fab_menu_add_item __cogito_fab_menu_add_item
#define cogito_fab_menu_set_color __cogito_fab_menu_set_color
#define cogito_chip_new __cogito_chip
#define cogito_chip_set_selected __cogito_chip_set_selected
#define cogito_chip_get_selected __cogito_chip_get_selected
#define cogito_chip_set_closable __cogito_chip_set_closable
#define cogito_chip_on_click __cogito_chip_on_click
#define cogito_chip_on_close __cogito_chip_on_close
#define cogito_divider_new __cogito_divider
#define cogito_card_new __cogito_card
#define cogito_avatar_new __cogito_avatar
#define cogito_avatar_set_image __cogito_avatar_set_image
#define cogito_badge_new __cogito_badge
#define cogito_badge_set_count __cogito_badge_set_count
#define cogito_badge_get_count __cogito_badge_get_count
#define cogito_banner_new __cogito_banner
#define cogito_banner_set_action __cogito_banner_set_action
#define cogito_banner_set_icon __cogito_banner_set_icon
#define cogito_bottom_sheet_new __cogito_bottom_sheet
#define cogito_side_sheet_new __cogito_side_sheet
#define cogito_timepicker_new __cogito_timepicker
#define cogito_timepicker_on_change __cogito_timepicker_on_change
#define cogito_timepicker_get_hour __cogito_timepicker_get_hour
#define cogito_timepicker_get_minute __cogito_timepicker_get_minute
#define cogito_timepicker_set_time __cogito_timepicker_set_time
#define cogito_appbar_add_button __cogito_appbar_add_button
#define cogito_appbar_set_controls __cogito_appbar_set_controls
#define cogito_appbar_set_title __cogito_appbar_set_title
#define cogito_appbar_set_subtitle __cogito_appbar_set_subtitle
#define cogito_iconbtn_add_menu __cogito_iconbtn_add_menu
#define cogito_run __cogito_run
#define cogito_timer_timeout __cogito_timer_timeout
#define cogito_timer_interval __cogito_timer_interval
#define cogito_timer_timeout_for __cogito_timer_timeout_for
#define cogito_timer_interval_for __cogito_timer_interval_for
#define cogito_timer_cancel __cogito_timer_cancel
#define cogito_timer_cancel_for __cogito_timer_cancel_for
#define cogito_timer_cancel_all __cogito_timer_cancel_all
#define cogito_load_sum __cogito_load_sum
#define cogito_set_script_dir __cogito_set_script_dir
#define cogito_active_indicator_new __cogito_active_indicator
#define cogito_switchbar_new __cogito_switchbar
#define cogito_switchbar_get_checked __cogito_switchbar_get_checked
#define cogito_switchbar_set_checked __cogito_switchbar_set_checked
#define cogito_switchbar_on_change __cogito_switchbar_on_change
#define cogito_drawing_area_new __cogito_drawing_area
#define cogito_drawing_area_on_press __cogito_drawing_area_on_press
#define cogito_drawing_area_on_drag __cogito_drawing_area_on_drag
#define cogito_drawing_area_on_release __cogito_drawing_area_on_release
#define cogito_drawing_area_on_draw __cogito_drawing_area_on_draw
#define cogito_drawing_area_get_x __cogito_drawing_area_get_x
#define cogito_drawing_area_get_y __cogito_drawing_area_get_y
#define cogito_drawing_area_get_pressed __cogito_drawing_area_get_pressed
#define cogito_drawing_area_clear __cogito_drawing_area_clear
#define cogito_canvas_set_color __cogito_canvas_set_color
#define cogito_canvas_set_line_width __cogito_canvas_set_line_width
#define cogito_canvas_line __cogito_canvas_line
#define cogito_canvas_rect __cogito_canvas_rect
#define cogito_canvas_fill_rect __cogito_canvas_fill_rect
#define cogito_shape_new __cogito_shape
#define cogito_shape_set_preset __cogito_shape_set_preset
#define cogito_shape_get_preset __cogito_shape_get_preset
#define cogito_shape_set_size __cogito_shape_set_size
#define cogito_shape_get_size __cogito_shape_get_size
#define cogito_shape_set_color __cogito_shape_set_color
#define cogito_shape_set_color_style __cogito_shape_set_color_style
#define cogito_shape_get_color_style __cogito_shape_get_color_style
#define cogito_shape_set_vertex __cogito_shape_set_vertex
#define cogito_shape_get_vertex_x __cogito_shape_get_vertex_x
#define cogito_shape_get_vertex_y __cogito_shape_get_vertex_y
#define cogito_content_list_new __cogito_content_list
#define cogito_empty_page_new __cogito_empty_page
#define cogito_empty_page_set_description __cogito_empty_page_set_description
#define cogito_empty_page_set_icon __cogito_empty_page_set_icon
#define cogito_empty_page_set_action __cogito_empty_page_set_action
#define cogito_tip_view_new __cogito_tip_view
#define cogito_tip_view_set_title __cogito_tip_view_set_title
#define cogito_settings_window_new __cogito_settings_window
#define cogito_settings_page_new __cogito_settings_page
#define cogito_settings_list_new __cogito_settings_list
#define cogito_settings_row_new __cogito_settings_row
#define cogito_welcome_screen_new __cogito_welcome_screen
#define cogito_welcome_screen_set_description __cogito_welcome_screen_set_description
#define cogito_welcome_screen_set_icon __cogito_welcome_screen_set_icon
#define cogito_welcome_screen_set_action __cogito_welcome_screen_set_action
#define cogito_view_dual_new __cogito_view_dual
#define cogito_view_dual_set_ratio __cogito_view_dual_set_ratio
#define cogito_view_chooser_new __cogito_view_chooser
#define cogito_view_chooser_set_items __cogito_view_chooser_set_items
#define cogito_view_chooser_bind __cogito_view_chooser_bind
#define cogito_about_window_new __cogito_about_window
#define cogito_about_window_set_icon __cogito_about_window_set_icon
#define cogito_about_window_set_description __cogito_about_window_set_description
#define cogito_about_window_set_website __cogito_about_window_set_website
#define cogito_about_window_set_issue_url __cogito_about_window_set_issue_url
#define cogito_menu_button_new __cogito_menu_button
#define cogito_menu_set_icon __cogito_menu_set_icon
#define cogito_menu_set_shortcut __cogito_menu_set_shortcut
#define cogito_menu_set_submenu __cogito_menu_set_submenu
#define cogito_menu_set_toggled __cogito_menu_set_toggled
#define cogito_split_button_new __cogito_split_button
#define cogito_split_button_add_menu __cogito_split_button_add_menu
#define cogito_split_button_add_menu_section __cogito_split_button_add_menu_section
#define cogito_split_button_set_size __cogito_split_button_set_size
#define cogito_split_button_set_variant __cogito_split_button_set_variant
#define cogito_buttongroup_set_size __cogito_buttongroup_set_size
#define cogito_buttongroup_get_size __cogito_buttongroup_get_size
#define cogito_buttongroup_set_shape __cogito_buttongroup_set_shape
#define cogito_buttongroup_get_shape __cogito_buttongroup_get_shape
#define cogito_buttongroup_set_connected __cogito_buttongroup_set_connected
#define cogito_buttongroup_get_connected __cogito_buttongroup_get_connected
#define cogito_card_set_subtitle __cogito_card_set_subtitle
#define cogito_card_on_click __cogito_card_on_click
#define cogito_card_set_variant __cogito_card_set_variant
#define cogito_card_set_header_image __cogito_card_set_header_image
#define cogito_card_add_action __cogito_card_add_action
#define cogito_card_add_overflow __cogito_card_add_overflow
// cogito_set_script_dir is provided by the cogito library
// ---- cask globals ----
static ErgoVal ergo_g_main_gafu_app = YV_NULLV;
static ErgoVal ergo_g_main_source_picker = YV_NULLV;
static ErgoVal ergo_g_main_upload_hint = YV_NULLV;
static ErgoVal ergo_g_main_scheme_switch = YV_NULLV;
static ErgoVal ergo_g_main_source_hex = YV_NULLV;
static ErgoVal ergo_g_main_selected_image_path = YV_NULLV;
static ErgoVal ergo_g_main_selected_image_preview = YV_NULLV;
static ErgoVal ergo_g_main_ENSOR_DEFAULT = YV_NULLV;
static ErgoVal ergo_g_main_ENSOR_CONTENT = YV_NULLV;
static ErgoVal ergo_g_main_mode_moon = YV_NULLV;
static ErgoVal ergo_g_main_mode_base = YV_NULLV;
static ErgoVal ergo_g_main_mode_bright = YV_NULLV;
static ErgoVal ergo_g_main_mode_brightest = YV_NULLV;
static ErgoVal ergo_g_main_dark_switch = YV_NULLV;

// ---- class definitions ----
typedef struct ErgoObj_cogito_App {
  ErgoObj base;
} ErgoObj_cogito_App;
static void ergo_drop_cogito_App(ErgoObj* o);

typedef struct ErgoObj_cogito_Window {
  ErgoObj base;
} ErgoObj_cogito_Window;
static void ergo_drop_cogito_Window(ErgoObj* o);

typedef struct ErgoObj_cogito_AppBar {
  ErgoObj base;
} ErgoObj_cogito_AppBar;
static void ergo_drop_cogito_AppBar(ErgoObj* o);

typedef struct ErgoObj_cogito_FABMenu {
  ErgoObj base;
} ErgoObj_cogito_FABMenu;
static void ergo_drop_cogito_FABMenu(ErgoObj* o);

typedef struct ErgoObj_cogito_Image {
  ErgoObj base;
} ErgoObj_cogito_Image;
static void ergo_drop_cogito_Image(ErgoObj* o);

typedef struct ErgoObj_cogito_Dialog {
  ErgoObj base;
} ErgoObj_cogito_Dialog;
static void ergo_drop_cogito_Dialog(ErgoObj* o);

typedef struct ErgoObj_cogito_DialogSlot {
  ErgoObj base;
} ErgoObj_cogito_DialogSlot;
static void ergo_drop_cogito_DialogSlot(ErgoObj* o);

typedef struct ErgoObj_cogito_VStack {
  ErgoObj base;
} ErgoObj_cogito_VStack;
static void ergo_drop_cogito_VStack(ErgoObj* o);

typedef struct ErgoObj_cogito_HStack {
  ErgoObj base;
} ErgoObj_cogito_HStack;
static void ergo_drop_cogito_HStack(ErgoObj* o);

typedef struct ErgoObj_cogito_ZStack {
  ErgoObj base;
} ErgoObj_cogito_ZStack;
static void ergo_drop_cogito_ZStack(ErgoObj* o);

typedef struct ErgoObj_cogito_Fixed {
  ErgoObj base;
} ErgoObj_cogito_Fixed;
static void ergo_drop_cogito_Fixed(ErgoObj* o);

typedef struct ErgoObj_cogito_Scroller {
  ErgoObj base;
} ErgoObj_cogito_Scroller;
static void ergo_drop_cogito_Scroller(ErgoObj* o);

typedef struct ErgoObj_cogito_Carousel {
  ErgoObj base;
} ErgoObj_cogito_Carousel;
static void ergo_drop_cogito_Carousel(ErgoObj* o);

typedef struct ErgoObj_cogito_CarouselItem {
  ErgoObj base;
} ErgoObj_cogito_CarouselItem;
static void ergo_drop_cogito_CarouselItem(ErgoObj* o);

typedef struct ErgoObj_cogito_List {
  ErgoObj base;
} ErgoObj_cogito_List;
static void ergo_drop_cogito_List(ErgoObj* o);

typedef struct ErgoObj_cogito_Grid {
  ErgoObj base;
} ErgoObj_cogito_Grid;
static void ergo_drop_cogito_Grid(ErgoObj* o);

typedef struct ErgoObj_cogito_Label {
  ErgoObj base;
} ErgoObj_cogito_Label;
static void ergo_drop_cogito_Label(ErgoObj* o);

typedef struct ErgoObj_cogito_Button {
  ErgoObj base;
} ErgoObj_cogito_Button;
static void ergo_drop_cogito_Button(ErgoObj* o);

typedef struct ErgoObj_cogito_IconButton {
  ErgoObj base;
} ErgoObj_cogito_IconButton;
static void ergo_drop_cogito_IconButton(ErgoObj* o);

typedef struct ErgoObj_cogito_Checkbox {
  ErgoObj base;
} ErgoObj_cogito_Checkbox;
static void ergo_drop_cogito_Checkbox(ErgoObj* o);

typedef struct ErgoObj_cogito_Switch {
  ErgoObj base;
} ErgoObj_cogito_Switch;
static void ergo_drop_cogito_Switch(ErgoObj* o);

typedef struct ErgoObj_cogito_SearchField {
  ErgoObj base;
} ErgoObj_cogito_SearchField;
static void ergo_drop_cogito_SearchField(ErgoObj* o);

typedef struct ErgoObj_cogito_TextField {
  ErgoObj base;
} ErgoObj_cogito_TextField;
static void ergo_drop_cogito_TextField(ErgoObj* o);

typedef struct ErgoObj_cogito_TextView {
  ErgoObj base;
} ErgoObj_cogito_TextView;
static void ergo_drop_cogito_TextView(ErgoObj* o);

typedef struct ErgoObj_cogito_DatePicker {
  ErgoObj base;
} ErgoObj_cogito_DatePicker;
static void ergo_drop_cogito_DatePicker(ErgoObj* o);

typedef struct ErgoObj_cogito_Stepper {
  ErgoObj base;
} ErgoObj_cogito_Stepper;
static void ergo_drop_cogito_Stepper(ErgoObj* o);

typedef struct ErgoObj_cogito_Dropdown {
  ErgoObj base;
} ErgoObj_cogito_Dropdown;
static void ergo_drop_cogito_Dropdown(ErgoObj* o);

typedef struct ErgoObj_cogito_Slider {
  ErgoObj base;
} ErgoObj_cogito_Slider;
static void ergo_drop_cogito_Slider(ErgoObj* o);

typedef struct ErgoObj_cogito_Tabs {
  ErgoObj base;
} ErgoObj_cogito_Tabs;
static void ergo_drop_cogito_Tabs(ErgoObj* o);

typedef struct ErgoObj_cogito_ButtonGroup {
  ErgoObj base;
} ErgoObj_cogito_ButtonGroup;
static void ergo_drop_cogito_ButtonGroup(ErgoObj* o);

typedef struct ErgoObj_cogito_ViewSwitcher {
  ErgoObj base;
} ErgoObj_cogito_ViewSwitcher;
static void ergo_drop_cogito_ViewSwitcher(ErgoObj* o);

typedef struct ErgoObj_cogito_Progress {
  ErgoObj base;
} ErgoObj_cogito_Progress;
static void ergo_drop_cogito_Progress(ErgoObj* o);

typedef struct ErgoObj_cogito_Divider {
  ErgoObj base;
} ErgoObj_cogito_Divider;
static void ergo_drop_cogito_Divider(ErgoObj* o);

typedef struct ErgoObj_cogito_Card {
  ErgoObj base;
} ErgoObj_cogito_Card;
static void ergo_drop_cogito_Card(ErgoObj* o);

typedef struct ErgoObj_cogito_Avatar {
  ErgoObj base;
} ErgoObj_cogito_Avatar;
static void ergo_drop_cogito_Avatar(ErgoObj* o);

typedef struct ErgoObj_cogito_Badge {
  ErgoObj base;
} ErgoObj_cogito_Badge;
static void ergo_drop_cogito_Badge(ErgoObj* o);

typedef struct ErgoObj_cogito_Banner {
  ErgoObj base;
} ErgoObj_cogito_Banner;
static void ergo_drop_cogito_Banner(ErgoObj* o);

typedef struct ErgoObj_cogito_BottomSheet {
  ErgoObj base;
} ErgoObj_cogito_BottomSheet;
static void ergo_drop_cogito_BottomSheet(ErgoObj* o);

typedef struct ErgoObj_cogito_SideSheet {
  ErgoObj base;
} ErgoObj_cogito_SideSheet;
static void ergo_drop_cogito_SideSheet(ErgoObj* o);

typedef struct ErgoObj_cogito_TimePicker {
  ErgoObj base;
} ErgoObj_cogito_TimePicker;
static void ergo_drop_cogito_TimePicker(ErgoObj* o);

typedef struct ErgoObj_cogito_TreeView {
  ErgoObj base;
} ErgoObj_cogito_TreeView;
static void ergo_drop_cogito_TreeView(ErgoObj* o);

typedef struct ErgoObj_cogito_ColorPicker {
  ErgoObj base;
} ErgoObj_cogito_ColorPicker;
static void ergo_drop_cogito_ColorPicker(ErgoObj* o);

typedef struct ErgoObj_cogito_Toasts {
  ErgoObj base;
} ErgoObj_cogito_Toasts;
static void ergo_drop_cogito_Toasts(ErgoObj* o);

typedef struct ErgoObj_cogito_Toast {
  ErgoObj base;
} ErgoObj_cogito_Toast;
static void ergo_drop_cogito_Toast(ErgoObj* o);

typedef struct ErgoObj_cogito_Toolbar {
  ErgoObj base;
} ErgoObj_cogito_Toolbar;
static void ergo_drop_cogito_Toolbar(ErgoObj* o);

typedef struct ErgoObj_cogito_Chip {
  ErgoObj base;
} ErgoObj_cogito_Chip;
static void ergo_drop_cogito_Chip(ErgoObj* o);

typedef struct ErgoObj_cogito_FAB {
  ErgoObj base;
} ErgoObj_cogito_FAB;
static void ergo_drop_cogito_FAB(ErgoObj* o);

typedef struct ErgoObj_cogito_NavRail {
  ErgoObj base;
} ErgoObj_cogito_NavRail;
static void ergo_drop_cogito_NavRail(ErgoObj* o);

typedef struct ErgoObj_cogito_BottomNav {
  ErgoObj base;
} ErgoObj_cogito_BottomNav;
static void ergo_drop_cogito_BottomNav(ErgoObj* o);

typedef struct ErgoObj_cogito_State {
  ErgoObj base;
} ErgoObj_cogito_State;
static void ergo_drop_cogito_State(ErgoObj* o);

typedef struct ErgoObj_cogito_ActiveIndicator {
  ErgoObj base;
} ErgoObj_cogito_ActiveIndicator;
static void ergo_drop_cogito_ActiveIndicator(ErgoObj* o);

typedef struct ErgoObj_cogito_SwitchBar {
  ErgoObj base;
} ErgoObj_cogito_SwitchBar;
static void ergo_drop_cogito_SwitchBar(ErgoObj* o);

typedef struct ErgoObj_cogito_DrawingArea {
  ErgoObj base;
} ErgoObj_cogito_DrawingArea;
static void ergo_drop_cogito_DrawingArea(ErgoObj* o);

typedef struct ErgoObj_cogito_Canvas {
  ErgoObj base;
} ErgoObj_cogito_Canvas;
static void ergo_drop_cogito_Canvas(ErgoObj* o);

typedef struct ErgoObj_cogito_Shape {
  ErgoObj base;
} ErgoObj_cogito_Shape;
static void ergo_drop_cogito_Shape(ErgoObj* o);

typedef struct ErgoObj_cogito_ContentList {
  ErgoObj base;
} ErgoObj_cogito_ContentList;
static void ergo_drop_cogito_ContentList(ErgoObj* o);

typedef struct ErgoObj_cogito_EmptyPage {
  ErgoObj base;
} ErgoObj_cogito_EmptyPage;
static void ergo_drop_cogito_EmptyPage(ErgoObj* o);

typedef struct ErgoObj_cogito_TipView {
  ErgoObj base;
} ErgoObj_cogito_TipView;
static void ergo_drop_cogito_TipView(ErgoObj* o);

typedef struct ErgoObj_cogito_SettingsWindow {
  ErgoObj base;
} ErgoObj_cogito_SettingsWindow;
static void ergo_drop_cogito_SettingsWindow(ErgoObj* o);

typedef struct ErgoObj_cogito_SettingsPage {
  ErgoObj base;
} ErgoObj_cogito_SettingsPage;
static void ergo_drop_cogito_SettingsPage(ErgoObj* o);

typedef struct ErgoObj_cogito_SettingsList {
  ErgoObj base;
} ErgoObj_cogito_SettingsList;
static void ergo_drop_cogito_SettingsList(ErgoObj* o);

typedef struct ErgoObj_cogito_SettingsRow {
  ErgoObj base;
} ErgoObj_cogito_SettingsRow;
static void ergo_drop_cogito_SettingsRow(ErgoObj* o);

typedef struct ErgoObj_cogito_WelcomeScreen {
  ErgoObj base;
} ErgoObj_cogito_WelcomeScreen;
static void ergo_drop_cogito_WelcomeScreen(ErgoObj* o);

typedef struct ErgoObj_cogito_ViewDual {
  ErgoObj base;
} ErgoObj_cogito_ViewDual;
static void ergo_drop_cogito_ViewDual(ErgoObj* o);

typedef struct ErgoObj_cogito_ViewChooser {
  ErgoObj base;
} ErgoObj_cogito_ViewChooser;
static void ergo_drop_cogito_ViewChooser(ErgoObj* o);

typedef struct ErgoObj_cogito_AboutWindow {
  ErgoObj base;
} ErgoObj_cogito_AboutWindow;
static void ergo_drop_cogito_AboutWindow(ErgoObj* o);

typedef struct ErgoObj_cogito_SplitButton {
  ErgoObj base;
} ErgoObj_cogito_SplitButton;
static void ergo_drop_cogito_SplitButton(ErgoObj* o);

static void ergo_drop_cogito_App(ErgoObj* o) {
  ErgoObj_cogito_App* self = (ErgoObj_cogito_App*)o;
}

static void ergo_drop_cogito_Window(ErgoObj* o) {
  ErgoObj_cogito_Window* self = (ErgoObj_cogito_Window*)o;
}

static void ergo_drop_cogito_AppBar(ErgoObj* o) {
  ErgoObj_cogito_AppBar* self = (ErgoObj_cogito_AppBar*)o;
}

static void ergo_drop_cogito_FABMenu(ErgoObj* o) {
  ErgoObj_cogito_FABMenu* self = (ErgoObj_cogito_FABMenu*)o;
}

static void ergo_drop_cogito_Image(ErgoObj* o) {
  ErgoObj_cogito_Image* self = (ErgoObj_cogito_Image*)o;
}

static void ergo_drop_cogito_Dialog(ErgoObj* o) {
  ErgoObj_cogito_Dialog* self = (ErgoObj_cogito_Dialog*)o;
}

static void ergo_drop_cogito_DialogSlot(ErgoObj* o) {
  ErgoObj_cogito_DialogSlot* self = (ErgoObj_cogito_DialogSlot*)o;
}

static void ergo_drop_cogito_VStack(ErgoObj* o) {
  ErgoObj_cogito_VStack* self = (ErgoObj_cogito_VStack*)o;
}

static void ergo_drop_cogito_HStack(ErgoObj* o) {
  ErgoObj_cogito_HStack* self = (ErgoObj_cogito_HStack*)o;
}

static void ergo_drop_cogito_ZStack(ErgoObj* o) {
  ErgoObj_cogito_ZStack* self = (ErgoObj_cogito_ZStack*)o;
}

static void ergo_drop_cogito_Fixed(ErgoObj* o) {
  ErgoObj_cogito_Fixed* self = (ErgoObj_cogito_Fixed*)o;
}

static void ergo_drop_cogito_Scroller(ErgoObj* o) {
  ErgoObj_cogito_Scroller* self = (ErgoObj_cogito_Scroller*)o;
}

static void ergo_drop_cogito_Carousel(ErgoObj* o) {
  ErgoObj_cogito_Carousel* self = (ErgoObj_cogito_Carousel*)o;
}

static void ergo_drop_cogito_CarouselItem(ErgoObj* o) {
  ErgoObj_cogito_CarouselItem* self = (ErgoObj_cogito_CarouselItem*)o;
}

static void ergo_drop_cogito_List(ErgoObj* o) {
  ErgoObj_cogito_List* self = (ErgoObj_cogito_List*)o;
}

static void ergo_drop_cogito_Grid(ErgoObj* o) {
  ErgoObj_cogito_Grid* self = (ErgoObj_cogito_Grid*)o;
}

static void ergo_drop_cogito_Label(ErgoObj* o) {
  ErgoObj_cogito_Label* self = (ErgoObj_cogito_Label*)o;
}

static void ergo_drop_cogito_Button(ErgoObj* o) {
  ErgoObj_cogito_Button* self = (ErgoObj_cogito_Button*)o;
}

static void ergo_drop_cogito_IconButton(ErgoObj* o) {
  ErgoObj_cogito_IconButton* self = (ErgoObj_cogito_IconButton*)o;
}

static void ergo_drop_cogito_Checkbox(ErgoObj* o) {
  ErgoObj_cogito_Checkbox* self = (ErgoObj_cogito_Checkbox*)o;
}

static void ergo_drop_cogito_Switch(ErgoObj* o) {
  ErgoObj_cogito_Switch* self = (ErgoObj_cogito_Switch*)o;
}

static void ergo_drop_cogito_SearchField(ErgoObj* o) {
  ErgoObj_cogito_SearchField* self = (ErgoObj_cogito_SearchField*)o;
}

static void ergo_drop_cogito_TextField(ErgoObj* o) {
  ErgoObj_cogito_TextField* self = (ErgoObj_cogito_TextField*)o;
}

static void ergo_drop_cogito_TextView(ErgoObj* o) {
  ErgoObj_cogito_TextView* self = (ErgoObj_cogito_TextView*)o;
}

static void ergo_drop_cogito_DatePicker(ErgoObj* o) {
  ErgoObj_cogito_DatePicker* self = (ErgoObj_cogito_DatePicker*)o;
}

static void ergo_drop_cogito_Stepper(ErgoObj* o) {
  ErgoObj_cogito_Stepper* self = (ErgoObj_cogito_Stepper*)o;
}

static void ergo_drop_cogito_Dropdown(ErgoObj* o) {
  ErgoObj_cogito_Dropdown* self = (ErgoObj_cogito_Dropdown*)o;
}

static void ergo_drop_cogito_Slider(ErgoObj* o) {
  ErgoObj_cogito_Slider* self = (ErgoObj_cogito_Slider*)o;
}

static void ergo_drop_cogito_Tabs(ErgoObj* o) {
  ErgoObj_cogito_Tabs* self = (ErgoObj_cogito_Tabs*)o;
}

static void ergo_drop_cogito_ButtonGroup(ErgoObj* o) {
  ErgoObj_cogito_ButtonGroup* self = (ErgoObj_cogito_ButtonGroup*)o;
}

static void ergo_drop_cogito_ViewSwitcher(ErgoObj* o) {
  ErgoObj_cogito_ViewSwitcher* self = (ErgoObj_cogito_ViewSwitcher*)o;
}

static void ergo_drop_cogito_Progress(ErgoObj* o) {
  ErgoObj_cogito_Progress* self = (ErgoObj_cogito_Progress*)o;
}

static void ergo_drop_cogito_Divider(ErgoObj* o) {
  ErgoObj_cogito_Divider* self = (ErgoObj_cogito_Divider*)o;
}

static void ergo_drop_cogito_Card(ErgoObj* o) {
  ErgoObj_cogito_Card* self = (ErgoObj_cogito_Card*)o;
}

static void ergo_drop_cogito_Avatar(ErgoObj* o) {
  ErgoObj_cogito_Avatar* self = (ErgoObj_cogito_Avatar*)o;
}

static void ergo_drop_cogito_Badge(ErgoObj* o) {
  ErgoObj_cogito_Badge* self = (ErgoObj_cogito_Badge*)o;
}

static void ergo_drop_cogito_Banner(ErgoObj* o) {
  ErgoObj_cogito_Banner* self = (ErgoObj_cogito_Banner*)o;
}

static void ergo_drop_cogito_BottomSheet(ErgoObj* o) {
  ErgoObj_cogito_BottomSheet* self = (ErgoObj_cogito_BottomSheet*)o;
}

static void ergo_drop_cogito_SideSheet(ErgoObj* o) {
  ErgoObj_cogito_SideSheet* self = (ErgoObj_cogito_SideSheet*)o;
}

static void ergo_drop_cogito_TimePicker(ErgoObj* o) {
  ErgoObj_cogito_TimePicker* self = (ErgoObj_cogito_TimePicker*)o;
}

static void ergo_drop_cogito_TreeView(ErgoObj* o) {
  ErgoObj_cogito_TreeView* self = (ErgoObj_cogito_TreeView*)o;
}

static void ergo_drop_cogito_ColorPicker(ErgoObj* o) {
  ErgoObj_cogito_ColorPicker* self = (ErgoObj_cogito_ColorPicker*)o;
}

static void ergo_drop_cogito_Toasts(ErgoObj* o) {
  ErgoObj_cogito_Toasts* self = (ErgoObj_cogito_Toasts*)o;
}

static void ergo_drop_cogito_Toast(ErgoObj* o) {
  ErgoObj_cogito_Toast* self = (ErgoObj_cogito_Toast*)o;
}

static void ergo_drop_cogito_Toolbar(ErgoObj* o) {
  ErgoObj_cogito_Toolbar* self = (ErgoObj_cogito_Toolbar*)o;
}

static void ergo_drop_cogito_Chip(ErgoObj* o) {
  ErgoObj_cogito_Chip* self = (ErgoObj_cogito_Chip*)o;
}

static void ergo_drop_cogito_FAB(ErgoObj* o) {
  ErgoObj_cogito_FAB* self = (ErgoObj_cogito_FAB*)o;
}

static void ergo_drop_cogito_NavRail(ErgoObj* o) {
  ErgoObj_cogito_NavRail* self = (ErgoObj_cogito_NavRail*)o;
}

static void ergo_drop_cogito_BottomNav(ErgoObj* o) {
  ErgoObj_cogito_BottomNav* self = (ErgoObj_cogito_BottomNav*)o;
}

static void ergo_drop_cogito_State(ErgoObj* o) {
  ErgoObj_cogito_State* self = (ErgoObj_cogito_State*)o;
}

static void ergo_drop_cogito_ActiveIndicator(ErgoObj* o) {
  ErgoObj_cogito_ActiveIndicator* self = (ErgoObj_cogito_ActiveIndicator*)o;
}

static void ergo_drop_cogito_SwitchBar(ErgoObj* o) {
  ErgoObj_cogito_SwitchBar* self = (ErgoObj_cogito_SwitchBar*)o;
}

static void ergo_drop_cogito_DrawingArea(ErgoObj* o) {
  ErgoObj_cogito_DrawingArea* self = (ErgoObj_cogito_DrawingArea*)o;
}

static void ergo_drop_cogito_Canvas(ErgoObj* o) {
  ErgoObj_cogito_Canvas* self = (ErgoObj_cogito_Canvas*)o;
}

static void ergo_drop_cogito_Shape(ErgoObj* o) {
  ErgoObj_cogito_Shape* self = (ErgoObj_cogito_Shape*)o;
}

static void ergo_drop_cogito_ContentList(ErgoObj* o) {
  ErgoObj_cogito_ContentList* self = (ErgoObj_cogito_ContentList*)o;
}

static void ergo_drop_cogito_EmptyPage(ErgoObj* o) {
  ErgoObj_cogito_EmptyPage* self = (ErgoObj_cogito_EmptyPage*)o;
}

static void ergo_drop_cogito_TipView(ErgoObj* o) {
  ErgoObj_cogito_TipView* self = (ErgoObj_cogito_TipView*)o;
}

static void ergo_drop_cogito_SettingsWindow(ErgoObj* o) {
  ErgoObj_cogito_SettingsWindow* self = (ErgoObj_cogito_SettingsWindow*)o;
}

static void ergo_drop_cogito_SettingsPage(ErgoObj* o) {
  ErgoObj_cogito_SettingsPage* self = (ErgoObj_cogito_SettingsPage*)o;
}

static void ergo_drop_cogito_SettingsList(ErgoObj* o) {
  ErgoObj_cogito_SettingsList* self = (ErgoObj_cogito_SettingsList*)o;
}

static void ergo_drop_cogito_SettingsRow(ErgoObj* o) {
  ErgoObj_cogito_SettingsRow* self = (ErgoObj_cogito_SettingsRow*)o;
}

static void ergo_drop_cogito_WelcomeScreen(ErgoObj* o) {
  ErgoObj_cogito_WelcomeScreen* self = (ErgoObj_cogito_WelcomeScreen*)o;
}

static void ergo_drop_cogito_ViewDual(ErgoObj* o) {
  ErgoObj_cogito_ViewDual* self = (ErgoObj_cogito_ViewDual*)o;
}

static void ergo_drop_cogito_ViewChooser(ErgoObj* o) {
  ErgoObj_cogito_ViewChooser* self = (ErgoObj_cogito_ViewChooser*)o;
}

static void ergo_drop_cogito_AboutWindow(ErgoObj* o) {
  ErgoObj_cogito_AboutWindow* self = (ErgoObj_cogito_AboutWindow*)o;
}

static void ergo_drop_cogito_SplitButton(ErgoObj* o) {
  ErgoObj_cogito_SplitButton* self = (ErgoObj_cogito_SplitButton*)o;
}


// ---- lambda forward decls ----
static ErgoVal ergo_lambda_1(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_2(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_3(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_4(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_5(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_6(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_7(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_8(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_9(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_10(void* env, int argc, ErgoVal* argv);

// ---- function value forward decls ----
static ErgoVal __fnwrap_cogito_label(void* env, int argc, ErgoVal* argv);
static ErgoVal __fnwrap_cogito_dialog(void* env, int argc, ErgoVal* argv);
static ErgoVal __fnwrap_cogito_side_sheet(void* env, int argc, ErgoVal* argv);
static ErgoVal __fnwrap_cogito_divider(void* env, int argc, ErgoVal* argv);
static ErgoVal __fnwrap_cogito_shape(void* env, int argc, ErgoVal* argv);

// ---- forward decls ----
static void ergo_main_apply_source_color(ErgoVal a0);
static void ergo_main_choose_source_image(void);
static void ergo_main_reset_everything(void);
static ErgoVal ergo_main_swatch(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_main_tone_column(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4, ErgoVal a5, ErgoVal a6, ErgoVal a7);
static ErgoVal ergo_main_fixed_column(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4, ErgoVal a5, ErgoVal a6, ErgoVal a7);
static void ergo_main_set_mode_selection(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4);
static ErgoVal ergo_main_build_palette_grid(void);
static ErgoVal ergo_main_build_right_panel(void);
static void ergo_init_main(void);
static void ergo_stdr___writef(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_stdr___read_line(void);
static ErgoVal ergo_stdr___readf_parse(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_stdr___read_text_file(ErgoVal a0);
static ErgoVal ergo_stdr___write_text_file(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_stdr___open_file_dialog(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_stdr___save_file_dialog(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_stdr_writef(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_stdr_readf(ErgoVal a0, ErgoVal a1);
static void ergo_stdr_write(ErgoVal a0);
static ErgoVal ergo_stdr_is_null(ErgoVal a0);
static ErgoVal ergo_stdr_str(ErgoVal a0);
static ErgoVal ergo_stdr___len(ErgoVal a0);
static ErgoVal ergo_stdr_len(ErgoVal a0);
static ErgoVal ergo_stdr_read_text_file(ErgoVal a0);
static ErgoVal ergo_stdr_write_text_file(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_stdr_open_file_dialog(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_stdr_save_file_dialog(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_about_window_set_description(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_about_window_set_icon(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_about_window_set_issue_url(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_about_window_set_website(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_about_window(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_active_indicator(void);
static ErgoVal ergo_cogito___cogito_app_get_icon(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_app_copy_to_clipboard(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_app_set_accent_color(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_app_set_dark_mode(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_app_set_app_name(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_app_set_appid(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_app_set_ensor_variant(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_app_set_contrast(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_app_set_icon(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_app(void);
static ErgoVal ergo_cogito___cogito_appbar_add_button(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_appbar_set_controls(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_appbar_set_subtitle(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_appbar_set_title(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_appbar(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_avatar_set_image(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_avatar(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_badge_get_count(ErgoVal a0);
static void ergo_cogito___cogito_badge_set_count(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_badge(ErgoVal a0);
static void ergo_cogito___cogito_banner_set_action(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_banner_set_icon(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_banner(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_bottom_nav_get_selected(ErgoVal a0);
static void ergo_cogito___cogito_bottom_nav_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_bottom_nav_set_items(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_bottom_nav_set_selected(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_bottom_nav(void);
static ErgoVal ergo_cogito___cogito_bottom_sheet(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_side_sheet(ErgoVal a0);
static void ergo_cogito___cogito_side_sheet_set_mode(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_build(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_button_add_menu(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_button_add_menu_section(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_button_set_menu_divider(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_button_get_menu_divider(ErgoVal a0);
static void ergo_cogito___cogito_button_set_menu_item_gap(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_button_get_menu_item_gap(ErgoVal a0);
static void ergo_cogito___cogito_button_set_menu_vibrant(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_button_get_menu_vibrant(ErgoVal a0);
static void ergo_cogito___cogito_menu_set_icon(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_menu_set_shortcut(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_menu_set_submenu(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_menu_set_toggled(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_button_get_size(ErgoVal a0);
static void ergo_cogito___cogito_button_on_click(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_button_set_size(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_button_set_text(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_button(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_card(ErgoVal a0);
static void ergo_cogito___cogito_card_add_action(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_card_add_overflow(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_card_on_click(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_card_set_header_image(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_card_set_subtitle(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_card_set_variant(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_carousel_get_active_index(ErgoVal a0);
static void ergo_cogito___cogito_carousel_item_set_halign(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_carousel_item_set_text(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_carousel_item_set_valign(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_carousel_item(void);
static void ergo_cogito___cogito_carousel_set_active_index(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_carousel(void);
static ErgoVal ergo_cogito___cogito_checkbox_get_checked(ErgoVal a0);
static void ergo_cogito___cogito_checkbox_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_checkbox_set_checked(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_checkbox(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_chip_get_selected(ErgoVal a0);
static void ergo_cogito___cogito_chip_on_click(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_chip_on_close(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_chip_set_closable(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_chip_set_selected(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_chip(ErgoVal a0);
static void ergo_cogito___cogito_colorpicker_on_change(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_colorpicker(void);
static ErgoVal ergo_cogito___cogito_app_set_accent_from_image(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_colorpicker_set_hex(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_colorpicker_get_hex(ErgoVal a0);
static void ergo_cogito___cogito_container_add(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_align(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_gap(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_halign(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_hexpand(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_margins(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4);
static void ergo_cogito___cogito_container_set_padding(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4);
static void ergo_cogito___cogito_container_set_valign(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_vexpand(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_content_list(void);
static void ergo_cogito___cogito_datepicker_on_change(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_datepicker(void);
static void ergo_cogito___cogito_dialog_close(ErgoVal a0);
static void ergo_cogito___cogito_dialog_remove(ErgoVal a0);
static void ergo_cogito___cogito_dialog_slot_clear(ErgoVal a0);
static void ergo_cogito___cogito_dialog_slot_show(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_dialog_slot(void);
static ErgoVal ergo_cogito___cogito_dialog(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_divider(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_dropdown_get_selected(ErgoVal a0);
static void ergo_cogito___cogito_dropdown_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_dropdown_set_items(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_dropdown_set_selected(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_dropdown(void);
static void ergo_cogito___cogito_empty_page_set_action(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_empty_page_set_description(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_empty_page_set_icon(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_empty_page(ErgoVal a0);
static void ergo_cogito___cogito_fab_on_click(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_fab_set_extended(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito___cogito_fab(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_fab_menu(ErgoVal a0);
static void ergo_cogito___cogito_fab_menu_add_item(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_fab_menu_set_color(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_find_children(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_find_parent(ErgoVal a0);
static void ergo_cogito___cogito_fixed_set_pos(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_cogito___cogito_fixed(void);
static void ergo_cogito___cogito_grid_on_activate(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_grid_on_select(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_grid_set_align(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_grid_set_gap(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_grid_set_span(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito___cogito_grid(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_hstack(void);
static void ergo_cogito___cogito_iconbtn_add_menu(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_iconbtn_add_menu_section(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito___cogito_iconbtn_get_checked(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_iconbtn_get_color_style(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_iconbtn_get_shape(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_iconbtn_get_size(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_iconbtn_get_toggle(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_iconbtn_get_width(ErgoVal a0);
static void ergo_cogito___cogito_iconbtn_set_checked(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_iconbtn_set_color_style(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_iconbtn_set_shape(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_iconbtn_set_size(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_iconbtn_set_toggle(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_iconbtn_set_width(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_iconbtn_set_menu_divider(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_iconbtn_get_menu_divider(ErgoVal a0);
static void ergo_cogito___cogito_iconbtn_set_menu_item_gap(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_iconbtn_get_menu_item_gap(ErgoVal a0);
static void ergo_cogito___cogito_iconbtn_set_menu_vibrant(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_iconbtn_get_menu_vibrant(ErgoVal a0);
static void ergo_cogito___cogito_iconbtn_on_click(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_iconbtn(ErgoVal a0);
static void ergo_cogito___cogito_image_set_icon(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_image_set_radius(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_image_set_size(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_image_set_source(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_image(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_drawing_area(void);
static void ergo_cogito___cogito_drawing_area_on_press(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_drawing_area_on_drag(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_drawing_area_on_release(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_drawing_area_on_draw(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_drawing_area_get_x(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_drawing_area_get_y(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_drawing_area_get_pressed(ErgoVal a0);
static void ergo_cogito___cogito_drawing_area_clear(ErgoVal a0);
static void ergo_cogito___cogito_canvas_set_color(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_canvas_set_line_width(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_canvas_line(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4);
static void ergo_cogito___cogito_canvas_rect(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4);
static void ergo_cogito___cogito_canvas_fill_rect(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4);
static ErgoVal ergo_cogito___cogito_shape(ErgoVal a0);
static void ergo_cogito___cogito_shape_set_preset(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_shape_get_preset(ErgoVal a0);
static void ergo_cogito___cogito_shape_set_size(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_shape_get_size(ErgoVal a0);
static void ergo_cogito___cogito_shape_set_color(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_shape_set_color_style(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_shape_get_color_style(ErgoVal a0);
static void ergo_cogito___cogito_shape_set_vertex(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_cogito___cogito_shape_get_vertex_x(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_shape_get_vertex_y(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_label_set_align(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_label_set_class(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_label_set_ellipsis(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_label_set_text(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_label_set_wrap(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_label(ErgoVal a0);
static void ergo_cogito___cogito_list_on_activate(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_list_on_select(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_list(void);
static void ergo_cogito___cogito_load_sum(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_menu_button(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_nav_rail_get_selected(ErgoVal a0);
static void ergo_cogito___cogito_nav_rail_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_nav_rail_set_badges(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_nav_rail_set_items(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_nav_rail_set_selected(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_nav_rail_set_toggle(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_nav_rail(void);
static ErgoVal ergo_cogito___cogito_node_get_editable(ErgoVal a0);
static void ergo_cogito___cogito_node_set_a11y_label(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_node_set_a11y_role(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_node_set_class(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_node_set_disabled(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_node_set_editable(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_node_set_id(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_node_set_tooltip(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_node_window(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_open_url(ErgoVal a0);
static void ergo_cogito___cogito_pointer_capture(ErgoVal a0);
static void ergo_cogito___cogito_pointer_release(void);
static ErgoVal ergo_cogito___cogito_progress_get_circular(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_progress_get_indeterminate(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_progress_get_thickness(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_progress_get_value(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_progress_get_wavy(ErgoVal a0);
static void ergo_cogito___cogito_progress_set_circular(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_progress_set_indeterminate(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_progress_set_thickness(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_progress_set_value(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_progress_set_wavy(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_progress(ErgoVal a0);
static void ergo_cogito___cogito_run(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_timer_timeout(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_timer_interval(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_timer_timeout_for(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito___cogito_timer_interval_for(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito___cogito_timer_cancel(ErgoVal a0);
static void ergo_cogito___cogito_timer_cancel_for(ErgoVal a0);
static void ergo_cogito___cogito_timer_cancel_all(void);
static void ergo_cogito___cogito_scroller_set_axes(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito___cogito_scroller(void);
static ErgoVal ergo_cogito___cogito_searchfield_get_text(ErgoVal a0);
static void ergo_cogito___cogito_searchfield_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_searchfield_set_text(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_searchfield(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_buttongroup(void);
static void ergo_cogito___cogito_buttongroup_on_select(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_buttongroup_set_size(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_buttongroup_get_size(ErgoVal a0);
static void ergo_cogito___cogito_buttongroup_set_shape(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_buttongroup_get_shape(ErgoVal a0);
static void ergo_cogito___cogito_buttongroup_set_connected(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_buttongroup_get_connected(ErgoVal a0);
static void ergo_cogito___cogito_node_set_flag(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_node_clear_flag(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_set_script_dir(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_settings_list(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_settings_page(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_settings_row(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_settings_window(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_slider_get_centered(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_slider_get_range_end(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_slider_get_range_start(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_slider_get_size(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_slider_get_value(ErgoVal a0);
static void ergo_cogito___cogito_slider_on_change(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_slider_range(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_cogito___cogito_slider_set_centered(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_slider_set_icon(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_slider_set_range_end(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_slider_set_range_start(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_slider_set_range(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_slider_set_size(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_slider_set_value(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_slider(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_split_button_add_menu(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_split_button_add_menu_section(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_split_button_set_size(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_split_button_set_variant(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_split_button(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_state_get(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_state_new(ErgoVal a0);
static void ergo_cogito___cogito_state_set(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_stepper(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_cogito___cogito_switch_get_checked(ErgoVal a0);
static void ergo_cogito___cogito_switch_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_switch_set_checked(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_switch(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_switchbar_get_checked(ErgoVal a0);
static void ergo_cogito___cogito_switchbar_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_switchbar_set_checked(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_switchbar(ErgoVal a0);
static void ergo_cogito___cogito_tabs_bind(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_tabs_get_selected(ErgoVal a0);
static void ergo_cogito___cogito_tabs_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_tabs_set_ids(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_tabs_set_items(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_tabs_set_selected(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_tabs(void);
static ErgoVal ergo_cogito___cogito_textfield_get_hint(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_textfield_get_text(ErgoVal a0);
static void ergo_cogito___cogito_textfield_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_textfield_set_hint(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_textfield_set_text(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_textfield(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_textview_get_text(ErgoVal a0);
static void ergo_cogito___cogito_textview_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_textview_set_text(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_textview(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_timepicker_get_hour(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_timepicker_get_minute(ErgoVal a0);
static void ergo_cogito___cogito_timepicker_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_timepicker_set_time(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito___cogito_timepicker(void);
static void ergo_cogito___cogito_tip_view_set_title(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_tip_view(ErgoVal a0);
static void ergo_cogito___cogito_toast_on_click(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_toast_set_text(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_toast(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_toasts(void);
static ErgoVal ergo_cogito___cogito_toolbar(void);
static void ergo_cogito___cogito_toolbar_set_vibrant(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_toolbar_get_vibrant(ErgoVal a0);
static void ergo_cogito___cogito_toolbar_set_vertical(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_toolbar_get_vertical(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_treeview(void);
static void ergo_cogito___cogito_view_chooser_bind(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_view_chooser_set_items(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_view_chooser(void);
static void ergo_cogito___cogito_view_dual_set_ratio(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_view_dual(void);
static void ergo_cogito___cogito_view_switcher_add_lazy(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_view_switcher_set_active(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_view_switcher(void);
static ErgoVal ergo_cogito___cogito_vstack(void);
static void ergo_cogito___cogito_welcome_screen_set_action(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_welcome_screen_set_description(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_welcome_screen_set_icon(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_welcome_screen(ErgoVal a0);
static void ergo_cogito___cogito_window_clear_dialog(ErgoVal a0);
static void ergo_cogito___cogito_window_clear_side_sheet(ErgoVal a0);
static void ergo_cogito___cogito_window_set_autosize(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_window_set_builder(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_window_set_dialog(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_window_set_resizable(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_window_set_side_sheet(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_window(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito___cogito_zstack(void);
static ErgoVal ergo_m_cogito_App_run(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_App_set_appid(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_App_set_app_name(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_App_set_accent_color(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_App_set_dark_mode(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_App_set_accent_from_image(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_App_set_ensor_variant(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_App_set_contrast(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_App_set_icon(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_App_get_icon(ErgoVal self);
static ErgoVal ergo_m_cogito_App_copy_to_clipboard(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Window_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Window_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Window_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Window_set_autosize(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Window_set_resizable(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Window_set_a11y_label(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Window_set_a11y_role(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Window_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Window_set_dialog(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Window_clear_dialog(ErgoVal self);
static ErgoVal ergo_m_cogito_Window_set_side_sheet(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Window_clear_side_sheet(ErgoVal self);
static ErgoVal ergo_m_cogito_Window_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Window_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Window_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AppBar_add_button(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_AppBar_set_window_controls(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AppBar_set_title(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AppBar_set_title_widget(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AppBar_set_subtitle(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AppBar_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AppBar_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AppBar_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AppBar_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AppBar_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FABMenu_add_item(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_FABMenu_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FABMenu_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FABMenu_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FABMenu_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FABMenu_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Image_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Image_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_set_icon(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_set_source(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_set_size(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_Image_set_radius(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_set_alt_text(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Image_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dialog_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dialog_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Dialog_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Dialog_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dialog_window(ErgoVal self);
static ErgoVal ergo_m_cogito_Dialog_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dialog_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dialog_close(ErgoVal self);
static ErgoVal ergo_m_cogito_Dialog_remove(ErgoVal self);
static ErgoVal ergo_m_cogito_Dialog_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dialog_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dialog_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DialogSlot_show(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DialogSlot_clear(ErgoVal self);
static ErgoVal ergo_m_cogito_DialogSlot_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DialogSlot_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DialogSlot_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DialogSlot_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DialogSlot_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_VStack_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_VStack_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_VStack_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_VStack_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_VStack_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_VStack_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_VStack_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_VStack_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_VStack_set_gap(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_VStack_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_VStack_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_VStack_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_VStack_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_VStack_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_VStack_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_VStack_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_HStack_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_HStack_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_HStack_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_HStack_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_HStack_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_HStack_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_HStack_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_HStack_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_HStack_set_gap(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_HStack_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_HStack_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_HStack_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_HStack_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_HStack_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_HStack_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_HStack_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ZStack_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ZStack_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_ZStack_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_ZStack_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ZStack_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ZStack_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ZStack_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_ZStack_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_ZStack_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_ZStack_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ZStack_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ZStack_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ZStack_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ZStack_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ZStack_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Fixed_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Fixed_set_pos(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_m_cogito_Fixed_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Fixed_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Fixed_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Fixed_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Fixed_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Fixed_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Fixed_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Fixed_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Fixed_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Scroller_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Scroller_set_axes(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_Scroller_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Scroller_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Scroller_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Scroller_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Scroller_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Scroller_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Scroller_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Scroller_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Scroller_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Carousel_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Carousel_set_active_index(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Carousel_active_index(ErgoVal self);
static ErgoVal ergo_m_cogito_Carousel_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Carousel_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Carousel_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Carousel_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_CarouselItem_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_CarouselItem_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_CarouselItem_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_CarouselItem_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_CarouselItem_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_CarouselItem_set_text(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_CarouselItem_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_CarouselItem_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_List_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_List_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_List_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_List_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_List_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_List_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_List_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_List_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_List_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_List_on_select(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_List_on_activate(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_List_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_List_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_List_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_List_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_List_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_List_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Grid_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Grid_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Grid_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Grid_set_gap(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_Grid_set_span(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_m_cogito_Grid_set_cell_align(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_m_cogito_Grid_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Grid_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Grid_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Grid_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_Grid_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_Grid_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_Grid_on_select(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Grid_on_activate(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Grid_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Grid_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Grid_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Grid_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Grid_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Grid_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Label_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Label_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_Label_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_Label_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_Label_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_set_text(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_set_wrap(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_set_ellipsis(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_set_text_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Label_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Button_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Button_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_Button_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_Button_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_Button_set_text(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_set_size(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_size(ErgoVal self);
static ErgoVal ergo_m_cogito_Button_set_xs(ErgoVal self);
static ErgoVal ergo_m_cogito_Button_set_s(ErgoVal self);
static ErgoVal ergo_m_cogito_Button_set_m(ErgoVal self);
static ErgoVal ergo_m_cogito_Button_set_l(ErgoVal self);
static ErgoVal ergo_m_cogito_Button_set_xl(ErgoVal self);
static ErgoVal ergo_m_cogito_Button_on_click(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_add_menu(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_Button_add_menu_section(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_Button_menu_set_icon(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_menu_set_shortcut(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_menu_set_submenu(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_menu_set_toggled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_set_menu_divider(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_menu_divider(ErgoVal self);
static ErgoVal ergo_m_cogito_Button_set_menu_item_gap(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_menu_item_gap(ErgoVal self);
static ErgoVal ergo_m_cogito_Button_set_menu_vibrant(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_menu_vibrant(ErgoVal self);
static ErgoVal ergo_m_cogito_Button_window(ErgoVal self);
static ErgoVal ergo_m_cogito_Button_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Button_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_IconButton_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_IconButton_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_add_menu(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_IconButton_add_menu_section(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_IconButton_menu_set_icon(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_menu_set_shortcut(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_menu_set_submenu(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_menu_set_toggled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_window(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_shape(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_shape(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_color_style(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_color_style(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_width(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_width(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_toggle(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_toggle(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_checked(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_checked(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_size(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_size(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_xs(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_s(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_m(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_l(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_xl(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_menu_divider(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_menu_divider(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_menu_item_gap(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_menu_item_gap(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_set_menu_vibrant(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_menu_vibrant(ErgoVal self);
static ErgoVal ergo_m_cogito_IconButton_on_click(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_IconButton_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Checkbox_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Checkbox_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Checkbox_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Checkbox_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Checkbox_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Checkbox_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_Checkbox_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_Checkbox_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_Checkbox_set_checked(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Checkbox_checked(ErgoVal self);
static ErgoVal ergo_m_cogito_Checkbox_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Checkbox_window(ErgoVal self);
static ErgoVal ergo_m_cogito_Checkbox_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Checkbox_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Checkbox_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Checkbox_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Checkbox_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Switch_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Switch_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Switch_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Switch_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Switch_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Switch_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_Switch_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_Switch_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_Switch_set_checked(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Switch_checked(ErgoVal self);
static ErgoVal ergo_m_cogito_Switch_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Switch_window(ErgoVal self);
static ErgoVal ergo_m_cogito_Switch_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Switch_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Switch_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Switch_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Switch_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SearchField_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_SearchField_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_SearchField_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SearchField_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SearchField_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SearchField_set_text(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SearchField_text(ErgoVal self);
static ErgoVal ergo_m_cogito_SearchField_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SearchField_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SearchField_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SearchField_set_editable(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SearchField_editable(ErgoVal self);
static ErgoVal ergo_m_cogito_SearchField_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SearchField_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_TextField_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_TextField_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_TextField_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_TextField_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_TextField_set_text(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_text(ErgoVal self);
static ErgoVal ergo_m_cogito_TextField_set_hint_text(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_hint_text(ErgoVal self);
static ErgoVal ergo_m_cogito_TextField_set_hint(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_hint(ErgoVal self);
static ErgoVal ergo_m_cogito_TextField_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_set_editable(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_editable(ErgoVal self);
static ErgoVal ergo_m_cogito_TextField_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextView_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_TextView_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_TextView_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextView_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextView_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextView_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_TextView_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_TextView_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_TextView_set_text(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextView_text(ErgoVal self);
static ErgoVal ergo_m_cogito_TextView_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextView_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextView_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextView_set_editable(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextView_editable(ErgoVal self);
static ErgoVal ergo_m_cogito_TextView_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextView_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextView_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DatePicker_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_DatePicker_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_DatePicker_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DatePicker_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DatePicker_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DatePicker_set_date(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_m_cogito_DatePicker_date(ErgoVal self);
static ErgoVal ergo_m_cogito_DatePicker_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DatePicker_set_a11y_label(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DatePicker_set_a11y_role(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DatePicker_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DatePicker_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DatePicker_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DatePicker_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DatePicker_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Stepper_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Stepper_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Stepper_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Stepper_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Stepper_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Stepper_set_value(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Stepper_value(ErgoVal self);
static ErgoVal ergo_m_cogito_Stepper_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Stepper_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Stepper_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Stepper_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Stepper_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dropdown_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Dropdown_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Dropdown_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dropdown_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dropdown_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dropdown_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_Dropdown_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_Dropdown_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_Dropdown_set_items(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dropdown_set_selected(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dropdown_selected(ErgoVal self);
static ErgoVal ergo_m_cogito_Dropdown_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dropdown_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dropdown_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dropdown_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dropdown_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dropdown_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Slider_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Slider_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_Slider_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_Slider_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_Slider_set_value(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_value(ErgoVal self);
static ErgoVal ergo_m_cogito_Slider_set_centered(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_centered(ErgoVal self);
static ErgoVal ergo_m_cogito_Slider_set_range(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_Slider_set_range_start(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_set_range_end(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_range_start(ErgoVal self);
static ErgoVal ergo_m_cogito_Slider_range_end(ErgoVal self);
static ErgoVal ergo_m_cogito_Slider_set_size(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_size(ErgoVal self);
static ErgoVal ergo_m_cogito_Slider_set_xs(ErgoVal self);
static ErgoVal ergo_m_cogito_Slider_set_s(ErgoVal self);
static ErgoVal ergo_m_cogito_Slider_set_m(ErgoVal self);
static ErgoVal ergo_m_cogito_Slider_set_l(ErgoVal self);
static ErgoVal ergo_m_cogito_Slider_set_xl(ErgoVal self);
static ErgoVal ergo_m_cogito_Slider_set_icon(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_on_select(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Tabs_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Tabs_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_set_items(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_set_ids(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_set_selected(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_selected(ErgoVal self);
static ErgoVal ergo_m_cogito_Tabs_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_bind(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_ButtonGroup_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_ButtonGroup_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_set_size(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_set_shape(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_set_connected(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_set_items(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_set_selected(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_selected(ErgoVal self);
static ErgoVal ergo_m_cogito_ButtonGroup_on_select(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ButtonGroup_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewSwitcher_add(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_ViewSwitcher_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_ViewSwitcher_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_ViewSwitcher_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewSwitcher_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewSwitcher_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewSwitcher_set_active(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewSwitcher_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewSwitcher_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewSwitcher_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewSwitcher_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewSwitcher_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewSwitcher_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Progress_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Progress_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_set_value(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_value(ErgoVal self);
static ErgoVal ergo_m_cogito_Progress_set_indeterminate(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_indeterminate(ErgoVal self);
static ErgoVal ergo_m_cogito_Progress_set_thickness(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_thickness(ErgoVal self);
static ErgoVal ergo_m_cogito_Progress_set_wavy(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_wavy(ErgoVal self);
static ErgoVal ergo_m_cogito_Progress_set_circular(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_circular(ErgoVal self);
static ErgoVal ergo_m_cogito_Progress_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Divider_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Divider_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Divider_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Divider_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Divider_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Divider_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Divider_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Divider_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Divider_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Divider_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Card_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Card_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_set_gap(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_set_subtitle(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_on_click(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_set_variant(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_set_header_image(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_add_action(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Card_add_overflow(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_Avatar_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Avatar_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Avatar_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Avatar_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Avatar_set_image(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Avatar_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Avatar_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Avatar_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Avatar_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Badge_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Badge_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Badge_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Badge_set_count(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Badge_count(ErgoVal self);
static ErgoVal ergo_m_cogito_Badge_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Badge_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Banner_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Banner_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Banner_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Banner_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Banner_set_action(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_Banner_set_icon(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Banner_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Banner_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Banner_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Banner_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomSheet_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomSheet_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_BottomSheet_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_BottomSheet_set_gap(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomSheet_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomSheet_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomSheet_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomSheet_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomSheet_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SideSheet_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SideSheet_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_SideSheet_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_SideSheet_set_gap(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SideSheet_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SideSheet_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SideSheet_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SideSheet_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SideSheet_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SideSheet_set_mode(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TimePicker_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_TimePicker_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_TimePicker_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TimePicker_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TimePicker_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TimePicker_hour(ErgoVal self);
static ErgoVal ergo_m_cogito_TimePicker_minute(ErgoVal self);
static ErgoVal ergo_m_cogito_TimePicker_set_time(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_TimePicker_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TimePicker_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TimePicker_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TimePicker_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TimePicker_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TreeView_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TreeView_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_TreeView_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_TreeView_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TreeView_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TreeView_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TreeView_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TreeView_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TreeView_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TreeView_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ColorPicker_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_ColorPicker_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_ColorPicker_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ColorPicker_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ColorPicker_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ColorPicker_set_hex(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ColorPicker_hex(ErgoVal self);
static ErgoVal ergo_m_cogito_ColorPicker_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ColorPicker_set_a11y_label(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ColorPicker_set_a11y_role(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ColorPicker_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ColorPicker_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ColorPicker_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ColorPicker_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ColorPicker_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toasts_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toasts_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Toasts_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Toasts_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toasts_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toasts_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toasts_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toasts_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toasts_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toasts_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toast_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Toast_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Toast_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toast_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toast_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toast_set_text(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toast_on_click(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toast_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toast_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toast_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toast_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toast_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toolbar_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toolbar_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toolbar_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toolbar_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toolbar_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toolbar_set_vibrant(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toolbar_vibrant(ErgoVal self);
static ErgoVal ergo_m_cogito_Toolbar_set_vertical(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toolbar_vertical(ErgoVal self);
static ErgoVal ergo_m_cogito_Chip_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Chip_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Chip_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Chip_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Chip_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Chip_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Chip_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Chip_set_selected(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Chip_selected(ErgoVal self);
static ErgoVal ergo_m_cogito_Chip_set_closable(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Chip_on_click(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Chip_on_close(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Chip_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Chip_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Chip_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FAB_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_FAB_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_FAB_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FAB_set_halign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FAB_set_valign(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FAB_align_begin(ErgoVal self);
static ErgoVal ergo_m_cogito_FAB_align_center(ErgoVal self);
static ErgoVal ergo_m_cogito_FAB_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_FAB_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FAB_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FAB_set_extended(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_FAB_on_click(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FAB_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FAB_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_FAB_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_NavRail_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_NavRail_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_NavRail_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_NavRail_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_NavRail_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_NavRail_set_items(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_NavRail_set_badges(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_NavRail_set_toggle(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_NavRail_set_selected(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_NavRail_selected(ErgoVal self);
static ErgoVal ergo_m_cogito_NavRail_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_NavRail_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_NavRail_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_NavRail_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomNav_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_BottomNav_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_BottomNav_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomNav_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomNav_set_items(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_BottomNav_set_selected(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomNav_selected(ErgoVal self);
static ErgoVal ergo_m_cogito_BottomNav_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomNav_set_disabled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomNav_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomNav_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_State_get(ErgoVal self);
static ErgoVal ergo_m_cogito_State_set(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_cogito_app(void);
static void ergo_cogito_load_sum(ErgoVal a0);
static void ergo_cogito_set_script_dir(ErgoVal a0);
static ErgoVal ergo_cogito_open_url(ErgoVal a0);
static ErgoVal ergo_cogito_set_timeout(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_set_interval(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_set_timeout_for(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito_set_interval_for(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito_clear_timer(ErgoVal a0);
static void ergo_cogito_clear_timers_for(ErgoVal a0);
static void ergo_cogito_clear_timers(void);
static void ergo_cogito_set_class(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_set_a11y_label(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_set_a11y_role(ErgoVal a0, ErgoVal a1);
static void ergo_cogito_set_tooltip(ErgoVal a0, ErgoVal a1);
static void ergo_cogito_pointer_capture(ErgoVal a0);
static void ergo_cogito_pointer_release(void);
static ErgoVal ergo_cogito_window(void);
static ErgoVal ergo_cogito_window_title(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito_window_size(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito_build(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_state(ErgoVal a0);
static void ergo_cogito_set_id(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_vstack(void);
static ErgoVal ergo_cogito_hstack(void);
static ErgoVal ergo_cogito_zstack(void);
static ErgoVal ergo_cogito_fixed(void);
static ErgoVal ergo_cogito_scroller(void);
static ErgoVal ergo_cogito_carousel(void);
static ErgoVal ergo_cogito_carousel_item(void);
static ErgoVal ergo_cogito_list(void);
static ErgoVal ergo_cogito_grid(ErgoVal a0);
static ErgoVal ergo_cogito_tabs(void);
static ErgoVal ergo_cogito_buttongroup(void);
static ErgoVal ergo_cogito_view_switcher(void);
static ErgoVal ergo_cogito_progress(ErgoVal a0);
static ErgoVal ergo_cogito_divider(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_toasts(void);
static ErgoVal ergo_cogito_toast(ErgoVal a0);
static ErgoVal ergo_cogito_label(ErgoVal a0);
static ErgoVal ergo_cogito_image(ErgoVal a0);
static ErgoVal ergo_cogito_dialog(ErgoVal a0);
static ErgoVal ergo_cogito_dialog_slot(void);
static ErgoVal ergo_cogito_button(ErgoVal a0);
static ErgoVal ergo_cogito_iconbtn(ErgoVal a0);
static ErgoVal ergo_cogito_appbar(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_checkbox(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_switch(ErgoVal a0);
static ErgoVal ergo_cogito_textfield(ErgoVal a0);
static ErgoVal ergo_cogito_searchfield(ErgoVal a0);
static ErgoVal ergo_cogito_textview(ErgoVal a0);
static ErgoVal ergo_cogito_dropdown(void);
static ErgoVal ergo_cogito_datepicker(void);
static ErgoVal ergo_cogito_stepper(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_cogito_slider(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito_slider_range(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_cogito_treeview(void);
static ErgoVal ergo_cogito_colorpicker(void);
static ErgoVal ergo_cogito_toolbar(void);
static ErgoVal ergo_cogito_chip(ErgoVal a0);
static ErgoVal ergo_cogito_fab(ErgoVal a0);
static ErgoVal ergo_cogito_nav_rail(void);
static ErgoVal ergo_cogito_bottom_nav(void);
static ErgoVal ergo_cogito_CARD_ELEVATED(void);
static ErgoVal ergo_cogito_CARD_FILLED(void);
static ErgoVal ergo_cogito_CARD_OUTLINED(void);
static ErgoVal ergo_cogito_card(ErgoVal a0);
static ErgoVal ergo_cogito_card_untitled(void);
static ErgoVal ergo_cogito_avatar(ErgoVal a0);
static ErgoVal ergo_cogito_badge(ErgoVal a0);
static ErgoVal ergo_cogito_badge_dot(void);
static ErgoVal ergo_cogito_banner(ErgoVal a0);
static ErgoVal ergo_cogito_bottom_sheet(ErgoVal a0);
static ErgoVal ergo_cogito_bottom_sheet_untitled(void);
static ErgoVal ergo_cogito_side_sheet(ErgoVal a0);
static ErgoVal ergo_cogito_side_sheet_untitled(void);
static ErgoVal ergo_cogito_timepicker(void);
static ErgoVal ergo_cogito_find_parent(ErgoVal a0);
static void ergo_cogito_dialog_slot_clear(ErgoVal a0);
static ErgoVal ergo_cogito_find_children(ErgoVal a0);
static ErgoVal ergo_cogito_active_indicator(void);
static ErgoVal ergo_cogito_switchbar(ErgoVal a0);
static ErgoVal ergo_cogito_drawing_area(void);
static ErgoVal ergo_cogito_shape(ErgoVal a0);
static ErgoVal ergo_cogito_content_list(void);
static ErgoVal ergo_cogito_empty_page(ErgoVal a0);
static ErgoVal ergo_cogito_tip_view(ErgoVal a0);
static ErgoVal ergo_cogito_settings_window(ErgoVal a0);
static ErgoVal ergo_cogito_settings_page(ErgoVal a0);
static ErgoVal ergo_cogito_settings_list(ErgoVal a0);
static ErgoVal ergo_cogito_settings_row(ErgoVal a0);
static ErgoVal ergo_cogito_welcome_screen(ErgoVal a0);
static ErgoVal ergo_cogito_view_dual(void);
static ErgoVal ergo_cogito_view_chooser(void);
static ErgoVal ergo_cogito_about_window(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_menu_button(ErgoVal a0);
static ErgoVal ergo_cogito_split_button(ErgoVal a0);
static ErgoVal ergo_cogito_fab_menu(ErgoVal a0);
static ErgoVal ergo_m_cogito_ActiveIndicator_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_ActiveIndicator_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ActiveIndicator_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SwitchBar_set_checked(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SwitchBar_get_checked(ErgoVal self);
static ErgoVal ergo_m_cogito_SwitchBar_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SwitchBar_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SwitchBar_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SwitchBar_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_SwitchBar_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SwitchBar_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SwitchBar_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DrawingArea_on_press(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DrawingArea_on_drag(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DrawingArea_on_release(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DrawingArea_on_draw(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DrawingArea_get_x(ErgoVal self);
static ErgoVal ergo_m_cogito_DrawingArea_get_y(ErgoVal self);
static ErgoVal ergo_m_cogito_DrawingArea_get_pressed(ErgoVal self);
static ErgoVal ergo_m_cogito_DrawingArea_clear(ErgoVal self);
static ErgoVal ergo_m_cogito_DrawingArea_set_color(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DrawingArea_set_line_width(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DrawingArea_line(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_DrawingArea_rect(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_DrawingArea_fill_rect(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_DrawingArea_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_DrawingArea_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DrawingArea_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DrawingArea_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_DrawingArea_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Canvas_set_color(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Canvas_set_line_width(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Canvas_line(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Canvas_rect(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Canvas_fill_rect(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Shape_set_preset(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Shape_get_preset(ErgoVal self);
static ErgoVal ergo_m_cogito_Shape_set_size(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Shape_get_size(ErgoVal self);
static ErgoVal ergo_m_cogito_Shape_set_color(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Shape_set_color_style(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Shape_get_color_style(ErgoVal self);
static ErgoVal ergo_m_cogito_Shape_set_vertex(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_m_cogito_Shape_get_vertex_x(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Shape_get_vertex_y(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Shape_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Shape_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Shape_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ContentList_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ContentList_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ContentList_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_ContentList_set_gap(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ContentList_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ContentList_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ContentList_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_EmptyPage_set_description(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_EmptyPage_set_icon(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_EmptyPage_set_action(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_EmptyPage_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_EmptyPage_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_EmptyPage_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_EmptyPage_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_EmptyPage_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_EmptyPage_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_EmptyPage_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TipView_set_title(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TipView_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_TipView_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TipView_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TipView_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsWindow_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsWindow_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsWindow_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_SettingsWindow_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsWindow_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsWindow_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsWindow_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsPage_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsPage_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsPage_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_SettingsPage_set_gap(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsPage_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsPage_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsPage_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsList_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsList_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsList_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_SettingsList_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsList_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsList_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsRow_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsRow_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsRow_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_SettingsRow_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsRow_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SettingsRow_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_WelcomeScreen_set_description(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_WelcomeScreen_set_icon(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_WelcomeScreen_set_action(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_WelcomeScreen_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_WelcomeScreen_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_WelcomeScreen_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_WelcomeScreen_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_WelcomeScreen_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_WelcomeScreen_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_WelcomeScreen_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewDual_add(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewDual_set_ratio(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewDual_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewDual_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_ViewDual_set_hexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewDual_set_vexpand(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewDual_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewDual_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewChooser_set_items(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewChooser_bind(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewChooser_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_ViewChooser_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewChooser_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AboutWindow_set_icon(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AboutWindow_set_description(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AboutWindow_set_website(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AboutWindow_set_issue_url(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AboutWindow_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_AboutWindow_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AboutWindow_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SplitButton_add_menu(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_SplitButton_add_menu_section(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_SplitButton_menu_set_icon(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SplitButton_menu_set_shortcut(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SplitButton_menu_set_submenu(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SplitButton_menu_set_toggled(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SplitButton_on_click(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SplitButton_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_SplitButton_set_class(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SplitButton_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SplitButton_set_size(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SplitButton_set_variant(ErgoVal self, ErgoVal a0);
static void ergo_entry(void);

// ---- function value defs ----
static ErgoVal __fnwrap_cogito_label(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("fn arity mismatch");
  ErgoVal arg0 = argv[0];
  return ergo_cogito_label(arg0);
}
static ErgoVal __fnwrap_cogito_dialog(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("fn arity mismatch");
  ErgoVal arg0 = argv[0];
  return ergo_cogito_dialog(arg0);
}
static ErgoVal __fnwrap_cogito_side_sheet(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("fn arity mismatch");
  ErgoVal arg0 = argv[0];
  return ergo_cogito_side_sheet(arg0);
}
static ErgoVal __fnwrap_cogito_divider(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 2) ergo_trap("fn arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal arg1 = argv[1];
  return ergo_cogito_divider(arg0, arg1);
}
static ErgoVal __fnwrap_cogito_shape(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("fn arity mismatch");
  ErgoVal arg0 = argv[0];
  return ergo_cogito_shape(arg0);
}

// ---- cask global init ----
static void ergo_init_main(void) {
  ErgoVal __t1 = ergo_cogito_app();
  ergo_move_into(&ergo_g_main_gafu_app, __t1);
  ErgoVal __t2 = ergo_cogito_colorpicker();
  ErgoVal __t3 = YV_STR(stdr_str_lit("#65558F"));
  ErgoVal __t4 = YV_NULLV;
  {
    ErgoVal __parts1[1] = { __t3 };
    ErgoStr* __s2 = stdr_str_from_parts(1, __parts1);
    __t4 = YV_STR(__s2);
  }
  ergo_release_val(__t3);
  ErgoVal __t5 = ergo_m_cogito_ColorPicker_set_hex(__t2, __t4);
  ergo_release_val(__t2);
  ergo_release_val(__t4);
  ergo_move_into(&ergo_g_main_source_picker, __t5);
  ErgoVal __t6 = YV_STR(stdr_str_lit("Choose an Image or Drop Here…"));
  ErgoVal __t7 = YV_NULLV;
  {
    ErgoVal __parts3[1] = { __t6 };
    ErgoStr* __s4 = stdr_str_from_parts(1, __parts3);
    __t7 = YV_STR(__s4);
  }
  ergo_release_val(__t6);
  ErgoVal __t8 = ergo_cogito_label(__t7);
  ergo_release_val(__t7);
  ergo_move_into(&ergo_g_main_upload_hint, __t8);
  ErgoVal __t9 = YV_STR(stdr_str_lit(""));
  ErgoVal __t10 = ergo_cogito_switch(__t9);
  ergo_release_val(__t9);
  ergo_move_into(&ergo_g_main_scheme_switch, __t10);
  ErgoVal __t11 = YV_STR(stdr_str_lit("#65558F"));
  ErgoVal __t12 = YV_NULLV;
  {
    ErgoVal __parts5[1] = { __t11 };
    ErgoStr* __s6 = stdr_str_from_parts(1, __parts5);
    __t12 = YV_STR(__s6);
  }
  ergo_release_val(__t11);
  ergo_move_into(&ergo_g_main_source_hex, __t12);
  ErgoVal __t13 = YV_STR(stdr_str_lit(""));
  ergo_move_into(&ergo_g_main_selected_image_path, __t13);
  ErgoVal __t14 = YV_STR(stdr_str_lit(""));
  ErgoVal __t15 = ergo_cogito_image(__t14);
  ergo_release_val(__t14);
  ergo_move_into(&ergo_g_main_selected_image_preview, __t15);
  ErgoVal __t16 = YV_INT(0);
  ergo_move_into(&ergo_g_main_ENSOR_DEFAULT, __t16);
  ErgoVal __t17 = YV_INT(5);
  ergo_move_into(&ergo_g_main_ENSOR_CONTENT, __t17);
  ErgoVal __t18 = YV_STR(stdr_str_lit("sf:moon"));
  ErgoVal __t19 = YV_NULLV;
  {
    ErgoVal __parts7[1] = { __t18 };
    ErgoStr* __s8 = stdr_str_from_parts(1, __parts7);
    __t19 = YV_STR(__s8);
  }
  ergo_release_val(__t18);
  ErgoVal __t20 = ergo_cogito_iconbtn(__t19);
  ergo_release_val(__t19);
  ergo_move_into(&ergo_g_main_mode_moon, __t20);
  ErgoVal __t21 = YV_STR(stdr_str_lit("sf:sun.max"));
  ErgoVal __t22 = YV_NULLV;
  {
    ErgoVal __parts9[1] = { __t21 };
    ErgoStr* __s10 = stdr_str_from_parts(1, __parts9);
    __t22 = YV_STR(__s10);
  }
  ergo_release_val(__t21);
  ErgoVal __t23 = ergo_cogito_iconbtn(__t22);
  ergo_release_val(__t22);
  ergo_move_into(&ergo_g_main_mode_base, __t23);
  ErgoVal __t24 = YV_STR(stdr_str_lit("sf:sun.lefthalf.filled"));
  ErgoVal __t25 = YV_NULLV;
  {
    ErgoVal __parts11[1] = { __t24 };
    ErgoStr* __s12 = stdr_str_from_parts(1, __parts11);
    __t25 = YV_STR(__s12);
  }
  ergo_release_val(__t24);
  ErgoVal __t26 = ergo_cogito_iconbtn(__t25);
  ergo_release_val(__t25);
  ergo_move_into(&ergo_g_main_mode_bright, __t26);
  ErgoVal __t27 = YV_STR(stdr_str_lit("sf:sun.max.fill"));
  ErgoVal __t28 = YV_NULLV;
  {
    ErgoVal __parts13[1] = { __t27 };
    ErgoStr* __s14 = stdr_str_from_parts(1, __parts13);
    __t28 = YV_STR(__s14);
  }
  ergo_release_val(__t27);
  ErgoVal __t29 = ergo_cogito_iconbtn(__t28);
  ergo_release_val(__t28);
  ergo_move_into(&ergo_g_main_mode_brightest, __t29);
  ErgoVal __t30 = YV_STR(stdr_str_lit("Dark Mode"));
  ErgoVal __t31 = YV_NULLV;
  {
    ErgoVal __parts15[1] = { __t30 };
    ErgoStr* __s16 = stdr_str_from_parts(1, __parts15);
    __t31 = YV_STR(__s16);
  }
  ergo_release_val(__t30);
  ErgoVal __t32 = ergo_cogito_switchbar(__t31);
  ergo_release_val(__t31);
  ergo_move_into(&ergo_g_main_dark_switch, __t32);
}

// ---- compiled functions ----
static void ergo_main_apply_source_color(ErgoVal a0) {
  ErgoVal __t33 = a0; ergo_retain_val(__t33);
  ErgoVal __t34 = ergo_stdr_len(__t33);
  ergo_release_val(__t33);
  ErgoVal __t35 = YV_INT(0);
  ErgoVal __t36 = ergo_eq(__t34, __t35);
  ergo_release_val(__t34);
  ergo_release_val(__t35);
  bool __b1 = ergo_as_bool(__t36);
  ergo_release_val(__t36);
  if (__b1) {
    return;
  }
  ErgoVal __t37 = a0; ergo_retain_val(__t37);
  ErgoVal __t38 = __t37; ergo_retain_val(__t38);
  ergo_move_into(&ergo_g_main_source_hex, __t37);
  ergo_release_val(__t38);
  ErgoVal __t39 = ergo_g_main_source_picker; ergo_retain_val(__t39);
  ErgoVal __t40 = ergo_g_main_source_hex; ergo_retain_val(__t40);
  ErgoVal __t41 = ergo_m_cogito_ColorPicker_set_hex(__t39, __t40);
  ergo_release_val(__t39);
  ergo_release_val(__t40);
  ergo_release_val(__t41);
  ErgoVal __t42 = ergo_g_main_gafu_app; ergo_retain_val(__t42);
  ErgoVal __t43 = ergo_g_main_source_hex; ergo_retain_val(__t43);
  ErgoVal __t44 = YV_BOOL(false);
  ErgoVal __t45 = ergo_m_cogito_App_set_accent_color(__t42, __t43, __t44);
  ergo_release_val(__t42);
  ergo_release_val(__t43);
  ergo_release_val(__t44);
  ergo_release_val(__t45);
}

static void ergo_main_choose_source_image(void) {
  ErgoVal picked__2 = YV_NULLV;
  ErgoVal __t46 = YV_STR(stdr_str_lit("Choose image"));
  ErgoVal __t47 = YV_NULLV;
  {
    ErgoVal __parts17[1] = { __t46 };
    ErgoStr* __s18 = stdr_str_from_parts(1, __parts17);
    __t47 = YV_STR(__s18);
  }
  ergo_release_val(__t46);
  ErgoVal __t48 = YV_STR(stdr_str_lit("png"));
  ErgoVal __t49 = YV_NULLV;
  {
    ErgoVal __parts19[1] = { __t48 };
    ErgoStr* __s20 = stdr_str_from_parts(1, __parts19);
    __t49 = YV_STR(__s20);
  }
  ergo_release_val(__t48);
  ErgoVal __t50 = ergo_stdr_open_file_dialog(__t47, __t49);
  ergo_release_val(__t47);
  ergo_release_val(__t49);
  ergo_move_into(&picked__2, __t50);
  ErgoVal __t51 = picked__2; ergo_retain_val(__t51);
  ErgoVal __t52 = ergo_stdr_is_null(__t51);
  ergo_release_val(__t51);
  bool __b3 = ergo_as_bool(__t52);
  ergo_release_val(__t52);
  if (__b3) {
    return;
  }
  ErgoVal path__4 = YV_NULLV;
  ErgoVal __t53 = picked__2; ergo_retain_val(__t53);
  ErgoVal __t54 = YV_STR(stdr_to_string(__t53));
  ergo_release_val(__t53);
  ergo_move_into(&path__4, __t54);
  ErgoVal __t55 = path__4; ergo_retain_val(__t55);
  ErgoVal __t56 = ergo_stdr_len(__t55);
  ergo_release_val(__t55);
  ErgoVal __t57 = YV_INT(0);
  ErgoVal __t58 = ergo_eq(__t56, __t57);
  ergo_release_val(__t56);
  ergo_release_val(__t57);
  bool __b5 = ergo_as_bool(__t58);
  ergo_release_val(__t58);
  if (__b5) {
    return;
  }
  ErgoVal __t59 = path__4; ergo_retain_val(__t59);
  ErgoVal __t60 = __t59; ergo_retain_val(__t60);
  ergo_move_into(&ergo_g_main_selected_image_path, __t59);
  ergo_release_val(__t60);
  ErgoVal __t61 = ergo_g_main_scheme_switch; ergo_retain_val(__t61);
  ErgoVal __t62 = YV_BOOL(false);
  ErgoVal __t63 = ergo_m_cogito_Switch_set_disabled(__t61, __t62);
  ergo_release_val(__t61);
  ergo_release_val(__t62);
  ergo_release_val(__t63);
  ErgoVal __t64 = ergo_g_main_upload_hint; ergo_retain_val(__t64);
  ErgoVal __t65 = YV_STR(stdr_str_lit("Image Selected."));
  ErgoVal __t66 = YV_NULLV;
  {
    ErgoVal __parts21[1] = { __t65 };
    ErgoStr* __s22 = stdr_str_from_parts(1, __parts21);
    __t66 = YV_STR(__s22);
  }
  ergo_release_val(__t65);
  ErgoVal __t67 = ergo_m_cogito_Label_set_text(__t64, __t66);
  ergo_release_val(__t64);
  ergo_release_val(__t66);
  ergo_release_val(__t67);
  ErgoVal __t68 = ergo_g_main_selected_image_preview; ergo_retain_val(__t68);
  ErgoVal __t69 = ergo_g_main_selected_image_path; ergo_retain_val(__t69);
  ErgoVal __t70 = ergo_m_cogito_Image_set_source(__t68, __t69);
  ergo_release_val(__t68);
  ergo_release_val(__t69);
  ergo_release_val(__t70);
  ErgoVal __t71 = ergo_g_main_selected_image_preview; ergo_retain_val(__t71);
  ErgoVal __t72 = YV_INT(320);
  ErgoVal __t73 = YV_INT(180);
  ErgoVal __t74 = ergo_m_cogito_Image_set_size(__t71, __t72, __t73);
  ergo_release_val(__t71);
  ergo_release_val(__t72);
  ergo_release_val(__t73);
  ergo_release_val(__t74);
  ErgoVal __t75 = ergo_g_main_selected_image_preview; ergo_retain_val(__t75);
  ErgoVal __t76 = YV_INT(26);
  ErgoVal __t77 = ergo_m_cogito_Image_set_radius(__t75, __t76);
  ergo_release_val(__t75);
  ergo_release_val(__t76);
  ergo_release_val(__t77);
  ErgoVal seeded__6 = YV_NULLV;
  ErgoVal __t78 = ergo_g_main_gafu_app; ergo_retain_val(__t78);
  ErgoVal __t79 = ergo_g_main_selected_image_path; ergo_retain_val(__t79);
  ErgoVal __t80 = YV_BOOL(false);
  ErgoVal __t81 = ergo_m_cogito_App_set_accent_from_image(__t78, __t79, __t80);
  ergo_release_val(__t78);
  ergo_release_val(__t79);
  ergo_release_val(__t80);
  ergo_move_into(&seeded__6, __t81);
  ErgoVal __t82 = seeded__6; ergo_retain_val(__t82);
  ErgoVal __t83 = YV_STR(stdr_str_lit(""));
  ErgoVal __t84 = ergo_ne(__t82, __t83);
  ergo_release_val(__t82);
  ergo_release_val(__t83);
  bool __b7 = ergo_as_bool(__t84);
  ergo_release_val(__t84);
  if (__b7) {
    ErgoVal __t85 = seeded__6; ergo_retain_val(__t85);
    ergo_main_apply_source_color(__t85);
    ergo_release_val(__t85);
    ErgoVal __t86 = YV_NULLV;
    ergo_release_val(__t86);
  }
  ergo_release_val(seeded__6);
  ergo_release_val(path__4);
  ergo_release_val(picked__2);
}

static void ergo_main_reset_everything(void) {
  ErgoVal __t87 = YV_STR(stdr_str_lit(""));
  ErgoVal __t88 = __t87; ergo_retain_val(__t88);
  ergo_move_into(&ergo_g_main_selected_image_path, __t87);
  ergo_release_val(__t88);
  ErgoVal __t89 = ergo_g_main_upload_hint; ergo_retain_val(__t89);
  ErgoVal __t90 = YV_STR(stdr_str_lit("Choose File Or Drag & Drop Here…"));
  ErgoVal __t91 = YV_NULLV;
  {
    ErgoVal __parts23[1] = { __t90 };
    ErgoStr* __s24 = stdr_str_from_parts(1, __parts23);
    __t91 = YV_STR(__s24);
  }
  ergo_release_val(__t90);
  ErgoVal __t92 = ergo_m_cogito_Label_set_text(__t89, __t91);
  ergo_release_val(__t89);
  ergo_release_val(__t91);
  ergo_release_val(__t92);
  ErgoVal __t93 = ergo_g_main_selected_image_preview; ergo_retain_val(__t93);
  ErgoVal __t94 = YV_STR(stdr_str_lit(""));
  ErgoVal __t95 = ergo_m_cogito_Image_set_source(__t93, __t94);
  ergo_release_val(__t93);
  ergo_release_val(__t94);
  ergo_release_val(__t95);
  ErgoVal __t96 = ergo_g_main_selected_image_preview; ergo_retain_val(__t96);
  ErgoVal __t97 = YV_INT(0);
  ErgoVal __t98 = YV_INT(0);
  ErgoVal __t99 = ergo_m_cogito_Image_set_size(__t96, __t97, __t98);
  ergo_release_val(__t96);
  ergo_release_val(__t97);
  ergo_release_val(__t98);
  ergo_release_val(__t99);
  ErgoVal __t100 = ergo_g_main_scheme_switch; ergo_retain_val(__t100);
  ErgoVal __t101 = YV_BOOL(false);
  ErgoVal __t102 = ergo_m_cogito_Switch_set_checked(__t100, __t101);
  ergo_release_val(__t100);
  ergo_release_val(__t101);
  ergo_release_val(__t102);
  ErgoVal __t103 = ergo_g_main_scheme_switch; ergo_retain_val(__t103);
  ErgoVal __t104 = YV_BOOL(true);
  ErgoVal __t105 = ergo_m_cogito_Switch_set_disabled(__t103, __t104);
  ergo_release_val(__t103);
  ergo_release_val(__t104);
  ergo_release_val(__t105);
  ErgoVal __t106 = ergo_g_main_gafu_app; ergo_retain_val(__t106);
  ErgoVal __t107 = ergo_g_main_ENSOR_DEFAULT; ergo_retain_val(__t107);
  ErgoVal __t108 = ergo_m_cogito_App_set_ensor_variant(__t106, __t107);
  ergo_release_val(__t106);
  ergo_release_val(__t107);
  ergo_release_val(__t108);
  ErgoVal __t109 = YV_STR(stdr_str_lit("#65558F"));
  ErgoVal __t110 = YV_NULLV;
  {
    ErgoVal __parts25[1] = { __t109 };
    ErgoStr* __s26 = stdr_str_from_parts(1, __parts25);
    __t110 = YV_STR(__s26);
  }
  ergo_release_val(__t109);
  ergo_main_apply_source_color(__t110);
  ergo_release_val(__t110);
  ErgoVal __t111 = YV_NULLV;
  ergo_release_val(__t111);
  ErgoVal __t112 = YV_INT(1);
  ErgoVal __t113 = ergo_g_main_mode_moon; ergo_retain_val(__t113);
  ErgoVal __t114 = ergo_g_main_mode_base; ergo_retain_val(__t114);
  ErgoVal __t115 = ergo_g_main_mode_bright; ergo_retain_val(__t115);
  ErgoVal __t116 = ergo_g_main_mode_brightest; ergo_retain_val(__t116);
  ergo_main_set_mode_selection(__t112, __t113, __t114, __t115, __t116);
  ergo_release_val(__t112);
  ergo_release_val(__t113);
  ergo_release_val(__t114);
  ergo_release_val(__t115);
  ergo_release_val(__t116);
  ErgoVal __t117 = YV_NULLV;
  ergo_release_val(__t117);
  ErgoVal __t118 = ergo_g_main_gafu_app; ergo_retain_val(__t118);
  ErgoVal __t119 = YV_INT(0);
  ErgoVal __t120 = ergo_m_cogito_App_set_contrast(__t118, __t119);
  ergo_release_val(__t118);
  ergo_release_val(__t119);
  ergo_release_val(__t120);
  ErgoVal __t121 = ergo_g_main_dark_switch; ergo_retain_val(__t121);
  ErgoVal __t122 = YV_BOOL(false);
  ErgoVal __t123 = ergo_m_cogito_SwitchBar_set_checked(__t121, __t122);
  ergo_release_val(__t121);
  ergo_release_val(__t122);
  ergo_release_val(__t123);
  ErgoVal __t124 = ergo_g_main_gafu_app; ergo_retain_val(__t124);
  ErgoVal __t125 = YV_BOOL(false);
  ErgoVal __t126 = YV_BOOL(false);
  ErgoVal __t127 = ergo_m_cogito_App_set_dark_mode(__t124, __t125, __t126);
  ergo_release_val(__t124);
  ergo_release_val(__t125);
  ergo_release_val(__t126);
  ergo_release_val(__t127);
}

static ErgoVal ergo_main_swatch(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal box__8 = YV_NULLV;
  ErgoVal __t128 = a0; ergo_retain_val(__t128);
  ErgoVal __t129 = ergo_cogito_button(__t128);
  ergo_release_val(__t128);
  ErgoVal __t130 = a1; ergo_retain_val(__t130);
  ErgoVal __t131 = ergo_m_cogito_Button_set_class(__t129, __t130);
  ergo_release_val(__t129);
  ergo_release_val(__t130);
  ErgoVal __t132 = YV_BOOL(true);
  ErgoVal __t133 = ergo_m_cogito_Button_set_hexpand(__t131, __t132);
  ergo_release_val(__t131);
  ergo_release_val(__t132);
  ergo_move_into(&box__8, __t133);
  ErgoVal __t134 = box__8; ergo_retain_val(__t134);
  ergo_move_into(&__ret, __t134);
  return __ret;
  ergo_release_val(box__8);
  return __ret;
}

static ErgoVal ergo_main_tone_column(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4, ErgoVal a5, ErgoVal a6, ErgoVal a7) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal col__9 = YV_NULLV;
  ErgoVal __t135 = ergo_cogito_vstack();
  ErgoVal __t136 = YV_BOOL(true);
  ErgoVal __t137 = ergo_m_cogito_VStack_set_hexpand(__t135, __t136);
  ergo_release_val(__t135);
  ergo_release_val(__t136);
  ErgoVal __t138 = YV_STR(stdr_str_lit("gafu-tone-col"));
  ErgoVal __t139 = YV_NULLV;
  {
    ErgoVal __parts27[1] = { __t138 };
    ErgoStr* __s28 = stdr_str_from_parts(1, __parts27);
    __t139 = YV_STR(__s28);
  }
  ergo_release_val(__t138);
  ErgoVal __t140 = ergo_m_cogito_VStack_set_class(__t137, __t139);
  ergo_release_val(__t137);
  ergo_release_val(__t139);
  ergo_move_into(&col__9, __t140);
  ErgoVal __t141 = col__9; ergo_retain_val(__t141);
  ErgoVal __t142 = a0; ergo_retain_val(__t142);
  ErgoVal __t143 = a1; ergo_retain_val(__t143);
  ErgoVal __t144 = ergo_main_swatch(__t142, __t143);
  ergo_release_val(__t142);
  ergo_release_val(__t143);
  ErgoVal __t145 = ergo_m_cogito_VStack_add(__t141, __t144);
  ergo_release_val(__t141);
  ergo_release_val(__t144);
  ergo_release_val(__t145);
  ErgoVal __t146 = col__9; ergo_retain_val(__t146);
  ErgoVal __t147 = a2; ergo_retain_val(__t147);
  ErgoVal __t148 = a3; ergo_retain_val(__t148);
  ErgoVal __t149 = ergo_main_swatch(__t147, __t148);
  ergo_release_val(__t147);
  ergo_release_val(__t148);
  ErgoVal __t150 = ergo_m_cogito_VStack_add(__t146, __t149);
  ergo_release_val(__t146);
  ergo_release_val(__t149);
  ergo_release_val(__t150);
  ErgoVal __t151 = col__9; ergo_retain_val(__t151);
  ErgoVal __t152 = a4; ergo_retain_val(__t152);
  ErgoVal __t153 = a5; ergo_retain_val(__t153);
  ErgoVal __t154 = ergo_main_swatch(__t152, __t153);
  ergo_release_val(__t152);
  ergo_release_val(__t153);
  ErgoVal __t155 = ergo_m_cogito_VStack_add(__t151, __t154);
  ergo_release_val(__t151);
  ergo_release_val(__t154);
  ergo_release_val(__t155);
  ErgoVal __t156 = col__9; ergo_retain_val(__t156);
  ErgoVal __t157 = a6; ergo_retain_val(__t157);
  ErgoVal __t158 = a7; ergo_retain_val(__t158);
  ErgoVal __t159 = ergo_main_swatch(__t157, __t158);
  ergo_release_val(__t157);
  ergo_release_val(__t158);
  ErgoVal __t160 = ergo_m_cogito_VStack_add(__t156, __t159);
  ergo_release_val(__t156);
  ergo_release_val(__t159);
  ergo_release_val(__t160);
  ErgoVal __t161 = col__9; ergo_retain_val(__t161);
  ergo_move_into(&__ret, __t161);
  return __ret;
  ergo_release_val(col__9);
  return __ret;
}

static ErgoVal ergo_main_fixed_column(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4, ErgoVal a5, ErgoVal a6, ErgoVal a7) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal col__10 = YV_NULLV;
  ErgoVal __t162 = ergo_cogito_vstack();
  ErgoVal __t163 = YV_BOOL(true);
  ErgoVal __t164 = ergo_m_cogito_VStack_set_hexpand(__t162, __t163);
  ergo_release_val(__t162);
  ergo_release_val(__t163);
  ergo_move_into(&col__10, __t164);
  ErgoVal top__11 = YV_NULLV;
  ErgoVal __t165 = ergo_cogito_hstack();
  ErgoVal __t166 = YV_BOOL(true);
  ErgoVal __t167 = ergo_m_cogito_HStack_set_hexpand(__t165, __t166);
  ergo_release_val(__t165);
  ergo_release_val(__t166);
  ergo_move_into(&top__11, __t167);
  ErgoVal __t168 = top__11; ergo_retain_val(__t168);
  ErgoVal __t169 = a0; ergo_retain_val(__t169);
  ErgoVal __t170 = a1; ergo_retain_val(__t170);
  ErgoVal __t171 = ergo_main_swatch(__t169, __t170);
  ergo_release_val(__t169);
  ergo_release_val(__t170);
  ErgoVal __t172 = ergo_m_cogito_HStack_add(__t168, __t171);
  ergo_release_val(__t168);
  ergo_release_val(__t171);
  ergo_release_val(__t172);
  ErgoVal __t173 = top__11; ergo_retain_val(__t173);
  ErgoVal __t174 = a2; ergo_retain_val(__t174);
  ErgoVal __t175 = a3; ergo_retain_val(__t175);
  ErgoVal __t176 = ergo_main_swatch(__t174, __t175);
  ergo_release_val(__t174);
  ergo_release_val(__t175);
  ErgoVal __t177 = ergo_m_cogito_HStack_add(__t173, __t176);
  ergo_release_val(__t173);
  ergo_release_val(__t176);
  ergo_release_val(__t177);
  ErgoVal __t178 = col__10; ergo_retain_val(__t178);
  ErgoVal __t179 = top__11; ergo_retain_val(__t179);
  ErgoVal __t180 = ergo_m_cogito_VStack_add(__t178, __t179);
  ergo_release_val(__t178);
  ergo_release_val(__t179);
  ergo_release_val(__t180);
  ErgoVal __t181 = col__10; ergo_retain_val(__t181);
  ErgoVal __t182 = a4; ergo_retain_val(__t182);
  ErgoVal __t183 = a5; ergo_retain_val(__t183);
  ErgoVal __t184 = ergo_main_swatch(__t182, __t183);
  ergo_release_val(__t182);
  ergo_release_val(__t183);
  ErgoVal __t185 = ergo_m_cogito_VStack_add(__t181, __t184);
  ergo_release_val(__t181);
  ergo_release_val(__t184);
  ergo_release_val(__t185);
  ErgoVal __t186 = col__10; ergo_retain_val(__t186);
  ErgoVal __t187 = a6; ergo_retain_val(__t187);
  ErgoVal __t188 = a7; ergo_retain_val(__t188);
  ErgoVal __t189 = ergo_main_swatch(__t187, __t188);
  ergo_release_val(__t187);
  ergo_release_val(__t188);
  ErgoVal __t190 = ergo_m_cogito_VStack_add(__t186, __t189);
  ergo_release_val(__t186);
  ergo_release_val(__t189);
  ergo_release_val(__t190);
  ErgoVal __t191 = col__10; ergo_retain_val(__t191);
  ergo_move_into(&__ret, __t191);
  return __ret;
  ergo_release_val(top__11);
  ergo_release_val(col__10);
  return __ret;
}

static void ergo_main_set_mode_selection(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4) {
  ErgoVal __t192 = a1; ergo_retain_val(__t192);
  ErgoVal __t193 = a0; ergo_retain_val(__t193);
  ErgoVal __t194 = YV_INT(0);
  ErgoVal __t195 = ergo_eq(__t193, __t194);
  ergo_release_val(__t193);
  ergo_release_val(__t194);
  ErgoVal __t196 = ergo_m_cogito_IconButton_set_checked(__t192, __t195);
  ergo_release_val(__t192);
  ergo_release_val(__t195);
  ergo_release_val(__t196);
  ErgoVal __t197 = a2; ergo_retain_val(__t197);
  ErgoVal __t198 = a0; ergo_retain_val(__t198);
  ErgoVal __t199 = YV_INT(1);
  ErgoVal __t200 = ergo_eq(__t198, __t199);
  ergo_release_val(__t198);
  ergo_release_val(__t199);
  ErgoVal __t201 = ergo_m_cogito_IconButton_set_checked(__t197, __t200);
  ergo_release_val(__t197);
  ergo_release_val(__t200);
  ergo_release_val(__t201);
  ErgoVal __t202 = a3; ergo_retain_val(__t202);
  ErgoVal __t203 = a0; ergo_retain_val(__t203);
  ErgoVal __t204 = YV_INT(2);
  ErgoVal __t205 = ergo_eq(__t203, __t204);
  ergo_release_val(__t203);
  ergo_release_val(__t204);
  ErgoVal __t206 = ergo_m_cogito_IconButton_set_checked(__t202, __t205);
  ergo_release_val(__t202);
  ergo_release_val(__t205);
  ergo_release_val(__t206);
  ErgoVal __t207 = a4; ergo_retain_val(__t207);
  ErgoVal __t208 = a0; ergo_retain_val(__t208);
  ErgoVal __t209 = YV_INT(3);
  ErgoVal __t210 = ergo_eq(__t208, __t209);
  ergo_release_val(__t208);
  ergo_release_val(__t209);
  ErgoVal __t211 = ergo_m_cogito_IconButton_set_checked(__t207, __t210);
  ergo_release_val(__t207);
  ergo_release_val(__t210);
  ergo_release_val(__t211);
  ErgoVal __t212 = a1; ergo_retain_val(__t212);
  ErgoVal __t213 = YV_NULLV;
  ErgoVal __t214 = a0; ergo_retain_val(__t214);
  ErgoVal __t215 = YV_INT(0);
  ErgoVal __t216 = ergo_eq(__t214, __t215);
  ergo_release_val(__t214);
  ergo_release_val(__t215);
  bool __b12 = ergo_as_bool(__t216);
  ergo_release_val(__t216);
  if (__b12) {
    ErgoVal __t217 = YV_STR(stdr_str_lit("filled"));
    ErgoVal __t218 = YV_NULLV;
    {
      ErgoVal __parts29[1] = { __t217 };
      ErgoStr* __s30 = stdr_str_from_parts(1, __parts29);
      __t218 = YV_STR(__s30);
    }
    ergo_release_val(__t217);
    ergo_move_into(&__t213, __t218);
  } else {
    ErgoVal __t219 = YV_STR(stdr_str_lit("tonal"));
    ErgoVal __t220 = YV_NULLV;
    {
      ErgoVal __parts31[1] = { __t219 };
      ErgoStr* __s32 = stdr_str_from_parts(1, __parts31);
      __t220 = YV_STR(__s32);
    }
    ergo_release_val(__t219);
    ergo_move_into(&__t213, __t220);
  }
  ErgoVal __t221 = ergo_m_cogito_IconButton_set_class(__t212, __t213);
  ergo_release_val(__t212);
  ergo_release_val(__t213);
  ergo_release_val(__t221);
  ErgoVal __t222 = a2; ergo_retain_val(__t222);
  ErgoVal __t223 = YV_NULLV;
  ErgoVal __t224 = a0; ergo_retain_val(__t224);
  ErgoVal __t225 = YV_INT(1);
  ErgoVal __t226 = ergo_eq(__t224, __t225);
  ergo_release_val(__t224);
  ergo_release_val(__t225);
  bool __b13 = ergo_as_bool(__t226);
  ergo_release_val(__t226);
  if (__b13) {
    ErgoVal __t227 = YV_STR(stdr_str_lit("filled"));
    ErgoVal __t228 = YV_NULLV;
    {
      ErgoVal __parts33[1] = { __t227 };
      ErgoStr* __s34 = stdr_str_from_parts(1, __parts33);
      __t228 = YV_STR(__s34);
    }
    ergo_release_val(__t227);
    ergo_move_into(&__t223, __t228);
  } else {
    ErgoVal __t229 = YV_STR(stdr_str_lit("tonal"));
    ErgoVal __t230 = YV_NULLV;
    {
      ErgoVal __parts35[1] = { __t229 };
      ErgoStr* __s36 = stdr_str_from_parts(1, __parts35);
      __t230 = YV_STR(__s36);
    }
    ergo_release_val(__t229);
    ergo_move_into(&__t223, __t230);
  }
  ErgoVal __t231 = ergo_m_cogito_IconButton_set_class(__t222, __t223);
  ergo_release_val(__t222);
  ergo_release_val(__t223);
  ergo_release_val(__t231);
  ErgoVal __t232 = a3; ergo_retain_val(__t232);
  ErgoVal __t233 = YV_NULLV;
  ErgoVal __t234 = a0; ergo_retain_val(__t234);
  ErgoVal __t235 = YV_INT(2);
  ErgoVal __t236 = ergo_eq(__t234, __t235);
  ergo_release_val(__t234);
  ergo_release_val(__t235);
  bool __b14 = ergo_as_bool(__t236);
  ergo_release_val(__t236);
  if (__b14) {
    ErgoVal __t237 = YV_STR(stdr_str_lit("filled"));
    ErgoVal __t238 = YV_NULLV;
    {
      ErgoVal __parts37[1] = { __t237 };
      ErgoStr* __s38 = stdr_str_from_parts(1, __parts37);
      __t238 = YV_STR(__s38);
    }
    ergo_release_val(__t237);
    ergo_move_into(&__t233, __t238);
  } else {
    ErgoVal __t239 = YV_STR(stdr_str_lit("tonal"));
    ErgoVal __t240 = YV_NULLV;
    {
      ErgoVal __parts39[1] = { __t239 };
      ErgoStr* __s40 = stdr_str_from_parts(1, __parts39);
      __t240 = YV_STR(__s40);
    }
    ergo_release_val(__t239);
    ergo_move_into(&__t233, __t240);
  }
  ErgoVal __t241 = ergo_m_cogito_IconButton_set_class(__t232, __t233);
  ergo_release_val(__t232);
  ergo_release_val(__t233);
  ergo_release_val(__t241);
  ErgoVal __t242 = a4; ergo_retain_val(__t242);
  ErgoVal __t243 = YV_NULLV;
  ErgoVal __t244 = a0; ergo_retain_val(__t244);
  ErgoVal __t245 = YV_INT(3);
  ErgoVal __t246 = ergo_eq(__t244, __t245);
  ergo_release_val(__t244);
  ergo_release_val(__t245);
  bool __b15 = ergo_as_bool(__t246);
  ergo_release_val(__t246);
  if (__b15) {
    ErgoVal __t247 = YV_STR(stdr_str_lit("filled"));
    ErgoVal __t248 = YV_NULLV;
    {
      ErgoVal __parts41[1] = { __t247 };
      ErgoStr* __s42 = stdr_str_from_parts(1, __parts41);
      __t248 = YV_STR(__s42);
    }
    ergo_release_val(__t247);
    ergo_move_into(&__t243, __t248);
  } else {
    ErgoVal __t249 = YV_STR(stdr_str_lit("tonal"));
    ErgoVal __t250 = YV_NULLV;
    {
      ErgoVal __parts43[1] = { __t249 };
      ErgoStr* __s44 = stdr_str_from_parts(1, __parts43);
      __t250 = YV_STR(__s44);
    }
    ergo_release_val(__t249);
    ergo_move_into(&__t243, __t250);
  }
  ErgoVal __t251 = ergo_m_cogito_IconButton_set_class(__t242, __t243);
  ergo_release_val(__t242);
  ergo_release_val(__t243);
  ergo_release_val(__t251);
}

static ErgoVal ergo_main_build_palette_grid(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal group__16 = YV_NULLV;
  ErgoVal __t252 = ergo_cogito_vstack();
  ErgoVal __t253 = YV_BOOL(true);
  ErgoVal __t254 = ergo_m_cogito_VStack_set_hexpand(__t252, __t253);
  ergo_release_val(__t252);
  ergo_release_val(__t253);
  ErgoVal __t255 = YV_INT(12);
  ErgoVal __t256 = YV_INT(18);
  ErgoVal __t257 = YV_INT(18);
  ErgoVal __t258 = YV_INT(18);
  ErgoVal __t259 = ergo_m_cogito_VStack_set_padding(__t254, __t255, __t256, __t257, __t258);
  ergo_release_val(__t254);
  ergo_release_val(__t255);
  ergo_release_val(__t256);
  ergo_release_val(__t257);
  ergo_release_val(__t258);
  ergo_move_into(&group__16, __t259);
  ErgoVal tones__17 = YV_NULLV;
  ErgoVal __t260 = ergo_cogito_hstack();
  ErgoVal __t261 = YV_INT(12);
  ErgoVal __t262 = ergo_m_cogito_HStack_set_gap(__t260, __t261);
  ergo_release_val(__t260);
  ergo_release_val(__t261);
  ErgoVal __t263 = YV_BOOL(true);
  ErgoVal __t264 = ergo_m_cogito_HStack_set_hexpand(__t262, __t263);
  ergo_release_val(__t262);
  ergo_release_val(__t263);
  ErgoVal __t265 = YV_STR(stdr_str_lit("Primary"));
  ErgoVal __t266 = YV_NULLV;
  {
    ErgoVal __parts45[1] = { __t265 };
    ErgoStr* __s46 = stdr_str_from_parts(1, __parts45);
    __t266 = YV_STR(__s46);
  }
  ergo_release_val(__t265);
  ErgoVal __t267 = YV_STR(stdr_str_lit("primary"));
  ErgoVal __t268 = YV_NULLV;
  {
    ErgoVal __parts47[1] = { __t267 };
    ErgoStr* __s48 = stdr_str_from_parts(1, __parts47);
    __t268 = YV_STR(__s48);
  }
  ergo_release_val(__t267);
  ErgoVal __t269 = YV_STR(stdr_str_lit("On Primary"));
  ErgoVal __t270 = YV_NULLV;
  {
    ErgoVal __parts49[1] = { __t269 };
    ErgoStr* __s50 = stdr_str_from_parts(1, __parts49);
    __t270 = YV_STR(__s50);
  }
  ergo_release_val(__t269);
  ErgoVal __t271 = YV_STR(stdr_str_lit("on-primary"));
  ErgoVal __t272 = YV_NULLV;
  {
    ErgoVal __parts51[1] = { __t271 };
    ErgoStr* __s52 = stdr_str_from_parts(1, __parts51);
    __t272 = YV_STR(__s52);
  }
  ergo_release_val(__t271);
  ErgoVal __t273 = YV_STR(stdr_str_lit("Primary Container"));
  ErgoVal __t274 = YV_NULLV;
  {
    ErgoVal __parts53[1] = { __t273 };
    ErgoStr* __s54 = stdr_str_from_parts(1, __parts53);
    __t274 = YV_STR(__s54);
  }
  ergo_release_val(__t273);
  ErgoVal __t275 = YV_STR(stdr_str_lit("primary-container"));
  ErgoVal __t276 = YV_NULLV;
  {
    ErgoVal __parts55[1] = { __t275 };
    ErgoStr* __s56 = stdr_str_from_parts(1, __parts55);
    __t276 = YV_STR(__s56);
  }
  ergo_release_val(__t275);
  ErgoVal __t277 = YV_STR(stdr_str_lit("On Primary Container"));
  ErgoVal __t278 = YV_NULLV;
  {
    ErgoVal __parts57[1] = { __t277 };
    ErgoStr* __s58 = stdr_str_from_parts(1, __parts57);
    __t278 = YV_STR(__s58);
  }
  ergo_release_val(__t277);
  ErgoVal __t279 = YV_STR(stdr_str_lit("on-primary-container"));
  ErgoVal __t280 = YV_NULLV;
  {
    ErgoVal __parts59[1] = { __t279 };
    ErgoStr* __s60 = stdr_str_from_parts(1, __parts59);
    __t280 = YV_STR(__s60);
  }
  ergo_release_val(__t279);
  ErgoVal __t281 = ergo_main_tone_column(__t266, __t268, __t270, __t272, __t274, __t276, __t278, __t280);
  ergo_release_val(__t266);
  ergo_release_val(__t268);
  ergo_release_val(__t270);
  ergo_release_val(__t272);
  ergo_release_val(__t274);
  ergo_release_val(__t276);
  ergo_release_val(__t278);
  ergo_release_val(__t280);
  ErgoVal __t282 = ergo_m_cogito_HStack_add(__t264, __t281);
  ergo_release_val(__t264);
  ergo_release_val(__t281);
  ErgoVal __t283 = YV_STR(stdr_str_lit("Secondary"));
  ErgoVal __t284 = YV_NULLV;
  {
    ErgoVal __parts61[1] = { __t283 };
    ErgoStr* __s62 = stdr_str_from_parts(1, __parts61);
    __t284 = YV_STR(__s62);
  }
  ergo_release_val(__t283);
  ErgoVal __t285 = YV_STR(stdr_str_lit("secondary"));
  ErgoVal __t286 = YV_NULLV;
  {
    ErgoVal __parts63[1] = { __t285 };
    ErgoStr* __s64 = stdr_str_from_parts(1, __parts63);
    __t286 = YV_STR(__s64);
  }
  ergo_release_val(__t285);
  ErgoVal __t287 = YV_STR(stdr_str_lit("On Secondary"));
  ErgoVal __t288 = YV_NULLV;
  {
    ErgoVal __parts65[1] = { __t287 };
    ErgoStr* __s66 = stdr_str_from_parts(1, __parts65);
    __t288 = YV_STR(__s66);
  }
  ergo_release_val(__t287);
  ErgoVal __t289 = YV_STR(stdr_str_lit("on-secondary"));
  ErgoVal __t290 = YV_NULLV;
  {
    ErgoVal __parts67[1] = { __t289 };
    ErgoStr* __s68 = stdr_str_from_parts(1, __parts67);
    __t290 = YV_STR(__s68);
  }
  ergo_release_val(__t289);
  ErgoVal __t291 = YV_STR(stdr_str_lit("Secondary Container"));
  ErgoVal __t292 = YV_NULLV;
  {
    ErgoVal __parts69[1] = { __t291 };
    ErgoStr* __s70 = stdr_str_from_parts(1, __parts69);
    __t292 = YV_STR(__s70);
  }
  ergo_release_val(__t291);
  ErgoVal __t293 = YV_STR(stdr_str_lit("secondary-container"));
  ErgoVal __t294 = YV_NULLV;
  {
    ErgoVal __parts71[1] = { __t293 };
    ErgoStr* __s72 = stdr_str_from_parts(1, __parts71);
    __t294 = YV_STR(__s72);
  }
  ergo_release_val(__t293);
  ErgoVal __t295 = YV_STR(stdr_str_lit("On Secondary Container"));
  ErgoVal __t296 = YV_NULLV;
  {
    ErgoVal __parts73[1] = { __t295 };
    ErgoStr* __s74 = stdr_str_from_parts(1, __parts73);
    __t296 = YV_STR(__s74);
  }
  ergo_release_val(__t295);
  ErgoVal __t297 = YV_STR(stdr_str_lit("on-secondary-container"));
  ErgoVal __t298 = YV_NULLV;
  {
    ErgoVal __parts75[1] = { __t297 };
    ErgoStr* __s76 = stdr_str_from_parts(1, __parts75);
    __t298 = YV_STR(__s76);
  }
  ergo_release_val(__t297);
  ErgoVal __t299 = ergo_main_tone_column(__t284, __t286, __t288, __t290, __t292, __t294, __t296, __t298);
  ergo_release_val(__t284);
  ergo_release_val(__t286);
  ergo_release_val(__t288);
  ergo_release_val(__t290);
  ergo_release_val(__t292);
  ergo_release_val(__t294);
  ergo_release_val(__t296);
  ergo_release_val(__t298);
  ErgoVal __t300 = ergo_m_cogito_HStack_add(__t282, __t299);
  ergo_release_val(__t282);
  ergo_release_val(__t299);
  ErgoVal __t301 = YV_STR(stdr_str_lit("Tertiary"));
  ErgoVal __t302 = YV_NULLV;
  {
    ErgoVal __parts77[1] = { __t301 };
    ErgoStr* __s78 = stdr_str_from_parts(1, __parts77);
    __t302 = YV_STR(__s78);
  }
  ergo_release_val(__t301);
  ErgoVal __t303 = YV_STR(stdr_str_lit("tertiary"));
  ErgoVal __t304 = YV_NULLV;
  {
    ErgoVal __parts79[1] = { __t303 };
    ErgoStr* __s80 = stdr_str_from_parts(1, __parts79);
    __t304 = YV_STR(__s80);
  }
  ergo_release_val(__t303);
  ErgoVal __t305 = YV_STR(stdr_str_lit("On Tertiary"));
  ErgoVal __t306 = YV_NULLV;
  {
    ErgoVal __parts81[1] = { __t305 };
    ErgoStr* __s82 = stdr_str_from_parts(1, __parts81);
    __t306 = YV_STR(__s82);
  }
  ergo_release_val(__t305);
  ErgoVal __t307 = YV_STR(stdr_str_lit("on-tertiary"));
  ErgoVal __t308 = YV_NULLV;
  {
    ErgoVal __parts83[1] = { __t307 };
    ErgoStr* __s84 = stdr_str_from_parts(1, __parts83);
    __t308 = YV_STR(__s84);
  }
  ergo_release_val(__t307);
  ErgoVal __t309 = YV_STR(stdr_str_lit("Tertiary Container"));
  ErgoVal __t310 = YV_NULLV;
  {
    ErgoVal __parts85[1] = { __t309 };
    ErgoStr* __s86 = stdr_str_from_parts(1, __parts85);
    __t310 = YV_STR(__s86);
  }
  ergo_release_val(__t309);
  ErgoVal __t311 = YV_STR(stdr_str_lit("tertiary-container"));
  ErgoVal __t312 = YV_NULLV;
  {
    ErgoVal __parts87[1] = { __t311 };
    ErgoStr* __s88 = stdr_str_from_parts(1, __parts87);
    __t312 = YV_STR(__s88);
  }
  ergo_release_val(__t311);
  ErgoVal __t313 = YV_STR(stdr_str_lit("On Tertiary Container"));
  ErgoVal __t314 = YV_NULLV;
  {
    ErgoVal __parts89[1] = { __t313 };
    ErgoStr* __s90 = stdr_str_from_parts(1, __parts89);
    __t314 = YV_STR(__s90);
  }
  ergo_release_val(__t313);
  ErgoVal __t315 = YV_STR(stdr_str_lit("on-tertiary-container"));
  ErgoVal __t316 = YV_NULLV;
  {
    ErgoVal __parts91[1] = { __t315 };
    ErgoStr* __s92 = stdr_str_from_parts(1, __parts91);
    __t316 = YV_STR(__s92);
  }
  ergo_release_val(__t315);
  ErgoVal __t317 = ergo_main_tone_column(__t302, __t304, __t306, __t308, __t310, __t312, __t314, __t316);
  ergo_release_val(__t302);
  ergo_release_val(__t304);
  ergo_release_val(__t306);
  ergo_release_val(__t308);
  ergo_release_val(__t310);
  ergo_release_val(__t312);
  ergo_release_val(__t314);
  ergo_release_val(__t316);
  ErgoVal __t318 = ergo_m_cogito_HStack_add(__t300, __t317);
  ergo_release_val(__t300);
  ergo_release_val(__t317);
  ErgoVal __t319 = YV_STR(stdr_str_lit("Error"));
  ErgoVal __t320 = YV_NULLV;
  {
    ErgoVal __parts93[1] = { __t319 };
    ErgoStr* __s94 = stdr_str_from_parts(1, __parts93);
    __t320 = YV_STR(__s94);
  }
  ergo_release_val(__t319);
  ErgoVal __t321 = YV_STR(stdr_str_lit("error"));
  ErgoVal __t322 = YV_NULLV;
  {
    ErgoVal __parts95[1] = { __t321 };
    ErgoStr* __s96 = stdr_str_from_parts(1, __parts95);
    __t322 = YV_STR(__s96);
  }
  ergo_release_val(__t321);
  ErgoVal __t323 = YV_STR(stdr_str_lit("On Error"));
  ErgoVal __t324 = YV_NULLV;
  {
    ErgoVal __parts97[1] = { __t323 };
    ErgoStr* __s98 = stdr_str_from_parts(1, __parts97);
    __t324 = YV_STR(__s98);
  }
  ergo_release_val(__t323);
  ErgoVal __t325 = YV_STR(stdr_str_lit("on-error"));
  ErgoVal __t326 = YV_NULLV;
  {
    ErgoVal __parts99[1] = { __t325 };
    ErgoStr* __s100 = stdr_str_from_parts(1, __parts99);
    __t326 = YV_STR(__s100);
  }
  ergo_release_val(__t325);
  ErgoVal __t327 = YV_STR(stdr_str_lit("Error Container"));
  ErgoVal __t328 = YV_NULLV;
  {
    ErgoVal __parts101[1] = { __t327 };
    ErgoStr* __s102 = stdr_str_from_parts(1, __parts101);
    __t328 = YV_STR(__s102);
  }
  ergo_release_val(__t327);
  ErgoVal __t329 = YV_STR(stdr_str_lit("error-container"));
  ErgoVal __t330 = YV_NULLV;
  {
    ErgoVal __parts103[1] = { __t329 };
    ErgoStr* __s104 = stdr_str_from_parts(1, __parts103);
    __t330 = YV_STR(__s104);
  }
  ergo_release_val(__t329);
  ErgoVal __t331 = YV_STR(stdr_str_lit("On Error Container"));
  ErgoVal __t332 = YV_NULLV;
  {
    ErgoVal __parts105[1] = { __t331 };
    ErgoStr* __s106 = stdr_str_from_parts(1, __parts105);
    __t332 = YV_STR(__s106);
  }
  ergo_release_val(__t331);
  ErgoVal __t333 = YV_STR(stdr_str_lit("on-error-container"));
  ErgoVal __t334 = YV_NULLV;
  {
    ErgoVal __parts107[1] = { __t333 };
    ErgoStr* __s108 = stdr_str_from_parts(1, __parts107);
    __t334 = YV_STR(__s108);
  }
  ergo_release_val(__t333);
  ErgoVal __t335 = ergo_main_tone_column(__t320, __t322, __t324, __t326, __t328, __t330, __t332, __t334);
  ergo_release_val(__t320);
  ergo_release_val(__t322);
  ergo_release_val(__t324);
  ergo_release_val(__t326);
  ergo_release_val(__t328);
  ergo_release_val(__t330);
  ergo_release_val(__t332);
  ergo_release_val(__t334);
  ErgoVal __t336 = ergo_m_cogito_HStack_add(__t318, __t335);
  ergo_release_val(__t318);
  ergo_release_val(__t335);
  ergo_move_into(&tones__17, __t336);
  ErgoVal fixed__18 = YV_NULLV;
  ErgoVal __t337 = ergo_cogito_hstack();
  ErgoVal __t338 = YV_INT(12);
  ErgoVal __t339 = ergo_m_cogito_HStack_set_gap(__t337, __t338);
  ergo_release_val(__t337);
  ergo_release_val(__t338);
  ErgoVal __t340 = YV_BOOL(true);
  ErgoVal __t341 = ergo_m_cogito_HStack_set_hexpand(__t339, __t340);
  ergo_release_val(__t339);
  ergo_release_val(__t340);
  ErgoVal __t342 = YV_STR(stdr_str_lit("Primary Fixed"));
  ErgoVal __t343 = YV_NULLV;
  {
    ErgoVal __parts109[1] = { __t342 };
    ErgoStr* __s110 = stdr_str_from_parts(1, __parts109);
    __t343 = YV_STR(__s110);
  }
  ergo_release_val(__t342);
  ErgoVal __t344 = YV_STR(stdr_str_lit("primary-fixed"));
  ErgoVal __t345 = YV_NULLV;
  {
    ErgoVal __parts111[1] = { __t344 };
    ErgoStr* __s112 = stdr_str_from_parts(1, __parts111);
    __t345 = YV_STR(__s112);
  }
  ergo_release_val(__t344);
  ErgoVal __t346 = YV_STR(stdr_str_lit("Primary Fixed Dim"));
  ErgoVal __t347 = YV_NULLV;
  {
    ErgoVal __parts113[1] = { __t346 };
    ErgoStr* __s114 = stdr_str_from_parts(1, __parts113);
    __t347 = YV_STR(__s114);
  }
  ergo_release_val(__t346);
  ErgoVal __t348 = YV_STR(stdr_str_lit("primary-fixed-dim"));
  ErgoVal __t349 = YV_NULLV;
  {
    ErgoVal __parts115[1] = { __t348 };
    ErgoStr* __s116 = stdr_str_from_parts(1, __parts115);
    __t349 = YV_STR(__s116);
  }
  ergo_release_val(__t348);
  ErgoVal __t350 = YV_STR(stdr_str_lit("On Primary Fixed"));
  ErgoVal __t351 = YV_NULLV;
  {
    ErgoVal __parts117[1] = { __t350 };
    ErgoStr* __s118 = stdr_str_from_parts(1, __parts117);
    __t351 = YV_STR(__s118);
  }
  ergo_release_val(__t350);
  ErgoVal __t352 = YV_STR(stdr_str_lit("on-primary-fixed"));
  ErgoVal __t353 = YV_NULLV;
  {
    ErgoVal __parts119[1] = { __t352 };
    ErgoStr* __s120 = stdr_str_from_parts(1, __parts119);
    __t353 = YV_STR(__s120);
  }
  ergo_release_val(__t352);
  ErgoVal __t354 = YV_STR(stdr_str_lit("On Primary Fixed Variant"));
  ErgoVal __t355 = YV_NULLV;
  {
    ErgoVal __parts121[1] = { __t354 };
    ErgoStr* __s122 = stdr_str_from_parts(1, __parts121);
    __t355 = YV_STR(__s122);
  }
  ergo_release_val(__t354);
  ErgoVal __t356 = YV_STR(stdr_str_lit("on-primary-fixed-variant"));
  ErgoVal __t357 = YV_NULLV;
  {
    ErgoVal __parts123[1] = { __t356 };
    ErgoStr* __s124 = stdr_str_from_parts(1, __parts123);
    __t357 = YV_STR(__s124);
  }
  ergo_release_val(__t356);
  ErgoVal __t358 = ergo_main_fixed_column(__t343, __t345, __t347, __t349, __t351, __t353, __t355, __t357);
  ergo_release_val(__t343);
  ergo_release_val(__t345);
  ergo_release_val(__t347);
  ergo_release_val(__t349);
  ergo_release_val(__t351);
  ergo_release_val(__t353);
  ergo_release_val(__t355);
  ergo_release_val(__t357);
  ErgoVal __t359 = ergo_m_cogito_HStack_add(__t341, __t358);
  ergo_release_val(__t341);
  ergo_release_val(__t358);
  ErgoVal __t360 = YV_STR(stdr_str_lit("Secondary Fixed"));
  ErgoVal __t361 = YV_NULLV;
  {
    ErgoVal __parts125[1] = { __t360 };
    ErgoStr* __s126 = stdr_str_from_parts(1, __parts125);
    __t361 = YV_STR(__s126);
  }
  ergo_release_val(__t360);
  ErgoVal __t362 = YV_STR(stdr_str_lit("secondary-fixed"));
  ErgoVal __t363 = YV_NULLV;
  {
    ErgoVal __parts127[1] = { __t362 };
    ErgoStr* __s128 = stdr_str_from_parts(1, __parts127);
    __t363 = YV_STR(__s128);
  }
  ergo_release_val(__t362);
  ErgoVal __t364 = YV_STR(stdr_str_lit("Secondary Fixed Dim"));
  ErgoVal __t365 = YV_NULLV;
  {
    ErgoVal __parts129[1] = { __t364 };
    ErgoStr* __s130 = stdr_str_from_parts(1, __parts129);
    __t365 = YV_STR(__s130);
  }
  ergo_release_val(__t364);
  ErgoVal __t366 = YV_STR(stdr_str_lit("secondary-fixed-dim"));
  ErgoVal __t367 = YV_NULLV;
  {
    ErgoVal __parts131[1] = { __t366 };
    ErgoStr* __s132 = stdr_str_from_parts(1, __parts131);
    __t367 = YV_STR(__s132);
  }
  ergo_release_val(__t366);
  ErgoVal __t368 = YV_STR(stdr_str_lit("On Secondary Fixed"));
  ErgoVal __t369 = YV_NULLV;
  {
    ErgoVal __parts133[1] = { __t368 };
    ErgoStr* __s134 = stdr_str_from_parts(1, __parts133);
    __t369 = YV_STR(__s134);
  }
  ergo_release_val(__t368);
  ErgoVal __t370 = YV_STR(stdr_str_lit("on-secondary-fixed"));
  ErgoVal __t371 = YV_NULLV;
  {
    ErgoVal __parts135[1] = { __t370 };
    ErgoStr* __s136 = stdr_str_from_parts(1, __parts135);
    __t371 = YV_STR(__s136);
  }
  ergo_release_val(__t370);
  ErgoVal __t372 = YV_STR(stdr_str_lit("On Secondary Fixed Variant"));
  ErgoVal __t373 = YV_NULLV;
  {
    ErgoVal __parts137[1] = { __t372 };
    ErgoStr* __s138 = stdr_str_from_parts(1, __parts137);
    __t373 = YV_STR(__s138);
  }
  ergo_release_val(__t372);
  ErgoVal __t374 = YV_STR(stdr_str_lit("on-secondary-fixed-variant"));
  ErgoVal __t375 = YV_NULLV;
  {
    ErgoVal __parts139[1] = { __t374 };
    ErgoStr* __s140 = stdr_str_from_parts(1, __parts139);
    __t375 = YV_STR(__s140);
  }
  ergo_release_val(__t374);
  ErgoVal __t376 = ergo_main_fixed_column(__t361, __t363, __t365, __t367, __t369, __t371, __t373, __t375);
  ergo_release_val(__t361);
  ergo_release_val(__t363);
  ergo_release_val(__t365);
  ergo_release_val(__t367);
  ergo_release_val(__t369);
  ergo_release_val(__t371);
  ergo_release_val(__t373);
  ergo_release_val(__t375);
  ErgoVal __t377 = ergo_m_cogito_HStack_add(__t359, __t376);
  ergo_release_val(__t359);
  ergo_release_val(__t376);
  ErgoVal __t378 = YV_STR(stdr_str_lit("Tertiary Fixed"));
  ErgoVal __t379 = YV_NULLV;
  {
    ErgoVal __parts141[1] = { __t378 };
    ErgoStr* __s142 = stdr_str_from_parts(1, __parts141);
    __t379 = YV_STR(__s142);
  }
  ergo_release_val(__t378);
  ErgoVal __t380 = YV_STR(stdr_str_lit("tertiary-fixed"));
  ErgoVal __t381 = YV_NULLV;
  {
    ErgoVal __parts143[1] = { __t380 };
    ErgoStr* __s144 = stdr_str_from_parts(1, __parts143);
    __t381 = YV_STR(__s144);
  }
  ergo_release_val(__t380);
  ErgoVal __t382 = YV_STR(stdr_str_lit("Tertiary Fixed Dim"));
  ErgoVal __t383 = YV_NULLV;
  {
    ErgoVal __parts145[1] = { __t382 };
    ErgoStr* __s146 = stdr_str_from_parts(1, __parts145);
    __t383 = YV_STR(__s146);
  }
  ergo_release_val(__t382);
  ErgoVal __t384 = YV_STR(stdr_str_lit("tertiary-fixed-dim"));
  ErgoVal __t385 = YV_NULLV;
  {
    ErgoVal __parts147[1] = { __t384 };
    ErgoStr* __s148 = stdr_str_from_parts(1, __parts147);
    __t385 = YV_STR(__s148);
  }
  ergo_release_val(__t384);
  ErgoVal __t386 = YV_STR(stdr_str_lit("On Tertiary Fixed"));
  ErgoVal __t387 = YV_NULLV;
  {
    ErgoVal __parts149[1] = { __t386 };
    ErgoStr* __s150 = stdr_str_from_parts(1, __parts149);
    __t387 = YV_STR(__s150);
  }
  ergo_release_val(__t386);
  ErgoVal __t388 = YV_STR(stdr_str_lit("on-tertiary-fixed"));
  ErgoVal __t389 = YV_NULLV;
  {
    ErgoVal __parts151[1] = { __t388 };
    ErgoStr* __s152 = stdr_str_from_parts(1, __parts151);
    __t389 = YV_STR(__s152);
  }
  ergo_release_val(__t388);
  ErgoVal __t390 = YV_STR(stdr_str_lit("On Tertiary Fixed Variant"));
  ErgoVal __t391 = YV_NULLV;
  {
    ErgoVal __parts153[1] = { __t390 };
    ErgoStr* __s154 = stdr_str_from_parts(1, __parts153);
    __t391 = YV_STR(__s154);
  }
  ergo_release_val(__t390);
  ErgoVal __t392 = YV_STR(stdr_str_lit("on-tertiary-fixed-variant"));
  ErgoVal __t393 = YV_NULLV;
  {
    ErgoVal __parts155[1] = { __t392 };
    ErgoStr* __s156 = stdr_str_from_parts(1, __parts155);
    __t393 = YV_STR(__s156);
  }
  ergo_release_val(__t392);
  ErgoVal __t394 = ergo_main_fixed_column(__t379, __t381, __t383, __t385, __t387, __t389, __t391, __t393);
  ergo_release_val(__t379);
  ergo_release_val(__t381);
  ergo_release_val(__t383);
  ergo_release_val(__t385);
  ergo_release_val(__t387);
  ergo_release_val(__t389);
  ergo_release_val(__t391);
  ergo_release_val(__t393);
  ErgoVal __t395 = ergo_m_cogito_HStack_add(__t377, __t394);
  ergo_release_val(__t377);
  ergo_release_val(__t394);
  ergo_move_into(&fixed__18, __t395);
  ErgoVal lower__19 = YV_NULLV;
  ErgoVal __t396 = ergo_cogito_hstack();
  ErgoVal __t397 = YV_INT(12);
  ErgoVal __t398 = ergo_m_cogito_HStack_set_gap(__t396, __t397);
  ergo_release_val(__t396);
  ergo_release_val(__t397);
  ErgoVal __t399 = YV_BOOL(true);
  ErgoVal __t400 = ergo_m_cogito_HStack_set_hexpand(__t398, __t399);
  ergo_release_val(__t398);
  ergo_release_val(__t399);
  ergo_move_into(&lower__19, __t400);
  ErgoVal neutral__20 = YV_NULLV;
  ErgoVal __t401 = ergo_cogito_vstack();
  ErgoVal __t402 = YV_INT(12);
  ErgoVal __t403 = ergo_m_cogito_VStack_set_gap(__t401, __t402);
  ergo_release_val(__t401);
  ergo_release_val(__t402);
  ErgoVal __t404 = YV_BOOL(true);
  ErgoVal __t405 = ergo_m_cogito_VStack_set_hexpand(__t403, __t404);
  ergo_release_val(__t403);
  ergo_release_val(__t404);
  ergo_move_into(&neutral__20, __t405);
  ErgoVal surfaces__21 = YV_NULLV;
  ErgoVal __t406 = ergo_cogito_hstack();
  ErgoVal __t407 = YV_BOOL(true);
  ErgoVal __t408 = ergo_m_cogito_HStack_set_hexpand(__t406, __t407);
  ergo_release_val(__t406);
  ergo_release_val(__t407);
  ErgoVal __t409 = YV_STR(stdr_str_lit("Surface Dim"));
  ErgoVal __t410 = YV_NULLV;
  {
    ErgoVal __parts157[1] = { __t409 };
    ErgoStr* __s158 = stdr_str_from_parts(1, __parts157);
    __t410 = YV_STR(__s158);
  }
  ergo_release_val(__t409);
  ErgoVal __t411 = YV_STR(stdr_str_lit("surface-dim"));
  ErgoVal __t412 = YV_NULLV;
  {
    ErgoVal __parts159[1] = { __t411 };
    ErgoStr* __s160 = stdr_str_from_parts(1, __parts159);
    __t412 = YV_STR(__s160);
  }
  ergo_release_val(__t411);
  ErgoVal __t413 = ergo_main_swatch(__t410, __t412);
  ergo_release_val(__t410);
  ergo_release_val(__t412);
  ErgoVal __t414 = ergo_m_cogito_HStack_add(__t408, __t413);
  ergo_release_val(__t408);
  ergo_release_val(__t413);
  ErgoVal __t415 = YV_STR(stdr_str_lit("Surface"));
  ErgoVal __t416 = YV_NULLV;
  {
    ErgoVal __parts161[1] = { __t415 };
    ErgoStr* __s162 = stdr_str_from_parts(1, __parts161);
    __t416 = YV_STR(__s162);
  }
  ergo_release_val(__t415);
  ErgoVal __t417 = YV_STR(stdr_str_lit("surface"));
  ErgoVal __t418 = YV_NULLV;
  {
    ErgoVal __parts163[1] = { __t417 };
    ErgoStr* __s164 = stdr_str_from_parts(1, __parts163);
    __t418 = YV_STR(__s164);
  }
  ergo_release_val(__t417);
  ErgoVal __t419 = ergo_main_swatch(__t416, __t418);
  ergo_release_val(__t416);
  ergo_release_val(__t418);
  ErgoVal __t420 = ergo_m_cogito_HStack_add(__t414, __t419);
  ergo_release_val(__t414);
  ergo_release_val(__t419);
  ErgoVal __t421 = YV_STR(stdr_str_lit("Surface Bright"));
  ErgoVal __t422 = YV_NULLV;
  {
    ErgoVal __parts165[1] = { __t421 };
    ErgoStr* __s166 = stdr_str_from_parts(1, __parts165);
    __t422 = YV_STR(__s166);
  }
  ergo_release_val(__t421);
  ErgoVal __t423 = YV_STR(stdr_str_lit("surface-bright"));
  ErgoVal __t424 = YV_NULLV;
  {
    ErgoVal __parts167[1] = { __t423 };
    ErgoStr* __s168 = stdr_str_from_parts(1, __parts167);
    __t424 = YV_STR(__s168);
  }
  ergo_release_val(__t423);
  ErgoVal __t425 = ergo_main_swatch(__t422, __t424);
  ergo_release_val(__t422);
  ergo_release_val(__t424);
  ErgoVal __t426 = ergo_m_cogito_HStack_add(__t420, __t425);
  ergo_release_val(__t420);
  ergo_release_val(__t425);
  ergo_move_into(&surfaces__21, __t426);
  ErgoVal containers__22 = YV_NULLV;
  ErgoVal __t427 = ergo_cogito_hstack();
  ErgoVal __t428 = YV_BOOL(true);
  ErgoVal __t429 = ergo_m_cogito_HStack_set_hexpand(__t427, __t428);
  ergo_release_val(__t427);
  ergo_release_val(__t428);
  ErgoVal __t430 = YV_STR(stdr_str_lit("Surface Container Lowest"));
  ErgoVal __t431 = YV_NULLV;
  {
    ErgoVal __parts169[1] = { __t430 };
    ErgoStr* __s170 = stdr_str_from_parts(1, __parts169);
    __t431 = YV_STR(__s170);
  }
  ergo_release_val(__t430);
  ErgoVal __t432 = YV_STR(stdr_str_lit("surface-container-lowest"));
  ErgoVal __t433 = YV_NULLV;
  {
    ErgoVal __parts171[1] = { __t432 };
    ErgoStr* __s172 = stdr_str_from_parts(1, __parts171);
    __t433 = YV_STR(__s172);
  }
  ergo_release_val(__t432);
  ErgoVal __t434 = ergo_main_swatch(__t431, __t433);
  ergo_release_val(__t431);
  ergo_release_val(__t433);
  ErgoVal __t435 = ergo_m_cogito_HStack_add(__t429, __t434);
  ergo_release_val(__t429);
  ergo_release_val(__t434);
  ErgoVal __t436 = YV_STR(stdr_str_lit("Surface Container Low"));
  ErgoVal __t437 = YV_NULLV;
  {
    ErgoVal __parts173[1] = { __t436 };
    ErgoStr* __s174 = stdr_str_from_parts(1, __parts173);
    __t437 = YV_STR(__s174);
  }
  ergo_release_val(__t436);
  ErgoVal __t438 = YV_STR(stdr_str_lit("surface-container-low"));
  ErgoVal __t439 = YV_NULLV;
  {
    ErgoVal __parts175[1] = { __t438 };
    ErgoStr* __s176 = stdr_str_from_parts(1, __parts175);
    __t439 = YV_STR(__s176);
  }
  ergo_release_val(__t438);
  ErgoVal __t440 = ergo_main_swatch(__t437, __t439);
  ergo_release_val(__t437);
  ergo_release_val(__t439);
  ErgoVal __t441 = ergo_m_cogito_HStack_add(__t435, __t440);
  ergo_release_val(__t435);
  ergo_release_val(__t440);
  ErgoVal __t442 = YV_STR(stdr_str_lit("Surface Container"));
  ErgoVal __t443 = YV_NULLV;
  {
    ErgoVal __parts177[1] = { __t442 };
    ErgoStr* __s178 = stdr_str_from_parts(1, __parts177);
    __t443 = YV_STR(__s178);
  }
  ergo_release_val(__t442);
  ErgoVal __t444 = YV_STR(stdr_str_lit("surface-container"));
  ErgoVal __t445 = YV_NULLV;
  {
    ErgoVal __parts179[1] = { __t444 };
    ErgoStr* __s180 = stdr_str_from_parts(1, __parts179);
    __t445 = YV_STR(__s180);
  }
  ergo_release_val(__t444);
  ErgoVal __t446 = ergo_main_swatch(__t443, __t445);
  ergo_release_val(__t443);
  ergo_release_val(__t445);
  ErgoVal __t447 = ergo_m_cogito_HStack_add(__t441, __t446);
  ergo_release_val(__t441);
  ergo_release_val(__t446);
  ErgoVal __t448 = YV_STR(stdr_str_lit("Surface Container High"));
  ErgoVal __t449 = YV_NULLV;
  {
    ErgoVal __parts181[1] = { __t448 };
    ErgoStr* __s182 = stdr_str_from_parts(1, __parts181);
    __t449 = YV_STR(__s182);
  }
  ergo_release_val(__t448);
  ErgoVal __t450 = YV_STR(stdr_str_lit("surface-container-high"));
  ErgoVal __t451 = YV_NULLV;
  {
    ErgoVal __parts183[1] = { __t450 };
    ErgoStr* __s184 = stdr_str_from_parts(1, __parts183);
    __t451 = YV_STR(__s184);
  }
  ergo_release_val(__t450);
  ErgoVal __t452 = ergo_main_swatch(__t449, __t451);
  ergo_release_val(__t449);
  ergo_release_val(__t451);
  ErgoVal __t453 = ergo_m_cogito_HStack_add(__t447, __t452);
  ergo_release_val(__t447);
  ergo_release_val(__t452);
  ErgoVal __t454 = YV_STR(stdr_str_lit("Surface Container Highest"));
  ErgoVal __t455 = YV_NULLV;
  {
    ErgoVal __parts185[1] = { __t454 };
    ErgoStr* __s186 = stdr_str_from_parts(1, __parts185);
    __t455 = YV_STR(__s186);
  }
  ergo_release_val(__t454);
  ErgoVal __t456 = YV_STR(stdr_str_lit("surface-container-highest"));
  ErgoVal __t457 = YV_NULLV;
  {
    ErgoVal __parts187[1] = { __t456 };
    ErgoStr* __s188 = stdr_str_from_parts(1, __parts187);
    __t457 = YV_STR(__s188);
  }
  ergo_release_val(__t456);
  ErgoVal __t458 = ergo_main_swatch(__t455, __t457);
  ergo_release_val(__t455);
  ergo_release_val(__t457);
  ErgoVal __t459 = ergo_m_cogito_HStack_add(__t453, __t458);
  ergo_release_val(__t453);
  ergo_release_val(__t458);
  ergo_move_into(&containers__22, __t459);
  ErgoVal on_surface__23 = YV_NULLV;
  ErgoVal __t460 = ergo_cogito_hstack();
  ErgoVal __t461 = YV_BOOL(true);
  ErgoVal __t462 = ergo_m_cogito_HStack_set_hexpand(__t460, __t461);
  ergo_release_val(__t460);
  ergo_release_val(__t461);
  ErgoVal __t463 = YV_STR(stdr_str_lit("On Surface"));
  ErgoVal __t464 = YV_NULLV;
  {
    ErgoVal __parts189[1] = { __t463 };
    ErgoStr* __s190 = stdr_str_from_parts(1, __parts189);
    __t464 = YV_STR(__s190);
  }
  ergo_release_val(__t463);
  ErgoVal __t465 = YV_STR(stdr_str_lit("on-surface"));
  ErgoVal __t466 = YV_NULLV;
  {
    ErgoVal __parts191[1] = { __t465 };
    ErgoStr* __s192 = stdr_str_from_parts(1, __parts191);
    __t466 = YV_STR(__s192);
  }
  ergo_release_val(__t465);
  ErgoVal __t467 = ergo_main_swatch(__t464, __t466);
  ergo_release_val(__t464);
  ergo_release_val(__t466);
  ErgoVal __t468 = ergo_m_cogito_HStack_add(__t462, __t467);
  ergo_release_val(__t462);
  ergo_release_val(__t467);
  ErgoVal __t469 = YV_STR(stdr_str_lit("On Surface Variant"));
  ErgoVal __t470 = YV_NULLV;
  {
    ErgoVal __parts193[1] = { __t469 };
    ErgoStr* __s194 = stdr_str_from_parts(1, __parts193);
    __t470 = YV_STR(__s194);
  }
  ergo_release_val(__t469);
  ErgoVal __t471 = YV_STR(stdr_str_lit("on-surface-variant"));
  ErgoVal __t472 = YV_NULLV;
  {
    ErgoVal __parts195[1] = { __t471 };
    ErgoStr* __s196 = stdr_str_from_parts(1, __parts195);
    __t472 = YV_STR(__s196);
  }
  ergo_release_val(__t471);
  ErgoVal __t473 = ergo_main_swatch(__t470, __t472);
  ergo_release_val(__t470);
  ergo_release_val(__t472);
  ErgoVal __t474 = ergo_m_cogito_HStack_add(__t468, __t473);
  ergo_release_val(__t468);
  ergo_release_val(__t473);
  ErgoVal __t475 = YV_STR(stdr_str_lit("Outline"));
  ErgoVal __t476 = YV_NULLV;
  {
    ErgoVal __parts197[1] = { __t475 };
    ErgoStr* __s198 = stdr_str_from_parts(1, __parts197);
    __t476 = YV_STR(__s198);
  }
  ergo_release_val(__t475);
  ErgoVal __t477 = YV_STR(stdr_str_lit("outline-fill"));
  ErgoVal __t478 = YV_NULLV;
  {
    ErgoVal __parts199[1] = { __t477 };
    ErgoStr* __s200 = stdr_str_from_parts(1, __parts199);
    __t478 = YV_STR(__s200);
  }
  ergo_release_val(__t477);
  ErgoVal __t479 = ergo_main_swatch(__t476, __t478);
  ergo_release_val(__t476);
  ergo_release_val(__t478);
  ErgoVal __t480 = ergo_m_cogito_HStack_add(__t474, __t479);
  ergo_release_val(__t474);
  ergo_release_val(__t479);
  ErgoVal __t481 = YV_STR(stdr_str_lit("Border"));
  ErgoVal __t482 = YV_NULLV;
  {
    ErgoVal __parts201[1] = { __t481 };
    ErgoStr* __s202 = stdr_str_from_parts(1, __parts201);
    __t482 = YV_STR(__s202);
  }
  ergo_release_val(__t481);
  ErgoVal __t483 = YV_STR(stdr_str_lit("outline-variant-fill"));
  ErgoVal __t484 = YV_NULLV;
  {
    ErgoVal __parts203[1] = { __t483 };
    ErgoStr* __s204 = stdr_str_from_parts(1, __parts203);
    __t484 = YV_STR(__s204);
  }
  ergo_release_val(__t483);
  ErgoVal __t485 = ergo_main_swatch(__t482, __t484);
  ergo_release_val(__t482);
  ergo_release_val(__t484);
  ErgoVal __t486 = ergo_m_cogito_HStack_add(__t480, __t485);
  ergo_release_val(__t480);
  ergo_release_val(__t485);
  ergo_move_into(&on_surface__23, __t486);
  ErgoVal __t487 = neutral__20; ergo_retain_val(__t487);
  ErgoVal __t488 = surfaces__21; ergo_retain_val(__t488);
  ErgoVal __t489 = ergo_m_cogito_VStack_add(__t487, __t488);
  ergo_release_val(__t487);
  ergo_release_val(__t488);
  ergo_release_val(__t489);
  ErgoVal __t490 = neutral__20; ergo_retain_val(__t490);
  ErgoVal __t491 = containers__22; ergo_retain_val(__t491);
  ErgoVal __t492 = ergo_m_cogito_VStack_add(__t490, __t491);
  ergo_release_val(__t490);
  ergo_release_val(__t491);
  ergo_release_val(__t492);
  ErgoVal __t493 = neutral__20; ergo_retain_val(__t493);
  ErgoVal __t494 = on_surface__23; ergo_retain_val(__t494);
  ErgoVal __t495 = ergo_m_cogito_VStack_add(__t493, __t494);
  ergo_release_val(__t493);
  ergo_release_val(__t494);
  ergo_release_val(__t495);
  ErgoVal shadow_row__24 = YV_NULLV;
  ErgoVal __t496 = ergo_cogito_vstack();
  ErgoVal __t497 = YV_BOOL(true);
  ErgoVal __t498 = ergo_m_cogito_VStack_set_hexpand(__t496, __t497);
  ergo_release_val(__t496);
  ergo_release_val(__t497);
  ErgoVal __t499 = YV_STR(stdr_str_lit("Shadow"));
  ErgoVal __t500 = YV_NULLV;
  {
    ErgoVal __parts205[1] = { __t499 };
    ErgoStr* __s206 = stdr_str_from_parts(1, __parts205);
    __t500 = YV_STR(__s206);
  }
  ergo_release_val(__t499);
  ErgoVal __t501 = YV_STR(stdr_str_lit("shadow"));
  ErgoVal __t502 = YV_NULLV;
  {
    ErgoVal __parts207[1] = { __t501 };
    ErgoStr* __s208 = stdr_str_from_parts(1, __parts207);
    __t502 = YV_STR(__s208);
  }
  ergo_release_val(__t501);
  ErgoVal __t503 = ergo_main_swatch(__t500, __t502);
  ergo_release_val(__t500);
  ergo_release_val(__t502);
  ErgoVal __t504 = ergo_m_cogito_VStack_add(__t498, __t503);
  ergo_release_val(__t498);
  ergo_release_val(__t503);
  ErgoVal __t505 = YV_STR(stdr_str_lit("Scrim"));
  ErgoVal __t506 = YV_NULLV;
  {
    ErgoVal __parts209[1] = { __t505 };
    ErgoStr* __s210 = stdr_str_from_parts(1, __parts209);
    __t506 = YV_STR(__s210);
  }
  ergo_release_val(__t505);
  ErgoVal __t507 = YV_STR(stdr_str_lit("scrim"));
  ErgoVal __t508 = YV_NULLV;
  {
    ErgoVal __parts211[1] = { __t507 };
    ErgoStr* __s212 = stdr_str_from_parts(1, __parts211);
    __t508 = YV_STR(__s212);
  }
  ergo_release_val(__t507);
  ErgoVal __t509 = ergo_main_swatch(__t506, __t508);
  ergo_release_val(__t506);
  ergo_release_val(__t508);
  ErgoVal __t510 = ergo_m_cogito_VStack_add(__t504, __t509);
  ergo_release_val(__t504);
  ergo_release_val(__t509);
  ergo_move_into(&shadow_row__24, __t510);
  ErgoVal inverse__25 = YV_NULLV;
  ErgoVal __t511 = ergo_cogito_vstack();
  ErgoVal __t512 = YV_BOOL(true);
  ErgoVal __t513 = ergo_m_cogito_VStack_set_hexpand(__t511, __t512);
  ergo_release_val(__t511);
  ergo_release_val(__t512);
  ErgoVal __t514 = YV_STR(stdr_str_lit("Inverse Surface"));
  ErgoVal __t515 = YV_NULLV;
  {
    ErgoVal __parts213[1] = { __t514 };
    ErgoStr* __s214 = stdr_str_from_parts(1, __parts213);
    __t515 = YV_STR(__s214);
  }
  ergo_release_val(__t514);
  ErgoVal __t516 = YV_STR(stdr_str_lit("inverse-surface"));
  ErgoVal __t517 = YV_NULLV;
  {
    ErgoVal __parts215[1] = { __t516 };
    ErgoStr* __s216 = stdr_str_from_parts(1, __parts215);
    __t517 = YV_STR(__s216);
  }
  ergo_release_val(__t516);
  ErgoVal __t518 = ergo_main_swatch(__t515, __t517);
  ergo_release_val(__t515);
  ergo_release_val(__t517);
  ErgoVal __t519 = ergo_m_cogito_VStack_add(__t513, __t518);
  ergo_release_val(__t513);
  ergo_release_val(__t518);
  ErgoVal __t520 = YV_STR(stdr_str_lit("On Inverse Surface"));
  ErgoVal __t521 = YV_NULLV;
  {
    ErgoVal __parts217[1] = { __t520 };
    ErgoStr* __s218 = stdr_str_from_parts(1, __parts217);
    __t521 = YV_STR(__s218);
  }
  ergo_release_val(__t520);
  ErgoVal __t522 = YV_STR(stdr_str_lit("on-inverse-surface"));
  ErgoVal __t523 = YV_NULLV;
  {
    ErgoVal __parts219[1] = { __t522 };
    ErgoStr* __s220 = stdr_str_from_parts(1, __parts219);
    __t523 = YV_STR(__s220);
  }
  ergo_release_val(__t522);
  ErgoVal __t524 = ergo_main_swatch(__t521, __t523);
  ergo_release_val(__t521);
  ergo_release_val(__t523);
  ErgoVal __t525 = ergo_m_cogito_VStack_add(__t519, __t524);
  ergo_release_val(__t519);
  ergo_release_val(__t524);
  ErgoVal __t526 = YV_STR(stdr_str_lit("Inverse Primary"));
  ErgoVal __t527 = YV_NULLV;
  {
    ErgoVal __parts221[1] = { __t526 };
    ErgoStr* __s222 = stdr_str_from_parts(1, __parts221);
    __t527 = YV_STR(__s222);
  }
  ergo_release_val(__t526);
  ErgoVal __t528 = YV_STR(stdr_str_lit("inverse-primary-fill"));
  ErgoVal __t529 = YV_NULLV;
  {
    ErgoVal __parts223[1] = { __t528 };
    ErgoStr* __s224 = stdr_str_from_parts(1, __parts223);
    __t529 = YV_STR(__s224);
  }
  ergo_release_val(__t528);
  ErgoVal __t530 = ergo_main_swatch(__t527, __t529);
  ergo_release_val(__t527);
  ergo_release_val(__t529);
  ErgoVal __t531 = ergo_m_cogito_VStack_add(__t525, __t530);
  ergo_release_val(__t525);
  ergo_release_val(__t530);
  ergo_move_into(&inverse__25, __t531);
  ErgoVal __t532 = lower__19; ergo_retain_val(__t532);
  ErgoVal __t533 = neutral__20; ergo_retain_val(__t533);
  ErgoVal __t534 = ergo_m_cogito_HStack_add(__t532, __t533);
  ergo_release_val(__t532);
  ergo_release_val(__t533);
  ergo_release_val(__t534);
  ErgoVal __t535 = lower__19; ergo_retain_val(__t535);
  ErgoVal __t536 = inverse__25; ergo_retain_val(__t536);
  ErgoVal __t537 = ergo_m_cogito_HStack_add(__t535, __t536);
  ergo_release_val(__t535);
  ergo_release_val(__t536);
  ergo_release_val(__t537);
  ErgoVal __t538 = lower__19; ergo_retain_val(__t538);
  ErgoVal __t539 = shadow_row__24; ergo_retain_val(__t539);
  ErgoVal __t540 = ergo_m_cogito_HStack_add(__t538, __t539);
  ergo_release_val(__t538);
  ergo_release_val(__t539);
  ergo_release_val(__t540);
  ErgoVal __t541 = group__16; ergo_retain_val(__t541);
  ErgoVal __t542 = tones__17; ergo_retain_val(__t542);
  ErgoVal __t543 = ergo_m_cogito_VStack_add(__t541, __t542);
  ergo_release_val(__t541);
  ergo_release_val(__t542);
  ergo_release_val(__t543);
  ErgoVal __t544 = group__16; ergo_retain_val(__t544);
  ErgoVal __t545 = fixed__18; ergo_retain_val(__t545);
  ErgoVal __t546 = ergo_m_cogito_VStack_add(__t544, __t545);
  ergo_release_val(__t544);
  ergo_release_val(__t545);
  ergo_release_val(__t546);
  ErgoVal __t547 = group__16; ergo_retain_val(__t547);
  ErgoVal __t548 = lower__19; ergo_retain_val(__t548);
  ErgoVal __t549 = ergo_m_cogito_VStack_add(__t547, __t548);
  ergo_release_val(__t547);
  ergo_release_val(__t548);
  ergo_release_val(__t549);
  ErgoVal __t550 = group__16; ergo_retain_val(__t550);
  ergo_move_into(&__ret, __t550);
  return __ret;
  ergo_release_val(inverse__25);
  ergo_release_val(shadow_row__24);
  ergo_release_val(on_surface__23);
  ergo_release_val(containers__22);
  ergo_release_val(surfaces__21);
  ergo_release_val(neutral__20);
  ergo_release_val(lower__19);
  ergo_release_val(fixed__18);
  ergo_release_val(tones__17);
  ergo_release_val(group__16);
  return __ret;
}

static ErgoVal ergo_main_build_right_panel(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal panel__26 = YV_NULLV;
  ErgoVal __t551 = ergo_cogito_vstack();
  ErgoVal __t552 = YV_INT(24);
  ErgoVal __t553 = ergo_m_cogito_VStack_set_gap(__t551, __t552);
  ergo_release_val(__t551);
  ergo_release_val(__t552);
  ErgoVal __t554 = YV_INT(0);
  ErgoVal __t555 = YV_INT(18);
  ErgoVal __t556 = YV_INT(18);
  ErgoVal __t557 = YV_INT(18);
  ErgoVal __t558 = ergo_m_cogito_VStack_set_padding(__t553, __t554, __t555, __t556, __t557);
  ergo_release_val(__t553);
  ergo_release_val(__t554);
  ergo_release_val(__t555);
  ergo_release_val(__t556);
  ergo_release_val(__t557);
  ErgoVal __t559 = YV_BOOL(true);
  ErgoVal __t560 = ergo_m_cogito_VStack_set_vexpand(__t558, __t559);
  ergo_release_val(__t558);
  ergo_release_val(__t559);
  ErgoVal __t561 = YV_BOOL(true);
  ErgoVal __t562 = ergo_m_cogito_VStack_set_hexpand(__t560, __t561);
  ergo_release_val(__t560);
  ergo_release_val(__t561);
  ErgoVal __t563 = YV_STR(stdr_str_lit("surface-container"));
  ErgoVal __t564 = YV_NULLV;
  {
    ErgoVal __parts225[1] = { __t563 };
    ErgoStr* __s226 = stdr_str_from_parts(1, __parts225);
    __t564 = YV_STR(__s226);
  }
  ergo_release_val(__t563);
  ErgoVal __t565 = ergo_m_cogito_VStack_set_class(__t562, __t564);
  ergo_release_val(__t562);
  ergo_release_val(__t564);
  ergo_move_into(&panel__26, __t565);
  ErgoVal upload__27 = YV_NULLV;
  ErgoVal __t566 = ergo_cogito_zstack();
  ErgoVal __t567 = YV_INT(1);
  ErgoVal __t568 = ergo_m_cogito_ZStack_halign(__t566, __t567);
  ergo_release_val(__t566);
  ergo_release_val(__t567);
  ErgoVal __t569 = YV_INT(1);
  ErgoVal __t570 = ergo_m_cogito_ZStack_valign(__t568, __t569);
  ergo_release_val(__t568);
  ergo_release_val(__t569);
  ErgoVal __t571 = YV_STR(stdr_str_lit("gafu-upload-wrap"));
  ErgoVal __t572 = YV_NULLV;
  {
    ErgoVal __parts227[1] = { __t571 };
    ErgoStr* __s228 = stdr_str_from_parts(1, __parts227);
    __t572 = YV_STR(__s228);
  }
  ergo_release_val(__t571);
  ErgoVal __t573 = ergo_m_cogito_ZStack_set_class(__t570, __t572);
  ergo_release_val(__t570);
  ergo_release_val(__t572);
  ergo_move_into(&upload__27, __t573);
  ErgoVal upload_hit__28 = YV_NULLV;
  ErgoVal __t574 = YV_STR(stdr_str_lit(""));
  ErgoVal __t575 = ergo_cogito_button(__t574);
  ergo_release_val(__t574);
  ErgoVal __t576 = YV_STR(stdr_str_lit("secondary-container"));
  ErgoVal __t577 = YV_NULLV;
  {
    ErgoVal __parts229[1] = { __t576 };
    ErgoStr* __s230 = stdr_str_from_parts(1, __parts229);
    __t577 = YV_STR(__s230);
  }
  ergo_release_val(__t576);
  ErgoVal __t578 = ergo_m_cogito_Button_set_class(__t575, __t577);
  ergo_release_val(__t575);
  ergo_release_val(__t577);
  ErgoVal __t579 = YV_STR(stdr_str_lit("gafu-upload-hit"));
  ErgoVal __t580 = YV_NULLV;
  {
    ErgoVal __parts231[1] = { __t579 };
    ErgoStr* __s232 = stdr_str_from_parts(1, __parts231);
    __t580 = YV_STR(__s232);
  }
  ergo_release_val(__t579);
  ErgoVal __t581 = ergo_m_cogito_Button_set_class(__t578, __t580);
  ergo_release_val(__t578);
  ergo_release_val(__t580);
  ErgoVal __t582 = YV_BOOL(true);
  ErgoVal __t583 = ergo_m_cogito_Button_set_hexpand(__t581, __t582);
  ergo_release_val(__t581);
  ergo_release_val(__t582);
  ErgoVal __t584 = YV_BOOL(true);
  ErgoVal __t585 = ergo_m_cogito_Button_set_vexpand(__t583, __t584);
  ergo_release_val(__t583);
  ergo_release_val(__t584);
  ErgoVal __t586 = YV_FN(ergo_fn_new(ergo_lambda_1, 1));
  ErgoVal __t587 = ergo_m_cogito_Button_on_click(__t585, __t586);
  ergo_release_val(__t585);
  ergo_release_val(__t586);
  ergo_move_into(&upload_hit__28, __t587);
  ErgoVal __t588 = upload__27; ergo_retain_val(__t588);
  ErgoVal __t589 = upload_hit__28; ergo_retain_val(__t589);
  ErgoVal __t590 = ergo_m_cogito_ZStack_add(__t588, __t589);
  ergo_release_val(__t588);
  ergo_release_val(__t589);
  ergo_release_val(__t590);
  ErgoVal upload_content__29 = YV_NULLV;
  ErgoVal __t591 = ergo_cogito_vstack();
  ErgoVal __t592 = YV_STR(stdr_str_lit("gafu-upload-content"));
  ErgoVal __t593 = YV_NULLV;
  {
    ErgoVal __parts233[1] = { __t592 };
    ErgoStr* __s234 = stdr_str_from_parts(1, __parts233);
    __t593 = YV_STR(__s234);
  }
  ergo_release_val(__t592);
  ErgoVal __t594 = ergo_m_cogito_VStack_set_class(__t591, __t593);
  ergo_release_val(__t591);
  ergo_release_val(__t593);
  ergo_move_into(&upload_content__29, __t594);
  ErgoVal image__30 = YV_NULLV;
  ErgoVal __t595 = YV_STR(stdr_str_lit("sf:photo.badge.plus"));
  ErgoVal __t596 = YV_NULLV;
  {
    ErgoVal __parts235[1] = { __t595 };
    ErgoStr* __s236 = stdr_str_from_parts(1, __parts235);
    __t596 = YV_STR(__s236);
  }
  ergo_release_val(__t595);
  ErgoVal __t597 = ergo_cogito_image(__t596);
  ergo_release_val(__t596);
  ErgoVal __t598 = YV_STR(stdr_str_lit("gafu-upload-icon"));
  ErgoVal __t599 = YV_NULLV;
  {
    ErgoVal __parts237[1] = { __t598 };
    ErgoStr* __s238 = stdr_str_from_parts(1, __parts237);
    __t599 = YV_STR(__s238);
  }
  ergo_release_val(__t598);
  ErgoVal __t600 = ergo_m_cogito_Image_set_class(__t597, __t599);
  ergo_release_val(__t597);
  ergo_release_val(__t599);
  ergo_move_into(&image__30, __t600);
  ErgoVal __t601 = upload_content__29; ergo_retain_val(__t601);
  ErgoVal __t602 = image__30; ergo_retain_val(__t602);
  ErgoVal __t603 = ergo_m_cogito_VStack_add(__t601, __t602);
  ergo_release_val(__t601);
  ergo_release_val(__t602);
  ergo_release_val(__t603);
  ErgoVal __t604 = ergo_g_main_upload_hint; ergo_retain_val(__t604);
  ErgoVal __t605 = YV_STR(stdr_str_lit("gafu-upload-label"));
  ErgoVal __t606 = YV_NULLV;
  {
    ErgoVal __parts239[1] = { __t605 };
    ErgoStr* __s240 = stdr_str_from_parts(1, __parts239);
    __t606 = YV_STR(__s240);
  }
  ergo_release_val(__t605);
  ErgoVal __t607 = ergo_m_cogito_Label_set_class(__t604, __t606);
  ergo_release_val(__t604);
  ergo_release_val(__t606);
  ergo_release_val(__t607);
  ErgoVal __t608 = upload_content__29; ergo_retain_val(__t608);
  ErgoVal __t609 = ergo_g_main_upload_hint; ergo_retain_val(__t609);
  ErgoVal __t610 = ergo_m_cogito_VStack_add(__t608, __t609);
  ergo_release_val(__t608);
  ergo_release_val(__t609);
  ergo_release_val(__t610);
  ErgoVal __t611 = upload__27; ergo_retain_val(__t611);
  ErgoVal __t612 = upload_content__29; ergo_retain_val(__t612);
  ErgoVal __t613 = ergo_m_cogito_ZStack_add(__t611, __t612);
  ergo_release_val(__t611);
  ergo_release_val(__t612);
  ergo_release_val(__t613);
  ErgoVal __t614 = ergo_g_main_selected_image_preview; ergo_retain_val(__t614);
  ErgoVal __t615 = YV_STR(stdr_str_lit("gafu-selected-image"));
  ErgoVal __t616 = YV_NULLV;
  {
    ErgoVal __parts241[1] = { __t615 };
    ErgoStr* __s242 = stdr_str_from_parts(1, __parts241);
    __t616 = YV_STR(__s242);
  }
  ergo_release_val(__t615);
  ErgoVal __t617 = ergo_m_cogito_Image_set_class(__t614, __t616);
  ergo_release_val(__t614);
  ergo_release_val(__t616);
  ErgoVal __t618 = YV_INT(0);
  ErgoVal __t619 = YV_INT(0);
  ErgoVal __t620 = ergo_m_cogito_Image_set_size(__t617, __t618, __t619);
  ergo_release_val(__t617);
  ergo_release_val(__t618);
  ergo_release_val(__t619);
  ErgoVal __t621 = YV_BOOL(true);
  ErgoVal __t622 = ergo_m_cogito_Image_set_hexpand(__t620, __t621);
  ergo_release_val(__t620);
  ergo_release_val(__t621);
  ergo_release_val(__t622);
  ErgoVal __t623 = upload__27; ergo_retain_val(__t623);
  ErgoVal __t624 = ergo_g_main_selected_image_preview; ergo_retain_val(__t624);
  ErgoVal __t625 = ergo_m_cogito_ZStack_add(__t623, __t624);
  ergo_release_val(__t623);
  ergo_release_val(__t624);
  ergo_release_val(__t625);
  ErgoVal tools__31 = YV_NULLV;
  ErgoVal __t626 = ergo_cogito_hstack();
  ErgoVal __t627 = YV_INT(10);
  ErgoVal __t628 = ergo_m_cogito_HStack_set_gap(__t626, __t627);
  ergo_release_val(__t626);
  ergo_release_val(__t627);
  ErgoVal __t629 = YV_STR(stdr_str_lit("gafu-tools"));
  ErgoVal __t630 = YV_NULLV;
  {
    ErgoVal __parts243[1] = { __t629 };
    ErgoStr* __s244 = stdr_str_from_parts(1, __parts243);
    __t630 = YV_STR(__s244);
  }
  ergo_release_val(__t629);
  ErgoVal __t631 = ergo_m_cogito_HStack_set_class(__t628, __t630);
  ergo_release_val(__t628);
  ergo_release_val(__t630);
  ErgoVal __t632 = YV_INT(1);
  ErgoVal __t633 = ergo_m_cogito_HStack_halign(__t631, __t632);
  ergo_release_val(__t631);
  ergo_release_val(__t632);
  ergo_move_into(&tools__31, __t633);
  ErgoVal __t634 = tools__31; ergo_retain_val(__t634);
  ErgoVal __t635 = YV_STR(stdr_str_lit("sf:laptopcomputer"));
  ErgoVal __t636 = YV_NULLV;
  {
    ErgoVal __parts245[1] = { __t635 };
    ErgoStr* __s246 = stdr_str_from_parts(1, __parts245);
    __t636 = YV_STR(__s246);
  }
  ergo_release_val(__t635);
  ErgoVal __t637 = ergo_cogito_image(__t636);
  ergo_release_val(__t636);
  ErgoVal __t638 = YV_STR(stdr_str_lit("gafu-tool"));
  ErgoVal __t639 = YV_NULLV;
  {
    ErgoVal __parts247[1] = { __t638 };
    ErgoStr* __s248 = stdr_str_from_parts(1, __parts247);
    __t639 = YV_STR(__s248);
  }
  ergo_release_val(__t638);
  ErgoVal __t640 = ergo_m_cogito_Image_set_class(__t637, __t639);
  ergo_release_val(__t637);
  ergo_release_val(__t639);
  ErgoVal __t641 = ergo_m_cogito_HStack_add(__t634, __t640);
  ergo_release_val(__t634);
  ergo_release_val(__t640);
  ergo_release_val(__t641);
  ErgoVal __t642 = tools__31; ergo_retain_val(__t642);
  ErgoVal __t643 = ergo_g_main_scheme_switch; ergo_retain_val(__t643);
  ErgoVal __t644 = YV_BOOL(false);
  ErgoVal __t645 = ergo_m_cogito_Switch_set_checked(__t643, __t644);
  ergo_release_val(__t643);
  ergo_release_val(__t644);
  ErgoVal __t646 = YV_STR(stdr_str_lit("gafu-tool-switch"));
  ErgoVal __t647 = YV_NULLV;
  {
    ErgoVal __parts249[1] = { __t646 };
    ErgoStr* __s250 = stdr_str_from_parts(1, __parts249);
    __t647 = YV_STR(__s250);
  }
  ergo_release_val(__t646);
  ErgoVal __t648 = ergo_m_cogito_Switch_set_class(__t645, __t647);
  ergo_release_val(__t645);
  ergo_release_val(__t647);
  ErgoVal __t649 = ergo_g_main_selected_image_path; ergo_retain_val(__t649);
  ErgoVal __t650 = ergo_stdr_len(__t649);
  ergo_release_val(__t649);
  ErgoVal __t651 = YV_INT(0);
  ErgoVal __t652 = ergo_eq(__t650, __t651);
  ergo_release_val(__t650);
  ergo_release_val(__t651);
  ErgoVal __t653 = ergo_m_cogito_Switch_set_disabled(__t648, __t652);
  ergo_release_val(__t648);
  ergo_release_val(__t652);
  ErgoVal __t654 = YV_FN(ergo_fn_new(ergo_lambda_2, 1));
  ErgoVal __t655 = ergo_m_cogito_Switch_on_change(__t653, __t654);
  ergo_release_val(__t653);
  ergo_release_val(__t654);
  ErgoVal __t656 = ergo_m_cogito_HStack_add(__t642, __t655);
  ergo_release_val(__t642);
  ergo_release_val(__t655);
  ergo_release_val(__t656);
  ErgoVal __t657 = tools__31; ergo_retain_val(__t657);
  ErgoVal __t658 = YV_STR(stdr_str_lit("sf:photo"));
  ErgoVal __t659 = YV_NULLV;
  {
    ErgoVal __parts251[1] = { __t658 };
    ErgoStr* __s252 = stdr_str_from_parts(1, __parts251);
    __t659 = YV_STR(__s252);
  }
  ergo_release_val(__t658);
  ErgoVal __t660 = ergo_cogito_image(__t659);
  ergo_release_val(__t659);
  ErgoVal __t661 = YV_STR(stdr_str_lit("gafu-tool"));
  ErgoVal __t662 = YV_NULLV;
  {
    ErgoVal __parts253[1] = { __t661 };
    ErgoStr* __s254 = stdr_str_from_parts(1, __parts253);
    __t662 = YV_STR(__s254);
  }
  ergo_release_val(__t661);
  ErgoVal __t663 = ergo_m_cogito_Image_set_class(__t660, __t662);
  ergo_release_val(__t660);
  ergo_release_val(__t662);
  ErgoVal __t664 = ergo_m_cogito_HStack_add(__t657, __t663);
  ergo_release_val(__t657);
  ergo_release_val(__t663);
  ergo_release_val(__t664);
  ErgoVal modes__32 = YV_NULLV;
  ErgoVal __t665 = ergo_cogito_buttongroup();
  ErgoVal __t666 = YV_STR(stdr_str_lit("gafu-modes"));
  ErgoVal __t667 = YV_NULLV;
  {
    ErgoVal __parts255[1] = { __t666 };
    ErgoStr* __s256 = stdr_str_from_parts(1, __parts255);
    __t667 = YV_STR(__s256);
  }
  ergo_release_val(__t666);
  ErgoVal __t668 = ergo_m_cogito_ButtonGroup_set_class(__t665, __t667);
  ergo_release_val(__t665);
  ergo_release_val(__t667);
  ErgoVal __t669 = YV_STR(stdr_str_lit("surface-container-lowest"));
  ErgoVal __t670 = YV_NULLV;
  {
    ErgoVal __parts257[1] = { __t669 };
    ErgoStr* __s258 = stdr_str_from_parts(1, __parts257);
    __t670 = YV_STR(__s258);
  }
  ergo_release_val(__t669);
  ErgoVal __t671 = ergo_m_cogito_ButtonGroup_set_class(__t668, __t670);
  ergo_release_val(__t668);
  ergo_release_val(__t670);
  ErgoVal __t672 = YV_INT(1);
  ErgoVal __t673 = ergo_m_cogito_ButtonGroup_set_halign(__t671, __t672);
  ergo_release_val(__t671);
  ergo_release_val(__t672);
  ergo_move_into(&modes__32, __t673);
  ErgoVal __t674 = ergo_g_main_mode_moon; ergo_retain_val(__t674);
  ErgoVal __t675 = YV_BOOL(true);
  ErgoVal __t676 = ergo_m_cogito_IconButton_set_toggle(__t674, __t675);
  ergo_release_val(__t674);
  ergo_release_val(__t675);
  ErgoVal __t677 = YV_BOOL(false);
  ErgoVal __t678 = ergo_m_cogito_IconButton_set_checked(__t676, __t677);
  ergo_release_val(__t676);
  ergo_release_val(__t677);
  ErgoVal __t679 = YV_STR(stdr_str_lit("tonal"));
  ErgoVal __t680 = YV_NULLV;
  {
    ErgoVal __parts259[1] = { __t679 };
    ErgoStr* __s260 = stdr_str_from_parts(1, __parts259);
    __t680 = YV_STR(__s260);
  }
  ergo_release_val(__t679);
  ErgoVal __t681 = ergo_m_cogito_IconButton_set_class(__t678, __t680);
  ergo_release_val(__t678);
  ergo_release_val(__t680);
  ergo_release_val(__t681);
  ErgoVal __t682 = ergo_g_main_mode_base; ergo_retain_val(__t682);
  ErgoVal __t683 = YV_BOOL(true);
  ErgoVal __t684 = ergo_m_cogito_IconButton_set_toggle(__t682, __t683);
  ergo_release_val(__t682);
  ergo_release_val(__t683);
  ErgoVal __t685 = YV_BOOL(true);
  ErgoVal __t686 = ergo_m_cogito_IconButton_set_checked(__t684, __t685);
  ergo_release_val(__t684);
  ergo_release_val(__t685);
  ErgoVal __t687 = YV_STR(stdr_str_lit("filled"));
  ErgoVal __t688 = YV_NULLV;
  {
    ErgoVal __parts261[1] = { __t687 };
    ErgoStr* __s262 = stdr_str_from_parts(1, __parts261);
    __t688 = YV_STR(__s262);
  }
  ergo_release_val(__t687);
  ErgoVal __t689 = ergo_m_cogito_IconButton_set_class(__t686, __t688);
  ergo_release_val(__t686);
  ergo_release_val(__t688);
  ergo_release_val(__t689);
  ErgoVal __t690 = ergo_g_main_mode_bright; ergo_retain_val(__t690);
  ErgoVal __t691 = YV_BOOL(true);
  ErgoVal __t692 = ergo_m_cogito_IconButton_set_toggle(__t690, __t691);
  ergo_release_val(__t690);
  ergo_release_val(__t691);
  ErgoVal __t693 = YV_BOOL(false);
  ErgoVal __t694 = ergo_m_cogito_IconButton_set_checked(__t692, __t693);
  ergo_release_val(__t692);
  ergo_release_val(__t693);
  ErgoVal __t695 = YV_STR(stdr_str_lit("tonal"));
  ErgoVal __t696 = YV_NULLV;
  {
    ErgoVal __parts263[1] = { __t695 };
    ErgoStr* __s264 = stdr_str_from_parts(1, __parts263);
    __t696 = YV_STR(__s264);
  }
  ergo_release_val(__t695);
  ErgoVal __t697 = ergo_m_cogito_IconButton_set_class(__t694, __t696);
  ergo_release_val(__t694);
  ergo_release_val(__t696);
  ergo_release_val(__t697);
  ErgoVal __t698 = ergo_g_main_mode_brightest; ergo_retain_val(__t698);
  ErgoVal __t699 = YV_BOOL(true);
  ErgoVal __t700 = ergo_m_cogito_IconButton_set_toggle(__t698, __t699);
  ergo_release_val(__t698);
  ergo_release_val(__t699);
  ErgoVal __t701 = YV_BOOL(false);
  ErgoVal __t702 = ergo_m_cogito_IconButton_set_checked(__t700, __t701);
  ergo_release_val(__t700);
  ergo_release_val(__t701);
  ErgoVal __t703 = YV_STR(stdr_str_lit("tonal"));
  ErgoVal __t704 = YV_NULLV;
  {
    ErgoVal __parts265[1] = { __t703 };
    ErgoStr* __s266 = stdr_str_from_parts(1, __parts265);
    __t704 = YV_STR(__s266);
  }
  ergo_release_val(__t703);
  ErgoVal __t705 = ergo_m_cogito_IconButton_set_class(__t702, __t704);
  ergo_release_val(__t702);
  ergo_release_val(__t704);
  ergo_release_val(__t705);
  ErgoVal __t706 = ergo_g_main_mode_moon; ergo_retain_val(__t706);
  ErgoVal __t707 = YV_FN(ergo_fn_new(ergo_lambda_3, 1));
  ErgoVal __t708 = ergo_m_cogito_IconButton_on_click(__t706, __t707);
  ergo_release_val(__t706);
  ergo_release_val(__t707);
  ergo_release_val(__t708);
  ErgoVal __t709 = ergo_g_main_mode_base; ergo_retain_val(__t709);
  ErgoVal __t710 = YV_FN(ergo_fn_new(ergo_lambda_4, 1));
  ErgoVal __t711 = ergo_m_cogito_IconButton_on_click(__t709, __t710);
  ergo_release_val(__t709);
  ergo_release_val(__t710);
  ergo_release_val(__t711);
  ErgoVal __t712 = ergo_g_main_mode_bright; ergo_retain_val(__t712);
  ErgoVal __t713 = YV_FN(ergo_fn_new(ergo_lambda_5, 1));
  ErgoVal __t714 = ergo_m_cogito_IconButton_on_click(__t712, __t713);
  ergo_release_val(__t712);
  ergo_release_val(__t713);
  ergo_release_val(__t714);
  ErgoVal __t715 = ergo_g_main_mode_brightest; ergo_retain_val(__t715);
  ErgoVal __t716 = YV_FN(ergo_fn_new(ergo_lambda_6, 1));
  ErgoVal __t717 = ergo_m_cogito_IconButton_on_click(__t715, __t716);
  ergo_release_val(__t715);
  ergo_release_val(__t716);
  ergo_release_val(__t717);
  ErgoVal __t718 = modes__32; ergo_retain_val(__t718);
  ErgoVal __t719 = ergo_g_main_mode_moon; ergo_retain_val(__t719);
  ErgoVal __t720 = ergo_m_cogito_ButtonGroup_add(__t718, __t719);
  ergo_release_val(__t718);
  ergo_release_val(__t719);
  ergo_release_val(__t720);
  ErgoVal __t721 = modes__32; ergo_retain_val(__t721);
  ErgoVal __t722 = ergo_g_main_mode_base; ergo_retain_val(__t722);
  ErgoVal __t723 = ergo_m_cogito_ButtonGroup_add(__t721, __t722);
  ergo_release_val(__t721);
  ergo_release_val(__t722);
  ergo_release_val(__t723);
  ErgoVal __t724 = modes__32; ergo_retain_val(__t724);
  ErgoVal __t725 = ergo_g_main_mode_bright; ergo_retain_val(__t725);
  ErgoVal __t726 = ergo_m_cogito_ButtonGroup_add(__t724, __t725);
  ergo_release_val(__t724);
  ergo_release_val(__t725);
  ergo_release_val(__t726);
  ErgoVal __t727 = modes__32; ergo_retain_val(__t727);
  ErgoVal __t728 = ergo_g_main_mode_brightest; ergo_retain_val(__t728);
  ErgoVal __t729 = ergo_m_cogito_ButtonGroup_add(__t727, __t728);
  ergo_release_val(__t727);
  ergo_release_val(__t728);
  ergo_release_val(__t729);
  ErgoVal source__33 = YV_NULLV;
  ErgoVal __t730 = YV_STR(stdr_str_lit(""));
  ErgoVal __t731 = ergo_cogito_card(__t730);
  ergo_release_val(__t730);
  ErgoVal __t732 = YV_INT(10);
  ErgoVal __t733 = ergo_m_cogito_Card_set_gap(__t731, __t732);
  ergo_release_val(__t731);
  ergo_release_val(__t732);
  ErgoVal __t734 = YV_BOOL(true);
  ErgoVal __t735 = ergo_m_cogito_Card_set_hexpand(__t733, __t734);
  ergo_release_val(__t733);
  ergo_release_val(__t734);
  ErgoVal __t736 = YV_STR(stdr_str_lit("surface-container-high"));
  ErgoVal __t737 = YV_NULLV;
  {
    ErgoVal __parts267[1] = { __t736 };
    ErgoStr* __s268 = stdr_str_from_parts(1, __parts267);
    __t737 = YV_STR(__s268);
  }
  ergo_release_val(__t736);
  ErgoVal __t738 = ergo_m_cogito_Card_set_class(__t735, __t737);
  ergo_release_val(__t735);
  ergo_release_val(__t737);
  ErgoVal __t739 = YV_STR(stdr_str_lit("gafu-source"));
  ErgoVal __t740 = YV_NULLV;
  {
    ErgoVal __parts269[1] = { __t739 };
    ErgoStr* __s270 = stdr_str_from_parts(1, __parts269);
    __t740 = YV_STR(__s270);
  }
  ergo_release_val(__t739);
  ErgoVal __t741 = ergo_m_cogito_Card_set_class(__t738, __t740);
  ergo_release_val(__t738);
  ergo_release_val(__t740);
  ergo_move_into(&source__33, __t741);
  ErgoVal __t742 = source__33; ergo_retain_val(__t742);
  ErgoVal __t743 = YV_STR(stdr_str_lit("Source Color"));
  ErgoVal __t744 = YV_NULLV;
  {
    ErgoVal __parts271[1] = { __t743 };
    ErgoStr* __s272 = stdr_str_from_parts(1, __parts271);
    __t744 = YV_STR(__s272);
  }
  ergo_release_val(__t743);
  ErgoVal __t745 = ergo_cogito_label(__t744);
  ergo_release_val(__t744);
  ErgoVal __t746 = YV_INT(1);
  ErgoVal __t747 = ergo_m_cogito_Label_valign(__t745, __t746);
  ergo_release_val(__t745);
  ergo_release_val(__t746);
  ErgoVal __t748 = ergo_m_cogito_Card_add(__t742, __t747);
  ergo_release_val(__t742);
  ergo_release_val(__t747);
  ergo_release_val(__t748);
  ErgoVal source_controls__34 = YV_NULLV;
  ErgoVal __t749 = ergo_cogito_hstack();
  ErgoVal __t750 = YV_INT(6);
  ErgoVal __t751 = ergo_m_cogito_HStack_set_gap(__t749, __t750);
  ergo_release_val(__t749);
  ergo_release_val(__t750);
  ergo_move_into(&source_controls__34, __t751);
  ErgoVal __t752 = ergo_g_main_source_picker; ergo_retain_val(__t752);
  ErgoVal __t753 = YV_STR(stdr_str_lit("gafu-source-picker"));
  ErgoVal __t754 = YV_NULLV;
  {
    ErgoVal __parts273[1] = { __t753 };
    ErgoStr* __s274 = stdr_str_from_parts(1, __parts273);
    __t754 = YV_STR(__s274);
  }
  ergo_release_val(__t753);
  ErgoVal __t755 = ergo_m_cogito_ColorPicker_set_class(__t752, __t754);
  ergo_release_val(__t752);
  ergo_release_val(__t754);
  ErgoVal __t756 = ergo_g_main_source_hex; ergo_retain_val(__t756);
  ErgoVal __t757 = ergo_m_cogito_ColorPicker_set_hex(__t755, __t756);
  ergo_release_val(__t755);
  ergo_release_val(__t756);
  ErgoVal __t758 = YV_BOOL(true);
  ErgoVal __t759 = ergo_m_cogito_ColorPicker_set_hexpand(__t757, __t758);
  ergo_release_val(__t757);
  ergo_release_val(__t758);
  ErgoVal __t760 = YV_FN(ergo_fn_new(ergo_lambda_7, 1));
  ErgoVal __t761 = ergo_m_cogito_ColorPicker_on_change(__t759, __t760);
  ergo_release_val(__t759);
  ergo_release_val(__t760);
  ergo_release_val(__t761);
  ErgoVal __t762 = source_controls__34; ergo_retain_val(__t762);
  ErgoVal __t763 = ergo_g_main_source_picker; ergo_retain_val(__t763);
  ErgoVal __t764 = ergo_m_cogito_HStack_add(__t762, __t763);
  ergo_release_val(__t762);
  ergo_release_val(__t763);
  ergo_release_val(__t764);
  ErgoVal __t765 = source_controls__34; ergo_retain_val(__t765);
  ErgoVal __t766 = YV_STR(stdr_str_lit("sf:doc.on.doc"));
  ErgoVal __t767 = YV_NULLV;
  {
    ErgoVal __parts275[1] = { __t766 };
    ErgoStr* __s276 = stdr_str_from_parts(1, __parts275);
    __t767 = YV_STR(__s276);
  }
  ergo_release_val(__t766);
  ErgoVal __t768 = ergo_cogito_iconbtn(__t767);
  ergo_release_val(__t767);
  ErgoVal __t769 = YV_STR(stdr_str_lit("iconic"));
  ErgoVal __t770 = YV_NULLV;
  {
    ErgoVal __parts277[1] = { __t769 };
    ErgoStr* __s278 = stdr_str_from_parts(1, __parts277);
    __t770 = YV_STR(__s278);
  }
  ergo_release_val(__t769);
  ErgoVal __t771 = ergo_m_cogito_IconButton_set_class(__t768, __t770);
  ergo_release_val(__t768);
  ergo_release_val(__t770);
  ErgoVal __t772 = YV_FN(ergo_fn_new(ergo_lambda_8, 1));
  ErgoVal __t773 = ergo_m_cogito_IconButton_on_click(__t771, __t772);
  ergo_release_val(__t771);
  ergo_release_val(__t772);
  ErgoVal __t774 = ergo_m_cogito_HStack_add(__t765, __t773);
  ergo_release_val(__t765);
  ergo_release_val(__t773);
  ergo_release_val(__t774);
  ErgoVal __t775 = source__33; ergo_retain_val(__t775);
  ErgoVal __t776 = source_controls__34; ergo_retain_val(__t776);
  ErgoVal __t777 = ergo_m_cogito_Card_add(__t775, __t776);
  ergo_release_val(__t775);
  ergo_release_val(__t776);
  ergo_release_val(__t777);
  ErgoVal __t778 = ergo_g_main_dark_switch; ergo_retain_val(__t778);
  ErgoVal __t779 = YV_BOOL(false);
  ErgoVal __t780 = ergo_m_cogito_SwitchBar_set_checked(__t778, __t779);
  ergo_release_val(__t778);
  ergo_release_val(__t779);
  ErgoVal __t781 = YV_STR(stdr_str_lit("surface-container-high"));
  ErgoVal __t782 = YV_NULLV;
  {
    ErgoVal __parts279[1] = { __t781 };
    ErgoStr* __s280 = stdr_str_from_parts(1, __parts279);
    __t782 = YV_STR(__s280);
  }
  ergo_release_val(__t781);
  ErgoVal __t783 = ergo_m_cogito_SwitchBar_set_class(__t780, __t782);
  ergo_release_val(__t780);
  ergo_release_val(__t782);
  ErgoVal __t784 = YV_FN(ergo_fn_new(ergo_lambda_9, 1));
  ErgoVal __t785 = ergo_m_cogito_SwitchBar_on_change(__t783, __t784);
  ergo_release_val(__t783);
  ergo_release_val(__t784);
  ergo_release_val(__t785);
  ErgoVal __t786 = panel__26; ergo_retain_val(__t786);
  ErgoVal __t787 = upload__27; ergo_retain_val(__t787);
  ErgoVal __t788 = ergo_m_cogito_VStack_add(__t786, __t787);
  ergo_release_val(__t786);
  ergo_release_val(__t787);
  ergo_release_val(__t788);
  ErgoVal __t789 = panel__26; ergo_retain_val(__t789);
  ErgoVal __t790 = tools__31; ergo_retain_val(__t790);
  ErgoVal __t791 = ergo_m_cogito_VStack_add(__t789, __t790);
  ergo_release_val(__t789);
  ergo_release_val(__t790);
  ergo_release_val(__t791);
  ErgoVal __t792 = panel__26; ergo_retain_val(__t792);
  ErgoVal __t793 = modes__32; ergo_retain_val(__t793);
  ErgoVal __t794 = ergo_m_cogito_VStack_add(__t792, __t793);
  ergo_release_val(__t792);
  ergo_release_val(__t793);
  ergo_release_val(__t794);
  ErgoVal __t795 = panel__26; ergo_retain_val(__t795);
  ErgoVal __t796 = source__33; ergo_retain_val(__t796);
  ErgoVal __t797 = ergo_m_cogito_VStack_add(__t795, __t796);
  ergo_release_val(__t795);
  ergo_release_val(__t796);
  ergo_release_val(__t797);
  ErgoVal __t798 = panel__26; ergo_retain_val(__t798);
  ErgoVal __t799 = ergo_g_main_dark_switch; ergo_retain_val(__t799);
  ErgoVal __t800 = ergo_m_cogito_VStack_add(__t798, __t799);
  ergo_release_val(__t798);
  ergo_release_val(__t799);
  ergo_release_val(__t800);
  ErgoVal __t801 = panel__26; ergo_retain_val(__t801);
  ergo_move_into(&__ret, __t801);
  return __ret;
  ergo_release_val(source_controls__34);
  ergo_release_val(source__33);
  ergo_release_val(modes__32);
  ergo_release_val(tools__31);
  ergo_release_val(image__30);
  ergo_release_val(upload_content__29);
  ergo_release_val(upload_hit__28);
  ergo_release_val(upload__27);
  ergo_release_val(panel__26);
  return __ret;
}

static void ergo_stdr___writef(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_stdr___read_line(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr___readf_parse(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr___read_text_file(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr___write_text_file(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr___open_file_dialog(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr___save_file_dialog(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_stdr_writef(ErgoVal a0, ErgoVal a1) {
  ErgoVal __t802 = a0; ergo_retain_val(__t802);
  ErgoVal __t803 = a1; ergo_retain_val(__t803);
  stdr_writef_args(__t802, __t803);
  ergo_release_val(__t802);
  ergo_release_val(__t803);
  ErgoVal __t804 = YV_NULLV;
  ergo_release_val(__t804);
}

static ErgoVal ergo_stdr_readf(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t805 = a0; ergo_retain_val(__t805);
  ErgoVal __t806 = a1; ergo_retain_val(__t806);
  stdr_writef_args(__t805, __t806);
  ergo_release_val(__t805);
  ergo_release_val(__t806);
  ErgoVal __t807 = YV_NULLV;
  ergo_release_val(__t807);
  ErgoVal line__35 = YV_NULLV;
  ErgoVal __t808 = YV_STR(stdr_read_line());
  ergo_move_into(&line__35, __t808);
  ErgoVal parsed__36 = YV_NULLV;
  ErgoVal __t809 = a0; ergo_retain_val(__t809);
  ErgoVal __t810 = line__35; ergo_retain_val(__t810);
  ErgoVal __t811 = a1; ergo_retain_val(__t811);
  ErgoVal __t812 = stdr_readf_parse(__t809, __t810, __t811);
  ergo_release_val(__t809);
  ergo_release_val(__t810);
  ergo_release_val(__t811);
  ergo_move_into(&parsed__36, __t812);
  ErgoArr* __tup1 = stdr_arr_new(2);
  ErgoVal __t813 = YV_ARR(__tup1);
  ErgoVal __t814 = line__35; ergo_retain_val(__t814);
  ergo_arr_add(__tup1, __t814);
  ErgoVal __t815 = parsed__36; ergo_retain_val(__t815);
  ergo_arr_add(__tup1, __t815);
  ergo_move_into(&__ret, __t813);
  return __ret;
  ergo_release_val(parsed__36);
  ergo_release_val(line__35);
  return __ret;
}

static void ergo_stdr_write(ErgoVal a0) {
  ErgoVal __t816 = YV_STR(stdr_str_lit("{}"));
  ErgoVal __t817 = YV_NULLV;
  {
    ErgoVal __parts281[1] = { __t816 };
    ErgoStr* __s282 = stdr_str_from_parts(1, __parts281);
    __t817 = YV_STR(__s282);
  }
  ergo_release_val(__t816);
  ErgoArr* __tup2 = stdr_arr_new(1);
  ErgoVal __t818 = YV_ARR(__tup2);
  ErgoVal __t819 = a0; ergo_retain_val(__t819);
  ergo_arr_add(__tup2, __t819);
  ergo_stdr_writef(__t817, __t818);
  ergo_release_val(__t817);
  ergo_release_val(__t818);
  ErgoVal __t820 = YV_NULLV;
  ergo_release_val(__t820);
}

static ErgoVal ergo_stdr_is_null(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t821 = a0; ergo_retain_val(__t821);
  ErgoVal __t822 = YV_NULLV;
  ErgoVal __t823 = ergo_eq(__t821, __t822);
  ergo_release_val(__t821);
  ergo_release_val(__t822);
  ergo_move_into(&__ret, __t823);
  return __ret;
  return __ret;
}

static ErgoVal ergo_stdr_str(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr___len(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr_len(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t824 = a0; ergo_retain_val(__t824);
  ErgoVal __t825 = YV_INT(stdr_len(__t824));
  ergo_release_val(__t824);
  ergo_move_into(&__ret, __t825);
  return __ret;
  return __ret;
}

static ErgoVal ergo_stdr_read_text_file(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t826 = a0; ergo_retain_val(__t826);
  ErgoVal __t827 = stdr_read_text_file(__t826);
  ergo_release_val(__t826);
  ergo_move_into(&__ret, __t827);
  return __ret;
  return __ret;
}

static ErgoVal ergo_stdr_write_text_file(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t828 = a0; ergo_retain_val(__t828);
  ErgoVal __t829 = a1; ergo_retain_val(__t829);
  ErgoVal __t830 = stdr_write_text_file(__t828, __t829);
  ergo_release_val(__t828);
  ergo_release_val(__t829);
  ergo_move_into(&__ret, __t830);
  return __ret;
  return __ret;
}

static ErgoVal ergo_stdr_open_file_dialog(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t831 = a0; ergo_retain_val(__t831);
  ErgoVal __t832 = a1; ergo_retain_val(__t832);
  ErgoVal __t833 = stdr_open_file_dialog(__t831, __t832);
  ergo_release_val(__t831);
  ergo_release_val(__t832);
  ergo_move_into(&__ret, __t833);
  return __ret;
  return __ret;
}

static ErgoVal ergo_stdr_save_file_dialog(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t834 = a0; ergo_retain_val(__t834);
  ErgoVal __t835 = a1; ergo_retain_val(__t835);
  ErgoVal __t836 = a2; ergo_retain_val(__t836);
  ErgoVal __t837 = stdr_save_file_dialog(__t834, __t835, __t836);
  ergo_release_val(__t834);
  ergo_release_val(__t835);
  ergo_release_val(__t836);
  ergo_move_into(&__ret, __t837);
  return __ret;
  return __ret;
}

static void ergo_cogito___cogito_about_window_set_description(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_about_window_set_icon(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_about_window_set_issue_url(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_about_window_set_website(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_about_window(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_active_indicator(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_app_get_icon(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_app_copy_to_clipboard(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_app_set_accent_color(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_app_set_dark_mode(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_app_set_app_name(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_app_set_appid(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_app_set_ensor_variant(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_app_set_contrast(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_app_set_icon(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_app(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_appbar_add_button(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_appbar_set_controls(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_appbar_set_subtitle(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_appbar_set_title(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_appbar(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_avatar_set_image(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_avatar(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_badge_get_count(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_badge_set_count(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_badge(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_banner_set_action(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_banner_set_icon(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_banner(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_bottom_nav_get_selected(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_bottom_nav_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_bottom_nav_set_items(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_bottom_nav_set_selected(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_bottom_nav(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_bottom_sheet(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_side_sheet(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_side_sheet_set_mode(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_build(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_button_add_menu(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_button_add_menu_section(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_button_set_menu_divider(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_button_get_menu_divider(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_button_set_menu_item_gap(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_button_get_menu_item_gap(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_button_set_menu_vibrant(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_button_get_menu_vibrant(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_menu_set_icon(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_menu_set_shortcut(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_menu_set_submenu(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_menu_set_toggled(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_button_get_size(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_button_on_click(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_button_set_size(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_button_set_text(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_button(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_card(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_card_add_action(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_card_add_overflow(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_card_on_click(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_card_set_header_image(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_card_set_subtitle(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_card_set_variant(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_carousel_get_active_index(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_carousel_item_set_halign(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_carousel_item_set_text(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_carousel_item_set_valign(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_carousel_item(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_carousel_set_active_index(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_carousel(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_checkbox_get_checked(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_checkbox_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_checkbox_set_checked(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_checkbox(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_chip_get_selected(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_chip_on_click(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_chip_on_close(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_chip_set_closable(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_chip_set_selected(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_chip(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_colorpicker_on_change(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_colorpicker(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_app_set_accent_from_image(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_colorpicker_set_hex(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_colorpicker_get_hex(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_container_add(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_align(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_gap(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_halign(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_hexpand(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_margins(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4) {
}

static void ergo_cogito___cogito_container_set_padding(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4) {
}

static void ergo_cogito___cogito_container_set_valign(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_vexpand(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_content_list(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_datepicker_on_change(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_datepicker(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_dialog_close(ErgoVal a0) {
}

static void ergo_cogito___cogito_dialog_remove(ErgoVal a0) {
}

static void ergo_cogito___cogito_dialog_slot_clear(ErgoVal a0) {
}

static void ergo_cogito___cogito_dialog_slot_show(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_dialog_slot(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_dialog(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_divider(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_dropdown_get_selected(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_dropdown_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_dropdown_set_items(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_dropdown_set_selected(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_dropdown(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_empty_page_set_action(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_empty_page_set_description(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_empty_page_set_icon(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_empty_page(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_fab_on_click(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_fab_set_extended(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static ErgoVal ergo_cogito___cogito_fab(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_fab_menu(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_fab_menu_add_item(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_fab_menu_set_color(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_find_children(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_find_parent(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_fixed_set_pos(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
}

static ErgoVal ergo_cogito___cogito_fixed(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_grid_on_activate(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_grid_on_select(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_grid_set_align(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_grid_set_gap(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_grid_set_span(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static ErgoVal ergo_cogito___cogito_grid(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_hstack(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_iconbtn_add_menu(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_iconbtn_add_menu_section(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static ErgoVal ergo_cogito___cogito_iconbtn_get_checked(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_iconbtn_get_color_style(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_iconbtn_get_shape(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_iconbtn_get_size(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_iconbtn_get_toggle(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_iconbtn_get_width(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_iconbtn_set_checked(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_iconbtn_set_color_style(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_iconbtn_set_shape(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_iconbtn_set_size(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_iconbtn_set_toggle(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_iconbtn_set_width(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_iconbtn_set_menu_divider(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_iconbtn_get_menu_divider(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_iconbtn_set_menu_item_gap(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_iconbtn_get_menu_item_gap(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_iconbtn_set_menu_vibrant(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_iconbtn_get_menu_vibrant(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_iconbtn_on_click(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_iconbtn(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_image_set_icon(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_image_set_radius(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_image_set_size(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_image_set_source(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_image(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_drawing_area(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_drawing_area_on_press(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_drawing_area_on_drag(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_drawing_area_on_release(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_drawing_area_on_draw(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_drawing_area_get_x(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_drawing_area_get_y(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_drawing_area_get_pressed(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_drawing_area_clear(ErgoVal a0) {
}

static void ergo_cogito___cogito_canvas_set_color(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_canvas_set_line_width(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_canvas_line(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4) {
}

static void ergo_cogito___cogito_canvas_rect(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4) {
}

static void ergo_cogito___cogito_canvas_fill_rect(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4) {
}

static ErgoVal ergo_cogito___cogito_shape(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_shape_set_preset(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_shape_get_preset(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_shape_set_size(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_shape_get_size(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_shape_set_color(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_shape_set_color_style(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_shape_get_color_style(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_shape_set_vertex(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
}

static ErgoVal ergo_cogito___cogito_shape_get_vertex_x(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_shape_get_vertex_y(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_label_set_align(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_label_set_class(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_label_set_ellipsis(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_label_set_text(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_label_set_wrap(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_label(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_list_on_activate(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_list_on_select(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_list(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_load_sum(ErgoVal a0) {
}

static ErgoVal ergo_cogito___cogito_menu_button(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_nav_rail_get_selected(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_nav_rail_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_nav_rail_set_badges(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_nav_rail_set_items(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_nav_rail_set_selected(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_nav_rail_set_toggle(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_nav_rail(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_node_get_editable(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_node_set_a11y_label(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_node_set_a11y_role(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_node_set_class(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_node_set_disabled(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_node_set_editable(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_node_set_id(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_node_set_tooltip(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_node_window(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_open_url(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_pointer_capture(ErgoVal a0) {
}

static void ergo_cogito___cogito_pointer_release(void) {
}

static ErgoVal ergo_cogito___cogito_progress_get_circular(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_progress_get_indeterminate(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_progress_get_thickness(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_progress_get_value(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_progress_get_wavy(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_progress_set_circular(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_progress_set_indeterminate(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_progress_set_thickness(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_progress_set_value(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_progress_set_wavy(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_progress(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_run(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_timer_timeout(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_timer_interval(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_timer_timeout_for(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_timer_interval_for(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_timer_cancel(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_timer_cancel_for(ErgoVal a0) {
}

static void ergo_cogito___cogito_timer_cancel_all(void) {
}

static void ergo_cogito___cogito_scroller_set_axes(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static ErgoVal ergo_cogito___cogito_scroller(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_searchfield_get_text(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_searchfield_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_searchfield_set_text(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_searchfield(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_buttongroup(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_buttongroup_on_select(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_buttongroup_set_size(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_buttongroup_get_size(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_buttongroup_set_shape(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_buttongroup_get_shape(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_buttongroup_set_connected(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_buttongroup_get_connected(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_node_set_flag(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_node_clear_flag(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_set_script_dir(ErgoVal a0) {
}

static ErgoVal ergo_cogito___cogito_settings_list(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_settings_page(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_settings_row(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_settings_window(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_slider_get_centered(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_slider_get_range_end(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_slider_get_range_start(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_slider_get_size(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_slider_get_value(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_slider_on_change(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_slider_range(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_slider_set_centered(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_slider_set_icon(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_slider_set_range_end(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_slider_set_range_start(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_slider_set_range(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_slider_set_size(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_slider_set_value(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_slider(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_split_button_add_menu(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_split_button_add_menu_section(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_split_button_set_size(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_split_button_set_variant(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_split_button(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_state_get(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_state_new(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_state_set(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_stepper(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_switch_get_checked(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_switch_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_switch_set_checked(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_switch(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_switchbar_get_checked(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_switchbar_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_switchbar_set_checked(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_switchbar(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_tabs_bind(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_tabs_get_selected(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_tabs_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_tabs_set_ids(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_tabs_set_items(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_tabs_set_selected(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_tabs(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_textfield_get_hint(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_textfield_get_text(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_textfield_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_textfield_set_hint(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_textfield_set_text(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_textfield(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_textview_get_text(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_textview_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_textview_set_text(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_textview(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_timepicker_get_hour(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_timepicker_get_minute(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_timepicker_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_timepicker_set_time(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static ErgoVal ergo_cogito___cogito_timepicker(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_tip_view_set_title(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_tip_view(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_toast_on_click(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_toast_set_text(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_toast(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_toasts(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_toolbar(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_toolbar_set_vibrant(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_toolbar_get_vibrant(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_toolbar_set_vertical(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_toolbar_get_vertical(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_treeview(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_view_chooser_bind(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_view_chooser_set_items(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_view_chooser(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_view_dual_set_ratio(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_view_dual(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_view_switcher_add_lazy(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_view_switcher_set_active(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_view_switcher(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_vstack(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_welcome_screen_set_action(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_welcome_screen_set_description(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_welcome_screen_set_icon(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_welcome_screen(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_window_clear_dialog(ErgoVal a0) {
}

static void ergo_cogito___cogito_window_clear_side_sheet(ErgoVal a0) {
}

static void ergo_cogito___cogito_window_set_autosize(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_window_set_builder(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_window_set_dialog(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_window_set_resizable(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_window_set_side_sheet(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_window(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_zstack(void) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_m_cogito_App_run(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t838 = self; ergo_retain_val(__t838);
  ErgoVal __t839 = a0; ergo_retain_val(__t839);
  __cogito_run(__t838, __t839);
  ergo_release_val(__t838);
  ergo_release_val(__t839);
  ErgoVal __t840 = YV_NULLV;
  ergo_release_val(__t840);
  ErgoVal __t841 = self; ergo_retain_val(__t841);
  ergo_move_into(&__ret, __t841);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_App_set_appid(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t842 = self; ergo_retain_val(__t842);
  ErgoVal __t843 = a0; ergo_retain_val(__t843);
  __cogito_app_set_appid(__t842, __t843);
  ergo_release_val(__t842);
  ergo_release_val(__t843);
  ErgoVal __t844 = YV_NULLV;
  ergo_release_val(__t844);
  ErgoVal __t845 = self; ergo_retain_val(__t845);
  ergo_move_into(&__ret, __t845);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_App_set_app_name(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t846 = self; ergo_retain_val(__t846);
  ErgoVal __t847 = a0; ergo_retain_val(__t847);
  __cogito_app_set_app_name(__t846, __t847);
  ergo_release_val(__t846);
  ergo_release_val(__t847);
  ErgoVal __t848 = YV_NULLV;
  ergo_release_val(__t848);
  ErgoVal __t849 = self; ergo_retain_val(__t849);
  ergo_move_into(&__ret, __t849);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_App_set_accent_color(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t850 = self; ergo_retain_val(__t850);
  ErgoVal __t851 = a0; ergo_retain_val(__t851);
  ErgoVal __t852 = a1; ergo_retain_val(__t852);
  __cogito_app_set_accent_color(__t850, __t851, __t852);
  ergo_release_val(__t850);
  ergo_release_val(__t851);
  ergo_release_val(__t852);
  ErgoVal __t853 = YV_NULLV;
  ergo_release_val(__t853);
  ErgoVal __t854 = self; ergo_retain_val(__t854);
  ergo_move_into(&__ret, __t854);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_App_set_dark_mode(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t855 = self; ergo_retain_val(__t855);
  ErgoVal __t856 = a0; ergo_retain_val(__t856);
  ErgoVal __t857 = a1; ergo_retain_val(__t857);
  __cogito_app_set_dark_mode(__t855, __t856, __t857);
  ergo_release_val(__t855);
  ergo_release_val(__t856);
  ergo_release_val(__t857);
  ErgoVal __t858 = YV_NULLV;
  ergo_release_val(__t858);
  ErgoVal __t859 = self; ergo_retain_val(__t859);
  ergo_move_into(&__ret, __t859);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_App_set_accent_from_image(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal out__37 = YV_NULLV;
  ErgoVal __t860 = self; ergo_retain_val(__t860);
  ErgoVal __t861 = a0; ergo_retain_val(__t861);
  ErgoVal __t862 = a1; ergo_retain_val(__t862);
  ErgoVal __t863 = __cogito_app_set_accent_from_image(__t860, __t861, __t862);
  ergo_release_val(__t860);
  ergo_release_val(__t861);
  ergo_release_val(__t862);
  ergo_move_into(&out__37, __t863);
  ErgoVal __t864 = YV_NULLV;
  ErgoVal __t865 = out__37; ergo_retain_val(__t865);
  ErgoVal __t866 = ergo_stdr_is_null(__t865);
  ergo_release_val(__t865);
  bool __b38 = ergo_as_bool(__t866);
  ergo_release_val(__t866);
  if (__b38) {
    ErgoVal __t867 = YV_STR(stdr_str_lit(""));
    ergo_move_into(&__t864, __t867);
  } else {
    ErgoVal __t868 = out__37; ergo_retain_val(__t868);
    ErgoVal __t869 = YV_STR(stdr_to_string(__t868));
    ergo_release_val(__t868);
    ergo_move_into(&__t864, __t869);
  }
  ergo_move_into(&__ret, __t864);
  return __ret;
  ergo_release_val(out__37);
  return __ret;
}

static ErgoVal ergo_m_cogito_App_set_ensor_variant(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t870 = self; ergo_retain_val(__t870);
  ErgoVal __t871 = a0; ergo_retain_val(__t871);
  __cogito_app_set_ensor_variant(__t870, __t871);
  ergo_release_val(__t870);
  ergo_release_val(__t871);
  ErgoVal __t872 = YV_NULLV;
  ergo_release_val(__t872);
  ErgoVal __t873 = self; ergo_retain_val(__t873);
  ergo_move_into(&__ret, __t873);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_App_set_contrast(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t874 = self; ergo_retain_val(__t874);
  ErgoVal __t875 = a0; ergo_retain_val(__t875);
  __cogito_app_set_contrast(__t874, __t875);
  ergo_release_val(__t874);
  ergo_release_val(__t875);
  ErgoVal __t876 = YV_NULLV;
  ergo_release_val(__t876);
  ErgoVal __t877 = self; ergo_retain_val(__t877);
  ergo_move_into(&__ret, __t877);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_App_set_icon(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t878 = self; ergo_retain_val(__t878);
  ErgoVal __t879 = a0; ergo_retain_val(__t879);
  __cogito_app_set_icon(__t878, __t879);
  ergo_release_val(__t878);
  ergo_release_val(__t879);
  ErgoVal __t880 = YV_NULLV;
  ergo_release_val(__t880);
  ErgoVal __t881 = self; ergo_retain_val(__t881);
  ergo_move_into(&__ret, __t881);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_App_get_icon(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t882 = self; ergo_retain_val(__t882);
  ErgoVal __t883 = __cogito_app_get_icon(__t882);
  ergo_release_val(__t882);
  ergo_move_into(&__ret, __t883);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_App_copy_to_clipboard(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t884 = self; ergo_retain_val(__t884);
  ErgoVal __t885 = a0; ergo_retain_val(__t885);
  ErgoVal __t886 = __cogito_app_copy_to_clipboard(__t884, __t885);
  ergo_release_val(__t884);
  ergo_release_val(__t885);
  ergo_move_into(&__ret, __t886);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t887 = self; ergo_retain_val(__t887);
  ErgoVal __t888 = a0; ergo_retain_val(__t888);
  __cogito_container_add(__t887, __t888);
  ergo_release_val(__t887);
  ergo_release_val(__t888);
  ErgoVal __t889 = YV_NULLV;
  ergo_release_val(__t889);
  ErgoVal __t890 = self; ergo_retain_val(__t890);
  ergo_move_into(&__ret, __t890);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t891 = self; ergo_retain_val(__t891);
  ErgoVal __t892 = a0; ergo_retain_val(__t892);
  ErgoVal __t893 = a1; ergo_retain_val(__t893);
  ErgoVal __t894 = a2; ergo_retain_val(__t894);
  ErgoVal __t895 = a3; ergo_retain_val(__t895);
  __cogito_container_set_margins(__t891, __t892, __t893, __t894, __t895);
  ergo_release_val(__t891);
  ergo_release_val(__t892);
  ergo_release_val(__t893);
  ergo_release_val(__t894);
  ergo_release_val(__t895);
  ErgoVal __t896 = YV_NULLV;
  ergo_release_val(__t896);
  ErgoVal __t897 = self; ergo_retain_val(__t897);
  ergo_move_into(&__ret, __t897);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t898 = self; ergo_retain_val(__t898);
  ErgoVal __t899 = a0; ergo_retain_val(__t899);
  ErgoVal __t900 = a1; ergo_retain_val(__t900);
  ErgoVal __t901 = a2; ergo_retain_val(__t901);
  ErgoVal __t902 = a3; ergo_retain_val(__t902);
  __cogito_container_set_padding(__t898, __t899, __t900, __t901, __t902);
  ergo_release_val(__t898);
  ergo_release_val(__t899);
  ergo_release_val(__t900);
  ergo_release_val(__t901);
  ergo_release_val(__t902);
  ErgoVal __t903 = YV_NULLV;
  ergo_release_val(__t903);
  ErgoVal __t904 = self; ergo_retain_val(__t904);
  ergo_move_into(&__ret, __t904);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_set_autosize(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t905 = self; ergo_retain_val(__t905);
  ErgoVal __t906 = a0; ergo_retain_val(__t906);
  __cogito_window_set_autosize(__t905, __t906);
  ergo_release_val(__t905);
  ergo_release_val(__t906);
  ErgoVal __t907 = YV_NULLV;
  ergo_release_val(__t907);
  ErgoVal __t908 = self; ergo_retain_val(__t908);
  ergo_move_into(&__ret, __t908);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_set_resizable(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t909 = self; ergo_retain_val(__t909);
  ErgoVal __t910 = a0; ergo_retain_val(__t910);
  __cogito_window_set_resizable(__t909, __t910);
  ergo_release_val(__t909);
  ergo_release_val(__t910);
  ErgoVal __t911 = YV_NULLV;
  ergo_release_val(__t911);
  ErgoVal __t912 = self; ergo_retain_val(__t912);
  ergo_move_into(&__ret, __t912);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_set_a11y_label(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t913 = self; ergo_retain_val(__t913);
  ErgoVal __t914 = a0; ergo_retain_val(__t914);
  __cogito_node_set_a11y_label(__t913, __t914);
  ergo_release_val(__t913);
  ergo_release_val(__t914);
  ErgoVal __t915 = YV_NULLV;
  ergo_release_val(__t915);
  ErgoVal __t916 = self; ergo_retain_val(__t916);
  ergo_move_into(&__ret, __t916);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_set_a11y_role(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t917 = self; ergo_retain_val(__t917);
  ErgoVal __t918 = a0; ergo_retain_val(__t918);
  __cogito_node_set_a11y_role(__t917, __t918);
  ergo_release_val(__t917);
  ergo_release_val(__t918);
  ErgoVal __t919 = YV_NULLV;
  ergo_release_val(__t919);
  ErgoVal __t920 = self; ergo_retain_val(__t920);
  ergo_move_into(&__ret, __t920);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t921 = self; ergo_retain_val(__t921);
  ErgoVal __t922 = a0; ergo_retain_val(__t922);
  __cogito_node_set_disabled(__t921, __t922);
  ergo_release_val(__t921);
  ergo_release_val(__t922);
  ErgoVal __t923 = YV_NULLV;
  ergo_release_val(__t923);
  ErgoVal __t924 = self; ergo_retain_val(__t924);
  ergo_move_into(&__ret, __t924);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_set_dialog(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t925 = self; ergo_retain_val(__t925);
  ErgoVal __t926 = a0; ergo_retain_val(__t926);
  __cogito_window_set_dialog(__t925, __t926);
  ergo_release_val(__t925);
  ergo_release_val(__t926);
  ErgoVal __t927 = YV_NULLV;
  ergo_release_val(__t927);
  ErgoVal __t928 = self; ergo_retain_val(__t928);
  ergo_move_into(&__ret, __t928);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_clear_dialog(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t929 = self; ergo_retain_val(__t929);
  __cogito_window_clear_dialog(__t929);
  ergo_release_val(__t929);
  ErgoVal __t930 = YV_NULLV;
  ergo_release_val(__t930);
  ErgoVal __t931 = self; ergo_retain_val(__t931);
  ergo_move_into(&__ret, __t931);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_set_side_sheet(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t932 = self; ergo_retain_val(__t932);
  ErgoVal __t933 = a0; ergo_retain_val(__t933);
  __cogito_window_set_side_sheet(__t932, __t933);
  ergo_release_val(__t932);
  ergo_release_val(__t933);
  ErgoVal __t934 = YV_NULLV;
  ergo_release_val(__t934);
  ErgoVal __t935 = self; ergo_retain_val(__t935);
  ergo_move_into(&__ret, __t935);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_clear_side_sheet(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t936 = self; ergo_retain_val(__t936);
  __cogito_window_clear_side_sheet(__t936);
  ergo_release_val(__t936);
  ErgoVal __t937 = YV_NULLV;
  ergo_release_val(__t937);
  ErgoVal __t938 = self; ergo_retain_val(__t938);
  ergo_move_into(&__ret, __t938);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t939 = self; ergo_retain_val(__t939);
  ErgoVal __t940 = a0; ergo_retain_val(__t940);
  __cogito_window_set_builder(__t939, __t940);
  ergo_release_val(__t939);
  ergo_release_val(__t940);
  ErgoVal __t941 = YV_NULLV;
  ergo_release_val(__t941);
  ErgoVal __t942 = self; ergo_retain_val(__t942);
  ErgoVal __t943 = a0; ergo_retain_val(__t943);
  __cogito_build(__t942, __t943);
  ergo_release_val(__t942);
  ergo_release_val(__t943);
  ErgoVal __t944 = YV_NULLV;
  ergo_release_val(__t944);
  ErgoVal __t945 = self; ergo_retain_val(__t945);
  ergo_move_into(&__ret, __t945);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t946 = self; ergo_retain_val(__t946);
  ErgoVal __t947 = a0; ergo_retain_val(__t947);
  __cogito_node_set_class(__t946, __t947);
  ergo_release_val(__t946);
  ergo_release_val(__t947);
  ErgoVal __t948 = YV_NULLV;
  ergo_release_val(__t948);
  ErgoVal __t949 = self; ergo_retain_val(__t949);
  ergo_move_into(&__ret, __t949);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Window_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t950 = self; ergo_retain_val(__t950);
  ErgoVal __t951 = a0; ergo_retain_val(__t951);
  __cogito_node_set_id(__t950, __t951);
  ergo_release_val(__t950);
  ergo_release_val(__t951);
  ErgoVal __t952 = YV_NULLV;
  ergo_release_val(__t952);
  ErgoVal __t953 = self; ergo_retain_val(__t953);
  ergo_move_into(&__ret, __t953);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AppBar_add_button(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t954 = self; ergo_retain_val(__t954);
  ErgoVal __t955 = a0; ergo_retain_val(__t955);
  ErgoVal __t956 = a1; ergo_retain_val(__t956);
  ErgoVal __t957 = __cogito_appbar_add_button(__t954, __t955, __t956);
  ergo_release_val(__t954);
  ergo_release_val(__t955);
  ergo_release_val(__t956);
  ergo_move_into(&__ret, __t957);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AppBar_set_window_controls(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t958 = self; ergo_retain_val(__t958);
  ErgoVal __t959 = a0; ergo_retain_val(__t959);
  __cogito_appbar_set_controls(__t958, __t959);
  ergo_release_val(__t958);
  ergo_release_val(__t959);
  ErgoVal __t960 = YV_NULLV;
  ergo_release_val(__t960);
  ErgoVal __t961 = self; ergo_retain_val(__t961);
  ergo_move_into(&__ret, __t961);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AppBar_set_title(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t962 = self; ergo_retain_val(__t962);
  ErgoVal __t963 = a0; ergo_retain_val(__t963);
  __cogito_appbar_set_title(__t962, __t963);
  ergo_release_val(__t962);
  ergo_release_val(__t963);
  ErgoVal __t964 = YV_NULLV;
  ergo_release_val(__t964);
  ErgoVal __t965 = self; ergo_retain_val(__t965);
  ergo_move_into(&__ret, __t965);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AppBar_set_title_widget(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t966 = a0; ergo_retain_val(__t966);
  ErgoVal __t967 = YV_STR(stdr_str_lit("appbar-title"));
  ErgoVal __t968 = YV_NULLV;
  {
    ErgoVal __parts283[1] = { __t967 };
    ErgoStr* __s284 = stdr_str_from_parts(1, __parts283);
    __t968 = YV_STR(__s284);
  }
  ergo_release_val(__t967);
  __cogito_node_set_class(__t966, __t968);
  ergo_release_val(__t966);
  ergo_release_val(__t968);
  ErgoVal __t969 = YV_NULLV;
  ergo_release_val(__t969);
  ErgoVal __t970 = self; ergo_retain_val(__t970);
  ErgoVal __t971 = a0; ergo_retain_val(__t971);
  __cogito_container_add(__t970, __t971);
  ergo_release_val(__t970);
  ergo_release_val(__t971);
  ErgoVal __t972 = YV_NULLV;
  ergo_release_val(__t972);
  ErgoVal __t973 = self; ergo_retain_val(__t973);
  ergo_move_into(&__ret, __t973);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AppBar_set_subtitle(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t974 = self; ergo_retain_val(__t974);
  ErgoVal __t975 = a0; ergo_retain_val(__t975);
  __cogito_appbar_set_subtitle(__t974, __t975);
  ergo_release_val(__t974);
  ergo_release_val(__t975);
  ErgoVal __t976 = YV_NULLV;
  ergo_release_val(__t976);
  ErgoVal __t977 = self; ergo_retain_val(__t977);
  ergo_move_into(&__ret, __t977);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AppBar_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t978 = self; ergo_retain_val(__t978);
  ErgoVal __t979 = a0; ergo_retain_val(__t979);
  __cogito_container_set_hexpand(__t978, __t979);
  ergo_release_val(__t978);
  ergo_release_val(__t979);
  ErgoVal __t980 = YV_NULLV;
  ergo_release_val(__t980);
  ErgoVal __t981 = self; ergo_retain_val(__t981);
  ergo_move_into(&__ret, __t981);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AppBar_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t982 = self; ergo_retain_val(__t982);
  ErgoVal __t983 = a0; ergo_retain_val(__t983);
  __cogito_container_set_vexpand(__t982, __t983);
  ergo_release_val(__t982);
  ergo_release_val(__t983);
  ErgoVal __t984 = YV_NULLV;
  ergo_release_val(__t984);
  ErgoVal __t985 = self; ergo_retain_val(__t985);
  ergo_move_into(&__ret, __t985);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AppBar_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t986 = self; ergo_retain_val(__t986);
  ErgoVal __t987 = a0; ergo_retain_val(__t987);
  __cogito_node_set_disabled(__t986, __t987);
  ergo_release_val(__t986);
  ergo_release_val(__t987);
  ErgoVal __t988 = YV_NULLV;
  ergo_release_val(__t988);
  ErgoVal __t989 = self; ergo_retain_val(__t989);
  ergo_move_into(&__ret, __t989);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AppBar_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t990 = self; ergo_retain_val(__t990);
  ErgoVal __t991 = a0; ergo_retain_val(__t991);
  __cogito_node_set_class(__t990, __t991);
  ergo_release_val(__t990);
  ergo_release_val(__t991);
  ErgoVal __t992 = YV_NULLV;
  ergo_release_val(__t992);
  ErgoVal __t993 = self; ergo_retain_val(__t993);
  ergo_move_into(&__ret, __t993);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AppBar_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t994 = self; ergo_retain_val(__t994);
  ErgoVal __t995 = a0; ergo_retain_val(__t995);
  __cogito_node_set_id(__t994, __t995);
  ergo_release_val(__t994);
  ergo_release_val(__t995);
  ErgoVal __t996 = YV_NULLV;
  ergo_release_val(__t996);
  ErgoVal __t997 = self; ergo_retain_val(__t997);
  ergo_move_into(&__ret, __t997);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FABMenu_add_item(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t998 = self; ergo_retain_val(__t998);
  ErgoVal __t999 = a0; ergo_retain_val(__t999);
  ErgoVal __t1000 = a1; ergo_retain_val(__t1000);
  __cogito_fab_menu_add_item(__t998, __t999, __t1000);
  ergo_release_val(__t998);
  ergo_release_val(__t999);
  ergo_release_val(__t1000);
  ErgoVal __t1001 = YV_NULLV;
  ergo_release_val(__t1001);
  ErgoVal __t1002 = self; ergo_retain_val(__t1002);
  ergo_move_into(&__ret, __t1002);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FABMenu_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1003 = self; ergo_retain_val(__t1003);
  ErgoVal __t1004 = a0; ergo_retain_val(__t1004);
  __cogito_container_set_hexpand(__t1003, __t1004);
  ergo_release_val(__t1003);
  ergo_release_val(__t1004);
  ErgoVal __t1005 = YV_NULLV;
  ergo_release_val(__t1005);
  ErgoVal __t1006 = self; ergo_retain_val(__t1006);
  ergo_move_into(&__ret, __t1006);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FABMenu_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1007 = self; ergo_retain_val(__t1007);
  ErgoVal __t1008 = a0; ergo_retain_val(__t1008);
  __cogito_container_set_vexpand(__t1007, __t1008);
  ergo_release_val(__t1007);
  ergo_release_val(__t1008);
  ErgoVal __t1009 = YV_NULLV;
  ergo_release_val(__t1009);
  ErgoVal __t1010 = self; ergo_retain_val(__t1010);
  ergo_move_into(&__ret, __t1010);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FABMenu_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1011 = self; ergo_retain_val(__t1011);
  ErgoVal __t1012 = a0; ergo_retain_val(__t1012);
  __cogito_node_set_disabled(__t1011, __t1012);
  ergo_release_val(__t1011);
  ergo_release_val(__t1012);
  ErgoVal __t1013 = YV_NULLV;
  ergo_release_val(__t1013);
  ErgoVal __t1014 = self; ergo_retain_val(__t1014);
  ergo_move_into(&__ret, __t1014);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FABMenu_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1015 = self; ergo_retain_val(__t1015);
  ErgoVal __t1016 = a0; ergo_retain_val(__t1016);
  __cogito_node_set_class(__t1015, __t1016);
  ergo_release_val(__t1015);
  ergo_release_val(__t1016);
  ErgoVal __t1017 = YV_NULLV;
  ergo_release_val(__t1017);
  ErgoVal __t1018 = self; ergo_retain_val(__t1018);
  ergo_move_into(&__ret, __t1018);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FABMenu_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1019 = self; ergo_retain_val(__t1019);
  ErgoVal __t1020 = a0; ergo_retain_val(__t1020);
  __cogito_node_set_id(__t1019, __t1020);
  ergo_release_val(__t1019);
  ergo_release_val(__t1020);
  ErgoVal __t1021 = YV_NULLV;
  ergo_release_val(__t1021);
  ErgoVal __t1022 = self; ergo_retain_val(__t1022);
  ergo_move_into(&__ret, __t1022);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1023 = self; ergo_retain_val(__t1023);
  ErgoVal __t1024 = a0; ergo_retain_val(__t1024);
  ErgoVal __t1025 = a1; ergo_retain_val(__t1025);
  ErgoVal __t1026 = a2; ergo_retain_val(__t1026);
  ErgoVal __t1027 = a3; ergo_retain_val(__t1027);
  __cogito_container_set_margins(__t1023, __t1024, __t1025, __t1026, __t1027);
  ergo_release_val(__t1023);
  ergo_release_val(__t1024);
  ergo_release_val(__t1025);
  ergo_release_val(__t1026);
  ergo_release_val(__t1027);
  ErgoVal __t1028 = YV_NULLV;
  ergo_release_val(__t1028);
  ErgoVal __t1029 = self; ergo_retain_val(__t1029);
  ergo_move_into(&__ret, __t1029);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1030 = self; ergo_retain_val(__t1030);
  ErgoVal __t1031 = a0; ergo_retain_val(__t1031);
  ErgoVal __t1032 = a1; ergo_retain_val(__t1032);
  ErgoVal __t1033 = a2; ergo_retain_val(__t1033);
  ErgoVal __t1034 = a3; ergo_retain_val(__t1034);
  __cogito_container_set_padding(__t1030, __t1031, __t1032, __t1033, __t1034);
  ergo_release_val(__t1030);
  ergo_release_val(__t1031);
  ergo_release_val(__t1032);
  ergo_release_val(__t1033);
  ergo_release_val(__t1034);
  ErgoVal __t1035 = YV_NULLV;
  ergo_release_val(__t1035);
  ErgoVal __t1036 = self; ergo_retain_val(__t1036);
  ergo_move_into(&__ret, __t1036);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1037 = self; ergo_retain_val(__t1037);
  ErgoVal __t1038 = a0; ergo_retain_val(__t1038);
  __cogito_container_set_align(__t1037, __t1038);
  ergo_release_val(__t1037);
  ergo_release_val(__t1038);
  ErgoVal __t1039 = YV_NULLV;
  ergo_release_val(__t1039);
  ErgoVal __t1040 = self; ergo_retain_val(__t1040);
  ergo_move_into(&__ret, __t1040);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1041 = self; ergo_retain_val(__t1041);
  ErgoVal __t1042 = a0; ergo_retain_val(__t1042);
  __cogito_container_set_halign(__t1041, __t1042);
  ergo_release_val(__t1041);
  ergo_release_val(__t1042);
  ErgoVal __t1043 = YV_NULLV;
  ergo_release_val(__t1043);
  ErgoVal __t1044 = self; ergo_retain_val(__t1044);
  ergo_move_into(&__ret, __t1044);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1045 = self; ergo_retain_val(__t1045);
  ErgoVal __t1046 = a0; ergo_retain_val(__t1046);
  __cogito_container_set_valign(__t1045, __t1046);
  ergo_release_val(__t1045);
  ergo_release_val(__t1046);
  ErgoVal __t1047 = YV_NULLV;
  ergo_release_val(__t1047);
  ErgoVal __t1048 = self; ergo_retain_val(__t1048);
  ergo_move_into(&__ret, __t1048);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1049 = self; ergo_retain_val(__t1049);
  ErgoVal __t1050 = a0; ergo_retain_val(__t1050);
  __cogito_container_set_halign(__t1049, __t1050);
  ergo_release_val(__t1049);
  ergo_release_val(__t1050);
  ErgoVal __t1051 = YV_NULLV;
  ergo_release_val(__t1051);
  ErgoVal __t1052 = self; ergo_retain_val(__t1052);
  ergo_move_into(&__ret, __t1052);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1053 = self; ergo_retain_val(__t1053);
  ErgoVal __t1054 = a0; ergo_retain_val(__t1054);
  __cogito_container_set_valign(__t1053, __t1054);
  ergo_release_val(__t1053);
  ergo_release_val(__t1054);
  ErgoVal __t1055 = YV_NULLV;
  ergo_release_val(__t1055);
  ErgoVal __t1056 = self; ergo_retain_val(__t1056);
  ergo_move_into(&__ret, __t1056);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_icon(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1057 = self; ergo_retain_val(__t1057);
  ErgoVal __t1058 = a0; ergo_retain_val(__t1058);
  __cogito_image_set_icon(__t1057, __t1058);
  ergo_release_val(__t1057);
  ergo_release_val(__t1058);
  ErgoVal __t1059 = YV_NULLV;
  ergo_release_val(__t1059);
  ErgoVal __t1060 = self; ergo_retain_val(__t1060);
  ergo_move_into(&__ret, __t1060);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_source(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1061 = self; ergo_retain_val(__t1061);
  ErgoVal __t1062 = a0; ergo_retain_val(__t1062);
  __cogito_image_set_icon(__t1061, __t1062);
  ergo_release_val(__t1061);
  ergo_release_val(__t1062);
  ErgoVal __t1063 = YV_NULLV;
  ergo_release_val(__t1063);
  ErgoVal __t1064 = self; ergo_retain_val(__t1064);
  ergo_move_into(&__ret, __t1064);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_size(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1065 = self; ergo_retain_val(__t1065);
  ErgoVal __t1066 = a0; ergo_retain_val(__t1066);
  ErgoVal __t1067 = a1; ergo_retain_val(__t1067);
  __cogito_image_set_size(__t1065, __t1066, __t1067);
  ergo_release_val(__t1065);
  ergo_release_val(__t1066);
  ergo_release_val(__t1067);
  ErgoVal __t1068 = YV_NULLV;
  ergo_release_val(__t1068);
  ErgoVal __t1069 = self; ergo_retain_val(__t1069);
  ergo_move_into(&__ret, __t1069);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_radius(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1070 = self; ergo_retain_val(__t1070);
  ErgoVal __t1071 = a0; ergo_retain_val(__t1071);
  __cogito_image_set_radius(__t1070, __t1071);
  ergo_release_val(__t1070);
  ergo_release_val(__t1071);
  ErgoVal __t1072 = YV_NULLV;
  ergo_release_val(__t1072);
  ErgoVal __t1073 = self; ergo_retain_val(__t1073);
  ergo_move_into(&__ret, __t1073);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_alt_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1074 = self; ergo_retain_val(__t1074);
  ErgoVal __t1075 = a0; ergo_retain_val(__t1075);
  __cogito_node_set_a11y_label(__t1074, __t1075);
  ergo_release_val(__t1074);
  ergo_release_val(__t1075);
  ErgoVal __t1076 = YV_NULLV;
  ergo_release_val(__t1076);
  ErgoVal __t1077 = self; ergo_retain_val(__t1077);
  ergo_move_into(&__ret, __t1077);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1078 = self; ergo_retain_val(__t1078);
  ErgoVal __t1079 = a0; ergo_retain_val(__t1079);
  __cogito_container_set_hexpand(__t1078, __t1079);
  ergo_release_val(__t1078);
  ergo_release_val(__t1079);
  ErgoVal __t1080 = YV_NULLV;
  ergo_release_val(__t1080);
  ErgoVal __t1081 = self; ergo_retain_val(__t1081);
  ergo_move_into(&__ret, __t1081);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1082 = self; ergo_retain_val(__t1082);
  ErgoVal __t1083 = a0; ergo_retain_val(__t1083);
  __cogito_container_set_vexpand(__t1082, __t1083);
  ergo_release_val(__t1082);
  ergo_release_val(__t1083);
  ErgoVal __t1084 = YV_NULLV;
  ergo_release_val(__t1084);
  ErgoVal __t1085 = self; ergo_retain_val(__t1085);
  ergo_move_into(&__ret, __t1085);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1086 = self; ergo_retain_val(__t1086);
  ErgoVal __t1087 = a0; ergo_retain_val(__t1087);
  __cogito_node_set_disabled(__t1086, __t1087);
  ergo_release_val(__t1086);
  ergo_release_val(__t1087);
  ErgoVal __t1088 = YV_NULLV;
  ergo_release_val(__t1088);
  ErgoVal __t1089 = self; ergo_retain_val(__t1089);
  ergo_move_into(&__ret, __t1089);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1090 = self; ergo_retain_val(__t1090);
  ErgoVal __t1091 = a0; ergo_retain_val(__t1091);
  __cogito_node_set_class(__t1090, __t1091);
  ergo_release_val(__t1090);
  ergo_release_val(__t1091);
  ErgoVal __t1092 = YV_NULLV;
  ergo_release_val(__t1092);
  ErgoVal __t1093 = self; ergo_retain_val(__t1093);
  ergo_move_into(&__ret, __t1093);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Image_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1094 = self; ergo_retain_val(__t1094);
  ErgoVal __t1095 = a0; ergo_retain_val(__t1095);
  __cogito_node_set_id(__t1094, __t1095);
  ergo_release_val(__t1094);
  ergo_release_val(__t1095);
  ErgoVal __t1096 = YV_NULLV;
  ergo_release_val(__t1096);
  ErgoVal __t1097 = self; ergo_retain_val(__t1097);
  ergo_move_into(&__ret, __t1097);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dialog_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1098 = self; ergo_retain_val(__t1098);
  ErgoVal __t1099 = a0; ergo_retain_val(__t1099);
  __cogito_container_add(__t1098, __t1099);
  ergo_release_val(__t1098);
  ergo_release_val(__t1099);
  ErgoVal __t1100 = YV_NULLV;
  ergo_release_val(__t1100);
  ErgoVal __t1101 = self; ergo_retain_val(__t1101);
  ergo_move_into(&__ret, __t1101);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dialog_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1102 = self; ergo_retain_val(__t1102);
  ErgoVal __t1103 = a0; ergo_retain_val(__t1103);
  ErgoVal __t1104 = a1; ergo_retain_val(__t1104);
  ErgoVal __t1105 = a2; ergo_retain_val(__t1105);
  ErgoVal __t1106 = a3; ergo_retain_val(__t1106);
  __cogito_container_set_padding(__t1102, __t1103, __t1104, __t1105, __t1106);
  ergo_release_val(__t1102);
  ergo_release_val(__t1103);
  ergo_release_val(__t1104);
  ergo_release_val(__t1105);
  ergo_release_val(__t1106);
  ErgoVal __t1107 = YV_NULLV;
  ergo_release_val(__t1107);
  ErgoVal __t1108 = self; ergo_retain_val(__t1108);
  ergo_move_into(&__ret, __t1108);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dialog_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1109 = self; ergo_retain_val(__t1109);
  ErgoVal __t1110 = a0; ergo_retain_val(__t1110);
  ErgoVal __t1111 = a1; ergo_retain_val(__t1111);
  ErgoVal __t1112 = a2; ergo_retain_val(__t1112);
  ErgoVal __t1113 = a3; ergo_retain_val(__t1113);
  __cogito_container_set_margins(__t1109, __t1110, __t1111, __t1112, __t1113);
  ergo_release_val(__t1109);
  ergo_release_val(__t1110);
  ergo_release_val(__t1111);
  ergo_release_val(__t1112);
  ergo_release_val(__t1113);
  ErgoVal __t1114 = YV_NULLV;
  ergo_release_val(__t1114);
  ErgoVal __t1115 = self; ergo_retain_val(__t1115);
  ergo_move_into(&__ret, __t1115);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dialog_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1116 = self; ergo_retain_val(__t1116);
  ErgoVal __t1117 = a0; ergo_retain_val(__t1117);
  __cogito_build(__t1116, __t1117);
  ergo_release_val(__t1116);
  ergo_release_val(__t1117);
  ErgoVal __t1118 = YV_NULLV;
  ergo_release_val(__t1118);
  ErgoVal __t1119 = self; ergo_retain_val(__t1119);
  ergo_move_into(&__ret, __t1119);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dialog_window(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1120 = self; ergo_retain_val(__t1120);
  ErgoVal __t1121 = __cogito_node_window(__t1120);
  ergo_release_val(__t1120);
  ergo_move_into(&__ret, __t1121);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dialog_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1122 = self; ergo_retain_val(__t1122);
  ErgoVal __t1123 = a0; ergo_retain_val(__t1123);
  __cogito_container_set_hexpand(__t1122, __t1123);
  ergo_release_val(__t1122);
  ergo_release_val(__t1123);
  ErgoVal __t1124 = YV_NULLV;
  ergo_release_val(__t1124);
  ErgoVal __t1125 = self; ergo_retain_val(__t1125);
  ergo_move_into(&__ret, __t1125);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dialog_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1126 = self; ergo_retain_val(__t1126);
  ErgoVal __t1127 = a0; ergo_retain_val(__t1127);
  __cogito_container_set_vexpand(__t1126, __t1127);
  ergo_release_val(__t1126);
  ergo_release_val(__t1127);
  ErgoVal __t1128 = YV_NULLV;
  ergo_release_val(__t1128);
  ErgoVal __t1129 = self; ergo_retain_val(__t1129);
  ergo_move_into(&__ret, __t1129);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dialog_close(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1130 = self; ergo_retain_val(__t1130);
  __cogito_dialog_close(__t1130);
  ergo_release_val(__t1130);
  ErgoVal __t1131 = YV_NULLV;
  ergo_release_val(__t1131);
  ErgoVal __t1132 = self; ergo_retain_val(__t1132);
  ergo_move_into(&__ret, __t1132);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dialog_remove(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1133 = self; ergo_retain_val(__t1133);
  __cogito_dialog_remove(__t1133);
  ergo_release_val(__t1133);
  ErgoVal __t1134 = YV_NULLV;
  ergo_release_val(__t1134);
  ErgoVal __t1135 = self; ergo_retain_val(__t1135);
  ergo_move_into(&__ret, __t1135);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dialog_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1136 = self; ergo_retain_val(__t1136);
  ErgoVal __t1137 = a0; ergo_retain_val(__t1137);
  __cogito_node_set_disabled(__t1136, __t1137);
  ergo_release_val(__t1136);
  ergo_release_val(__t1137);
  ErgoVal __t1138 = YV_NULLV;
  ergo_release_val(__t1138);
  ErgoVal __t1139 = self; ergo_retain_val(__t1139);
  ergo_move_into(&__ret, __t1139);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dialog_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1140 = self; ergo_retain_val(__t1140);
  ErgoVal __t1141 = a0; ergo_retain_val(__t1141);
  __cogito_node_set_class(__t1140, __t1141);
  ergo_release_val(__t1140);
  ergo_release_val(__t1141);
  ErgoVal __t1142 = YV_NULLV;
  ergo_release_val(__t1142);
  ErgoVal __t1143 = self; ergo_retain_val(__t1143);
  ergo_move_into(&__ret, __t1143);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dialog_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1144 = self; ergo_retain_val(__t1144);
  ErgoVal __t1145 = a0; ergo_retain_val(__t1145);
  __cogito_node_set_id(__t1144, __t1145);
  ergo_release_val(__t1144);
  ergo_release_val(__t1145);
  ErgoVal __t1146 = YV_NULLV;
  ergo_release_val(__t1146);
  ErgoVal __t1147 = self; ergo_retain_val(__t1147);
  ergo_move_into(&__ret, __t1147);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DialogSlot_show(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1148 = self; ergo_retain_val(__t1148);
  ErgoVal __t1149 = a0; ergo_retain_val(__t1149);
  __cogito_dialog_slot_show(__t1148, __t1149);
  ergo_release_val(__t1148);
  ergo_release_val(__t1149);
  ErgoVal __t1150 = YV_NULLV;
  ergo_release_val(__t1150);
  ErgoVal __t1151 = self; ergo_retain_val(__t1151);
  ergo_move_into(&__ret, __t1151);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DialogSlot_clear(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1152 = self; ergo_retain_val(__t1152);
  __cogito_dialog_slot_clear(__t1152);
  ergo_release_val(__t1152);
  ErgoVal __t1153 = YV_NULLV;
  ergo_release_val(__t1153);
  ErgoVal __t1154 = self; ergo_retain_val(__t1154);
  ergo_move_into(&__ret, __t1154);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DialogSlot_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1155 = self; ergo_retain_val(__t1155);
  ErgoVal __t1156 = a0; ergo_retain_val(__t1156);
  __cogito_container_set_hexpand(__t1155, __t1156);
  ergo_release_val(__t1155);
  ergo_release_val(__t1156);
  ErgoVal __t1157 = YV_NULLV;
  ergo_release_val(__t1157);
  ErgoVal __t1158 = self; ergo_retain_val(__t1158);
  ergo_move_into(&__ret, __t1158);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DialogSlot_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1159 = self; ergo_retain_val(__t1159);
  ErgoVal __t1160 = a0; ergo_retain_val(__t1160);
  __cogito_container_set_vexpand(__t1159, __t1160);
  ergo_release_val(__t1159);
  ergo_release_val(__t1160);
  ErgoVal __t1161 = YV_NULLV;
  ergo_release_val(__t1161);
  ErgoVal __t1162 = self; ergo_retain_val(__t1162);
  ergo_move_into(&__ret, __t1162);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DialogSlot_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1163 = self; ergo_retain_val(__t1163);
  ErgoVal __t1164 = a0; ergo_retain_val(__t1164);
  __cogito_node_set_disabled(__t1163, __t1164);
  ergo_release_val(__t1163);
  ergo_release_val(__t1164);
  ErgoVal __t1165 = YV_NULLV;
  ergo_release_val(__t1165);
  ErgoVal __t1166 = self; ergo_retain_val(__t1166);
  ergo_move_into(&__ret, __t1166);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DialogSlot_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1167 = self; ergo_retain_val(__t1167);
  ErgoVal __t1168 = a0; ergo_retain_val(__t1168);
  __cogito_node_set_class(__t1167, __t1168);
  ergo_release_val(__t1167);
  ergo_release_val(__t1168);
  ErgoVal __t1169 = YV_NULLV;
  ergo_release_val(__t1169);
  ErgoVal __t1170 = self; ergo_retain_val(__t1170);
  ergo_move_into(&__ret, __t1170);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DialogSlot_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1171 = self; ergo_retain_val(__t1171);
  ErgoVal __t1172 = a0; ergo_retain_val(__t1172);
  __cogito_node_set_id(__t1171, __t1172);
  ergo_release_val(__t1171);
  ergo_release_val(__t1172);
  ErgoVal __t1173 = YV_NULLV;
  ergo_release_val(__t1173);
  ErgoVal __t1174 = self; ergo_retain_val(__t1174);
  ergo_move_into(&__ret, __t1174);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1175 = self; ergo_retain_val(__t1175);
  ErgoVal __t1176 = a0; ergo_retain_val(__t1176);
  __cogito_container_add(__t1175, __t1176);
  ergo_release_val(__t1175);
  ergo_release_val(__t1176);
  ErgoVal __t1177 = YV_NULLV;
  ergo_release_val(__t1177);
  ErgoVal __t1178 = self; ergo_retain_val(__t1178);
  ergo_move_into(&__ret, __t1178);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1179 = self; ergo_retain_val(__t1179);
  ErgoVal __t1180 = a0; ergo_retain_val(__t1180);
  ErgoVal __t1181 = a1; ergo_retain_val(__t1181);
  ErgoVal __t1182 = a2; ergo_retain_val(__t1182);
  ErgoVal __t1183 = a3; ergo_retain_val(__t1183);
  __cogito_container_set_margins(__t1179, __t1180, __t1181, __t1182, __t1183);
  ergo_release_val(__t1179);
  ergo_release_val(__t1180);
  ergo_release_val(__t1181);
  ergo_release_val(__t1182);
  ergo_release_val(__t1183);
  ErgoVal __t1184 = YV_NULLV;
  ergo_release_val(__t1184);
  ErgoVal __t1185 = self; ergo_retain_val(__t1185);
  ergo_move_into(&__ret, __t1185);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1186 = self; ergo_retain_val(__t1186);
  ErgoVal __t1187 = a0; ergo_retain_val(__t1187);
  ErgoVal __t1188 = a1; ergo_retain_val(__t1188);
  ErgoVal __t1189 = a2; ergo_retain_val(__t1189);
  ErgoVal __t1190 = a3; ergo_retain_val(__t1190);
  __cogito_container_set_padding(__t1186, __t1187, __t1188, __t1189, __t1190);
  ergo_release_val(__t1186);
  ergo_release_val(__t1187);
  ergo_release_val(__t1188);
  ergo_release_val(__t1189);
  ergo_release_val(__t1190);
  ErgoVal __t1191 = YV_NULLV;
  ergo_release_val(__t1191);
  ErgoVal __t1192 = self; ergo_retain_val(__t1192);
  ergo_move_into(&__ret, __t1192);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1193 = self; ergo_retain_val(__t1193);
  ErgoVal __t1194 = a0; ergo_retain_val(__t1194);
  __cogito_container_set_align(__t1193, __t1194);
  ergo_release_val(__t1193);
  ergo_release_val(__t1194);
  ErgoVal __t1195 = YV_NULLV;
  ergo_release_val(__t1195);
  ErgoVal __t1196 = self; ergo_retain_val(__t1196);
  ergo_move_into(&__ret, __t1196);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1197 = self; ergo_retain_val(__t1197);
  ErgoVal __t1198 = a0; ergo_retain_val(__t1198);
  __cogito_container_set_halign(__t1197, __t1198);
  ergo_release_val(__t1197);
  ergo_release_val(__t1198);
  ErgoVal __t1199 = YV_NULLV;
  ergo_release_val(__t1199);
  ErgoVal __t1200 = self; ergo_retain_val(__t1200);
  ergo_move_into(&__ret, __t1200);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1201 = self; ergo_retain_val(__t1201);
  ErgoVal __t1202 = a0; ergo_retain_val(__t1202);
  __cogito_container_set_valign(__t1201, __t1202);
  ergo_release_val(__t1201);
  ergo_release_val(__t1202);
  ErgoVal __t1203 = YV_NULLV;
  ergo_release_val(__t1203);
  ErgoVal __t1204 = self; ergo_retain_val(__t1204);
  ergo_move_into(&__ret, __t1204);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1205 = self; ergo_retain_val(__t1205);
  ErgoVal __t1206 = a0; ergo_retain_val(__t1206);
  __cogito_container_set_hexpand(__t1205, __t1206);
  ergo_release_val(__t1205);
  ergo_release_val(__t1206);
  ErgoVal __t1207 = YV_NULLV;
  ergo_release_val(__t1207);
  ErgoVal __t1208 = self; ergo_retain_val(__t1208);
  ergo_move_into(&__ret, __t1208);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1209 = self; ergo_retain_val(__t1209);
  ErgoVal __t1210 = a0; ergo_retain_val(__t1210);
  __cogito_container_set_vexpand(__t1209, __t1210);
  ergo_release_val(__t1209);
  ergo_release_val(__t1210);
  ErgoVal __t1211 = YV_NULLV;
  ergo_release_val(__t1211);
  ErgoVal __t1212 = self; ergo_retain_val(__t1212);
  ergo_move_into(&__ret, __t1212);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_set_gap(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1213 = self; ergo_retain_val(__t1213);
  ErgoVal __t1214 = a0; ergo_retain_val(__t1214);
  __cogito_container_set_gap(__t1213, __t1214);
  ergo_release_val(__t1213);
  ergo_release_val(__t1214);
  ErgoVal __t1215 = YV_NULLV;
  ergo_release_val(__t1215);
  ErgoVal __t1216 = self; ergo_retain_val(__t1216);
  ergo_move_into(&__ret, __t1216);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1217 = self; ergo_retain_val(__t1217);
  ErgoVal __t1218 = YV_INT(0);
  __cogito_container_set_align(__t1217, __t1218);
  ergo_release_val(__t1217);
  ergo_release_val(__t1218);
  ErgoVal __t1219 = YV_NULLV;
  ergo_release_val(__t1219);
  ErgoVal __t1220 = self; ergo_retain_val(__t1220);
  ergo_move_into(&__ret, __t1220);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1221 = self; ergo_retain_val(__t1221);
  ErgoVal __t1222 = YV_INT(1);
  __cogito_container_set_align(__t1221, __t1222);
  ergo_release_val(__t1221);
  ergo_release_val(__t1222);
  ErgoVal __t1223 = YV_NULLV;
  ergo_release_val(__t1223);
  ErgoVal __t1224 = self; ergo_retain_val(__t1224);
  ergo_move_into(&__ret, __t1224);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1225 = self; ergo_retain_val(__t1225);
  ErgoVal __t1226 = YV_INT(2);
  __cogito_container_set_align(__t1225, __t1226);
  ergo_release_val(__t1225);
  ergo_release_val(__t1226);
  ErgoVal __t1227 = YV_NULLV;
  ergo_release_val(__t1227);
  ErgoVal __t1228 = self; ergo_retain_val(__t1228);
  ergo_move_into(&__ret, __t1228);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1229 = self; ergo_retain_val(__t1229);
  ErgoVal __t1230 = a0; ergo_retain_val(__t1230);
  __cogito_build(__t1229, __t1230);
  ergo_release_val(__t1229);
  ergo_release_val(__t1230);
  ErgoVal __t1231 = YV_NULLV;
  ergo_release_val(__t1231);
  ErgoVal __t1232 = self; ergo_retain_val(__t1232);
  ergo_move_into(&__ret, __t1232);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1233 = self; ergo_retain_val(__t1233);
  ErgoVal __t1234 = a0; ergo_retain_val(__t1234);
  __cogito_node_set_disabled(__t1233, __t1234);
  ergo_release_val(__t1233);
  ergo_release_val(__t1234);
  ErgoVal __t1235 = YV_NULLV;
  ergo_release_val(__t1235);
  ErgoVal __t1236 = self; ergo_retain_val(__t1236);
  ergo_move_into(&__ret, __t1236);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1237 = self; ergo_retain_val(__t1237);
  ErgoVal __t1238 = a0; ergo_retain_val(__t1238);
  __cogito_node_set_class(__t1237, __t1238);
  ergo_release_val(__t1237);
  ergo_release_val(__t1238);
  ErgoVal __t1239 = YV_NULLV;
  ergo_release_val(__t1239);
  ErgoVal __t1240 = self; ergo_retain_val(__t1240);
  ergo_move_into(&__ret, __t1240);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_VStack_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1241 = self; ergo_retain_val(__t1241);
  ErgoVal __t1242 = a0; ergo_retain_val(__t1242);
  __cogito_node_set_id(__t1241, __t1242);
  ergo_release_val(__t1241);
  ergo_release_val(__t1242);
  ErgoVal __t1243 = YV_NULLV;
  ergo_release_val(__t1243);
  ErgoVal __t1244 = self; ergo_retain_val(__t1244);
  ergo_move_into(&__ret, __t1244);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1245 = self; ergo_retain_val(__t1245);
  ErgoVal __t1246 = a0; ergo_retain_val(__t1246);
  __cogito_container_add(__t1245, __t1246);
  ergo_release_val(__t1245);
  ergo_release_val(__t1246);
  ErgoVal __t1247 = YV_NULLV;
  ergo_release_val(__t1247);
  ErgoVal __t1248 = self; ergo_retain_val(__t1248);
  ergo_move_into(&__ret, __t1248);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1249 = self; ergo_retain_val(__t1249);
  ErgoVal __t1250 = a0; ergo_retain_val(__t1250);
  ErgoVal __t1251 = a1; ergo_retain_val(__t1251);
  ErgoVal __t1252 = a2; ergo_retain_val(__t1252);
  ErgoVal __t1253 = a3; ergo_retain_val(__t1253);
  __cogito_container_set_margins(__t1249, __t1250, __t1251, __t1252, __t1253);
  ergo_release_val(__t1249);
  ergo_release_val(__t1250);
  ergo_release_val(__t1251);
  ergo_release_val(__t1252);
  ergo_release_val(__t1253);
  ErgoVal __t1254 = YV_NULLV;
  ergo_release_val(__t1254);
  ErgoVal __t1255 = self; ergo_retain_val(__t1255);
  ergo_move_into(&__ret, __t1255);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1256 = self; ergo_retain_val(__t1256);
  ErgoVal __t1257 = a0; ergo_retain_val(__t1257);
  ErgoVal __t1258 = a1; ergo_retain_val(__t1258);
  ErgoVal __t1259 = a2; ergo_retain_val(__t1259);
  ErgoVal __t1260 = a3; ergo_retain_val(__t1260);
  __cogito_container_set_padding(__t1256, __t1257, __t1258, __t1259, __t1260);
  ergo_release_val(__t1256);
  ergo_release_val(__t1257);
  ergo_release_val(__t1258);
  ergo_release_val(__t1259);
  ergo_release_val(__t1260);
  ErgoVal __t1261 = YV_NULLV;
  ergo_release_val(__t1261);
  ErgoVal __t1262 = self; ergo_retain_val(__t1262);
  ergo_move_into(&__ret, __t1262);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1263 = self; ergo_retain_val(__t1263);
  ErgoVal __t1264 = a0; ergo_retain_val(__t1264);
  __cogito_container_set_align(__t1263, __t1264);
  ergo_release_val(__t1263);
  ergo_release_val(__t1264);
  ErgoVal __t1265 = YV_NULLV;
  ergo_release_val(__t1265);
  ErgoVal __t1266 = self; ergo_retain_val(__t1266);
  ergo_move_into(&__ret, __t1266);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1267 = self; ergo_retain_val(__t1267);
  ErgoVal __t1268 = a0; ergo_retain_val(__t1268);
  __cogito_container_set_halign(__t1267, __t1268);
  ergo_release_val(__t1267);
  ergo_release_val(__t1268);
  ErgoVal __t1269 = YV_NULLV;
  ergo_release_val(__t1269);
  ErgoVal __t1270 = self; ergo_retain_val(__t1270);
  ergo_move_into(&__ret, __t1270);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1271 = self; ergo_retain_val(__t1271);
  ErgoVal __t1272 = a0; ergo_retain_val(__t1272);
  __cogito_container_set_valign(__t1271, __t1272);
  ergo_release_val(__t1271);
  ergo_release_val(__t1272);
  ErgoVal __t1273 = YV_NULLV;
  ergo_release_val(__t1273);
  ErgoVal __t1274 = self; ergo_retain_val(__t1274);
  ergo_move_into(&__ret, __t1274);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1275 = self; ergo_retain_val(__t1275);
  ErgoVal __t1276 = a0; ergo_retain_val(__t1276);
  __cogito_container_set_hexpand(__t1275, __t1276);
  ergo_release_val(__t1275);
  ergo_release_val(__t1276);
  ErgoVal __t1277 = YV_NULLV;
  ergo_release_val(__t1277);
  ErgoVal __t1278 = self; ergo_retain_val(__t1278);
  ergo_move_into(&__ret, __t1278);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1279 = self; ergo_retain_val(__t1279);
  ErgoVal __t1280 = a0; ergo_retain_val(__t1280);
  __cogito_container_set_vexpand(__t1279, __t1280);
  ergo_release_val(__t1279);
  ergo_release_val(__t1280);
  ErgoVal __t1281 = YV_NULLV;
  ergo_release_val(__t1281);
  ErgoVal __t1282 = self; ergo_retain_val(__t1282);
  ergo_move_into(&__ret, __t1282);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_set_gap(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1283 = self; ergo_retain_val(__t1283);
  ErgoVal __t1284 = a0; ergo_retain_val(__t1284);
  __cogito_container_set_gap(__t1283, __t1284);
  ergo_release_val(__t1283);
  ergo_release_val(__t1284);
  ErgoVal __t1285 = YV_NULLV;
  ergo_release_val(__t1285);
  ErgoVal __t1286 = self; ergo_retain_val(__t1286);
  ergo_move_into(&__ret, __t1286);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1287 = self; ergo_retain_val(__t1287);
  ErgoVal __t1288 = YV_INT(0);
  __cogito_container_set_align(__t1287, __t1288);
  ergo_release_val(__t1287);
  ergo_release_val(__t1288);
  ErgoVal __t1289 = YV_NULLV;
  ergo_release_val(__t1289);
  ErgoVal __t1290 = self; ergo_retain_val(__t1290);
  ergo_move_into(&__ret, __t1290);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1291 = self; ergo_retain_val(__t1291);
  ErgoVal __t1292 = YV_INT(1);
  __cogito_container_set_align(__t1291, __t1292);
  ergo_release_val(__t1291);
  ergo_release_val(__t1292);
  ErgoVal __t1293 = YV_NULLV;
  ergo_release_val(__t1293);
  ErgoVal __t1294 = self; ergo_retain_val(__t1294);
  ergo_move_into(&__ret, __t1294);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1295 = self; ergo_retain_val(__t1295);
  ErgoVal __t1296 = YV_INT(2);
  __cogito_container_set_align(__t1295, __t1296);
  ergo_release_val(__t1295);
  ergo_release_val(__t1296);
  ErgoVal __t1297 = YV_NULLV;
  ergo_release_val(__t1297);
  ErgoVal __t1298 = self; ergo_retain_val(__t1298);
  ergo_move_into(&__ret, __t1298);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1299 = self; ergo_retain_val(__t1299);
  ErgoVal __t1300 = a0; ergo_retain_val(__t1300);
  __cogito_build(__t1299, __t1300);
  ergo_release_val(__t1299);
  ergo_release_val(__t1300);
  ErgoVal __t1301 = YV_NULLV;
  ergo_release_val(__t1301);
  ErgoVal __t1302 = self; ergo_retain_val(__t1302);
  ergo_move_into(&__ret, __t1302);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1303 = self; ergo_retain_val(__t1303);
  ErgoVal __t1304 = a0; ergo_retain_val(__t1304);
  __cogito_node_set_disabled(__t1303, __t1304);
  ergo_release_val(__t1303);
  ergo_release_val(__t1304);
  ErgoVal __t1305 = YV_NULLV;
  ergo_release_val(__t1305);
  ErgoVal __t1306 = self; ergo_retain_val(__t1306);
  ergo_move_into(&__ret, __t1306);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1307 = self; ergo_retain_val(__t1307);
  ErgoVal __t1308 = a0; ergo_retain_val(__t1308);
  __cogito_node_set_class(__t1307, __t1308);
  ergo_release_val(__t1307);
  ergo_release_val(__t1308);
  ErgoVal __t1309 = YV_NULLV;
  ergo_release_val(__t1309);
  ErgoVal __t1310 = self; ergo_retain_val(__t1310);
  ergo_move_into(&__ret, __t1310);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_HStack_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1311 = self; ergo_retain_val(__t1311);
  ErgoVal __t1312 = a0; ergo_retain_val(__t1312);
  __cogito_node_set_id(__t1311, __t1312);
  ergo_release_val(__t1311);
  ergo_release_val(__t1312);
  ErgoVal __t1313 = YV_NULLV;
  ergo_release_val(__t1313);
  ErgoVal __t1314 = self; ergo_retain_val(__t1314);
  ergo_move_into(&__ret, __t1314);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1315 = self; ergo_retain_val(__t1315);
  ErgoVal __t1316 = a0; ergo_retain_val(__t1316);
  __cogito_container_add(__t1315, __t1316);
  ergo_release_val(__t1315);
  ergo_release_val(__t1316);
  ErgoVal __t1317 = YV_NULLV;
  ergo_release_val(__t1317);
  ErgoVal __t1318 = self; ergo_retain_val(__t1318);
  ergo_move_into(&__ret, __t1318);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1319 = self; ergo_retain_val(__t1319);
  ErgoVal __t1320 = a0; ergo_retain_val(__t1320);
  ErgoVal __t1321 = a1; ergo_retain_val(__t1321);
  ErgoVal __t1322 = a2; ergo_retain_val(__t1322);
  ErgoVal __t1323 = a3; ergo_retain_val(__t1323);
  __cogito_container_set_margins(__t1319, __t1320, __t1321, __t1322, __t1323);
  ergo_release_val(__t1319);
  ergo_release_val(__t1320);
  ergo_release_val(__t1321);
  ergo_release_val(__t1322);
  ergo_release_val(__t1323);
  ErgoVal __t1324 = YV_NULLV;
  ergo_release_val(__t1324);
  ErgoVal __t1325 = self; ergo_retain_val(__t1325);
  ergo_move_into(&__ret, __t1325);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1326 = self; ergo_retain_val(__t1326);
  ErgoVal __t1327 = a0; ergo_retain_val(__t1327);
  ErgoVal __t1328 = a1; ergo_retain_val(__t1328);
  ErgoVal __t1329 = a2; ergo_retain_val(__t1329);
  ErgoVal __t1330 = a3; ergo_retain_val(__t1330);
  __cogito_container_set_padding(__t1326, __t1327, __t1328, __t1329, __t1330);
  ergo_release_val(__t1326);
  ergo_release_val(__t1327);
  ergo_release_val(__t1328);
  ergo_release_val(__t1329);
  ergo_release_val(__t1330);
  ErgoVal __t1331 = YV_NULLV;
  ergo_release_val(__t1331);
  ErgoVal __t1332 = self; ergo_retain_val(__t1332);
  ergo_move_into(&__ret, __t1332);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1333 = self; ergo_retain_val(__t1333);
  ErgoVal __t1334 = a0; ergo_retain_val(__t1334);
  __cogito_container_set_align(__t1333, __t1334);
  ergo_release_val(__t1333);
  ergo_release_val(__t1334);
  ErgoVal __t1335 = YV_NULLV;
  ergo_release_val(__t1335);
  ErgoVal __t1336 = self; ergo_retain_val(__t1336);
  ergo_move_into(&__ret, __t1336);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1337 = self; ergo_retain_val(__t1337);
  ErgoVal __t1338 = a0; ergo_retain_val(__t1338);
  __cogito_container_set_halign(__t1337, __t1338);
  ergo_release_val(__t1337);
  ergo_release_val(__t1338);
  ErgoVal __t1339 = YV_NULLV;
  ergo_release_val(__t1339);
  ErgoVal __t1340 = self; ergo_retain_val(__t1340);
  ergo_move_into(&__ret, __t1340);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1341 = self; ergo_retain_val(__t1341);
  ErgoVal __t1342 = a0; ergo_retain_val(__t1342);
  __cogito_container_set_valign(__t1341, __t1342);
  ergo_release_val(__t1341);
  ergo_release_val(__t1342);
  ErgoVal __t1343 = YV_NULLV;
  ergo_release_val(__t1343);
  ErgoVal __t1344 = self; ergo_retain_val(__t1344);
  ergo_move_into(&__ret, __t1344);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1345 = self; ergo_retain_val(__t1345);
  ErgoVal __t1346 = YV_INT(0);
  __cogito_container_set_align(__t1345, __t1346);
  ergo_release_val(__t1345);
  ergo_release_val(__t1346);
  ErgoVal __t1347 = YV_NULLV;
  ergo_release_val(__t1347);
  ErgoVal __t1348 = self; ergo_retain_val(__t1348);
  ergo_move_into(&__ret, __t1348);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1349 = self; ergo_retain_val(__t1349);
  ErgoVal __t1350 = YV_INT(1);
  __cogito_container_set_align(__t1349, __t1350);
  ergo_release_val(__t1349);
  ergo_release_val(__t1350);
  ErgoVal __t1351 = YV_NULLV;
  ergo_release_val(__t1351);
  ErgoVal __t1352 = self; ergo_retain_val(__t1352);
  ergo_move_into(&__ret, __t1352);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1353 = self; ergo_retain_val(__t1353);
  ErgoVal __t1354 = YV_INT(2);
  __cogito_container_set_align(__t1353, __t1354);
  ergo_release_val(__t1353);
  ergo_release_val(__t1354);
  ErgoVal __t1355 = YV_NULLV;
  ergo_release_val(__t1355);
  ErgoVal __t1356 = self; ergo_retain_val(__t1356);
  ergo_move_into(&__ret, __t1356);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1357 = self; ergo_retain_val(__t1357);
  ErgoVal __t1358 = a0; ergo_retain_val(__t1358);
  __cogito_build(__t1357, __t1358);
  ergo_release_val(__t1357);
  ergo_release_val(__t1358);
  ErgoVal __t1359 = YV_NULLV;
  ergo_release_val(__t1359);
  ErgoVal __t1360 = self; ergo_retain_val(__t1360);
  ergo_move_into(&__ret, __t1360);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1361 = self; ergo_retain_val(__t1361);
  ErgoVal __t1362 = a0; ergo_retain_val(__t1362);
  __cogito_container_set_hexpand(__t1361, __t1362);
  ergo_release_val(__t1361);
  ergo_release_val(__t1362);
  ErgoVal __t1363 = YV_NULLV;
  ergo_release_val(__t1363);
  ErgoVal __t1364 = self; ergo_retain_val(__t1364);
  ergo_move_into(&__ret, __t1364);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1365 = self; ergo_retain_val(__t1365);
  ErgoVal __t1366 = a0; ergo_retain_val(__t1366);
  __cogito_container_set_vexpand(__t1365, __t1366);
  ergo_release_val(__t1365);
  ergo_release_val(__t1366);
  ErgoVal __t1367 = YV_NULLV;
  ergo_release_val(__t1367);
  ErgoVal __t1368 = self; ergo_retain_val(__t1368);
  ergo_move_into(&__ret, __t1368);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1369 = self; ergo_retain_val(__t1369);
  ErgoVal __t1370 = a0; ergo_retain_val(__t1370);
  __cogito_node_set_disabled(__t1369, __t1370);
  ergo_release_val(__t1369);
  ergo_release_val(__t1370);
  ErgoVal __t1371 = YV_NULLV;
  ergo_release_val(__t1371);
  ErgoVal __t1372 = self; ergo_retain_val(__t1372);
  ergo_move_into(&__ret, __t1372);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1373 = self; ergo_retain_val(__t1373);
  ErgoVal __t1374 = a0; ergo_retain_val(__t1374);
  __cogito_node_set_class(__t1373, __t1374);
  ergo_release_val(__t1373);
  ergo_release_val(__t1374);
  ErgoVal __t1375 = YV_NULLV;
  ergo_release_val(__t1375);
  ErgoVal __t1376 = self; ergo_retain_val(__t1376);
  ergo_move_into(&__ret, __t1376);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ZStack_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1377 = self; ergo_retain_val(__t1377);
  ErgoVal __t1378 = a0; ergo_retain_val(__t1378);
  __cogito_node_set_id(__t1377, __t1378);
  ergo_release_val(__t1377);
  ergo_release_val(__t1378);
  ErgoVal __t1379 = YV_NULLV;
  ergo_release_val(__t1379);
  ErgoVal __t1380 = self; ergo_retain_val(__t1380);
  ergo_move_into(&__ret, __t1380);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Fixed_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1381 = self; ergo_retain_val(__t1381);
  ErgoVal __t1382 = a0; ergo_retain_val(__t1382);
  __cogito_container_add(__t1381, __t1382);
  ergo_release_val(__t1381);
  ergo_release_val(__t1382);
  ErgoVal __t1383 = YV_NULLV;
  ergo_release_val(__t1383);
  ErgoVal __t1384 = self; ergo_retain_val(__t1384);
  ergo_move_into(&__ret, __t1384);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Fixed_set_pos(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1385 = self; ergo_retain_val(__t1385);
  ErgoVal __t1386 = a0; ergo_retain_val(__t1386);
  ErgoVal __t1387 = a1; ergo_retain_val(__t1387);
  ErgoVal __t1388 = a2; ergo_retain_val(__t1388);
  __cogito_fixed_set_pos(__t1385, __t1386, __t1387, __t1388);
  ergo_release_val(__t1385);
  ergo_release_val(__t1386);
  ergo_release_val(__t1387);
  ergo_release_val(__t1388);
  ErgoVal __t1389 = YV_NULLV;
  ergo_release_val(__t1389);
  ErgoVal __t1390 = self; ergo_retain_val(__t1390);
  ergo_move_into(&__ret, __t1390);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Fixed_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1391 = self; ergo_retain_val(__t1391);
  ErgoVal __t1392 = a0; ergo_retain_val(__t1392);
  ErgoVal __t1393 = a1; ergo_retain_val(__t1393);
  ErgoVal __t1394 = a2; ergo_retain_val(__t1394);
  ErgoVal __t1395 = a3; ergo_retain_val(__t1395);
  __cogito_container_set_padding(__t1391, __t1392, __t1393, __t1394, __t1395);
  ergo_release_val(__t1391);
  ergo_release_val(__t1392);
  ergo_release_val(__t1393);
  ergo_release_val(__t1394);
  ergo_release_val(__t1395);
  ErgoVal __t1396 = YV_NULLV;
  ergo_release_val(__t1396);
  ErgoVal __t1397 = self; ergo_retain_val(__t1397);
  ergo_move_into(&__ret, __t1397);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Fixed_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1398 = self; ergo_retain_val(__t1398);
  ErgoVal __t1399 = a0; ergo_retain_val(__t1399);
  __cogito_build(__t1398, __t1399);
  ergo_release_val(__t1398);
  ergo_release_val(__t1399);
  ErgoVal __t1400 = YV_NULLV;
  ergo_release_val(__t1400);
  ErgoVal __t1401 = self; ergo_retain_val(__t1401);
  ergo_move_into(&__ret, __t1401);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Fixed_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1402 = self; ergo_retain_val(__t1402);
  ErgoVal __t1403 = a0; ergo_retain_val(__t1403);
  __cogito_container_set_hexpand(__t1402, __t1403);
  ergo_release_val(__t1402);
  ergo_release_val(__t1403);
  ErgoVal __t1404 = YV_NULLV;
  ergo_release_val(__t1404);
  ErgoVal __t1405 = self; ergo_retain_val(__t1405);
  ergo_move_into(&__ret, __t1405);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Fixed_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1406 = self; ergo_retain_val(__t1406);
  ErgoVal __t1407 = a0; ergo_retain_val(__t1407);
  __cogito_container_set_vexpand(__t1406, __t1407);
  ergo_release_val(__t1406);
  ergo_release_val(__t1407);
  ErgoVal __t1408 = YV_NULLV;
  ergo_release_val(__t1408);
  ErgoVal __t1409 = self; ergo_retain_val(__t1409);
  ergo_move_into(&__ret, __t1409);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Fixed_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1410 = self; ergo_retain_val(__t1410);
  ErgoVal __t1411 = a0; ergo_retain_val(__t1411);
  __cogito_container_set_halign(__t1410, __t1411);
  ergo_release_val(__t1410);
  ergo_release_val(__t1411);
  ErgoVal __t1412 = YV_NULLV;
  ergo_release_val(__t1412);
  ErgoVal __t1413 = self; ergo_retain_val(__t1413);
  ergo_move_into(&__ret, __t1413);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Fixed_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1414 = self; ergo_retain_val(__t1414);
  ErgoVal __t1415 = a0; ergo_retain_val(__t1415);
  __cogito_container_set_valign(__t1414, __t1415);
  ergo_release_val(__t1414);
  ergo_release_val(__t1415);
  ErgoVal __t1416 = YV_NULLV;
  ergo_release_val(__t1416);
  ErgoVal __t1417 = self; ergo_retain_val(__t1417);
  ergo_move_into(&__ret, __t1417);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Fixed_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1418 = self; ergo_retain_val(__t1418);
  ErgoVal __t1419 = a0; ergo_retain_val(__t1419);
  __cogito_node_set_disabled(__t1418, __t1419);
  ergo_release_val(__t1418);
  ergo_release_val(__t1419);
  ErgoVal __t1420 = YV_NULLV;
  ergo_release_val(__t1420);
  ErgoVal __t1421 = self; ergo_retain_val(__t1421);
  ergo_move_into(&__ret, __t1421);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Fixed_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1422 = self; ergo_retain_val(__t1422);
  ErgoVal __t1423 = a0; ergo_retain_val(__t1423);
  __cogito_node_set_class(__t1422, __t1423);
  ergo_release_val(__t1422);
  ergo_release_val(__t1423);
  ErgoVal __t1424 = YV_NULLV;
  ergo_release_val(__t1424);
  ErgoVal __t1425 = self; ergo_retain_val(__t1425);
  ergo_move_into(&__ret, __t1425);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Fixed_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1426 = self; ergo_retain_val(__t1426);
  ErgoVal __t1427 = a0; ergo_retain_val(__t1427);
  __cogito_node_set_id(__t1426, __t1427);
  ergo_release_val(__t1426);
  ergo_release_val(__t1427);
  ErgoVal __t1428 = YV_NULLV;
  ergo_release_val(__t1428);
  ErgoVal __t1429 = self; ergo_retain_val(__t1429);
  ergo_move_into(&__ret, __t1429);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Scroller_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1430 = self; ergo_retain_val(__t1430);
  ErgoVal __t1431 = a0; ergo_retain_val(__t1431);
  __cogito_container_add(__t1430, __t1431);
  ergo_release_val(__t1430);
  ergo_release_val(__t1431);
  ErgoVal __t1432 = YV_NULLV;
  ergo_release_val(__t1432);
  ErgoVal __t1433 = self; ergo_retain_val(__t1433);
  ergo_move_into(&__ret, __t1433);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Scroller_set_axes(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1434 = self; ergo_retain_val(__t1434);
  ErgoVal __t1435 = a0; ergo_retain_val(__t1435);
  ErgoVal __t1436 = a1; ergo_retain_val(__t1436);
  __cogito_scroller_set_axes(__t1434, __t1435, __t1436);
  ergo_release_val(__t1434);
  ergo_release_val(__t1435);
  ergo_release_val(__t1436);
  ErgoVal __t1437 = YV_NULLV;
  ergo_release_val(__t1437);
  ErgoVal __t1438 = self; ergo_retain_val(__t1438);
  ergo_move_into(&__ret, __t1438);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Scroller_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1439 = self; ergo_retain_val(__t1439);
  ErgoVal __t1440 = a0; ergo_retain_val(__t1440);
  ErgoVal __t1441 = a1; ergo_retain_val(__t1441);
  ErgoVal __t1442 = a2; ergo_retain_val(__t1442);
  ErgoVal __t1443 = a3; ergo_retain_val(__t1443);
  __cogito_container_set_padding(__t1439, __t1440, __t1441, __t1442, __t1443);
  ergo_release_val(__t1439);
  ergo_release_val(__t1440);
  ergo_release_val(__t1441);
  ergo_release_val(__t1442);
  ergo_release_val(__t1443);
  ErgoVal __t1444 = YV_NULLV;
  ergo_release_val(__t1444);
  ErgoVal __t1445 = self; ergo_retain_val(__t1445);
  ergo_move_into(&__ret, __t1445);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Scroller_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1446 = self; ergo_retain_val(__t1446);
  ErgoVal __t1447 = a0; ergo_retain_val(__t1447);
  __cogito_build(__t1446, __t1447);
  ergo_release_val(__t1446);
  ergo_release_val(__t1447);
  ErgoVal __t1448 = YV_NULLV;
  ergo_release_val(__t1448);
  ErgoVal __t1449 = self; ergo_retain_val(__t1449);
  ergo_move_into(&__ret, __t1449);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Scroller_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1450 = self; ergo_retain_val(__t1450);
  ErgoVal __t1451 = a0; ergo_retain_val(__t1451);
  __cogito_container_set_hexpand(__t1450, __t1451);
  ergo_release_val(__t1450);
  ergo_release_val(__t1451);
  ErgoVal __t1452 = YV_NULLV;
  ergo_release_val(__t1452);
  ErgoVal __t1453 = self; ergo_retain_val(__t1453);
  ergo_move_into(&__ret, __t1453);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Scroller_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1454 = self; ergo_retain_val(__t1454);
  ErgoVal __t1455 = a0; ergo_retain_val(__t1455);
  __cogito_container_set_vexpand(__t1454, __t1455);
  ergo_release_val(__t1454);
  ergo_release_val(__t1455);
  ErgoVal __t1456 = YV_NULLV;
  ergo_release_val(__t1456);
  ErgoVal __t1457 = self; ergo_retain_val(__t1457);
  ergo_move_into(&__ret, __t1457);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Scroller_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1458 = self; ergo_retain_val(__t1458);
  ErgoVal __t1459 = a0; ergo_retain_val(__t1459);
  __cogito_container_set_halign(__t1458, __t1459);
  ergo_release_val(__t1458);
  ergo_release_val(__t1459);
  ErgoVal __t1460 = YV_NULLV;
  ergo_release_val(__t1460);
  ErgoVal __t1461 = self; ergo_retain_val(__t1461);
  ergo_move_into(&__ret, __t1461);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Scroller_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1462 = self; ergo_retain_val(__t1462);
  ErgoVal __t1463 = a0; ergo_retain_val(__t1463);
  __cogito_container_set_valign(__t1462, __t1463);
  ergo_release_val(__t1462);
  ergo_release_val(__t1463);
  ErgoVal __t1464 = YV_NULLV;
  ergo_release_val(__t1464);
  ErgoVal __t1465 = self; ergo_retain_val(__t1465);
  ergo_move_into(&__ret, __t1465);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Scroller_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1466 = self; ergo_retain_val(__t1466);
  ErgoVal __t1467 = a0; ergo_retain_val(__t1467);
  __cogito_node_set_disabled(__t1466, __t1467);
  ergo_release_val(__t1466);
  ergo_release_val(__t1467);
  ErgoVal __t1468 = YV_NULLV;
  ergo_release_val(__t1468);
  ErgoVal __t1469 = self; ergo_retain_val(__t1469);
  ergo_move_into(&__ret, __t1469);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Scroller_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1470 = self; ergo_retain_val(__t1470);
  ErgoVal __t1471 = a0; ergo_retain_val(__t1471);
  __cogito_node_set_class(__t1470, __t1471);
  ergo_release_val(__t1470);
  ergo_release_val(__t1471);
  ErgoVal __t1472 = YV_NULLV;
  ergo_release_val(__t1472);
  ErgoVal __t1473 = self; ergo_retain_val(__t1473);
  ergo_move_into(&__ret, __t1473);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Scroller_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1474 = self; ergo_retain_val(__t1474);
  ErgoVal __t1475 = a0; ergo_retain_val(__t1475);
  __cogito_node_set_id(__t1474, __t1475);
  ergo_release_val(__t1474);
  ergo_release_val(__t1475);
  ErgoVal __t1476 = YV_NULLV;
  ergo_release_val(__t1476);
  ErgoVal __t1477 = self; ergo_retain_val(__t1477);
  ergo_move_into(&__ret, __t1477);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Carousel_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1478 = self; ergo_retain_val(__t1478);
  ErgoVal __t1479 = a0; ergo_retain_val(__t1479);
  __cogito_container_add(__t1478, __t1479);
  ergo_release_val(__t1478);
  ergo_release_val(__t1479);
  ErgoVal __t1480 = YV_NULLV;
  ergo_release_val(__t1480);
  ErgoVal __t1481 = self; ergo_retain_val(__t1481);
  ergo_move_into(&__ret, __t1481);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Carousel_set_active_index(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1482 = self; ergo_retain_val(__t1482);
  ErgoVal __t1483 = a0; ergo_retain_val(__t1483);
  __cogito_carousel_set_active_index(__t1482, __t1483);
  ergo_release_val(__t1482);
  ergo_release_val(__t1483);
  ErgoVal __t1484 = YV_NULLV;
  ergo_release_val(__t1484);
  ErgoVal __t1485 = self; ergo_retain_val(__t1485);
  ergo_move_into(&__ret, __t1485);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Carousel_active_index(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1486 = self; ergo_retain_val(__t1486);
  ErgoVal __t1487 = __cogito_carousel_get_active_index(__t1486);
  ergo_release_val(__t1486);
  ergo_move_into(&__ret, __t1487);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Carousel_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1488 = self; ergo_retain_val(__t1488);
  ErgoVal __t1489 = a0; ergo_retain_val(__t1489);
  __cogito_container_set_hexpand(__t1488, __t1489);
  ergo_release_val(__t1488);
  ergo_release_val(__t1489);
  ErgoVal __t1490 = YV_NULLV;
  ergo_release_val(__t1490);
  ErgoVal __t1491 = self; ergo_retain_val(__t1491);
  ergo_move_into(&__ret, __t1491);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Carousel_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1492 = self; ergo_retain_val(__t1492);
  ErgoVal __t1493 = a0; ergo_retain_val(__t1493);
  __cogito_container_set_vexpand(__t1492, __t1493);
  ergo_release_val(__t1492);
  ergo_release_val(__t1493);
  ErgoVal __t1494 = YV_NULLV;
  ergo_release_val(__t1494);
  ErgoVal __t1495 = self; ergo_retain_val(__t1495);
  ergo_move_into(&__ret, __t1495);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Carousel_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1496 = self; ergo_retain_val(__t1496);
  ErgoVal __t1497 = a0; ergo_retain_val(__t1497);
  __cogito_node_set_class(__t1496, __t1497);
  ergo_release_val(__t1496);
  ergo_release_val(__t1497);
  ErgoVal __t1498 = YV_NULLV;
  ergo_release_val(__t1498);
  ErgoVal __t1499 = self; ergo_retain_val(__t1499);
  ergo_move_into(&__ret, __t1499);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Carousel_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1500 = self; ergo_retain_val(__t1500);
  ErgoVal __t1501 = a0; ergo_retain_val(__t1501);
  __cogito_node_set_id(__t1500, __t1501);
  ergo_release_val(__t1500);
  ergo_release_val(__t1501);
  ErgoVal __t1502 = YV_NULLV;
  ergo_release_val(__t1502);
  ErgoVal __t1503 = self; ergo_retain_val(__t1503);
  ergo_move_into(&__ret, __t1503);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_CarouselItem_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1504 = self; ergo_retain_val(__t1504);
  ErgoVal __t1505 = a0; ergo_retain_val(__t1505);
  __cogito_container_add(__t1504, __t1505);
  ergo_release_val(__t1504);
  ergo_release_val(__t1505);
  ErgoVal __t1506 = YV_NULLV;
  ergo_release_val(__t1506);
  ErgoVal __t1507 = self; ergo_retain_val(__t1507);
  ergo_move_into(&__ret, __t1507);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_CarouselItem_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1508 = self; ergo_retain_val(__t1508);
  ErgoVal __t1509 = a0; ergo_retain_val(__t1509);
  __cogito_container_set_hexpand(__t1508, __t1509);
  ergo_release_val(__t1508);
  ergo_release_val(__t1509);
  ErgoVal __t1510 = YV_NULLV;
  ergo_release_val(__t1510);
  ErgoVal __t1511 = self; ergo_retain_val(__t1511);
  ergo_move_into(&__ret, __t1511);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_CarouselItem_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1512 = self; ergo_retain_val(__t1512);
  ErgoVal __t1513 = a0; ergo_retain_val(__t1513);
  __cogito_container_set_vexpand(__t1512, __t1513);
  ergo_release_val(__t1512);
  ergo_release_val(__t1513);
  ErgoVal __t1514 = YV_NULLV;
  ergo_release_val(__t1514);
  ErgoVal __t1515 = self; ergo_retain_val(__t1515);
  ergo_move_into(&__ret, __t1515);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_CarouselItem_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1516 = self; ergo_retain_val(__t1516);
  ErgoVal __t1517 = a0; ergo_retain_val(__t1517);
  __cogito_node_set_class(__t1516, __t1517);
  ergo_release_val(__t1516);
  ergo_release_val(__t1517);
  ErgoVal __t1518 = YV_NULLV;
  ergo_release_val(__t1518);
  ErgoVal __t1519 = self; ergo_retain_val(__t1519);
  ergo_move_into(&__ret, __t1519);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_CarouselItem_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1520 = self; ergo_retain_val(__t1520);
  ErgoVal __t1521 = a0; ergo_retain_val(__t1521);
  __cogito_node_set_id(__t1520, __t1521);
  ergo_release_val(__t1520);
  ergo_release_val(__t1521);
  ErgoVal __t1522 = YV_NULLV;
  ergo_release_val(__t1522);
  ErgoVal __t1523 = self; ergo_retain_val(__t1523);
  ergo_move_into(&__ret, __t1523);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_CarouselItem_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1524 = self; ergo_retain_val(__t1524);
  ErgoVal __t1525 = a0; ergo_retain_val(__t1525);
  __cogito_carousel_item_set_text(__t1524, __t1525);
  ergo_release_val(__t1524);
  ergo_release_val(__t1525);
  ErgoVal __t1526 = YV_NULLV;
  ergo_release_val(__t1526);
  ErgoVal __t1527 = self; ergo_retain_val(__t1527);
  ergo_move_into(&__ret, __t1527);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_CarouselItem_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1528 = self; ergo_retain_val(__t1528);
  ErgoVal __t1529 = a0; ergo_retain_val(__t1529);
  __cogito_carousel_item_set_halign(__t1528, __t1529);
  ergo_release_val(__t1528);
  ergo_release_val(__t1529);
  ErgoVal __t1530 = YV_NULLV;
  ergo_release_val(__t1530);
  ErgoVal __t1531 = self; ergo_retain_val(__t1531);
  ergo_move_into(&__ret, __t1531);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_CarouselItem_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1532 = self; ergo_retain_val(__t1532);
  ErgoVal __t1533 = a0; ergo_retain_val(__t1533);
  __cogito_carousel_item_set_valign(__t1532, __t1533);
  ergo_release_val(__t1532);
  ergo_release_val(__t1533);
  ErgoVal __t1534 = YV_NULLV;
  ergo_release_val(__t1534);
  ErgoVal __t1535 = self; ergo_retain_val(__t1535);
  ergo_move_into(&__ret, __t1535);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1536 = self; ergo_retain_val(__t1536);
  ErgoVal __t1537 = a0; ergo_retain_val(__t1537);
  __cogito_container_add(__t1536, __t1537);
  ergo_release_val(__t1536);
  ergo_release_val(__t1537);
  ErgoVal __t1538 = YV_NULLV;
  ergo_release_val(__t1538);
  ErgoVal __t1539 = self; ergo_retain_val(__t1539);
  ergo_move_into(&__ret, __t1539);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1540 = self; ergo_retain_val(__t1540);
  ErgoVal __t1541 = a0; ergo_retain_val(__t1541);
  ErgoVal __t1542 = a1; ergo_retain_val(__t1542);
  ErgoVal __t1543 = a2; ergo_retain_val(__t1543);
  ErgoVal __t1544 = a3; ergo_retain_val(__t1544);
  __cogito_container_set_margins(__t1540, __t1541, __t1542, __t1543, __t1544);
  ergo_release_val(__t1540);
  ergo_release_val(__t1541);
  ergo_release_val(__t1542);
  ergo_release_val(__t1543);
  ergo_release_val(__t1544);
  ErgoVal __t1545 = YV_NULLV;
  ergo_release_val(__t1545);
  ErgoVal __t1546 = self; ergo_retain_val(__t1546);
  ergo_move_into(&__ret, __t1546);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1547 = self; ergo_retain_val(__t1547);
  ErgoVal __t1548 = a0; ergo_retain_val(__t1548);
  ErgoVal __t1549 = a1; ergo_retain_val(__t1549);
  ErgoVal __t1550 = a2; ergo_retain_val(__t1550);
  ErgoVal __t1551 = a3; ergo_retain_val(__t1551);
  __cogito_container_set_padding(__t1547, __t1548, __t1549, __t1550, __t1551);
  ergo_release_val(__t1547);
  ergo_release_val(__t1548);
  ergo_release_val(__t1549);
  ergo_release_val(__t1550);
  ergo_release_val(__t1551);
  ErgoVal __t1552 = YV_NULLV;
  ergo_release_val(__t1552);
  ErgoVal __t1553 = self; ergo_retain_val(__t1553);
  ergo_move_into(&__ret, __t1553);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1554 = self; ergo_retain_val(__t1554);
  ErgoVal __t1555 = a0; ergo_retain_val(__t1555);
  __cogito_container_set_align(__t1554, __t1555);
  ergo_release_val(__t1554);
  ergo_release_val(__t1555);
  ErgoVal __t1556 = YV_NULLV;
  ergo_release_val(__t1556);
  ErgoVal __t1557 = self; ergo_retain_val(__t1557);
  ergo_move_into(&__ret, __t1557);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1558 = self; ergo_retain_val(__t1558);
  ErgoVal __t1559 = a0; ergo_retain_val(__t1559);
  __cogito_container_set_halign(__t1558, __t1559);
  ergo_release_val(__t1558);
  ergo_release_val(__t1559);
  ErgoVal __t1560 = YV_NULLV;
  ergo_release_val(__t1560);
  ErgoVal __t1561 = self; ergo_retain_val(__t1561);
  ergo_move_into(&__ret, __t1561);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1562 = self; ergo_retain_val(__t1562);
  ErgoVal __t1563 = a0; ergo_retain_val(__t1563);
  __cogito_container_set_valign(__t1562, __t1563);
  ergo_release_val(__t1562);
  ergo_release_val(__t1563);
  ErgoVal __t1564 = YV_NULLV;
  ergo_release_val(__t1564);
  ErgoVal __t1565 = self; ergo_retain_val(__t1565);
  ergo_move_into(&__ret, __t1565);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1566 = self; ergo_retain_val(__t1566);
  ErgoVal __t1567 = YV_INT(0);
  __cogito_container_set_align(__t1566, __t1567);
  ergo_release_val(__t1566);
  ergo_release_val(__t1567);
  ErgoVal __t1568 = YV_NULLV;
  ergo_release_val(__t1568);
  ErgoVal __t1569 = self; ergo_retain_val(__t1569);
  ergo_move_into(&__ret, __t1569);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1570 = self; ergo_retain_val(__t1570);
  ErgoVal __t1571 = YV_INT(1);
  __cogito_container_set_align(__t1570, __t1571);
  ergo_release_val(__t1570);
  ergo_release_val(__t1571);
  ErgoVal __t1572 = YV_NULLV;
  ergo_release_val(__t1572);
  ErgoVal __t1573 = self; ergo_retain_val(__t1573);
  ergo_move_into(&__ret, __t1573);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1574 = self; ergo_retain_val(__t1574);
  ErgoVal __t1575 = YV_INT(2);
  __cogito_container_set_align(__t1574, __t1575);
  ergo_release_val(__t1574);
  ergo_release_val(__t1575);
  ErgoVal __t1576 = YV_NULLV;
  ergo_release_val(__t1576);
  ErgoVal __t1577 = self; ergo_retain_val(__t1577);
  ergo_move_into(&__ret, __t1577);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_on_select(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1578 = self; ergo_retain_val(__t1578);
  ErgoVal __t1579 = a0; ergo_retain_val(__t1579);
  __cogito_list_on_select(__t1578, __t1579);
  ergo_release_val(__t1578);
  ergo_release_val(__t1579);
  ErgoVal __t1580 = YV_NULLV;
  ergo_release_val(__t1580);
  ErgoVal __t1581 = self; ergo_retain_val(__t1581);
  ergo_move_into(&__ret, __t1581);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_on_activate(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1582 = self; ergo_retain_val(__t1582);
  ErgoVal __t1583 = a0; ergo_retain_val(__t1583);
  __cogito_list_on_activate(__t1582, __t1583);
  ergo_release_val(__t1582);
  ergo_release_val(__t1583);
  ErgoVal __t1584 = YV_NULLV;
  ergo_release_val(__t1584);
  ErgoVal __t1585 = self; ergo_retain_val(__t1585);
  ergo_move_into(&__ret, __t1585);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1586 = self; ergo_retain_val(__t1586);
  ErgoVal __t1587 = a0; ergo_retain_val(__t1587);
  __cogito_build(__t1586, __t1587);
  ergo_release_val(__t1586);
  ergo_release_val(__t1587);
  ErgoVal __t1588 = YV_NULLV;
  ergo_release_val(__t1588);
  ErgoVal __t1589 = self; ergo_retain_val(__t1589);
  ergo_move_into(&__ret, __t1589);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1590 = self; ergo_retain_val(__t1590);
  ErgoVal __t1591 = a0; ergo_retain_val(__t1591);
  __cogito_container_set_hexpand(__t1590, __t1591);
  ergo_release_val(__t1590);
  ergo_release_val(__t1591);
  ErgoVal __t1592 = YV_NULLV;
  ergo_release_val(__t1592);
  ErgoVal __t1593 = self; ergo_retain_val(__t1593);
  ergo_move_into(&__ret, __t1593);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1594 = self; ergo_retain_val(__t1594);
  ErgoVal __t1595 = a0; ergo_retain_val(__t1595);
  __cogito_container_set_vexpand(__t1594, __t1595);
  ergo_release_val(__t1594);
  ergo_release_val(__t1595);
  ErgoVal __t1596 = YV_NULLV;
  ergo_release_val(__t1596);
  ErgoVal __t1597 = self; ergo_retain_val(__t1597);
  ergo_move_into(&__ret, __t1597);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1598 = self; ergo_retain_val(__t1598);
  ErgoVal __t1599 = a0; ergo_retain_val(__t1599);
  __cogito_node_set_disabled(__t1598, __t1599);
  ergo_release_val(__t1598);
  ergo_release_val(__t1599);
  ErgoVal __t1600 = YV_NULLV;
  ergo_release_val(__t1600);
  ErgoVal __t1601 = self; ergo_retain_val(__t1601);
  ergo_move_into(&__ret, __t1601);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1602 = self; ergo_retain_val(__t1602);
  ErgoVal __t1603 = a0; ergo_retain_val(__t1603);
  __cogito_node_set_class(__t1602, __t1603);
  ergo_release_val(__t1602);
  ergo_release_val(__t1603);
  ErgoVal __t1604 = YV_NULLV;
  ergo_release_val(__t1604);
  ErgoVal __t1605 = self; ergo_retain_val(__t1605);
  ergo_move_into(&__ret, __t1605);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_List_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1606 = self; ergo_retain_val(__t1606);
  ErgoVal __t1607 = a0; ergo_retain_val(__t1607);
  __cogito_node_set_id(__t1606, __t1607);
  ergo_release_val(__t1606);
  ergo_release_val(__t1607);
  ErgoVal __t1608 = YV_NULLV;
  ergo_release_val(__t1608);
  ErgoVal __t1609 = self; ergo_retain_val(__t1609);
  ergo_move_into(&__ret, __t1609);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1610 = self; ergo_retain_val(__t1610);
  ErgoVal __t1611 = a0; ergo_retain_val(__t1611);
  __cogito_container_add(__t1610, __t1611);
  ergo_release_val(__t1610);
  ergo_release_val(__t1611);
  ErgoVal __t1612 = YV_NULLV;
  ergo_release_val(__t1612);
  ErgoVal __t1613 = self; ergo_retain_val(__t1613);
  ergo_move_into(&__ret, __t1613);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1614 = self; ergo_retain_val(__t1614);
  ErgoVal __t1615 = a0; ergo_retain_val(__t1615);
  ErgoVal __t1616 = a1; ergo_retain_val(__t1616);
  ErgoVal __t1617 = a2; ergo_retain_val(__t1617);
  ErgoVal __t1618 = a3; ergo_retain_val(__t1618);
  __cogito_container_set_margins(__t1614, __t1615, __t1616, __t1617, __t1618);
  ergo_release_val(__t1614);
  ergo_release_val(__t1615);
  ergo_release_val(__t1616);
  ergo_release_val(__t1617);
  ergo_release_val(__t1618);
  ErgoVal __t1619 = YV_NULLV;
  ergo_release_val(__t1619);
  ErgoVal __t1620 = self; ergo_retain_val(__t1620);
  ergo_move_into(&__ret, __t1620);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1621 = self; ergo_retain_val(__t1621);
  ErgoVal __t1622 = a0; ergo_retain_val(__t1622);
  ErgoVal __t1623 = a1; ergo_retain_val(__t1623);
  ErgoVal __t1624 = a2; ergo_retain_val(__t1624);
  ErgoVal __t1625 = a3; ergo_retain_val(__t1625);
  __cogito_container_set_padding(__t1621, __t1622, __t1623, __t1624, __t1625);
  ergo_release_val(__t1621);
  ergo_release_val(__t1622);
  ergo_release_val(__t1623);
  ergo_release_val(__t1624);
  ergo_release_val(__t1625);
  ErgoVal __t1626 = YV_NULLV;
  ergo_release_val(__t1626);
  ErgoVal __t1627 = self; ergo_retain_val(__t1627);
  ergo_move_into(&__ret, __t1627);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_set_gap(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1628 = self; ergo_retain_val(__t1628);
  ErgoVal __t1629 = a0; ergo_retain_val(__t1629);
  ErgoVal __t1630 = a1; ergo_retain_val(__t1630);
  __cogito_grid_set_gap(__t1628, __t1629, __t1630);
  ergo_release_val(__t1628);
  ergo_release_val(__t1629);
  ergo_release_val(__t1630);
  ErgoVal __t1631 = YV_NULLV;
  ergo_release_val(__t1631);
  ErgoVal __t1632 = self; ergo_retain_val(__t1632);
  ergo_move_into(&__ret, __t1632);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_set_span(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1633 = a0; ergo_retain_val(__t1633);
  ErgoVal __t1634 = a1; ergo_retain_val(__t1634);
  ErgoVal __t1635 = a2; ergo_retain_val(__t1635);
  __cogito_grid_set_span(__t1633, __t1634, __t1635);
  ergo_release_val(__t1633);
  ergo_release_val(__t1634);
  ergo_release_val(__t1635);
  ErgoVal __t1636 = YV_NULLV;
  ergo_release_val(__t1636);
  ErgoVal __t1637 = self; ergo_retain_val(__t1637);
  ergo_move_into(&__ret, __t1637);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_set_cell_align(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1638 = a0; ergo_retain_val(__t1638);
  ErgoVal __t1639 = a1; ergo_retain_val(__t1639);
  ErgoVal __t1640 = a2; ergo_retain_val(__t1640);
  __cogito_grid_set_align(__t1638, __t1639, __t1640);
  ergo_release_val(__t1638);
  ergo_release_val(__t1639);
  ergo_release_val(__t1640);
  ErgoVal __t1641 = YV_NULLV;
  ergo_release_val(__t1641);
  ErgoVal __t1642 = self; ergo_retain_val(__t1642);
  ergo_move_into(&__ret, __t1642);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1643 = self; ergo_retain_val(__t1643);
  ErgoVal __t1644 = a0; ergo_retain_val(__t1644);
  __cogito_container_set_align(__t1643, __t1644);
  ergo_release_val(__t1643);
  ergo_release_val(__t1644);
  ErgoVal __t1645 = YV_NULLV;
  ergo_release_val(__t1645);
  ErgoVal __t1646 = self; ergo_retain_val(__t1646);
  ergo_move_into(&__ret, __t1646);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1647 = self; ergo_retain_val(__t1647);
  ErgoVal __t1648 = a0; ergo_retain_val(__t1648);
  __cogito_container_set_halign(__t1647, __t1648);
  ergo_release_val(__t1647);
  ergo_release_val(__t1648);
  ErgoVal __t1649 = YV_NULLV;
  ergo_release_val(__t1649);
  ErgoVal __t1650 = self; ergo_retain_val(__t1650);
  ergo_move_into(&__ret, __t1650);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1651 = self; ergo_retain_val(__t1651);
  ErgoVal __t1652 = a0; ergo_retain_val(__t1652);
  __cogito_container_set_valign(__t1651, __t1652);
  ergo_release_val(__t1651);
  ergo_release_val(__t1652);
  ErgoVal __t1653 = YV_NULLV;
  ergo_release_val(__t1653);
  ErgoVal __t1654 = self; ergo_retain_val(__t1654);
  ergo_move_into(&__ret, __t1654);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1655 = self; ergo_retain_val(__t1655);
  ErgoVal __t1656 = YV_INT(0);
  __cogito_container_set_align(__t1655, __t1656);
  ergo_release_val(__t1655);
  ergo_release_val(__t1656);
  ErgoVal __t1657 = YV_NULLV;
  ergo_release_val(__t1657);
  ErgoVal __t1658 = self; ergo_retain_val(__t1658);
  ergo_move_into(&__ret, __t1658);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1659 = self; ergo_retain_val(__t1659);
  ErgoVal __t1660 = YV_INT(1);
  __cogito_container_set_align(__t1659, __t1660);
  ergo_release_val(__t1659);
  ergo_release_val(__t1660);
  ErgoVal __t1661 = YV_NULLV;
  ergo_release_val(__t1661);
  ErgoVal __t1662 = self; ergo_retain_val(__t1662);
  ergo_move_into(&__ret, __t1662);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1663 = self; ergo_retain_val(__t1663);
  ErgoVal __t1664 = YV_INT(2);
  __cogito_container_set_align(__t1663, __t1664);
  ergo_release_val(__t1663);
  ergo_release_val(__t1664);
  ErgoVal __t1665 = YV_NULLV;
  ergo_release_val(__t1665);
  ErgoVal __t1666 = self; ergo_retain_val(__t1666);
  ergo_move_into(&__ret, __t1666);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_on_select(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1667 = self; ergo_retain_val(__t1667);
  ErgoVal __t1668 = a0; ergo_retain_val(__t1668);
  __cogito_grid_on_select(__t1667, __t1668);
  ergo_release_val(__t1667);
  ergo_release_val(__t1668);
  ErgoVal __t1669 = YV_NULLV;
  ergo_release_val(__t1669);
  ErgoVal __t1670 = self; ergo_retain_val(__t1670);
  ergo_move_into(&__ret, __t1670);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_on_activate(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1671 = self; ergo_retain_val(__t1671);
  ErgoVal __t1672 = a0; ergo_retain_val(__t1672);
  __cogito_grid_on_activate(__t1671, __t1672);
  ergo_release_val(__t1671);
  ergo_release_val(__t1672);
  ErgoVal __t1673 = YV_NULLV;
  ergo_release_val(__t1673);
  ErgoVal __t1674 = self; ergo_retain_val(__t1674);
  ergo_move_into(&__ret, __t1674);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1675 = self; ergo_retain_val(__t1675);
  ErgoVal __t1676 = a0; ergo_retain_val(__t1676);
  __cogito_build(__t1675, __t1676);
  ergo_release_val(__t1675);
  ergo_release_val(__t1676);
  ErgoVal __t1677 = YV_NULLV;
  ergo_release_val(__t1677);
  ErgoVal __t1678 = self; ergo_retain_val(__t1678);
  ergo_move_into(&__ret, __t1678);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1679 = self; ergo_retain_val(__t1679);
  ErgoVal __t1680 = a0; ergo_retain_val(__t1680);
  __cogito_container_set_hexpand(__t1679, __t1680);
  ergo_release_val(__t1679);
  ergo_release_val(__t1680);
  ErgoVal __t1681 = YV_NULLV;
  ergo_release_val(__t1681);
  ErgoVal __t1682 = self; ergo_retain_val(__t1682);
  ergo_move_into(&__ret, __t1682);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1683 = self; ergo_retain_val(__t1683);
  ErgoVal __t1684 = a0; ergo_retain_val(__t1684);
  __cogito_container_set_vexpand(__t1683, __t1684);
  ergo_release_val(__t1683);
  ergo_release_val(__t1684);
  ErgoVal __t1685 = YV_NULLV;
  ergo_release_val(__t1685);
  ErgoVal __t1686 = self; ergo_retain_val(__t1686);
  ergo_move_into(&__ret, __t1686);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1687 = self; ergo_retain_val(__t1687);
  ErgoVal __t1688 = a0; ergo_retain_val(__t1688);
  __cogito_node_set_disabled(__t1687, __t1688);
  ergo_release_val(__t1687);
  ergo_release_val(__t1688);
  ErgoVal __t1689 = YV_NULLV;
  ergo_release_val(__t1689);
  ErgoVal __t1690 = self; ergo_retain_val(__t1690);
  ergo_move_into(&__ret, __t1690);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1691 = self; ergo_retain_val(__t1691);
  ErgoVal __t1692 = a0; ergo_retain_val(__t1692);
  __cogito_node_set_class(__t1691, __t1692);
  ergo_release_val(__t1691);
  ergo_release_val(__t1692);
  ErgoVal __t1693 = YV_NULLV;
  ergo_release_val(__t1693);
  ErgoVal __t1694 = self; ergo_retain_val(__t1694);
  ergo_move_into(&__ret, __t1694);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Grid_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1695 = self; ergo_retain_val(__t1695);
  ErgoVal __t1696 = a0; ergo_retain_val(__t1696);
  __cogito_node_set_id(__t1695, __t1696);
  ergo_release_val(__t1695);
  ergo_release_val(__t1696);
  ErgoVal __t1697 = YV_NULLV;
  ergo_release_val(__t1697);
  ErgoVal __t1698 = self; ergo_retain_val(__t1698);
  ergo_move_into(&__ret, __t1698);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1699 = self; ergo_retain_val(__t1699);
  ErgoVal __t1700 = a0; ergo_retain_val(__t1700);
  ErgoVal __t1701 = a1; ergo_retain_val(__t1701);
  ErgoVal __t1702 = a2; ergo_retain_val(__t1702);
  ErgoVal __t1703 = a3; ergo_retain_val(__t1703);
  __cogito_container_set_margins(__t1699, __t1700, __t1701, __t1702, __t1703);
  ergo_release_val(__t1699);
  ergo_release_val(__t1700);
  ergo_release_val(__t1701);
  ergo_release_val(__t1702);
  ergo_release_val(__t1703);
  ErgoVal __t1704 = YV_NULLV;
  ergo_release_val(__t1704);
  ErgoVal __t1705 = self; ergo_retain_val(__t1705);
  ergo_move_into(&__ret, __t1705);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1706 = self; ergo_retain_val(__t1706);
  ErgoVal __t1707 = a0; ergo_retain_val(__t1707);
  ErgoVal __t1708 = a1; ergo_retain_val(__t1708);
  ErgoVal __t1709 = a2; ergo_retain_val(__t1709);
  ErgoVal __t1710 = a3; ergo_retain_val(__t1710);
  __cogito_container_set_padding(__t1706, __t1707, __t1708, __t1709, __t1710);
  ergo_release_val(__t1706);
  ergo_release_val(__t1707);
  ergo_release_val(__t1708);
  ergo_release_val(__t1709);
  ergo_release_val(__t1710);
  ErgoVal __t1711 = YV_NULLV;
  ergo_release_val(__t1711);
  ErgoVal __t1712 = self; ergo_retain_val(__t1712);
  ergo_move_into(&__ret, __t1712);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1713 = self; ergo_retain_val(__t1713);
  ErgoVal __t1714 = a0; ergo_retain_val(__t1714);
  __cogito_container_set_align(__t1713, __t1714);
  ergo_release_val(__t1713);
  ergo_release_val(__t1714);
  ErgoVal __t1715 = YV_NULLV;
  ergo_release_val(__t1715);
  ErgoVal __t1716 = self; ergo_retain_val(__t1716);
  ergo_move_into(&__ret, __t1716);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1717 = self; ergo_retain_val(__t1717);
  ErgoVal __t1718 = a0; ergo_retain_val(__t1718);
  __cogito_container_set_halign(__t1717, __t1718);
  ergo_release_val(__t1717);
  ergo_release_val(__t1718);
  ErgoVal __t1719 = YV_NULLV;
  ergo_release_val(__t1719);
  ErgoVal __t1720 = self; ergo_retain_val(__t1720);
  ergo_move_into(&__ret, __t1720);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1721 = self; ergo_retain_val(__t1721);
  ErgoVal __t1722 = a0; ergo_retain_val(__t1722);
  __cogito_container_set_valign(__t1721, __t1722);
  ergo_release_val(__t1721);
  ergo_release_val(__t1722);
  ErgoVal __t1723 = YV_NULLV;
  ergo_release_val(__t1723);
  ErgoVal __t1724 = self; ergo_retain_val(__t1724);
  ergo_move_into(&__ret, __t1724);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1725 = self; ergo_retain_val(__t1725);
  ErgoVal __t1726 = a0; ergo_retain_val(__t1726);
  __cogito_container_set_halign(__t1725, __t1726);
  ergo_release_val(__t1725);
  ergo_release_val(__t1726);
  ErgoVal __t1727 = YV_NULLV;
  ergo_release_val(__t1727);
  ErgoVal __t1728 = self; ergo_retain_val(__t1728);
  ergo_move_into(&__ret, __t1728);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1729 = self; ergo_retain_val(__t1729);
  ErgoVal __t1730 = a0; ergo_retain_val(__t1730);
  __cogito_container_set_valign(__t1729, __t1730);
  ergo_release_val(__t1729);
  ergo_release_val(__t1730);
  ErgoVal __t1731 = YV_NULLV;
  ergo_release_val(__t1731);
  ErgoVal __t1732 = self; ergo_retain_val(__t1732);
  ergo_move_into(&__ret, __t1732);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1733 = self; ergo_retain_val(__t1733);
  ErgoVal __t1734 = YV_INT(0);
  __cogito_container_set_align(__t1733, __t1734);
  ergo_release_val(__t1733);
  ergo_release_val(__t1734);
  ErgoVal __t1735 = YV_NULLV;
  ergo_release_val(__t1735);
  ErgoVal __t1736 = self; ergo_retain_val(__t1736);
  ergo_move_into(&__ret, __t1736);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1737 = self; ergo_retain_val(__t1737);
  ErgoVal __t1738 = YV_INT(1);
  __cogito_container_set_align(__t1737, __t1738);
  ergo_release_val(__t1737);
  ergo_release_val(__t1738);
  ErgoVal __t1739 = YV_NULLV;
  ergo_release_val(__t1739);
  ErgoVal __t1740 = self; ergo_retain_val(__t1740);
  ergo_move_into(&__ret, __t1740);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1741 = self; ergo_retain_val(__t1741);
  ErgoVal __t1742 = YV_INT(2);
  __cogito_container_set_align(__t1741, __t1742);
  ergo_release_val(__t1741);
  ergo_release_val(__t1742);
  ErgoVal __t1743 = YV_NULLV;
  ergo_release_val(__t1743);
  ErgoVal __t1744 = self; ergo_retain_val(__t1744);
  ergo_move_into(&__ret, __t1744);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1745 = self; ergo_retain_val(__t1745);
  ErgoVal __t1746 = a0; ergo_retain_val(__t1746);
  __cogito_label_set_class(__t1745, __t1746);
  ergo_release_val(__t1745);
  ergo_release_val(__t1746);
  ErgoVal __t1747 = YV_NULLV;
  ergo_release_val(__t1747);
  ErgoVal __t1748 = self; ergo_retain_val(__t1748);
  ergo_move_into(&__ret, __t1748);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1749 = self; ergo_retain_val(__t1749);
  ErgoVal __t1750 = a0; ergo_retain_val(__t1750);
  __cogito_label_set_text(__t1749, __t1750);
  ergo_release_val(__t1749);
  ergo_release_val(__t1750);
  ErgoVal __t1751 = YV_NULLV;
  ergo_release_val(__t1751);
  ErgoVal __t1752 = self; ergo_retain_val(__t1752);
  ergo_move_into(&__ret, __t1752);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_wrap(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1753 = self; ergo_retain_val(__t1753);
  ErgoVal __t1754 = a0; ergo_retain_val(__t1754);
  __cogito_label_set_wrap(__t1753, __t1754);
  ergo_release_val(__t1753);
  ergo_release_val(__t1754);
  ErgoVal __t1755 = YV_NULLV;
  ergo_release_val(__t1755);
  ErgoVal __t1756 = self; ergo_retain_val(__t1756);
  ergo_move_into(&__ret, __t1756);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_ellipsis(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1757 = self; ergo_retain_val(__t1757);
  ErgoVal __t1758 = a0; ergo_retain_val(__t1758);
  __cogito_label_set_ellipsis(__t1757, __t1758);
  ergo_release_val(__t1757);
  ergo_release_val(__t1758);
  ErgoVal __t1759 = YV_NULLV;
  ergo_release_val(__t1759);
  ErgoVal __t1760 = self; ergo_retain_val(__t1760);
  ergo_move_into(&__ret, __t1760);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_text_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1761 = self; ergo_retain_val(__t1761);
  ErgoVal __t1762 = a0; ergo_retain_val(__t1762);
  __cogito_label_set_align(__t1761, __t1762);
  ergo_release_val(__t1761);
  ergo_release_val(__t1762);
  ErgoVal __t1763 = YV_NULLV;
  ergo_release_val(__t1763);
  ErgoVal __t1764 = self; ergo_retain_val(__t1764);
  ergo_move_into(&__ret, __t1764);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1765 = self; ergo_retain_val(__t1765);
  ErgoVal __t1766 = a0; ergo_retain_val(__t1766);
  __cogito_container_set_hexpand(__t1765, __t1766);
  ergo_release_val(__t1765);
  ergo_release_val(__t1766);
  ErgoVal __t1767 = YV_NULLV;
  ergo_release_val(__t1767);
  ErgoVal __t1768 = self; ergo_retain_val(__t1768);
  ergo_move_into(&__ret, __t1768);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1769 = self; ergo_retain_val(__t1769);
  ErgoVal __t1770 = a0; ergo_retain_val(__t1770);
  __cogito_container_set_vexpand(__t1769, __t1770);
  ergo_release_val(__t1769);
  ergo_release_val(__t1770);
  ErgoVal __t1771 = YV_NULLV;
  ergo_release_val(__t1771);
  ErgoVal __t1772 = self; ergo_retain_val(__t1772);
  ergo_move_into(&__ret, __t1772);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1773 = self; ergo_retain_val(__t1773);
  ErgoVal __t1774 = a0; ergo_retain_val(__t1774);
  __cogito_node_set_disabled(__t1773, __t1774);
  ergo_release_val(__t1773);
  ergo_release_val(__t1774);
  ErgoVal __t1775 = YV_NULLV;
  ergo_release_val(__t1775);
  ErgoVal __t1776 = self; ergo_retain_val(__t1776);
  ergo_move_into(&__ret, __t1776);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Label_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1777 = self; ergo_retain_val(__t1777);
  ErgoVal __t1778 = a0; ergo_retain_val(__t1778);
  __cogito_node_set_id(__t1777, __t1778);
  ergo_release_val(__t1777);
  ergo_release_val(__t1778);
  ErgoVal __t1779 = YV_NULLV;
  ergo_release_val(__t1779);
  ErgoVal __t1780 = self; ergo_retain_val(__t1780);
  ergo_move_into(&__ret, __t1780);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1781 = self; ergo_retain_val(__t1781);
  ErgoVal __t1782 = a0; ergo_retain_val(__t1782);
  ErgoVal __t1783 = a1; ergo_retain_val(__t1783);
  ErgoVal __t1784 = a2; ergo_retain_val(__t1784);
  ErgoVal __t1785 = a3; ergo_retain_val(__t1785);
  __cogito_container_set_margins(__t1781, __t1782, __t1783, __t1784, __t1785);
  ergo_release_val(__t1781);
  ergo_release_val(__t1782);
  ergo_release_val(__t1783);
  ergo_release_val(__t1784);
  ergo_release_val(__t1785);
  ErgoVal __t1786 = YV_NULLV;
  ergo_release_val(__t1786);
  ErgoVal __t1787 = self; ergo_retain_val(__t1787);
  ergo_move_into(&__ret, __t1787);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1788 = self; ergo_retain_val(__t1788);
  ErgoVal __t1789 = a0; ergo_retain_val(__t1789);
  ErgoVal __t1790 = a1; ergo_retain_val(__t1790);
  ErgoVal __t1791 = a2; ergo_retain_val(__t1791);
  ErgoVal __t1792 = a3; ergo_retain_val(__t1792);
  __cogito_container_set_padding(__t1788, __t1789, __t1790, __t1791, __t1792);
  ergo_release_val(__t1788);
  ergo_release_val(__t1789);
  ergo_release_val(__t1790);
  ergo_release_val(__t1791);
  ergo_release_val(__t1792);
  ErgoVal __t1793 = YV_NULLV;
  ergo_release_val(__t1793);
  ErgoVal __t1794 = self; ergo_retain_val(__t1794);
  ergo_move_into(&__ret, __t1794);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1795 = self; ergo_retain_val(__t1795);
  ErgoVal __t1796 = a0; ergo_retain_val(__t1796);
  __cogito_container_set_align(__t1795, __t1796);
  ergo_release_val(__t1795);
  ergo_release_val(__t1796);
  ErgoVal __t1797 = YV_NULLV;
  ergo_release_val(__t1797);
  ErgoVal __t1798 = self; ergo_retain_val(__t1798);
  ergo_move_into(&__ret, __t1798);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1799 = self; ergo_retain_val(__t1799);
  ErgoVal __t1800 = a0; ergo_retain_val(__t1800);
  __cogito_container_set_halign(__t1799, __t1800);
  ergo_release_val(__t1799);
  ergo_release_val(__t1800);
  ErgoVal __t1801 = YV_NULLV;
  ergo_release_val(__t1801);
  ErgoVal __t1802 = self; ergo_retain_val(__t1802);
  ergo_move_into(&__ret, __t1802);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1803 = self; ergo_retain_val(__t1803);
  ErgoVal __t1804 = a0; ergo_retain_val(__t1804);
  __cogito_container_set_valign(__t1803, __t1804);
  ergo_release_val(__t1803);
  ergo_release_val(__t1804);
  ErgoVal __t1805 = YV_NULLV;
  ergo_release_val(__t1805);
  ErgoVal __t1806 = self; ergo_retain_val(__t1806);
  ergo_move_into(&__ret, __t1806);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1807 = self; ergo_retain_val(__t1807);
  ErgoVal __t1808 = a0; ergo_retain_val(__t1808);
  __cogito_container_set_halign(__t1807, __t1808);
  ergo_release_val(__t1807);
  ergo_release_val(__t1808);
  ErgoVal __t1809 = YV_NULLV;
  ergo_release_val(__t1809);
  ErgoVal __t1810 = self; ergo_retain_val(__t1810);
  ergo_move_into(&__ret, __t1810);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1811 = self; ergo_retain_val(__t1811);
  ErgoVal __t1812 = a0; ergo_retain_val(__t1812);
  __cogito_container_set_valign(__t1811, __t1812);
  ergo_release_val(__t1811);
  ergo_release_val(__t1812);
  ErgoVal __t1813 = YV_NULLV;
  ergo_release_val(__t1813);
  ErgoVal __t1814 = self; ergo_retain_val(__t1814);
  ergo_move_into(&__ret, __t1814);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1815 = self; ergo_retain_val(__t1815);
  ErgoVal __t1816 = a0; ergo_retain_val(__t1816);
  __cogito_container_set_hexpand(__t1815, __t1816);
  ergo_release_val(__t1815);
  ergo_release_val(__t1816);
  ErgoVal __t1817 = YV_NULLV;
  ergo_release_val(__t1817);
  ErgoVal __t1818 = self; ergo_retain_val(__t1818);
  ergo_move_into(&__ret, __t1818);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1819 = self; ergo_retain_val(__t1819);
  ErgoVal __t1820 = a0; ergo_retain_val(__t1820);
  __cogito_container_set_vexpand(__t1819, __t1820);
  ergo_release_val(__t1819);
  ergo_release_val(__t1820);
  ErgoVal __t1821 = YV_NULLV;
  ergo_release_val(__t1821);
  ErgoVal __t1822 = self; ergo_retain_val(__t1822);
  ergo_move_into(&__ret, __t1822);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1823 = self; ergo_retain_val(__t1823);
  ErgoVal __t1824 = YV_INT(0);
  __cogito_container_set_align(__t1823, __t1824);
  ergo_release_val(__t1823);
  ergo_release_val(__t1824);
  ErgoVal __t1825 = YV_NULLV;
  ergo_release_val(__t1825);
  ErgoVal __t1826 = self; ergo_retain_val(__t1826);
  ergo_move_into(&__ret, __t1826);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1827 = self; ergo_retain_val(__t1827);
  ErgoVal __t1828 = YV_INT(1);
  __cogito_container_set_align(__t1827, __t1828);
  ergo_release_val(__t1827);
  ergo_release_val(__t1828);
  ErgoVal __t1829 = YV_NULLV;
  ergo_release_val(__t1829);
  ErgoVal __t1830 = self; ergo_retain_val(__t1830);
  ergo_move_into(&__ret, __t1830);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1831 = self; ergo_retain_val(__t1831);
  ErgoVal __t1832 = YV_INT(2);
  __cogito_container_set_align(__t1831, __t1832);
  ergo_release_val(__t1831);
  ergo_release_val(__t1832);
  ErgoVal __t1833 = YV_NULLV;
  ergo_release_val(__t1833);
  ErgoVal __t1834 = self; ergo_retain_val(__t1834);
  ergo_move_into(&__ret, __t1834);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1835 = self; ergo_retain_val(__t1835);
  ErgoVal __t1836 = a0; ergo_retain_val(__t1836);
  __cogito_button_set_text(__t1835, __t1836);
  ergo_release_val(__t1835);
  ergo_release_val(__t1836);
  ErgoVal __t1837 = YV_NULLV;
  ergo_release_val(__t1837);
  ErgoVal __t1838 = self; ergo_retain_val(__t1838);
  ergo_move_into(&__ret, __t1838);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_size(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1839 = self; ergo_retain_val(__t1839);
  ErgoVal __t1840 = a0; ergo_retain_val(__t1840);
  __cogito_button_set_size(__t1839, __t1840);
  ergo_release_val(__t1839);
  ergo_release_val(__t1840);
  ErgoVal __t1841 = YV_NULLV;
  ergo_release_val(__t1841);
  ErgoVal __t1842 = self; ergo_retain_val(__t1842);
  ergo_move_into(&__ret, __t1842);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_size(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1843 = self; ergo_retain_val(__t1843);
  ErgoVal __t1844 = __cogito_button_get_size(__t1843);
  ergo_release_val(__t1843);
  ergo_move_into(&__ret, __t1844);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_xs(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1845 = self; ergo_retain_val(__t1845);
  ErgoVal __t1846 = YV_INT(0);
  __cogito_button_set_size(__t1845, __t1846);
  ergo_release_val(__t1845);
  ergo_release_val(__t1846);
  ErgoVal __t1847 = YV_NULLV;
  ergo_release_val(__t1847);
  ErgoVal __t1848 = self; ergo_retain_val(__t1848);
  ergo_move_into(&__ret, __t1848);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_s(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1849 = self; ergo_retain_val(__t1849);
  ErgoVal __t1850 = YV_INT(1);
  __cogito_button_set_size(__t1849, __t1850);
  ergo_release_val(__t1849);
  ergo_release_val(__t1850);
  ErgoVal __t1851 = YV_NULLV;
  ergo_release_val(__t1851);
  ErgoVal __t1852 = self; ergo_retain_val(__t1852);
  ergo_move_into(&__ret, __t1852);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_m(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1853 = self; ergo_retain_val(__t1853);
  ErgoVal __t1854 = YV_INT(2);
  __cogito_button_set_size(__t1853, __t1854);
  ergo_release_val(__t1853);
  ergo_release_val(__t1854);
  ErgoVal __t1855 = YV_NULLV;
  ergo_release_val(__t1855);
  ErgoVal __t1856 = self; ergo_retain_val(__t1856);
  ergo_move_into(&__ret, __t1856);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_l(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1857 = self; ergo_retain_val(__t1857);
  ErgoVal __t1858 = YV_INT(3);
  __cogito_button_set_size(__t1857, __t1858);
  ergo_release_val(__t1857);
  ergo_release_val(__t1858);
  ErgoVal __t1859 = YV_NULLV;
  ergo_release_val(__t1859);
  ErgoVal __t1860 = self; ergo_retain_val(__t1860);
  ergo_move_into(&__ret, __t1860);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_xl(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1861 = self; ergo_retain_val(__t1861);
  ErgoVal __t1862 = YV_INT(4);
  __cogito_button_set_size(__t1861, __t1862);
  ergo_release_val(__t1861);
  ergo_release_val(__t1862);
  ErgoVal __t1863 = YV_NULLV;
  ergo_release_val(__t1863);
  ErgoVal __t1864 = self; ergo_retain_val(__t1864);
  ergo_move_into(&__ret, __t1864);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_on_click(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1865 = self; ergo_retain_val(__t1865);
  ErgoVal __t1866 = a0; ergo_retain_val(__t1866);
  __cogito_button_on_click(__t1865, __t1866);
  ergo_release_val(__t1865);
  ergo_release_val(__t1866);
  ErgoVal __t1867 = YV_NULLV;
  ergo_release_val(__t1867);
  ErgoVal __t1868 = self; ergo_retain_val(__t1868);
  ergo_move_into(&__ret, __t1868);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_add_menu(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1869 = self; ergo_retain_val(__t1869);
  ErgoVal __t1870 = a0; ergo_retain_val(__t1870);
  ErgoVal __t1871 = a1; ergo_retain_val(__t1871);
  __cogito_button_add_menu(__t1869, __t1870, __t1871);
  ergo_release_val(__t1869);
  ergo_release_val(__t1870);
  ergo_release_val(__t1871);
  ErgoVal __t1872 = YV_NULLV;
  ergo_release_val(__t1872);
  ErgoVal __t1873 = self; ergo_retain_val(__t1873);
  ergo_move_into(&__ret, __t1873);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_add_menu_section(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1874 = self; ergo_retain_val(__t1874);
  ErgoVal __t1875 = a0; ergo_retain_val(__t1875);
  ErgoVal __t1876 = a1; ergo_retain_val(__t1876);
  __cogito_button_add_menu_section(__t1874, __t1875, __t1876);
  ergo_release_val(__t1874);
  ergo_release_val(__t1875);
  ergo_release_val(__t1876);
  ErgoVal __t1877 = YV_NULLV;
  ergo_release_val(__t1877);
  ErgoVal __t1878 = self; ergo_retain_val(__t1878);
  ergo_move_into(&__ret, __t1878);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_menu_set_icon(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1879 = self; ergo_retain_val(__t1879);
  ErgoVal __t1880 = a0; ergo_retain_val(__t1880);
  __cogito_menu_set_icon(__t1879, __t1880);
  ergo_release_val(__t1879);
  ergo_release_val(__t1880);
  ErgoVal __t1881 = YV_NULLV;
  ergo_release_val(__t1881);
  ErgoVal __t1882 = self; ergo_retain_val(__t1882);
  ergo_move_into(&__ret, __t1882);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_menu_set_shortcut(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1883 = self; ergo_retain_val(__t1883);
  ErgoVal __t1884 = a0; ergo_retain_val(__t1884);
  __cogito_menu_set_shortcut(__t1883, __t1884);
  ergo_release_val(__t1883);
  ergo_release_val(__t1884);
  ErgoVal __t1885 = YV_NULLV;
  ergo_release_val(__t1885);
  ErgoVal __t1886 = self; ergo_retain_val(__t1886);
  ergo_move_into(&__ret, __t1886);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_menu_set_submenu(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1887 = self; ergo_retain_val(__t1887);
  ErgoVal __t1888 = a0; ergo_retain_val(__t1888);
  __cogito_menu_set_submenu(__t1887, __t1888);
  ergo_release_val(__t1887);
  ergo_release_val(__t1888);
  ErgoVal __t1889 = YV_NULLV;
  ergo_release_val(__t1889);
  ErgoVal __t1890 = self; ergo_retain_val(__t1890);
  ergo_move_into(&__ret, __t1890);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_menu_set_toggled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1891 = self; ergo_retain_val(__t1891);
  ErgoVal __t1892 = a0; ergo_retain_val(__t1892);
  __cogito_menu_set_toggled(__t1891, __t1892);
  ergo_release_val(__t1891);
  ergo_release_val(__t1892);
  ErgoVal __t1893 = YV_NULLV;
  ergo_release_val(__t1893);
  ErgoVal __t1894 = self; ergo_retain_val(__t1894);
  ergo_move_into(&__ret, __t1894);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_menu_divider(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1895 = self; ergo_retain_val(__t1895);
  ErgoVal __t1896 = a0; ergo_retain_val(__t1896);
  __cogito_button_set_menu_divider(__t1895, __t1896);
  ergo_release_val(__t1895);
  ergo_release_val(__t1896);
  ErgoVal __t1897 = YV_NULLV;
  ergo_release_val(__t1897);
  ErgoVal __t1898 = self; ergo_retain_val(__t1898);
  ergo_move_into(&__ret, __t1898);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_menu_divider(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1899 = self; ergo_retain_val(__t1899);
  ErgoVal __t1900 = __cogito_button_get_menu_divider(__t1899);
  ergo_release_val(__t1899);
  ergo_move_into(&__ret, __t1900);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_menu_item_gap(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1901 = self; ergo_retain_val(__t1901);
  ErgoVal __t1902 = a0; ergo_retain_val(__t1902);
  __cogito_button_set_menu_item_gap(__t1901, __t1902);
  ergo_release_val(__t1901);
  ergo_release_val(__t1902);
  ErgoVal __t1903 = YV_NULLV;
  ergo_release_val(__t1903);
  ErgoVal __t1904 = self; ergo_retain_val(__t1904);
  ergo_move_into(&__ret, __t1904);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_menu_item_gap(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1905 = self; ergo_retain_val(__t1905);
  ErgoVal __t1906 = __cogito_button_get_menu_item_gap(__t1905);
  ergo_release_val(__t1905);
  ergo_move_into(&__ret, __t1906);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_menu_vibrant(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1907 = self; ergo_retain_val(__t1907);
  ErgoVal __t1908 = a0; ergo_retain_val(__t1908);
  __cogito_button_set_menu_vibrant(__t1907, __t1908);
  ergo_release_val(__t1907);
  ergo_release_val(__t1908);
  ErgoVal __t1909 = YV_NULLV;
  ergo_release_val(__t1909);
  ErgoVal __t1910 = self; ergo_retain_val(__t1910);
  ergo_move_into(&__ret, __t1910);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_menu_vibrant(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1911 = self; ergo_retain_val(__t1911);
  ErgoVal __t1912 = __cogito_button_get_menu_vibrant(__t1911);
  ergo_release_val(__t1911);
  ergo_move_into(&__ret, __t1912);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_window(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1913 = self; ergo_retain_val(__t1913);
  ErgoVal __t1914 = __cogito_node_window(__t1913);
  ergo_release_val(__t1913);
  ergo_move_into(&__ret, __t1914);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1915 = self; ergo_retain_val(__t1915);
  ErgoVal __t1916 = a0; ergo_retain_val(__t1916);
  __cogito_node_set_disabled(__t1915, __t1916);
  ergo_release_val(__t1915);
  ergo_release_val(__t1916);
  ErgoVal __t1917 = YV_NULLV;
  ergo_release_val(__t1917);
  ErgoVal __t1918 = self; ergo_retain_val(__t1918);
  ergo_move_into(&__ret, __t1918);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1919 = self; ergo_retain_val(__t1919);
  ErgoVal __t1920 = a0; ergo_retain_val(__t1920);
  __cogito_node_set_class(__t1919, __t1920);
  ergo_release_val(__t1919);
  ergo_release_val(__t1920);
  ErgoVal __t1921 = YV_NULLV;
  ergo_release_val(__t1921);
  ErgoVal __t1922 = self; ergo_retain_val(__t1922);
  ergo_move_into(&__ret, __t1922);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Button_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1923 = self; ergo_retain_val(__t1923);
  ErgoVal __t1924 = a0; ergo_retain_val(__t1924);
  __cogito_node_set_id(__t1923, __t1924);
  ergo_release_val(__t1923);
  ergo_release_val(__t1924);
  ErgoVal __t1925 = YV_NULLV;
  ergo_release_val(__t1925);
  ErgoVal __t1926 = self; ergo_retain_val(__t1926);
  ergo_move_into(&__ret, __t1926);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1927 = self; ergo_retain_val(__t1927);
  ErgoVal __t1928 = a0; ergo_retain_val(__t1928);
  ErgoVal __t1929 = a1; ergo_retain_val(__t1929);
  ErgoVal __t1930 = a2; ergo_retain_val(__t1930);
  ErgoVal __t1931 = a3; ergo_retain_val(__t1931);
  __cogito_container_set_margins(__t1927, __t1928, __t1929, __t1930, __t1931);
  ergo_release_val(__t1927);
  ergo_release_val(__t1928);
  ergo_release_val(__t1929);
  ergo_release_val(__t1930);
  ergo_release_val(__t1931);
  ErgoVal __t1932 = YV_NULLV;
  ergo_release_val(__t1932);
  ErgoVal __t1933 = self; ergo_retain_val(__t1933);
  ergo_move_into(&__ret, __t1933);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1934 = self; ergo_retain_val(__t1934);
  ErgoVal __t1935 = a0; ergo_retain_val(__t1935);
  ErgoVal __t1936 = a1; ergo_retain_val(__t1936);
  ErgoVal __t1937 = a2; ergo_retain_val(__t1937);
  ErgoVal __t1938 = a3; ergo_retain_val(__t1938);
  __cogito_container_set_padding(__t1934, __t1935, __t1936, __t1937, __t1938);
  ergo_release_val(__t1934);
  ergo_release_val(__t1935);
  ergo_release_val(__t1936);
  ergo_release_val(__t1937);
  ergo_release_val(__t1938);
  ErgoVal __t1939 = YV_NULLV;
  ergo_release_val(__t1939);
  ErgoVal __t1940 = self; ergo_retain_val(__t1940);
  ergo_move_into(&__ret, __t1940);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1941 = self; ergo_retain_val(__t1941);
  ErgoVal __t1942 = a0; ergo_retain_val(__t1942);
  __cogito_container_set_align(__t1941, __t1942);
  ergo_release_val(__t1941);
  ergo_release_val(__t1942);
  ErgoVal __t1943 = YV_NULLV;
  ergo_release_val(__t1943);
  ErgoVal __t1944 = self; ergo_retain_val(__t1944);
  ergo_move_into(&__ret, __t1944);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1945 = self; ergo_retain_val(__t1945);
  ErgoVal __t1946 = a0; ergo_retain_val(__t1946);
  __cogito_container_set_halign(__t1945, __t1946);
  ergo_release_val(__t1945);
  ergo_release_val(__t1946);
  ErgoVal __t1947 = YV_NULLV;
  ergo_release_val(__t1947);
  ErgoVal __t1948 = self; ergo_retain_val(__t1948);
  ergo_move_into(&__ret, __t1948);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1949 = self; ergo_retain_val(__t1949);
  ErgoVal __t1950 = a0; ergo_retain_val(__t1950);
  __cogito_container_set_valign(__t1949, __t1950);
  ergo_release_val(__t1949);
  ergo_release_val(__t1950);
  ErgoVal __t1951 = YV_NULLV;
  ergo_release_val(__t1951);
  ErgoVal __t1952 = self; ergo_retain_val(__t1952);
  ergo_move_into(&__ret, __t1952);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1953 = self; ergo_retain_val(__t1953);
  ErgoVal __t1954 = a0; ergo_retain_val(__t1954);
  __cogito_container_set_halign(__t1953, __t1954);
  ergo_release_val(__t1953);
  ergo_release_val(__t1954);
  ErgoVal __t1955 = YV_NULLV;
  ergo_release_val(__t1955);
  ErgoVal __t1956 = self; ergo_retain_val(__t1956);
  ergo_move_into(&__ret, __t1956);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1957 = self; ergo_retain_val(__t1957);
  ErgoVal __t1958 = a0; ergo_retain_val(__t1958);
  __cogito_container_set_valign(__t1957, __t1958);
  ergo_release_val(__t1957);
  ergo_release_val(__t1958);
  ErgoVal __t1959 = YV_NULLV;
  ergo_release_val(__t1959);
  ErgoVal __t1960 = self; ergo_retain_val(__t1960);
  ergo_move_into(&__ret, __t1960);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1961 = self; ergo_retain_val(__t1961);
  ErgoVal __t1962 = a0; ergo_retain_val(__t1962);
  __cogito_container_set_hexpand(__t1961, __t1962);
  ergo_release_val(__t1961);
  ergo_release_val(__t1962);
  ErgoVal __t1963 = YV_NULLV;
  ergo_release_val(__t1963);
  ErgoVal __t1964 = self; ergo_retain_val(__t1964);
  ergo_move_into(&__ret, __t1964);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1965 = self; ergo_retain_val(__t1965);
  ErgoVal __t1966 = a0; ergo_retain_val(__t1966);
  __cogito_container_set_vexpand(__t1965, __t1966);
  ergo_release_val(__t1965);
  ergo_release_val(__t1966);
  ErgoVal __t1967 = YV_NULLV;
  ergo_release_val(__t1967);
  ErgoVal __t1968 = self; ergo_retain_val(__t1968);
  ergo_move_into(&__ret, __t1968);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1969 = self; ergo_retain_val(__t1969);
  ErgoVal __t1970 = YV_INT(0);
  __cogito_container_set_align(__t1969, __t1970);
  ergo_release_val(__t1969);
  ergo_release_val(__t1970);
  ErgoVal __t1971 = YV_NULLV;
  ergo_release_val(__t1971);
  ErgoVal __t1972 = self; ergo_retain_val(__t1972);
  ergo_move_into(&__ret, __t1972);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1973 = self; ergo_retain_val(__t1973);
  ErgoVal __t1974 = YV_INT(1);
  __cogito_container_set_align(__t1973, __t1974);
  ergo_release_val(__t1973);
  ergo_release_val(__t1974);
  ErgoVal __t1975 = YV_NULLV;
  ergo_release_val(__t1975);
  ErgoVal __t1976 = self; ergo_retain_val(__t1976);
  ergo_move_into(&__ret, __t1976);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1977 = self; ergo_retain_val(__t1977);
  ErgoVal __t1978 = YV_INT(2);
  __cogito_container_set_align(__t1977, __t1978);
  ergo_release_val(__t1977);
  ergo_release_val(__t1978);
  ErgoVal __t1979 = YV_NULLV;
  ergo_release_val(__t1979);
  ErgoVal __t1980 = self; ergo_retain_val(__t1980);
  ergo_move_into(&__ret, __t1980);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_add_menu(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1981 = self; ergo_retain_val(__t1981);
  ErgoVal __t1982 = a0; ergo_retain_val(__t1982);
  ErgoVal __t1983 = a1; ergo_retain_val(__t1983);
  __cogito_iconbtn_add_menu(__t1981, __t1982, __t1983);
  ergo_release_val(__t1981);
  ergo_release_val(__t1982);
  ergo_release_val(__t1983);
  ErgoVal __t1984 = YV_NULLV;
  ergo_release_val(__t1984);
  ErgoVal __t1985 = self; ergo_retain_val(__t1985);
  ergo_move_into(&__ret, __t1985);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_add_menu_section(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1986 = self; ergo_retain_val(__t1986);
  ErgoVal __t1987 = a0; ergo_retain_val(__t1987);
  ErgoVal __t1988 = a1; ergo_retain_val(__t1988);
  __cogito_iconbtn_add_menu_section(__t1986, __t1987, __t1988);
  ergo_release_val(__t1986);
  ergo_release_val(__t1987);
  ergo_release_val(__t1988);
  ErgoVal __t1989 = YV_NULLV;
  ergo_release_val(__t1989);
  ErgoVal __t1990 = self; ergo_retain_val(__t1990);
  ergo_move_into(&__ret, __t1990);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_menu_set_icon(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1991 = self; ergo_retain_val(__t1991);
  ErgoVal __t1992 = a0; ergo_retain_val(__t1992);
  __cogito_menu_set_icon(__t1991, __t1992);
  ergo_release_val(__t1991);
  ergo_release_val(__t1992);
  ErgoVal __t1993 = YV_NULLV;
  ergo_release_val(__t1993);
  ErgoVal __t1994 = self; ergo_retain_val(__t1994);
  ergo_move_into(&__ret, __t1994);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_menu_set_shortcut(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1995 = self; ergo_retain_val(__t1995);
  ErgoVal __t1996 = a0; ergo_retain_val(__t1996);
  __cogito_menu_set_shortcut(__t1995, __t1996);
  ergo_release_val(__t1995);
  ergo_release_val(__t1996);
  ErgoVal __t1997 = YV_NULLV;
  ergo_release_val(__t1997);
  ErgoVal __t1998 = self; ergo_retain_val(__t1998);
  ergo_move_into(&__ret, __t1998);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_menu_set_submenu(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t1999 = self; ergo_retain_val(__t1999);
  ErgoVal __t2000 = a0; ergo_retain_val(__t2000);
  __cogito_menu_set_submenu(__t1999, __t2000);
  ergo_release_val(__t1999);
  ergo_release_val(__t2000);
  ErgoVal __t2001 = YV_NULLV;
  ergo_release_val(__t2001);
  ErgoVal __t2002 = self; ergo_retain_val(__t2002);
  ergo_move_into(&__ret, __t2002);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_menu_set_toggled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2003 = self; ergo_retain_val(__t2003);
  ErgoVal __t2004 = a0; ergo_retain_val(__t2004);
  __cogito_menu_set_toggled(__t2003, __t2004);
  ergo_release_val(__t2003);
  ergo_release_val(__t2004);
  ErgoVal __t2005 = YV_NULLV;
  ergo_release_val(__t2005);
  ErgoVal __t2006 = self; ergo_retain_val(__t2006);
  ergo_move_into(&__ret, __t2006);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_window(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2007 = self; ergo_retain_val(__t2007);
  ErgoVal __t2008 = __cogito_node_window(__t2007);
  ergo_release_val(__t2007);
  ergo_move_into(&__ret, __t2008);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_shape(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2009 = self; ergo_retain_val(__t2009);
  ErgoVal __t2010 = a0; ergo_retain_val(__t2010);
  __cogito_iconbtn_set_shape(__t2009, __t2010);
  ergo_release_val(__t2009);
  ergo_release_val(__t2010);
  ErgoVal __t2011 = YV_NULLV;
  ergo_release_val(__t2011);
  ErgoVal __t2012 = self; ergo_retain_val(__t2012);
  ergo_move_into(&__ret, __t2012);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_shape(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2013 = self; ergo_retain_val(__t2013);
  ErgoVal __t2014 = __cogito_iconbtn_get_shape(__t2013);
  ergo_release_val(__t2013);
  ergo_move_into(&__ret, __t2014);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_color_style(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2015 = self; ergo_retain_val(__t2015);
  ErgoVal __t2016 = a0; ergo_retain_val(__t2016);
  __cogito_iconbtn_set_color_style(__t2015, __t2016);
  ergo_release_val(__t2015);
  ergo_release_val(__t2016);
  ErgoVal __t2017 = YV_NULLV;
  ergo_release_val(__t2017);
  ErgoVal __t2018 = self; ergo_retain_val(__t2018);
  ergo_move_into(&__ret, __t2018);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_color_style(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2019 = self; ergo_retain_val(__t2019);
  ErgoVal __t2020 = __cogito_iconbtn_get_color_style(__t2019);
  ergo_release_val(__t2019);
  ergo_move_into(&__ret, __t2020);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_width(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2021 = self; ergo_retain_val(__t2021);
  ErgoVal __t2022 = a0; ergo_retain_val(__t2022);
  __cogito_iconbtn_set_width(__t2021, __t2022);
  ergo_release_val(__t2021);
  ergo_release_val(__t2022);
  ErgoVal __t2023 = YV_NULLV;
  ergo_release_val(__t2023);
  ErgoVal __t2024 = self; ergo_retain_val(__t2024);
  ergo_move_into(&__ret, __t2024);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_width(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2025 = self; ergo_retain_val(__t2025);
  ErgoVal __t2026 = __cogito_iconbtn_get_width(__t2025);
  ergo_release_val(__t2025);
  ergo_move_into(&__ret, __t2026);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_toggle(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2027 = self; ergo_retain_val(__t2027);
  ErgoVal __t2028 = a0; ergo_retain_val(__t2028);
  __cogito_iconbtn_set_toggle(__t2027, __t2028);
  ergo_release_val(__t2027);
  ergo_release_val(__t2028);
  ErgoVal __t2029 = YV_NULLV;
  ergo_release_val(__t2029);
  ErgoVal __t2030 = self; ergo_retain_val(__t2030);
  ergo_move_into(&__ret, __t2030);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_toggle(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2031 = self; ergo_retain_val(__t2031);
  ErgoVal __t2032 = __cogito_iconbtn_get_toggle(__t2031);
  ergo_release_val(__t2031);
  ergo_move_into(&__ret, __t2032);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_checked(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2033 = self; ergo_retain_val(__t2033);
  ErgoVal __t2034 = a0; ergo_retain_val(__t2034);
  __cogito_iconbtn_set_checked(__t2033, __t2034);
  ergo_release_val(__t2033);
  ergo_release_val(__t2034);
  ErgoVal __t2035 = YV_NULLV;
  ergo_release_val(__t2035);
  ErgoVal __t2036 = self; ergo_retain_val(__t2036);
  ergo_move_into(&__ret, __t2036);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_checked(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2037 = self; ergo_retain_val(__t2037);
  ErgoVal __t2038 = __cogito_iconbtn_get_checked(__t2037);
  ergo_release_val(__t2037);
  ergo_move_into(&__ret, __t2038);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_size(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2039 = self; ergo_retain_val(__t2039);
  ErgoVal __t2040 = a0; ergo_retain_val(__t2040);
  __cogito_iconbtn_set_size(__t2039, __t2040);
  ergo_release_val(__t2039);
  ergo_release_val(__t2040);
  ErgoVal __t2041 = YV_NULLV;
  ergo_release_val(__t2041);
  ErgoVal __t2042 = self; ergo_retain_val(__t2042);
  ergo_move_into(&__ret, __t2042);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_size(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2043 = self; ergo_retain_val(__t2043);
  ErgoVal __t2044 = __cogito_iconbtn_get_size(__t2043);
  ergo_release_val(__t2043);
  ergo_move_into(&__ret, __t2044);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_xs(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2045 = self; ergo_retain_val(__t2045);
  ErgoVal __t2046 = YV_INT(0);
  __cogito_iconbtn_set_size(__t2045, __t2046);
  ergo_release_val(__t2045);
  ergo_release_val(__t2046);
  ErgoVal __t2047 = YV_NULLV;
  ergo_release_val(__t2047);
  ErgoVal __t2048 = self; ergo_retain_val(__t2048);
  ergo_move_into(&__ret, __t2048);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_s(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2049 = self; ergo_retain_val(__t2049);
  ErgoVal __t2050 = YV_INT(1);
  __cogito_iconbtn_set_size(__t2049, __t2050);
  ergo_release_val(__t2049);
  ergo_release_val(__t2050);
  ErgoVal __t2051 = YV_NULLV;
  ergo_release_val(__t2051);
  ErgoVal __t2052 = self; ergo_retain_val(__t2052);
  ergo_move_into(&__ret, __t2052);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_m(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2053 = self; ergo_retain_val(__t2053);
  ErgoVal __t2054 = YV_INT(2);
  __cogito_iconbtn_set_size(__t2053, __t2054);
  ergo_release_val(__t2053);
  ergo_release_val(__t2054);
  ErgoVal __t2055 = YV_NULLV;
  ergo_release_val(__t2055);
  ErgoVal __t2056 = self; ergo_retain_val(__t2056);
  ergo_move_into(&__ret, __t2056);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_l(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2057 = self; ergo_retain_val(__t2057);
  ErgoVal __t2058 = YV_INT(3);
  __cogito_iconbtn_set_size(__t2057, __t2058);
  ergo_release_val(__t2057);
  ergo_release_val(__t2058);
  ErgoVal __t2059 = YV_NULLV;
  ergo_release_val(__t2059);
  ErgoVal __t2060 = self; ergo_retain_val(__t2060);
  ergo_move_into(&__ret, __t2060);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_xl(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2061 = self; ergo_retain_val(__t2061);
  ErgoVal __t2062 = YV_INT(4);
  __cogito_iconbtn_set_size(__t2061, __t2062);
  ergo_release_val(__t2061);
  ergo_release_val(__t2062);
  ErgoVal __t2063 = YV_NULLV;
  ergo_release_val(__t2063);
  ErgoVal __t2064 = self; ergo_retain_val(__t2064);
  ergo_move_into(&__ret, __t2064);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_menu_divider(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2065 = self; ergo_retain_val(__t2065);
  ErgoVal __t2066 = a0; ergo_retain_val(__t2066);
  __cogito_iconbtn_set_menu_divider(__t2065, __t2066);
  ergo_release_val(__t2065);
  ergo_release_val(__t2066);
  ErgoVal __t2067 = YV_NULLV;
  ergo_release_val(__t2067);
  ErgoVal __t2068 = self; ergo_retain_val(__t2068);
  ergo_move_into(&__ret, __t2068);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_menu_divider(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2069 = self; ergo_retain_val(__t2069);
  ErgoVal __t2070 = __cogito_iconbtn_get_menu_divider(__t2069);
  ergo_release_val(__t2069);
  ergo_move_into(&__ret, __t2070);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_menu_item_gap(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2071 = self; ergo_retain_val(__t2071);
  ErgoVal __t2072 = a0; ergo_retain_val(__t2072);
  __cogito_iconbtn_set_menu_item_gap(__t2071, __t2072);
  ergo_release_val(__t2071);
  ergo_release_val(__t2072);
  ErgoVal __t2073 = YV_NULLV;
  ergo_release_val(__t2073);
  ErgoVal __t2074 = self; ergo_retain_val(__t2074);
  ergo_move_into(&__ret, __t2074);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_menu_item_gap(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2075 = self; ergo_retain_val(__t2075);
  ErgoVal __t2076 = __cogito_iconbtn_get_menu_item_gap(__t2075);
  ergo_release_val(__t2075);
  ergo_move_into(&__ret, __t2076);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_menu_vibrant(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2077 = self; ergo_retain_val(__t2077);
  ErgoVal __t2078 = a0; ergo_retain_val(__t2078);
  __cogito_iconbtn_set_menu_vibrant(__t2077, __t2078);
  ergo_release_val(__t2077);
  ergo_release_val(__t2078);
  ErgoVal __t2079 = YV_NULLV;
  ergo_release_val(__t2079);
  ErgoVal __t2080 = self; ergo_retain_val(__t2080);
  ergo_move_into(&__ret, __t2080);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_menu_vibrant(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2081 = self; ergo_retain_val(__t2081);
  ErgoVal __t2082 = __cogito_iconbtn_get_menu_vibrant(__t2081);
  ergo_release_val(__t2081);
  ergo_move_into(&__ret, __t2082);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_on_click(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2083 = self; ergo_retain_val(__t2083);
  ErgoVal __t2084 = a0; ergo_retain_val(__t2084);
  __cogito_iconbtn_on_click(__t2083, __t2084);
  ergo_release_val(__t2083);
  ergo_release_val(__t2084);
  ErgoVal __t2085 = YV_NULLV;
  ergo_release_val(__t2085);
  ErgoVal __t2086 = self; ergo_retain_val(__t2086);
  ergo_move_into(&__ret, __t2086);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2087 = self; ergo_retain_val(__t2087);
  ErgoVal __t2088 = a0; ergo_retain_val(__t2088);
  __cogito_node_set_disabled(__t2087, __t2088);
  ergo_release_val(__t2087);
  ergo_release_val(__t2088);
  ErgoVal __t2089 = YV_NULLV;
  ergo_release_val(__t2089);
  ErgoVal __t2090 = self; ergo_retain_val(__t2090);
  ergo_move_into(&__ret, __t2090);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2091 = self; ergo_retain_val(__t2091);
  ErgoVal __t2092 = a0; ergo_retain_val(__t2092);
  __cogito_node_set_class(__t2091, __t2092);
  ergo_release_val(__t2091);
  ergo_release_val(__t2092);
  ErgoVal __t2093 = YV_NULLV;
  ergo_release_val(__t2093);
  ErgoVal __t2094 = self; ergo_retain_val(__t2094);
  ergo_move_into(&__ret, __t2094);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_IconButton_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2095 = self; ergo_retain_val(__t2095);
  ErgoVal __t2096 = a0; ergo_retain_val(__t2096);
  __cogito_node_set_id(__t2095, __t2096);
  ergo_release_val(__t2095);
  ergo_release_val(__t2096);
  ErgoVal __t2097 = YV_NULLV;
  ergo_release_val(__t2097);
  ErgoVal __t2098 = self; ergo_retain_val(__t2098);
  ergo_move_into(&__ret, __t2098);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2099 = self; ergo_retain_val(__t2099);
  ErgoVal __t2100 = a0; ergo_retain_val(__t2100);
  ErgoVal __t2101 = a1; ergo_retain_val(__t2101);
  ErgoVal __t2102 = a2; ergo_retain_val(__t2102);
  ErgoVal __t2103 = a3; ergo_retain_val(__t2103);
  __cogito_container_set_margins(__t2099, __t2100, __t2101, __t2102, __t2103);
  ergo_release_val(__t2099);
  ergo_release_val(__t2100);
  ergo_release_val(__t2101);
  ergo_release_val(__t2102);
  ergo_release_val(__t2103);
  ErgoVal __t2104 = YV_NULLV;
  ergo_release_val(__t2104);
  ErgoVal __t2105 = self; ergo_retain_val(__t2105);
  ergo_move_into(&__ret, __t2105);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2106 = self; ergo_retain_val(__t2106);
  ErgoVal __t2107 = a0; ergo_retain_val(__t2107);
  ErgoVal __t2108 = a1; ergo_retain_val(__t2108);
  ErgoVal __t2109 = a2; ergo_retain_val(__t2109);
  ErgoVal __t2110 = a3; ergo_retain_val(__t2110);
  __cogito_container_set_padding(__t2106, __t2107, __t2108, __t2109, __t2110);
  ergo_release_val(__t2106);
  ergo_release_val(__t2107);
  ergo_release_val(__t2108);
  ergo_release_val(__t2109);
  ergo_release_val(__t2110);
  ErgoVal __t2111 = YV_NULLV;
  ergo_release_val(__t2111);
  ErgoVal __t2112 = self; ergo_retain_val(__t2112);
  ergo_move_into(&__ret, __t2112);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2113 = self; ergo_retain_val(__t2113);
  ErgoVal __t2114 = a0; ergo_retain_val(__t2114);
  __cogito_container_set_align(__t2113, __t2114);
  ergo_release_val(__t2113);
  ergo_release_val(__t2114);
  ErgoVal __t2115 = YV_NULLV;
  ergo_release_val(__t2115);
  ErgoVal __t2116 = self; ergo_retain_val(__t2116);
  ergo_move_into(&__ret, __t2116);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2117 = self; ergo_retain_val(__t2117);
  ErgoVal __t2118 = a0; ergo_retain_val(__t2118);
  __cogito_container_set_halign(__t2117, __t2118);
  ergo_release_val(__t2117);
  ergo_release_val(__t2118);
  ErgoVal __t2119 = YV_NULLV;
  ergo_release_val(__t2119);
  ErgoVal __t2120 = self; ergo_retain_val(__t2120);
  ergo_move_into(&__ret, __t2120);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2121 = self; ergo_retain_val(__t2121);
  ErgoVal __t2122 = a0; ergo_retain_val(__t2122);
  __cogito_container_set_valign(__t2121, __t2122);
  ergo_release_val(__t2121);
  ergo_release_val(__t2122);
  ErgoVal __t2123 = YV_NULLV;
  ergo_release_val(__t2123);
  ErgoVal __t2124 = self; ergo_retain_val(__t2124);
  ergo_move_into(&__ret, __t2124);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2125 = self; ergo_retain_val(__t2125);
  ErgoVal __t2126 = YV_INT(0);
  __cogito_container_set_align(__t2125, __t2126);
  ergo_release_val(__t2125);
  ergo_release_val(__t2126);
  ErgoVal __t2127 = YV_NULLV;
  ergo_release_val(__t2127);
  ErgoVal __t2128 = self; ergo_retain_val(__t2128);
  ergo_move_into(&__ret, __t2128);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2129 = self; ergo_retain_val(__t2129);
  ErgoVal __t2130 = YV_INT(1);
  __cogito_container_set_align(__t2129, __t2130);
  ergo_release_val(__t2129);
  ergo_release_val(__t2130);
  ErgoVal __t2131 = YV_NULLV;
  ergo_release_val(__t2131);
  ErgoVal __t2132 = self; ergo_retain_val(__t2132);
  ergo_move_into(&__ret, __t2132);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2133 = self; ergo_retain_val(__t2133);
  ErgoVal __t2134 = YV_INT(2);
  __cogito_container_set_align(__t2133, __t2134);
  ergo_release_val(__t2133);
  ergo_release_val(__t2134);
  ErgoVal __t2135 = YV_NULLV;
  ergo_release_val(__t2135);
  ErgoVal __t2136 = self; ergo_retain_val(__t2136);
  ergo_move_into(&__ret, __t2136);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_set_checked(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2137 = self; ergo_retain_val(__t2137);
  ErgoVal __t2138 = a0; ergo_retain_val(__t2138);
  __cogito_checkbox_set_checked(__t2137, __t2138);
  ergo_release_val(__t2137);
  ergo_release_val(__t2138);
  ErgoVal __t2139 = YV_NULLV;
  ergo_release_val(__t2139);
  ErgoVal __t2140 = self; ergo_retain_val(__t2140);
  ergo_move_into(&__ret, __t2140);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_checked(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2141 = self; ergo_retain_val(__t2141);
  ErgoVal __t2142 = __cogito_checkbox_get_checked(__t2141);
  ergo_release_val(__t2141);
  ergo_move_into(&__ret, __t2142);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2143 = self; ergo_retain_val(__t2143);
  ErgoVal __t2144 = a0; ergo_retain_val(__t2144);
  __cogito_checkbox_on_change(__t2143, __t2144);
  ergo_release_val(__t2143);
  ergo_release_val(__t2144);
  ErgoVal __t2145 = YV_NULLV;
  ergo_release_val(__t2145);
  ErgoVal __t2146 = self; ergo_retain_val(__t2146);
  ergo_move_into(&__ret, __t2146);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_window(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2147 = self; ergo_retain_val(__t2147);
  ErgoVal __t2148 = __cogito_node_window(__t2147);
  ergo_release_val(__t2147);
  ergo_move_into(&__ret, __t2148);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2149 = self; ergo_retain_val(__t2149);
  ErgoVal __t2150 = a0; ergo_retain_val(__t2150);
  __cogito_container_set_hexpand(__t2149, __t2150);
  ergo_release_val(__t2149);
  ergo_release_val(__t2150);
  ErgoVal __t2151 = YV_NULLV;
  ergo_release_val(__t2151);
  ErgoVal __t2152 = self; ergo_retain_val(__t2152);
  ergo_move_into(&__ret, __t2152);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2153 = self; ergo_retain_val(__t2153);
  ErgoVal __t2154 = a0; ergo_retain_val(__t2154);
  __cogito_container_set_vexpand(__t2153, __t2154);
  ergo_release_val(__t2153);
  ergo_release_val(__t2154);
  ErgoVal __t2155 = YV_NULLV;
  ergo_release_val(__t2155);
  ErgoVal __t2156 = self; ergo_retain_val(__t2156);
  ergo_move_into(&__ret, __t2156);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2157 = self; ergo_retain_val(__t2157);
  ErgoVal __t2158 = a0; ergo_retain_val(__t2158);
  __cogito_node_set_disabled(__t2157, __t2158);
  ergo_release_val(__t2157);
  ergo_release_val(__t2158);
  ErgoVal __t2159 = YV_NULLV;
  ergo_release_val(__t2159);
  ErgoVal __t2160 = self; ergo_retain_val(__t2160);
  ergo_move_into(&__ret, __t2160);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2161 = self; ergo_retain_val(__t2161);
  ErgoVal __t2162 = a0; ergo_retain_val(__t2162);
  __cogito_node_set_class(__t2161, __t2162);
  ergo_release_val(__t2161);
  ergo_release_val(__t2162);
  ErgoVal __t2163 = YV_NULLV;
  ergo_release_val(__t2163);
  ErgoVal __t2164 = self; ergo_retain_val(__t2164);
  ergo_move_into(&__ret, __t2164);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Checkbox_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2165 = self; ergo_retain_val(__t2165);
  ErgoVal __t2166 = a0; ergo_retain_val(__t2166);
  __cogito_node_set_id(__t2165, __t2166);
  ergo_release_val(__t2165);
  ergo_release_val(__t2166);
  ErgoVal __t2167 = YV_NULLV;
  ergo_release_val(__t2167);
  ErgoVal __t2168 = self; ergo_retain_val(__t2168);
  ergo_move_into(&__ret, __t2168);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2169 = self; ergo_retain_val(__t2169);
  ErgoVal __t2170 = a0; ergo_retain_val(__t2170);
  ErgoVal __t2171 = a1; ergo_retain_val(__t2171);
  ErgoVal __t2172 = a2; ergo_retain_val(__t2172);
  ErgoVal __t2173 = a3; ergo_retain_val(__t2173);
  __cogito_container_set_margins(__t2169, __t2170, __t2171, __t2172, __t2173);
  ergo_release_val(__t2169);
  ergo_release_val(__t2170);
  ergo_release_val(__t2171);
  ergo_release_val(__t2172);
  ergo_release_val(__t2173);
  ErgoVal __t2174 = YV_NULLV;
  ergo_release_val(__t2174);
  ErgoVal __t2175 = self; ergo_retain_val(__t2175);
  ergo_move_into(&__ret, __t2175);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2176 = self; ergo_retain_val(__t2176);
  ErgoVal __t2177 = a0; ergo_retain_val(__t2177);
  ErgoVal __t2178 = a1; ergo_retain_val(__t2178);
  ErgoVal __t2179 = a2; ergo_retain_val(__t2179);
  ErgoVal __t2180 = a3; ergo_retain_val(__t2180);
  __cogito_container_set_padding(__t2176, __t2177, __t2178, __t2179, __t2180);
  ergo_release_val(__t2176);
  ergo_release_val(__t2177);
  ergo_release_val(__t2178);
  ergo_release_val(__t2179);
  ergo_release_val(__t2180);
  ErgoVal __t2181 = YV_NULLV;
  ergo_release_val(__t2181);
  ErgoVal __t2182 = self; ergo_retain_val(__t2182);
  ergo_move_into(&__ret, __t2182);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2183 = self; ergo_retain_val(__t2183);
  ErgoVal __t2184 = a0; ergo_retain_val(__t2184);
  __cogito_container_set_align(__t2183, __t2184);
  ergo_release_val(__t2183);
  ergo_release_val(__t2184);
  ErgoVal __t2185 = YV_NULLV;
  ergo_release_val(__t2185);
  ErgoVal __t2186 = self; ergo_retain_val(__t2186);
  ergo_move_into(&__ret, __t2186);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2187 = self; ergo_retain_val(__t2187);
  ErgoVal __t2188 = a0; ergo_retain_val(__t2188);
  __cogito_container_set_halign(__t2187, __t2188);
  ergo_release_val(__t2187);
  ergo_release_val(__t2188);
  ErgoVal __t2189 = YV_NULLV;
  ergo_release_val(__t2189);
  ErgoVal __t2190 = self; ergo_retain_val(__t2190);
  ergo_move_into(&__ret, __t2190);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2191 = self; ergo_retain_val(__t2191);
  ErgoVal __t2192 = a0; ergo_retain_val(__t2192);
  __cogito_container_set_valign(__t2191, __t2192);
  ergo_release_val(__t2191);
  ergo_release_val(__t2192);
  ErgoVal __t2193 = YV_NULLV;
  ergo_release_val(__t2193);
  ErgoVal __t2194 = self; ergo_retain_val(__t2194);
  ergo_move_into(&__ret, __t2194);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2195 = self; ergo_retain_val(__t2195);
  ErgoVal __t2196 = YV_INT(0);
  __cogito_container_set_align(__t2195, __t2196);
  ergo_release_val(__t2195);
  ergo_release_val(__t2196);
  ErgoVal __t2197 = YV_NULLV;
  ergo_release_val(__t2197);
  ErgoVal __t2198 = self; ergo_retain_val(__t2198);
  ergo_move_into(&__ret, __t2198);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2199 = self; ergo_retain_val(__t2199);
  ErgoVal __t2200 = YV_INT(1);
  __cogito_container_set_align(__t2199, __t2200);
  ergo_release_val(__t2199);
  ergo_release_val(__t2200);
  ErgoVal __t2201 = YV_NULLV;
  ergo_release_val(__t2201);
  ErgoVal __t2202 = self; ergo_retain_val(__t2202);
  ergo_move_into(&__ret, __t2202);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2203 = self; ergo_retain_val(__t2203);
  ErgoVal __t2204 = YV_INT(2);
  __cogito_container_set_align(__t2203, __t2204);
  ergo_release_val(__t2203);
  ergo_release_val(__t2204);
  ErgoVal __t2205 = YV_NULLV;
  ergo_release_val(__t2205);
  ErgoVal __t2206 = self; ergo_retain_val(__t2206);
  ergo_move_into(&__ret, __t2206);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_set_checked(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2207 = self; ergo_retain_val(__t2207);
  ErgoVal __t2208 = a0; ergo_retain_val(__t2208);
  __cogito_switch_set_checked(__t2207, __t2208);
  ergo_release_val(__t2207);
  ergo_release_val(__t2208);
  ErgoVal __t2209 = YV_NULLV;
  ergo_release_val(__t2209);
  ErgoVal __t2210 = self; ergo_retain_val(__t2210);
  ergo_move_into(&__ret, __t2210);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_checked(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2211 = self; ergo_retain_val(__t2211);
  ErgoVal __t2212 = __cogito_switch_get_checked(__t2211);
  ergo_release_val(__t2211);
  ergo_move_into(&__ret, __t2212);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2213 = self; ergo_retain_val(__t2213);
  ErgoVal __t2214 = a0; ergo_retain_val(__t2214);
  __cogito_switch_on_change(__t2213, __t2214);
  ergo_release_val(__t2213);
  ergo_release_val(__t2214);
  ErgoVal __t2215 = YV_NULLV;
  ergo_release_val(__t2215);
  ErgoVal __t2216 = self; ergo_retain_val(__t2216);
  ergo_move_into(&__ret, __t2216);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_window(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2217 = self; ergo_retain_val(__t2217);
  ErgoVal __t2218 = __cogito_node_window(__t2217);
  ergo_release_val(__t2217);
  ergo_move_into(&__ret, __t2218);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2219 = self; ergo_retain_val(__t2219);
  ErgoVal __t2220 = a0; ergo_retain_val(__t2220);
  __cogito_container_set_hexpand(__t2219, __t2220);
  ergo_release_val(__t2219);
  ergo_release_val(__t2220);
  ErgoVal __t2221 = YV_NULLV;
  ergo_release_val(__t2221);
  ErgoVal __t2222 = self; ergo_retain_val(__t2222);
  ergo_move_into(&__ret, __t2222);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2223 = self; ergo_retain_val(__t2223);
  ErgoVal __t2224 = a0; ergo_retain_val(__t2224);
  __cogito_container_set_vexpand(__t2223, __t2224);
  ergo_release_val(__t2223);
  ergo_release_val(__t2224);
  ErgoVal __t2225 = YV_NULLV;
  ergo_release_val(__t2225);
  ErgoVal __t2226 = self; ergo_retain_val(__t2226);
  ergo_move_into(&__ret, __t2226);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2227 = self; ergo_retain_val(__t2227);
  ErgoVal __t2228 = a0; ergo_retain_val(__t2228);
  __cogito_node_set_disabled(__t2227, __t2228);
  ergo_release_val(__t2227);
  ergo_release_val(__t2228);
  ErgoVal __t2229 = YV_NULLV;
  ergo_release_val(__t2229);
  ErgoVal __t2230 = self; ergo_retain_val(__t2230);
  ergo_move_into(&__ret, __t2230);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2231 = self; ergo_retain_val(__t2231);
  ErgoVal __t2232 = a0; ergo_retain_val(__t2232);
  __cogito_node_set_class(__t2231, __t2232);
  ergo_release_val(__t2231);
  ergo_release_val(__t2232);
  ErgoVal __t2233 = YV_NULLV;
  ergo_release_val(__t2233);
  ErgoVal __t2234 = self; ergo_retain_val(__t2234);
  ergo_move_into(&__ret, __t2234);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Switch_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2235 = self; ergo_retain_val(__t2235);
  ErgoVal __t2236 = a0; ergo_retain_val(__t2236);
  __cogito_node_set_id(__t2235, __t2236);
  ergo_release_val(__t2235);
  ergo_release_val(__t2236);
  ErgoVal __t2237 = YV_NULLV;
  ergo_release_val(__t2237);
  ErgoVal __t2238 = self; ergo_retain_val(__t2238);
  ergo_move_into(&__ret, __t2238);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2239 = self; ergo_retain_val(__t2239);
  ErgoVal __t2240 = a0; ergo_retain_val(__t2240);
  ErgoVal __t2241 = a1; ergo_retain_val(__t2241);
  ErgoVal __t2242 = a2; ergo_retain_val(__t2242);
  ErgoVal __t2243 = a3; ergo_retain_val(__t2243);
  __cogito_container_set_margins(__t2239, __t2240, __t2241, __t2242, __t2243);
  ergo_release_val(__t2239);
  ergo_release_val(__t2240);
  ergo_release_val(__t2241);
  ergo_release_val(__t2242);
  ergo_release_val(__t2243);
  ErgoVal __t2244 = YV_NULLV;
  ergo_release_val(__t2244);
  ErgoVal __t2245 = self; ergo_retain_val(__t2245);
  ergo_move_into(&__ret, __t2245);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2246 = self; ergo_retain_val(__t2246);
  ErgoVal __t2247 = a0; ergo_retain_val(__t2247);
  ErgoVal __t2248 = a1; ergo_retain_val(__t2248);
  ErgoVal __t2249 = a2; ergo_retain_val(__t2249);
  ErgoVal __t2250 = a3; ergo_retain_val(__t2250);
  __cogito_container_set_padding(__t2246, __t2247, __t2248, __t2249, __t2250);
  ergo_release_val(__t2246);
  ergo_release_val(__t2247);
  ergo_release_val(__t2248);
  ergo_release_val(__t2249);
  ergo_release_val(__t2250);
  ErgoVal __t2251 = YV_NULLV;
  ergo_release_val(__t2251);
  ErgoVal __t2252 = self; ergo_retain_val(__t2252);
  ergo_move_into(&__ret, __t2252);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2253 = self; ergo_retain_val(__t2253);
  ErgoVal __t2254 = a0; ergo_retain_val(__t2254);
  __cogito_container_set_align(__t2253, __t2254);
  ergo_release_val(__t2253);
  ergo_release_val(__t2254);
  ErgoVal __t2255 = YV_NULLV;
  ergo_release_val(__t2255);
  ErgoVal __t2256 = self; ergo_retain_val(__t2256);
  ergo_move_into(&__ret, __t2256);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2257 = self; ergo_retain_val(__t2257);
  ErgoVal __t2258 = a0; ergo_retain_val(__t2258);
  __cogito_container_set_halign(__t2257, __t2258);
  ergo_release_val(__t2257);
  ergo_release_val(__t2258);
  ErgoVal __t2259 = YV_NULLV;
  ergo_release_val(__t2259);
  ErgoVal __t2260 = self; ergo_retain_val(__t2260);
  ergo_move_into(&__ret, __t2260);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2261 = self; ergo_retain_val(__t2261);
  ErgoVal __t2262 = a0; ergo_retain_val(__t2262);
  __cogito_container_set_valign(__t2261, __t2262);
  ergo_release_val(__t2261);
  ergo_release_val(__t2262);
  ErgoVal __t2263 = YV_NULLV;
  ergo_release_val(__t2263);
  ErgoVal __t2264 = self; ergo_retain_val(__t2264);
  ergo_move_into(&__ret, __t2264);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2265 = self; ergo_retain_val(__t2265);
  ErgoVal __t2266 = a0; ergo_retain_val(__t2266);
  __cogito_searchfield_set_text(__t2265, __t2266);
  ergo_release_val(__t2265);
  ergo_release_val(__t2266);
  ErgoVal __t2267 = YV_NULLV;
  ergo_release_val(__t2267);
  ErgoVal __t2268 = self; ergo_retain_val(__t2268);
  ergo_move_into(&__ret, __t2268);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_text(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2269 = self; ergo_retain_val(__t2269);
  ErgoVal __t2270 = __cogito_searchfield_get_text(__t2269);
  ergo_release_val(__t2269);
  ergo_move_into(&__ret, __t2270);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2271 = self; ergo_retain_val(__t2271);
  ErgoVal __t2272 = a0; ergo_retain_val(__t2272);
  __cogito_searchfield_on_change(__t2271, __t2272);
  ergo_release_val(__t2271);
  ergo_release_val(__t2272);
  ErgoVal __t2273 = YV_NULLV;
  ergo_release_val(__t2273);
  ErgoVal __t2274 = self; ergo_retain_val(__t2274);
  ergo_move_into(&__ret, __t2274);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2275 = self; ergo_retain_val(__t2275);
  ErgoVal __t2276 = a0; ergo_retain_val(__t2276);
  __cogito_container_set_hexpand(__t2275, __t2276);
  ergo_release_val(__t2275);
  ergo_release_val(__t2276);
  ErgoVal __t2277 = YV_NULLV;
  ergo_release_val(__t2277);
  ErgoVal __t2278 = self; ergo_retain_val(__t2278);
  ergo_move_into(&__ret, __t2278);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2279 = self; ergo_retain_val(__t2279);
  ErgoVal __t2280 = a0; ergo_retain_val(__t2280);
  __cogito_container_set_vexpand(__t2279, __t2280);
  ergo_release_val(__t2279);
  ergo_release_val(__t2280);
  ErgoVal __t2281 = YV_NULLV;
  ergo_release_val(__t2281);
  ErgoVal __t2282 = self; ergo_retain_val(__t2282);
  ergo_move_into(&__ret, __t2282);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_set_editable(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2283 = self; ergo_retain_val(__t2283);
  ErgoVal __t2284 = a0; ergo_retain_val(__t2284);
  __cogito_node_set_editable(__t2283, __t2284);
  ergo_release_val(__t2283);
  ergo_release_val(__t2284);
  ErgoVal __t2285 = YV_NULLV;
  ergo_release_val(__t2285);
  ErgoVal __t2286 = self; ergo_retain_val(__t2286);
  ergo_move_into(&__ret, __t2286);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_editable(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2287 = self; ergo_retain_val(__t2287);
  ErgoVal __t2288 = __cogito_node_get_editable(__t2287);
  ergo_release_val(__t2287);
  ergo_move_into(&__ret, __t2288);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2289 = self; ergo_retain_val(__t2289);
  ErgoVal __t2290 = a0; ergo_retain_val(__t2290);
  __cogito_node_set_class(__t2289, __t2290);
  ergo_release_val(__t2289);
  ergo_release_val(__t2290);
  ErgoVal __t2291 = YV_NULLV;
  ergo_release_val(__t2291);
  ErgoVal __t2292 = self; ergo_retain_val(__t2292);
  ergo_move_into(&__ret, __t2292);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SearchField_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2293 = self; ergo_retain_val(__t2293);
  ErgoVal __t2294 = a0; ergo_retain_val(__t2294);
  __cogito_node_set_id(__t2293, __t2294);
  ergo_release_val(__t2293);
  ergo_release_val(__t2294);
  ErgoVal __t2295 = YV_NULLV;
  ergo_release_val(__t2295);
  ErgoVal __t2296 = self; ergo_retain_val(__t2296);
  ergo_move_into(&__ret, __t2296);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2297 = self; ergo_retain_val(__t2297);
  ErgoVal __t2298 = a0; ergo_retain_val(__t2298);
  ErgoVal __t2299 = a1; ergo_retain_val(__t2299);
  ErgoVal __t2300 = a2; ergo_retain_val(__t2300);
  ErgoVal __t2301 = a3; ergo_retain_val(__t2301);
  __cogito_container_set_margins(__t2297, __t2298, __t2299, __t2300, __t2301);
  ergo_release_val(__t2297);
  ergo_release_val(__t2298);
  ergo_release_val(__t2299);
  ergo_release_val(__t2300);
  ergo_release_val(__t2301);
  ErgoVal __t2302 = YV_NULLV;
  ergo_release_val(__t2302);
  ErgoVal __t2303 = self; ergo_retain_val(__t2303);
  ergo_move_into(&__ret, __t2303);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2304 = self; ergo_retain_val(__t2304);
  ErgoVal __t2305 = a0; ergo_retain_val(__t2305);
  ErgoVal __t2306 = a1; ergo_retain_val(__t2306);
  ErgoVal __t2307 = a2; ergo_retain_val(__t2307);
  ErgoVal __t2308 = a3; ergo_retain_val(__t2308);
  __cogito_container_set_padding(__t2304, __t2305, __t2306, __t2307, __t2308);
  ergo_release_val(__t2304);
  ergo_release_val(__t2305);
  ergo_release_val(__t2306);
  ergo_release_val(__t2307);
  ergo_release_val(__t2308);
  ErgoVal __t2309 = YV_NULLV;
  ergo_release_val(__t2309);
  ErgoVal __t2310 = self; ergo_retain_val(__t2310);
  ergo_move_into(&__ret, __t2310);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2311 = self; ergo_retain_val(__t2311);
  ErgoVal __t2312 = a0; ergo_retain_val(__t2312);
  __cogito_container_set_align(__t2311, __t2312);
  ergo_release_val(__t2311);
  ergo_release_val(__t2312);
  ErgoVal __t2313 = YV_NULLV;
  ergo_release_val(__t2313);
  ErgoVal __t2314 = self; ergo_retain_val(__t2314);
  ergo_move_into(&__ret, __t2314);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2315 = self; ergo_retain_val(__t2315);
  ErgoVal __t2316 = a0; ergo_retain_val(__t2316);
  __cogito_container_set_halign(__t2315, __t2316);
  ergo_release_val(__t2315);
  ergo_release_val(__t2316);
  ErgoVal __t2317 = YV_NULLV;
  ergo_release_val(__t2317);
  ErgoVal __t2318 = self; ergo_retain_val(__t2318);
  ergo_move_into(&__ret, __t2318);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2319 = self; ergo_retain_val(__t2319);
  ErgoVal __t2320 = a0; ergo_retain_val(__t2320);
  __cogito_container_set_valign(__t2319, __t2320);
  ergo_release_val(__t2319);
  ergo_release_val(__t2320);
  ErgoVal __t2321 = YV_NULLV;
  ergo_release_val(__t2321);
  ErgoVal __t2322 = self; ergo_retain_val(__t2322);
  ergo_move_into(&__ret, __t2322);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2323 = self; ergo_retain_val(__t2323);
  ErgoVal __t2324 = YV_INT(0);
  __cogito_container_set_align(__t2323, __t2324);
  ergo_release_val(__t2323);
  ergo_release_val(__t2324);
  ErgoVal __t2325 = YV_NULLV;
  ergo_release_val(__t2325);
  ErgoVal __t2326 = self; ergo_retain_val(__t2326);
  ergo_move_into(&__ret, __t2326);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2327 = self; ergo_retain_val(__t2327);
  ErgoVal __t2328 = YV_INT(1);
  __cogito_container_set_align(__t2327, __t2328);
  ergo_release_val(__t2327);
  ergo_release_val(__t2328);
  ErgoVal __t2329 = YV_NULLV;
  ergo_release_val(__t2329);
  ErgoVal __t2330 = self; ergo_retain_val(__t2330);
  ergo_move_into(&__ret, __t2330);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2331 = self; ergo_retain_val(__t2331);
  ErgoVal __t2332 = YV_INT(2);
  __cogito_container_set_align(__t2331, __t2332);
  ergo_release_val(__t2331);
  ergo_release_val(__t2332);
  ErgoVal __t2333 = YV_NULLV;
  ergo_release_val(__t2333);
  ErgoVal __t2334 = self; ergo_retain_val(__t2334);
  ergo_move_into(&__ret, __t2334);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2335 = self; ergo_retain_val(__t2335);
  ErgoVal __t2336 = a0; ergo_retain_val(__t2336);
  __cogito_textfield_set_text(__t2335, __t2336);
  ergo_release_val(__t2335);
  ergo_release_val(__t2336);
  ErgoVal __t2337 = YV_NULLV;
  ergo_release_val(__t2337);
  ErgoVal __t2338 = self; ergo_retain_val(__t2338);
  ergo_move_into(&__ret, __t2338);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_text(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2339 = self; ergo_retain_val(__t2339);
  ErgoVal __t2340 = __cogito_textfield_get_text(__t2339);
  ergo_release_val(__t2339);
  ergo_move_into(&__ret, __t2340);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_hint_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2341 = self; ergo_retain_val(__t2341);
  ErgoVal __t2342 = a0; ergo_retain_val(__t2342);
  __cogito_textfield_set_hint(__t2341, __t2342);
  ergo_release_val(__t2341);
  ergo_release_val(__t2342);
  ErgoVal __t2343 = YV_NULLV;
  ergo_release_val(__t2343);
  ErgoVal __t2344 = self; ergo_retain_val(__t2344);
  ergo_move_into(&__ret, __t2344);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_hint_text(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2345 = self; ergo_retain_val(__t2345);
  ErgoVal __t2346 = __cogito_textfield_get_hint(__t2345);
  ergo_release_val(__t2345);
  ergo_move_into(&__ret, __t2346);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_hint(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2347 = self; ergo_retain_val(__t2347);
  ErgoVal __t2348 = a0; ergo_retain_val(__t2348);
  __cogito_textfield_set_hint(__t2347, __t2348);
  ergo_release_val(__t2347);
  ergo_release_val(__t2348);
  ErgoVal __t2349 = YV_NULLV;
  ergo_release_val(__t2349);
  ErgoVal __t2350 = self; ergo_retain_val(__t2350);
  ergo_move_into(&__ret, __t2350);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_hint(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2351 = self; ergo_retain_val(__t2351);
  ErgoVal __t2352 = __cogito_textfield_get_hint(__t2351);
  ergo_release_val(__t2351);
  ergo_move_into(&__ret, __t2352);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2353 = self; ergo_retain_val(__t2353);
  ErgoVal __t2354 = a0; ergo_retain_val(__t2354);
  __cogito_textfield_on_change(__t2353, __t2354);
  ergo_release_val(__t2353);
  ergo_release_val(__t2354);
  ErgoVal __t2355 = YV_NULLV;
  ergo_release_val(__t2355);
  ErgoVal __t2356 = self; ergo_retain_val(__t2356);
  ergo_move_into(&__ret, __t2356);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2357 = self; ergo_retain_val(__t2357);
  ErgoVal __t2358 = a0; ergo_retain_val(__t2358);
  __cogito_container_set_hexpand(__t2357, __t2358);
  ergo_release_val(__t2357);
  ergo_release_val(__t2358);
  ErgoVal __t2359 = YV_NULLV;
  ergo_release_val(__t2359);
  ErgoVal __t2360 = self; ergo_retain_val(__t2360);
  ergo_move_into(&__ret, __t2360);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2361 = self; ergo_retain_val(__t2361);
  ErgoVal __t2362 = a0; ergo_retain_val(__t2362);
  __cogito_container_set_vexpand(__t2361, __t2362);
  ergo_release_val(__t2361);
  ergo_release_val(__t2362);
  ErgoVal __t2363 = YV_NULLV;
  ergo_release_val(__t2363);
  ErgoVal __t2364 = self; ergo_retain_val(__t2364);
  ergo_move_into(&__ret, __t2364);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_editable(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2365 = self; ergo_retain_val(__t2365);
  ErgoVal __t2366 = a0; ergo_retain_val(__t2366);
  __cogito_node_set_editable(__t2365, __t2366);
  ergo_release_val(__t2365);
  ergo_release_val(__t2366);
  ErgoVal __t2367 = YV_NULLV;
  ergo_release_val(__t2367);
  ErgoVal __t2368 = self; ergo_retain_val(__t2368);
  ergo_move_into(&__ret, __t2368);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_editable(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2369 = self; ergo_retain_val(__t2369);
  ErgoVal __t2370 = __cogito_node_get_editable(__t2369);
  ergo_release_val(__t2369);
  ergo_move_into(&__ret, __t2370);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2371 = self; ergo_retain_val(__t2371);
  ErgoVal __t2372 = a0; ergo_retain_val(__t2372);
  __cogito_node_set_disabled(__t2371, __t2372);
  ergo_release_val(__t2371);
  ergo_release_val(__t2372);
  ErgoVal __t2373 = YV_NULLV;
  ergo_release_val(__t2373);
  ErgoVal __t2374 = self; ergo_retain_val(__t2374);
  ergo_move_into(&__ret, __t2374);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2375 = self; ergo_retain_val(__t2375);
  ErgoVal __t2376 = a0; ergo_retain_val(__t2376);
  __cogito_node_set_class(__t2375, __t2376);
  ergo_release_val(__t2375);
  ergo_release_val(__t2376);
  ErgoVal __t2377 = YV_NULLV;
  ergo_release_val(__t2377);
  ErgoVal __t2378 = self; ergo_retain_val(__t2378);
  ergo_move_into(&__ret, __t2378);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextField_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2379 = self; ergo_retain_val(__t2379);
  ErgoVal __t2380 = a0; ergo_retain_val(__t2380);
  __cogito_node_set_id(__t2379, __t2380);
  ergo_release_val(__t2379);
  ergo_release_val(__t2380);
  ErgoVal __t2381 = YV_NULLV;
  ergo_release_val(__t2381);
  ErgoVal __t2382 = self; ergo_retain_val(__t2382);
  ergo_move_into(&__ret, __t2382);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2383 = self; ergo_retain_val(__t2383);
  ErgoVal __t2384 = a0; ergo_retain_val(__t2384);
  ErgoVal __t2385 = a1; ergo_retain_val(__t2385);
  ErgoVal __t2386 = a2; ergo_retain_val(__t2386);
  ErgoVal __t2387 = a3; ergo_retain_val(__t2387);
  __cogito_container_set_margins(__t2383, __t2384, __t2385, __t2386, __t2387);
  ergo_release_val(__t2383);
  ergo_release_val(__t2384);
  ergo_release_val(__t2385);
  ergo_release_val(__t2386);
  ergo_release_val(__t2387);
  ErgoVal __t2388 = YV_NULLV;
  ergo_release_val(__t2388);
  ErgoVal __t2389 = self; ergo_retain_val(__t2389);
  ergo_move_into(&__ret, __t2389);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2390 = self; ergo_retain_val(__t2390);
  ErgoVal __t2391 = a0; ergo_retain_val(__t2391);
  ErgoVal __t2392 = a1; ergo_retain_val(__t2392);
  ErgoVal __t2393 = a2; ergo_retain_val(__t2393);
  ErgoVal __t2394 = a3; ergo_retain_val(__t2394);
  __cogito_container_set_padding(__t2390, __t2391, __t2392, __t2393, __t2394);
  ergo_release_val(__t2390);
  ergo_release_val(__t2391);
  ergo_release_val(__t2392);
  ergo_release_val(__t2393);
  ergo_release_val(__t2394);
  ErgoVal __t2395 = YV_NULLV;
  ergo_release_val(__t2395);
  ErgoVal __t2396 = self; ergo_retain_val(__t2396);
  ergo_move_into(&__ret, __t2396);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2397 = self; ergo_retain_val(__t2397);
  ErgoVal __t2398 = a0; ergo_retain_val(__t2398);
  __cogito_container_set_align(__t2397, __t2398);
  ergo_release_val(__t2397);
  ergo_release_val(__t2398);
  ErgoVal __t2399 = YV_NULLV;
  ergo_release_val(__t2399);
  ErgoVal __t2400 = self; ergo_retain_val(__t2400);
  ergo_move_into(&__ret, __t2400);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2401 = self; ergo_retain_val(__t2401);
  ErgoVal __t2402 = a0; ergo_retain_val(__t2402);
  __cogito_container_set_halign(__t2401, __t2402);
  ergo_release_val(__t2401);
  ergo_release_val(__t2402);
  ErgoVal __t2403 = YV_NULLV;
  ergo_release_val(__t2403);
  ErgoVal __t2404 = self; ergo_retain_val(__t2404);
  ergo_move_into(&__ret, __t2404);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2405 = self; ergo_retain_val(__t2405);
  ErgoVal __t2406 = a0; ergo_retain_val(__t2406);
  __cogito_container_set_valign(__t2405, __t2406);
  ergo_release_val(__t2405);
  ergo_release_val(__t2406);
  ErgoVal __t2407 = YV_NULLV;
  ergo_release_val(__t2407);
  ErgoVal __t2408 = self; ergo_retain_val(__t2408);
  ergo_move_into(&__ret, __t2408);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2409 = self; ergo_retain_val(__t2409);
  ErgoVal __t2410 = YV_INT(0);
  __cogito_container_set_align(__t2409, __t2410);
  ergo_release_val(__t2409);
  ergo_release_val(__t2410);
  ErgoVal __t2411 = YV_NULLV;
  ergo_release_val(__t2411);
  ErgoVal __t2412 = self; ergo_retain_val(__t2412);
  ergo_move_into(&__ret, __t2412);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2413 = self; ergo_retain_val(__t2413);
  ErgoVal __t2414 = YV_INT(1);
  __cogito_container_set_align(__t2413, __t2414);
  ergo_release_val(__t2413);
  ergo_release_val(__t2414);
  ErgoVal __t2415 = YV_NULLV;
  ergo_release_val(__t2415);
  ErgoVal __t2416 = self; ergo_retain_val(__t2416);
  ergo_move_into(&__ret, __t2416);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2417 = self; ergo_retain_val(__t2417);
  ErgoVal __t2418 = YV_INT(2);
  __cogito_container_set_align(__t2417, __t2418);
  ergo_release_val(__t2417);
  ergo_release_val(__t2418);
  ErgoVal __t2419 = YV_NULLV;
  ergo_release_val(__t2419);
  ErgoVal __t2420 = self; ergo_retain_val(__t2420);
  ergo_move_into(&__ret, __t2420);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2421 = self; ergo_retain_val(__t2421);
  ErgoVal __t2422 = a0; ergo_retain_val(__t2422);
  __cogito_textview_set_text(__t2421, __t2422);
  ergo_release_val(__t2421);
  ergo_release_val(__t2422);
  ErgoVal __t2423 = YV_NULLV;
  ergo_release_val(__t2423);
  ErgoVal __t2424 = self; ergo_retain_val(__t2424);
  ergo_move_into(&__ret, __t2424);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_text(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2425 = self; ergo_retain_val(__t2425);
  ErgoVal __t2426 = __cogito_textview_get_text(__t2425);
  ergo_release_val(__t2425);
  ergo_move_into(&__ret, __t2426);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2427 = self; ergo_retain_val(__t2427);
  ErgoVal __t2428 = a0; ergo_retain_val(__t2428);
  __cogito_textview_on_change(__t2427, __t2428);
  ergo_release_val(__t2427);
  ergo_release_val(__t2428);
  ErgoVal __t2429 = YV_NULLV;
  ergo_release_val(__t2429);
  ErgoVal __t2430 = self; ergo_retain_val(__t2430);
  ergo_move_into(&__ret, __t2430);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2431 = self; ergo_retain_val(__t2431);
  ErgoVal __t2432 = a0; ergo_retain_val(__t2432);
  __cogito_container_set_hexpand(__t2431, __t2432);
  ergo_release_val(__t2431);
  ergo_release_val(__t2432);
  ErgoVal __t2433 = YV_NULLV;
  ergo_release_val(__t2433);
  ErgoVal __t2434 = self; ergo_retain_val(__t2434);
  ergo_move_into(&__ret, __t2434);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2435 = self; ergo_retain_val(__t2435);
  ErgoVal __t2436 = a0; ergo_retain_val(__t2436);
  __cogito_container_set_vexpand(__t2435, __t2436);
  ergo_release_val(__t2435);
  ergo_release_val(__t2436);
  ErgoVal __t2437 = YV_NULLV;
  ergo_release_val(__t2437);
  ErgoVal __t2438 = self; ergo_retain_val(__t2438);
  ergo_move_into(&__ret, __t2438);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_set_editable(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2439 = self; ergo_retain_val(__t2439);
  ErgoVal __t2440 = a0; ergo_retain_val(__t2440);
  __cogito_node_set_editable(__t2439, __t2440);
  ergo_release_val(__t2439);
  ergo_release_val(__t2440);
  ErgoVal __t2441 = YV_NULLV;
  ergo_release_val(__t2441);
  ErgoVal __t2442 = self; ergo_retain_val(__t2442);
  ergo_move_into(&__ret, __t2442);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_editable(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2443 = self; ergo_retain_val(__t2443);
  ErgoVal __t2444 = __cogito_node_get_editable(__t2443);
  ergo_release_val(__t2443);
  ergo_move_into(&__ret, __t2444);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2445 = self; ergo_retain_val(__t2445);
  ErgoVal __t2446 = a0; ergo_retain_val(__t2446);
  __cogito_node_set_disabled(__t2445, __t2446);
  ergo_release_val(__t2445);
  ergo_release_val(__t2446);
  ErgoVal __t2447 = YV_NULLV;
  ergo_release_val(__t2447);
  ErgoVal __t2448 = self; ergo_retain_val(__t2448);
  ergo_move_into(&__ret, __t2448);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2449 = self; ergo_retain_val(__t2449);
  ErgoVal __t2450 = a0; ergo_retain_val(__t2450);
  __cogito_node_set_class(__t2449, __t2450);
  ergo_release_val(__t2449);
  ergo_release_val(__t2450);
  ErgoVal __t2451 = YV_NULLV;
  ergo_release_val(__t2451);
  ErgoVal __t2452 = self; ergo_retain_val(__t2452);
  ergo_move_into(&__ret, __t2452);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TextView_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2453 = self; ergo_retain_val(__t2453);
  ErgoVal __t2454 = a0; ergo_retain_val(__t2454);
  __cogito_node_set_id(__t2453, __t2454);
  ergo_release_val(__t2453);
  ergo_release_val(__t2454);
  ErgoVal __t2455 = YV_NULLV;
  ergo_release_val(__t2455);
  ErgoVal __t2456 = self; ergo_retain_val(__t2456);
  ergo_move_into(&__ret, __t2456);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2457 = self; ergo_retain_val(__t2457);
  ErgoVal __t2458 = a0; ergo_retain_val(__t2458);
  ErgoVal __t2459 = a1; ergo_retain_val(__t2459);
  ErgoVal __t2460 = a2; ergo_retain_val(__t2460);
  ErgoVal __t2461 = a3; ergo_retain_val(__t2461);
  __cogito_container_set_margins(__t2457, __t2458, __t2459, __t2460, __t2461);
  ergo_release_val(__t2457);
  ergo_release_val(__t2458);
  ergo_release_val(__t2459);
  ergo_release_val(__t2460);
  ergo_release_val(__t2461);
  ErgoVal __t2462 = YV_NULLV;
  ergo_release_val(__t2462);
  ErgoVal __t2463 = self; ergo_retain_val(__t2463);
  ergo_move_into(&__ret, __t2463);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2464 = self; ergo_retain_val(__t2464);
  ErgoVal __t2465 = a0; ergo_retain_val(__t2465);
  ErgoVal __t2466 = a1; ergo_retain_val(__t2466);
  ErgoVal __t2467 = a2; ergo_retain_val(__t2467);
  ErgoVal __t2468 = a3; ergo_retain_val(__t2468);
  __cogito_container_set_padding(__t2464, __t2465, __t2466, __t2467, __t2468);
  ergo_release_val(__t2464);
  ergo_release_val(__t2465);
  ergo_release_val(__t2466);
  ergo_release_val(__t2467);
  ergo_release_val(__t2468);
  ErgoVal __t2469 = YV_NULLV;
  ergo_release_val(__t2469);
  ErgoVal __t2470 = self; ergo_retain_val(__t2470);
  ergo_move_into(&__ret, __t2470);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2471 = self; ergo_retain_val(__t2471);
  ErgoVal __t2472 = a0; ergo_retain_val(__t2472);
  __cogito_container_set_align(__t2471, __t2472);
  ergo_release_val(__t2471);
  ergo_release_val(__t2472);
  ErgoVal __t2473 = YV_NULLV;
  ergo_release_val(__t2473);
  ErgoVal __t2474 = self; ergo_retain_val(__t2474);
  ergo_move_into(&__ret, __t2474);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2475 = self; ergo_retain_val(__t2475);
  ErgoVal __t2476 = a0; ergo_retain_val(__t2476);
  __cogito_container_set_halign(__t2475, __t2476);
  ergo_release_val(__t2475);
  ergo_release_val(__t2476);
  ErgoVal __t2477 = YV_NULLV;
  ergo_release_val(__t2477);
  ErgoVal __t2478 = self; ergo_retain_val(__t2478);
  ergo_move_into(&__ret, __t2478);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2479 = self; ergo_retain_val(__t2479);
  ErgoVal __t2480 = a0; ergo_retain_val(__t2480);
  __cogito_container_set_valign(__t2479, __t2480);
  ergo_release_val(__t2479);
  ergo_release_val(__t2480);
  ErgoVal __t2481 = YV_NULLV;
  ergo_release_val(__t2481);
  ErgoVal __t2482 = self; ergo_retain_val(__t2482);
  ergo_move_into(&__ret, __t2482);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_set_date(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_date(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2483 = self; ergo_retain_val(__t2483);
  ErgoVal __t2484 = a0; ergo_retain_val(__t2484);
  __cogito_datepicker_on_change(__t2483, __t2484);
  ergo_release_val(__t2483);
  ergo_release_val(__t2484);
  ErgoVal __t2485 = YV_NULLV;
  ergo_release_val(__t2485);
  ErgoVal __t2486 = self; ergo_retain_val(__t2486);
  ergo_move_into(&__ret, __t2486);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_set_a11y_label(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2487 = self; ergo_retain_val(__t2487);
  ErgoVal __t2488 = a0; ergo_retain_val(__t2488);
  __cogito_node_set_a11y_label(__t2487, __t2488);
  ergo_release_val(__t2487);
  ergo_release_val(__t2488);
  ErgoVal __t2489 = YV_NULLV;
  ergo_release_val(__t2489);
  ErgoVal __t2490 = self; ergo_retain_val(__t2490);
  ergo_move_into(&__ret, __t2490);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_set_a11y_role(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2491 = self; ergo_retain_val(__t2491);
  ErgoVal __t2492 = a0; ergo_retain_val(__t2492);
  __cogito_node_set_a11y_role(__t2491, __t2492);
  ergo_release_val(__t2491);
  ergo_release_val(__t2492);
  ErgoVal __t2493 = YV_NULLV;
  ergo_release_val(__t2493);
  ErgoVal __t2494 = self; ergo_retain_val(__t2494);
  ergo_move_into(&__ret, __t2494);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2495 = self; ergo_retain_val(__t2495);
  ErgoVal __t2496 = a0; ergo_retain_val(__t2496);
  __cogito_container_set_hexpand(__t2495, __t2496);
  ergo_release_val(__t2495);
  ergo_release_val(__t2496);
  ErgoVal __t2497 = YV_NULLV;
  ergo_release_val(__t2497);
  ErgoVal __t2498 = self; ergo_retain_val(__t2498);
  ergo_move_into(&__ret, __t2498);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2499 = self; ergo_retain_val(__t2499);
  ErgoVal __t2500 = a0; ergo_retain_val(__t2500);
  __cogito_container_set_vexpand(__t2499, __t2500);
  ergo_release_val(__t2499);
  ergo_release_val(__t2500);
  ErgoVal __t2501 = YV_NULLV;
  ergo_release_val(__t2501);
  ErgoVal __t2502 = self; ergo_retain_val(__t2502);
  ergo_move_into(&__ret, __t2502);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2503 = self; ergo_retain_val(__t2503);
  ErgoVal __t2504 = a0; ergo_retain_val(__t2504);
  __cogito_node_set_disabled(__t2503, __t2504);
  ergo_release_val(__t2503);
  ergo_release_val(__t2504);
  ErgoVal __t2505 = YV_NULLV;
  ergo_release_val(__t2505);
  ErgoVal __t2506 = self; ergo_retain_val(__t2506);
  ergo_move_into(&__ret, __t2506);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2507 = self; ergo_retain_val(__t2507);
  ErgoVal __t2508 = a0; ergo_retain_val(__t2508);
  __cogito_node_set_class(__t2507, __t2508);
  ergo_release_val(__t2507);
  ergo_release_val(__t2508);
  ErgoVal __t2509 = YV_NULLV;
  ergo_release_val(__t2509);
  ErgoVal __t2510 = self; ergo_retain_val(__t2510);
  ergo_move_into(&__ret, __t2510);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DatePicker_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2511 = self; ergo_retain_val(__t2511);
  ErgoVal __t2512 = a0; ergo_retain_val(__t2512);
  __cogito_node_set_id(__t2511, __t2512);
  ergo_release_val(__t2511);
  ergo_release_val(__t2512);
  ErgoVal __t2513 = YV_NULLV;
  ergo_release_val(__t2513);
  ErgoVal __t2514 = self; ergo_retain_val(__t2514);
  ergo_move_into(&__ret, __t2514);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Stepper_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2515 = self; ergo_retain_val(__t2515);
  ErgoVal __t2516 = a0; ergo_retain_val(__t2516);
  ErgoVal __t2517 = a1; ergo_retain_val(__t2517);
  ErgoVal __t2518 = a2; ergo_retain_val(__t2518);
  ErgoVal __t2519 = a3; ergo_retain_val(__t2519);
  __cogito_container_set_margins(__t2515, __t2516, __t2517, __t2518, __t2519);
  ergo_release_val(__t2515);
  ergo_release_val(__t2516);
  ergo_release_val(__t2517);
  ergo_release_val(__t2518);
  ergo_release_val(__t2519);
  ErgoVal __t2520 = YV_NULLV;
  ergo_release_val(__t2520);
  ErgoVal __t2521 = self; ergo_retain_val(__t2521);
  ergo_move_into(&__ret, __t2521);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Stepper_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2522 = self; ergo_retain_val(__t2522);
  ErgoVal __t2523 = a0; ergo_retain_val(__t2523);
  ErgoVal __t2524 = a1; ergo_retain_val(__t2524);
  ErgoVal __t2525 = a2; ergo_retain_val(__t2525);
  ErgoVal __t2526 = a3; ergo_retain_val(__t2526);
  __cogito_container_set_padding(__t2522, __t2523, __t2524, __t2525, __t2526);
  ergo_release_val(__t2522);
  ergo_release_val(__t2523);
  ergo_release_val(__t2524);
  ergo_release_val(__t2525);
  ergo_release_val(__t2526);
  ErgoVal __t2527 = YV_NULLV;
  ergo_release_val(__t2527);
  ErgoVal __t2528 = self; ergo_retain_val(__t2528);
  ergo_move_into(&__ret, __t2528);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Stepper_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2529 = self; ergo_retain_val(__t2529);
  ErgoVal __t2530 = a0; ergo_retain_val(__t2530);
  __cogito_container_set_align(__t2529, __t2530);
  ergo_release_val(__t2529);
  ergo_release_val(__t2530);
  ErgoVal __t2531 = YV_NULLV;
  ergo_release_val(__t2531);
  ErgoVal __t2532 = self; ergo_retain_val(__t2532);
  ergo_move_into(&__ret, __t2532);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Stepper_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2533 = self; ergo_retain_val(__t2533);
  ErgoVal __t2534 = a0; ergo_retain_val(__t2534);
  __cogito_container_set_halign(__t2533, __t2534);
  ergo_release_val(__t2533);
  ergo_release_val(__t2534);
  ErgoVal __t2535 = YV_NULLV;
  ergo_release_val(__t2535);
  ErgoVal __t2536 = self; ergo_retain_val(__t2536);
  ergo_move_into(&__ret, __t2536);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Stepper_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2537 = self; ergo_retain_val(__t2537);
  ErgoVal __t2538 = a0; ergo_retain_val(__t2538);
  __cogito_container_set_valign(__t2537, __t2538);
  ergo_release_val(__t2537);
  ergo_release_val(__t2538);
  ErgoVal __t2539 = YV_NULLV;
  ergo_release_val(__t2539);
  ErgoVal __t2540 = self; ergo_retain_val(__t2540);
  ergo_move_into(&__ret, __t2540);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Stepper_set_value(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_m_cogito_Stepper_value(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_m_cogito_Stepper_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_m_cogito_Stepper_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2541 = self; ergo_retain_val(__t2541);
  ErgoVal __t2542 = a0; ergo_retain_val(__t2542);
  __cogito_container_set_hexpand(__t2541, __t2542);
  ergo_release_val(__t2541);
  ergo_release_val(__t2542);
  ErgoVal __t2543 = YV_NULLV;
  ergo_release_val(__t2543);
  ErgoVal __t2544 = self; ergo_retain_val(__t2544);
  ergo_move_into(&__ret, __t2544);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Stepper_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2545 = self; ergo_retain_val(__t2545);
  ErgoVal __t2546 = a0; ergo_retain_val(__t2546);
  __cogito_container_set_vexpand(__t2545, __t2546);
  ergo_release_val(__t2545);
  ergo_release_val(__t2546);
  ErgoVal __t2547 = YV_NULLV;
  ergo_release_val(__t2547);
  ErgoVal __t2548 = self; ergo_retain_val(__t2548);
  ergo_move_into(&__ret, __t2548);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Stepper_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2549 = self; ergo_retain_val(__t2549);
  ErgoVal __t2550 = a0; ergo_retain_val(__t2550);
  __cogito_node_set_class(__t2549, __t2550);
  ergo_release_val(__t2549);
  ergo_release_val(__t2550);
  ErgoVal __t2551 = YV_NULLV;
  ergo_release_val(__t2551);
  ErgoVal __t2552 = self; ergo_retain_val(__t2552);
  ergo_move_into(&__ret, __t2552);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Stepper_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2553 = self; ergo_retain_val(__t2553);
  ErgoVal __t2554 = a0; ergo_retain_val(__t2554);
  __cogito_node_set_id(__t2553, __t2554);
  ergo_release_val(__t2553);
  ergo_release_val(__t2554);
  ErgoVal __t2555 = YV_NULLV;
  ergo_release_val(__t2555);
  ErgoVal __t2556 = self; ergo_retain_val(__t2556);
  ergo_move_into(&__ret, __t2556);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2557 = self; ergo_retain_val(__t2557);
  ErgoVal __t2558 = a0; ergo_retain_val(__t2558);
  ErgoVal __t2559 = a1; ergo_retain_val(__t2559);
  ErgoVal __t2560 = a2; ergo_retain_val(__t2560);
  ErgoVal __t2561 = a3; ergo_retain_val(__t2561);
  __cogito_container_set_margins(__t2557, __t2558, __t2559, __t2560, __t2561);
  ergo_release_val(__t2557);
  ergo_release_val(__t2558);
  ergo_release_val(__t2559);
  ergo_release_val(__t2560);
  ergo_release_val(__t2561);
  ErgoVal __t2562 = YV_NULLV;
  ergo_release_val(__t2562);
  ErgoVal __t2563 = self; ergo_retain_val(__t2563);
  ergo_move_into(&__ret, __t2563);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2564 = self; ergo_retain_val(__t2564);
  ErgoVal __t2565 = a0; ergo_retain_val(__t2565);
  ErgoVal __t2566 = a1; ergo_retain_val(__t2566);
  ErgoVal __t2567 = a2; ergo_retain_val(__t2567);
  ErgoVal __t2568 = a3; ergo_retain_val(__t2568);
  __cogito_container_set_padding(__t2564, __t2565, __t2566, __t2567, __t2568);
  ergo_release_val(__t2564);
  ergo_release_val(__t2565);
  ergo_release_val(__t2566);
  ergo_release_val(__t2567);
  ergo_release_val(__t2568);
  ErgoVal __t2569 = YV_NULLV;
  ergo_release_val(__t2569);
  ErgoVal __t2570 = self; ergo_retain_val(__t2570);
  ergo_move_into(&__ret, __t2570);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2571 = self; ergo_retain_val(__t2571);
  ErgoVal __t2572 = a0; ergo_retain_val(__t2572);
  __cogito_container_set_align(__t2571, __t2572);
  ergo_release_val(__t2571);
  ergo_release_val(__t2572);
  ErgoVal __t2573 = YV_NULLV;
  ergo_release_val(__t2573);
  ErgoVal __t2574 = self; ergo_retain_val(__t2574);
  ergo_move_into(&__ret, __t2574);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2575 = self; ergo_retain_val(__t2575);
  ErgoVal __t2576 = a0; ergo_retain_val(__t2576);
  __cogito_container_set_halign(__t2575, __t2576);
  ergo_release_val(__t2575);
  ergo_release_val(__t2576);
  ErgoVal __t2577 = YV_NULLV;
  ergo_release_val(__t2577);
  ErgoVal __t2578 = self; ergo_retain_val(__t2578);
  ergo_move_into(&__ret, __t2578);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2579 = self; ergo_retain_val(__t2579);
  ErgoVal __t2580 = a0; ergo_retain_val(__t2580);
  __cogito_container_set_valign(__t2579, __t2580);
  ergo_release_val(__t2579);
  ergo_release_val(__t2580);
  ErgoVal __t2581 = YV_NULLV;
  ergo_release_val(__t2581);
  ErgoVal __t2582 = self; ergo_retain_val(__t2582);
  ergo_move_into(&__ret, __t2582);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2583 = self; ergo_retain_val(__t2583);
  ErgoVal __t2584 = YV_INT(0);
  __cogito_container_set_align(__t2583, __t2584);
  ergo_release_val(__t2583);
  ergo_release_val(__t2584);
  ErgoVal __t2585 = YV_NULLV;
  ergo_release_val(__t2585);
  ErgoVal __t2586 = self; ergo_retain_val(__t2586);
  ergo_move_into(&__ret, __t2586);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2587 = self; ergo_retain_val(__t2587);
  ErgoVal __t2588 = YV_INT(1);
  __cogito_container_set_align(__t2587, __t2588);
  ergo_release_val(__t2587);
  ergo_release_val(__t2588);
  ErgoVal __t2589 = YV_NULLV;
  ergo_release_val(__t2589);
  ErgoVal __t2590 = self; ergo_retain_val(__t2590);
  ergo_move_into(&__ret, __t2590);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2591 = self; ergo_retain_val(__t2591);
  ErgoVal __t2592 = YV_INT(2);
  __cogito_container_set_align(__t2591, __t2592);
  ergo_release_val(__t2591);
  ergo_release_val(__t2592);
  ErgoVal __t2593 = YV_NULLV;
  ergo_release_val(__t2593);
  ErgoVal __t2594 = self; ergo_retain_val(__t2594);
  ergo_move_into(&__ret, __t2594);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_set_items(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2595 = self; ergo_retain_val(__t2595);
  ErgoVal __t2596 = a0; ergo_retain_val(__t2596);
  __cogito_dropdown_set_items(__t2595, __t2596);
  ergo_release_val(__t2595);
  ergo_release_val(__t2596);
  ErgoVal __t2597 = YV_NULLV;
  ergo_release_val(__t2597);
  ErgoVal __t2598 = self; ergo_retain_val(__t2598);
  ergo_move_into(&__ret, __t2598);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_set_selected(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2599 = self; ergo_retain_val(__t2599);
  ErgoVal __t2600 = a0; ergo_retain_val(__t2600);
  __cogito_dropdown_set_selected(__t2599, __t2600);
  ergo_release_val(__t2599);
  ergo_release_val(__t2600);
  ErgoVal __t2601 = YV_NULLV;
  ergo_release_val(__t2601);
  ErgoVal __t2602 = self; ergo_retain_val(__t2602);
  ergo_move_into(&__ret, __t2602);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_selected(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2603 = self; ergo_retain_val(__t2603);
  ErgoVal __t2604 = __cogito_dropdown_get_selected(__t2603);
  ergo_release_val(__t2603);
  ergo_move_into(&__ret, __t2604);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2605 = self; ergo_retain_val(__t2605);
  ErgoVal __t2606 = a0; ergo_retain_val(__t2606);
  __cogito_dropdown_on_change(__t2605, __t2606);
  ergo_release_val(__t2605);
  ergo_release_val(__t2606);
  ErgoVal __t2607 = YV_NULLV;
  ergo_release_val(__t2607);
  ErgoVal __t2608 = self; ergo_retain_val(__t2608);
  ergo_move_into(&__ret, __t2608);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2609 = self; ergo_retain_val(__t2609);
  ErgoVal __t2610 = a0; ergo_retain_val(__t2610);
  __cogito_container_set_hexpand(__t2609, __t2610);
  ergo_release_val(__t2609);
  ergo_release_val(__t2610);
  ErgoVal __t2611 = YV_NULLV;
  ergo_release_val(__t2611);
  ErgoVal __t2612 = self; ergo_retain_val(__t2612);
  ergo_move_into(&__ret, __t2612);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2613 = self; ergo_retain_val(__t2613);
  ErgoVal __t2614 = a0; ergo_retain_val(__t2614);
  __cogito_container_set_vexpand(__t2613, __t2614);
  ergo_release_val(__t2613);
  ergo_release_val(__t2614);
  ErgoVal __t2615 = YV_NULLV;
  ergo_release_val(__t2615);
  ErgoVal __t2616 = self; ergo_retain_val(__t2616);
  ergo_move_into(&__ret, __t2616);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2617 = self; ergo_retain_val(__t2617);
  ErgoVal __t2618 = a0; ergo_retain_val(__t2618);
  __cogito_node_set_disabled(__t2617, __t2618);
  ergo_release_val(__t2617);
  ergo_release_val(__t2618);
  ErgoVal __t2619 = YV_NULLV;
  ergo_release_val(__t2619);
  ErgoVal __t2620 = self; ergo_retain_val(__t2620);
  ergo_move_into(&__ret, __t2620);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2621 = self; ergo_retain_val(__t2621);
  ErgoVal __t2622 = a0; ergo_retain_val(__t2622);
  __cogito_node_set_class(__t2621, __t2622);
  ergo_release_val(__t2621);
  ergo_release_val(__t2622);
  ErgoVal __t2623 = YV_NULLV;
  ergo_release_val(__t2623);
  ErgoVal __t2624 = self; ergo_retain_val(__t2624);
  ergo_move_into(&__ret, __t2624);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dropdown_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2625 = self; ergo_retain_val(__t2625);
  ErgoVal __t2626 = a0; ergo_retain_val(__t2626);
  __cogito_node_set_id(__t2625, __t2626);
  ergo_release_val(__t2625);
  ergo_release_val(__t2626);
  ErgoVal __t2627 = YV_NULLV;
  ergo_release_val(__t2627);
  ErgoVal __t2628 = self; ergo_retain_val(__t2628);
  ergo_move_into(&__ret, __t2628);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2629 = self; ergo_retain_val(__t2629);
  ErgoVal __t2630 = a0; ergo_retain_val(__t2630);
  ErgoVal __t2631 = a1; ergo_retain_val(__t2631);
  ErgoVal __t2632 = a2; ergo_retain_val(__t2632);
  ErgoVal __t2633 = a3; ergo_retain_val(__t2633);
  __cogito_container_set_margins(__t2629, __t2630, __t2631, __t2632, __t2633);
  ergo_release_val(__t2629);
  ergo_release_val(__t2630);
  ergo_release_val(__t2631);
  ergo_release_val(__t2632);
  ergo_release_val(__t2633);
  ErgoVal __t2634 = YV_NULLV;
  ergo_release_val(__t2634);
  ErgoVal __t2635 = self; ergo_retain_val(__t2635);
  ergo_move_into(&__ret, __t2635);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2636 = self; ergo_retain_val(__t2636);
  ErgoVal __t2637 = a0; ergo_retain_val(__t2637);
  ErgoVal __t2638 = a1; ergo_retain_val(__t2638);
  ErgoVal __t2639 = a2; ergo_retain_val(__t2639);
  ErgoVal __t2640 = a3; ergo_retain_val(__t2640);
  __cogito_container_set_padding(__t2636, __t2637, __t2638, __t2639, __t2640);
  ergo_release_val(__t2636);
  ergo_release_val(__t2637);
  ergo_release_val(__t2638);
  ergo_release_val(__t2639);
  ergo_release_val(__t2640);
  ErgoVal __t2641 = YV_NULLV;
  ergo_release_val(__t2641);
  ErgoVal __t2642 = self; ergo_retain_val(__t2642);
  ergo_move_into(&__ret, __t2642);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2643 = self; ergo_retain_val(__t2643);
  ErgoVal __t2644 = a0; ergo_retain_val(__t2644);
  __cogito_container_set_align(__t2643, __t2644);
  ergo_release_val(__t2643);
  ergo_release_val(__t2644);
  ErgoVal __t2645 = YV_NULLV;
  ergo_release_val(__t2645);
  ErgoVal __t2646 = self; ergo_retain_val(__t2646);
  ergo_move_into(&__ret, __t2646);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2647 = self; ergo_retain_val(__t2647);
  ErgoVal __t2648 = a0; ergo_retain_val(__t2648);
  __cogito_container_set_halign(__t2647, __t2648);
  ergo_release_val(__t2647);
  ergo_release_val(__t2648);
  ErgoVal __t2649 = YV_NULLV;
  ergo_release_val(__t2649);
  ErgoVal __t2650 = self; ergo_retain_val(__t2650);
  ergo_move_into(&__ret, __t2650);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2651 = self; ergo_retain_val(__t2651);
  ErgoVal __t2652 = a0; ergo_retain_val(__t2652);
  __cogito_container_set_valign(__t2651, __t2652);
  ergo_release_val(__t2651);
  ergo_release_val(__t2652);
  ErgoVal __t2653 = YV_NULLV;
  ergo_release_val(__t2653);
  ErgoVal __t2654 = self; ergo_retain_val(__t2654);
  ergo_move_into(&__ret, __t2654);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2655 = self; ergo_retain_val(__t2655);
  ErgoVal __t2656 = YV_INT(0);
  __cogito_container_set_align(__t2655, __t2656);
  ergo_release_val(__t2655);
  ergo_release_val(__t2656);
  ErgoVal __t2657 = YV_NULLV;
  ergo_release_val(__t2657);
  ErgoVal __t2658 = self; ergo_retain_val(__t2658);
  ergo_move_into(&__ret, __t2658);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2659 = self; ergo_retain_val(__t2659);
  ErgoVal __t2660 = YV_INT(1);
  __cogito_container_set_align(__t2659, __t2660);
  ergo_release_val(__t2659);
  ergo_release_val(__t2660);
  ErgoVal __t2661 = YV_NULLV;
  ergo_release_val(__t2661);
  ErgoVal __t2662 = self; ergo_retain_val(__t2662);
  ergo_move_into(&__ret, __t2662);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2663 = self; ergo_retain_val(__t2663);
  ErgoVal __t2664 = YV_INT(2);
  __cogito_container_set_align(__t2663, __t2664);
  ergo_release_val(__t2663);
  ergo_release_val(__t2664);
  ErgoVal __t2665 = YV_NULLV;
  ergo_release_val(__t2665);
  ErgoVal __t2666 = self; ergo_retain_val(__t2666);
  ergo_move_into(&__ret, __t2666);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_value(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2667 = self; ergo_retain_val(__t2667);
  ErgoVal __t2668 = a0; ergo_retain_val(__t2668);
  __cogito_slider_set_value(__t2667, __t2668);
  ergo_release_val(__t2667);
  ergo_release_val(__t2668);
  ErgoVal __t2669 = YV_NULLV;
  ergo_release_val(__t2669);
  ErgoVal __t2670 = self; ergo_retain_val(__t2670);
  ergo_move_into(&__ret, __t2670);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_value(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2671 = self; ergo_retain_val(__t2671);
  ErgoVal __t2672 = __cogito_slider_get_value(__t2671);
  ergo_release_val(__t2671);
  ergo_move_into(&__ret, __t2672);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_centered(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2673 = self; ergo_retain_val(__t2673);
  ErgoVal __t2674 = a0; ergo_retain_val(__t2674);
  __cogito_slider_set_centered(__t2673, __t2674);
  ergo_release_val(__t2673);
  ergo_release_val(__t2674);
  ErgoVal __t2675 = YV_NULLV;
  ergo_release_val(__t2675);
  ErgoVal __t2676 = self; ergo_retain_val(__t2676);
  ergo_move_into(&__ret, __t2676);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_centered(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2677 = self; ergo_retain_val(__t2677);
  ErgoVal __t2678 = __cogito_slider_get_centered(__t2677);
  ergo_release_val(__t2677);
  ergo_move_into(&__ret, __t2678);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_range(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2679 = self; ergo_retain_val(__t2679);
  ErgoVal __t2680 = a0; ergo_retain_val(__t2680);
  ErgoVal __t2681 = a1; ergo_retain_val(__t2681);
  __cogito_slider_set_range(__t2679, __t2680, __t2681);
  ergo_release_val(__t2679);
  ergo_release_val(__t2680);
  ergo_release_val(__t2681);
  ErgoVal __t2682 = YV_NULLV;
  ergo_release_val(__t2682);
  ErgoVal __t2683 = self; ergo_retain_val(__t2683);
  ergo_move_into(&__ret, __t2683);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_range_start(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2684 = self; ergo_retain_val(__t2684);
  ErgoVal __t2685 = a0; ergo_retain_val(__t2685);
  __cogito_slider_set_range_start(__t2684, __t2685);
  ergo_release_val(__t2684);
  ergo_release_val(__t2685);
  ErgoVal __t2686 = YV_NULLV;
  ergo_release_val(__t2686);
  ErgoVal __t2687 = self; ergo_retain_val(__t2687);
  ergo_move_into(&__ret, __t2687);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_range_end(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2688 = self; ergo_retain_val(__t2688);
  ErgoVal __t2689 = a0; ergo_retain_val(__t2689);
  __cogito_slider_set_range_end(__t2688, __t2689);
  ergo_release_val(__t2688);
  ergo_release_val(__t2689);
  ErgoVal __t2690 = YV_NULLV;
  ergo_release_val(__t2690);
  ErgoVal __t2691 = self; ergo_retain_val(__t2691);
  ergo_move_into(&__ret, __t2691);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_range_start(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2692 = self; ergo_retain_val(__t2692);
  ErgoVal __t2693 = __cogito_slider_get_range_start(__t2692);
  ergo_release_val(__t2692);
  ergo_move_into(&__ret, __t2693);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_range_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2694 = self; ergo_retain_val(__t2694);
  ErgoVal __t2695 = __cogito_slider_get_range_end(__t2694);
  ergo_release_val(__t2694);
  ergo_move_into(&__ret, __t2695);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_size(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2696 = self; ergo_retain_val(__t2696);
  ErgoVal __t2697 = a0; ergo_retain_val(__t2697);
  __cogito_slider_set_size(__t2696, __t2697);
  ergo_release_val(__t2696);
  ergo_release_val(__t2697);
  ErgoVal __t2698 = YV_NULLV;
  ergo_release_val(__t2698);
  ErgoVal __t2699 = self; ergo_retain_val(__t2699);
  ergo_move_into(&__ret, __t2699);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_size(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2700 = self; ergo_retain_val(__t2700);
  ErgoVal __t2701 = __cogito_slider_get_size(__t2700);
  ergo_release_val(__t2700);
  ergo_move_into(&__ret, __t2701);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_xs(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2702 = self; ergo_retain_val(__t2702);
  ErgoVal __t2703 = YV_INT(0);
  __cogito_slider_set_size(__t2702, __t2703);
  ergo_release_val(__t2702);
  ergo_release_val(__t2703);
  ErgoVal __t2704 = YV_NULLV;
  ergo_release_val(__t2704);
  ErgoVal __t2705 = self; ergo_retain_val(__t2705);
  ergo_move_into(&__ret, __t2705);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_s(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2706 = self; ergo_retain_val(__t2706);
  ErgoVal __t2707 = YV_INT(1);
  __cogito_slider_set_size(__t2706, __t2707);
  ergo_release_val(__t2706);
  ergo_release_val(__t2707);
  ErgoVal __t2708 = YV_NULLV;
  ergo_release_val(__t2708);
  ErgoVal __t2709 = self; ergo_retain_val(__t2709);
  ergo_move_into(&__ret, __t2709);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_m(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2710 = self; ergo_retain_val(__t2710);
  ErgoVal __t2711 = YV_INT(2);
  __cogito_slider_set_size(__t2710, __t2711);
  ergo_release_val(__t2710);
  ergo_release_val(__t2711);
  ErgoVal __t2712 = YV_NULLV;
  ergo_release_val(__t2712);
  ErgoVal __t2713 = self; ergo_retain_val(__t2713);
  ergo_move_into(&__ret, __t2713);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_l(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2714 = self; ergo_retain_val(__t2714);
  ErgoVal __t2715 = YV_INT(3);
  __cogito_slider_set_size(__t2714, __t2715);
  ergo_release_val(__t2714);
  ergo_release_val(__t2715);
  ErgoVal __t2716 = YV_NULLV;
  ergo_release_val(__t2716);
  ErgoVal __t2717 = self; ergo_retain_val(__t2717);
  ergo_move_into(&__ret, __t2717);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_xl(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2718 = self; ergo_retain_val(__t2718);
  ErgoVal __t2719 = YV_INT(4);
  __cogito_slider_set_size(__t2718, __t2719);
  ergo_release_val(__t2718);
  ergo_release_val(__t2719);
  ErgoVal __t2720 = YV_NULLV;
  ergo_release_val(__t2720);
  ErgoVal __t2721 = self; ergo_retain_val(__t2721);
  ergo_move_into(&__ret, __t2721);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_icon(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2722 = self; ergo_retain_val(__t2722);
  ErgoVal __t2723 = a0; ergo_retain_val(__t2723);
  __cogito_slider_set_icon(__t2722, __t2723);
  ergo_release_val(__t2722);
  ergo_release_val(__t2723);
  ErgoVal __t2724 = YV_NULLV;
  ergo_release_val(__t2724);
  ErgoVal __t2725 = self; ergo_retain_val(__t2725);
  ergo_move_into(&__ret, __t2725);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2726 = self; ergo_retain_val(__t2726);
  ErgoVal __t2727 = a0; ergo_retain_val(__t2727);
  __cogito_slider_on_change(__t2726, __t2727);
  ergo_release_val(__t2726);
  ergo_release_val(__t2727);
  ErgoVal __t2728 = YV_NULLV;
  ergo_release_val(__t2728);
  ErgoVal __t2729 = self; ergo_retain_val(__t2729);
  ergo_move_into(&__ret, __t2729);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_on_select(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2730 = self; ergo_retain_val(__t2730);
  ErgoVal __t2731 = a0; ergo_retain_val(__t2731);
  __cogito_slider_on_change(__t2730, __t2731);
  ergo_release_val(__t2730);
  ergo_release_val(__t2731);
  ErgoVal __t2732 = YV_NULLV;
  ergo_release_val(__t2732);
  ErgoVal __t2733 = self; ergo_retain_val(__t2733);
  ergo_move_into(&__ret, __t2733);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2734 = self; ergo_retain_val(__t2734);
  ErgoVal __t2735 = a0; ergo_retain_val(__t2735);
  __cogito_container_set_hexpand(__t2734, __t2735);
  ergo_release_val(__t2734);
  ergo_release_val(__t2735);
  ErgoVal __t2736 = YV_NULLV;
  ergo_release_val(__t2736);
  ErgoVal __t2737 = self; ergo_retain_val(__t2737);
  ergo_move_into(&__ret, __t2737);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2738 = self; ergo_retain_val(__t2738);
  ErgoVal __t2739 = a0; ergo_retain_val(__t2739);
  __cogito_container_set_vexpand(__t2738, __t2739);
  ergo_release_val(__t2738);
  ergo_release_val(__t2739);
  ErgoVal __t2740 = YV_NULLV;
  ergo_release_val(__t2740);
  ErgoVal __t2741 = self; ergo_retain_val(__t2741);
  ergo_move_into(&__ret, __t2741);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2742 = self; ergo_retain_val(__t2742);
  ErgoVal __t2743 = a0; ergo_retain_val(__t2743);
  __cogito_node_set_disabled(__t2742, __t2743);
  ergo_release_val(__t2742);
  ergo_release_val(__t2743);
  ErgoVal __t2744 = YV_NULLV;
  ergo_release_val(__t2744);
  ErgoVal __t2745 = self; ergo_retain_val(__t2745);
  ergo_move_into(&__ret, __t2745);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2746 = self; ergo_retain_val(__t2746);
  ErgoVal __t2747 = a0; ergo_retain_val(__t2747);
  __cogito_node_set_class(__t2746, __t2747);
  ergo_release_val(__t2746);
  ergo_release_val(__t2747);
  ErgoVal __t2748 = YV_NULLV;
  ergo_release_val(__t2748);
  ErgoVal __t2749 = self; ergo_retain_val(__t2749);
  ergo_move_into(&__ret, __t2749);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Slider_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2750 = self; ergo_retain_val(__t2750);
  ErgoVal __t2751 = a0; ergo_retain_val(__t2751);
  __cogito_node_set_id(__t2750, __t2751);
  ergo_release_val(__t2750);
  ergo_release_val(__t2751);
  ErgoVal __t2752 = YV_NULLV;
  ergo_release_val(__t2752);
  ErgoVal __t2753 = self; ergo_retain_val(__t2753);
  ergo_move_into(&__ret, __t2753);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2754 = self; ergo_retain_val(__t2754);
  ErgoVal __t2755 = a0; ergo_retain_val(__t2755);
  ErgoVal __t2756 = a1; ergo_retain_val(__t2756);
  ErgoVal __t2757 = a2; ergo_retain_val(__t2757);
  ErgoVal __t2758 = a3; ergo_retain_val(__t2758);
  __cogito_container_set_margins(__t2754, __t2755, __t2756, __t2757, __t2758);
  ergo_release_val(__t2754);
  ergo_release_val(__t2755);
  ergo_release_val(__t2756);
  ergo_release_val(__t2757);
  ergo_release_val(__t2758);
  ErgoVal __t2759 = YV_NULLV;
  ergo_release_val(__t2759);
  ErgoVal __t2760 = self; ergo_retain_val(__t2760);
  ergo_move_into(&__ret, __t2760);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2761 = self; ergo_retain_val(__t2761);
  ErgoVal __t2762 = a0; ergo_retain_val(__t2762);
  ErgoVal __t2763 = a1; ergo_retain_val(__t2763);
  ErgoVal __t2764 = a2; ergo_retain_val(__t2764);
  ErgoVal __t2765 = a3; ergo_retain_val(__t2765);
  __cogito_container_set_padding(__t2761, __t2762, __t2763, __t2764, __t2765);
  ergo_release_val(__t2761);
  ergo_release_val(__t2762);
  ergo_release_val(__t2763);
  ergo_release_val(__t2764);
  ergo_release_val(__t2765);
  ErgoVal __t2766 = YV_NULLV;
  ergo_release_val(__t2766);
  ErgoVal __t2767 = self; ergo_retain_val(__t2767);
  ergo_move_into(&__ret, __t2767);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2768 = self; ergo_retain_val(__t2768);
  ErgoVal __t2769 = a0; ergo_retain_val(__t2769);
  __cogito_container_set_align(__t2768, __t2769);
  ergo_release_val(__t2768);
  ergo_release_val(__t2769);
  ErgoVal __t2770 = YV_NULLV;
  ergo_release_val(__t2770);
  ErgoVal __t2771 = self; ergo_retain_val(__t2771);
  ergo_move_into(&__ret, __t2771);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2772 = self; ergo_retain_val(__t2772);
  ErgoVal __t2773 = a0; ergo_retain_val(__t2773);
  __cogito_container_set_halign(__t2772, __t2773);
  ergo_release_val(__t2772);
  ergo_release_val(__t2773);
  ErgoVal __t2774 = YV_NULLV;
  ergo_release_val(__t2774);
  ErgoVal __t2775 = self; ergo_retain_val(__t2775);
  ergo_move_into(&__ret, __t2775);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2776 = self; ergo_retain_val(__t2776);
  ErgoVal __t2777 = a0; ergo_retain_val(__t2777);
  __cogito_container_set_valign(__t2776, __t2777);
  ergo_release_val(__t2776);
  ergo_release_val(__t2777);
  ErgoVal __t2778 = YV_NULLV;
  ergo_release_val(__t2778);
  ErgoVal __t2779 = self; ergo_retain_val(__t2779);
  ergo_move_into(&__ret, __t2779);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_set_items(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2780 = self; ergo_retain_val(__t2780);
  ErgoVal __t2781 = a0; ergo_retain_val(__t2781);
  __cogito_tabs_set_items(__t2780, __t2781);
  ergo_release_val(__t2780);
  ergo_release_val(__t2781);
  ErgoVal __t2782 = YV_NULLV;
  ergo_release_val(__t2782);
  ErgoVal __t2783 = self; ergo_retain_val(__t2783);
  ergo_move_into(&__ret, __t2783);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_set_ids(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2784 = self; ergo_retain_val(__t2784);
  ErgoVal __t2785 = a0; ergo_retain_val(__t2785);
  __cogito_tabs_set_ids(__t2784, __t2785);
  ergo_release_val(__t2784);
  ergo_release_val(__t2785);
  ErgoVal __t2786 = YV_NULLV;
  ergo_release_val(__t2786);
  ErgoVal __t2787 = self; ergo_retain_val(__t2787);
  ergo_move_into(&__ret, __t2787);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_set_selected(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2788 = self; ergo_retain_val(__t2788);
  ErgoVal __t2789 = a0; ergo_retain_val(__t2789);
  __cogito_tabs_set_selected(__t2788, __t2789);
  ergo_release_val(__t2788);
  ergo_release_val(__t2789);
  ErgoVal __t2790 = YV_NULLV;
  ergo_release_val(__t2790);
  ErgoVal __t2791 = self; ergo_retain_val(__t2791);
  ergo_move_into(&__ret, __t2791);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_selected(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2792 = self; ergo_retain_val(__t2792);
  ErgoVal __t2793 = __cogito_tabs_get_selected(__t2792);
  ergo_release_val(__t2792);
  ergo_move_into(&__ret, __t2793);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2794 = self; ergo_retain_val(__t2794);
  ErgoVal __t2795 = a0; ergo_retain_val(__t2795);
  __cogito_tabs_on_change(__t2794, __t2795);
  ergo_release_val(__t2794);
  ergo_release_val(__t2795);
  ErgoVal __t2796 = YV_NULLV;
  ergo_release_val(__t2796);
  ErgoVal __t2797 = self; ergo_retain_val(__t2797);
  ergo_move_into(&__ret, __t2797);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_bind(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2798 = self; ergo_retain_val(__t2798);
  ErgoVal __t2799 = a0; ergo_retain_val(__t2799);
  __cogito_tabs_bind(__t2798, __t2799);
  ergo_release_val(__t2798);
  ergo_release_val(__t2799);
  ErgoVal __t2800 = YV_NULLV;
  ergo_release_val(__t2800);
  ErgoVal __t2801 = self; ergo_retain_val(__t2801);
  ergo_move_into(&__ret, __t2801);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2802 = self; ergo_retain_val(__t2802);
  ErgoVal __t2803 = a0; ergo_retain_val(__t2803);
  __cogito_container_set_hexpand(__t2802, __t2803);
  ergo_release_val(__t2802);
  ergo_release_val(__t2803);
  ErgoVal __t2804 = YV_NULLV;
  ergo_release_val(__t2804);
  ErgoVal __t2805 = self; ergo_retain_val(__t2805);
  ergo_move_into(&__ret, __t2805);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2806 = self; ergo_retain_val(__t2806);
  ErgoVal __t2807 = a0; ergo_retain_val(__t2807);
  __cogito_container_set_vexpand(__t2806, __t2807);
  ergo_release_val(__t2806);
  ergo_release_val(__t2807);
  ErgoVal __t2808 = YV_NULLV;
  ergo_release_val(__t2808);
  ErgoVal __t2809 = self; ergo_retain_val(__t2809);
  ergo_move_into(&__ret, __t2809);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2810 = self; ergo_retain_val(__t2810);
  ErgoVal __t2811 = a0; ergo_retain_val(__t2811);
  __cogito_node_set_disabled(__t2810, __t2811);
  ergo_release_val(__t2810);
  ergo_release_val(__t2811);
  ErgoVal __t2812 = YV_NULLV;
  ergo_release_val(__t2812);
  ErgoVal __t2813 = self; ergo_retain_val(__t2813);
  ergo_move_into(&__ret, __t2813);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2814 = self; ergo_retain_val(__t2814);
  ErgoVal __t2815 = a0; ergo_retain_val(__t2815);
  __cogito_node_set_class(__t2814, __t2815);
  ergo_release_val(__t2814);
  ergo_release_val(__t2815);
  ErgoVal __t2816 = YV_NULLV;
  ergo_release_val(__t2816);
  ErgoVal __t2817 = self; ergo_retain_val(__t2817);
  ergo_move_into(&__ret, __t2817);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Tabs_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2818 = self; ergo_retain_val(__t2818);
  ErgoVal __t2819 = a0; ergo_retain_val(__t2819);
  __cogito_node_set_id(__t2818, __t2819);
  ergo_release_val(__t2818);
  ergo_release_val(__t2819);
  ErgoVal __t2820 = YV_NULLV;
  ergo_release_val(__t2820);
  ErgoVal __t2821 = self; ergo_retain_val(__t2821);
  ergo_move_into(&__ret, __t2821);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2822 = self; ergo_retain_val(__t2822);
  ErgoVal __t2823 = a0; ergo_retain_val(__t2823);
  __cogito_container_add(__t2822, __t2823);
  ergo_release_val(__t2822);
  ergo_release_val(__t2823);
  ErgoVal __t2824 = YV_NULLV;
  ergo_release_val(__t2824);
  ErgoVal __t2825 = self; ergo_retain_val(__t2825);
  ergo_move_into(&__ret, __t2825);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2826 = self; ergo_retain_val(__t2826);
  ErgoVal __t2827 = a0; ergo_retain_val(__t2827);
  ErgoVal __t2828 = a1; ergo_retain_val(__t2828);
  ErgoVal __t2829 = a2; ergo_retain_val(__t2829);
  ErgoVal __t2830 = a3; ergo_retain_val(__t2830);
  __cogito_container_set_margins(__t2826, __t2827, __t2828, __t2829, __t2830);
  ergo_release_val(__t2826);
  ergo_release_val(__t2827);
  ergo_release_val(__t2828);
  ergo_release_val(__t2829);
  ergo_release_val(__t2830);
  ErgoVal __t2831 = YV_NULLV;
  ergo_release_val(__t2831);
  ErgoVal __t2832 = self; ergo_retain_val(__t2832);
  ergo_move_into(&__ret, __t2832);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2833 = self; ergo_retain_val(__t2833);
  ErgoVal __t2834 = a0; ergo_retain_val(__t2834);
  ErgoVal __t2835 = a1; ergo_retain_val(__t2835);
  ErgoVal __t2836 = a2; ergo_retain_val(__t2836);
  ErgoVal __t2837 = a3; ergo_retain_val(__t2837);
  __cogito_container_set_padding(__t2833, __t2834, __t2835, __t2836, __t2837);
  ergo_release_val(__t2833);
  ergo_release_val(__t2834);
  ergo_release_val(__t2835);
  ergo_release_val(__t2836);
  ergo_release_val(__t2837);
  ErgoVal __t2838 = YV_NULLV;
  ergo_release_val(__t2838);
  ErgoVal __t2839 = self; ergo_retain_val(__t2839);
  ergo_move_into(&__ret, __t2839);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2840 = self; ergo_retain_val(__t2840);
  ErgoVal __t2841 = a0; ergo_retain_val(__t2841);
  __cogito_container_set_align(__t2840, __t2841);
  ergo_release_val(__t2840);
  ergo_release_val(__t2841);
  ErgoVal __t2842 = YV_NULLV;
  ergo_release_val(__t2842);
  ErgoVal __t2843 = self; ergo_retain_val(__t2843);
  ergo_move_into(&__ret, __t2843);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2844 = self; ergo_retain_val(__t2844);
  ErgoVal __t2845 = a0; ergo_retain_val(__t2845);
  __cogito_container_set_halign(__t2844, __t2845);
  ergo_release_val(__t2844);
  ergo_release_val(__t2845);
  ErgoVal __t2846 = YV_NULLV;
  ergo_release_val(__t2846);
  ErgoVal __t2847 = self; ergo_retain_val(__t2847);
  ergo_move_into(&__ret, __t2847);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2848 = self; ergo_retain_val(__t2848);
  ErgoVal __t2849 = a0; ergo_retain_val(__t2849);
  __cogito_container_set_valign(__t2848, __t2849);
  ergo_release_val(__t2848);
  ergo_release_val(__t2849);
  ErgoVal __t2850 = YV_NULLV;
  ergo_release_val(__t2850);
  ErgoVal __t2851 = self; ergo_retain_val(__t2851);
  ergo_move_into(&__ret, __t2851);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_size(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2852 = self; ergo_retain_val(__t2852);
  ErgoVal __t2853 = a0; ergo_retain_val(__t2853);
  __cogito_buttongroup_set_size(__t2852, __t2853);
  ergo_release_val(__t2852);
  ergo_release_val(__t2853);
  ErgoVal __t2854 = YV_NULLV;
  ergo_release_val(__t2854);
  ErgoVal __t2855 = self; ergo_retain_val(__t2855);
  ergo_move_into(&__ret, __t2855);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_shape(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2856 = self; ergo_retain_val(__t2856);
  ErgoVal __t2857 = a0; ergo_retain_val(__t2857);
  __cogito_buttongroup_set_shape(__t2856, __t2857);
  ergo_release_val(__t2856);
  ergo_release_val(__t2857);
  ErgoVal __t2858 = YV_NULLV;
  ergo_release_val(__t2858);
  ErgoVal __t2859 = self; ergo_retain_val(__t2859);
  ergo_move_into(&__ret, __t2859);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_connected(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2860 = self; ergo_retain_val(__t2860);
  ErgoVal __t2861 = a0; ergo_retain_val(__t2861);
  __cogito_buttongroup_set_connected(__t2860, __t2861);
  ergo_release_val(__t2860);
  ergo_release_val(__t2861);
  ErgoVal __t2862 = YV_NULLV;
  ergo_release_val(__t2862);
  ErgoVal __t2863 = self; ergo_retain_val(__t2863);
  ergo_move_into(&__ret, __t2863);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_items(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_selected(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_selected(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_on_select(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2864 = self; ergo_retain_val(__t2864);
  ErgoVal __t2865 = a0; ergo_retain_val(__t2865);
  __cogito_buttongroup_on_select(__t2864, __t2865);
  ergo_release_val(__t2864);
  ergo_release_val(__t2865);
  ErgoVal __t2866 = YV_NULLV;
  ergo_release_val(__t2866);
  ErgoVal __t2867 = self; ergo_retain_val(__t2867);
  ergo_move_into(&__ret, __t2867);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2868 = self; ergo_retain_val(__t2868);
  ErgoVal __t2869 = a0; ergo_retain_val(__t2869);
  __cogito_container_set_hexpand(__t2868, __t2869);
  ergo_release_val(__t2868);
  ergo_release_val(__t2869);
  ErgoVal __t2870 = YV_NULLV;
  ergo_release_val(__t2870);
  ErgoVal __t2871 = self; ergo_retain_val(__t2871);
  ergo_move_into(&__ret, __t2871);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2872 = self; ergo_retain_val(__t2872);
  ErgoVal __t2873 = a0; ergo_retain_val(__t2873);
  __cogito_container_set_vexpand(__t2872, __t2873);
  ergo_release_val(__t2872);
  ergo_release_val(__t2873);
  ErgoVal __t2874 = YV_NULLV;
  ergo_release_val(__t2874);
  ErgoVal __t2875 = self; ergo_retain_val(__t2875);
  ergo_move_into(&__ret, __t2875);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2876 = self; ergo_retain_val(__t2876);
  ErgoVal __t2877 = a0; ergo_retain_val(__t2877);
  __cogito_node_set_class(__t2876, __t2877);
  ergo_release_val(__t2876);
  ergo_release_val(__t2877);
  ErgoVal __t2878 = YV_NULLV;
  ergo_release_val(__t2878);
  ErgoVal __t2879 = self; ergo_retain_val(__t2879);
  ergo_move_into(&__ret, __t2879);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ButtonGroup_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2880 = self; ergo_retain_val(__t2880);
  ErgoVal __t2881 = a0; ergo_retain_val(__t2881);
  __cogito_node_set_id(__t2880, __t2881);
  ergo_release_val(__t2880);
  ergo_release_val(__t2881);
  ErgoVal __t2882 = YV_NULLV;
  ergo_release_val(__t2882);
  ErgoVal __t2883 = self; ergo_retain_val(__t2883);
  ergo_move_into(&__ret, __t2883);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewSwitcher_add(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2884 = self; ergo_retain_val(__t2884);
  ErgoVal __t2885 = a0; ergo_retain_val(__t2885);
  ErgoVal __t2886 = a1; ergo_retain_val(__t2886);
  __cogito_view_switcher_add_lazy(__t2884, __t2885, __t2886);
  ergo_release_val(__t2884);
  ergo_release_val(__t2885);
  ergo_release_val(__t2886);
  ErgoVal __t2887 = YV_NULLV;
  ergo_release_val(__t2887);
  ErgoVal __t2888 = self; ergo_retain_val(__t2888);
  ergo_move_into(&__ret, __t2888);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewSwitcher_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2889 = self; ergo_retain_val(__t2889);
  ErgoVal __t2890 = a0; ergo_retain_val(__t2890);
  ErgoVal __t2891 = a1; ergo_retain_val(__t2891);
  ErgoVal __t2892 = a2; ergo_retain_val(__t2892);
  ErgoVal __t2893 = a3; ergo_retain_val(__t2893);
  __cogito_container_set_margins(__t2889, __t2890, __t2891, __t2892, __t2893);
  ergo_release_val(__t2889);
  ergo_release_val(__t2890);
  ergo_release_val(__t2891);
  ergo_release_val(__t2892);
  ergo_release_val(__t2893);
  ErgoVal __t2894 = YV_NULLV;
  ergo_release_val(__t2894);
  ErgoVal __t2895 = self; ergo_retain_val(__t2895);
  ergo_move_into(&__ret, __t2895);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewSwitcher_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2896 = self; ergo_retain_val(__t2896);
  ErgoVal __t2897 = a0; ergo_retain_val(__t2897);
  ErgoVal __t2898 = a1; ergo_retain_val(__t2898);
  ErgoVal __t2899 = a2; ergo_retain_val(__t2899);
  ErgoVal __t2900 = a3; ergo_retain_val(__t2900);
  __cogito_container_set_padding(__t2896, __t2897, __t2898, __t2899, __t2900);
  ergo_release_val(__t2896);
  ergo_release_val(__t2897);
  ergo_release_val(__t2898);
  ergo_release_val(__t2899);
  ergo_release_val(__t2900);
  ErgoVal __t2901 = YV_NULLV;
  ergo_release_val(__t2901);
  ErgoVal __t2902 = self; ergo_retain_val(__t2902);
  ergo_move_into(&__ret, __t2902);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewSwitcher_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2903 = self; ergo_retain_val(__t2903);
  ErgoVal __t2904 = a0; ergo_retain_val(__t2904);
  __cogito_container_set_align(__t2903, __t2904);
  ergo_release_val(__t2903);
  ergo_release_val(__t2904);
  ErgoVal __t2905 = YV_NULLV;
  ergo_release_val(__t2905);
  ErgoVal __t2906 = self; ergo_retain_val(__t2906);
  ergo_move_into(&__ret, __t2906);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewSwitcher_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2907 = self; ergo_retain_val(__t2907);
  ErgoVal __t2908 = a0; ergo_retain_val(__t2908);
  __cogito_container_set_halign(__t2907, __t2908);
  ergo_release_val(__t2907);
  ergo_release_val(__t2908);
  ErgoVal __t2909 = YV_NULLV;
  ergo_release_val(__t2909);
  ErgoVal __t2910 = self; ergo_retain_val(__t2910);
  ergo_move_into(&__ret, __t2910);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewSwitcher_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2911 = self; ergo_retain_val(__t2911);
  ErgoVal __t2912 = a0; ergo_retain_val(__t2912);
  __cogito_container_set_valign(__t2911, __t2912);
  ergo_release_val(__t2911);
  ergo_release_val(__t2912);
  ErgoVal __t2913 = YV_NULLV;
  ergo_release_val(__t2913);
  ErgoVal __t2914 = self; ergo_retain_val(__t2914);
  ergo_move_into(&__ret, __t2914);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewSwitcher_set_active(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2915 = self; ergo_retain_val(__t2915);
  ErgoVal __t2916 = a0; ergo_retain_val(__t2916);
  __cogito_view_switcher_set_active(__t2915, __t2916);
  ergo_release_val(__t2915);
  ergo_release_val(__t2916);
  ErgoVal __t2917 = YV_NULLV;
  ergo_release_val(__t2917);
  ErgoVal __t2918 = self; ergo_retain_val(__t2918);
  ergo_move_into(&__ret, __t2918);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewSwitcher_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2919 = self; ergo_retain_val(__t2919);
  ErgoVal __t2920 = a0; ergo_retain_val(__t2920);
  __cogito_build(__t2919, __t2920);
  ergo_release_val(__t2919);
  ergo_release_val(__t2920);
  ErgoVal __t2921 = YV_NULLV;
  ergo_release_val(__t2921);
  ErgoVal __t2922 = self; ergo_retain_val(__t2922);
  ergo_move_into(&__ret, __t2922);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewSwitcher_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2923 = self; ergo_retain_val(__t2923);
  ErgoVal __t2924 = a0; ergo_retain_val(__t2924);
  __cogito_container_set_hexpand(__t2923, __t2924);
  ergo_release_val(__t2923);
  ergo_release_val(__t2924);
  ErgoVal __t2925 = YV_NULLV;
  ergo_release_val(__t2925);
  ErgoVal __t2926 = self; ergo_retain_val(__t2926);
  ergo_move_into(&__ret, __t2926);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewSwitcher_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2927 = self; ergo_retain_val(__t2927);
  ErgoVal __t2928 = a0; ergo_retain_val(__t2928);
  __cogito_container_set_vexpand(__t2927, __t2928);
  ergo_release_val(__t2927);
  ergo_release_val(__t2928);
  ErgoVal __t2929 = YV_NULLV;
  ergo_release_val(__t2929);
  ErgoVal __t2930 = self; ergo_retain_val(__t2930);
  ergo_move_into(&__ret, __t2930);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewSwitcher_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2931 = self; ergo_retain_val(__t2931);
  ErgoVal __t2932 = a0; ergo_retain_val(__t2932);
  __cogito_node_set_disabled(__t2931, __t2932);
  ergo_release_val(__t2931);
  ergo_release_val(__t2932);
  ErgoVal __t2933 = YV_NULLV;
  ergo_release_val(__t2933);
  ErgoVal __t2934 = self; ergo_retain_val(__t2934);
  ergo_move_into(&__ret, __t2934);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewSwitcher_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2935 = self; ergo_retain_val(__t2935);
  ErgoVal __t2936 = a0; ergo_retain_val(__t2936);
  __cogito_node_set_class(__t2935, __t2936);
  ergo_release_val(__t2935);
  ergo_release_val(__t2936);
  ErgoVal __t2937 = YV_NULLV;
  ergo_release_val(__t2937);
  ErgoVal __t2938 = self; ergo_retain_val(__t2938);
  ergo_move_into(&__ret, __t2938);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewSwitcher_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2939 = self; ergo_retain_val(__t2939);
  ErgoVal __t2940 = a0; ergo_retain_val(__t2940);
  __cogito_node_set_id(__t2939, __t2940);
  ergo_release_val(__t2939);
  ergo_release_val(__t2940);
  ErgoVal __t2941 = YV_NULLV;
  ergo_release_val(__t2941);
  ErgoVal __t2942 = self; ergo_retain_val(__t2942);
  ergo_move_into(&__ret, __t2942);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2943 = self; ergo_retain_val(__t2943);
  ErgoVal __t2944 = a0; ergo_retain_val(__t2944);
  ErgoVal __t2945 = a1; ergo_retain_val(__t2945);
  ErgoVal __t2946 = a2; ergo_retain_val(__t2946);
  ErgoVal __t2947 = a3; ergo_retain_val(__t2947);
  __cogito_container_set_margins(__t2943, __t2944, __t2945, __t2946, __t2947);
  ergo_release_val(__t2943);
  ergo_release_val(__t2944);
  ergo_release_val(__t2945);
  ergo_release_val(__t2946);
  ergo_release_val(__t2947);
  ErgoVal __t2948 = YV_NULLV;
  ergo_release_val(__t2948);
  ErgoVal __t2949 = self; ergo_retain_val(__t2949);
  ergo_move_into(&__ret, __t2949);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2950 = self; ergo_retain_val(__t2950);
  ErgoVal __t2951 = a0; ergo_retain_val(__t2951);
  ErgoVal __t2952 = a1; ergo_retain_val(__t2952);
  ErgoVal __t2953 = a2; ergo_retain_val(__t2953);
  ErgoVal __t2954 = a3; ergo_retain_val(__t2954);
  __cogito_container_set_padding(__t2950, __t2951, __t2952, __t2953, __t2954);
  ergo_release_val(__t2950);
  ergo_release_val(__t2951);
  ergo_release_val(__t2952);
  ergo_release_val(__t2953);
  ergo_release_val(__t2954);
  ErgoVal __t2955 = YV_NULLV;
  ergo_release_val(__t2955);
  ErgoVal __t2956 = self; ergo_retain_val(__t2956);
  ergo_move_into(&__ret, __t2956);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2957 = self; ergo_retain_val(__t2957);
  ErgoVal __t2958 = a0; ergo_retain_val(__t2958);
  __cogito_container_set_align(__t2957, __t2958);
  ergo_release_val(__t2957);
  ergo_release_val(__t2958);
  ErgoVal __t2959 = YV_NULLV;
  ergo_release_val(__t2959);
  ErgoVal __t2960 = self; ergo_retain_val(__t2960);
  ergo_move_into(&__ret, __t2960);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2961 = self; ergo_retain_val(__t2961);
  ErgoVal __t2962 = a0; ergo_retain_val(__t2962);
  __cogito_container_set_halign(__t2961, __t2962);
  ergo_release_val(__t2961);
  ergo_release_val(__t2962);
  ErgoVal __t2963 = YV_NULLV;
  ergo_release_val(__t2963);
  ErgoVal __t2964 = self; ergo_retain_val(__t2964);
  ergo_move_into(&__ret, __t2964);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2965 = self; ergo_retain_val(__t2965);
  ErgoVal __t2966 = a0; ergo_retain_val(__t2966);
  __cogito_container_set_valign(__t2965, __t2966);
  ergo_release_val(__t2965);
  ergo_release_val(__t2966);
  ErgoVal __t2967 = YV_NULLV;
  ergo_release_val(__t2967);
  ErgoVal __t2968 = self; ergo_retain_val(__t2968);
  ergo_move_into(&__ret, __t2968);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_value(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2969 = self; ergo_retain_val(__t2969);
  ErgoVal __t2970 = a0; ergo_retain_val(__t2970);
  __cogito_progress_set_value(__t2969, __t2970);
  ergo_release_val(__t2969);
  ergo_release_val(__t2970);
  ErgoVal __t2971 = YV_NULLV;
  ergo_release_val(__t2971);
  ErgoVal __t2972 = self; ergo_retain_val(__t2972);
  ergo_move_into(&__ret, __t2972);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_value(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2973 = self; ergo_retain_val(__t2973);
  ErgoVal __t2974 = __cogito_progress_get_value(__t2973);
  ergo_release_val(__t2973);
  ergo_move_into(&__ret, __t2974);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_indeterminate(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2975 = self; ergo_retain_val(__t2975);
  ErgoVal __t2976 = a0; ergo_retain_val(__t2976);
  __cogito_progress_set_indeterminate(__t2975, __t2976);
  ergo_release_val(__t2975);
  ergo_release_val(__t2976);
  ErgoVal __t2977 = YV_NULLV;
  ergo_release_val(__t2977);
  ErgoVal __t2978 = self; ergo_retain_val(__t2978);
  ergo_move_into(&__ret, __t2978);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_indeterminate(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2979 = self; ergo_retain_val(__t2979);
  ErgoVal __t2980 = __cogito_progress_get_indeterminate(__t2979);
  ergo_release_val(__t2979);
  ergo_move_into(&__ret, __t2980);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_thickness(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2981 = self; ergo_retain_val(__t2981);
  ErgoVal __t2982 = a0; ergo_retain_val(__t2982);
  __cogito_progress_set_thickness(__t2981, __t2982);
  ergo_release_val(__t2981);
  ergo_release_val(__t2982);
  ErgoVal __t2983 = YV_NULLV;
  ergo_release_val(__t2983);
  ErgoVal __t2984 = self; ergo_retain_val(__t2984);
  ergo_move_into(&__ret, __t2984);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_thickness(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2985 = self; ergo_retain_val(__t2985);
  ErgoVal __t2986 = __cogito_progress_get_thickness(__t2985);
  ergo_release_val(__t2985);
  ergo_move_into(&__ret, __t2986);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_wavy(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2987 = self; ergo_retain_val(__t2987);
  ErgoVal __t2988 = a0; ergo_retain_val(__t2988);
  __cogito_progress_set_wavy(__t2987, __t2988);
  ergo_release_val(__t2987);
  ergo_release_val(__t2988);
  ErgoVal __t2989 = YV_NULLV;
  ergo_release_val(__t2989);
  ErgoVal __t2990 = self; ergo_retain_val(__t2990);
  ergo_move_into(&__ret, __t2990);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_wavy(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2991 = self; ergo_retain_val(__t2991);
  ErgoVal __t2992 = __cogito_progress_get_wavy(__t2991);
  ergo_release_val(__t2991);
  ergo_move_into(&__ret, __t2992);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_circular(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2993 = self; ergo_retain_val(__t2993);
  ErgoVal __t2994 = a0; ergo_retain_val(__t2994);
  __cogito_progress_set_circular(__t2993, __t2994);
  ergo_release_val(__t2993);
  ergo_release_val(__t2994);
  ErgoVal __t2995 = YV_NULLV;
  ergo_release_val(__t2995);
  ErgoVal __t2996 = self; ergo_retain_val(__t2996);
  ergo_move_into(&__ret, __t2996);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_circular(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2997 = self; ergo_retain_val(__t2997);
  ErgoVal __t2998 = __cogito_progress_get_circular(__t2997);
  ergo_release_val(__t2997);
  ergo_move_into(&__ret, __t2998);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t2999 = self; ergo_retain_val(__t2999);
  ErgoVal __t3000 = a0; ergo_retain_val(__t3000);
  __cogito_container_set_hexpand(__t2999, __t3000);
  ergo_release_val(__t2999);
  ergo_release_val(__t3000);
  ErgoVal __t3001 = YV_NULLV;
  ergo_release_val(__t3001);
  ErgoVal __t3002 = self; ergo_retain_val(__t3002);
  ergo_move_into(&__ret, __t3002);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3003 = self; ergo_retain_val(__t3003);
  ErgoVal __t3004 = a0; ergo_retain_val(__t3004);
  __cogito_container_set_vexpand(__t3003, __t3004);
  ergo_release_val(__t3003);
  ergo_release_val(__t3004);
  ErgoVal __t3005 = YV_NULLV;
  ergo_release_val(__t3005);
  ErgoVal __t3006 = self; ergo_retain_val(__t3006);
  ergo_move_into(&__ret, __t3006);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3007 = self; ergo_retain_val(__t3007);
  ErgoVal __t3008 = a0; ergo_retain_val(__t3008);
  __cogito_node_set_disabled(__t3007, __t3008);
  ergo_release_val(__t3007);
  ergo_release_val(__t3008);
  ErgoVal __t3009 = YV_NULLV;
  ergo_release_val(__t3009);
  ErgoVal __t3010 = self; ergo_retain_val(__t3010);
  ergo_move_into(&__ret, __t3010);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3011 = self; ergo_retain_val(__t3011);
  ErgoVal __t3012 = a0; ergo_retain_val(__t3012);
  __cogito_node_set_class(__t3011, __t3012);
  ergo_release_val(__t3011);
  ergo_release_val(__t3012);
  ErgoVal __t3013 = YV_NULLV;
  ergo_release_val(__t3013);
  ErgoVal __t3014 = self; ergo_retain_val(__t3014);
  ergo_move_into(&__ret, __t3014);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Progress_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3015 = self; ergo_retain_val(__t3015);
  ErgoVal __t3016 = a0; ergo_retain_val(__t3016);
  __cogito_node_set_id(__t3015, __t3016);
  ergo_release_val(__t3015);
  ergo_release_val(__t3016);
  ErgoVal __t3017 = YV_NULLV;
  ergo_release_val(__t3017);
  ErgoVal __t3018 = self; ergo_retain_val(__t3018);
  ergo_move_into(&__ret, __t3018);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Divider_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3019 = self; ergo_retain_val(__t3019);
  ErgoVal __t3020 = a0; ergo_retain_val(__t3020);
  ErgoVal __t3021 = a1; ergo_retain_val(__t3021);
  ErgoVal __t3022 = a2; ergo_retain_val(__t3022);
  ErgoVal __t3023 = a3; ergo_retain_val(__t3023);
  __cogito_container_set_margins(__t3019, __t3020, __t3021, __t3022, __t3023);
  ergo_release_val(__t3019);
  ergo_release_val(__t3020);
  ergo_release_val(__t3021);
  ergo_release_val(__t3022);
  ergo_release_val(__t3023);
  ErgoVal __t3024 = YV_NULLV;
  ergo_release_val(__t3024);
  ErgoVal __t3025 = self; ergo_retain_val(__t3025);
  ergo_move_into(&__ret, __t3025);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Divider_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3026 = self; ergo_retain_val(__t3026);
  ErgoVal __t3027 = a0; ergo_retain_val(__t3027);
  ErgoVal __t3028 = a1; ergo_retain_val(__t3028);
  ErgoVal __t3029 = a2; ergo_retain_val(__t3029);
  ErgoVal __t3030 = a3; ergo_retain_val(__t3030);
  __cogito_container_set_padding(__t3026, __t3027, __t3028, __t3029, __t3030);
  ergo_release_val(__t3026);
  ergo_release_val(__t3027);
  ergo_release_val(__t3028);
  ergo_release_val(__t3029);
  ergo_release_val(__t3030);
  ErgoVal __t3031 = YV_NULLV;
  ergo_release_val(__t3031);
  ErgoVal __t3032 = self; ergo_retain_val(__t3032);
  ergo_move_into(&__ret, __t3032);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Divider_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3033 = self; ergo_retain_val(__t3033);
  ErgoVal __t3034 = a0; ergo_retain_val(__t3034);
  __cogito_container_set_align(__t3033, __t3034);
  ergo_release_val(__t3033);
  ergo_release_val(__t3034);
  ErgoVal __t3035 = YV_NULLV;
  ergo_release_val(__t3035);
  ErgoVal __t3036 = self; ergo_retain_val(__t3036);
  ergo_move_into(&__ret, __t3036);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Divider_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3037 = self; ergo_retain_val(__t3037);
  ErgoVal __t3038 = a0; ergo_retain_val(__t3038);
  __cogito_container_set_halign(__t3037, __t3038);
  ergo_release_val(__t3037);
  ergo_release_val(__t3038);
  ErgoVal __t3039 = YV_NULLV;
  ergo_release_val(__t3039);
  ErgoVal __t3040 = self; ergo_retain_val(__t3040);
  ergo_move_into(&__ret, __t3040);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Divider_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3041 = self; ergo_retain_val(__t3041);
  ErgoVal __t3042 = a0; ergo_retain_val(__t3042);
  __cogito_container_set_valign(__t3041, __t3042);
  ergo_release_val(__t3041);
  ergo_release_val(__t3042);
  ErgoVal __t3043 = YV_NULLV;
  ergo_release_val(__t3043);
  ErgoVal __t3044 = self; ergo_retain_val(__t3044);
  ergo_move_into(&__ret, __t3044);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Divider_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3045 = self; ergo_retain_val(__t3045);
  ErgoVal __t3046 = a0; ergo_retain_val(__t3046);
  __cogito_container_set_hexpand(__t3045, __t3046);
  ergo_release_val(__t3045);
  ergo_release_val(__t3046);
  ErgoVal __t3047 = YV_NULLV;
  ergo_release_val(__t3047);
  ErgoVal __t3048 = self; ergo_retain_val(__t3048);
  ergo_move_into(&__ret, __t3048);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Divider_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3049 = self; ergo_retain_val(__t3049);
  ErgoVal __t3050 = a0; ergo_retain_val(__t3050);
  __cogito_container_set_vexpand(__t3049, __t3050);
  ergo_release_val(__t3049);
  ergo_release_val(__t3050);
  ErgoVal __t3051 = YV_NULLV;
  ergo_release_val(__t3051);
  ErgoVal __t3052 = self; ergo_retain_val(__t3052);
  ergo_move_into(&__ret, __t3052);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Divider_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3053 = self; ergo_retain_val(__t3053);
  ErgoVal __t3054 = a0; ergo_retain_val(__t3054);
  __cogito_node_set_disabled(__t3053, __t3054);
  ergo_release_val(__t3053);
  ergo_release_val(__t3054);
  ErgoVal __t3055 = YV_NULLV;
  ergo_release_val(__t3055);
  ErgoVal __t3056 = self; ergo_retain_val(__t3056);
  ergo_move_into(&__ret, __t3056);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Divider_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3057 = self; ergo_retain_val(__t3057);
  ErgoVal __t3058 = a0; ergo_retain_val(__t3058);
  __cogito_node_set_class(__t3057, __t3058);
  ergo_release_val(__t3057);
  ergo_release_val(__t3058);
  ErgoVal __t3059 = YV_NULLV;
  ergo_release_val(__t3059);
  ErgoVal __t3060 = self; ergo_retain_val(__t3060);
  ergo_move_into(&__ret, __t3060);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Divider_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3061 = self; ergo_retain_val(__t3061);
  ErgoVal __t3062 = a0; ergo_retain_val(__t3062);
  __cogito_node_set_id(__t3061, __t3062);
  ergo_release_val(__t3061);
  ergo_release_val(__t3062);
  ErgoVal __t3063 = YV_NULLV;
  ergo_release_val(__t3063);
  ErgoVal __t3064 = self; ergo_retain_val(__t3064);
  ergo_move_into(&__ret, __t3064);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3065 = self; ergo_retain_val(__t3065);
  ErgoVal __t3066 = a0; ergo_retain_val(__t3066);
  __cogito_container_add(__t3065, __t3066);
  ergo_release_val(__t3065);
  ergo_release_val(__t3066);
  ErgoVal __t3067 = YV_NULLV;
  ergo_release_val(__t3067);
  ErgoVal __t3068 = self; ergo_retain_val(__t3068);
  ergo_move_into(&__ret, __t3068);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3069 = self; ergo_retain_val(__t3069);
  ErgoVal __t3070 = a0; ergo_retain_val(__t3070);
  ErgoVal __t3071 = a1; ergo_retain_val(__t3071);
  ErgoVal __t3072 = a2; ergo_retain_val(__t3072);
  ErgoVal __t3073 = a3; ergo_retain_val(__t3073);
  __cogito_container_set_margins(__t3069, __t3070, __t3071, __t3072, __t3073);
  ergo_release_val(__t3069);
  ergo_release_val(__t3070);
  ergo_release_val(__t3071);
  ergo_release_val(__t3072);
  ergo_release_val(__t3073);
  ErgoVal __t3074 = YV_NULLV;
  ergo_release_val(__t3074);
  ErgoVal __t3075 = self; ergo_retain_val(__t3075);
  ergo_move_into(&__ret, __t3075);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3076 = self; ergo_retain_val(__t3076);
  ErgoVal __t3077 = a0; ergo_retain_val(__t3077);
  ErgoVal __t3078 = a1; ergo_retain_val(__t3078);
  ErgoVal __t3079 = a2; ergo_retain_val(__t3079);
  ErgoVal __t3080 = a3; ergo_retain_val(__t3080);
  __cogito_container_set_padding(__t3076, __t3077, __t3078, __t3079, __t3080);
  ergo_release_val(__t3076);
  ergo_release_val(__t3077);
  ergo_release_val(__t3078);
  ergo_release_val(__t3079);
  ergo_release_val(__t3080);
  ErgoVal __t3081 = YV_NULLV;
  ergo_release_val(__t3081);
  ErgoVal __t3082 = self; ergo_retain_val(__t3082);
  ergo_move_into(&__ret, __t3082);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3083 = self; ergo_retain_val(__t3083);
  ErgoVal __t3084 = a0; ergo_retain_val(__t3084);
  __cogito_container_set_align(__t3083, __t3084);
  ergo_release_val(__t3083);
  ergo_release_val(__t3084);
  ErgoVal __t3085 = YV_NULLV;
  ergo_release_val(__t3085);
  ErgoVal __t3086 = self; ergo_retain_val(__t3086);
  ergo_move_into(&__ret, __t3086);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3087 = self; ergo_retain_val(__t3087);
  ErgoVal __t3088 = a0; ergo_retain_val(__t3088);
  __cogito_container_set_halign(__t3087, __t3088);
  ergo_release_val(__t3087);
  ergo_release_val(__t3088);
  ErgoVal __t3089 = YV_NULLV;
  ergo_release_val(__t3089);
  ErgoVal __t3090 = self; ergo_retain_val(__t3090);
  ergo_move_into(&__ret, __t3090);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3091 = self; ergo_retain_val(__t3091);
  ErgoVal __t3092 = a0; ergo_retain_val(__t3092);
  __cogito_container_set_valign(__t3091, __t3092);
  ergo_release_val(__t3091);
  ergo_release_val(__t3092);
  ErgoVal __t3093 = YV_NULLV;
  ergo_release_val(__t3093);
  ErgoVal __t3094 = self; ergo_retain_val(__t3094);
  ergo_move_into(&__ret, __t3094);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_gap(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3095 = self; ergo_retain_val(__t3095);
  ErgoVal __t3096 = a0; ergo_retain_val(__t3096);
  __cogito_container_set_gap(__t3095, __t3096);
  ergo_release_val(__t3095);
  ergo_release_val(__t3096);
  ErgoVal __t3097 = YV_NULLV;
  ergo_release_val(__t3097);
  ErgoVal __t3098 = self; ergo_retain_val(__t3098);
  ergo_move_into(&__ret, __t3098);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3099 = self; ergo_retain_val(__t3099);
  ErgoVal __t3100 = a0; ergo_retain_val(__t3100);
  __cogito_build(__t3099, __t3100);
  ergo_release_val(__t3099);
  ergo_release_val(__t3100);
  ErgoVal __t3101 = YV_NULLV;
  ergo_release_val(__t3101);
  ErgoVal __t3102 = self; ergo_retain_val(__t3102);
  ergo_move_into(&__ret, __t3102);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3103 = self; ergo_retain_val(__t3103);
  ErgoVal __t3104 = a0; ergo_retain_val(__t3104);
  __cogito_container_set_hexpand(__t3103, __t3104);
  ergo_release_val(__t3103);
  ergo_release_val(__t3104);
  ErgoVal __t3105 = YV_NULLV;
  ergo_release_val(__t3105);
  ErgoVal __t3106 = self; ergo_retain_val(__t3106);
  ergo_move_into(&__ret, __t3106);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3107 = self; ergo_retain_val(__t3107);
  ErgoVal __t3108 = a0; ergo_retain_val(__t3108);
  __cogito_container_set_vexpand(__t3107, __t3108);
  ergo_release_val(__t3107);
  ergo_release_val(__t3108);
  ErgoVal __t3109 = YV_NULLV;
  ergo_release_val(__t3109);
  ErgoVal __t3110 = self; ergo_retain_val(__t3110);
  ergo_move_into(&__ret, __t3110);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3111 = self; ergo_retain_val(__t3111);
  ErgoVal __t3112 = a0; ergo_retain_val(__t3112);
  __cogito_node_set_disabled(__t3111, __t3112);
  ergo_release_val(__t3111);
  ergo_release_val(__t3112);
  ErgoVal __t3113 = YV_NULLV;
  ergo_release_val(__t3113);
  ErgoVal __t3114 = self; ergo_retain_val(__t3114);
  ergo_move_into(&__ret, __t3114);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3115 = self; ergo_retain_val(__t3115);
  ErgoVal __t3116 = a0; ergo_retain_val(__t3116);
  __cogito_node_set_class(__t3115, __t3116);
  ergo_release_val(__t3115);
  ergo_release_val(__t3116);
  ErgoVal __t3117 = YV_NULLV;
  ergo_release_val(__t3117);
  ErgoVal __t3118 = self; ergo_retain_val(__t3118);
  ergo_move_into(&__ret, __t3118);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3119 = self; ergo_retain_val(__t3119);
  ErgoVal __t3120 = a0; ergo_retain_val(__t3120);
  __cogito_node_set_id(__t3119, __t3120);
  ergo_release_val(__t3119);
  ergo_release_val(__t3120);
  ErgoVal __t3121 = YV_NULLV;
  ergo_release_val(__t3121);
  ErgoVal __t3122 = self; ergo_retain_val(__t3122);
  ergo_move_into(&__ret, __t3122);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_subtitle(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3123 = self; ergo_retain_val(__t3123);
  ErgoVal __t3124 = a0; ergo_retain_val(__t3124);
  __cogito_card_set_subtitle(__t3123, __t3124);
  ergo_release_val(__t3123);
  ergo_release_val(__t3124);
  ErgoVal __t3125 = YV_NULLV;
  ergo_release_val(__t3125);
  ErgoVal __t3126 = self; ergo_retain_val(__t3126);
  ergo_move_into(&__ret, __t3126);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_on_click(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3127 = self; ergo_retain_val(__t3127);
  ErgoVal __t3128 = a0; ergo_retain_val(__t3128);
  __cogito_card_on_click(__t3127, __t3128);
  ergo_release_val(__t3127);
  ergo_release_val(__t3128);
  ErgoVal __t3129 = YV_NULLV;
  ergo_release_val(__t3129);
  ErgoVal __t3130 = self; ergo_retain_val(__t3130);
  ergo_move_into(&__ret, __t3130);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_variant(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3131 = self; ergo_retain_val(__t3131);
  ErgoVal __t3132 = a0; ergo_retain_val(__t3132);
  __cogito_card_set_variant(__t3131, __t3132);
  ergo_release_val(__t3131);
  ergo_release_val(__t3132);
  ErgoVal __t3133 = YV_NULLV;
  ergo_release_val(__t3133);
  ErgoVal __t3134 = self; ergo_retain_val(__t3134);
  ergo_move_into(&__ret, __t3134);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_set_header_image(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3135 = self; ergo_retain_val(__t3135);
  ErgoVal __t3136 = a0; ergo_retain_val(__t3136);
  __cogito_card_set_header_image(__t3135, __t3136);
  ergo_release_val(__t3135);
  ergo_release_val(__t3136);
  ErgoVal __t3137 = YV_NULLV;
  ergo_release_val(__t3137);
  ErgoVal __t3138 = self; ergo_retain_val(__t3138);
  ergo_move_into(&__ret, __t3138);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_add_action(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3139 = self; ergo_retain_val(__t3139);
  ErgoVal __t3140 = a0; ergo_retain_val(__t3140);
  __cogito_card_add_action(__t3139, __t3140);
  ergo_release_val(__t3139);
  ergo_release_val(__t3140);
  ErgoVal __t3141 = YV_NULLV;
  ergo_release_val(__t3141);
  ErgoVal __t3142 = self; ergo_retain_val(__t3142);
  ergo_move_into(&__ret, __t3142);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Card_add_overflow(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3143 = self; ergo_retain_val(__t3143);
  ErgoVal __t3144 = a0; ergo_retain_val(__t3144);
  ErgoVal __t3145 = a1; ergo_retain_val(__t3145);
  __cogito_card_add_overflow(__t3143, __t3144, __t3145);
  ergo_release_val(__t3143);
  ergo_release_val(__t3144);
  ergo_release_val(__t3145);
  ErgoVal __t3146 = YV_NULLV;
  ergo_release_val(__t3146);
  ErgoVal __t3147 = self; ergo_retain_val(__t3147);
  ergo_move_into(&__ret, __t3147);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Avatar_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3148 = self; ergo_retain_val(__t3148);
  ErgoVal __t3149 = a0; ergo_retain_val(__t3149);
  ErgoVal __t3150 = a1; ergo_retain_val(__t3150);
  ErgoVal __t3151 = a2; ergo_retain_val(__t3151);
  ErgoVal __t3152 = a3; ergo_retain_val(__t3152);
  __cogito_container_set_margins(__t3148, __t3149, __t3150, __t3151, __t3152);
  ergo_release_val(__t3148);
  ergo_release_val(__t3149);
  ergo_release_val(__t3150);
  ergo_release_val(__t3151);
  ergo_release_val(__t3152);
  ErgoVal __t3153 = YV_NULLV;
  ergo_release_val(__t3153);
  ErgoVal __t3154 = self; ergo_retain_val(__t3154);
  ergo_move_into(&__ret, __t3154);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Avatar_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3155 = self; ergo_retain_val(__t3155);
  ErgoVal __t3156 = a0; ergo_retain_val(__t3156);
  ErgoVal __t3157 = a1; ergo_retain_val(__t3157);
  ErgoVal __t3158 = a2; ergo_retain_val(__t3158);
  ErgoVal __t3159 = a3; ergo_retain_val(__t3159);
  __cogito_container_set_padding(__t3155, __t3156, __t3157, __t3158, __t3159);
  ergo_release_val(__t3155);
  ergo_release_val(__t3156);
  ergo_release_val(__t3157);
  ergo_release_val(__t3158);
  ergo_release_val(__t3159);
  ErgoVal __t3160 = YV_NULLV;
  ergo_release_val(__t3160);
  ErgoVal __t3161 = self; ergo_retain_val(__t3161);
  ergo_move_into(&__ret, __t3161);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Avatar_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3162 = self; ergo_retain_val(__t3162);
  ErgoVal __t3163 = a0; ergo_retain_val(__t3163);
  __cogito_container_set_halign(__t3162, __t3163);
  ergo_release_val(__t3162);
  ergo_release_val(__t3163);
  ErgoVal __t3164 = YV_NULLV;
  ergo_release_val(__t3164);
  ErgoVal __t3165 = self; ergo_retain_val(__t3165);
  ergo_move_into(&__ret, __t3165);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Avatar_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3166 = self; ergo_retain_val(__t3166);
  ErgoVal __t3167 = a0; ergo_retain_val(__t3167);
  __cogito_container_set_valign(__t3166, __t3167);
  ergo_release_val(__t3166);
  ergo_release_val(__t3167);
  ErgoVal __t3168 = YV_NULLV;
  ergo_release_val(__t3168);
  ErgoVal __t3169 = self; ergo_retain_val(__t3169);
  ergo_move_into(&__ret, __t3169);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Avatar_set_image(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3170 = self; ergo_retain_val(__t3170);
  ErgoVal __t3171 = a0; ergo_retain_val(__t3171);
  __cogito_avatar_set_image(__t3170, __t3171);
  ergo_release_val(__t3170);
  ergo_release_val(__t3171);
  ErgoVal __t3172 = YV_NULLV;
  ergo_release_val(__t3172);
  ErgoVal __t3173 = self; ergo_retain_val(__t3173);
  ergo_move_into(&__ret, __t3173);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Avatar_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3174 = self; ergo_retain_val(__t3174);
  ErgoVal __t3175 = a0; ergo_retain_val(__t3175);
  __cogito_container_set_hexpand(__t3174, __t3175);
  ergo_release_val(__t3174);
  ergo_release_val(__t3175);
  ErgoVal __t3176 = YV_NULLV;
  ergo_release_val(__t3176);
  ErgoVal __t3177 = self; ergo_retain_val(__t3177);
  ergo_move_into(&__ret, __t3177);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Avatar_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3178 = self; ergo_retain_val(__t3178);
  ErgoVal __t3179 = a0; ergo_retain_val(__t3179);
  __cogito_container_set_vexpand(__t3178, __t3179);
  ergo_release_val(__t3178);
  ergo_release_val(__t3179);
  ErgoVal __t3180 = YV_NULLV;
  ergo_release_val(__t3180);
  ErgoVal __t3181 = self; ergo_retain_val(__t3181);
  ergo_move_into(&__ret, __t3181);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Avatar_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3182 = self; ergo_retain_val(__t3182);
  ErgoVal __t3183 = a0; ergo_retain_val(__t3183);
  __cogito_node_set_class(__t3182, __t3183);
  ergo_release_val(__t3182);
  ergo_release_val(__t3183);
  ErgoVal __t3184 = YV_NULLV;
  ergo_release_val(__t3184);
  ErgoVal __t3185 = self; ergo_retain_val(__t3185);
  ergo_move_into(&__ret, __t3185);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Avatar_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3186 = self; ergo_retain_val(__t3186);
  ErgoVal __t3187 = a0; ergo_retain_val(__t3187);
  __cogito_node_set_id(__t3186, __t3187);
  ergo_release_val(__t3186);
  ergo_release_val(__t3187);
  ErgoVal __t3188 = YV_NULLV;
  ergo_release_val(__t3188);
  ErgoVal __t3189 = self; ergo_retain_val(__t3189);
  ergo_move_into(&__ret, __t3189);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Badge_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3190 = self; ergo_retain_val(__t3190);
  ErgoVal __t3191 = a0; ergo_retain_val(__t3191);
  ErgoVal __t3192 = a1; ergo_retain_val(__t3192);
  ErgoVal __t3193 = a2; ergo_retain_val(__t3193);
  ErgoVal __t3194 = a3; ergo_retain_val(__t3194);
  __cogito_container_set_margins(__t3190, __t3191, __t3192, __t3193, __t3194);
  ergo_release_val(__t3190);
  ergo_release_val(__t3191);
  ergo_release_val(__t3192);
  ergo_release_val(__t3193);
  ergo_release_val(__t3194);
  ErgoVal __t3195 = YV_NULLV;
  ergo_release_val(__t3195);
  ErgoVal __t3196 = self; ergo_retain_val(__t3196);
  ergo_move_into(&__ret, __t3196);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Badge_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3197 = self; ergo_retain_val(__t3197);
  ErgoVal __t3198 = a0; ergo_retain_val(__t3198);
  __cogito_container_set_halign(__t3197, __t3198);
  ergo_release_val(__t3197);
  ergo_release_val(__t3198);
  ErgoVal __t3199 = YV_NULLV;
  ergo_release_val(__t3199);
  ErgoVal __t3200 = self; ergo_retain_val(__t3200);
  ergo_move_into(&__ret, __t3200);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Badge_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3201 = self; ergo_retain_val(__t3201);
  ErgoVal __t3202 = a0; ergo_retain_val(__t3202);
  __cogito_container_set_valign(__t3201, __t3202);
  ergo_release_val(__t3201);
  ergo_release_val(__t3202);
  ErgoVal __t3203 = YV_NULLV;
  ergo_release_val(__t3203);
  ErgoVal __t3204 = self; ergo_retain_val(__t3204);
  ergo_move_into(&__ret, __t3204);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Badge_set_count(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3205 = self; ergo_retain_val(__t3205);
  ErgoVal __t3206 = a0; ergo_retain_val(__t3206);
  __cogito_badge_set_count(__t3205, __t3206);
  ergo_release_val(__t3205);
  ergo_release_val(__t3206);
  ErgoVal __t3207 = YV_NULLV;
  ergo_release_val(__t3207);
  ErgoVal __t3208 = self; ergo_retain_val(__t3208);
  ergo_move_into(&__ret, __t3208);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Badge_count(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3209 = self; ergo_retain_val(__t3209);
  ErgoVal __t3210 = __cogito_badge_get_count(__t3209);
  ergo_release_val(__t3209);
  ergo_move_into(&__ret, __t3210);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Badge_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3211 = self; ergo_retain_val(__t3211);
  ErgoVal __t3212 = a0; ergo_retain_val(__t3212);
  __cogito_node_set_class(__t3211, __t3212);
  ergo_release_val(__t3211);
  ergo_release_val(__t3212);
  ErgoVal __t3213 = YV_NULLV;
  ergo_release_val(__t3213);
  ErgoVal __t3214 = self; ergo_retain_val(__t3214);
  ergo_move_into(&__ret, __t3214);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Badge_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3215 = self; ergo_retain_val(__t3215);
  ErgoVal __t3216 = a0; ergo_retain_val(__t3216);
  __cogito_node_set_id(__t3215, __t3216);
  ergo_release_val(__t3215);
  ergo_release_val(__t3216);
  ErgoVal __t3217 = YV_NULLV;
  ergo_release_val(__t3217);
  ErgoVal __t3218 = self; ergo_retain_val(__t3218);
  ergo_move_into(&__ret, __t3218);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Banner_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3219 = self; ergo_retain_val(__t3219);
  ErgoVal __t3220 = a0; ergo_retain_val(__t3220);
  ErgoVal __t3221 = a1; ergo_retain_val(__t3221);
  ErgoVal __t3222 = a2; ergo_retain_val(__t3222);
  ErgoVal __t3223 = a3; ergo_retain_val(__t3223);
  __cogito_container_set_margins(__t3219, __t3220, __t3221, __t3222, __t3223);
  ergo_release_val(__t3219);
  ergo_release_val(__t3220);
  ergo_release_val(__t3221);
  ergo_release_val(__t3222);
  ergo_release_val(__t3223);
  ErgoVal __t3224 = YV_NULLV;
  ergo_release_val(__t3224);
  ErgoVal __t3225 = self; ergo_retain_val(__t3225);
  ergo_move_into(&__ret, __t3225);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Banner_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3226 = self; ergo_retain_val(__t3226);
  ErgoVal __t3227 = a0; ergo_retain_val(__t3227);
  ErgoVal __t3228 = a1; ergo_retain_val(__t3228);
  ErgoVal __t3229 = a2; ergo_retain_val(__t3229);
  ErgoVal __t3230 = a3; ergo_retain_val(__t3230);
  __cogito_container_set_padding(__t3226, __t3227, __t3228, __t3229, __t3230);
  ergo_release_val(__t3226);
  ergo_release_val(__t3227);
  ergo_release_val(__t3228);
  ergo_release_val(__t3229);
  ergo_release_val(__t3230);
  ErgoVal __t3231 = YV_NULLV;
  ergo_release_val(__t3231);
  ErgoVal __t3232 = self; ergo_retain_val(__t3232);
  ergo_move_into(&__ret, __t3232);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Banner_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3233 = self; ergo_retain_val(__t3233);
  ErgoVal __t3234 = a0; ergo_retain_val(__t3234);
  __cogito_container_set_halign(__t3233, __t3234);
  ergo_release_val(__t3233);
  ergo_release_val(__t3234);
  ErgoVal __t3235 = YV_NULLV;
  ergo_release_val(__t3235);
  ErgoVal __t3236 = self; ergo_retain_val(__t3236);
  ergo_move_into(&__ret, __t3236);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Banner_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3237 = self; ergo_retain_val(__t3237);
  ErgoVal __t3238 = a0; ergo_retain_val(__t3238);
  __cogito_container_set_valign(__t3237, __t3238);
  ergo_release_val(__t3237);
  ergo_release_val(__t3238);
  ErgoVal __t3239 = YV_NULLV;
  ergo_release_val(__t3239);
  ErgoVal __t3240 = self; ergo_retain_val(__t3240);
  ergo_move_into(&__ret, __t3240);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Banner_set_action(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3241 = self; ergo_retain_val(__t3241);
  ErgoVal __t3242 = a0; ergo_retain_val(__t3242);
  ErgoVal __t3243 = a1; ergo_retain_val(__t3243);
  __cogito_banner_set_action(__t3241, __t3242, __t3243);
  ergo_release_val(__t3241);
  ergo_release_val(__t3242);
  ergo_release_val(__t3243);
  ErgoVal __t3244 = YV_NULLV;
  ergo_release_val(__t3244);
  ErgoVal __t3245 = self; ergo_retain_val(__t3245);
  ergo_move_into(&__ret, __t3245);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Banner_set_icon(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3246 = self; ergo_retain_val(__t3246);
  ErgoVal __t3247 = a0; ergo_retain_val(__t3247);
  __cogito_banner_set_icon(__t3246, __t3247);
  ergo_release_val(__t3246);
  ergo_release_val(__t3247);
  ErgoVal __t3248 = YV_NULLV;
  ergo_release_val(__t3248);
  ErgoVal __t3249 = self; ergo_retain_val(__t3249);
  ergo_move_into(&__ret, __t3249);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Banner_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3250 = self; ergo_retain_val(__t3250);
  ErgoVal __t3251 = a0; ergo_retain_val(__t3251);
  __cogito_container_set_hexpand(__t3250, __t3251);
  ergo_release_val(__t3250);
  ergo_release_val(__t3251);
  ErgoVal __t3252 = YV_NULLV;
  ergo_release_val(__t3252);
  ErgoVal __t3253 = self; ergo_retain_val(__t3253);
  ergo_move_into(&__ret, __t3253);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Banner_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3254 = self; ergo_retain_val(__t3254);
  ErgoVal __t3255 = a0; ergo_retain_val(__t3255);
  __cogito_container_set_vexpand(__t3254, __t3255);
  ergo_release_val(__t3254);
  ergo_release_val(__t3255);
  ErgoVal __t3256 = YV_NULLV;
  ergo_release_val(__t3256);
  ErgoVal __t3257 = self; ergo_retain_val(__t3257);
  ergo_move_into(&__ret, __t3257);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Banner_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3258 = self; ergo_retain_val(__t3258);
  ErgoVal __t3259 = a0; ergo_retain_val(__t3259);
  __cogito_node_set_class(__t3258, __t3259);
  ergo_release_val(__t3258);
  ergo_release_val(__t3259);
  ErgoVal __t3260 = YV_NULLV;
  ergo_release_val(__t3260);
  ErgoVal __t3261 = self; ergo_retain_val(__t3261);
  ergo_move_into(&__ret, __t3261);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Banner_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3262 = self; ergo_retain_val(__t3262);
  ErgoVal __t3263 = a0; ergo_retain_val(__t3263);
  __cogito_node_set_id(__t3262, __t3263);
  ergo_release_val(__t3262);
  ergo_release_val(__t3263);
  ErgoVal __t3264 = YV_NULLV;
  ergo_release_val(__t3264);
  ErgoVal __t3265 = self; ergo_retain_val(__t3265);
  ergo_move_into(&__ret, __t3265);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomSheet_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3266 = self; ergo_retain_val(__t3266);
  ErgoVal __t3267 = a0; ergo_retain_val(__t3267);
  __cogito_container_add(__t3266, __t3267);
  ergo_release_val(__t3266);
  ergo_release_val(__t3267);
  ErgoVal __t3268 = YV_NULLV;
  ergo_release_val(__t3268);
  ErgoVal __t3269 = self; ergo_retain_val(__t3269);
  ergo_move_into(&__ret, __t3269);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomSheet_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3270 = self; ergo_retain_val(__t3270);
  ErgoVal __t3271 = a0; ergo_retain_val(__t3271);
  ErgoVal __t3272 = a1; ergo_retain_val(__t3272);
  ErgoVal __t3273 = a2; ergo_retain_val(__t3273);
  ErgoVal __t3274 = a3; ergo_retain_val(__t3274);
  __cogito_container_set_margins(__t3270, __t3271, __t3272, __t3273, __t3274);
  ergo_release_val(__t3270);
  ergo_release_val(__t3271);
  ergo_release_val(__t3272);
  ergo_release_val(__t3273);
  ergo_release_val(__t3274);
  ErgoVal __t3275 = YV_NULLV;
  ergo_release_val(__t3275);
  ErgoVal __t3276 = self; ergo_retain_val(__t3276);
  ergo_move_into(&__ret, __t3276);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomSheet_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3277 = self; ergo_retain_val(__t3277);
  ErgoVal __t3278 = a0; ergo_retain_val(__t3278);
  ErgoVal __t3279 = a1; ergo_retain_val(__t3279);
  ErgoVal __t3280 = a2; ergo_retain_val(__t3280);
  ErgoVal __t3281 = a3; ergo_retain_val(__t3281);
  __cogito_container_set_padding(__t3277, __t3278, __t3279, __t3280, __t3281);
  ergo_release_val(__t3277);
  ergo_release_val(__t3278);
  ergo_release_val(__t3279);
  ergo_release_val(__t3280);
  ergo_release_val(__t3281);
  ErgoVal __t3282 = YV_NULLV;
  ergo_release_val(__t3282);
  ErgoVal __t3283 = self; ergo_retain_val(__t3283);
  ergo_move_into(&__ret, __t3283);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomSheet_set_gap(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3284 = self; ergo_retain_val(__t3284);
  ErgoVal __t3285 = a0; ergo_retain_val(__t3285);
  __cogito_container_set_gap(__t3284, __t3285);
  ergo_release_val(__t3284);
  ergo_release_val(__t3285);
  ErgoVal __t3286 = YV_NULLV;
  ergo_release_val(__t3286);
  ErgoVal __t3287 = self; ergo_retain_val(__t3287);
  ergo_move_into(&__ret, __t3287);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomSheet_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3288 = self; ergo_retain_val(__t3288);
  ErgoVal __t3289 = a0; ergo_retain_val(__t3289);
  __cogito_build(__t3288, __t3289);
  ergo_release_val(__t3288);
  ergo_release_val(__t3289);
  ErgoVal __t3290 = YV_NULLV;
  ergo_release_val(__t3290);
  ErgoVal __t3291 = self; ergo_retain_val(__t3291);
  ergo_move_into(&__ret, __t3291);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomSheet_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3292 = self; ergo_retain_val(__t3292);
  ErgoVal __t3293 = a0; ergo_retain_val(__t3293);
  __cogito_container_set_hexpand(__t3292, __t3293);
  ergo_release_val(__t3292);
  ergo_release_val(__t3293);
  ErgoVal __t3294 = YV_NULLV;
  ergo_release_val(__t3294);
  ErgoVal __t3295 = self; ergo_retain_val(__t3295);
  ergo_move_into(&__ret, __t3295);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomSheet_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3296 = self; ergo_retain_val(__t3296);
  ErgoVal __t3297 = a0; ergo_retain_val(__t3297);
  __cogito_container_set_vexpand(__t3296, __t3297);
  ergo_release_val(__t3296);
  ergo_release_val(__t3297);
  ErgoVal __t3298 = YV_NULLV;
  ergo_release_val(__t3298);
  ErgoVal __t3299 = self; ergo_retain_val(__t3299);
  ergo_move_into(&__ret, __t3299);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomSheet_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3300 = self; ergo_retain_val(__t3300);
  ErgoVal __t3301 = a0; ergo_retain_val(__t3301);
  __cogito_node_set_class(__t3300, __t3301);
  ergo_release_val(__t3300);
  ergo_release_val(__t3301);
  ErgoVal __t3302 = YV_NULLV;
  ergo_release_val(__t3302);
  ErgoVal __t3303 = self; ergo_retain_val(__t3303);
  ergo_move_into(&__ret, __t3303);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomSheet_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3304 = self; ergo_retain_val(__t3304);
  ErgoVal __t3305 = a0; ergo_retain_val(__t3305);
  __cogito_node_set_id(__t3304, __t3305);
  ergo_release_val(__t3304);
  ergo_release_val(__t3305);
  ErgoVal __t3306 = YV_NULLV;
  ergo_release_val(__t3306);
  ErgoVal __t3307 = self; ergo_retain_val(__t3307);
  ergo_move_into(&__ret, __t3307);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SideSheet_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3308 = self; ergo_retain_val(__t3308);
  ErgoVal __t3309 = a0; ergo_retain_val(__t3309);
  __cogito_container_add(__t3308, __t3309);
  ergo_release_val(__t3308);
  ergo_release_val(__t3309);
  ErgoVal __t3310 = YV_NULLV;
  ergo_release_val(__t3310);
  ErgoVal __t3311 = self; ergo_retain_val(__t3311);
  ergo_move_into(&__ret, __t3311);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SideSheet_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3312 = self; ergo_retain_val(__t3312);
  ErgoVal __t3313 = a0; ergo_retain_val(__t3313);
  ErgoVal __t3314 = a1; ergo_retain_val(__t3314);
  ErgoVal __t3315 = a2; ergo_retain_val(__t3315);
  ErgoVal __t3316 = a3; ergo_retain_val(__t3316);
  __cogito_container_set_margins(__t3312, __t3313, __t3314, __t3315, __t3316);
  ergo_release_val(__t3312);
  ergo_release_val(__t3313);
  ergo_release_val(__t3314);
  ergo_release_val(__t3315);
  ergo_release_val(__t3316);
  ErgoVal __t3317 = YV_NULLV;
  ergo_release_val(__t3317);
  ErgoVal __t3318 = self; ergo_retain_val(__t3318);
  ergo_move_into(&__ret, __t3318);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SideSheet_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3319 = self; ergo_retain_val(__t3319);
  ErgoVal __t3320 = a0; ergo_retain_val(__t3320);
  ErgoVal __t3321 = a1; ergo_retain_val(__t3321);
  ErgoVal __t3322 = a2; ergo_retain_val(__t3322);
  ErgoVal __t3323 = a3; ergo_retain_val(__t3323);
  __cogito_container_set_padding(__t3319, __t3320, __t3321, __t3322, __t3323);
  ergo_release_val(__t3319);
  ergo_release_val(__t3320);
  ergo_release_val(__t3321);
  ergo_release_val(__t3322);
  ergo_release_val(__t3323);
  ErgoVal __t3324 = YV_NULLV;
  ergo_release_val(__t3324);
  ErgoVal __t3325 = self; ergo_retain_val(__t3325);
  ergo_move_into(&__ret, __t3325);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SideSheet_set_gap(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3326 = self; ergo_retain_val(__t3326);
  ErgoVal __t3327 = a0; ergo_retain_val(__t3327);
  __cogito_container_set_gap(__t3326, __t3327);
  ergo_release_val(__t3326);
  ergo_release_val(__t3327);
  ErgoVal __t3328 = YV_NULLV;
  ergo_release_val(__t3328);
  ErgoVal __t3329 = self; ergo_retain_val(__t3329);
  ergo_move_into(&__ret, __t3329);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SideSheet_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3330 = self; ergo_retain_val(__t3330);
  ErgoVal __t3331 = a0; ergo_retain_val(__t3331);
  __cogito_build(__t3330, __t3331);
  ergo_release_val(__t3330);
  ergo_release_val(__t3331);
  ErgoVal __t3332 = YV_NULLV;
  ergo_release_val(__t3332);
  ErgoVal __t3333 = self; ergo_retain_val(__t3333);
  ergo_move_into(&__ret, __t3333);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SideSheet_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3334 = self; ergo_retain_val(__t3334);
  ErgoVal __t3335 = a0; ergo_retain_val(__t3335);
  __cogito_container_set_hexpand(__t3334, __t3335);
  ergo_release_val(__t3334);
  ergo_release_val(__t3335);
  ErgoVal __t3336 = YV_NULLV;
  ergo_release_val(__t3336);
  ErgoVal __t3337 = self; ergo_retain_val(__t3337);
  ergo_move_into(&__ret, __t3337);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SideSheet_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3338 = self; ergo_retain_val(__t3338);
  ErgoVal __t3339 = a0; ergo_retain_val(__t3339);
  __cogito_container_set_vexpand(__t3338, __t3339);
  ergo_release_val(__t3338);
  ergo_release_val(__t3339);
  ErgoVal __t3340 = YV_NULLV;
  ergo_release_val(__t3340);
  ErgoVal __t3341 = self; ergo_retain_val(__t3341);
  ergo_move_into(&__ret, __t3341);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SideSheet_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3342 = self; ergo_retain_val(__t3342);
  ErgoVal __t3343 = a0; ergo_retain_val(__t3343);
  __cogito_node_set_class(__t3342, __t3343);
  ergo_release_val(__t3342);
  ergo_release_val(__t3343);
  ErgoVal __t3344 = YV_NULLV;
  ergo_release_val(__t3344);
  ErgoVal __t3345 = self; ergo_retain_val(__t3345);
  ergo_move_into(&__ret, __t3345);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SideSheet_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3346 = self; ergo_retain_val(__t3346);
  ErgoVal __t3347 = a0; ergo_retain_val(__t3347);
  __cogito_node_set_id(__t3346, __t3347);
  ergo_release_val(__t3346);
  ergo_release_val(__t3347);
  ErgoVal __t3348 = YV_NULLV;
  ergo_release_val(__t3348);
  ErgoVal __t3349 = self; ergo_retain_val(__t3349);
  ergo_move_into(&__ret, __t3349);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SideSheet_set_mode(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3350 = self; ergo_retain_val(__t3350);
  ErgoVal __t3351 = a0; ergo_retain_val(__t3351);
  __cogito_side_sheet_set_mode(__t3350, __t3351);
  ergo_release_val(__t3350);
  ergo_release_val(__t3351);
  ErgoVal __t3352 = YV_NULLV;
  ergo_release_val(__t3352);
  ErgoVal __t3353 = self; ergo_retain_val(__t3353);
  ergo_move_into(&__ret, __t3353);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TimePicker_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3354 = self; ergo_retain_val(__t3354);
  ErgoVal __t3355 = a0; ergo_retain_val(__t3355);
  ErgoVal __t3356 = a1; ergo_retain_val(__t3356);
  ErgoVal __t3357 = a2; ergo_retain_val(__t3357);
  ErgoVal __t3358 = a3; ergo_retain_val(__t3358);
  __cogito_container_set_margins(__t3354, __t3355, __t3356, __t3357, __t3358);
  ergo_release_val(__t3354);
  ergo_release_val(__t3355);
  ergo_release_val(__t3356);
  ergo_release_val(__t3357);
  ergo_release_val(__t3358);
  ErgoVal __t3359 = YV_NULLV;
  ergo_release_val(__t3359);
  ErgoVal __t3360 = self; ergo_retain_val(__t3360);
  ergo_move_into(&__ret, __t3360);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TimePicker_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3361 = self; ergo_retain_val(__t3361);
  ErgoVal __t3362 = a0; ergo_retain_val(__t3362);
  ErgoVal __t3363 = a1; ergo_retain_val(__t3363);
  ErgoVal __t3364 = a2; ergo_retain_val(__t3364);
  ErgoVal __t3365 = a3; ergo_retain_val(__t3365);
  __cogito_container_set_padding(__t3361, __t3362, __t3363, __t3364, __t3365);
  ergo_release_val(__t3361);
  ergo_release_val(__t3362);
  ergo_release_val(__t3363);
  ergo_release_val(__t3364);
  ergo_release_val(__t3365);
  ErgoVal __t3366 = YV_NULLV;
  ergo_release_val(__t3366);
  ErgoVal __t3367 = self; ergo_retain_val(__t3367);
  ergo_move_into(&__ret, __t3367);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TimePicker_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3368 = self; ergo_retain_val(__t3368);
  ErgoVal __t3369 = a0; ergo_retain_val(__t3369);
  __cogito_container_set_halign(__t3368, __t3369);
  ergo_release_val(__t3368);
  ergo_release_val(__t3369);
  ErgoVal __t3370 = YV_NULLV;
  ergo_release_val(__t3370);
  ErgoVal __t3371 = self; ergo_retain_val(__t3371);
  ergo_move_into(&__ret, __t3371);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TimePicker_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3372 = self; ergo_retain_val(__t3372);
  ErgoVal __t3373 = a0; ergo_retain_val(__t3373);
  __cogito_container_set_valign(__t3372, __t3373);
  ergo_release_val(__t3372);
  ergo_release_val(__t3373);
  ErgoVal __t3374 = YV_NULLV;
  ergo_release_val(__t3374);
  ErgoVal __t3375 = self; ergo_retain_val(__t3375);
  ergo_move_into(&__ret, __t3375);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TimePicker_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3376 = self; ergo_retain_val(__t3376);
  ErgoVal __t3377 = a0; ergo_retain_val(__t3377);
  __cogito_timepicker_on_change(__t3376, __t3377);
  ergo_release_val(__t3376);
  ergo_release_val(__t3377);
  ErgoVal __t3378 = YV_NULLV;
  ergo_release_val(__t3378);
  ErgoVal __t3379 = self; ergo_retain_val(__t3379);
  ergo_move_into(&__ret, __t3379);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TimePicker_hour(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3380 = self; ergo_retain_val(__t3380);
  ErgoVal __t3381 = __cogito_timepicker_get_hour(__t3380);
  ergo_release_val(__t3380);
  ergo_move_into(&__ret, __t3381);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TimePicker_minute(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3382 = self; ergo_retain_val(__t3382);
  ErgoVal __t3383 = __cogito_timepicker_get_minute(__t3382);
  ergo_release_val(__t3382);
  ergo_move_into(&__ret, __t3383);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TimePicker_set_time(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3384 = self; ergo_retain_val(__t3384);
  ErgoVal __t3385 = a0; ergo_retain_val(__t3385);
  ErgoVal __t3386 = a1; ergo_retain_val(__t3386);
  __cogito_timepicker_set_time(__t3384, __t3385, __t3386);
  ergo_release_val(__t3384);
  ergo_release_val(__t3385);
  ergo_release_val(__t3386);
  ErgoVal __t3387 = YV_NULLV;
  ergo_release_val(__t3387);
  ErgoVal __t3388 = self; ergo_retain_val(__t3388);
  ergo_move_into(&__ret, __t3388);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TimePicker_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3389 = self; ergo_retain_val(__t3389);
  ErgoVal __t3390 = a0; ergo_retain_val(__t3390);
  __cogito_container_set_hexpand(__t3389, __t3390);
  ergo_release_val(__t3389);
  ergo_release_val(__t3390);
  ErgoVal __t3391 = YV_NULLV;
  ergo_release_val(__t3391);
  ErgoVal __t3392 = self; ergo_retain_val(__t3392);
  ergo_move_into(&__ret, __t3392);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TimePicker_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3393 = self; ergo_retain_val(__t3393);
  ErgoVal __t3394 = a0; ergo_retain_val(__t3394);
  __cogito_container_set_vexpand(__t3393, __t3394);
  ergo_release_val(__t3393);
  ergo_release_val(__t3394);
  ErgoVal __t3395 = YV_NULLV;
  ergo_release_val(__t3395);
  ErgoVal __t3396 = self; ergo_retain_val(__t3396);
  ergo_move_into(&__ret, __t3396);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TimePicker_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3397 = self; ergo_retain_val(__t3397);
  ErgoVal __t3398 = a0; ergo_retain_val(__t3398);
  __cogito_node_set_disabled(__t3397, __t3398);
  ergo_release_val(__t3397);
  ergo_release_val(__t3398);
  ErgoVal __t3399 = YV_NULLV;
  ergo_release_val(__t3399);
  ErgoVal __t3400 = self; ergo_retain_val(__t3400);
  ergo_move_into(&__ret, __t3400);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TimePicker_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3401 = self; ergo_retain_val(__t3401);
  ErgoVal __t3402 = a0; ergo_retain_val(__t3402);
  __cogito_node_set_class(__t3401, __t3402);
  ergo_release_val(__t3401);
  ergo_release_val(__t3402);
  ErgoVal __t3403 = YV_NULLV;
  ergo_release_val(__t3403);
  ErgoVal __t3404 = self; ergo_retain_val(__t3404);
  ergo_move_into(&__ret, __t3404);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TimePicker_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3405 = self; ergo_retain_val(__t3405);
  ErgoVal __t3406 = a0; ergo_retain_val(__t3406);
  __cogito_node_set_id(__t3405, __t3406);
  ergo_release_val(__t3405);
  ergo_release_val(__t3406);
  ErgoVal __t3407 = YV_NULLV;
  ergo_release_val(__t3407);
  ErgoVal __t3408 = self; ergo_retain_val(__t3408);
  ergo_move_into(&__ret, __t3408);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TreeView_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3409 = self; ergo_retain_val(__t3409);
  ErgoVal __t3410 = a0; ergo_retain_val(__t3410);
  __cogito_container_add(__t3409, __t3410);
  ergo_release_val(__t3409);
  ergo_release_val(__t3410);
  ErgoVal __t3411 = YV_NULLV;
  ergo_release_val(__t3411);
  ErgoVal __t3412 = self; ergo_retain_val(__t3412);
  ergo_move_into(&__ret, __t3412);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TreeView_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3413 = self; ergo_retain_val(__t3413);
  ErgoVal __t3414 = a0; ergo_retain_val(__t3414);
  ErgoVal __t3415 = a1; ergo_retain_val(__t3415);
  ErgoVal __t3416 = a2; ergo_retain_val(__t3416);
  ErgoVal __t3417 = a3; ergo_retain_val(__t3417);
  __cogito_container_set_margins(__t3413, __t3414, __t3415, __t3416, __t3417);
  ergo_release_val(__t3413);
  ergo_release_val(__t3414);
  ergo_release_val(__t3415);
  ergo_release_val(__t3416);
  ergo_release_val(__t3417);
  ErgoVal __t3418 = YV_NULLV;
  ergo_release_val(__t3418);
  ErgoVal __t3419 = self; ergo_retain_val(__t3419);
  ergo_move_into(&__ret, __t3419);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TreeView_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3420 = self; ergo_retain_val(__t3420);
  ErgoVal __t3421 = a0; ergo_retain_val(__t3421);
  ErgoVal __t3422 = a1; ergo_retain_val(__t3422);
  ErgoVal __t3423 = a2; ergo_retain_val(__t3423);
  ErgoVal __t3424 = a3; ergo_retain_val(__t3424);
  __cogito_container_set_padding(__t3420, __t3421, __t3422, __t3423, __t3424);
  ergo_release_val(__t3420);
  ergo_release_val(__t3421);
  ergo_release_val(__t3422);
  ergo_release_val(__t3423);
  ergo_release_val(__t3424);
  ErgoVal __t3425 = YV_NULLV;
  ergo_release_val(__t3425);
  ErgoVal __t3426 = self; ergo_retain_val(__t3426);
  ergo_move_into(&__ret, __t3426);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TreeView_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3427 = self; ergo_retain_val(__t3427);
  ErgoVal __t3428 = a0; ergo_retain_val(__t3428);
  __cogito_container_set_align(__t3427, __t3428);
  ergo_release_val(__t3427);
  ergo_release_val(__t3428);
  ErgoVal __t3429 = YV_NULLV;
  ergo_release_val(__t3429);
  ErgoVal __t3430 = self; ergo_retain_val(__t3430);
  ergo_move_into(&__ret, __t3430);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TreeView_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3431 = self; ergo_retain_val(__t3431);
  ErgoVal __t3432 = a0; ergo_retain_val(__t3432);
  __cogito_container_set_halign(__t3431, __t3432);
  ergo_release_val(__t3431);
  ergo_release_val(__t3432);
  ErgoVal __t3433 = YV_NULLV;
  ergo_release_val(__t3433);
  ErgoVal __t3434 = self; ergo_retain_val(__t3434);
  ergo_move_into(&__ret, __t3434);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TreeView_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3435 = self; ergo_retain_val(__t3435);
  ErgoVal __t3436 = a0; ergo_retain_val(__t3436);
  __cogito_container_set_valign(__t3435, __t3436);
  ergo_release_val(__t3435);
  ergo_release_val(__t3436);
  ErgoVal __t3437 = YV_NULLV;
  ergo_release_val(__t3437);
  ErgoVal __t3438 = self; ergo_retain_val(__t3438);
  ergo_move_into(&__ret, __t3438);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TreeView_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3439 = self; ergo_retain_val(__t3439);
  ErgoVal __t3440 = a0; ergo_retain_val(__t3440);
  __cogito_container_set_hexpand(__t3439, __t3440);
  ergo_release_val(__t3439);
  ergo_release_val(__t3440);
  ErgoVal __t3441 = YV_NULLV;
  ergo_release_val(__t3441);
  ErgoVal __t3442 = self; ergo_retain_val(__t3442);
  ergo_move_into(&__ret, __t3442);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TreeView_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3443 = self; ergo_retain_val(__t3443);
  ErgoVal __t3444 = a0; ergo_retain_val(__t3444);
  __cogito_container_set_vexpand(__t3443, __t3444);
  ergo_release_val(__t3443);
  ergo_release_val(__t3444);
  ErgoVal __t3445 = YV_NULLV;
  ergo_release_val(__t3445);
  ErgoVal __t3446 = self; ergo_retain_val(__t3446);
  ergo_move_into(&__ret, __t3446);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TreeView_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3447 = self; ergo_retain_val(__t3447);
  ErgoVal __t3448 = a0; ergo_retain_val(__t3448);
  __cogito_node_set_class(__t3447, __t3448);
  ergo_release_val(__t3447);
  ergo_release_val(__t3448);
  ErgoVal __t3449 = YV_NULLV;
  ergo_release_val(__t3449);
  ErgoVal __t3450 = self; ergo_retain_val(__t3450);
  ergo_move_into(&__ret, __t3450);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TreeView_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3451 = self; ergo_retain_val(__t3451);
  ErgoVal __t3452 = a0; ergo_retain_val(__t3452);
  __cogito_node_set_id(__t3451, __t3452);
  ergo_release_val(__t3451);
  ergo_release_val(__t3452);
  ErgoVal __t3453 = YV_NULLV;
  ergo_release_val(__t3453);
  ErgoVal __t3454 = self; ergo_retain_val(__t3454);
  ergo_move_into(&__ret, __t3454);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3455 = self; ergo_retain_val(__t3455);
  ErgoVal __t3456 = a0; ergo_retain_val(__t3456);
  ErgoVal __t3457 = a1; ergo_retain_val(__t3457);
  ErgoVal __t3458 = a2; ergo_retain_val(__t3458);
  ErgoVal __t3459 = a3; ergo_retain_val(__t3459);
  __cogito_container_set_margins(__t3455, __t3456, __t3457, __t3458, __t3459);
  ergo_release_val(__t3455);
  ergo_release_val(__t3456);
  ergo_release_val(__t3457);
  ergo_release_val(__t3458);
  ergo_release_val(__t3459);
  ErgoVal __t3460 = YV_NULLV;
  ergo_release_val(__t3460);
  ErgoVal __t3461 = self; ergo_retain_val(__t3461);
  ergo_move_into(&__ret, __t3461);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3462 = self; ergo_retain_val(__t3462);
  ErgoVal __t3463 = a0; ergo_retain_val(__t3463);
  ErgoVal __t3464 = a1; ergo_retain_val(__t3464);
  ErgoVal __t3465 = a2; ergo_retain_val(__t3465);
  ErgoVal __t3466 = a3; ergo_retain_val(__t3466);
  __cogito_container_set_padding(__t3462, __t3463, __t3464, __t3465, __t3466);
  ergo_release_val(__t3462);
  ergo_release_val(__t3463);
  ergo_release_val(__t3464);
  ergo_release_val(__t3465);
  ergo_release_val(__t3466);
  ErgoVal __t3467 = YV_NULLV;
  ergo_release_val(__t3467);
  ErgoVal __t3468 = self; ergo_retain_val(__t3468);
  ergo_move_into(&__ret, __t3468);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3469 = self; ergo_retain_val(__t3469);
  ErgoVal __t3470 = a0; ergo_retain_val(__t3470);
  __cogito_container_set_align(__t3469, __t3470);
  ergo_release_val(__t3469);
  ergo_release_val(__t3470);
  ErgoVal __t3471 = YV_NULLV;
  ergo_release_val(__t3471);
  ErgoVal __t3472 = self; ergo_retain_val(__t3472);
  ergo_move_into(&__ret, __t3472);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3473 = self; ergo_retain_val(__t3473);
  ErgoVal __t3474 = a0; ergo_retain_val(__t3474);
  __cogito_container_set_halign(__t3473, __t3474);
  ergo_release_val(__t3473);
  ergo_release_val(__t3474);
  ErgoVal __t3475 = YV_NULLV;
  ergo_release_val(__t3475);
  ErgoVal __t3476 = self; ergo_retain_val(__t3476);
  ergo_move_into(&__ret, __t3476);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3477 = self; ergo_retain_val(__t3477);
  ErgoVal __t3478 = a0; ergo_retain_val(__t3478);
  __cogito_container_set_valign(__t3477, __t3478);
  ergo_release_val(__t3477);
  ergo_release_val(__t3478);
  ErgoVal __t3479 = YV_NULLV;
  ergo_release_val(__t3479);
  ErgoVal __t3480 = self; ergo_retain_val(__t3480);
  ergo_move_into(&__ret, __t3480);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_set_hex(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3481 = self; ergo_retain_val(__t3481);
  ErgoVal __t3482 = a0; ergo_retain_val(__t3482);
  __cogito_colorpicker_set_hex(__t3481, __t3482);
  ergo_release_val(__t3481);
  ergo_release_val(__t3482);
  ErgoVal __t3483 = YV_NULLV;
  ergo_release_val(__t3483);
  ErgoVal __t3484 = self; ergo_retain_val(__t3484);
  ergo_move_into(&__ret, __t3484);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_hex(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3485 = self; ergo_retain_val(__t3485);
  ErgoVal __t3486 = __cogito_colorpicker_get_hex(__t3485);
  ergo_release_val(__t3485);
  ergo_move_into(&__ret, __t3486);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3487 = self; ergo_retain_val(__t3487);
  ErgoVal __t3488 = a0; ergo_retain_val(__t3488);
  __cogito_colorpicker_on_change(__t3487, __t3488);
  ergo_release_val(__t3487);
  ergo_release_val(__t3488);
  ErgoVal __t3489 = YV_NULLV;
  ergo_release_val(__t3489);
  ErgoVal __t3490 = self; ergo_retain_val(__t3490);
  ergo_move_into(&__ret, __t3490);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_set_a11y_label(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3491 = self; ergo_retain_val(__t3491);
  ErgoVal __t3492 = a0; ergo_retain_val(__t3492);
  __cogito_node_set_a11y_label(__t3491, __t3492);
  ergo_release_val(__t3491);
  ergo_release_val(__t3492);
  ErgoVal __t3493 = YV_NULLV;
  ergo_release_val(__t3493);
  ErgoVal __t3494 = self; ergo_retain_val(__t3494);
  ergo_move_into(&__ret, __t3494);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_set_a11y_role(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3495 = self; ergo_retain_val(__t3495);
  ErgoVal __t3496 = a0; ergo_retain_val(__t3496);
  __cogito_node_set_a11y_role(__t3495, __t3496);
  ergo_release_val(__t3495);
  ergo_release_val(__t3496);
  ErgoVal __t3497 = YV_NULLV;
  ergo_release_val(__t3497);
  ErgoVal __t3498 = self; ergo_retain_val(__t3498);
  ergo_move_into(&__ret, __t3498);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3499 = self; ergo_retain_val(__t3499);
  ErgoVal __t3500 = a0; ergo_retain_val(__t3500);
  __cogito_container_set_hexpand(__t3499, __t3500);
  ergo_release_val(__t3499);
  ergo_release_val(__t3500);
  ErgoVal __t3501 = YV_NULLV;
  ergo_release_val(__t3501);
  ErgoVal __t3502 = self; ergo_retain_val(__t3502);
  ergo_move_into(&__ret, __t3502);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3503 = self; ergo_retain_val(__t3503);
  ErgoVal __t3504 = a0; ergo_retain_val(__t3504);
  __cogito_container_set_vexpand(__t3503, __t3504);
  ergo_release_val(__t3503);
  ergo_release_val(__t3504);
  ErgoVal __t3505 = YV_NULLV;
  ergo_release_val(__t3505);
  ErgoVal __t3506 = self; ergo_retain_val(__t3506);
  ergo_move_into(&__ret, __t3506);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3507 = self; ergo_retain_val(__t3507);
  ErgoVal __t3508 = a0; ergo_retain_val(__t3508);
  __cogito_node_set_disabled(__t3507, __t3508);
  ergo_release_val(__t3507);
  ergo_release_val(__t3508);
  ErgoVal __t3509 = YV_NULLV;
  ergo_release_val(__t3509);
  ErgoVal __t3510 = self; ergo_retain_val(__t3510);
  ergo_move_into(&__ret, __t3510);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3511 = self; ergo_retain_val(__t3511);
  ErgoVal __t3512 = a0; ergo_retain_val(__t3512);
  __cogito_node_set_class(__t3511, __t3512);
  ergo_release_val(__t3511);
  ergo_release_val(__t3512);
  ErgoVal __t3513 = YV_NULLV;
  ergo_release_val(__t3513);
  ErgoVal __t3514 = self; ergo_retain_val(__t3514);
  ergo_move_into(&__ret, __t3514);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ColorPicker_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3515 = self; ergo_retain_val(__t3515);
  ErgoVal __t3516 = a0; ergo_retain_val(__t3516);
  __cogito_node_set_id(__t3515, __t3516);
  ergo_release_val(__t3515);
  ergo_release_val(__t3516);
  ErgoVal __t3517 = YV_NULLV;
  ergo_release_val(__t3517);
  ErgoVal __t3518 = self; ergo_retain_val(__t3518);
  ergo_move_into(&__ret, __t3518);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toasts_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3519 = self; ergo_retain_val(__t3519);
  ErgoVal __t3520 = a0; ergo_retain_val(__t3520);
  __cogito_container_add(__t3519, __t3520);
  ergo_release_val(__t3519);
  ergo_release_val(__t3520);
  ErgoVal __t3521 = YV_NULLV;
  ergo_release_val(__t3521);
  ErgoVal __t3522 = self; ergo_retain_val(__t3522);
  ergo_move_into(&__ret, __t3522);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toasts_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3523 = self; ergo_retain_val(__t3523);
  ErgoVal __t3524 = a0; ergo_retain_val(__t3524);
  ErgoVal __t3525 = a1; ergo_retain_val(__t3525);
  ErgoVal __t3526 = a2; ergo_retain_val(__t3526);
  ErgoVal __t3527 = a3; ergo_retain_val(__t3527);
  __cogito_container_set_margins(__t3523, __t3524, __t3525, __t3526, __t3527);
  ergo_release_val(__t3523);
  ergo_release_val(__t3524);
  ergo_release_val(__t3525);
  ergo_release_val(__t3526);
  ergo_release_val(__t3527);
  ErgoVal __t3528 = YV_NULLV;
  ergo_release_val(__t3528);
  ErgoVal __t3529 = self; ergo_retain_val(__t3529);
  ergo_move_into(&__ret, __t3529);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toasts_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3530 = self; ergo_retain_val(__t3530);
  ErgoVal __t3531 = a0; ergo_retain_val(__t3531);
  ErgoVal __t3532 = a1; ergo_retain_val(__t3532);
  ErgoVal __t3533 = a2; ergo_retain_val(__t3533);
  ErgoVal __t3534 = a3; ergo_retain_val(__t3534);
  __cogito_container_set_padding(__t3530, __t3531, __t3532, __t3533, __t3534);
  ergo_release_val(__t3530);
  ergo_release_val(__t3531);
  ergo_release_val(__t3532);
  ergo_release_val(__t3533);
  ergo_release_val(__t3534);
  ErgoVal __t3535 = YV_NULLV;
  ergo_release_val(__t3535);
  ErgoVal __t3536 = self; ergo_retain_val(__t3536);
  ergo_move_into(&__ret, __t3536);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toasts_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3537 = self; ergo_retain_val(__t3537);
  ErgoVal __t3538 = a0; ergo_retain_val(__t3538);
  __cogito_container_set_align(__t3537, __t3538);
  ergo_release_val(__t3537);
  ergo_release_val(__t3538);
  ErgoVal __t3539 = YV_NULLV;
  ergo_release_val(__t3539);
  ErgoVal __t3540 = self; ergo_retain_val(__t3540);
  ergo_move_into(&__ret, __t3540);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toasts_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3541 = self; ergo_retain_val(__t3541);
  ErgoVal __t3542 = a0; ergo_retain_val(__t3542);
  __cogito_build(__t3541, __t3542);
  ergo_release_val(__t3541);
  ergo_release_val(__t3542);
  ErgoVal __t3543 = YV_NULLV;
  ergo_release_val(__t3543);
  ErgoVal __t3544 = self; ergo_retain_val(__t3544);
  ergo_move_into(&__ret, __t3544);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toasts_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3545 = self; ergo_retain_val(__t3545);
  ErgoVal __t3546 = a0; ergo_retain_val(__t3546);
  __cogito_container_set_hexpand(__t3545, __t3546);
  ergo_release_val(__t3545);
  ergo_release_val(__t3546);
  ErgoVal __t3547 = YV_NULLV;
  ergo_release_val(__t3547);
  ErgoVal __t3548 = self; ergo_retain_val(__t3548);
  ergo_move_into(&__ret, __t3548);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toasts_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3549 = self; ergo_retain_val(__t3549);
  ErgoVal __t3550 = a0; ergo_retain_val(__t3550);
  __cogito_container_set_vexpand(__t3549, __t3550);
  ergo_release_val(__t3549);
  ergo_release_val(__t3550);
  ErgoVal __t3551 = YV_NULLV;
  ergo_release_val(__t3551);
  ErgoVal __t3552 = self; ergo_retain_val(__t3552);
  ergo_move_into(&__ret, __t3552);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toasts_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3553 = self; ergo_retain_val(__t3553);
  ErgoVal __t3554 = a0; ergo_retain_val(__t3554);
  __cogito_node_set_disabled(__t3553, __t3554);
  ergo_release_val(__t3553);
  ergo_release_val(__t3554);
  ErgoVal __t3555 = YV_NULLV;
  ergo_release_val(__t3555);
  ErgoVal __t3556 = self; ergo_retain_val(__t3556);
  ergo_move_into(&__ret, __t3556);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toasts_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3557 = self; ergo_retain_val(__t3557);
  ErgoVal __t3558 = a0; ergo_retain_val(__t3558);
  __cogito_node_set_class(__t3557, __t3558);
  ergo_release_val(__t3557);
  ergo_release_val(__t3558);
  ErgoVal __t3559 = YV_NULLV;
  ergo_release_val(__t3559);
  ErgoVal __t3560 = self; ergo_retain_val(__t3560);
  ergo_move_into(&__ret, __t3560);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toasts_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3561 = self; ergo_retain_val(__t3561);
  ErgoVal __t3562 = a0; ergo_retain_val(__t3562);
  __cogito_node_set_id(__t3561, __t3562);
  ergo_release_val(__t3561);
  ergo_release_val(__t3562);
  ErgoVal __t3563 = YV_NULLV;
  ergo_release_val(__t3563);
  ErgoVal __t3564 = self; ergo_retain_val(__t3564);
  ergo_move_into(&__ret, __t3564);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toast_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3565 = self; ergo_retain_val(__t3565);
  ErgoVal __t3566 = a0; ergo_retain_val(__t3566);
  ErgoVal __t3567 = a1; ergo_retain_val(__t3567);
  ErgoVal __t3568 = a2; ergo_retain_val(__t3568);
  ErgoVal __t3569 = a3; ergo_retain_val(__t3569);
  __cogito_container_set_margins(__t3565, __t3566, __t3567, __t3568, __t3569);
  ergo_release_val(__t3565);
  ergo_release_val(__t3566);
  ergo_release_val(__t3567);
  ergo_release_val(__t3568);
  ergo_release_val(__t3569);
  ErgoVal __t3570 = YV_NULLV;
  ergo_release_val(__t3570);
  ErgoVal __t3571 = self; ergo_retain_val(__t3571);
  ergo_move_into(&__ret, __t3571);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toast_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3572 = self; ergo_retain_val(__t3572);
  ErgoVal __t3573 = a0; ergo_retain_val(__t3573);
  ErgoVal __t3574 = a1; ergo_retain_val(__t3574);
  ErgoVal __t3575 = a2; ergo_retain_val(__t3575);
  ErgoVal __t3576 = a3; ergo_retain_val(__t3576);
  __cogito_container_set_padding(__t3572, __t3573, __t3574, __t3575, __t3576);
  ergo_release_val(__t3572);
  ergo_release_val(__t3573);
  ergo_release_val(__t3574);
  ergo_release_val(__t3575);
  ergo_release_val(__t3576);
  ErgoVal __t3577 = YV_NULLV;
  ergo_release_val(__t3577);
  ErgoVal __t3578 = self; ergo_retain_val(__t3578);
  ergo_move_into(&__ret, __t3578);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toast_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3579 = self; ergo_retain_val(__t3579);
  ErgoVal __t3580 = a0; ergo_retain_val(__t3580);
  __cogito_container_set_align(__t3579, __t3580);
  ergo_release_val(__t3579);
  ergo_release_val(__t3580);
  ErgoVal __t3581 = YV_NULLV;
  ergo_release_val(__t3581);
  ErgoVal __t3582 = self; ergo_retain_val(__t3582);
  ergo_move_into(&__ret, __t3582);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toast_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3583 = self; ergo_retain_val(__t3583);
  ErgoVal __t3584 = a0; ergo_retain_val(__t3584);
  __cogito_container_set_halign(__t3583, __t3584);
  ergo_release_val(__t3583);
  ergo_release_val(__t3584);
  ErgoVal __t3585 = YV_NULLV;
  ergo_release_val(__t3585);
  ErgoVal __t3586 = self; ergo_retain_val(__t3586);
  ergo_move_into(&__ret, __t3586);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toast_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3587 = self; ergo_retain_val(__t3587);
  ErgoVal __t3588 = a0; ergo_retain_val(__t3588);
  __cogito_container_set_valign(__t3587, __t3588);
  ergo_release_val(__t3587);
  ergo_release_val(__t3588);
  ErgoVal __t3589 = YV_NULLV;
  ergo_release_val(__t3589);
  ErgoVal __t3590 = self; ergo_retain_val(__t3590);
  ergo_move_into(&__ret, __t3590);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toast_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3591 = self; ergo_retain_val(__t3591);
  ErgoVal __t3592 = a0; ergo_retain_val(__t3592);
  __cogito_toast_set_text(__t3591, __t3592);
  ergo_release_val(__t3591);
  ergo_release_val(__t3592);
  ErgoVal __t3593 = YV_NULLV;
  ergo_release_val(__t3593);
  ErgoVal __t3594 = self; ergo_retain_val(__t3594);
  ergo_move_into(&__ret, __t3594);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toast_on_click(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3595 = self; ergo_retain_val(__t3595);
  ErgoVal __t3596 = a0; ergo_retain_val(__t3596);
  __cogito_toast_on_click(__t3595, __t3596);
  ergo_release_val(__t3595);
  ergo_release_val(__t3596);
  ErgoVal __t3597 = YV_NULLV;
  ergo_release_val(__t3597);
  ErgoVal __t3598 = self; ergo_retain_val(__t3598);
  ergo_move_into(&__ret, __t3598);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toast_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3599 = self; ergo_retain_val(__t3599);
  ErgoVal __t3600 = a0; ergo_retain_val(__t3600);
  __cogito_container_set_hexpand(__t3599, __t3600);
  ergo_release_val(__t3599);
  ergo_release_val(__t3600);
  ErgoVal __t3601 = YV_NULLV;
  ergo_release_val(__t3601);
  ErgoVal __t3602 = self; ergo_retain_val(__t3602);
  ergo_move_into(&__ret, __t3602);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toast_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3603 = self; ergo_retain_val(__t3603);
  ErgoVal __t3604 = a0; ergo_retain_val(__t3604);
  __cogito_container_set_vexpand(__t3603, __t3604);
  ergo_release_val(__t3603);
  ergo_release_val(__t3604);
  ErgoVal __t3605 = YV_NULLV;
  ergo_release_val(__t3605);
  ErgoVal __t3606 = self; ergo_retain_val(__t3606);
  ergo_move_into(&__ret, __t3606);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toast_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3607 = self; ergo_retain_val(__t3607);
  ErgoVal __t3608 = a0; ergo_retain_val(__t3608);
  __cogito_node_set_disabled(__t3607, __t3608);
  ergo_release_val(__t3607);
  ergo_release_val(__t3608);
  ErgoVal __t3609 = YV_NULLV;
  ergo_release_val(__t3609);
  ErgoVal __t3610 = self; ergo_retain_val(__t3610);
  ergo_move_into(&__ret, __t3610);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toast_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3611 = self; ergo_retain_val(__t3611);
  ErgoVal __t3612 = a0; ergo_retain_val(__t3612);
  __cogito_node_set_class(__t3611, __t3612);
  ergo_release_val(__t3611);
  ergo_release_val(__t3612);
  ErgoVal __t3613 = YV_NULLV;
  ergo_release_val(__t3613);
  ErgoVal __t3614 = self; ergo_retain_val(__t3614);
  ergo_move_into(&__ret, __t3614);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toast_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3615 = self; ergo_retain_val(__t3615);
  ErgoVal __t3616 = a0; ergo_retain_val(__t3616);
  __cogito_node_set_id(__t3615, __t3616);
  ergo_release_val(__t3615);
  ergo_release_val(__t3616);
  ErgoVal __t3617 = YV_NULLV;
  ergo_release_val(__t3617);
  ErgoVal __t3618 = self; ergo_retain_val(__t3618);
  ergo_move_into(&__ret, __t3618);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toolbar_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3619 = self; ergo_retain_val(__t3619);
  ErgoVal __t3620 = a0; ergo_retain_val(__t3620);
  __cogito_container_add(__t3619, __t3620);
  ergo_release_val(__t3619);
  ergo_release_val(__t3620);
  ErgoVal __t3621 = YV_NULLV;
  ergo_release_val(__t3621);
  ErgoVal __t3622 = self; ergo_retain_val(__t3622);
  ergo_move_into(&__ret, __t3622);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toolbar_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3623 = self; ergo_retain_val(__t3623);
  ErgoVal __t3624 = a0; ergo_retain_val(__t3624);
  __cogito_container_set_hexpand(__t3623, __t3624);
  ergo_release_val(__t3623);
  ergo_release_val(__t3624);
  ErgoVal __t3625 = YV_NULLV;
  ergo_release_val(__t3625);
  ErgoVal __t3626 = self; ergo_retain_val(__t3626);
  ergo_move_into(&__ret, __t3626);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toolbar_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3627 = self; ergo_retain_val(__t3627);
  ErgoVal __t3628 = a0; ergo_retain_val(__t3628);
  __cogito_container_set_vexpand(__t3627, __t3628);
  ergo_release_val(__t3627);
  ergo_release_val(__t3628);
  ErgoVal __t3629 = YV_NULLV;
  ergo_release_val(__t3629);
  ErgoVal __t3630 = self; ergo_retain_val(__t3630);
  ergo_move_into(&__ret, __t3630);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toolbar_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3631 = self; ergo_retain_val(__t3631);
  ErgoVal __t3632 = a0; ergo_retain_val(__t3632);
  __cogito_node_set_class(__t3631, __t3632);
  ergo_release_val(__t3631);
  ergo_release_val(__t3632);
  ErgoVal __t3633 = YV_NULLV;
  ergo_release_val(__t3633);
  ErgoVal __t3634 = self; ergo_retain_val(__t3634);
  ergo_move_into(&__ret, __t3634);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toolbar_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3635 = self; ergo_retain_val(__t3635);
  ErgoVal __t3636 = a0; ergo_retain_val(__t3636);
  __cogito_node_set_id(__t3635, __t3636);
  ergo_release_val(__t3635);
  ergo_release_val(__t3636);
  ErgoVal __t3637 = YV_NULLV;
  ergo_release_val(__t3637);
  ErgoVal __t3638 = self; ergo_retain_val(__t3638);
  ergo_move_into(&__ret, __t3638);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toolbar_set_vibrant(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3639 = self; ergo_retain_val(__t3639);
  ErgoVal __t3640 = a0; ergo_retain_val(__t3640);
  __cogito_toolbar_set_vibrant(__t3639, __t3640);
  ergo_release_val(__t3639);
  ergo_release_val(__t3640);
  ErgoVal __t3641 = YV_NULLV;
  ergo_release_val(__t3641);
  ErgoVal __t3642 = self; ergo_retain_val(__t3642);
  ergo_move_into(&__ret, __t3642);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toolbar_vibrant(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3643 = self; ergo_retain_val(__t3643);
  ErgoVal __t3644 = __cogito_toolbar_get_vibrant(__t3643);
  ergo_release_val(__t3643);
  ergo_move_into(&__ret, __t3644);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toolbar_set_vertical(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3645 = self; ergo_retain_val(__t3645);
  ErgoVal __t3646 = a0; ergo_retain_val(__t3646);
  __cogito_toolbar_set_vertical(__t3645, __t3646);
  ergo_release_val(__t3645);
  ergo_release_val(__t3646);
  ErgoVal __t3647 = YV_NULLV;
  ergo_release_val(__t3647);
  ErgoVal __t3648 = self; ergo_retain_val(__t3648);
  ergo_move_into(&__ret, __t3648);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Toolbar_vertical(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3649 = self; ergo_retain_val(__t3649);
  ErgoVal __t3650 = __cogito_toolbar_get_vertical(__t3649);
  ergo_release_val(__t3649);
  ergo_move_into(&__ret, __t3650);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3651 = self; ergo_retain_val(__t3651);
  ErgoVal __t3652 = a0; ergo_retain_val(__t3652);
  ErgoVal __t3653 = a1; ergo_retain_val(__t3653);
  ErgoVal __t3654 = a2; ergo_retain_val(__t3654);
  ErgoVal __t3655 = a3; ergo_retain_val(__t3655);
  __cogito_container_set_margins(__t3651, __t3652, __t3653, __t3654, __t3655);
  ergo_release_val(__t3651);
  ergo_release_val(__t3652);
  ergo_release_val(__t3653);
  ergo_release_val(__t3654);
  ergo_release_val(__t3655);
  ErgoVal __t3656 = YV_NULLV;
  ergo_release_val(__t3656);
  ErgoVal __t3657 = self; ergo_retain_val(__t3657);
  ergo_move_into(&__ret, __t3657);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3658 = self; ergo_retain_val(__t3658);
  ErgoVal __t3659 = a0; ergo_retain_val(__t3659);
  ErgoVal __t3660 = a1; ergo_retain_val(__t3660);
  ErgoVal __t3661 = a2; ergo_retain_val(__t3661);
  ErgoVal __t3662 = a3; ergo_retain_val(__t3662);
  __cogito_container_set_padding(__t3658, __t3659, __t3660, __t3661, __t3662);
  ergo_release_val(__t3658);
  ergo_release_val(__t3659);
  ergo_release_val(__t3660);
  ergo_release_val(__t3661);
  ergo_release_val(__t3662);
  ErgoVal __t3663 = YV_NULLV;
  ergo_release_val(__t3663);
  ErgoVal __t3664 = self; ergo_retain_val(__t3664);
  ergo_move_into(&__ret, __t3664);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3665 = self; ergo_retain_val(__t3665);
  ErgoVal __t3666 = a0; ergo_retain_val(__t3666);
  __cogito_container_set_align(__t3665, __t3666);
  ergo_release_val(__t3665);
  ergo_release_val(__t3666);
  ErgoVal __t3667 = YV_NULLV;
  ergo_release_val(__t3667);
  ErgoVal __t3668 = self; ergo_retain_val(__t3668);
  ergo_move_into(&__ret, __t3668);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3669 = self; ergo_retain_val(__t3669);
  ErgoVal __t3670 = a0; ergo_retain_val(__t3670);
  __cogito_container_set_halign(__t3669, __t3670);
  ergo_release_val(__t3669);
  ergo_release_val(__t3670);
  ErgoVal __t3671 = YV_NULLV;
  ergo_release_val(__t3671);
  ErgoVal __t3672 = self; ergo_retain_val(__t3672);
  ergo_move_into(&__ret, __t3672);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3673 = self; ergo_retain_val(__t3673);
  ErgoVal __t3674 = a0; ergo_retain_val(__t3674);
  __cogito_container_set_valign(__t3673, __t3674);
  ergo_release_val(__t3673);
  ergo_release_val(__t3674);
  ErgoVal __t3675 = YV_NULLV;
  ergo_release_val(__t3675);
  ErgoVal __t3676 = self; ergo_retain_val(__t3676);
  ergo_move_into(&__ret, __t3676);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3677 = self; ergo_retain_val(__t3677);
  ErgoVal __t3678 = a0; ergo_retain_val(__t3678);
  __cogito_container_set_hexpand(__t3677, __t3678);
  ergo_release_val(__t3677);
  ergo_release_val(__t3678);
  ErgoVal __t3679 = YV_NULLV;
  ergo_release_val(__t3679);
  ErgoVal __t3680 = self; ergo_retain_val(__t3680);
  ergo_move_into(&__ret, __t3680);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3681 = self; ergo_retain_val(__t3681);
  ErgoVal __t3682 = a0; ergo_retain_val(__t3682);
  __cogito_container_set_vexpand(__t3681, __t3682);
  ergo_release_val(__t3681);
  ergo_release_val(__t3682);
  ErgoVal __t3683 = YV_NULLV;
  ergo_release_val(__t3683);
  ErgoVal __t3684 = self; ergo_retain_val(__t3684);
  ergo_move_into(&__ret, __t3684);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_set_selected(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3685 = self; ergo_retain_val(__t3685);
  ErgoVal __t3686 = a0; ergo_retain_val(__t3686);
  __cogito_chip_set_selected(__t3685, __t3686);
  ergo_release_val(__t3685);
  ergo_release_val(__t3686);
  ErgoVal __t3687 = YV_NULLV;
  ergo_release_val(__t3687);
  ErgoVal __t3688 = self; ergo_retain_val(__t3688);
  ergo_move_into(&__ret, __t3688);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_selected(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3689 = self; ergo_retain_val(__t3689);
  ErgoVal __t3690 = __cogito_chip_get_selected(__t3689);
  ergo_release_val(__t3689);
  ergo_move_into(&__ret, __t3690);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_set_closable(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3691 = self; ergo_retain_val(__t3691);
  ErgoVal __t3692 = a0; ergo_retain_val(__t3692);
  __cogito_chip_set_closable(__t3691, __t3692);
  ergo_release_val(__t3691);
  ergo_release_val(__t3692);
  ErgoVal __t3693 = YV_NULLV;
  ergo_release_val(__t3693);
  ErgoVal __t3694 = self; ergo_retain_val(__t3694);
  ergo_move_into(&__ret, __t3694);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_on_click(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3695 = self; ergo_retain_val(__t3695);
  ErgoVal __t3696 = a0; ergo_retain_val(__t3696);
  __cogito_chip_on_click(__t3695, __t3696);
  ergo_release_val(__t3695);
  ergo_release_val(__t3696);
  ErgoVal __t3697 = YV_NULLV;
  ergo_release_val(__t3697);
  ErgoVal __t3698 = self; ergo_retain_val(__t3698);
  ergo_move_into(&__ret, __t3698);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_on_close(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3699 = self; ergo_retain_val(__t3699);
  ErgoVal __t3700 = a0; ergo_retain_val(__t3700);
  __cogito_chip_on_close(__t3699, __t3700);
  ergo_release_val(__t3699);
  ergo_release_val(__t3700);
  ErgoVal __t3701 = YV_NULLV;
  ergo_release_val(__t3701);
  ErgoVal __t3702 = self; ergo_retain_val(__t3702);
  ergo_move_into(&__ret, __t3702);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3703 = self; ergo_retain_val(__t3703);
  ErgoVal __t3704 = a0; ergo_retain_val(__t3704);
  __cogito_node_set_disabled(__t3703, __t3704);
  ergo_release_val(__t3703);
  ergo_release_val(__t3704);
  ErgoVal __t3705 = YV_NULLV;
  ergo_release_val(__t3705);
  ErgoVal __t3706 = self; ergo_retain_val(__t3706);
  ergo_move_into(&__ret, __t3706);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3707 = self; ergo_retain_val(__t3707);
  ErgoVal __t3708 = a0; ergo_retain_val(__t3708);
  __cogito_node_set_class(__t3707, __t3708);
  ergo_release_val(__t3707);
  ergo_release_val(__t3708);
  ErgoVal __t3709 = YV_NULLV;
  ergo_release_val(__t3709);
  ErgoVal __t3710 = self; ergo_retain_val(__t3710);
  ergo_move_into(&__ret, __t3710);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Chip_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3711 = self; ergo_retain_val(__t3711);
  ErgoVal __t3712 = a0; ergo_retain_val(__t3712);
  __cogito_node_set_id(__t3711, __t3712);
  ergo_release_val(__t3711);
  ergo_release_val(__t3712);
  ErgoVal __t3713 = YV_NULLV;
  ergo_release_val(__t3713);
  ErgoVal __t3714 = self; ergo_retain_val(__t3714);
  ergo_move_into(&__ret, __t3714);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3715 = self; ergo_retain_val(__t3715);
  ErgoVal __t3716 = a0; ergo_retain_val(__t3716);
  ErgoVal __t3717 = a1; ergo_retain_val(__t3717);
  ErgoVal __t3718 = a2; ergo_retain_val(__t3718);
  ErgoVal __t3719 = a3; ergo_retain_val(__t3719);
  __cogito_container_set_margins(__t3715, __t3716, __t3717, __t3718, __t3719);
  ergo_release_val(__t3715);
  ergo_release_val(__t3716);
  ergo_release_val(__t3717);
  ergo_release_val(__t3718);
  ergo_release_val(__t3719);
  ErgoVal __t3720 = YV_NULLV;
  ergo_release_val(__t3720);
  ErgoVal __t3721 = self; ergo_retain_val(__t3721);
  ergo_move_into(&__ret, __t3721);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3722 = self; ergo_retain_val(__t3722);
  ErgoVal __t3723 = a0; ergo_retain_val(__t3723);
  ErgoVal __t3724 = a1; ergo_retain_val(__t3724);
  ErgoVal __t3725 = a2; ergo_retain_val(__t3725);
  ErgoVal __t3726 = a3; ergo_retain_val(__t3726);
  __cogito_container_set_padding(__t3722, __t3723, __t3724, __t3725, __t3726);
  ergo_release_val(__t3722);
  ergo_release_val(__t3723);
  ergo_release_val(__t3724);
  ergo_release_val(__t3725);
  ergo_release_val(__t3726);
  ErgoVal __t3727 = YV_NULLV;
  ergo_release_val(__t3727);
  ErgoVal __t3728 = self; ergo_retain_val(__t3728);
  ergo_move_into(&__ret, __t3728);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3729 = self; ergo_retain_val(__t3729);
  ErgoVal __t3730 = a0; ergo_retain_val(__t3730);
  __cogito_container_set_align(__t3729, __t3730);
  ergo_release_val(__t3729);
  ergo_release_val(__t3730);
  ErgoVal __t3731 = YV_NULLV;
  ergo_release_val(__t3731);
  ErgoVal __t3732 = self; ergo_retain_val(__t3732);
  ergo_move_into(&__ret, __t3732);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3733 = self; ergo_retain_val(__t3733);
  ErgoVal __t3734 = a0; ergo_retain_val(__t3734);
  __cogito_container_set_halign(__t3733, __t3734);
  ergo_release_val(__t3733);
  ergo_release_val(__t3734);
  ErgoVal __t3735 = YV_NULLV;
  ergo_release_val(__t3735);
  ErgoVal __t3736 = self; ergo_retain_val(__t3736);
  ergo_move_into(&__ret, __t3736);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3737 = self; ergo_retain_val(__t3737);
  ErgoVal __t3738 = a0; ergo_retain_val(__t3738);
  __cogito_container_set_valign(__t3737, __t3738);
  ergo_release_val(__t3737);
  ergo_release_val(__t3738);
  ErgoVal __t3739 = YV_NULLV;
  ergo_release_val(__t3739);
  ErgoVal __t3740 = self; ergo_retain_val(__t3740);
  ergo_move_into(&__ret, __t3740);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_align_begin(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3741 = self; ergo_retain_val(__t3741);
  ErgoVal __t3742 = YV_INT(0);
  __cogito_container_set_align(__t3741, __t3742);
  ergo_release_val(__t3741);
  ergo_release_val(__t3742);
  ErgoVal __t3743 = YV_NULLV;
  ergo_release_val(__t3743);
  ErgoVal __t3744 = self; ergo_retain_val(__t3744);
  ergo_move_into(&__ret, __t3744);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_align_center(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3745 = self; ergo_retain_val(__t3745);
  ErgoVal __t3746 = YV_INT(1);
  __cogito_container_set_align(__t3745, __t3746);
  ergo_release_val(__t3745);
  ergo_release_val(__t3746);
  ErgoVal __t3747 = YV_NULLV;
  ergo_release_val(__t3747);
  ErgoVal __t3748 = self; ergo_retain_val(__t3748);
  ergo_move_into(&__ret, __t3748);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_align_end(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3749 = self; ergo_retain_val(__t3749);
  ErgoVal __t3750 = YV_INT(2);
  __cogito_container_set_align(__t3749, __t3750);
  ergo_release_val(__t3749);
  ergo_release_val(__t3750);
  ErgoVal __t3751 = YV_NULLV;
  ergo_release_val(__t3751);
  ErgoVal __t3752 = self; ergo_retain_val(__t3752);
  ergo_move_into(&__ret, __t3752);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3753 = self; ergo_retain_val(__t3753);
  ErgoVal __t3754 = a0; ergo_retain_val(__t3754);
  __cogito_container_set_hexpand(__t3753, __t3754);
  ergo_release_val(__t3753);
  ergo_release_val(__t3754);
  ErgoVal __t3755 = YV_NULLV;
  ergo_release_val(__t3755);
  ErgoVal __t3756 = self; ergo_retain_val(__t3756);
  ergo_move_into(&__ret, __t3756);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3757 = self; ergo_retain_val(__t3757);
  ErgoVal __t3758 = a0; ergo_retain_val(__t3758);
  __cogito_container_set_vexpand(__t3757, __t3758);
  ergo_release_val(__t3757);
  ergo_release_val(__t3758);
  ErgoVal __t3759 = YV_NULLV;
  ergo_release_val(__t3759);
  ErgoVal __t3760 = self; ergo_retain_val(__t3760);
  ergo_move_into(&__ret, __t3760);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_set_extended(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3761 = self; ergo_retain_val(__t3761);
  ErgoVal __t3762 = a0; ergo_retain_val(__t3762);
  ErgoVal __t3763 = a1; ergo_retain_val(__t3763);
  __cogito_fab_set_extended(__t3761, __t3762, __t3763);
  ergo_release_val(__t3761);
  ergo_release_val(__t3762);
  ergo_release_val(__t3763);
  ErgoVal __t3764 = YV_NULLV;
  ergo_release_val(__t3764);
  ErgoVal __t3765 = self; ergo_retain_val(__t3765);
  ergo_move_into(&__ret, __t3765);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_on_click(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3766 = self; ergo_retain_val(__t3766);
  ErgoVal __t3767 = a0; ergo_retain_val(__t3767);
  __cogito_fab_on_click(__t3766, __t3767);
  ergo_release_val(__t3766);
  ergo_release_val(__t3767);
  ErgoVal __t3768 = YV_NULLV;
  ergo_release_val(__t3768);
  ErgoVal __t3769 = self; ergo_retain_val(__t3769);
  ergo_move_into(&__ret, __t3769);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3770 = self; ergo_retain_val(__t3770);
  ErgoVal __t3771 = a0; ergo_retain_val(__t3771);
  __cogito_node_set_disabled(__t3770, __t3771);
  ergo_release_val(__t3770);
  ergo_release_val(__t3771);
  ErgoVal __t3772 = YV_NULLV;
  ergo_release_val(__t3772);
  ErgoVal __t3773 = self; ergo_retain_val(__t3773);
  ergo_move_into(&__ret, __t3773);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3774 = self; ergo_retain_val(__t3774);
  ErgoVal __t3775 = a0; ergo_retain_val(__t3775);
  __cogito_node_set_class(__t3774, __t3775);
  ergo_release_val(__t3774);
  ergo_release_val(__t3775);
  ErgoVal __t3776 = YV_NULLV;
  ergo_release_val(__t3776);
  ErgoVal __t3777 = self; ergo_retain_val(__t3777);
  ergo_move_into(&__ret, __t3777);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_FAB_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3778 = self; ergo_retain_val(__t3778);
  ErgoVal __t3779 = a0; ergo_retain_val(__t3779);
  __cogito_node_set_id(__t3778, __t3779);
  ergo_release_val(__t3778);
  ergo_release_val(__t3779);
  ErgoVal __t3780 = YV_NULLV;
  ergo_release_val(__t3780);
  ErgoVal __t3781 = self; ergo_retain_val(__t3781);
  ergo_move_into(&__ret, __t3781);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3782 = self; ergo_retain_val(__t3782);
  ErgoVal __t3783 = a0; ergo_retain_val(__t3783);
  ErgoVal __t3784 = a1; ergo_retain_val(__t3784);
  ErgoVal __t3785 = a2; ergo_retain_val(__t3785);
  ErgoVal __t3786 = a3; ergo_retain_val(__t3786);
  __cogito_container_set_margins(__t3782, __t3783, __t3784, __t3785, __t3786);
  ergo_release_val(__t3782);
  ergo_release_val(__t3783);
  ergo_release_val(__t3784);
  ergo_release_val(__t3785);
  ergo_release_val(__t3786);
  ErgoVal __t3787 = YV_NULLV;
  ergo_release_val(__t3787);
  ErgoVal __t3788 = self; ergo_retain_val(__t3788);
  ergo_move_into(&__ret, __t3788);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3789 = self; ergo_retain_val(__t3789);
  ErgoVal __t3790 = a0; ergo_retain_val(__t3790);
  ErgoVal __t3791 = a1; ergo_retain_val(__t3791);
  ErgoVal __t3792 = a2; ergo_retain_val(__t3792);
  ErgoVal __t3793 = a3; ergo_retain_val(__t3793);
  __cogito_container_set_padding(__t3789, __t3790, __t3791, __t3792, __t3793);
  ergo_release_val(__t3789);
  ergo_release_val(__t3790);
  ergo_release_val(__t3791);
  ergo_release_val(__t3792);
  ergo_release_val(__t3793);
  ErgoVal __t3794 = YV_NULLV;
  ergo_release_val(__t3794);
  ErgoVal __t3795 = self; ergo_retain_val(__t3795);
  ergo_move_into(&__ret, __t3795);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3796 = self; ergo_retain_val(__t3796);
  ErgoVal __t3797 = a0; ergo_retain_val(__t3797);
  __cogito_container_set_hexpand(__t3796, __t3797);
  ergo_release_val(__t3796);
  ergo_release_val(__t3797);
  ErgoVal __t3798 = YV_NULLV;
  ergo_release_val(__t3798);
  ErgoVal __t3799 = self; ergo_retain_val(__t3799);
  ergo_move_into(&__ret, __t3799);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3800 = self; ergo_retain_val(__t3800);
  ErgoVal __t3801 = a0; ergo_retain_val(__t3801);
  __cogito_container_set_vexpand(__t3800, __t3801);
  ergo_release_val(__t3800);
  ergo_release_val(__t3801);
  ErgoVal __t3802 = YV_NULLV;
  ergo_release_val(__t3802);
  ErgoVal __t3803 = self; ergo_retain_val(__t3803);
  ergo_move_into(&__ret, __t3803);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3804 = self; ergo_retain_val(__t3804);
  ErgoVal __t3805 = a0; ergo_retain_val(__t3805);
  __cogito_container_add(__t3804, __t3805);
  ergo_release_val(__t3804);
  ergo_release_val(__t3805);
  ErgoVal __t3806 = YV_NULLV;
  ergo_release_val(__t3806);
  ErgoVal __t3807 = self; ergo_retain_val(__t3807);
  ergo_move_into(&__ret, __t3807);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_set_items(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3808 = self; ergo_retain_val(__t3808);
  ErgoVal __t3809 = a0; ergo_retain_val(__t3809);
  ErgoVal __t3810 = a1; ergo_retain_val(__t3810);
  __cogito_nav_rail_set_items(__t3808, __t3809, __t3810);
  ergo_release_val(__t3808);
  ergo_release_val(__t3809);
  ergo_release_val(__t3810);
  ErgoVal __t3811 = YV_NULLV;
  ergo_release_val(__t3811);
  ErgoVal __t3812 = self; ergo_retain_val(__t3812);
  ergo_move_into(&__ret, __t3812);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_set_badges(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3813 = self; ergo_retain_val(__t3813);
  ErgoVal __t3814 = a0; ergo_retain_val(__t3814);
  __cogito_nav_rail_set_badges(__t3813, __t3814);
  ergo_release_val(__t3813);
  ergo_release_val(__t3814);
  ErgoVal __t3815 = YV_NULLV;
  ergo_release_val(__t3815);
  ErgoVal __t3816 = self; ergo_retain_val(__t3816);
  ergo_move_into(&__ret, __t3816);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_set_toggle(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3817 = self; ergo_retain_val(__t3817);
  ErgoVal __t3818 = a0; ergo_retain_val(__t3818);
  __cogito_nav_rail_set_toggle(__t3817, __t3818);
  ergo_release_val(__t3817);
  ergo_release_val(__t3818);
  ErgoVal __t3819 = YV_NULLV;
  ergo_release_val(__t3819);
  ErgoVal __t3820 = self; ergo_retain_val(__t3820);
  ergo_move_into(&__ret, __t3820);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_set_selected(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3821 = self; ergo_retain_val(__t3821);
  ErgoVal __t3822 = a0; ergo_retain_val(__t3822);
  __cogito_nav_rail_set_selected(__t3821, __t3822);
  ergo_release_val(__t3821);
  ergo_release_val(__t3822);
  ErgoVal __t3823 = YV_NULLV;
  ergo_release_val(__t3823);
  ErgoVal __t3824 = self; ergo_retain_val(__t3824);
  ergo_move_into(&__ret, __t3824);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_selected(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3825 = self; ergo_retain_val(__t3825);
  ErgoVal __t3826 = __cogito_nav_rail_get_selected(__t3825);
  ergo_release_val(__t3825);
  ergo_move_into(&__ret, __t3826);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3827 = self; ergo_retain_val(__t3827);
  ErgoVal __t3828 = a0; ergo_retain_val(__t3828);
  __cogito_nav_rail_on_change(__t3827, __t3828);
  ergo_release_val(__t3827);
  ergo_release_val(__t3828);
  ErgoVal __t3829 = YV_NULLV;
  ergo_release_val(__t3829);
  ErgoVal __t3830 = self; ergo_retain_val(__t3830);
  ergo_move_into(&__ret, __t3830);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3831 = self; ergo_retain_val(__t3831);
  ErgoVal __t3832 = a0; ergo_retain_val(__t3832);
  __cogito_node_set_disabled(__t3831, __t3832);
  ergo_release_val(__t3831);
  ergo_release_val(__t3832);
  ErgoVal __t3833 = YV_NULLV;
  ergo_release_val(__t3833);
  ErgoVal __t3834 = self; ergo_retain_val(__t3834);
  ergo_move_into(&__ret, __t3834);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3835 = self; ergo_retain_val(__t3835);
  ErgoVal __t3836 = a0; ergo_retain_val(__t3836);
  __cogito_node_set_class(__t3835, __t3836);
  ergo_release_val(__t3835);
  ergo_release_val(__t3836);
  ErgoVal __t3837 = YV_NULLV;
  ergo_release_val(__t3837);
  ErgoVal __t3838 = self; ergo_retain_val(__t3838);
  ergo_move_into(&__ret, __t3838);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_NavRail_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3839 = self; ergo_retain_val(__t3839);
  ErgoVal __t3840 = a0; ergo_retain_val(__t3840);
  __cogito_node_set_id(__t3839, __t3840);
  ergo_release_val(__t3839);
  ergo_release_val(__t3840);
  ErgoVal __t3841 = YV_NULLV;
  ergo_release_val(__t3841);
  ErgoVal __t3842 = self; ergo_retain_val(__t3842);
  ergo_move_into(&__ret, __t3842);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomNav_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3843 = self; ergo_retain_val(__t3843);
  ErgoVal __t3844 = a0; ergo_retain_val(__t3844);
  ErgoVal __t3845 = a1; ergo_retain_val(__t3845);
  ErgoVal __t3846 = a2; ergo_retain_val(__t3846);
  ErgoVal __t3847 = a3; ergo_retain_val(__t3847);
  __cogito_container_set_margins(__t3843, __t3844, __t3845, __t3846, __t3847);
  ergo_release_val(__t3843);
  ergo_release_val(__t3844);
  ergo_release_val(__t3845);
  ergo_release_val(__t3846);
  ergo_release_val(__t3847);
  ErgoVal __t3848 = YV_NULLV;
  ergo_release_val(__t3848);
  ErgoVal __t3849 = self; ergo_retain_val(__t3849);
  ergo_move_into(&__ret, __t3849);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomNav_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3850 = self; ergo_retain_val(__t3850);
  ErgoVal __t3851 = a0; ergo_retain_val(__t3851);
  ErgoVal __t3852 = a1; ergo_retain_val(__t3852);
  ErgoVal __t3853 = a2; ergo_retain_val(__t3853);
  ErgoVal __t3854 = a3; ergo_retain_val(__t3854);
  __cogito_container_set_padding(__t3850, __t3851, __t3852, __t3853, __t3854);
  ergo_release_val(__t3850);
  ergo_release_val(__t3851);
  ergo_release_val(__t3852);
  ergo_release_val(__t3853);
  ergo_release_val(__t3854);
  ErgoVal __t3855 = YV_NULLV;
  ergo_release_val(__t3855);
  ErgoVal __t3856 = self; ergo_retain_val(__t3856);
  ergo_move_into(&__ret, __t3856);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomNav_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3857 = self; ergo_retain_val(__t3857);
  ErgoVal __t3858 = a0; ergo_retain_val(__t3858);
  __cogito_container_set_hexpand(__t3857, __t3858);
  ergo_release_val(__t3857);
  ergo_release_val(__t3858);
  ErgoVal __t3859 = YV_NULLV;
  ergo_release_val(__t3859);
  ErgoVal __t3860 = self; ergo_retain_val(__t3860);
  ergo_move_into(&__ret, __t3860);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomNav_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3861 = self; ergo_retain_val(__t3861);
  ErgoVal __t3862 = a0; ergo_retain_val(__t3862);
  __cogito_container_set_vexpand(__t3861, __t3862);
  ergo_release_val(__t3861);
  ergo_release_val(__t3862);
  ErgoVal __t3863 = YV_NULLV;
  ergo_release_val(__t3863);
  ErgoVal __t3864 = self; ergo_retain_val(__t3864);
  ergo_move_into(&__ret, __t3864);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomNav_set_items(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3865 = self; ergo_retain_val(__t3865);
  ErgoVal __t3866 = a0; ergo_retain_val(__t3866);
  ErgoVal __t3867 = a1; ergo_retain_val(__t3867);
  __cogito_bottom_nav_set_items(__t3865, __t3866, __t3867);
  ergo_release_val(__t3865);
  ergo_release_val(__t3866);
  ergo_release_val(__t3867);
  ErgoVal __t3868 = YV_NULLV;
  ergo_release_val(__t3868);
  ErgoVal __t3869 = self; ergo_retain_val(__t3869);
  ergo_move_into(&__ret, __t3869);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomNav_set_selected(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3870 = self; ergo_retain_val(__t3870);
  ErgoVal __t3871 = a0; ergo_retain_val(__t3871);
  __cogito_bottom_nav_set_selected(__t3870, __t3871);
  ergo_release_val(__t3870);
  ergo_release_val(__t3871);
  ErgoVal __t3872 = YV_NULLV;
  ergo_release_val(__t3872);
  ErgoVal __t3873 = self; ergo_retain_val(__t3873);
  ergo_move_into(&__ret, __t3873);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomNav_selected(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3874 = self; ergo_retain_val(__t3874);
  ErgoVal __t3875 = __cogito_bottom_nav_get_selected(__t3874);
  ergo_release_val(__t3874);
  ergo_move_into(&__ret, __t3875);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomNav_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3876 = self; ergo_retain_val(__t3876);
  ErgoVal __t3877 = a0; ergo_retain_val(__t3877);
  __cogito_bottom_nav_on_change(__t3876, __t3877);
  ergo_release_val(__t3876);
  ergo_release_val(__t3877);
  ErgoVal __t3878 = YV_NULLV;
  ergo_release_val(__t3878);
  ErgoVal __t3879 = self; ergo_retain_val(__t3879);
  ergo_move_into(&__ret, __t3879);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomNav_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3880 = self; ergo_retain_val(__t3880);
  ErgoVal __t3881 = a0; ergo_retain_val(__t3881);
  __cogito_node_set_disabled(__t3880, __t3881);
  ergo_release_val(__t3880);
  ergo_release_val(__t3881);
  ErgoVal __t3882 = YV_NULLV;
  ergo_release_val(__t3882);
  ErgoVal __t3883 = self; ergo_retain_val(__t3883);
  ergo_move_into(&__ret, __t3883);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomNav_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3884 = self; ergo_retain_val(__t3884);
  ErgoVal __t3885 = a0; ergo_retain_val(__t3885);
  __cogito_node_set_class(__t3884, __t3885);
  ergo_release_val(__t3884);
  ergo_release_val(__t3885);
  ErgoVal __t3886 = YV_NULLV;
  ergo_release_val(__t3886);
  ErgoVal __t3887 = self; ergo_retain_val(__t3887);
  ergo_move_into(&__ret, __t3887);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_BottomNav_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3888 = self; ergo_retain_val(__t3888);
  ErgoVal __t3889 = a0; ergo_retain_val(__t3889);
  __cogito_node_set_id(__t3888, __t3889);
  ergo_release_val(__t3888);
  ergo_release_val(__t3889);
  ErgoVal __t3890 = YV_NULLV;
  ergo_release_val(__t3890);
  ErgoVal __t3891 = self; ergo_retain_val(__t3891);
  ergo_move_into(&__ret, __t3891);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_State_get(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3892 = self; ergo_retain_val(__t3892);
  ErgoVal __t3893 = __cogito_state_get(__t3892);
  ergo_release_val(__t3892);
  ergo_move_into(&__ret, __t3893);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_State_set(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3894 = self; ergo_retain_val(__t3894);
  ErgoVal __t3895 = a0; ergo_retain_val(__t3895);
  __cogito_state_set(__t3894, __t3895);
  ergo_release_val(__t3894);
  ergo_release_val(__t3895);
  ErgoVal __t3896 = YV_NULLV;
  ergo_release_val(__t3896);
  ErgoVal __t3897 = self; ergo_retain_val(__t3897);
  ergo_move_into(&__ret, __t3897);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_app(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3898 = __cogito_app();
  ergo_move_into(&__ret, __t3898);
  return __ret;
  return __ret;
}

static void ergo_cogito_load_sum(ErgoVal a0) {
  ErgoVal __t3899 = a0; ergo_retain_val(__t3899);
  __cogito_load_sum(__t3899);
  ergo_release_val(__t3899);
  ErgoVal __t3900 = YV_NULLV;
  ergo_release_val(__t3900);
}

static void ergo_cogito_set_script_dir(ErgoVal a0) {
  ErgoVal __t3901 = a0; ergo_retain_val(__t3901);
  __cogito_set_script_dir(__t3901);
  ergo_release_val(__t3901);
  ErgoVal __t3902 = YV_NULLV;
  ergo_release_val(__t3902);
}

static ErgoVal ergo_cogito_open_url(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3903 = a0; ergo_retain_val(__t3903);
  ErgoVal __t3904 = __cogito_open_url(__t3903);
  ergo_release_val(__t3903);
  ergo_move_into(&__ret, __t3904);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_set_timeout(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3905 = a0; ergo_retain_val(__t3905);
  ErgoVal __t3906 = a1; ergo_retain_val(__t3906);
  ErgoVal __t3907 = __cogito_timer_timeout(__t3905, __t3906);
  ergo_release_val(__t3905);
  ergo_release_val(__t3906);
  ergo_move_into(&__ret, __t3907);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_set_interval(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3908 = a0; ergo_retain_val(__t3908);
  ErgoVal __t3909 = a1; ergo_retain_val(__t3909);
  ErgoVal __t3910 = __cogito_timer_interval(__t3908, __t3909);
  ergo_release_val(__t3908);
  ergo_release_val(__t3909);
  ergo_move_into(&__ret, __t3910);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_set_timeout_for(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3911 = a0; ergo_retain_val(__t3911);
  ErgoVal __t3912 = a1; ergo_retain_val(__t3912);
  ErgoVal __t3913 = a2; ergo_retain_val(__t3913);
  ErgoVal __t3914 = __cogito_timer_timeout_for(__t3911, __t3912, __t3913);
  ergo_release_val(__t3911);
  ergo_release_val(__t3912);
  ergo_release_val(__t3913);
  ergo_move_into(&__ret, __t3914);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_set_interval_for(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3915 = a0; ergo_retain_val(__t3915);
  ErgoVal __t3916 = a1; ergo_retain_val(__t3916);
  ErgoVal __t3917 = a2; ergo_retain_val(__t3917);
  ErgoVal __t3918 = __cogito_timer_interval_for(__t3915, __t3916, __t3917);
  ergo_release_val(__t3915);
  ergo_release_val(__t3916);
  ergo_release_val(__t3917);
  ergo_move_into(&__ret, __t3918);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_clear_timer(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3919 = a0; ergo_retain_val(__t3919);
  ErgoVal __t3920 = __cogito_timer_cancel(__t3919);
  ergo_release_val(__t3919);
  ergo_move_into(&__ret, __t3920);
  return __ret;
  return __ret;
}

static void ergo_cogito_clear_timers_for(ErgoVal a0) {
  ErgoVal __t3921 = a0; ergo_retain_val(__t3921);
  __cogito_timer_cancel_for(__t3921);
  ergo_release_val(__t3921);
  ErgoVal __t3922 = YV_NULLV;
  ergo_release_val(__t3922);
}

static void ergo_cogito_clear_timers(void) {
  __cogito_timer_cancel_all();
  ErgoVal __t3923 = YV_NULLV;
  ergo_release_val(__t3923);
}

static void ergo_cogito_set_class(ErgoVal a0, ErgoVal a1) {
  ErgoVal __t3924 = a0; ergo_retain_val(__t3924);
  ErgoVal __t3925 = a1; ergo_retain_val(__t3925);
  __cogito_node_set_class(__t3924, __t3925);
  ergo_release_val(__t3924);
  ergo_release_val(__t3925);
  ErgoVal __t3926 = YV_NULLV;
  ergo_release_val(__t3926);
}

static ErgoVal ergo_cogito_set_a11y_label(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3927 = a0; ergo_retain_val(__t3927);
  ErgoVal __t3928 = a1; ergo_retain_val(__t3928);
  __cogito_node_set_a11y_label(__t3927, __t3928);
  ergo_release_val(__t3927);
  ergo_release_val(__t3928);
  ErgoVal __t3929 = YV_NULLV;
  ergo_release_val(__t3929);
  ErgoVal __t3930 = a0; ergo_retain_val(__t3930);
  ergo_move_into(&__ret, __t3930);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_set_a11y_role(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3931 = a0; ergo_retain_val(__t3931);
  ErgoVal __t3932 = a1; ergo_retain_val(__t3932);
  __cogito_node_set_a11y_role(__t3931, __t3932);
  ergo_release_val(__t3931);
  ergo_release_val(__t3932);
  ErgoVal __t3933 = YV_NULLV;
  ergo_release_val(__t3933);
  ErgoVal __t3934 = a0; ergo_retain_val(__t3934);
  ergo_move_into(&__ret, __t3934);
  return __ret;
  return __ret;
}

static void ergo_cogito_set_tooltip(ErgoVal a0, ErgoVal a1) {
  ErgoVal __t3935 = a0; ergo_retain_val(__t3935);
  ErgoVal __t3936 = a1; ergo_retain_val(__t3936);
  __cogito_node_set_tooltip(__t3935, __t3936);
  ergo_release_val(__t3935);
  ergo_release_val(__t3936);
  ErgoVal __t3937 = YV_NULLV;
  ergo_release_val(__t3937);
}

static void ergo_cogito_pointer_capture(ErgoVal a0) {
  ErgoVal __t3938 = a0; ergo_retain_val(__t3938);
  __cogito_pointer_capture(__t3938);
  ergo_release_val(__t3938);
  ErgoVal __t3939 = YV_NULLV;
  ergo_release_val(__t3939);
}

static void ergo_cogito_pointer_release(void) {
  __cogito_pointer_release();
  ErgoVal __t3940 = YV_NULLV;
  ergo_release_val(__t3940);
}

static ErgoVal ergo_cogito_window(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3941 = YV_STR(stdr_str_lit("Cogito"));
  ErgoVal __t3942 = YV_NULLV;
  {
    ErgoVal __parts285[1] = { __t3941 };
    ErgoStr* __s286 = stdr_str_from_parts(1, __parts285);
    __t3942 = YV_STR(__s286);
  }
  ergo_release_val(__t3941);
  ErgoVal __t3943 = YV_INT(360);
  ErgoVal __t3944 = YV_INT(296);
  ErgoVal __t3945 = __cogito_window(__t3942, __t3943, __t3944);
  ergo_release_val(__t3942);
  ergo_release_val(__t3943);
  ergo_release_val(__t3944);
  ergo_move_into(&__ret, __t3945);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_window_title(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3946 = a0; ergo_retain_val(__t3946);
  ErgoVal __t3947 = a1; ergo_retain_val(__t3947);
  ErgoVal __t3948 = a2; ergo_retain_val(__t3948);
  ErgoVal __t3949 = __cogito_window(__t3946, __t3947, __t3948);
  ergo_release_val(__t3946);
  ergo_release_val(__t3947);
  ergo_release_val(__t3948);
  ergo_move_into(&__ret, __t3949);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_window_size(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3950 = a0; ergo_retain_val(__t3950);
  ErgoVal __t3951 = a1; ergo_retain_val(__t3951);
  ErgoVal __t3952 = a2; ergo_retain_val(__t3952);
  ErgoVal __t3953 = __cogito_window(__t3950, __t3951, __t3952);
  ergo_release_val(__t3950);
  ergo_release_val(__t3951);
  ergo_release_val(__t3952);
  ergo_move_into(&__ret, __t3953);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_build(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3954 = a0; ergo_retain_val(__t3954);
  ErgoVal __t3955 = a1; ergo_retain_val(__t3955);
  __cogito_build(__t3954, __t3955);
  ergo_release_val(__t3954);
  ergo_release_val(__t3955);
  ErgoVal __t3956 = YV_NULLV;
  ergo_release_val(__t3956);
  ErgoVal __t3957 = a0; ergo_retain_val(__t3957);
  ergo_move_into(&__ret, __t3957);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_state(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3958 = a0; ergo_retain_val(__t3958);
  ErgoVal __t3959 = __cogito_state_new(__t3958);
  ergo_release_val(__t3958);
  ergo_move_into(&__ret, __t3959);
  return __ret;
  return __ret;
}

static void ergo_cogito_set_id(ErgoVal a0, ErgoVal a1) {
  ErgoVal __t3960 = a0; ergo_retain_val(__t3960);
  ErgoVal __t3961 = a1; ergo_retain_val(__t3961);
  __cogito_node_set_id(__t3960, __t3961);
  ergo_release_val(__t3960);
  ergo_release_val(__t3961);
  ErgoVal __t3962 = YV_NULLV;
  ergo_release_val(__t3962);
}

static ErgoVal ergo_cogito_vstack(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3963 = __cogito_vstack();
  ergo_move_into(&__ret, __t3963);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_hstack(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3964 = __cogito_hstack();
  ergo_move_into(&__ret, __t3964);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_zstack(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3965 = __cogito_zstack();
  ergo_move_into(&__ret, __t3965);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_fixed(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3966 = __cogito_fixed();
  ergo_move_into(&__ret, __t3966);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_scroller(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3967 = __cogito_scroller();
  ergo_move_into(&__ret, __t3967);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_carousel(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3968 = __cogito_carousel();
  ergo_move_into(&__ret, __t3968);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_carousel_item(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3969 = __cogito_carousel_item();
  ergo_move_into(&__ret, __t3969);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_list(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3970 = __cogito_list();
  ergo_move_into(&__ret, __t3970);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_grid(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3971 = a0; ergo_retain_val(__t3971);
  ErgoVal __t3972 = __cogito_grid(__t3971);
  ergo_release_val(__t3971);
  ergo_move_into(&__ret, __t3972);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_tabs(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3973 = __cogito_tabs();
  ergo_move_into(&__ret, __t3973);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_buttongroup(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3974 = __cogito_buttongroup();
  ergo_move_into(&__ret, __t3974);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_view_switcher(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3975 = __cogito_view_switcher();
  ergo_move_into(&__ret, __t3975);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_progress(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3976 = a0; ergo_retain_val(__t3976);
  ErgoVal __t3977 = __cogito_progress(__t3976);
  ergo_release_val(__t3976);
  ergo_move_into(&__ret, __t3977);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_divider(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3978 = a0; ergo_retain_val(__t3978);
  ErgoVal __t3979 = a1; ergo_retain_val(__t3979);
  ErgoVal __t3980 = __cogito_divider(__t3978, __t3979);
  ergo_release_val(__t3978);
  ergo_release_val(__t3979);
  ergo_move_into(&__ret, __t3980);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_toasts(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3981 = __cogito_toasts();
  ergo_move_into(&__ret, __t3981);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_toast(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3982 = a0; ergo_retain_val(__t3982);
  ErgoVal __t3983 = __cogito_toast(__t3982);
  ergo_release_val(__t3982);
  ergo_move_into(&__ret, __t3983);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_label(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3984 = a0; ergo_retain_val(__t3984);
  ErgoVal __t3985 = __cogito_label(__t3984);
  ergo_release_val(__t3984);
  ergo_move_into(&__ret, __t3985);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_image(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3986 = a0; ergo_retain_val(__t3986);
  ErgoVal __t3987 = __cogito_image(__t3986);
  ergo_release_val(__t3986);
  ergo_move_into(&__ret, __t3987);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_dialog(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3988 = a0; ergo_retain_val(__t3988);
  ErgoVal __t3989 = __cogito_dialog(__t3988);
  ergo_release_val(__t3988);
  ergo_move_into(&__ret, __t3989);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_dialog_slot(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3990 = __cogito_dialog_slot();
  ergo_move_into(&__ret, __t3990);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_button(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3991 = a0; ergo_retain_val(__t3991);
  ErgoVal __t3992 = __cogito_button(__t3991);
  ergo_release_val(__t3991);
  ergo_move_into(&__ret, __t3992);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_iconbtn(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3993 = a0; ergo_retain_val(__t3993);
  ErgoVal __t3994 = __cogito_iconbtn(__t3993);
  ergo_release_val(__t3993);
  ergo_move_into(&__ret, __t3994);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_appbar(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3995 = a0; ergo_retain_val(__t3995);
  ErgoVal __t3996 = a1; ergo_retain_val(__t3996);
  ErgoVal __t3997 = __cogito_appbar(__t3995, __t3996);
  ergo_release_val(__t3995);
  ergo_release_val(__t3996);
  ergo_move_into(&__ret, __t3997);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_checkbox(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t3998 = a0; ergo_retain_val(__t3998);
  ErgoVal __t3999 = a1; ergo_retain_val(__t3999);
  ErgoVal __t4000 = __cogito_checkbox(__t3998, __t3999);
  ergo_release_val(__t3998);
  ergo_release_val(__t3999);
  ergo_move_into(&__ret, __t4000);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_switch(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4001 = a0; ergo_retain_val(__t4001);
  ErgoVal __t4002 = __cogito_switch(__t4001);
  ergo_release_val(__t4001);
  ergo_move_into(&__ret, __t4002);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_textfield(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4003 = a0; ergo_retain_val(__t4003);
  ErgoVal __t4004 = __cogito_textfield(__t4003);
  ergo_release_val(__t4003);
  ergo_move_into(&__ret, __t4004);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_searchfield(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4005 = a0; ergo_retain_val(__t4005);
  ErgoVal __t4006 = __cogito_searchfield(__t4005);
  ergo_release_val(__t4005);
  ergo_move_into(&__ret, __t4006);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_textview(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4007 = a0; ergo_retain_val(__t4007);
  ErgoVal __t4008 = __cogito_textview(__t4007);
  ergo_release_val(__t4007);
  ergo_move_into(&__ret, __t4008);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_dropdown(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4009 = __cogito_dropdown();
  ergo_move_into(&__ret, __t4009);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_datepicker(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4010 = __cogito_datepicker();
  ergo_move_into(&__ret, __t4010);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_stepper(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4011 = a0; ergo_retain_val(__t4011);
  ErgoVal __t4012 = a1; ergo_retain_val(__t4012);
  ErgoVal __t4013 = a2; ergo_retain_val(__t4013);
  ErgoVal __t4014 = a3; ergo_retain_val(__t4014);
  ErgoVal __t4015 = __cogito_stepper(__t4011, __t4012, __t4013, __t4014);
  ergo_release_val(__t4011);
  ergo_release_val(__t4012);
  ergo_release_val(__t4013);
  ergo_release_val(__t4014);
  ergo_move_into(&__ret, __t4015);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_slider(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4016 = a0; ergo_retain_val(__t4016);
  ErgoVal __t4017 = a1; ergo_retain_val(__t4017);
  ErgoVal __t4018 = a2; ergo_retain_val(__t4018);
  ErgoVal __t4019 = __cogito_slider(__t4016, __t4017, __t4018);
  ergo_release_val(__t4016);
  ergo_release_val(__t4017);
  ergo_release_val(__t4018);
  ergo_move_into(&__ret, __t4019);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_slider_range(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal sl__39 = YV_NULLV;
  ErgoVal __t4020 = a0; ergo_retain_val(__t4020);
  ErgoVal __t4021 = a1; ergo_retain_val(__t4021);
  ErgoVal __t4022 = a2; ergo_retain_val(__t4022);
  ErgoVal __t4023 = __cogito_slider(__t4020, __t4021, __t4022);
  ergo_release_val(__t4020);
  ergo_release_val(__t4021);
  ergo_release_val(__t4022);
  ergo_move_into(&sl__39, __t4023);
  ErgoVal __t4024 = sl__39; ergo_retain_val(__t4024);
  ErgoVal __t4025 = a2; ergo_retain_val(__t4025);
  ErgoVal __t4026 = a3; ergo_retain_val(__t4026);
  __cogito_slider_set_range(__t4024, __t4025, __t4026);
  ergo_release_val(__t4024);
  ergo_release_val(__t4025);
  ergo_release_val(__t4026);
  ErgoVal __t4027 = YV_NULLV;
  ergo_release_val(__t4027);
  ErgoVal __t4028 = sl__39; ergo_retain_val(__t4028);
  ergo_move_into(&__ret, __t4028);
  return __ret;
  ergo_release_val(sl__39);
  return __ret;
}

static ErgoVal ergo_cogito_treeview(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4029 = __cogito_treeview();
  ergo_move_into(&__ret, __t4029);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_colorpicker(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4030 = __cogito_colorpicker();
  ergo_move_into(&__ret, __t4030);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_toolbar(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4031 = __cogito_toolbar();
  ergo_move_into(&__ret, __t4031);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_chip(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4032 = a0; ergo_retain_val(__t4032);
  ErgoVal __t4033 = __cogito_chip(__t4032);
  ergo_release_val(__t4032);
  ergo_move_into(&__ret, __t4033);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_fab(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4034 = a0; ergo_retain_val(__t4034);
  ErgoVal __t4035 = __cogito_fab(__t4034);
  ergo_release_val(__t4034);
  ergo_move_into(&__ret, __t4035);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_nav_rail(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4036 = __cogito_nav_rail();
  ergo_move_into(&__ret, __t4036);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_bottom_nav(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4037 = __cogito_bottom_nav();
  ergo_move_into(&__ret, __t4037);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_CARD_ELEVATED(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4038 = YV_INT(0);
  ergo_move_into(&__ret, __t4038);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_CARD_FILLED(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4039 = YV_INT(1);
  ergo_move_into(&__ret, __t4039);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_CARD_OUTLINED(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4040 = YV_INT(2);
  ergo_move_into(&__ret, __t4040);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_card(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4041 = a0; ergo_retain_val(__t4041);
  ErgoVal __t4042 = __cogito_card(__t4041);
  ergo_release_val(__t4041);
  ergo_move_into(&__ret, __t4042);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_card_untitled(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4043 = YV_NULLV;
  ErgoVal __t4044 = __cogito_card(__t4043);
  ergo_release_val(__t4043);
  ergo_move_into(&__ret, __t4044);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_avatar(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4045 = a0; ergo_retain_val(__t4045);
  ErgoVal __t4046 = __cogito_avatar(__t4045);
  ergo_release_val(__t4045);
  ergo_move_into(&__ret, __t4046);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_badge(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4047 = a0; ergo_retain_val(__t4047);
  ErgoVal __t4048 = __cogito_badge(__t4047);
  ergo_release_val(__t4047);
  ergo_move_into(&__ret, __t4048);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_badge_dot(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4049 = YV_INT(0);
  ErgoVal __t4050 = __cogito_badge(__t4049);
  ergo_release_val(__t4049);
  ergo_move_into(&__ret, __t4050);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_banner(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4051 = a0; ergo_retain_val(__t4051);
  ErgoVal __t4052 = __cogito_banner(__t4051);
  ergo_release_val(__t4051);
  ergo_move_into(&__ret, __t4052);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_bottom_sheet(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4053 = a0; ergo_retain_val(__t4053);
  ErgoVal __t4054 = __cogito_bottom_sheet(__t4053);
  ergo_release_val(__t4053);
  ergo_move_into(&__ret, __t4054);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_bottom_sheet_untitled(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4055 = YV_NULLV;
  ErgoVal __t4056 = __cogito_bottom_sheet(__t4055);
  ergo_release_val(__t4055);
  ergo_move_into(&__ret, __t4056);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_side_sheet(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4057 = a0; ergo_retain_val(__t4057);
  ErgoVal __t4058 = __cogito_side_sheet(__t4057);
  ergo_release_val(__t4057);
  ergo_move_into(&__ret, __t4058);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_side_sheet_untitled(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4059 = YV_NULLV;
  ErgoVal __t4060 = __cogito_side_sheet(__t4059);
  ergo_release_val(__t4059);
  ergo_move_into(&__ret, __t4060);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_timepicker(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4061 = __cogito_timepicker();
  ergo_move_into(&__ret, __t4061);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_find_parent(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4062 = a0; ergo_retain_val(__t4062);
  ErgoVal __t4063 = __cogito_find_parent(__t4062);
  ergo_release_val(__t4062);
  ergo_move_into(&__ret, __t4063);
  return __ret;
  return __ret;
}

static void ergo_cogito_dialog_slot_clear(ErgoVal a0) {
  ErgoVal __t4064 = a0; ergo_retain_val(__t4064);
  __cogito_dialog_slot_clear(__t4064);
  ergo_release_val(__t4064);
  ErgoVal __t4065 = YV_NULLV;
  ergo_release_val(__t4065);
}

static ErgoVal ergo_cogito_find_children(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4066 = a0; ergo_retain_val(__t4066);
  ErgoVal __t4067 = __cogito_find_children(__t4066);
  ergo_release_val(__t4066);
  ergo_move_into(&__ret, __t4067);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_active_indicator(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4068 = __cogito_active_indicator();
  ergo_move_into(&__ret, __t4068);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_switchbar(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4069 = a0; ergo_retain_val(__t4069);
  ErgoVal __t4070 = __cogito_switchbar(__t4069);
  ergo_release_val(__t4069);
  ergo_move_into(&__ret, __t4070);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_drawing_area(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4071 = __cogito_drawing_area();
  ergo_move_into(&__ret, __t4071);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_shape(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4072 = a0; ergo_retain_val(__t4072);
  ErgoVal __t4073 = __cogito_shape(__t4072);
  ergo_release_val(__t4072);
  ergo_move_into(&__ret, __t4073);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_content_list(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4074 = __cogito_content_list();
  ergo_move_into(&__ret, __t4074);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_empty_page(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4075 = a0; ergo_retain_val(__t4075);
  ErgoVal __t4076 = __cogito_empty_page(__t4075);
  ergo_release_val(__t4075);
  ergo_move_into(&__ret, __t4076);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_tip_view(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4077 = a0; ergo_retain_val(__t4077);
  ErgoVal __t4078 = __cogito_tip_view(__t4077);
  ergo_release_val(__t4077);
  ergo_move_into(&__ret, __t4078);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_settings_window(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4079 = a0; ergo_retain_val(__t4079);
  ErgoVal __t4080 = __cogito_settings_window(__t4079);
  ergo_release_val(__t4079);
  ergo_move_into(&__ret, __t4080);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_settings_page(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4081 = a0; ergo_retain_val(__t4081);
  ErgoVal __t4082 = __cogito_settings_page(__t4081);
  ergo_release_val(__t4081);
  ergo_move_into(&__ret, __t4082);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_settings_list(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4083 = a0; ergo_retain_val(__t4083);
  ErgoVal __t4084 = __cogito_settings_list(__t4083);
  ergo_release_val(__t4083);
  ergo_move_into(&__ret, __t4084);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_settings_row(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4085 = a0; ergo_retain_val(__t4085);
  ErgoVal __t4086 = __cogito_settings_row(__t4085);
  ergo_release_val(__t4085);
  ergo_move_into(&__ret, __t4086);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_welcome_screen(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4087 = a0; ergo_retain_val(__t4087);
  ErgoVal __t4088 = __cogito_welcome_screen(__t4087);
  ergo_release_val(__t4087);
  ergo_move_into(&__ret, __t4088);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_view_dual(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4089 = __cogito_view_dual();
  ergo_move_into(&__ret, __t4089);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_view_chooser(void) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4090 = __cogito_view_chooser();
  ergo_move_into(&__ret, __t4090);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_about_window(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4091 = a0; ergo_retain_val(__t4091);
  ErgoVal __t4092 = a1; ergo_retain_val(__t4092);
  ErgoVal __t4093 = __cogito_about_window(__t4091, __t4092);
  ergo_release_val(__t4091);
  ergo_release_val(__t4092);
  ergo_move_into(&__ret, __t4093);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_menu_button(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4094 = a0; ergo_retain_val(__t4094);
  ErgoVal __t4095 = __cogito_menu_button(__t4094);
  ergo_release_val(__t4094);
  ergo_move_into(&__ret, __t4095);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_split_button(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4096 = a0; ergo_retain_val(__t4096);
  ErgoVal __t4097 = __cogito_split_button(__t4096);
  ergo_release_val(__t4096);
  ergo_move_into(&__ret, __t4097);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_fab_menu(ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4098 = a0; ergo_retain_val(__t4098);
  ErgoVal __t4099 = __cogito_fab_menu(__t4098);
  ergo_release_val(__t4098);
  ergo_move_into(&__ret, __t4099);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ActiveIndicator_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4100 = self; ergo_retain_val(__t4100);
  ErgoVal __t4101 = a0; ergo_retain_val(__t4101);
  ErgoVal __t4102 = a1; ergo_retain_val(__t4102);
  ErgoVal __t4103 = a2; ergo_retain_val(__t4103);
  ErgoVal __t4104 = a3; ergo_retain_val(__t4104);
  __cogito_container_set_margins(__t4100, __t4101, __t4102, __t4103, __t4104);
  ergo_release_val(__t4100);
  ergo_release_val(__t4101);
  ergo_release_val(__t4102);
  ergo_release_val(__t4103);
  ergo_release_val(__t4104);
  ErgoVal __t4105 = YV_NULLV;
  ergo_release_val(__t4105);
  ErgoVal __t4106 = self; ergo_retain_val(__t4106);
  ergo_move_into(&__ret, __t4106);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ActiveIndicator_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4107 = self; ergo_retain_val(__t4107);
  ErgoVal __t4108 = a0; ergo_retain_val(__t4108);
  __cogito_node_set_class(__t4107, __t4108);
  ergo_release_val(__t4107);
  ergo_release_val(__t4108);
  ErgoVal __t4109 = YV_NULLV;
  ergo_release_val(__t4109);
  ErgoVal __t4110 = self; ergo_retain_val(__t4110);
  ergo_move_into(&__ret, __t4110);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ActiveIndicator_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4111 = self; ergo_retain_val(__t4111);
  ErgoVal __t4112 = a0; ergo_retain_val(__t4112);
  __cogito_node_set_id(__t4111, __t4112);
  ergo_release_val(__t4111);
  ergo_release_val(__t4112);
  ErgoVal __t4113 = YV_NULLV;
  ergo_release_val(__t4113);
  ErgoVal __t4114 = self; ergo_retain_val(__t4114);
  ergo_move_into(&__ret, __t4114);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SwitchBar_set_checked(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4115 = self; ergo_retain_val(__t4115);
  ErgoVal __t4116 = a0; ergo_retain_val(__t4116);
  __cogito_switchbar_set_checked(__t4115, __t4116);
  ergo_release_val(__t4115);
  ergo_release_val(__t4116);
  ErgoVal __t4117 = YV_NULLV;
  ergo_release_val(__t4117);
  ErgoVal __t4118 = self; ergo_retain_val(__t4118);
  ergo_move_into(&__ret, __t4118);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SwitchBar_get_checked(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4119 = self; ergo_retain_val(__t4119);
  ErgoVal __t4120 = __cogito_switchbar_get_checked(__t4119);
  ergo_release_val(__t4119);
  ergo_move_into(&__ret, __t4120);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SwitchBar_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4121 = self; ergo_retain_val(__t4121);
  ErgoVal __t4122 = a0; ergo_retain_val(__t4122);
  __cogito_switchbar_on_change(__t4121, __t4122);
  ergo_release_val(__t4121);
  ergo_release_val(__t4122);
  ErgoVal __t4123 = YV_NULLV;
  ergo_release_val(__t4123);
  ErgoVal __t4124 = self; ergo_retain_val(__t4124);
  ergo_move_into(&__ret, __t4124);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SwitchBar_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4125 = self; ergo_retain_val(__t4125);
  ErgoVal __t4126 = a0; ergo_retain_val(__t4126);
  __cogito_container_add(__t4125, __t4126);
  ergo_release_val(__t4125);
  ergo_release_val(__t4126);
  ErgoVal __t4127 = YV_NULLV;
  ergo_release_val(__t4127);
  ErgoVal __t4128 = self; ergo_retain_val(__t4128);
  ergo_move_into(&__ret, __t4128);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SwitchBar_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4129 = self; ergo_retain_val(__t4129);
  ErgoVal __t4130 = a0; ergo_retain_val(__t4130);
  __cogito_build(__t4129, __t4130);
  ergo_release_val(__t4129);
  ergo_release_val(__t4130);
  ErgoVal __t4131 = YV_NULLV;
  ergo_release_val(__t4131);
  ErgoVal __t4132 = self; ergo_retain_val(__t4132);
  ergo_move_into(&__ret, __t4132);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SwitchBar_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4133 = self; ergo_retain_val(__t4133);
  ErgoVal __t4134 = a0; ergo_retain_val(__t4134);
  ErgoVal __t4135 = a1; ergo_retain_val(__t4135);
  ErgoVal __t4136 = a2; ergo_retain_val(__t4136);
  ErgoVal __t4137 = a3; ergo_retain_val(__t4137);
  __cogito_container_set_margins(__t4133, __t4134, __t4135, __t4136, __t4137);
  ergo_release_val(__t4133);
  ergo_release_val(__t4134);
  ergo_release_val(__t4135);
  ergo_release_val(__t4136);
  ergo_release_val(__t4137);
  ErgoVal __t4138 = YV_NULLV;
  ergo_release_val(__t4138);
  ErgoVal __t4139 = self; ergo_retain_val(__t4139);
  ergo_move_into(&__ret, __t4139);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SwitchBar_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4140 = self; ergo_retain_val(__t4140);
  ErgoVal __t4141 = a0; ergo_retain_val(__t4141);
  __cogito_container_set_hexpand(__t4140, __t4141);
  ergo_release_val(__t4140);
  ergo_release_val(__t4141);
  ErgoVal __t4142 = YV_NULLV;
  ergo_release_val(__t4142);
  ErgoVal __t4143 = self; ergo_retain_val(__t4143);
  ergo_move_into(&__ret, __t4143);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SwitchBar_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4144 = self; ergo_retain_val(__t4144);
  ErgoVal __t4145 = a0; ergo_retain_val(__t4145);
  __cogito_node_set_class(__t4144, __t4145);
  ergo_release_val(__t4144);
  ergo_release_val(__t4145);
  ErgoVal __t4146 = YV_NULLV;
  ergo_release_val(__t4146);
  ErgoVal __t4147 = self; ergo_retain_val(__t4147);
  ergo_move_into(&__ret, __t4147);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SwitchBar_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4148 = self; ergo_retain_val(__t4148);
  ErgoVal __t4149 = a0; ergo_retain_val(__t4149);
  __cogito_node_set_id(__t4148, __t4149);
  ergo_release_val(__t4148);
  ergo_release_val(__t4149);
  ErgoVal __t4150 = YV_NULLV;
  ergo_release_val(__t4150);
  ErgoVal __t4151 = self; ergo_retain_val(__t4151);
  ergo_move_into(&__ret, __t4151);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_on_press(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4152 = self; ergo_retain_val(__t4152);
  ErgoVal __t4153 = a0; ergo_retain_val(__t4153);
  __cogito_drawing_area_on_press(__t4152, __t4153);
  ergo_release_val(__t4152);
  ergo_release_val(__t4153);
  ErgoVal __t4154 = YV_NULLV;
  ergo_release_val(__t4154);
  ErgoVal __t4155 = self; ergo_retain_val(__t4155);
  ergo_move_into(&__ret, __t4155);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_on_drag(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4156 = self; ergo_retain_val(__t4156);
  ErgoVal __t4157 = a0; ergo_retain_val(__t4157);
  __cogito_drawing_area_on_drag(__t4156, __t4157);
  ergo_release_val(__t4156);
  ergo_release_val(__t4157);
  ErgoVal __t4158 = YV_NULLV;
  ergo_release_val(__t4158);
  ErgoVal __t4159 = self; ergo_retain_val(__t4159);
  ergo_move_into(&__ret, __t4159);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_on_release(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4160 = self; ergo_retain_val(__t4160);
  ErgoVal __t4161 = a0; ergo_retain_val(__t4161);
  __cogito_drawing_area_on_release(__t4160, __t4161);
  ergo_release_val(__t4160);
  ergo_release_val(__t4161);
  ErgoVal __t4162 = YV_NULLV;
  ergo_release_val(__t4162);
  ErgoVal __t4163 = self; ergo_retain_val(__t4163);
  ergo_move_into(&__ret, __t4163);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_on_draw(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4164 = self; ergo_retain_val(__t4164);
  ErgoVal __t4165 = a0; ergo_retain_val(__t4165);
  __cogito_drawing_area_on_draw(__t4164, __t4165);
  ergo_release_val(__t4164);
  ergo_release_val(__t4165);
  ErgoVal __t4166 = YV_NULLV;
  ergo_release_val(__t4166);
  ErgoVal __t4167 = self; ergo_retain_val(__t4167);
  ergo_move_into(&__ret, __t4167);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_get_x(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4168 = self; ergo_retain_val(__t4168);
  ErgoVal __t4169 = __cogito_drawing_area_get_x(__t4168);
  ergo_release_val(__t4168);
  ergo_move_into(&__ret, __t4169);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_get_y(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4170 = self; ergo_retain_val(__t4170);
  ErgoVal __t4171 = __cogito_drawing_area_get_y(__t4170);
  ergo_release_val(__t4170);
  ergo_move_into(&__ret, __t4171);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_get_pressed(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4172 = self; ergo_retain_val(__t4172);
  ErgoVal __t4173 = __cogito_drawing_area_get_pressed(__t4172);
  ergo_release_val(__t4172);
  ergo_move_into(&__ret, __t4173);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_clear(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4174 = self; ergo_retain_val(__t4174);
  __cogito_drawing_area_clear(__t4174);
  ergo_release_val(__t4174);
  ErgoVal __t4175 = YV_NULLV;
  ergo_release_val(__t4175);
  ErgoVal __t4176 = self; ergo_retain_val(__t4176);
  ergo_move_into(&__ret, __t4176);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_set_color(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4177 = self; ergo_retain_val(__t4177);
  ErgoVal __t4178 = a0; ergo_retain_val(__t4178);
  __cogito_canvas_set_color(__t4177, __t4178);
  ergo_release_val(__t4177);
  ergo_release_val(__t4178);
  ErgoVal __t4179 = YV_NULLV;
  ergo_release_val(__t4179);
  ErgoVal __t4180 = self; ergo_retain_val(__t4180);
  ergo_move_into(&__ret, __t4180);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_set_line_width(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4181 = self; ergo_retain_val(__t4181);
  ErgoVal __t4182 = a0; ergo_retain_val(__t4182);
  __cogito_canvas_set_line_width(__t4181, __t4182);
  ergo_release_val(__t4181);
  ergo_release_val(__t4182);
  ErgoVal __t4183 = YV_NULLV;
  ergo_release_val(__t4183);
  ErgoVal __t4184 = self; ergo_retain_val(__t4184);
  ergo_move_into(&__ret, __t4184);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_line(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4185 = self; ergo_retain_val(__t4185);
  ErgoVal __t4186 = a0; ergo_retain_val(__t4186);
  ErgoVal __t4187 = a1; ergo_retain_val(__t4187);
  ErgoVal __t4188 = a2; ergo_retain_val(__t4188);
  ErgoVal __t4189 = a3; ergo_retain_val(__t4189);
  __cogito_canvas_line(__t4185, __t4186, __t4187, __t4188, __t4189);
  ergo_release_val(__t4185);
  ergo_release_val(__t4186);
  ergo_release_val(__t4187);
  ergo_release_val(__t4188);
  ergo_release_val(__t4189);
  ErgoVal __t4190 = YV_NULLV;
  ergo_release_val(__t4190);
  ErgoVal __t4191 = self; ergo_retain_val(__t4191);
  ergo_move_into(&__ret, __t4191);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_rect(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4192 = self; ergo_retain_val(__t4192);
  ErgoVal __t4193 = a0; ergo_retain_val(__t4193);
  ErgoVal __t4194 = a1; ergo_retain_val(__t4194);
  ErgoVal __t4195 = a2; ergo_retain_val(__t4195);
  ErgoVal __t4196 = a3; ergo_retain_val(__t4196);
  __cogito_canvas_rect(__t4192, __t4193, __t4194, __t4195, __t4196);
  ergo_release_val(__t4192);
  ergo_release_val(__t4193);
  ergo_release_val(__t4194);
  ergo_release_val(__t4195);
  ergo_release_val(__t4196);
  ErgoVal __t4197 = YV_NULLV;
  ergo_release_val(__t4197);
  ErgoVal __t4198 = self; ergo_retain_val(__t4198);
  ergo_move_into(&__ret, __t4198);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_fill_rect(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4199 = self; ergo_retain_val(__t4199);
  ErgoVal __t4200 = a0; ergo_retain_val(__t4200);
  ErgoVal __t4201 = a1; ergo_retain_val(__t4201);
  ErgoVal __t4202 = a2; ergo_retain_val(__t4202);
  ErgoVal __t4203 = a3; ergo_retain_val(__t4203);
  __cogito_canvas_fill_rect(__t4199, __t4200, __t4201, __t4202, __t4203);
  ergo_release_val(__t4199);
  ergo_release_val(__t4200);
  ergo_release_val(__t4201);
  ergo_release_val(__t4202);
  ergo_release_val(__t4203);
  ErgoVal __t4204 = YV_NULLV;
  ergo_release_val(__t4204);
  ErgoVal __t4205 = self; ergo_retain_val(__t4205);
  ergo_move_into(&__ret, __t4205);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4206 = self; ergo_retain_val(__t4206);
  ErgoVal __t4207 = a0; ergo_retain_val(__t4207);
  ErgoVal __t4208 = a1; ergo_retain_val(__t4208);
  ErgoVal __t4209 = a2; ergo_retain_val(__t4209);
  ErgoVal __t4210 = a3; ergo_retain_val(__t4210);
  __cogito_container_set_margins(__t4206, __t4207, __t4208, __t4209, __t4210);
  ergo_release_val(__t4206);
  ergo_release_val(__t4207);
  ergo_release_val(__t4208);
  ergo_release_val(__t4209);
  ergo_release_val(__t4210);
  ErgoVal __t4211 = YV_NULLV;
  ergo_release_val(__t4211);
  ErgoVal __t4212 = self; ergo_retain_val(__t4212);
  ergo_move_into(&__ret, __t4212);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4213 = self; ergo_retain_val(__t4213);
  ErgoVal __t4214 = a0; ergo_retain_val(__t4214);
  __cogito_container_set_hexpand(__t4213, __t4214);
  ergo_release_val(__t4213);
  ergo_release_val(__t4214);
  ErgoVal __t4215 = YV_NULLV;
  ergo_release_val(__t4215);
  ErgoVal __t4216 = self; ergo_retain_val(__t4216);
  ergo_move_into(&__ret, __t4216);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4217 = self; ergo_retain_val(__t4217);
  ErgoVal __t4218 = a0; ergo_retain_val(__t4218);
  __cogito_container_set_vexpand(__t4217, __t4218);
  ergo_release_val(__t4217);
  ergo_release_val(__t4218);
  ErgoVal __t4219 = YV_NULLV;
  ergo_release_val(__t4219);
  ErgoVal __t4220 = self; ergo_retain_val(__t4220);
  ergo_move_into(&__ret, __t4220);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4221 = self; ergo_retain_val(__t4221);
  ErgoVal __t4222 = a0; ergo_retain_val(__t4222);
  __cogito_node_set_class(__t4221, __t4222);
  ergo_release_val(__t4221);
  ergo_release_val(__t4222);
  ErgoVal __t4223 = YV_NULLV;
  ergo_release_val(__t4223);
  ErgoVal __t4224 = self; ergo_retain_val(__t4224);
  ergo_move_into(&__ret, __t4224);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_DrawingArea_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4225 = self; ergo_retain_val(__t4225);
  ErgoVal __t4226 = a0; ergo_retain_val(__t4226);
  __cogito_node_set_id(__t4225, __t4226);
  ergo_release_val(__t4225);
  ergo_release_val(__t4226);
  ErgoVal __t4227 = YV_NULLV;
  ergo_release_val(__t4227);
  ErgoVal __t4228 = self; ergo_retain_val(__t4228);
  ergo_move_into(&__ret, __t4228);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Canvas_set_color(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4229 = self; ergo_retain_val(__t4229);
  ErgoVal __t4230 = a0; ergo_retain_val(__t4230);
  __cogito_canvas_set_color(__t4229, __t4230);
  ergo_release_val(__t4229);
  ergo_release_val(__t4230);
  ErgoVal __t4231 = YV_NULLV;
  ergo_release_val(__t4231);
  ErgoVal __t4232 = self; ergo_retain_val(__t4232);
  ergo_move_into(&__ret, __t4232);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Canvas_set_line_width(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4233 = self; ergo_retain_val(__t4233);
  ErgoVal __t4234 = a0; ergo_retain_val(__t4234);
  __cogito_canvas_set_line_width(__t4233, __t4234);
  ergo_release_val(__t4233);
  ergo_release_val(__t4234);
  ErgoVal __t4235 = YV_NULLV;
  ergo_release_val(__t4235);
  ErgoVal __t4236 = self; ergo_retain_val(__t4236);
  ergo_move_into(&__ret, __t4236);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Canvas_line(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4237 = self; ergo_retain_val(__t4237);
  ErgoVal __t4238 = a0; ergo_retain_val(__t4238);
  ErgoVal __t4239 = a1; ergo_retain_val(__t4239);
  ErgoVal __t4240 = a2; ergo_retain_val(__t4240);
  ErgoVal __t4241 = a3; ergo_retain_val(__t4241);
  __cogito_canvas_line(__t4237, __t4238, __t4239, __t4240, __t4241);
  ergo_release_val(__t4237);
  ergo_release_val(__t4238);
  ergo_release_val(__t4239);
  ergo_release_val(__t4240);
  ergo_release_val(__t4241);
  ErgoVal __t4242 = YV_NULLV;
  ergo_release_val(__t4242);
  ErgoVal __t4243 = self; ergo_retain_val(__t4243);
  ergo_move_into(&__ret, __t4243);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Canvas_rect(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4244 = self; ergo_retain_val(__t4244);
  ErgoVal __t4245 = a0; ergo_retain_val(__t4245);
  ErgoVal __t4246 = a1; ergo_retain_val(__t4246);
  ErgoVal __t4247 = a2; ergo_retain_val(__t4247);
  ErgoVal __t4248 = a3; ergo_retain_val(__t4248);
  __cogito_canvas_rect(__t4244, __t4245, __t4246, __t4247, __t4248);
  ergo_release_val(__t4244);
  ergo_release_val(__t4245);
  ergo_release_val(__t4246);
  ergo_release_val(__t4247);
  ergo_release_val(__t4248);
  ErgoVal __t4249 = YV_NULLV;
  ergo_release_val(__t4249);
  ErgoVal __t4250 = self; ergo_retain_val(__t4250);
  ergo_move_into(&__ret, __t4250);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Canvas_fill_rect(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4251 = self; ergo_retain_val(__t4251);
  ErgoVal __t4252 = a0; ergo_retain_val(__t4252);
  ErgoVal __t4253 = a1; ergo_retain_val(__t4253);
  ErgoVal __t4254 = a2; ergo_retain_val(__t4254);
  ErgoVal __t4255 = a3; ergo_retain_val(__t4255);
  __cogito_canvas_fill_rect(__t4251, __t4252, __t4253, __t4254, __t4255);
  ergo_release_val(__t4251);
  ergo_release_val(__t4252);
  ergo_release_val(__t4253);
  ergo_release_val(__t4254);
  ergo_release_val(__t4255);
  ErgoVal __t4256 = YV_NULLV;
  ergo_release_val(__t4256);
  ErgoVal __t4257 = self; ergo_retain_val(__t4257);
  ergo_move_into(&__ret, __t4257);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Shape_set_preset(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4258 = self; ergo_retain_val(__t4258);
  ErgoVal __t4259 = a0; ergo_retain_val(__t4259);
  __cogito_shape_set_preset(__t4258, __t4259);
  ergo_release_val(__t4258);
  ergo_release_val(__t4259);
  ErgoVal __t4260 = YV_NULLV;
  ergo_release_val(__t4260);
  ErgoVal __t4261 = self; ergo_retain_val(__t4261);
  ergo_move_into(&__ret, __t4261);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Shape_get_preset(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4262 = self; ergo_retain_val(__t4262);
  ErgoVal __t4263 = __cogito_shape_get_preset(__t4262);
  ergo_release_val(__t4262);
  ergo_move_into(&__ret, __t4263);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Shape_set_size(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4264 = self; ergo_retain_val(__t4264);
  ErgoVal __t4265 = a0; ergo_retain_val(__t4265);
  __cogito_shape_set_size(__t4264, __t4265);
  ergo_release_val(__t4264);
  ergo_release_val(__t4265);
  ErgoVal __t4266 = YV_NULLV;
  ergo_release_val(__t4266);
  ErgoVal __t4267 = self; ergo_retain_val(__t4267);
  ergo_move_into(&__ret, __t4267);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Shape_get_size(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4268 = self; ergo_retain_val(__t4268);
  ErgoVal __t4269 = __cogito_shape_get_size(__t4268);
  ergo_release_val(__t4268);
  ergo_move_into(&__ret, __t4269);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Shape_set_color(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4270 = self; ergo_retain_val(__t4270);
  ErgoVal __t4271 = a0; ergo_retain_val(__t4271);
  __cogito_shape_set_color(__t4270, __t4271);
  ergo_release_val(__t4270);
  ergo_release_val(__t4271);
  ErgoVal __t4272 = YV_NULLV;
  ergo_release_val(__t4272);
  ErgoVal __t4273 = self; ergo_retain_val(__t4273);
  ergo_move_into(&__ret, __t4273);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Shape_set_color_style(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4274 = self; ergo_retain_val(__t4274);
  ErgoVal __t4275 = a0; ergo_retain_val(__t4275);
  __cogito_shape_set_color_style(__t4274, __t4275);
  ergo_release_val(__t4274);
  ergo_release_val(__t4275);
  ErgoVal __t4276 = YV_NULLV;
  ergo_release_val(__t4276);
  ErgoVal __t4277 = self; ergo_retain_val(__t4277);
  ergo_move_into(&__ret, __t4277);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Shape_get_color_style(ErgoVal self) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4278 = self; ergo_retain_val(__t4278);
  ErgoVal __t4279 = __cogito_shape_get_color_style(__t4278);
  ergo_release_val(__t4278);
  ergo_move_into(&__ret, __t4279);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Shape_set_vertex(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4280 = self; ergo_retain_val(__t4280);
  ErgoVal __t4281 = a0; ergo_retain_val(__t4281);
  ErgoVal __t4282 = a1; ergo_retain_val(__t4282);
  ErgoVal __t4283 = a2; ergo_retain_val(__t4283);
  __cogito_shape_set_vertex(__t4280, __t4281, __t4282, __t4283);
  ergo_release_val(__t4280);
  ergo_release_val(__t4281);
  ergo_release_val(__t4282);
  ergo_release_val(__t4283);
  ErgoVal __t4284 = YV_NULLV;
  ergo_release_val(__t4284);
  ErgoVal __t4285 = self; ergo_retain_val(__t4285);
  ergo_move_into(&__ret, __t4285);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Shape_get_vertex_x(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4286 = self; ergo_retain_val(__t4286);
  ErgoVal __t4287 = a0; ergo_retain_val(__t4287);
  ErgoVal __t4288 = __cogito_shape_get_vertex_x(__t4286, __t4287);
  ergo_release_val(__t4286);
  ergo_release_val(__t4287);
  ergo_move_into(&__ret, __t4288);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Shape_get_vertex_y(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4289 = self; ergo_retain_val(__t4289);
  ErgoVal __t4290 = a0; ergo_retain_val(__t4290);
  ErgoVal __t4291 = __cogito_shape_get_vertex_y(__t4289, __t4290);
  ergo_release_val(__t4289);
  ergo_release_val(__t4290);
  ergo_move_into(&__ret, __t4291);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Shape_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4292 = self; ergo_retain_val(__t4292);
  ErgoVal __t4293 = a0; ergo_retain_val(__t4293);
  ErgoVal __t4294 = a1; ergo_retain_val(__t4294);
  ErgoVal __t4295 = a2; ergo_retain_val(__t4295);
  ErgoVal __t4296 = a3; ergo_retain_val(__t4296);
  __cogito_container_set_margins(__t4292, __t4293, __t4294, __t4295, __t4296);
  ergo_release_val(__t4292);
  ergo_release_val(__t4293);
  ergo_release_val(__t4294);
  ergo_release_val(__t4295);
  ergo_release_val(__t4296);
  ErgoVal __t4297 = YV_NULLV;
  ergo_release_val(__t4297);
  ErgoVal __t4298 = self; ergo_retain_val(__t4298);
  ergo_move_into(&__ret, __t4298);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Shape_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4299 = self; ergo_retain_val(__t4299);
  ErgoVal __t4300 = a0; ergo_retain_val(__t4300);
  __cogito_node_set_class(__t4299, __t4300);
  ergo_release_val(__t4299);
  ergo_release_val(__t4300);
  ErgoVal __t4301 = YV_NULLV;
  ergo_release_val(__t4301);
  ErgoVal __t4302 = self; ergo_retain_val(__t4302);
  ergo_move_into(&__ret, __t4302);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Shape_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4303 = self; ergo_retain_val(__t4303);
  ErgoVal __t4304 = a0; ergo_retain_val(__t4304);
  __cogito_node_set_id(__t4303, __t4304);
  ergo_release_val(__t4303);
  ergo_release_val(__t4304);
  ErgoVal __t4305 = YV_NULLV;
  ergo_release_val(__t4305);
  ErgoVal __t4306 = self; ergo_retain_val(__t4306);
  ergo_move_into(&__ret, __t4306);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ContentList_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4307 = self; ergo_retain_val(__t4307);
  ErgoVal __t4308 = a0; ergo_retain_val(__t4308);
  __cogito_container_add(__t4307, __t4308);
  ergo_release_val(__t4307);
  ergo_release_val(__t4308);
  ErgoVal __t4309 = YV_NULLV;
  ergo_release_val(__t4309);
  ErgoVal __t4310 = self; ergo_retain_val(__t4310);
  ergo_move_into(&__ret, __t4310);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ContentList_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4311 = self; ergo_retain_val(__t4311);
  ErgoVal __t4312 = a0; ergo_retain_val(__t4312);
  __cogito_build(__t4311, __t4312);
  ergo_release_val(__t4311);
  ergo_release_val(__t4312);
  ErgoVal __t4313 = YV_NULLV;
  ergo_release_val(__t4313);
  ErgoVal __t4314 = self; ergo_retain_val(__t4314);
  ergo_move_into(&__ret, __t4314);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ContentList_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4315 = self; ergo_retain_val(__t4315);
  ErgoVal __t4316 = a0; ergo_retain_val(__t4316);
  ErgoVal __t4317 = a1; ergo_retain_val(__t4317);
  ErgoVal __t4318 = a2; ergo_retain_val(__t4318);
  ErgoVal __t4319 = a3; ergo_retain_val(__t4319);
  __cogito_container_set_margins(__t4315, __t4316, __t4317, __t4318, __t4319);
  ergo_release_val(__t4315);
  ergo_release_val(__t4316);
  ergo_release_val(__t4317);
  ergo_release_val(__t4318);
  ergo_release_val(__t4319);
  ErgoVal __t4320 = YV_NULLV;
  ergo_release_val(__t4320);
  ErgoVal __t4321 = self; ergo_retain_val(__t4321);
  ergo_move_into(&__ret, __t4321);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ContentList_set_gap(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4322 = self; ergo_retain_val(__t4322);
  ErgoVal __t4323 = a0; ergo_retain_val(__t4323);
  __cogito_container_set_gap(__t4322, __t4323);
  ergo_release_val(__t4322);
  ergo_release_val(__t4323);
  ErgoVal __t4324 = YV_NULLV;
  ergo_release_val(__t4324);
  ErgoVal __t4325 = self; ergo_retain_val(__t4325);
  ergo_move_into(&__ret, __t4325);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ContentList_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4326 = self; ergo_retain_val(__t4326);
  ErgoVal __t4327 = a0; ergo_retain_val(__t4327);
  __cogito_container_set_hexpand(__t4326, __t4327);
  ergo_release_val(__t4326);
  ergo_release_val(__t4327);
  ErgoVal __t4328 = YV_NULLV;
  ergo_release_val(__t4328);
  ErgoVal __t4329 = self; ergo_retain_val(__t4329);
  ergo_move_into(&__ret, __t4329);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ContentList_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4330 = self; ergo_retain_val(__t4330);
  ErgoVal __t4331 = a0; ergo_retain_val(__t4331);
  __cogito_node_set_class(__t4330, __t4331);
  ergo_release_val(__t4330);
  ergo_release_val(__t4331);
  ErgoVal __t4332 = YV_NULLV;
  ergo_release_val(__t4332);
  ErgoVal __t4333 = self; ergo_retain_val(__t4333);
  ergo_move_into(&__ret, __t4333);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ContentList_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4334 = self; ergo_retain_val(__t4334);
  ErgoVal __t4335 = a0; ergo_retain_val(__t4335);
  __cogito_node_set_id(__t4334, __t4335);
  ergo_release_val(__t4334);
  ergo_release_val(__t4335);
  ErgoVal __t4336 = YV_NULLV;
  ergo_release_val(__t4336);
  ErgoVal __t4337 = self; ergo_retain_val(__t4337);
  ergo_move_into(&__ret, __t4337);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_EmptyPage_set_description(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4338 = self; ergo_retain_val(__t4338);
  ErgoVal __t4339 = a0; ergo_retain_val(__t4339);
  __cogito_empty_page_set_description(__t4338, __t4339);
  ergo_release_val(__t4338);
  ergo_release_val(__t4339);
  ErgoVal __t4340 = YV_NULLV;
  ergo_release_val(__t4340);
  ErgoVal __t4341 = self; ergo_retain_val(__t4341);
  ergo_move_into(&__ret, __t4341);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_EmptyPage_set_icon(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4342 = self; ergo_retain_val(__t4342);
  ErgoVal __t4343 = a0; ergo_retain_val(__t4343);
  __cogito_empty_page_set_icon(__t4342, __t4343);
  ergo_release_val(__t4342);
  ergo_release_val(__t4343);
  ErgoVal __t4344 = YV_NULLV;
  ergo_release_val(__t4344);
  ErgoVal __t4345 = self; ergo_retain_val(__t4345);
  ergo_move_into(&__ret, __t4345);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_EmptyPage_set_action(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4346 = self; ergo_retain_val(__t4346);
  ErgoVal __t4347 = a0; ergo_retain_val(__t4347);
  ErgoVal __t4348 = a1; ergo_retain_val(__t4348);
  __cogito_empty_page_set_action(__t4346, __t4347, __t4348);
  ergo_release_val(__t4346);
  ergo_release_val(__t4347);
  ergo_release_val(__t4348);
  ErgoVal __t4349 = YV_NULLV;
  ergo_release_val(__t4349);
  ErgoVal __t4350 = self; ergo_retain_val(__t4350);
  ergo_move_into(&__ret, __t4350);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_EmptyPage_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4351 = self; ergo_retain_val(__t4351);
  ErgoVal __t4352 = a0; ergo_retain_val(__t4352);
  __cogito_container_add(__t4351, __t4352);
  ergo_release_val(__t4351);
  ergo_release_val(__t4352);
  ErgoVal __t4353 = YV_NULLV;
  ergo_release_val(__t4353);
  ErgoVal __t4354 = self; ergo_retain_val(__t4354);
  ergo_move_into(&__ret, __t4354);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_EmptyPage_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4355 = self; ergo_retain_val(__t4355);
  ErgoVal __t4356 = a0; ergo_retain_val(__t4356);
  __cogito_build(__t4355, __t4356);
  ergo_release_val(__t4355);
  ergo_release_val(__t4356);
  ErgoVal __t4357 = YV_NULLV;
  ergo_release_val(__t4357);
  ErgoVal __t4358 = self; ergo_retain_val(__t4358);
  ergo_move_into(&__ret, __t4358);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_EmptyPage_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4359 = self; ergo_retain_val(__t4359);
  ErgoVal __t4360 = a0; ergo_retain_val(__t4360);
  ErgoVal __t4361 = a1; ergo_retain_val(__t4361);
  ErgoVal __t4362 = a2; ergo_retain_val(__t4362);
  ErgoVal __t4363 = a3; ergo_retain_val(__t4363);
  __cogito_container_set_margins(__t4359, __t4360, __t4361, __t4362, __t4363);
  ergo_release_val(__t4359);
  ergo_release_val(__t4360);
  ergo_release_val(__t4361);
  ergo_release_val(__t4362);
  ergo_release_val(__t4363);
  ErgoVal __t4364 = YV_NULLV;
  ergo_release_val(__t4364);
  ErgoVal __t4365 = self; ergo_retain_val(__t4365);
  ergo_move_into(&__ret, __t4365);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_EmptyPage_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4366 = self; ergo_retain_val(__t4366);
  ErgoVal __t4367 = a0; ergo_retain_val(__t4367);
  __cogito_container_set_hexpand(__t4366, __t4367);
  ergo_release_val(__t4366);
  ergo_release_val(__t4367);
  ErgoVal __t4368 = YV_NULLV;
  ergo_release_val(__t4368);
  ErgoVal __t4369 = self; ergo_retain_val(__t4369);
  ergo_move_into(&__ret, __t4369);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_EmptyPage_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4370 = self; ergo_retain_val(__t4370);
  ErgoVal __t4371 = a0; ergo_retain_val(__t4371);
  __cogito_container_set_vexpand(__t4370, __t4371);
  ergo_release_val(__t4370);
  ergo_release_val(__t4371);
  ErgoVal __t4372 = YV_NULLV;
  ergo_release_val(__t4372);
  ErgoVal __t4373 = self; ergo_retain_val(__t4373);
  ergo_move_into(&__ret, __t4373);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_EmptyPage_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4374 = self; ergo_retain_val(__t4374);
  ErgoVal __t4375 = a0; ergo_retain_val(__t4375);
  __cogito_node_set_class(__t4374, __t4375);
  ergo_release_val(__t4374);
  ergo_release_val(__t4375);
  ErgoVal __t4376 = YV_NULLV;
  ergo_release_val(__t4376);
  ErgoVal __t4377 = self; ergo_retain_val(__t4377);
  ergo_move_into(&__ret, __t4377);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_EmptyPage_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4378 = self; ergo_retain_val(__t4378);
  ErgoVal __t4379 = a0; ergo_retain_val(__t4379);
  __cogito_node_set_id(__t4378, __t4379);
  ergo_release_val(__t4378);
  ergo_release_val(__t4379);
  ErgoVal __t4380 = YV_NULLV;
  ergo_release_val(__t4380);
  ErgoVal __t4381 = self; ergo_retain_val(__t4381);
  ergo_move_into(&__ret, __t4381);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TipView_set_title(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4382 = self; ergo_retain_val(__t4382);
  ErgoVal __t4383 = a0; ergo_retain_val(__t4383);
  __cogito_tip_view_set_title(__t4382, __t4383);
  ergo_release_val(__t4382);
  ergo_release_val(__t4383);
  ErgoVal __t4384 = YV_NULLV;
  ergo_release_val(__t4384);
  ErgoVal __t4385 = self; ergo_retain_val(__t4385);
  ergo_move_into(&__ret, __t4385);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TipView_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4386 = self; ergo_retain_val(__t4386);
  ErgoVal __t4387 = a0; ergo_retain_val(__t4387);
  ErgoVal __t4388 = a1; ergo_retain_val(__t4388);
  ErgoVal __t4389 = a2; ergo_retain_val(__t4389);
  ErgoVal __t4390 = a3; ergo_retain_val(__t4390);
  __cogito_container_set_margins(__t4386, __t4387, __t4388, __t4389, __t4390);
  ergo_release_val(__t4386);
  ergo_release_val(__t4387);
  ergo_release_val(__t4388);
  ergo_release_val(__t4389);
  ergo_release_val(__t4390);
  ErgoVal __t4391 = YV_NULLV;
  ergo_release_val(__t4391);
  ErgoVal __t4392 = self; ergo_retain_val(__t4392);
  ergo_move_into(&__ret, __t4392);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TipView_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4393 = self; ergo_retain_val(__t4393);
  ErgoVal __t4394 = a0; ergo_retain_val(__t4394);
  __cogito_container_set_hexpand(__t4393, __t4394);
  ergo_release_val(__t4393);
  ergo_release_val(__t4394);
  ErgoVal __t4395 = YV_NULLV;
  ergo_release_val(__t4395);
  ErgoVal __t4396 = self; ergo_retain_val(__t4396);
  ergo_move_into(&__ret, __t4396);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TipView_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4397 = self; ergo_retain_val(__t4397);
  ErgoVal __t4398 = a0; ergo_retain_val(__t4398);
  __cogito_node_set_class(__t4397, __t4398);
  ergo_release_val(__t4397);
  ergo_release_val(__t4398);
  ErgoVal __t4399 = YV_NULLV;
  ergo_release_val(__t4399);
  ErgoVal __t4400 = self; ergo_retain_val(__t4400);
  ergo_move_into(&__ret, __t4400);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_TipView_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4401 = self; ergo_retain_val(__t4401);
  ErgoVal __t4402 = a0; ergo_retain_val(__t4402);
  __cogito_node_set_id(__t4401, __t4402);
  ergo_release_val(__t4401);
  ergo_release_val(__t4402);
  ErgoVal __t4403 = YV_NULLV;
  ergo_release_val(__t4403);
  ErgoVal __t4404 = self; ergo_retain_val(__t4404);
  ergo_move_into(&__ret, __t4404);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsWindow_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4405 = self; ergo_retain_val(__t4405);
  ErgoVal __t4406 = a0; ergo_retain_val(__t4406);
  __cogito_container_add(__t4405, __t4406);
  ergo_release_val(__t4405);
  ergo_release_val(__t4406);
  ErgoVal __t4407 = YV_NULLV;
  ergo_release_val(__t4407);
  ErgoVal __t4408 = self; ergo_retain_val(__t4408);
  ergo_move_into(&__ret, __t4408);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsWindow_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4409 = self; ergo_retain_val(__t4409);
  ErgoVal __t4410 = a0; ergo_retain_val(__t4410);
  __cogito_build(__t4409, __t4410);
  ergo_release_val(__t4409);
  ergo_release_val(__t4410);
  ErgoVal __t4411 = YV_NULLV;
  ergo_release_val(__t4411);
  ErgoVal __t4412 = self; ergo_retain_val(__t4412);
  ergo_move_into(&__ret, __t4412);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsWindow_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4413 = self; ergo_retain_val(__t4413);
  ErgoVal __t4414 = a0; ergo_retain_val(__t4414);
  ErgoVal __t4415 = a1; ergo_retain_val(__t4415);
  ErgoVal __t4416 = a2; ergo_retain_val(__t4416);
  ErgoVal __t4417 = a3; ergo_retain_val(__t4417);
  __cogito_container_set_margins(__t4413, __t4414, __t4415, __t4416, __t4417);
  ergo_release_val(__t4413);
  ergo_release_val(__t4414);
  ergo_release_val(__t4415);
  ergo_release_val(__t4416);
  ergo_release_val(__t4417);
  ErgoVal __t4418 = YV_NULLV;
  ergo_release_val(__t4418);
  ErgoVal __t4419 = self; ergo_retain_val(__t4419);
  ergo_move_into(&__ret, __t4419);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsWindow_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4420 = self; ergo_retain_val(__t4420);
  ErgoVal __t4421 = a0; ergo_retain_val(__t4421);
  __cogito_container_set_hexpand(__t4420, __t4421);
  ergo_release_val(__t4420);
  ergo_release_val(__t4421);
  ErgoVal __t4422 = YV_NULLV;
  ergo_release_val(__t4422);
  ErgoVal __t4423 = self; ergo_retain_val(__t4423);
  ergo_move_into(&__ret, __t4423);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsWindow_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4424 = self; ergo_retain_val(__t4424);
  ErgoVal __t4425 = a0; ergo_retain_val(__t4425);
  __cogito_container_set_vexpand(__t4424, __t4425);
  ergo_release_val(__t4424);
  ergo_release_val(__t4425);
  ErgoVal __t4426 = YV_NULLV;
  ergo_release_val(__t4426);
  ErgoVal __t4427 = self; ergo_retain_val(__t4427);
  ergo_move_into(&__ret, __t4427);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsWindow_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4428 = self; ergo_retain_val(__t4428);
  ErgoVal __t4429 = a0; ergo_retain_val(__t4429);
  __cogito_node_set_class(__t4428, __t4429);
  ergo_release_val(__t4428);
  ergo_release_val(__t4429);
  ErgoVal __t4430 = YV_NULLV;
  ergo_release_val(__t4430);
  ErgoVal __t4431 = self; ergo_retain_val(__t4431);
  ergo_move_into(&__ret, __t4431);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsWindow_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4432 = self; ergo_retain_val(__t4432);
  ErgoVal __t4433 = a0; ergo_retain_val(__t4433);
  __cogito_node_set_id(__t4432, __t4433);
  ergo_release_val(__t4432);
  ergo_release_val(__t4433);
  ErgoVal __t4434 = YV_NULLV;
  ergo_release_val(__t4434);
  ErgoVal __t4435 = self; ergo_retain_val(__t4435);
  ergo_move_into(&__ret, __t4435);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsPage_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4436 = self; ergo_retain_val(__t4436);
  ErgoVal __t4437 = a0; ergo_retain_val(__t4437);
  __cogito_container_add(__t4436, __t4437);
  ergo_release_val(__t4436);
  ergo_release_val(__t4437);
  ErgoVal __t4438 = YV_NULLV;
  ergo_release_val(__t4438);
  ErgoVal __t4439 = self; ergo_retain_val(__t4439);
  ergo_move_into(&__ret, __t4439);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsPage_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4440 = self; ergo_retain_val(__t4440);
  ErgoVal __t4441 = a0; ergo_retain_val(__t4441);
  __cogito_build(__t4440, __t4441);
  ergo_release_val(__t4440);
  ergo_release_val(__t4441);
  ErgoVal __t4442 = YV_NULLV;
  ergo_release_val(__t4442);
  ErgoVal __t4443 = self; ergo_retain_val(__t4443);
  ergo_move_into(&__ret, __t4443);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsPage_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4444 = self; ergo_retain_val(__t4444);
  ErgoVal __t4445 = a0; ergo_retain_val(__t4445);
  ErgoVal __t4446 = a1; ergo_retain_val(__t4446);
  ErgoVal __t4447 = a2; ergo_retain_val(__t4447);
  ErgoVal __t4448 = a3; ergo_retain_val(__t4448);
  __cogito_container_set_margins(__t4444, __t4445, __t4446, __t4447, __t4448);
  ergo_release_val(__t4444);
  ergo_release_val(__t4445);
  ergo_release_val(__t4446);
  ergo_release_val(__t4447);
  ergo_release_val(__t4448);
  ErgoVal __t4449 = YV_NULLV;
  ergo_release_val(__t4449);
  ErgoVal __t4450 = self; ergo_retain_val(__t4450);
  ergo_move_into(&__ret, __t4450);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsPage_set_gap(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4451 = self; ergo_retain_val(__t4451);
  ErgoVal __t4452 = a0; ergo_retain_val(__t4452);
  __cogito_container_set_gap(__t4451, __t4452);
  ergo_release_val(__t4451);
  ergo_release_val(__t4452);
  ErgoVal __t4453 = YV_NULLV;
  ergo_release_val(__t4453);
  ErgoVal __t4454 = self; ergo_retain_val(__t4454);
  ergo_move_into(&__ret, __t4454);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsPage_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4455 = self; ergo_retain_val(__t4455);
  ErgoVal __t4456 = a0; ergo_retain_val(__t4456);
  __cogito_container_set_hexpand(__t4455, __t4456);
  ergo_release_val(__t4455);
  ergo_release_val(__t4456);
  ErgoVal __t4457 = YV_NULLV;
  ergo_release_val(__t4457);
  ErgoVal __t4458 = self; ergo_retain_val(__t4458);
  ergo_move_into(&__ret, __t4458);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsPage_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4459 = self; ergo_retain_val(__t4459);
  ErgoVal __t4460 = a0; ergo_retain_val(__t4460);
  __cogito_node_set_class(__t4459, __t4460);
  ergo_release_val(__t4459);
  ergo_release_val(__t4460);
  ErgoVal __t4461 = YV_NULLV;
  ergo_release_val(__t4461);
  ErgoVal __t4462 = self; ergo_retain_val(__t4462);
  ergo_move_into(&__ret, __t4462);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsPage_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4463 = self; ergo_retain_val(__t4463);
  ErgoVal __t4464 = a0; ergo_retain_val(__t4464);
  __cogito_node_set_id(__t4463, __t4464);
  ergo_release_val(__t4463);
  ergo_release_val(__t4464);
  ErgoVal __t4465 = YV_NULLV;
  ergo_release_val(__t4465);
  ErgoVal __t4466 = self; ergo_retain_val(__t4466);
  ergo_move_into(&__ret, __t4466);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsList_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4467 = self; ergo_retain_val(__t4467);
  ErgoVal __t4468 = a0; ergo_retain_val(__t4468);
  __cogito_container_add(__t4467, __t4468);
  ergo_release_val(__t4467);
  ergo_release_val(__t4468);
  ErgoVal __t4469 = YV_NULLV;
  ergo_release_val(__t4469);
  ErgoVal __t4470 = self; ergo_retain_val(__t4470);
  ergo_move_into(&__ret, __t4470);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsList_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4471 = self; ergo_retain_val(__t4471);
  ErgoVal __t4472 = a0; ergo_retain_val(__t4472);
  __cogito_build(__t4471, __t4472);
  ergo_release_val(__t4471);
  ergo_release_val(__t4472);
  ErgoVal __t4473 = YV_NULLV;
  ergo_release_val(__t4473);
  ErgoVal __t4474 = self; ergo_retain_val(__t4474);
  ergo_move_into(&__ret, __t4474);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsList_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4475 = self; ergo_retain_val(__t4475);
  ErgoVal __t4476 = a0; ergo_retain_val(__t4476);
  ErgoVal __t4477 = a1; ergo_retain_val(__t4477);
  ErgoVal __t4478 = a2; ergo_retain_val(__t4478);
  ErgoVal __t4479 = a3; ergo_retain_val(__t4479);
  __cogito_container_set_margins(__t4475, __t4476, __t4477, __t4478, __t4479);
  ergo_release_val(__t4475);
  ergo_release_val(__t4476);
  ergo_release_val(__t4477);
  ergo_release_val(__t4478);
  ergo_release_val(__t4479);
  ErgoVal __t4480 = YV_NULLV;
  ergo_release_val(__t4480);
  ErgoVal __t4481 = self; ergo_retain_val(__t4481);
  ergo_move_into(&__ret, __t4481);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsList_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4482 = self; ergo_retain_val(__t4482);
  ErgoVal __t4483 = a0; ergo_retain_val(__t4483);
  __cogito_container_set_hexpand(__t4482, __t4483);
  ergo_release_val(__t4482);
  ergo_release_val(__t4483);
  ErgoVal __t4484 = YV_NULLV;
  ergo_release_val(__t4484);
  ErgoVal __t4485 = self; ergo_retain_val(__t4485);
  ergo_move_into(&__ret, __t4485);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsList_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4486 = self; ergo_retain_val(__t4486);
  ErgoVal __t4487 = a0; ergo_retain_val(__t4487);
  __cogito_node_set_class(__t4486, __t4487);
  ergo_release_val(__t4486);
  ergo_release_val(__t4487);
  ErgoVal __t4488 = YV_NULLV;
  ergo_release_val(__t4488);
  ErgoVal __t4489 = self; ergo_retain_val(__t4489);
  ergo_move_into(&__ret, __t4489);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsList_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4490 = self; ergo_retain_val(__t4490);
  ErgoVal __t4491 = a0; ergo_retain_val(__t4491);
  __cogito_node_set_id(__t4490, __t4491);
  ergo_release_val(__t4490);
  ergo_release_val(__t4491);
  ErgoVal __t4492 = YV_NULLV;
  ergo_release_val(__t4492);
  ErgoVal __t4493 = self; ergo_retain_val(__t4493);
  ergo_move_into(&__ret, __t4493);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsRow_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4494 = self; ergo_retain_val(__t4494);
  ErgoVal __t4495 = a0; ergo_retain_val(__t4495);
  __cogito_container_add(__t4494, __t4495);
  ergo_release_val(__t4494);
  ergo_release_val(__t4495);
  ErgoVal __t4496 = YV_NULLV;
  ergo_release_val(__t4496);
  ErgoVal __t4497 = self; ergo_retain_val(__t4497);
  ergo_move_into(&__ret, __t4497);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsRow_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4498 = self; ergo_retain_val(__t4498);
  ErgoVal __t4499 = a0; ergo_retain_val(__t4499);
  __cogito_build(__t4498, __t4499);
  ergo_release_val(__t4498);
  ergo_release_val(__t4499);
  ErgoVal __t4500 = YV_NULLV;
  ergo_release_val(__t4500);
  ErgoVal __t4501 = self; ergo_retain_val(__t4501);
  ergo_move_into(&__ret, __t4501);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsRow_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4502 = self; ergo_retain_val(__t4502);
  ErgoVal __t4503 = a0; ergo_retain_val(__t4503);
  ErgoVal __t4504 = a1; ergo_retain_val(__t4504);
  ErgoVal __t4505 = a2; ergo_retain_val(__t4505);
  ErgoVal __t4506 = a3; ergo_retain_val(__t4506);
  __cogito_container_set_margins(__t4502, __t4503, __t4504, __t4505, __t4506);
  ergo_release_val(__t4502);
  ergo_release_val(__t4503);
  ergo_release_val(__t4504);
  ergo_release_val(__t4505);
  ergo_release_val(__t4506);
  ErgoVal __t4507 = YV_NULLV;
  ergo_release_val(__t4507);
  ErgoVal __t4508 = self; ergo_retain_val(__t4508);
  ergo_move_into(&__ret, __t4508);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsRow_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4509 = self; ergo_retain_val(__t4509);
  ErgoVal __t4510 = a0; ergo_retain_val(__t4510);
  __cogito_container_set_hexpand(__t4509, __t4510);
  ergo_release_val(__t4509);
  ergo_release_val(__t4510);
  ErgoVal __t4511 = YV_NULLV;
  ergo_release_val(__t4511);
  ErgoVal __t4512 = self; ergo_retain_val(__t4512);
  ergo_move_into(&__ret, __t4512);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsRow_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4513 = self; ergo_retain_val(__t4513);
  ErgoVal __t4514 = a0; ergo_retain_val(__t4514);
  __cogito_node_set_class(__t4513, __t4514);
  ergo_release_val(__t4513);
  ergo_release_val(__t4514);
  ErgoVal __t4515 = YV_NULLV;
  ergo_release_val(__t4515);
  ErgoVal __t4516 = self; ergo_retain_val(__t4516);
  ergo_move_into(&__ret, __t4516);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SettingsRow_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4517 = self; ergo_retain_val(__t4517);
  ErgoVal __t4518 = a0; ergo_retain_val(__t4518);
  __cogito_node_set_id(__t4517, __t4518);
  ergo_release_val(__t4517);
  ergo_release_val(__t4518);
  ErgoVal __t4519 = YV_NULLV;
  ergo_release_val(__t4519);
  ErgoVal __t4520 = self; ergo_retain_val(__t4520);
  ergo_move_into(&__ret, __t4520);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_WelcomeScreen_set_description(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4521 = self; ergo_retain_val(__t4521);
  ErgoVal __t4522 = a0; ergo_retain_val(__t4522);
  __cogito_welcome_screen_set_description(__t4521, __t4522);
  ergo_release_val(__t4521);
  ergo_release_val(__t4522);
  ErgoVal __t4523 = YV_NULLV;
  ergo_release_val(__t4523);
  ErgoVal __t4524 = self; ergo_retain_val(__t4524);
  ergo_move_into(&__ret, __t4524);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_WelcomeScreen_set_icon(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4525 = self; ergo_retain_val(__t4525);
  ErgoVal __t4526 = a0; ergo_retain_val(__t4526);
  __cogito_welcome_screen_set_icon(__t4525, __t4526);
  ergo_release_val(__t4525);
  ergo_release_val(__t4526);
  ErgoVal __t4527 = YV_NULLV;
  ergo_release_val(__t4527);
  ErgoVal __t4528 = self; ergo_retain_val(__t4528);
  ergo_move_into(&__ret, __t4528);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_WelcomeScreen_set_action(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4529 = self; ergo_retain_val(__t4529);
  ErgoVal __t4530 = a0; ergo_retain_val(__t4530);
  ErgoVal __t4531 = a1; ergo_retain_val(__t4531);
  __cogito_welcome_screen_set_action(__t4529, __t4530, __t4531);
  ergo_release_val(__t4529);
  ergo_release_val(__t4530);
  ergo_release_val(__t4531);
  ErgoVal __t4532 = YV_NULLV;
  ergo_release_val(__t4532);
  ErgoVal __t4533 = self; ergo_retain_val(__t4533);
  ergo_move_into(&__ret, __t4533);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_WelcomeScreen_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4534 = self; ergo_retain_val(__t4534);
  ErgoVal __t4535 = a0; ergo_retain_val(__t4535);
  __cogito_container_add(__t4534, __t4535);
  ergo_release_val(__t4534);
  ergo_release_val(__t4535);
  ErgoVal __t4536 = YV_NULLV;
  ergo_release_val(__t4536);
  ErgoVal __t4537 = self; ergo_retain_val(__t4537);
  ergo_move_into(&__ret, __t4537);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_WelcomeScreen_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4538 = self; ergo_retain_val(__t4538);
  ErgoVal __t4539 = a0; ergo_retain_val(__t4539);
  __cogito_build(__t4538, __t4539);
  ergo_release_val(__t4538);
  ergo_release_val(__t4539);
  ErgoVal __t4540 = YV_NULLV;
  ergo_release_val(__t4540);
  ErgoVal __t4541 = self; ergo_retain_val(__t4541);
  ergo_move_into(&__ret, __t4541);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_WelcomeScreen_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4542 = self; ergo_retain_val(__t4542);
  ErgoVal __t4543 = a0; ergo_retain_val(__t4543);
  ErgoVal __t4544 = a1; ergo_retain_val(__t4544);
  ErgoVal __t4545 = a2; ergo_retain_val(__t4545);
  ErgoVal __t4546 = a3; ergo_retain_val(__t4546);
  __cogito_container_set_margins(__t4542, __t4543, __t4544, __t4545, __t4546);
  ergo_release_val(__t4542);
  ergo_release_val(__t4543);
  ergo_release_val(__t4544);
  ergo_release_val(__t4545);
  ergo_release_val(__t4546);
  ErgoVal __t4547 = YV_NULLV;
  ergo_release_val(__t4547);
  ErgoVal __t4548 = self; ergo_retain_val(__t4548);
  ergo_move_into(&__ret, __t4548);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_WelcomeScreen_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4549 = self; ergo_retain_val(__t4549);
  ErgoVal __t4550 = a0; ergo_retain_val(__t4550);
  __cogito_container_set_hexpand(__t4549, __t4550);
  ergo_release_val(__t4549);
  ergo_release_val(__t4550);
  ErgoVal __t4551 = YV_NULLV;
  ergo_release_val(__t4551);
  ErgoVal __t4552 = self; ergo_retain_val(__t4552);
  ergo_move_into(&__ret, __t4552);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_WelcomeScreen_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4553 = self; ergo_retain_val(__t4553);
  ErgoVal __t4554 = a0; ergo_retain_val(__t4554);
  __cogito_container_set_vexpand(__t4553, __t4554);
  ergo_release_val(__t4553);
  ergo_release_val(__t4554);
  ErgoVal __t4555 = YV_NULLV;
  ergo_release_val(__t4555);
  ErgoVal __t4556 = self; ergo_retain_val(__t4556);
  ergo_move_into(&__ret, __t4556);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_WelcomeScreen_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4557 = self; ergo_retain_val(__t4557);
  ErgoVal __t4558 = a0; ergo_retain_val(__t4558);
  __cogito_node_set_class(__t4557, __t4558);
  ergo_release_val(__t4557);
  ergo_release_val(__t4558);
  ErgoVal __t4559 = YV_NULLV;
  ergo_release_val(__t4559);
  ErgoVal __t4560 = self; ergo_retain_val(__t4560);
  ergo_move_into(&__ret, __t4560);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_WelcomeScreen_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4561 = self; ergo_retain_val(__t4561);
  ErgoVal __t4562 = a0; ergo_retain_val(__t4562);
  __cogito_node_set_id(__t4561, __t4562);
  ergo_release_val(__t4561);
  ergo_release_val(__t4562);
  ErgoVal __t4563 = YV_NULLV;
  ergo_release_val(__t4563);
  ErgoVal __t4564 = self; ergo_retain_val(__t4564);
  ergo_move_into(&__ret, __t4564);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewDual_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4565 = self; ergo_retain_val(__t4565);
  ErgoVal __t4566 = a0; ergo_retain_val(__t4566);
  __cogito_container_add(__t4565, __t4566);
  ergo_release_val(__t4565);
  ergo_release_val(__t4566);
  ErgoVal __t4567 = YV_NULLV;
  ergo_release_val(__t4567);
  ErgoVal __t4568 = self; ergo_retain_val(__t4568);
  ergo_move_into(&__ret, __t4568);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewDual_set_ratio(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4569 = self; ergo_retain_val(__t4569);
  ErgoVal __t4570 = a0; ergo_retain_val(__t4570);
  __cogito_view_dual_set_ratio(__t4569, __t4570);
  ergo_release_val(__t4569);
  ergo_release_val(__t4570);
  ErgoVal __t4571 = YV_NULLV;
  ergo_release_val(__t4571);
  ErgoVal __t4572 = self; ergo_retain_val(__t4572);
  ergo_move_into(&__ret, __t4572);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewDual_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4573 = self; ergo_retain_val(__t4573);
  ErgoVal __t4574 = a0; ergo_retain_val(__t4574);
  __cogito_build(__t4573, __t4574);
  ergo_release_val(__t4573);
  ergo_release_val(__t4574);
  ErgoVal __t4575 = YV_NULLV;
  ergo_release_val(__t4575);
  ErgoVal __t4576 = self; ergo_retain_val(__t4576);
  ergo_move_into(&__ret, __t4576);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewDual_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4577 = self; ergo_retain_val(__t4577);
  ErgoVal __t4578 = a0; ergo_retain_val(__t4578);
  ErgoVal __t4579 = a1; ergo_retain_val(__t4579);
  ErgoVal __t4580 = a2; ergo_retain_val(__t4580);
  ErgoVal __t4581 = a3; ergo_retain_val(__t4581);
  __cogito_container_set_margins(__t4577, __t4578, __t4579, __t4580, __t4581);
  ergo_release_val(__t4577);
  ergo_release_val(__t4578);
  ergo_release_val(__t4579);
  ergo_release_val(__t4580);
  ergo_release_val(__t4581);
  ErgoVal __t4582 = YV_NULLV;
  ergo_release_val(__t4582);
  ErgoVal __t4583 = self; ergo_retain_val(__t4583);
  ergo_move_into(&__ret, __t4583);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewDual_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4584 = self; ergo_retain_val(__t4584);
  ErgoVal __t4585 = a0; ergo_retain_val(__t4585);
  __cogito_container_set_hexpand(__t4584, __t4585);
  ergo_release_val(__t4584);
  ergo_release_val(__t4585);
  ErgoVal __t4586 = YV_NULLV;
  ergo_release_val(__t4586);
  ErgoVal __t4587 = self; ergo_retain_val(__t4587);
  ergo_move_into(&__ret, __t4587);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewDual_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4588 = self; ergo_retain_val(__t4588);
  ErgoVal __t4589 = a0; ergo_retain_val(__t4589);
  __cogito_container_set_vexpand(__t4588, __t4589);
  ergo_release_val(__t4588);
  ergo_release_val(__t4589);
  ErgoVal __t4590 = YV_NULLV;
  ergo_release_val(__t4590);
  ErgoVal __t4591 = self; ergo_retain_val(__t4591);
  ergo_move_into(&__ret, __t4591);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewDual_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4592 = self; ergo_retain_val(__t4592);
  ErgoVal __t4593 = a0; ergo_retain_val(__t4593);
  __cogito_node_set_class(__t4592, __t4593);
  ergo_release_val(__t4592);
  ergo_release_val(__t4593);
  ErgoVal __t4594 = YV_NULLV;
  ergo_release_val(__t4594);
  ErgoVal __t4595 = self; ergo_retain_val(__t4595);
  ergo_move_into(&__ret, __t4595);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewDual_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4596 = self; ergo_retain_val(__t4596);
  ErgoVal __t4597 = a0; ergo_retain_val(__t4597);
  __cogito_node_set_id(__t4596, __t4597);
  ergo_release_val(__t4596);
  ergo_release_val(__t4597);
  ErgoVal __t4598 = YV_NULLV;
  ergo_release_val(__t4598);
  ErgoVal __t4599 = self; ergo_retain_val(__t4599);
  ergo_move_into(&__ret, __t4599);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewChooser_set_items(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4600 = self; ergo_retain_val(__t4600);
  ErgoVal __t4601 = a0; ergo_retain_val(__t4601);
  __cogito_view_chooser_set_items(__t4600, __t4601);
  ergo_release_val(__t4600);
  ergo_release_val(__t4601);
  ErgoVal __t4602 = YV_NULLV;
  ergo_release_val(__t4602);
  ErgoVal __t4603 = self; ergo_retain_val(__t4603);
  ergo_move_into(&__ret, __t4603);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewChooser_bind(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4604 = self; ergo_retain_val(__t4604);
  ErgoVal __t4605 = a0; ergo_retain_val(__t4605);
  __cogito_view_chooser_bind(__t4604, __t4605);
  ergo_release_val(__t4604);
  ergo_release_val(__t4605);
  ErgoVal __t4606 = YV_NULLV;
  ergo_release_val(__t4606);
  ErgoVal __t4607 = self; ergo_retain_val(__t4607);
  ergo_move_into(&__ret, __t4607);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewChooser_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4608 = self; ergo_retain_val(__t4608);
  ErgoVal __t4609 = a0; ergo_retain_val(__t4609);
  ErgoVal __t4610 = a1; ergo_retain_val(__t4610);
  ErgoVal __t4611 = a2; ergo_retain_val(__t4611);
  ErgoVal __t4612 = a3; ergo_retain_val(__t4612);
  __cogito_container_set_margins(__t4608, __t4609, __t4610, __t4611, __t4612);
  ergo_release_val(__t4608);
  ergo_release_val(__t4609);
  ergo_release_val(__t4610);
  ergo_release_val(__t4611);
  ergo_release_val(__t4612);
  ErgoVal __t4613 = YV_NULLV;
  ergo_release_val(__t4613);
  ErgoVal __t4614 = self; ergo_retain_val(__t4614);
  ergo_move_into(&__ret, __t4614);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewChooser_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4615 = self; ergo_retain_val(__t4615);
  ErgoVal __t4616 = a0; ergo_retain_val(__t4616);
  __cogito_node_set_class(__t4615, __t4616);
  ergo_release_val(__t4615);
  ergo_release_val(__t4616);
  ErgoVal __t4617 = YV_NULLV;
  ergo_release_val(__t4617);
  ErgoVal __t4618 = self; ergo_retain_val(__t4618);
  ergo_move_into(&__ret, __t4618);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_ViewChooser_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4619 = self; ergo_retain_val(__t4619);
  ErgoVal __t4620 = a0; ergo_retain_val(__t4620);
  __cogito_node_set_id(__t4619, __t4620);
  ergo_release_val(__t4619);
  ergo_release_val(__t4620);
  ErgoVal __t4621 = YV_NULLV;
  ergo_release_val(__t4621);
  ErgoVal __t4622 = self; ergo_retain_val(__t4622);
  ergo_move_into(&__ret, __t4622);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AboutWindow_set_icon(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4623 = self; ergo_retain_val(__t4623);
  ErgoVal __t4624 = a0; ergo_retain_val(__t4624);
  __cogito_about_window_set_icon(__t4623, __t4624);
  ergo_release_val(__t4623);
  ergo_release_val(__t4624);
  ErgoVal __t4625 = YV_NULLV;
  ergo_release_val(__t4625);
  ErgoVal __t4626 = self; ergo_retain_val(__t4626);
  ergo_move_into(&__ret, __t4626);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AboutWindow_set_description(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4627 = self; ergo_retain_val(__t4627);
  ErgoVal __t4628 = a0; ergo_retain_val(__t4628);
  __cogito_about_window_set_description(__t4627, __t4628);
  ergo_release_val(__t4627);
  ergo_release_val(__t4628);
  ErgoVal __t4629 = YV_NULLV;
  ergo_release_val(__t4629);
  ErgoVal __t4630 = self; ergo_retain_val(__t4630);
  ergo_move_into(&__ret, __t4630);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AboutWindow_set_website(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4631 = self; ergo_retain_val(__t4631);
  ErgoVal __t4632 = a0; ergo_retain_val(__t4632);
  __cogito_about_window_set_website(__t4631, __t4632);
  ergo_release_val(__t4631);
  ergo_release_val(__t4632);
  ErgoVal __t4633 = YV_NULLV;
  ergo_release_val(__t4633);
  ErgoVal __t4634 = self; ergo_retain_val(__t4634);
  ergo_move_into(&__ret, __t4634);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AboutWindow_set_issue_url(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4635 = self; ergo_retain_val(__t4635);
  ErgoVal __t4636 = a0; ergo_retain_val(__t4636);
  __cogito_about_window_set_issue_url(__t4635, __t4636);
  ergo_release_val(__t4635);
  ergo_release_val(__t4636);
  ErgoVal __t4637 = YV_NULLV;
  ergo_release_val(__t4637);
  ErgoVal __t4638 = self; ergo_retain_val(__t4638);
  ergo_move_into(&__ret, __t4638);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AboutWindow_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4639 = self; ergo_retain_val(__t4639);
  ErgoVal __t4640 = a0; ergo_retain_val(__t4640);
  ErgoVal __t4641 = a1; ergo_retain_val(__t4641);
  ErgoVal __t4642 = a2; ergo_retain_val(__t4642);
  ErgoVal __t4643 = a3; ergo_retain_val(__t4643);
  __cogito_container_set_margins(__t4639, __t4640, __t4641, __t4642, __t4643);
  ergo_release_val(__t4639);
  ergo_release_val(__t4640);
  ergo_release_val(__t4641);
  ergo_release_val(__t4642);
  ergo_release_val(__t4643);
  ErgoVal __t4644 = YV_NULLV;
  ergo_release_val(__t4644);
  ErgoVal __t4645 = self; ergo_retain_val(__t4645);
  ergo_move_into(&__ret, __t4645);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AboutWindow_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4646 = self; ergo_retain_val(__t4646);
  ErgoVal __t4647 = a0; ergo_retain_val(__t4647);
  __cogito_node_set_class(__t4646, __t4647);
  ergo_release_val(__t4646);
  ergo_release_val(__t4647);
  ErgoVal __t4648 = YV_NULLV;
  ergo_release_val(__t4648);
  ErgoVal __t4649 = self; ergo_retain_val(__t4649);
  ergo_move_into(&__ret, __t4649);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_AboutWindow_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4650 = self; ergo_retain_val(__t4650);
  ErgoVal __t4651 = a0; ergo_retain_val(__t4651);
  __cogito_node_set_id(__t4650, __t4651);
  ergo_release_val(__t4650);
  ergo_release_val(__t4651);
  ErgoVal __t4652 = YV_NULLV;
  ergo_release_val(__t4652);
  ErgoVal __t4653 = self; ergo_retain_val(__t4653);
  ergo_move_into(&__ret, __t4653);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SplitButton_add_menu(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4654 = self; ergo_retain_val(__t4654);
  ErgoVal __t4655 = a0; ergo_retain_val(__t4655);
  ErgoVal __t4656 = a1; ergo_retain_val(__t4656);
  __cogito_split_button_add_menu(__t4654, __t4655, __t4656);
  ergo_release_val(__t4654);
  ergo_release_val(__t4655);
  ergo_release_val(__t4656);
  ErgoVal __t4657 = YV_NULLV;
  ergo_release_val(__t4657);
  ErgoVal __t4658 = self; ergo_retain_val(__t4658);
  ergo_move_into(&__ret, __t4658);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SplitButton_add_menu_section(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4659 = self; ergo_retain_val(__t4659);
  ErgoVal __t4660 = a0; ergo_retain_val(__t4660);
  ErgoVal __t4661 = a1; ergo_retain_val(__t4661);
  __cogito_split_button_add_menu_section(__t4659, __t4660, __t4661);
  ergo_release_val(__t4659);
  ergo_release_val(__t4660);
  ergo_release_val(__t4661);
  ErgoVal __t4662 = YV_NULLV;
  ergo_release_val(__t4662);
  ErgoVal __t4663 = self; ergo_retain_val(__t4663);
  ergo_move_into(&__ret, __t4663);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SplitButton_menu_set_icon(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4664 = self; ergo_retain_val(__t4664);
  ErgoVal __t4665 = a0; ergo_retain_val(__t4665);
  __cogito_menu_set_icon(__t4664, __t4665);
  ergo_release_val(__t4664);
  ergo_release_val(__t4665);
  ErgoVal __t4666 = YV_NULLV;
  ergo_release_val(__t4666);
  ErgoVal __t4667 = self; ergo_retain_val(__t4667);
  ergo_move_into(&__ret, __t4667);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SplitButton_menu_set_shortcut(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4668 = self; ergo_retain_val(__t4668);
  ErgoVal __t4669 = a0; ergo_retain_val(__t4669);
  __cogito_menu_set_shortcut(__t4668, __t4669);
  ergo_release_val(__t4668);
  ergo_release_val(__t4669);
  ErgoVal __t4670 = YV_NULLV;
  ergo_release_val(__t4670);
  ErgoVal __t4671 = self; ergo_retain_val(__t4671);
  ergo_move_into(&__ret, __t4671);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SplitButton_menu_set_submenu(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4672 = self; ergo_retain_val(__t4672);
  ErgoVal __t4673 = a0; ergo_retain_val(__t4673);
  __cogito_menu_set_submenu(__t4672, __t4673);
  ergo_release_val(__t4672);
  ergo_release_val(__t4673);
  ErgoVal __t4674 = YV_NULLV;
  ergo_release_val(__t4674);
  ErgoVal __t4675 = self; ergo_retain_val(__t4675);
  ergo_move_into(&__ret, __t4675);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SplitButton_menu_set_toggled(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4676 = self; ergo_retain_val(__t4676);
  ErgoVal __t4677 = a0; ergo_retain_val(__t4677);
  __cogito_menu_set_toggled(__t4676, __t4677);
  ergo_release_val(__t4676);
  ergo_release_val(__t4677);
  ErgoVal __t4678 = YV_NULLV;
  ergo_release_val(__t4678);
  ErgoVal __t4679 = self; ergo_retain_val(__t4679);
  ergo_move_into(&__ret, __t4679);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SplitButton_on_click(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4680 = self; ergo_retain_val(__t4680);
  ErgoVal __t4681 = a0; ergo_retain_val(__t4681);
  __cogito_button_on_click(__t4680, __t4681);
  ergo_release_val(__t4680);
  ergo_release_val(__t4681);
  ErgoVal __t4682 = YV_NULLV;
  ergo_release_val(__t4682);
  ErgoVal __t4683 = self; ergo_retain_val(__t4683);
  ergo_move_into(&__ret, __t4683);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SplitButton_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4684 = self; ergo_retain_val(__t4684);
  ErgoVal __t4685 = a0; ergo_retain_val(__t4685);
  ErgoVal __t4686 = a1; ergo_retain_val(__t4686);
  ErgoVal __t4687 = a2; ergo_retain_val(__t4687);
  ErgoVal __t4688 = a3; ergo_retain_val(__t4688);
  __cogito_container_set_margins(__t4684, __t4685, __t4686, __t4687, __t4688);
  ergo_release_val(__t4684);
  ergo_release_val(__t4685);
  ergo_release_val(__t4686);
  ergo_release_val(__t4687);
  ergo_release_val(__t4688);
  ErgoVal __t4689 = YV_NULLV;
  ergo_release_val(__t4689);
  ErgoVal __t4690 = self; ergo_retain_val(__t4690);
  ergo_move_into(&__ret, __t4690);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SplitButton_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4691 = self; ergo_retain_val(__t4691);
  ErgoVal __t4692 = a0; ergo_retain_val(__t4692);
  __cogito_node_set_class(__t4691, __t4692);
  ergo_release_val(__t4691);
  ergo_release_val(__t4692);
  ErgoVal __t4693 = YV_NULLV;
  ergo_release_val(__t4693);
  ErgoVal __t4694 = self; ergo_retain_val(__t4694);
  ergo_move_into(&__ret, __t4694);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SplitButton_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4695 = self; ergo_retain_val(__t4695);
  ErgoVal __t4696 = a0; ergo_retain_val(__t4696);
  __cogito_node_set_id(__t4695, __t4696);
  ergo_release_val(__t4695);
  ergo_release_val(__t4696);
  ErgoVal __t4697 = YV_NULLV;
  ergo_release_val(__t4697);
  ErgoVal __t4698 = self; ergo_retain_val(__t4698);
  ergo_move_into(&__ret, __t4698);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SplitButton_set_size(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4699 = self; ergo_retain_val(__t4699);
  ErgoVal __t4700 = a0; ergo_retain_val(__t4700);
  __cogito_split_button_set_size(__t4699, __t4700);
  ergo_release_val(__t4699);
  ergo_release_val(__t4700);
  ErgoVal __t4701 = YV_NULLV;
  ergo_release_val(__t4701);
  ErgoVal __t4702 = self; ergo_retain_val(__t4702);
  ergo_move_into(&__ret, __t4702);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_SplitButton_set_variant(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4703 = self; ergo_retain_val(__t4703);
  ErgoVal __t4704 = a0; ergo_retain_val(__t4704);
  __cogito_split_button_set_variant(__t4703, __t4704);
  ergo_release_val(__t4703);
  ergo_release_val(__t4704);
  ErgoVal __t4705 = YV_NULLV;
  ergo_release_val(__t4705);
  ErgoVal __t4706 = self; ergo_retain_val(__t4706);
  ergo_move_into(&__ret, __t4706);
  return __ret;
  return __ret;
}

// ---- entry ----
static void ergo_entry(void) {
  ergo_init_main();
  ErgoVal app__40 = YV_NULLV;
  ErgoVal __t4707 = ergo_g_main_gafu_app; ergo_retain_val(__t4707);
  ErgoVal __t4708 = YV_STR(stdr_str_lit("ergo.cogito.Gafu"));
  ErgoVal __t4709 = YV_NULLV;
  {
    ErgoVal __parts287[1] = { __t4708 };
    ErgoStr* __s288 = stdr_str_from_parts(1, __parts287);
    __t4709 = YV_STR(__s288);
  }
  ergo_release_val(__t4708);
  ErgoVal __t4710 = ergo_m_cogito_App_set_appid(__t4707, __t4709);
  ergo_release_val(__t4707);
  ergo_release_val(__t4709);
  ErgoVal __t4711 = YV_STR(stdr_str_lit("Gafu"));
  ErgoVal __t4712 = YV_NULLV;
  {
    ErgoVal __parts289[1] = { __t4711 };
    ErgoStr* __s290 = stdr_str_from_parts(1, __parts289);
    __t4712 = YV_STR(__s290);
  }
  ergo_release_val(__t4711);
  ErgoVal __t4713 = ergo_m_cogito_App_set_app_name(__t4710, __t4712);
  ergo_release_val(__t4710);
  ergo_release_val(__t4712);
  ErgoVal __t4714 = ergo_g_main_ENSOR_DEFAULT; ergo_retain_val(__t4714);
  ErgoVal __t4715 = ergo_m_cogito_App_set_ensor_variant(__t4713, __t4714);
  ergo_release_val(__t4713);
  ergo_release_val(__t4714);
  ErgoVal __t4716 = YV_BOOL(false);
  ErgoVal __t4717 = YV_BOOL(false);
  ErgoVal __t4718 = ergo_m_cogito_App_set_dark_mode(__t4715, __t4716, __t4717);
  ergo_release_val(__t4715);
  ergo_release_val(__t4716);
  ergo_release_val(__t4717);
  ErgoVal __t4719 = YV_STR(stdr_str_lit("#65558F"));
  ErgoVal __t4720 = YV_NULLV;
  {
    ErgoVal __parts291[1] = { __t4719 };
    ErgoStr* __s292 = stdr_str_from_parts(1, __parts291);
    __t4720 = YV_STR(__s292);
  }
  ergo_release_val(__t4719);
  ErgoVal __t4721 = YV_BOOL(false);
  ErgoVal __t4722 = ergo_m_cogito_App_set_accent_color(__t4718, __t4720, __t4721);
  ergo_release_val(__t4718);
  ergo_release_val(__t4720);
  ergo_release_val(__t4721);
  ergo_move_into(&app__40, __t4722);
  ErgoVal __t4723 = YV_STR(stdr_str_lit("gafu.sum"));
  ErgoVal __t4724 = YV_NULLV;
  {
    ErgoVal __parts293[1] = { __t4723 };
    ErgoStr* __s294 = stdr_str_from_parts(1, __parts293);
    __t4724 = YV_STR(__s294);
  }
  ergo_release_val(__t4723);
  ergo_cogito_load_sum(__t4724);
  ergo_release_val(__t4724);
  ErgoVal __t4725 = YV_NULLV;
  ergo_release_val(__t4725);
  ErgoVal win__41 = YV_NULLV;
  ErgoVal __t4726 = YV_STR(stdr_str_lit("Gafu"));
  ErgoVal __t4727 = YV_NULLV;
  {
    ErgoVal __parts295[1] = { __t4726 };
    ErgoStr* __s296 = stdr_str_from_parts(1, __parts295);
    __t4727 = YV_STR(__s296);
  }
  ergo_release_val(__t4726);
  ErgoVal __t4728 = YV_INT(1720);
  ErgoVal __t4729 = YV_INT(760);
  ErgoVal __t4730 = ergo_cogito_window_size(__t4727, __t4728, __t4729);
  ergo_release_val(__t4727);
  ergo_release_val(__t4728);
  ergo_release_val(__t4729);
  ErgoVal __t4731 = YV_BOOL(false);
  ErgoVal __t4732 = ergo_m_cogito_Window_set_autosize(__t4730, __t4731);
  ergo_release_val(__t4730);
  ergo_release_val(__t4731);
  ErgoVal __t4733 = YV_BOOL(true);
  ErgoVal __t4734 = ergo_m_cogito_Window_set_resizable(__t4732, __t4733);
  ergo_release_val(__t4732);
  ergo_release_val(__t4733);
  ergo_move_into(&win__41, __t4734);
  ErgoVal root__42 = YV_NULLV;
  ErgoVal __t4735 = ergo_cogito_hstack();
  ErgoVal __t4736 = YV_BOOL(true);
  ErgoVal __t4737 = ergo_m_cogito_HStack_set_hexpand(__t4735, __t4736);
  ergo_release_val(__t4735);
  ergo_release_val(__t4736);
  ErgoVal __t4738 = YV_BOOL(true);
  ErgoVal __t4739 = ergo_m_cogito_HStack_set_vexpand(__t4737, __t4738);
  ergo_release_val(__t4737);
  ergo_release_val(__t4738);
  ergo_move_into(&root__42, __t4739);
  ErgoVal left__43 = YV_NULLV;
  ErgoVal __t4740 = ergo_cogito_vstack();
  ErgoVal __t4741 = YV_BOOL(true);
  ErgoVal __t4742 = ergo_m_cogito_VStack_set_hexpand(__t4740, __t4741);
  ergo_release_val(__t4740);
  ergo_release_val(__t4741);
  ErgoVal __t4743 = YV_STR(stdr_str_lit("Palette"));
  ErgoVal __t4744 = YV_NULLV;
  {
    ErgoVal __parts297[1] = { __t4743 };
    ErgoStr* __s298 = stdr_str_from_parts(1, __parts297);
    __t4744 = YV_STR(__s298);
  }
  ergo_release_val(__t4743);
  ErgoVal __t4745 = YV_STR(stdr_str_lit(""));
  ErgoVal __t4746 = ergo_cogito_appbar(__t4744, __t4745);
  ergo_release_val(__t4744);
  ergo_release_val(__t4745);
  ErgoVal __t4747 = YV_STR(stdr_str_lit("gafu-left-bar"));
  ErgoVal __t4748 = YV_NULLV;
  {
    ErgoVal __parts299[1] = { __t4747 };
    ErgoStr* __s300 = stdr_str_from_parts(1, __parts299);
    __t4748 = YV_STR(__s300);
  }
  ergo_release_val(__t4747);
  ErgoVal __t4749 = ergo_m_cogito_AppBar_set_class(__t4746, __t4748);
  ergo_release_val(__t4746);
  ergo_release_val(__t4748);
  ErgoVal __t4750 = ergo_m_cogito_VStack_add(__t4742, __t4749);
  ergo_release_val(__t4742);
  ergo_release_val(__t4749);
  ErgoVal __t4751 = ergo_main_build_palette_grid();
  ErgoVal __t4752 = ergo_m_cogito_VStack_add(__t4750, __t4751);
  ergo_release_val(__t4750);
  ergo_release_val(__t4751);
  ergo_move_into(&left__43, __t4752);
  ErgoVal __t4753 = root__42; ergo_retain_val(__t4753);
  ErgoVal __t4754 = left__43; ergo_retain_val(__t4754);
  ErgoVal __t4755 = ergo_m_cogito_HStack_add(__t4753, __t4754);
  ergo_release_val(__t4753);
  ergo_release_val(__t4754);
  ergo_release_val(__t4755);
  ErgoVal right__44 = YV_NULLV;
  ErgoVal __t4756 = ergo_cogito_vstack();
  ErgoVal __t4757 = YV_BOOL(true);
  ErgoVal __t4758 = ergo_m_cogito_VStack_set_vexpand(__t4756, __t4757);
  ergo_release_val(__t4756);
  ergo_release_val(__t4757);
  ergo_move_into(&right__44, __t4758);
  ErgoVal bar__45 = YV_NULLV;
  ErgoVal __t4759 = YV_STR(stdr_str_lit("Source Color"));
  ErgoVal __t4760 = YV_NULLV;
  {
    ErgoVal __parts301[1] = { __t4759 };
    ErgoStr* __s302 = stdr_str_from_parts(1, __parts301);
    __t4760 = YV_STR(__s302);
  }
  ergo_release_val(__t4759);
  ErgoVal __t4761 = YV_STR(stdr_str_lit(""));
  ErgoVal __t4762 = ergo_cogito_appbar(__t4760, __t4761);
  ergo_release_val(__t4760);
  ergo_release_val(__t4761);
  ErgoVal __t4763 = YV_STR(stdr_str_lit("surface-container"));
  ErgoVal __t4764 = YV_NULLV;
  {
    ErgoVal __parts303[1] = { __t4763 };
    ErgoStr* __s304 = stdr_str_from_parts(1, __parts303);
    __t4764 = YV_STR(__s304);
  }
  ergo_release_val(__t4763);
  ErgoVal __t4765 = ergo_m_cogito_AppBar_set_class(__t4762, __t4764);
  ergo_release_val(__t4762);
  ergo_release_val(__t4764);
  ergo_move_into(&bar__45, __t4765);
  ErgoVal __t4766 = bar__45; ergo_retain_val(__t4766);
  ErgoVal __t4767 = YV_STR(stdr_str_lit("sf:arrow.clockwise"));
  ErgoVal __t4768 = YV_NULLV;
  {
    ErgoVal __parts305[1] = { __t4767 };
    ErgoStr* __s306 = stdr_str_from_parts(1, __parts305);
    __t4768 = YV_STR(__s306);
  }
  ergo_release_val(__t4767);
  ErgoVal __t4769 = YV_FN(ergo_fn_new(ergo_lambda_10, 1));
  ErgoVal __t4770 = ergo_m_cogito_AppBar_add_button(__t4766, __t4768, __t4769);
  ergo_release_val(__t4766);
  ergo_release_val(__t4768);
  ergo_release_val(__t4769);
  ergo_release_val(__t4770);
  ErgoVal __t4771 = right__44; ergo_retain_val(__t4771);
  ErgoVal __t4772 = bar__45; ergo_retain_val(__t4772);
  ErgoVal __t4773 = ergo_m_cogito_VStack_add(__t4771, __t4772);
  ergo_release_val(__t4771);
  ergo_release_val(__t4772);
  ergo_release_val(__t4773);
  ErgoVal __t4774 = right__44; ergo_retain_val(__t4774);
  ErgoVal __t4775 = ergo_main_build_right_panel();
  ErgoVal __t4776 = ergo_m_cogito_VStack_add(__t4774, __t4775);
  ergo_release_val(__t4774);
  ergo_release_val(__t4775);
  ergo_release_val(__t4776);
  ErgoVal __t4777 = root__42; ergo_retain_val(__t4777);
  ErgoVal __t4778 = right__44; ergo_retain_val(__t4778);
  ErgoVal __t4779 = ergo_m_cogito_HStack_add(__t4777, __t4778);
  ergo_release_val(__t4777);
  ergo_release_val(__t4778);
  ergo_release_val(__t4779);
  ErgoVal __t4780 = win__41; ergo_retain_val(__t4780);
  ErgoVal __t4781 = root__42; ergo_retain_val(__t4781);
  ErgoVal __t4782 = ergo_m_cogito_Window_add(__t4780, __t4781);
  ergo_release_val(__t4780);
  ergo_release_val(__t4781);
  ErgoVal __t4783 = YV_BOOL(true);
  ErgoVal __t4784 = ergo_m_cogito_Window_set_resizable(__t4782, __t4783);
  ergo_release_val(__t4782);
  ergo_release_val(__t4783);
  ergo_release_val(__t4784);
  ErgoVal __t4785 = app__40; ergo_retain_val(__t4785);
  ErgoVal __t4786 = win__41; ergo_retain_val(__t4786);
  ErgoVal __t4787 = ergo_m_cogito_App_run(__t4785, __t4786);
  ergo_release_val(__t4785);
  ergo_release_val(__t4786);
  ergo_release_val(__t4787);
  ergo_release_val(bar__45);
  ergo_release_val(right__44);
  ergo_release_val(left__43);
  ergo_release_val(root__42);
  ergo_release_val(win__41);
  ergo_release_val(app__40);
}

// ---- lambda defs ----
static ErgoVal ergo_lambda_1(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4788 = YV_NULLV;
  {
    ergo_main_choose_source_image();
    ErgoVal __t4789 = YV_NULLV;
    ergo_release_val(__t4789);
  }
  ergo_move_into(&__ret, __t4788);
  return __ret;
}

static ErgoVal ergo_lambda_2(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4790 = YV_NULLV;
  {
    ErgoVal __t4791 = arg0; ergo_retain_val(__t4791);
    ErgoVal __t4792 = ergo_m_cogito_Switch_checked(__t4791);
    ergo_release_val(__t4791);
    bool __b46 = ergo_as_bool(__t4792);
    ergo_release_val(__t4792);
    if (__b46) {
      ErgoVal __t4793 = ergo_g_main_gafu_app; ergo_retain_val(__t4793);
      ErgoVal __t4794 = ergo_g_main_ENSOR_CONTENT; ergo_retain_val(__t4794);
      ErgoVal __t4795 = ergo_m_cogito_App_set_ensor_variant(__t4793, __t4794);
      ergo_release_val(__t4793);
      ergo_release_val(__t4794);
      ergo_release_val(__t4795);
    } else {
      ErgoVal __t4796 = ergo_g_main_gafu_app; ergo_retain_val(__t4796);
      ErgoVal __t4797 = ergo_g_main_ENSOR_DEFAULT; ergo_retain_val(__t4797);
      ErgoVal __t4798 = ergo_m_cogito_App_set_ensor_variant(__t4796, __t4797);
      ergo_release_val(__t4796);
      ergo_release_val(__t4797);
      ergo_release_val(__t4798);
    }
  }
  ergo_move_into(&__ret, __t4790);
  return __ret;
}

static ErgoVal ergo_lambda_3(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4799 = YV_NULLV;
  {
    ErgoVal __t4800 = YV_INT(0);
    ErgoVal __t4801 = ergo_g_main_mode_moon; ergo_retain_val(__t4801);
    ErgoVal __t4802 = ergo_g_main_mode_base; ergo_retain_val(__t4802);
    ErgoVal __t4803 = ergo_g_main_mode_bright; ergo_retain_val(__t4803);
    ErgoVal __t4804 = ergo_g_main_mode_brightest; ergo_retain_val(__t4804);
    ergo_main_set_mode_selection(__t4800, __t4801, __t4802, __t4803, __t4804);
    ergo_release_val(__t4800);
    ergo_release_val(__t4801);
    ergo_release_val(__t4802);
    ergo_release_val(__t4803);
    ergo_release_val(__t4804);
    ErgoVal __t4805 = YV_NULLV;
    ergo_release_val(__t4805);
    ErgoVal __t4806 = ergo_g_main_gafu_app; ergo_retain_val(__t4806);
    ErgoVal __t4807 = YV_INT(1);
    ErgoVal __t4808 = ergo_neg(__t4807);
    ergo_release_val(__t4807);
    ErgoVal __t4809 = ergo_m_cogito_App_set_contrast(__t4806, __t4808);
    ergo_release_val(__t4806);
    ergo_release_val(__t4808);
    ergo_release_val(__t4809);
  }
  ergo_move_into(&__ret, __t4799);
  return __ret;
}

static ErgoVal ergo_lambda_4(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4810 = YV_NULLV;
  {
    ErgoVal __t4811 = YV_INT(1);
    ErgoVal __t4812 = ergo_g_main_mode_moon; ergo_retain_val(__t4812);
    ErgoVal __t4813 = ergo_g_main_mode_base; ergo_retain_val(__t4813);
    ErgoVal __t4814 = ergo_g_main_mode_bright; ergo_retain_val(__t4814);
    ErgoVal __t4815 = ergo_g_main_mode_brightest; ergo_retain_val(__t4815);
    ergo_main_set_mode_selection(__t4811, __t4812, __t4813, __t4814, __t4815);
    ergo_release_val(__t4811);
    ergo_release_val(__t4812);
    ergo_release_val(__t4813);
    ergo_release_val(__t4814);
    ergo_release_val(__t4815);
    ErgoVal __t4816 = YV_NULLV;
    ergo_release_val(__t4816);
    ErgoVal __t4817 = ergo_g_main_gafu_app; ergo_retain_val(__t4817);
    ErgoVal __t4818 = YV_INT(0);
    ErgoVal __t4819 = ergo_m_cogito_App_set_contrast(__t4817, __t4818);
    ergo_release_val(__t4817);
    ergo_release_val(__t4818);
    ergo_release_val(__t4819);
  }
  ergo_move_into(&__ret, __t4810);
  return __ret;
}

static ErgoVal ergo_lambda_5(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4820 = YV_NULLV;
  {
    ErgoVal __t4821 = YV_INT(2);
    ErgoVal __t4822 = ergo_g_main_mode_moon; ergo_retain_val(__t4822);
    ErgoVal __t4823 = ergo_g_main_mode_base; ergo_retain_val(__t4823);
    ErgoVal __t4824 = ergo_g_main_mode_bright; ergo_retain_val(__t4824);
    ErgoVal __t4825 = ergo_g_main_mode_brightest; ergo_retain_val(__t4825);
    ergo_main_set_mode_selection(__t4821, __t4822, __t4823, __t4824, __t4825);
    ergo_release_val(__t4821);
    ergo_release_val(__t4822);
    ergo_release_val(__t4823);
    ergo_release_val(__t4824);
    ergo_release_val(__t4825);
    ErgoVal __t4826 = YV_NULLV;
    ergo_release_val(__t4826);
    ErgoVal __t4827 = ergo_g_main_gafu_app; ergo_retain_val(__t4827);
    ErgoVal __t4828 = YV_FLOAT(0.5);
    ErgoVal __t4829 = ergo_m_cogito_App_set_contrast(__t4827, __t4828);
    ergo_release_val(__t4827);
    ergo_release_val(__t4828);
    ergo_release_val(__t4829);
  }
  ergo_move_into(&__ret, __t4820);
  return __ret;
}

static ErgoVal ergo_lambda_6(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4830 = YV_NULLV;
  {
    ErgoVal __t4831 = YV_INT(3);
    ErgoVal __t4832 = ergo_g_main_mode_moon; ergo_retain_val(__t4832);
    ErgoVal __t4833 = ergo_g_main_mode_base; ergo_retain_val(__t4833);
    ErgoVal __t4834 = ergo_g_main_mode_bright; ergo_retain_val(__t4834);
    ErgoVal __t4835 = ergo_g_main_mode_brightest; ergo_retain_val(__t4835);
    ergo_main_set_mode_selection(__t4831, __t4832, __t4833, __t4834, __t4835);
    ergo_release_val(__t4831);
    ergo_release_val(__t4832);
    ergo_release_val(__t4833);
    ergo_release_val(__t4834);
    ergo_release_val(__t4835);
    ErgoVal __t4836 = YV_NULLV;
    ergo_release_val(__t4836);
    ErgoVal __t4837 = ergo_g_main_gafu_app; ergo_retain_val(__t4837);
    ErgoVal __t4838 = YV_INT(1);
    ErgoVal __t4839 = ergo_m_cogito_App_set_contrast(__t4837, __t4838);
    ergo_release_val(__t4837);
    ergo_release_val(__t4838);
    ergo_release_val(__t4839);
  }
  ergo_move_into(&__ret, __t4830);
  return __ret;
}

static ErgoVal ergo_lambda_7(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4840 = YV_NULLV;
  {
    ErgoVal __t4841 = arg0; ergo_retain_val(__t4841);
    ErgoVal __t4842 = ergo_m_cogito_ColorPicker_hex(__t4841);
    ergo_release_val(__t4841);
    ergo_main_apply_source_color(__t4842);
    ergo_release_val(__t4842);
    ErgoVal __t4843 = YV_NULLV;
    ergo_release_val(__t4843);
  }
  ergo_move_into(&__ret, __t4840);
  return __ret;
}

static ErgoVal ergo_lambda_8(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4844 = YV_NULLV;
  {
    ErgoVal __t4845 = ergo_g_main_gafu_app; ergo_retain_val(__t4845);
    ErgoVal __t4846 = ergo_g_main_source_hex; ergo_retain_val(__t4846);
    ErgoVal __t4847 = ergo_m_cogito_App_copy_to_clipboard(__t4845, __t4846);
    ergo_release_val(__t4845);
    ergo_release_val(__t4846);
    ergo_release_val(__t4847);
  }
  ergo_move_into(&__ret, __t4844);
  return __ret;
}

static ErgoVal ergo_lambda_9(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4848 = YV_NULLV;
  {
    ErgoVal __t4849 = ergo_g_main_gafu_app; ergo_retain_val(__t4849);
    ErgoVal __t4850 = arg0; ergo_retain_val(__t4850);
    ErgoVal __t4851 = ergo_m_cogito_SwitchBar_get_checked(__t4850);
    ergo_release_val(__t4850);
    ErgoVal __t4852 = YV_BOOL(false);
    ErgoVal __t4853 = ergo_m_cogito_App_set_dark_mode(__t4849, __t4851, __t4852);
    ergo_release_val(__t4849);
    ergo_release_val(__t4851);
    ergo_release_val(__t4852);
    ergo_release_val(__t4853);
  }
  ergo_move_into(&__ret, __t4848);
  return __ret;
}

static ErgoVal ergo_lambda_10(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = YV_NULLV;
  ErgoVal __t4854 = YV_NULLV;
  {
    ergo_main_reset_everything();
    ErgoVal __t4855 = YV_NULLV;
    ergo_release_val(__t4855);
  }
  ergo_move_into(&__ret, __t4854);
  return __ret;
}


int main(void) {
  #ifdef __OBJC__
  @autoreleasepool {
    ergo_runtime_init();
    ErgoVal __script_dir = YV_STR(stdr_str_lit("/Users/nayu/Developer/Cogito/extras/gafu"));
    __cogito_set_script_dir(__script_dir);
    ergo_release_val(__script_dir);
    ergo_entry();
  }
  #else
  ergo_runtime_init();
  ErgoVal __script_dir = YV_STR(stdr_str_lit("/Users/nayu/Developer/Cogito/extras/gafu"));
  __cogito_set_script_dir(__script_dir);
  ergo_release_val(__script_dir);
  ergo_entry();
  #endif
  return 0;
}
