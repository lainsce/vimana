// ---- Yis runtime (minimal) ----
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
#include <stddef.h>
#include <errno.h>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <direct.h>
#endif
#if !defined(_WIN32)
#include <dirent.h>
#endif
#include <unistd.h>

static int yis_stdout_isatty = 0;

static int yis_argc = 0;
static char **yis_argv = NULL;

void yis_set_args(int argc, char **argv) {
  yis_argc = argc;
  yis_argv = argv;
}

static void yis_runtime_init(void) {
#if defined(_WIN32)
  yis_stdout_isatty = _isatty(_fileno(stdout));
#else
  yis_stdout_isatty = isatty(fileno(stdout));
#endif
  if (!yis_stdout_isatty) {
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
  EVT_DICT,
  EVT_OBJ,
  EVT_FN
} YisTag;

typedef struct YisVal YisVal;

typedef struct YisStr {
  int ref;
  size_t len;
  char* data;
} YisStr;

typedef struct YisArr {
  int ref;
  size_t len;
  size_t cap;
  YisVal* items;
} YisArr;

struct YisVal {
  YisTag tag;
  union {
    int64_t i;
    double f;
    bool b;
    void* p;
  } as;
};

typedef struct YisDictEnt {
  YisStr* key;
  YisVal val;
} YisDictEnt;

typedef struct YisDict {
  int ref;
  size_t len;
  size_t cap;
  YisDictEnt* entries;
} YisDict;

typedef struct YisObj {
  int ref;
  void (*drop)(struct YisObj*);
} YisObj;

typedef struct YisFn {
  int ref;
  int arity;
  YisVal (*fn)(void* env, int argc, YisVal* argv);
  void* env;
  int env_size;
} YisFn;

typedef struct YisRef {
  int ref;
  YisVal val;
} YisRef;

static int stdr_len(YisVal v);
static int64_t stdr_num(YisVal v);

#define YV_NULLV ((YisVal){.tag=EVT_NULL})
#define YV_INT(x) ((YisVal){.tag=EVT_INT, .as.i=(int64_t)(x)})
#define YV_FLOAT(x) ((YisVal){.tag=EVT_FLOAT, .as.f=(double)(x)})
#define YV_BOOL(x) ((YisVal){.tag=EVT_BOOL, .as.b=(x)?true:false})
#define YV_STR(x) ((YisVal){.tag=EVT_STR, .as.p=(x)})
#define YV_ARR(x) ((YisVal){.tag=EVT_ARR, .as.p=(x)})
#define YV_DICT(x) ((YisVal){.tag=EVT_DICT, .as.p=(x)})
#define YV_OBJ(x) ((YisVal){.tag=EVT_OBJ, .as.p=(x)})
#define YV_FN(x) ((YisVal){.tag=EVT_FN, .as.p=(x)})

static void yis_trap(const char* msg) {
  fprintf(stderr, "runtime error: %s\n", msg ? msg : "unknown error");
  fprintf(stderr, "  (run with debugger for stack trace)\n");
  abort();
}

static void yis_retain_val(YisVal v);
static void yis_release_val(YisVal v);
static int64_t yis_as_int(YisVal v);
static YisRef* yis_ref_new(void);
static void yis_ref_retain(YisRef* r);
static void yis_ref_release(YisRef* r);

// Static constant strings (ref=INT32_MAX means never freed)
static YisStr yis_static_empty    = { INT32_MAX, 0, "" };
static YisStr yis_static_null     = { INT32_MAX, 4, "null" };
static YisStr yis_static_true     = { INT32_MAX, 4, "true" };
static YisStr yis_static_false    = { INT32_MAX, 5, "false" };
static YisStr yis_static_array    = { INT32_MAX, 7, "[array]" };
static YisStr yis_static_dict     = { INT32_MAX, 6, "[dict]" };
static YisStr yis_static_object   = { INT32_MAX, 8, "[object]" };
static YisStr yis_static_function = { INT32_MAX, 10, "[function]" };
static YisStr yis_static_unknown  = { INT32_MAX, 3, "<?>" };

static bool yis_static_ascii_init = false;
static YisStr yis_static_ascii[256];
static char yis_static_ascii_data[256][2];

static void yis_init_static_ascii(void) {
  if (yis_static_ascii_init) return;
  for (int i = 0; i < 256; i++) {
    yis_static_ascii_data[i][0] = (char)i;
    yis_static_ascii_data[i][1] = 0;
    yis_static_ascii[i].ref = INT32_MAX;
    yis_static_ascii[i].len = 1;
    yis_static_ascii[i].data = yis_static_ascii_data[i];
  }
  yis_static_ascii_init = true;
}

static YisStr* yis_static_char(unsigned char c) {
  if (!yis_static_ascii_init) yis_init_static_ascii();
  return &yis_static_ascii[c];
}

static YisStr* stdr_str_lit(const char* s) {
  size_t n = strlen(s);
  YisStr* st = (YisStr*)malloc(sizeof(YisStr) + n + 1);
  st->ref = 1;
  st->len = n;
  st->data = (char*)(st + 1);
  memcpy(st->data, s, n + 1);
  return st;
}

static YisStr* stdr_str_from_parts(int n, YisVal* parts);
static YisStr* stdr_to_string(YisVal v);
static YisStr* stdr_str_from_slice(const char* s, size_t len);
static YisArr* stdr_arr_new(int n);
static void yis_arr_add(YisArr* a, YisVal v);
static YisVal yis_arr_get(YisArr* a, int64_t idx);
static void yis_arr_set(YisArr* a, int64_t idx, YisVal v);
static YisVal yis_arr_remove(YisArr* a, int64_t idx);

static YisDict* stdr_dict_new(void);
static void yis_dict_set(YisDict* d, YisVal key, YisVal val);
static YisVal yis_dict_get(YisDict* d, YisVal key);
static int yis_dict_len(YisDict* d);

static YisVal stdr_str_at(YisVal v, int64_t idx) {
  if (v.tag != EVT_STR) yis_trap("str_at expects string");
  YisStr* s = (YisStr*)v.as.p;
  if (idx < 0 || (size_t)idx >= s->len) return YV_STR(&yis_static_empty);
  return YV_STR(yis_static_char((unsigned char)s->data[idx]));
}

static YisVal stdr_slice(YisVal sv, int64_t start, int64_t end) {
  if (sv.tag != EVT_STR) yis_trap("slice expects string");
  YisStr* s = (YisStr*)sv.as.p;
  size_t len = s->len;
  if (start < 0) start = 0;
  if ((size_t)start > len) start = (int64_t)len;
  if (end < start) end = start;
  if ((size_t)end > len) end = (int64_t)len;
  size_t n = (size_t)(end - start);
  if (n == 0) return YV_STR(&yis_static_empty);
  if (n == 1) return YV_STR(yis_static_char((unsigned char)s->data[start]));
  return YV_STR(stdr_str_from_slice(s->data + start, n));
}

static YisVal stdr_str_concat(YisVal a, YisVal b) {
  YisVal parts[2] = { a, b };
  return YV_STR(stdr_str_from_parts(2, parts));
}

static int64_t stdr_char_code(YisVal cv) {
  if (cv.tag != EVT_STR) yis_trap("char_code expects string");
  YisStr* s = (YisStr*)cv.as.p;
  if (s->len == 0) return 0;
  return (unsigned char)s->data[0];
}

static int stdr_len(YisVal v) {
  if (v.tag == EVT_STR) return (int)((YisStr*)v.as.p)->len;
  if (v.tag == EVT_ARR) return (int)((YisArr*)v.as.p)->len;
  if (v.tag == EVT_DICT) return (int)((YisDict*)v.as.p)->len;
  return 0;
}

static int64_t stdr_num(YisVal v) {
  return yis_as_int(v);
}

static bool stdr_is_null(YisVal v) { return v.tag == EVT_NULL; }

static void stdr_write(YisVal v) {
  YisStr* s = stdr_to_string(v);
  fwrite(s->data, 1, s->len, stdout);
  fflush(stdout);
  yis_release_val(YV_STR(s));
}

static void writef(YisVal fmt, int argc, YisVal* argv) {
  if (fmt.tag != EVT_STR) yis_trap("writef expects string");
  YisStr* s = (YisStr*)fmt.as.p;
  size_t i = 0;
  size_t seg = 0;
  int argi = 0;
  while (i < s->len) {
    if (i + 1 < s->len && s->data[i] == '{' && s->data[i + 1] == '}') {
      if (i > seg) fwrite(s->data + seg, 1, i - seg, stdout);
      if (argi < argc) {
        YisStr* ps = stdr_to_string(argv[argi++]);
        fwrite(ps->data, 1, ps->len, stdout);
        yis_release_val(YV_STR(ps));
      }
      i += 2;
      seg = i;
      continue;
    }
    i++;
  }
  if (i > seg) fwrite(s->data + seg, 1, i - seg, stdout);
  if (yis_stdout_isatty) fflush(stdout);
}

static void stdr_writef_args(YisVal fmt, YisVal args) {
  if (args.tag != EVT_ARR) yis_trap("writef expects args tuple");
  YisArr* a = (YisArr*)args.as.p;
  writef(fmt, (int)a->len, a->items);
}

static YisStr* stdr_read_line(void) {
  size_t cap = 128;
  size_t len = 0;
  char* buf = (char*)malloc(cap);
  if (!buf) yis_trap("out of memory");
  int c;
  while ((c = fgetc(stdin)) != EOF) {
    if (c == '\n') break;
    if (len + 1 >= cap) {
      cap *= 2;
      buf = (char*)realloc(buf, cap);
      if (!buf) yis_trap("out of memory");
    }
    buf[len++] = (char)c;
  }
  if (len > 0 && buf[len - 1] == '\r') len--;
  YisStr* s = (YisStr*)malloc(sizeof(YisStr) + len + 1);
  if (!s) yis_trap("out of memory");
  s->ref = 1;
  s->len = len;
  s->data = (char*)(s + 1);
  memcpy(s->data, buf, len);
  s->data[len] = 0;
  free(buf);
  return s;
}

static YisVal stdr_read_text_file(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("read_text_file expects string path");
  YisStr* path = (YisStr*)pathv.as.p;
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
  YisStr* out = (YisStr*)malloc(sizeof(YisStr) + len + 1);
  if (!out) {
    fclose(f);
    yis_trap("out of memory");
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

static YisVal stdr_write_text_file(YisVal pathv, YisVal textv) {
  if (pathv.tag != EVT_STR) yis_trap("write_text_file expects string path");
  if (textv.tag != EVT_STR) yis_trap("write_text_file expects string text");
  YisStr* path = (YisStr*)pathv.as.p;
  YisStr* text = (YisStr*)textv.as.p;
  FILE* f = fopen(path->data, "wb");
  if (!f) return YV_BOOL(false);
  size_t n = 0;
  if (text->len > 0) n = fwrite(text->data, 1, text->len, f);
  bool ok = (n == text->len) && (fclose(f) == 0);
  return YV_BOOL(ok);
}

static bool stdr_is_dir_path(const char* path) {
  if (!path || !path[0]) return false;
  struct stat st;
  if (stat(path, &st) != 0) return false;
  return S_ISDIR(st.st_mode);
}

static int stdr_mkdir_single(const char* path) {
#if defined(_WIN32)
  if (_mkdir(path) == 0) return 0;
#else
  if (mkdir(path, 0755) == 0) return 0;
#endif
  if (errno == EEXIST && stdr_is_dir_path(path)) return 0;
  return -1;
}

static YisVal stdr_ensure_dir(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("ensure_dir expects string path");
  YisStr* path = (YisStr*)pathv.as.p;
  if (!path || path->len == 0) return YV_BOOL(false);

  size_t len = path->len;
  char* buf = (char*)malloc(len + 2);
  if (!buf) yis_trap("out of memory");
  memcpy(buf, path->data, len);
  buf[len] = 0;

  while (len > 1 && (buf[len - 1] == '/' || buf[len - 1] == '\\')) {
    buf[len - 1] = 0;
    len--;
  }
  if (len == 0) {
    free(buf);
    return YV_BOOL(false);
  }

  size_t start = 0;
#if defined(_WIN32)
  if (len >= 2 &&
      (((buf[0] >= 'A' && buf[0] <= 'Z') || (buf[0] >= 'a' && buf[0] <= 'z')) &&
       buf[1] == ':')) {
    start = 2;
  }
#endif
  if (buf[0] == '/' || buf[0] == '\\') start = 1;

  for (size_t i = start; i <= len; i++) {
    char ch = buf[i];
    if (ch != '/' && ch != '\\' && ch != 0) continue;
    buf[i] = 0;
    if (buf[0] != 0) {
      if (stdr_mkdir_single(buf) != 0) {
        free(buf);
        return YV_BOOL(false);
      }
    }
    buf[i] = ch;
  }

  free(buf);
  return YV_BOOL(true);
}

static YisVal stdr_remove_file(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("remove_file expects string path");
  YisStr* path = (YisStr*)pathv.as.p;
  if (!path || path->len == 0) return YV_BOOL(false);
#if defined(_WIN32)
  if (_unlink(path->data) == 0) return YV_BOOL(true);
#else
  if (unlink(path->data) == 0) return YV_BOOL(true);
#endif
  if (errno == ENOENT) return YV_BOOL(true);
  return YV_BOOL(false);
}

static YisVal stdr_move_file(YisVal srcv, YisVal dstv) {
  if (srcv.tag != EVT_STR) yis_trap("move_file expects source path string");
  if (dstv.tag != EVT_STR) yis_trap("move_file expects destination path string");
  YisStr* src = (YisStr*)srcv.as.p;
  YisStr* dst = (YisStr*)dstv.as.p;
  if (!src || src->len == 0 || !dst || dst->len == 0) return YV_BOOL(false);
#if defined(_WIN32)
  _unlink(dst->data);
#endif
  if (rename(src->data, dst->data) == 0) return YV_BOOL(true);
  return YV_BOOL(false);
}

static bool stdr_ends_with_ci(const char* text, const char* suffix) {
  if (!text || !suffix) return false;
  size_t tn = strlen(text);
  size_t sn = strlen(suffix);
  if (sn == 0 || sn > tn) return false;
  const char* t = text + (tn - sn);
  for (size_t i = 0; i < sn; i++) {
    unsigned char a = (unsigned char)t[i];
    unsigned char b = (unsigned char)suffix[i];
    if ((unsigned char)tolower(a) != (unsigned char)tolower(b)) return false;
  }
  return true;
}

static bool stdr_name_matches_exts(const char* name, YisArr* exts) {
  if (!name || !exts) return false;
  for (size_t i = 0; i < exts->len; i++) {
    YisVal ev = exts->items[i];
    if (ev.tag != EVT_STR) continue;
    YisStr* ext = (YisStr*)ev.as.p;
    if (!ext || ext->len == 0) continue;
    if (stdr_ends_with_ci(name, ext->data)) return true;
  }
  return false;
}

static char* stdr_join_path_c(const char* dir, const char* name) {
  if (!dir || !name) return NULL;
  size_t dl = strlen(dir);
  size_t nl = strlen(name);
  bool need_sep = (dl > 0 && dir[dl - 1] != '/');
  size_t total = dl + (need_sep ? 1 : 0) + nl + 1;
  char* out = (char*)malloc(total);
  if (!out) return NULL;
  memcpy(out, dir, dl);
  size_t p = dl;
  if (need_sep) out[p++] = '/';
  memcpy(out + p, name, nl);
  out[p + nl] = 0;
  return out;
}

static int stdr_cmp_paths(const void* a, const void* b) {
  const YisVal* va = (const YisVal*)a;
  const YisVal* vb = (const YisVal*)b;
  if (va->tag != EVT_STR || vb->tag != EVT_STR) return 0;
  YisStr* sa = (YisStr*)va->as.p;
  YisStr* sb = (YisStr*)vb->as.p;
  const char* pa = (sa && sa->data) ? sa->data : "";
  const char* pb = (sb && sb->data) ? sb->data : "";
  return strcmp(pa, pb);
}

#if !defined(_WIN32)
static int64_t stdr_stat_mtime_secs(const struct stat* st) {
#if defined(__APPLE__)
  return (int64_t)st->st_mtimespec.tv_sec;
#elif defined(_WIN32)
  return (int64_t)st->st_mtime;
#else
  return (int64_t)st->st_mtim.tv_sec;
#endif
}

static void stdr_find_files_walk(const char* dir, YisArr* exts, YisArr* out) {
  DIR* dp = opendir(dir);
  if (!dp) return;
  struct dirent* ent = NULL;
  while ((ent = readdir(dp)) != NULL) {
    const char* name = ent->d_name;
    if (!name) continue;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

    char* full = stdr_join_path_c(dir, name);
    if (!full) continue;

    struct stat st;
    if (lstat(full, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        stdr_find_files_walk(full, exts, out);
      } else if (S_ISREG(st.st_mode) && stdr_name_matches_exts(name, exts)) {
        yis_arr_add(out, YV_STR(stdr_str_from_slice(full, strlen(full))));
      }
    }
    free(full);
  }
  closedir(dp);
}

static void stdr_prune_old_files_walk(const char* dir, int64_t cutoff, int64_t* removed) {
  DIR* dp = opendir(dir);
  if (!dp) return;
  struct dirent* ent = NULL;
  while ((ent = readdir(dp)) != NULL) {
    const char* name = ent->d_name;
    if (!name) continue;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

    char* full = stdr_join_path_c(dir, name);
    if (!full) continue;

    struct stat st;
    if (lstat(full, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        stdr_prune_old_files_walk(full, cutoff, removed);
      } else if (S_ISREG(st.st_mode)) {
        int64_t mt = stdr_stat_mtime_secs(&st);
        if (mt <= cutoff && unlink(full) == 0) {
          if (removed) (*removed)++;
        }
      }
    }
    free(full);
  }
  closedir(dp);
}
#endif

static YisVal stdr_find_files(YisVal rootv, YisVal extsv) {
  if (rootv.tag != EVT_STR) yis_trap("find_files expects root path string");
  if (extsv.tag != EVT_ARR) yis_trap("find_files expects extensions array");
  YisStr* root = (YisStr*)rootv.as.p;
  YisArr* exts = (YisArr*)extsv.as.p;
  YisArr* out = stdr_arr_new(8);
  if (!root || root->len == 0 || !exts) return YV_ARR(out);

#if !defined(_WIN32)
  struct stat st;
  if (stat(root->data, &st) == 0 && S_ISDIR(st.st_mode)) {
    stdr_find_files_walk(root->data, exts, out);
  }
#else
  (void)root;
  (void)exts;
#endif

  if (out->len > 1) {
    qsort(out->items, out->len, sizeof(YisVal), stdr_cmp_paths);
  }
  return YV_ARR(out);
}

static YisVal stdr_prune_files_older_than(YisVal dirv, YisVal daysv) {
  if (dirv.tag != EVT_STR) yis_trap("prune_files_older_than expects directory path string");
  YisStr* dir = (YisStr*)dirv.as.p;
  int64_t days = stdr_num(daysv);
  if (!dir || dir->len == 0 || days <= 0) return YV_INT(0);

  time_t now = time(NULL);
  if (now == (time_t)-1) return YV_INT(0);
  int64_t cutoff = (int64_t)now - (days * 86400);
  int64_t removed = 0;

#if !defined(_WIN32)
  struct stat st;
  if (stat(dir->data, &st) == 0 && S_ISDIR(st.st_mode)) {
    stdr_prune_old_files_walk(dir->data, cutoff, &removed);
  }
#else
  (void)dir;
  (void)cutoff;
#endif

  return YV_INT(removed);
}

static YisVal stdr_run_command(YisVal cmdv) {
  if (cmdv.tag != EVT_STR) yis_trap("run_command expects string");
  YisStr* cmd = (YisStr*)cmdv.as.p;
#if defined(_WIN32)
  FILE* p = _popen(cmd->data, "r");
#else
  FILE* p = popen(cmd->data, "r");
#endif
  if (!p) {
    YisArr* out = stdr_arr_new(2);
    yis_arr_add(out, YV_INT(-1));
    yis_arr_add(out, YV_STR(stdr_str_lit("")));
    return YV_ARR(out);
  }
  size_t cap = 4096;
  size_t len = 0;
  char* buf = (char*)malloc(cap);
  if (!buf) {
#if defined(_WIN32)
    _pclose(p);
#else
    pclose(p);
#endif
    yis_trap("out of memory");
  }
  int c;
  while ((c = fgetc(p)) != EOF) {
    if (len + 1 >= cap) {
      cap *= 2;
      char* n = (char*)realloc(buf, cap);
      if (!n) {
        free(buf);
#if defined(_WIN32)
        _pclose(p);
#else
        pclose(p);
#endif
        yis_trap("out of memory");
      }
      buf = n;
    }
    buf[len++] = (char)c;
  }
  buf[len] = '\0';
#if defined(_WIN32)
  int status = _pclose(p);
#else
  int status = pclose(p);
#endif
  YisStr* out_str = stdr_str_from_slice(buf, len);
  free(buf);
  YisArr* out = stdr_arr_new(2);
  yis_arr_add(out, YV_INT(status));
  yis_arr_add(out, YV_STR(out_str));
  YisVal rv; rv.tag = EVT_ARR; rv.as.p = out;
  return rv;
}

static YisVal stdr_file_exists(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("file_exists expects string");
  YisStr* s = (YisStr*)pathv.as.p;
  struct stat st;
  return YV_BOOL(stat(s->data, &st) == 0 && S_ISREG(st.st_mode));
}

static YisVal stdr_file_mtime(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("file_mtime expects string");
  YisStr* s = (YisStr*)pathv.as.p;
  struct stat st;
  if (stat(s->data, &st) != 0) return YV_INT(-1);
#if defined(__APPLE__)
  return YV_INT((int64_t)st.st_mtimespec.tv_sec);
#elif defined(_WIN32)
  return YV_INT((int64_t)st.st_mtime);
#else
  return YV_INT((int64_t)st.st_mtim.tv_sec);
#endif
}

static YisVal stdr_getcwd(void) {
  char buf[4096];
  if (getcwd(buf, sizeof(buf))) return YV_STR(stdr_str_from_slice(buf, strlen(buf)));
  return YV_STR(stdr_str_from_slice("", 0));
}

static YisVal stdr_home_dir(void) {
  const char* home = getenv("HOME");
#if defined(_WIN32)
  if (!home || !home[0]) home = getenv("USERPROFILE");
#endif
  if (!home) home = "";
  return YV_STR(stdr_str_from_slice(home, strlen(home)));
}

static bool stdr_localtime_safe(time_t ts, struct tm* out_tm) {
  if (!out_tm) return false;
#if defined(_WIN32)
  return localtime_s(out_tm, &ts) == 0;
#else
  return localtime_r(&ts, out_tm) != NULL;
#endif
}

static YisVal stdr_unix_time(void) {
  time_t now = time(NULL);
  if (now == (time_t)-1) return YV_INT(-1);
  return YV_INT((int64_t)now);
}

static YisVal stdr_current_year(void) {
  time_t now = time(NULL);
  struct tm tmv;
  if (now == (time_t)-1 || !stdr_localtime_safe(now, &tmv)) return YV_INT(0);
  return YV_INT((int64_t)(tmv.tm_year + 1900));
}

static YisVal stdr_current_month(void) {
  time_t now = time(NULL);
  struct tm tmv;
  if (now == (time_t)-1 || !stdr_localtime_safe(now, &tmv)) return YV_INT(0);
  return YV_INT((int64_t)(tmv.tm_mon + 1));
}

static YisVal stdr_current_day(void) {
  time_t now = time(NULL);
  struct tm tmv;
  if (now == (time_t)-1 || !stdr_localtime_safe(now, &tmv)) return YV_INT(0);
  return YV_INT((int64_t)tmv.tm_mday);
}

static YisVal stdr_weekday(YisVal yearv, YisVal monthv, YisVal dayv) {
  int y = (int)stdr_num(yearv);
  int m = (int)stdr_num(monthv);
  int d = (int)stdr_num(dayv);
  if (y < 1 || m < 1 || m > 12 || d < 1 || d > 31) return YV_INT(0);

  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  tmv.tm_year = y - 1900;
  tmv.tm_mon = m - 1;
  tmv.tm_mday = d;
  tmv.tm_hour = 12;
  tmv.tm_isdst = -1;

  time_t ts = mktime(&tmv);
  if (ts == (time_t)-1) return YV_INT(0);
  struct tm out_tm;
  if (!stdr_localtime_safe(ts, &out_tm)) return YV_INT(0);
  return YV_INT((int64_t)out_tm.tm_wday);
}

static bool stdr_parse_iso_ymdhm(const char* s, int* y, int* m, int* d, int* hh, int* mm) {
  if (!s || !y || !m || !d || !hh || !mm) return false;
  if (strlen(s) < 16) return false;
  if (s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':') return false;

  for (int i = 0; i < 16; i++) {
    if (i == 4 || i == 7 || i == 10 || i == 13) continue;
    if (!isdigit((unsigned char)s[i])) return false;
  }

  *y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
  *m = (s[5] - '0') * 10 + (s[6] - '0');
  *d = (s[8] - '0') * 10 + (s[9] - '0');
  *hh = (s[11] - '0') * 10 + (s[12] - '0');
  *mm = (s[14] - '0') * 10 + (s[15] - '0');

  if (*m < 1 || *m > 12 || *d < 1 || *d > 31 || *hh < 0 || *hh > 23 || *mm < 0 || *mm > 59) {
    return false;
  }
  return true;
}

static int64_t stdr_mktime_with_tz(struct tm* tmv, const char* tz_name) {
  if (!tmv) return -1;
#if defined(_WIN32)
  if (tz_name && tz_name[0]) {
    _putenv_s("TZ", tz_name);
  } else {
    _putenv_s("TZ", "");
  }
  _tzset();
  time_t ts = mktime(tmv);
  _putenv_s("TZ", "");
  _tzset();
  if (ts == (time_t)-1) return -1;
  return (int64_t)ts;
#else
  char* old_tz = getenv("TZ");
  char* old_copy = old_tz ? strdup(old_tz) : NULL;
  if (tz_name && tz_name[0]) {
    setenv("TZ", tz_name, 1);
  } else {
    unsetenv("TZ");
  }
  tzset();

  time_t ts = mktime(tmv);

  if (old_copy) {
    setenv("TZ", old_copy, 1);
    free(old_copy);
  } else {
    unsetenv("TZ");
  }
  tzset();

  if (ts == (time_t)-1) return -1;
  return (int64_t)ts;
#endif
}

static YisVal stdr_iso_to_epoch(YisVal isov, YisVal tzv) {
  if (isov.tag != EVT_STR) yis_trap("iso_to_epoch expects iso string");
  if (tzv.tag != EVT_STR && tzv.tag != EVT_NULL) yis_trap("iso_to_epoch expects timezone string");

  YisStr* iso = (YisStr*)isov.as.p;
  if (!iso || iso->len == 0) return YV_INT(-1);

  int y = 0, m = 0, d = 0, hh = 0, mm = 0;
  if (!stdr_parse_iso_ymdhm(iso->data, &y, &m, &d, &hh, &mm)) return YV_INT(-1);

  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  tmv.tm_year = y - 1900;
  tmv.tm_mon = m - 1;
  tmv.tm_mday = d;
  tmv.tm_hour = hh;
  tmv.tm_min = mm;
  tmv.tm_sec = 0;
  tmv.tm_isdst = -1;

  const char* tz_name = "";
  if (tzv.tag == EVT_STR) {
    YisStr* tz = (YisStr*)tzv.as.p;
    if (tz && tz->len > 0) tz_name = tz->data;
  }

  return YV_INT(stdr_mktime_with_tz(&tmv, tz_name));
}

static YisVal stdr_capture_shell_first_line(const char* cmd) {
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

static YisVal stdr_open_file_dialog(YisVal promptv, YisVal extv) {
  if (promptv.tag != EVT_STR) yis_trap("open_file_dialog expects prompt string");
  if (extv.tag != EVT_STR) yis_trap("open_file_dialog expects extension string");
  YisStr* prompt = (YisStr*)promptv.as.p;
  YisStr* ext = (YisStr*)extv.as.p;
#if defined(__APPLE__)
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "osascript -e 'set _p to POSIX path of (choose file of type {\"%s\"} with prompt \"%s\")' -e 'return _p' 2>/dev/null",
           ext ? ext->data : "", prompt ? prompt->data : "");
  return stdr_capture_shell_first_line(cmd);
#elif defined(__linux__)
  char cmd[8192];
  if (ext && ext->data && ext->data[0]) {
    snprintf(cmd, sizeof(cmd),
             "zenity --file-selection --title=\"%s\" --file-filter=\"*.%s\" 2>/dev/null",
             prompt ? prompt->data : "Open", ext->data);
  } else {
    snprintf(cmd, sizeof(cmd),
             "zenity --file-selection --title=\"%s\" 2>/dev/null",
             prompt ? prompt->data : "Open");
  }
  return stdr_capture_shell_first_line(cmd);
#else
  (void)prompt;
  (void)ext;
  return YV_NULLV;
#endif
}

static YisVal stdr_open_folder_dialog(YisVal promptv) {
  if (promptv.tag != EVT_STR) yis_trap("open_folder_dialog expects prompt string");
  YisStr* prompt = (YisStr*)promptv.as.p;
#if defined(__APPLE__)
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "osascript -e 'set _p to POSIX path of (choose folder with prompt \"%s\")' -e 'return _p' 2>/dev/null",
           prompt ? prompt->data : "");
  return stdr_capture_shell_first_line(cmd);
#elif defined(__linux__)
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "zenity --file-selection --directory --title=\"%s\" 2>/dev/null",
           prompt ? prompt->data : "Open Folder");
  return stdr_capture_shell_first_line(cmd);
#else
  (void)prompt;
  return YV_NULLV;
#endif
}

static YisVal stdr_save_file_dialog(YisVal promptv, YisVal default_namev, YisVal extv) {
  if (promptv.tag != EVT_STR) yis_trap("save_file_dialog expects prompt string");
  if (default_namev.tag != EVT_STR) yis_trap("save_file_dialog expects default_name string");
  if (extv.tag != EVT_STR) yis_trap("save_file_dialog expects extension string");
  YisStr* prompt = (YisStr*)promptv.as.p;
  YisStr* def = (YisStr*)default_namev.as.p;
  YisStr* ext = (YisStr*)extv.as.p;
#if defined(__APPLE__)
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "osascript -e 'set _p to POSIX path of (choose file name with prompt \"%s\" default name \"%s\")' -e 'return _p' 2>/dev/null",
           prompt ? prompt->data : "", def ? def->data : "");
  YisVal out = stdr_capture_shell_first_line(cmd);
  (void)ext;
  return out;
#elif defined(__linux__)
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "zenity --file-selection --save --confirm-overwrite --title=\"%s\" --filename=\"%s\" 2>/dev/null",
           prompt ? prompt->data : "Save", def ? def->data : "");
  (void)ext;
  return stdr_capture_shell_first_line(cmd);
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

static YisStr* stdr_str_from_slice(const char* s, size_t len) {
  YisStr* st = (YisStr*)malloc(sizeof(YisStr) + len + 1);
  if (!st) yis_trap("out of memory");
  st->ref = 1;
  st->len = len;
  st->data = (char*)(st + 1);
  if (len > 0) memcpy(st->data, s, len);
  st->data[len] = 0;
  return st;
}

static YisVal stdr_args(void) {
  YisArr* a = stdr_arr_new(yis_argc > 0 ? yis_argc : 1);
  for (int i = 0; i < yis_argc; i++) {
    const char* s = yis_argv && yis_argv[i] ? yis_argv[i] : "";
    size_t len = strlen(s);
    yis_arr_add(a, YV_STR(stdr_str_from_slice(s, len)));
  }
  return YV_ARR(a);
}

static int64_t stdr_parse_int_slice(const char* s, size_t len) {
  if (len == 0) return 0;
  char stack[64];
  char* tmp = (len < sizeof(stack)) ? stack : (char*)malloc(len + 1);
  if (!tmp) yis_trap("out of memory");
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
  if (!tmp) yis_trap("out of memory");
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

static YisVal stdr_readf_parse(YisVal fmt, YisVal line, YisVal args) {
  if (fmt.tag != EVT_STR) yis_trap("readf expects string format");
  if (line.tag != EVT_STR) yis_trap("readf expects string input");
  if (args.tag != EVT_ARR) yis_trap("readf expects args tuple");

  YisStr* fs = (YisStr*)fmt.as.p;
  YisStr* ls = (YisStr*)line.as.p;
  YisArr* a = (YisArr*)args.as.p;

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
  if (!seg_ptrs || !seg_lens) yis_trap("out of memory");

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

  YisArr* out = stdr_arr_new((int)a->len);

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

    YisVal hint = a->items[i];
    YisVal v;
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
    yis_arr_add(out, v);
  }

  if (seg_ptrs != stack_ptrs) free(seg_ptrs);
  if (seg_lens != stack_lens) free(seg_lens);

  return YV_ARR(out);
}

static YisStr* stdr_to_string(YisVal v) {
  char buf[64];
  if (v.tag == EVT_NULL) return &yis_static_null;
  if (v.tag == EVT_BOOL) return v.as.b ? &yis_static_true : &yis_static_false;
  if (v.tag == EVT_INT) {
    snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i);
    return stdr_str_lit(buf);
  }
  if (v.tag == EVT_FLOAT) {
    snprintf(buf, sizeof(buf), "%.6f", v.as.f);
    return stdr_str_lit(buf);
  }
  if (v.tag == EVT_STR) {
    yis_retain_val(v);
    return (YisStr*)v.as.p;
  }
  if (v.tag == EVT_ARR) return &yis_static_array;
  if (v.tag == EVT_DICT) return &yis_static_dict;
  if (v.tag == EVT_OBJ) return &yis_static_object;
  if (v.tag == EVT_FN) return &yis_static_function;
  return &yis_static_unknown;
}

static YisStr* stdr_str_from_parts(int n, YisVal* parts) {
  size_t total = 0;
  YisStr* stack_strs[16];
  YisStr** strs = (n <= 16) ? stack_strs : (YisStr**)malloc(sizeof(YisStr*) * (size_t)n);
  for (int i = 0; i < n; i++) {
    strs[i] = stdr_to_string(parts[i]);
    total += strs[i]->len;
  }
  YisStr* out = (YisStr*)malloc(sizeof(YisStr) + total + 1);
  out->ref = 1;
  out->len = total;
  out->data = (char*)(out + 1);
  size_t off = 0;
  for (int i = 0; i < n; i++) {
    memcpy(out->data + off, strs[i]->data, strs[i]->len);
    off += strs[i]->len;
    yis_release_val(YV_STR(strs[i]));
  }
  out->data[total] = 0;
  if (strs != stack_strs) free(strs);
  return out;
}

static void yis_retain_val(YisVal v) {
  if (v.tag == EVT_STR) { int* r = &((YisStr*)v.as.p)->ref; if (*r != INT32_MAX) (*r)++; }
  else if (v.tag == EVT_ARR) ((YisArr*)v.as.p)->ref++;
  else if (v.tag == EVT_DICT) ((YisDict*)v.as.p)->ref++;
  else if (v.tag == EVT_OBJ) ((YisObj*)v.as.p)->ref++;
  else if (v.tag == EVT_FN) ((YisFn*)v.as.p)->ref++;
}

static void yis_release_val(YisVal v) {
  if (v.tag == EVT_STR) {
    YisStr* s = (YisStr*)v.as.p;
    if (s->ref == INT32_MAX) return;
    if (--s->ref == 0) {
      if (s->data != (char*)(s + 1)) free(s->data);
      free(s);
    }
  } else if (v.tag == EVT_ARR) {
    YisArr* a = (YisArr*)v.as.p;
    if (--a->ref == 0) {
      for (size_t i = 0; i < a->len; i++) yis_release_val(a->items[i]);
      free(a->items);
      free(a);
    }
  } else if (v.tag == EVT_DICT) {
    YisDict* d = (YisDict*)v.as.p;
    if (--d->ref == 0) {
      for (size_t i = 0; i < d->len; i++) {
        yis_release_val(YV_STR(d->entries[i].key));
        yis_release_val(d->entries[i].val);
      }
      free(d->entries);
      free(d);
    }
  } else if (v.tag == EVT_OBJ) {
    YisObj* o = (YisObj*)v.as.p;
    if (--o->ref == 0) {
      if (o->drop) o->drop(o);
      free(o);
    }
  } else if (v.tag == EVT_FN) {
    YisFn* f = (YisFn*)v.as.p;
    if (--f->ref == 0) {
      if (f->env && f->env_size > 0) {
        YisRef** caps = (YisRef**)f->env;
        for (int i = 0; i < f->env_size; i++) yis_ref_release(caps[i]);
        free(f->env);
      }
      free(f);
    }
  }
}

static YisVal yis_move(YisVal* slot) {
  YisVal v = *slot;
  *slot = YV_NULLV;
  return v;
}

static void yis_move_into(YisVal* slot, YisVal v) {
  yis_retain_val(v);
  yis_release_val(*slot);
  *slot = v;
}

static int64_t yis_as_int(YisVal v) {
  if (v.tag == EVT_INT) return v.as.i;
  if (v.tag == EVT_BOOL) return v.as.b ? 1 : 0;
  if (v.tag == EVT_FLOAT) return (int64_t)v.as.f;
  if (v.tag == EVT_NULL) return 0;
  if (v.tag == EVT_STR) {
    /* Coerce string to int when possible; otherwise 0 (avoids trap until codegen is fully audited) */
    YisStr *s = (YisStr*)v.as.p;
    if (s && s->len > 0 && s->len < 32) {
      char buf[32];
      memcpy(buf, s->data, s->len);
      buf[s->len] = '\0';
      char *end = NULL;
      long x = strtol(buf, &end, 10);
      if (end && end == buf + (ptrdiff_t)s->len)
        return (int64_t)x;
    }
    return 0;
  }
  {
    const char *tag_name = "?";
    switch (v.tag) {
      case EVT_NULL: tag_name = "null"; break;
      case EVT_STR: tag_name = "string"; break;
      case EVT_ARR: tag_name = "array"; break;
      case EVT_DICT: tag_name = "dict"; break;
      case EVT_OBJ: tag_name = "object"; break;
      case EVT_FN: tag_name = "function"; break;
      default: tag_name = "unknown"; break;
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "type mismatch: expected int (got %s)", tag_name);
    yis_trap(buf);
  }
  return 0;
}

static double yis_as_float(YisVal v) {
  if (v.tag == EVT_FLOAT) return v.as.f;
  if (v.tag == EVT_INT) return (double)v.as.i;
  if (v.tag == EVT_NULL) return 0.0;
  if (v.tag == EVT_BOOL) return v.as.b ? 1.0 : 0.0;
  if (v.tag == EVT_STR) {
    YisStr *s = (YisStr*)v.as.p;
    if (s && s->len > 0 && s->len < 32) {
      char buf[32];
      memcpy(buf, s->data, s->len);
      buf[s->len] = '\0';
      char *end = NULL;
      double x = strtod(buf, &end);
      if (end && end == buf + (ptrdiff_t)s->len)
        return x;
    }
    return 0.0;
  }
  {
    const char *tag_name = "?";
    switch (v.tag) {
      case EVT_NULL: tag_name = "null"; break;
      case EVT_ARR: tag_name = "array"; break;
      case EVT_DICT: tag_name = "dict"; break;
      case EVT_OBJ: tag_name = "object"; break;
      case EVT_FN: tag_name = "function"; break;
      default: tag_name = "unknown"; break;
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "type mismatch: expected float (got %s)", tag_name);
    yis_trap(buf);
  }
  return 0.0;
}

static bool yis_as_bool(YisVal v) {
  if (v.tag == EVT_BOOL) return v.as.b;
  if (v.tag == EVT_NULL) return false;
  if (v.tag == EVT_INT) return v.as.i != 0;
  if (v.tag == EVT_FLOAT) return v.as.f != 0.0;
  if (v.tag == EVT_STR) return ((YisStr*)v.as.p)->len != 0;
  if (v.tag == EVT_ARR) return ((YisArr*)v.as.p)->len != 0;
  if (v.tag == EVT_DICT) return ((YisDict*)v.as.p)->len != 0;
  return true;
}

static YisVal yis_add(YisVal a, YisVal b) {
  if (a.tag == EVT_STR || b.tag == EVT_STR) return stdr_str_concat(a, b);
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) + yis_as_float(b));
  return YV_INT(yis_as_int(a) + yis_as_int(b));
}

static YisVal yis_sub(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) - yis_as_float(b));
  return YV_INT(yis_as_int(a) - yis_as_int(b));
}

static YisVal yis_mul(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) * yis_as_float(b));
  return YV_INT(yis_as_int(a) * yis_as_int(b));
}

static YisVal yis_div(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) / yis_as_float(b));
  return YV_INT(yis_as_int(a) / yis_as_int(b));
}

static YisVal yis_mod(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) yis_trap("% expects integer");
  return YV_INT(yis_as_int(a) % yis_as_int(b));
}

static YisVal yis_neg(YisVal a) {
  if (a.tag == EVT_FLOAT) return YV_FLOAT(-a.as.f);
  return YV_INT(-yis_as_int(a));
}

static YisVal yis_eq(YisVal a, YisVal b) {
  if (a.tag != b.tag) return YV_BOOL(false);
  switch (a.tag) {
    case EVT_NULL: return YV_BOOL(true);
    case EVT_BOOL: return YV_BOOL(a.as.b == b.as.b);
    case EVT_INT: return YV_BOOL(a.as.i == b.as.i);
    case EVT_FLOAT: return YV_BOOL(a.as.f == b.as.f);
    case EVT_STR: {
      YisStr* sa = (YisStr*)a.as.p;
      YisStr* sb = (YisStr*)b.as.p;
      if (sa->len != sb->len) return YV_BOOL(false);
      return YV_BOOL(memcmp(sa->data, sb->data, sa->len) == 0);
    }
    default: return YV_BOOL(a.as.p == b.as.p);
  }
}

static YisVal yis_ne(YisVal a, YisVal b) {
  YisVal v = yis_eq(a, b);
  return YV_BOOL(!v.as.b);
}

static YisVal yis_lt(YisVal a, YisVal b) { return YV_BOOL(yis_as_float(a) < yis_as_float(b)); }
static YisVal yis_le(YisVal a, YisVal b) { return YV_BOOL(yis_as_float(a) <= yis_as_float(b)); }
static YisVal yis_gt(YisVal a, YisVal b) { return YV_BOOL(yis_as_float(a) > yis_as_float(b)); }
static YisVal yis_ge(YisVal a, YisVal b) { return YV_BOOL(yis_as_float(a) >= yis_as_float(b)); }

static YisArr* stdr_arr_new(int n) {
  YisArr* a = (YisArr*)malloc(sizeof(YisArr));
  a->ref = 1;
  a->len = 0;
  a->cap = (n > 0) ? (size_t)n : 4;
  a->items = (YisVal*)malloc(sizeof(YisVal) * a->cap);
  return a;
}

static void yis_arr_add(YisArr* a, YisVal v) {
  if (a->len >= a->cap) {
    a->cap *= 2;
    a->items = (YisVal*)realloc(a->items, sizeof(YisVal) * a->cap);
  }
  a->items[a->len++] = v;
}

static void stdr_push(YisVal av, YisVal val) {
  if (av.tag != EVT_ARR) yis_trap("push expects array");
  YisArr* a = (YisArr*)av.as.p;
  yis_retain_val(val);
  yis_arr_add(a, val);
}

static YisVal stdr_join(YisVal av) {
  if (av.tag != EVT_ARR) yis_trap("join expects array");
  YisArr* a = (YisArr*)av.as.p;
  return YV_STR(stdr_str_from_parts((int)a->len, a->items));
}

static YisVal stdr_array_concat(YisVal av, YisVal bv) {
  if (av.tag != EVT_ARR || bv.tag != EVT_ARR) yis_trap("concat expects two arrays");
  YisArr* a = (YisArr*)av.as.p;
  YisArr* b = (YisArr*)bv.as.p;
  YisArr* out = stdr_arr_new((int)(a->len + b->len));
  for (size_t i = 0; i < a->len; i++) {
    yis_retain_val(a->items[i]);
    yis_arr_add(out, a->items[i]);
  }
  for (size_t i = 0; i < b->len; i++) {
    yis_retain_val(b->items[i]);
    yis_arr_add(out, b->items[i]);
  }
  return YV_ARR(out);
}

static YisVal yis_arr_get(YisArr* a, int64_t idx) {
  if (!a) yis_trap("array index on null");
  if (idx < 0 || (size_t)idx >= a->len) return YV_NULLV;
  YisVal v = a->items[idx];
  yis_retain_val(v);
  return v;
}

static void yis_arr_set(YisArr* a, int64_t idx, YisVal v) {
  if (idx < 0) return;
  size_t uidx = (size_t)idx;
  if (uidx >= a->len) {
    if (uidx >= a->cap) {
      size_t new_cap = a->cap ? a->cap : 4;
      while (new_cap <= uidx) new_cap *= 2;
      a->items = (YisVal*)realloc(a->items, sizeof(YisVal) * new_cap);
      a->cap = new_cap;
    }
    for (size_t i = a->len; i <= uidx; i++) a->items[i] = YV_NULLV;
    a->len = uidx + 1;
  } else {
    yis_release_val(a->items[uidx]);
  }
  yis_retain_val(v);
  a->items[uidx] = v;
}

static YisVal yis_arr_remove(YisArr* a, int64_t idx) {
  if (idx < 0 || (size_t)idx >= a->len) return YV_NULLV;
  YisVal v = a->items[idx];
  for (size_t i = (size_t)idx; i + 1 < a->len; i++) {
    a->items[i] = a->items[i + 1];
  }
  a->len--;
  return v;
}

static int yis_str_cmp(YisStr* a, YisStr* b) {
  if (a->len != b->len) return (a->len > b->len) ? 1 : -1;
  return memcmp(a->data, b->data, a->len);
}

static YisDict* stdr_dict_new(void) {
  YisDict* d = (YisDict*)malloc(sizeof(YisDict));
  d->ref = 1;
  d->len = 0;
  d->cap = 8;
  d->entries = (YisDictEnt*)malloc(sizeof(YisDictEnt) * d->cap);
  return d;
}

static void yis_dict_set(YisDict* d, YisVal key, YisVal val) {
  if (key.tag != EVT_STR) yis_trap("dict key must be string");
  YisStr* k = (YisStr*)key.as.p;
  for (size_t i = 0; i < d->len; i++) {
    if (yis_str_cmp(d->entries[i].key, k) == 0) {
      yis_retain_val(val);
      yis_release_val(d->entries[i].val);
      d->entries[i].val = val;
      return;
    }
  }
  if (d->len >= d->cap) {
    d->cap *= 2;
    d->entries = (YisDictEnt*)realloc(d->entries, sizeof(YisDictEnt) * d->cap);
  }
  yis_retain_val(key);
  yis_retain_val(val);
  d->entries[d->len].key = k;
  d->entries[d->len].val = val;
  d->len++;
}

static YisVal yis_dict_get(YisDict* d, YisVal key) {
  if (!d) return YV_NULLV;
  if ((uintptr_t)d < 4096u) return YV_NULLV;
  if (key.tag != EVT_STR) return YV_NULLV;
  YisStr* k = (YisStr*)key.as.p;
  if (!k) return YV_NULLV;
  if ((uintptr_t)k < 4096u) return YV_NULLV;
  for (size_t i = 0; i < d->len; i++) {
    if (yis_str_cmp(d->entries[i].key, k) == 0) {
      YisVal v = d->entries[i].val;
      yis_retain_val(v);
      return v;
    }
  }
  return YV_NULLV;
}

static int yis_dict_len(YisDict* d) {
  return (int)d->len;
}

static YisObj* yis_obj_new(size_t size, void (*drop)(YisObj*)) {
  YisObj* o = (YisObj*)malloc(size);
  o->ref = 1;
  o->drop = drop;
  return o;
}

static YisRef* yis_ref_new(void) {
  YisRef* r = (YisRef*)malloc(sizeof(YisRef));
  if (!r) yis_trap("out of memory");
  r->ref = 1;
  r->val = YV_NULLV;
  return r;
}

static void yis_ref_retain(YisRef* r) {
  if (r) r->ref++;
}

static void yis_ref_release(YisRef* r) {
  if (!r) return;
  if (--r->ref == 0) {
    yis_release_val(r->val);
    free(r);
  }
}

static YisFn* yi_fn_new(YisVal (*fn)(void* env, int argc, YisVal* argv), int arity) {
  YisFn* f = (YisFn*)malloc(sizeof(YisFn));
  f->ref = 1;
  f->arity = arity;
  f->fn = fn;
  f->env = NULL;
  f->env_size = 0;
  return f;
}

static YisFn* yi_fn_new_with_env(YisVal (*fn)(void* env, int argc, YisVal* argv), int arity, void* env, int env_size) {
  YisFn* f = (YisFn*)malloc(sizeof(YisFn));
  f->ref = 1;
  f->arity = arity;
  f->fn = fn;
  f->env = env;
  f->env_size = env_size;
  return f;
}

static YisVal yis_call(YisVal fval, int argc, YisVal* argv) {
  if (fval.tag != EVT_FN) yis_trap("call expects function");
  YisFn* f = (YisFn*)fval.as.p;
  if (f->arity >= 0 && f->arity != argc) yis_trap("arity mismatch");
  return f->fn(f->env, argc, argv);
}

static YisVal stdr_parse_hex(YisVal sv) {
  if (sv.tag != EVT_STR) yis_trap("parse_hex expects string");
  YisStr* s = (YisStr*)sv.as.p;
  if (s->len == 0) return YV_INT(0);
  char* end = NULL;
  long long v = strtoll(s->data, &end, 16);
  if (end == s->data) return YV_INT(0);
  return YV_INT((int64_t)v);
}

static YisVal stdr_char_from_code(YisVal cv) {
  int64_t code = yis_as_int(cv);
  if (code < 0 || code > 0x10FFFF) return YV_STR(&yis_static_empty);
  char buf[4];
  int len = 0;
  if (code <= 0x7F) {
    buf[0] = (char)code; len = 1;
  } else if (code <= 0x7FF) {
    buf[0] = (char)(0xC0 | ((code >> 6) & 0x1F));
    buf[1] = (char)(0x80 | (code & 0x3F));
    len = 2;
  } else if (code <= 0xFFFF) {
    buf[0] = (char)(0xE0 | ((code >> 12) & 0x0F));
    buf[1] = (char)(0x80 | ((code >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (code & 0x3F));
    len = 3;
  } else {
    buf[0] = (char)(0xF0 | ((code >> 18) & 0x07));
    buf[1] = (char)(0x80 | ((code >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((code >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (code & 0x3F));
    len = 4;
  }
  return YV_STR(stdr_str_from_slice(buf, (size_t)len));
}

static YisVal stdr_floor(YisVal v) {
  double x = (v.tag == EVT_FLOAT) ? v.as.f : (double)yis_as_int(v);
  return YV_INT((int64_t)floor(x));
}

static YisVal stdr_ceil(YisVal v) {
  double x = (v.tag == EVT_FLOAT) ? v.as.f : (double)yis_as_int(v);
  return YV_INT((int64_t)ceil(x));
}

static YisVal stdr_keys(YisVal dv) {
  if (dv.tag != EVT_DICT) yis_trap("keys expects dict");
  YisDict* d = (YisDict*)dv.as.p;
  YisArr* out = stdr_arr_new((int)d->len);
  for (size_t i = 0; i < d->len; i++) {
    YisVal k = YV_STR(d->entries[i].key);
    yis_retain_val(k);
    yis_arr_add(out, k);
  }
  return YV_ARR(out);
}

static YisVal stdr_replace(YisVal textv, YisVal fromv, YisVal tov) {
  if (textv.tag != EVT_STR) yis_trap("replace expects string");
  if (fromv.tag != EVT_STR) yis_trap("replace expects string");
  if (tov.tag != EVT_STR) yis_trap("replace expects string");
  YisStr* text = (YisStr*)textv.as.p;
  YisStr* from = (YisStr*)fromv.as.p;
  YisStr* to = (YisStr*)tov.as.p;
  if (from->len == 0) { yis_retain_val(textv); return textv; }
  size_t cap = text->len + 64;
  char* buf = (char*)malloc(cap);
  if (!buf) yis_trap("out of memory");
  size_t out_len = 0;
  size_t i = 0;
  while (i + from->len <= text->len) {
    if (memcmp(text->data + i, from->data, from->len) == 0) {
      size_t need = out_len + to->len;
      if (need >= cap) {
        while (need >= cap) cap *= 2;
        buf = (char*)realloc(buf, cap);
        if (!buf) yis_trap("out of memory");
      }
      memcpy(buf + out_len, to->data, to->len);
      out_len += to->len;
      i += from->len;
    } else {
      if (out_len + 1 >= cap) {
        cap *= 2;
        buf = (char*)realloc(buf, cap);
        if (!buf) yis_trap("out of memory");
      }
      buf[out_len++] = text->data[i++];
    }
  }
  size_t tail = text->len - i;
  if (tail > 0) {
    if (out_len + tail >= cap) {
      cap = out_len + tail + 1;
      buf = (char*)realloc(buf, cap);
      if (!buf) yis_trap("out of memory");
    }
    memcpy(buf + out_len, text->data + i, tail);
    out_len += tail;
  }
  buf[out_len] = 0;
  YisStr* result = stdr_str_from_slice(buf, out_len);
  free(buf);
  return YV_STR(result);
}

// ---- External module bindings ----
// Injected by codegen when the program imports an external module.

/* inlined runtime helpers */
static YisVal yis_index(YisVal obj, YisVal idx) {
  if (obj.tag == EVT_ARR) return yis_arr_get((YisArr*)obj.as.p, yis_as_int(idx));
  if (obj.tag == EVT_DICT) return yis_dict_get((YisDict*)obj.as.p, idx);
  if (obj.tag == EVT_STR) return stdr_str_at(obj, yis_as_int(idx));
  return YV_NULLV;
}

static YisVal yis_index_set(YisVal obj, YisVal idx, YisVal val) {
  if (obj.tag == EVT_ARR) { yis_arr_set((YisArr*)obj.as.p, yis_as_int(idx), val); return val; }
  if (obj.tag == EVT_DICT) { yis_dict_set((YisDict*)obj.as.p, idx, val); return val; }
  return YV_NULLV;
}

static YisVal yis_arr_lit(int n, ...) {
  YisArr *a = stdr_arr_new(n);
  va_list ap; va_start(ap, n);
  for (int i = 0; i < n; i++) yis_arr_add(a, va_arg(ap, YisVal));
  va_end(ap);
  YisVal av; av.tag = EVT_ARR; av.as.p = a;
  return av;
}

static YisVal yis_dict_lit(int n, ...) {
  YisDict *d = stdr_dict_new();
  va_list ap; va_start(ap, n);
  for (int i = 0; i < n; i++) {
    YisVal k = va_arg(ap, YisVal);
    YisVal v = va_arg(ap, YisVal);
    yis_dict_set(d, k, v);
  }
  va_end(ap);
  YisVal dv; dv.tag = EVT_DICT; dv.as.p = d;
  return dv;
}


/* vimana bindings */
#include <vimana.h>

#include <stdio.h>
#include <stdlib.h>

typedef enum VimanaHandleKind {
  VIMANA_HANDLE_SYSTEM = 1,
  VIMANA_HANDLE_SCREEN,
  VIMANA_HANDLE_DEVICE,
  VIMANA_HANDLE_DATETIME,
  VIMANA_HANDLE_FILE,
} VimanaHandleKind;

typedef struct VimanaHandle {
  YisObj base;
  void *ptr;
  int kind;
} VimanaHandle;

typedef struct VimanaFrameCtx {
  YisVal frame;
} VimanaFrameCtx;

static void vimana_handle_drop(YisObj *obj) {
  VimanaHandle *handle = (VimanaHandle *)obj;
  if (!handle)
    return;
  handle->ptr = NULL;
}

static VimanaHandle *vimana_handle_new(void *ptr, int kind) {
  VimanaHandle *handle =
      (VimanaHandle *)yis_obj_new(sizeof(VimanaHandle), vimana_handle_drop);
  handle->ptr = ptr;
  handle->kind = kind;
  return handle;
}

static YisVal vimana_wrap(void *ptr, int kind) {
  if (!ptr)
    return YV_NULLV;
  return YV_OBJ(vimana_handle_new(ptr, kind));
}

static VimanaHandle *vimana_handle_from_val(YisVal value, const char *what) {
  if (value.tag != EVT_OBJ)
    yis_trap(what);
  return (VimanaHandle *)value.as.p;
}

static vimana_system *vimana_system_from_val(YisVal value, const char *what) {
  return (vimana_system *)vimana_handle_from_val(value, what)->ptr;
}

static vimana_screen *vimana_screen_from_val(YisVal value, const char *what) {
  return (vimana_screen *)vimana_handle_from_val(value, what)->ptr;
}

static const char *vimana_required_cstr(YisVal value, YisStr **tmp) {
  if (value.tag == EVT_NULL)
    return "";
  if (value.tag == EVT_STR)
    return ((YisStr *)value.as.p)->data;
  YisStr *coerced = stdr_to_string(value);
  if (tmp)
    *tmp = coerced;
  return coerced ? coerced->data : "";
}

static bool vimana_is_numeric_tag(YisTag tag);

static const char *vimana_tag_name(YisTag tag) {
  switch (tag) {
  case EVT_NULL:
    return "null";
  case EVT_INT:
    return "int";
  case EVT_FLOAT:
    return "float";
  case EVT_BOOL:
    return "bool";
  case EVT_STR:
    return "str";
  case EVT_ARR:
    return "arr";
  case EVT_DICT:
    return "dict";
  case EVT_OBJ:
    return "obj";
  case EVT_FN:
    return "fn";
  default:
    return "?";
  }
}

static void vimana_debug_dump_icn_value(YisVal value, const char *what) {
  fprintf(stderr, "vimana.put_icn debug: %s\n", what ? what : "bad icn");
  fprintf(stderr, "  value tag=%s\n", vimana_tag_name(value.tag));
  if (value.tag == EVT_ARR) {
    YisArr *arr = (YisArr *)value.as.p;
    size_t len = arr ? arr->len : 0;
    fprintf(stderr, "  array len=%zu\n", len);
    size_t limit = len < 8 ? len : 8;
    for (size_t i = 0; i < limit; i++) {
      YisVal item = arr->items[i];
      fprintf(stderr, "  item[%zu] tag=%s", i, vimana_tag_name(item.tag));
      if (vimana_is_numeric_tag(item.tag)) {
        fprintf(stderr, " value=%lld", (long long)yis_as_int(item));
      } else if (item.tag == EVT_STR) {
        YisStr *text = (YisStr *)item.as.p;
        fprintf(stderr, " len=%zu text='%.*s'", text ? text->len : 0,
                text ? (int)text->len : 0, text ? text->data : "");
      }
      fputc('\n', stderr);
    }
  } else if (value.tag == EVT_STR) {
    YisStr *text = (YisStr *)value.as.p;
    fprintf(stderr, "  string len=%zu text='%.*s'\n", text ? text->len : 0,
            text ? (int)text->len : 0, text ? text->data : "");
  }
}

static uint16_t vimana_icn_short_from_val(YisVal value) {
  if (value.tag == EVT_INT)
    return (uint16_t)yis_as_int(value);
  YisStr *tmp = NULL;
  const char *text = vimana_required_cstr(value, &tmp);
  unsigned long out = text && text[0] ? strtoul(text, NULL, 16) : 0;
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return (uint16_t)(out & 0xFFFFu);
}

static int vimana_hex_digit_value(char ch) {
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'a' && ch <= 'f')
    return 10 + (ch - 'a');
  if (ch >= 'A' && ch <= 'F')
    return 10 + (ch - 'A');
  return -1;
}

static uint16_t vimana_parse_hex_word(const char *text, const char *what) {
  if (!text)
    yis_trap(what);
  uint16_t out = 0;
  for (int i = 0; i < 4; i++) {
    int digit = vimana_hex_digit_value(text[i]);
    if (digit < 0)
      yis_trap(what);
    out = (uint16_t)((out << 4) | (uint16_t)digit);
  }
  return out;
}

static bool vimana_is_numeric_tag(YisTag tag) {
  return tag == EVT_INT || tag == EVT_FLOAT || tag == EVT_BOOL ||
         tag == EVT_NULL;
}

static uint8_t vimana_byte_from_val(YisVal value) {
  int out = 0;
  if (value.tag != EVT_NULL)
    out = (int)yis_as_int(value);
  if (out < 0)
    out = 0;
  if (out > 255)
    out = 255;
  return (uint8_t)out;
}

static void vimana_icn_rows_from_val(YisVal value, uint16_t rows[8],
                                     const char *what) {
  for (int i = 0; i < 8; i++)
    rows[i] = 0;
  if (value.tag != EVT_ARR)
    yis_trap(what);
  YisArr *arr = (YisArr *)value.as.p;
  if (!arr)
    return;
  size_t limit = arr->len < 8 ? arr->len : 8;
  for (size_t i = 0; i < limit; i++)
    rows[i] = vimana_icn_short_from_val(arr->items[i]);
}

static void vimana_uf2_rows_from_val(YisVal value, uint16_t rows[16],
                                      const char *what) {
  for (int i = 0; i < 16; i++)
    rows[i] = 0;
  if (value.tag != EVT_ARR)
    yis_trap(what);
  YisArr *arr = (YisArr *)value.as.p;
  if (!arr)
    return;
  size_t limit = arr->len < 16 ? arr->len : 16;
  for (size_t i = 0; i < limit; i++)
    rows[i] = vimana_icn_short_from_val(arr->items[i]);
}

static void vimana_icn_byte_rows_from_val(YisVal value, uint8_t rows[8],
                                          const char *what) {
  uint16_t word_rows[8] = {0};
  vimana_icn_rows_from_val(value, word_rows, what);
  for (int i = 0; i < 8; i++)
    rows[i] = (uint8_t)(word_rows[i] & 0xFFu);
}

static void vimana_icn_words_from_val(YisVal value, uint16_t words[4],
                                      const char *what) {
  for (int i = 0; i < 4; i++)
    words[i] = 0;

  if (value.tag == EVT_STR) {
    YisStr *text = (YisStr *)value.as.p;
    if (!text || text->len != 16) {
      vimana_debug_dump_icn_value(value, what);
      yis_trap(what);
    }
    for (size_t i = 0; i < 4; i++)
      words[i] = vimana_parse_hex_word(text->data + (i * 4), what);
    return;
  }

  if (value.tag != EVT_ARR) {
    vimana_debug_dump_icn_value(value, what);
    yis_trap(what);
  }
  YisArr *arr = (YisArr *)value.as.p;
  if (!arr || arr->len < 4) {
    vimana_debug_dump_icn_value(value, what);
    yis_trap(what);
  }
  for (size_t i = 0; i < 4; i++) {
    if (vimana_is_numeric_tag(arr->items[i].tag)) {
      words[i] = (uint16_t)(yis_as_int(arr->items[i]) & 0xFFFFu);
      continue;
    }
    YisStr *tmp = NULL;
    const char *text = vimana_required_cstr(arr->items[i], &tmp);
    if (!text || strlen(text) != 4) {
      if (tmp)
        yis_release_val(YV_STR(tmp));
      vimana_debug_dump_icn_value(value, what);
      yis_trap(what);
    }
    words[i] = vimana_parse_hex_word(text, what);
    if (tmp)
      yis_release_val(YV_STR(tmp));
  }
}

static void vimana_icn_bytes_from_val(YisVal value, uint8_t rows[8],
                                      const char *what) {
  if (value.tag == EVT_ARR) {
    YisArr *arr = (YisArr *)value.as.p;
    if (arr && arr->len >= 8) {
      vimana_icn_byte_rows_from_val(value, rows, what);
      return;
    }
  }

  uint16_t words[4] = {0};
  vimana_icn_words_from_val(value, words, what);
  for (int i = 0; i < 4; i++) {
    rows[i * 2] = (uint8_t)((words[i] >> 8) & 0xFFu);
    rows[i * 2 + 1] = (uint8_t)(words[i] & 0xFFu);
  }
}

static void vimana_sprite_bytes_from_val(YisVal value, int mode,
                                         uint8_t sprite[16],
                                         const char *what) {
  memset(sprite, 0, 16);
  if (mode != 0) {
    uint16_t rows[8] = {0};
    vimana_icn_rows_from_val(value, rows, what);
    for (int row = 0; row < 8; row++) {
      uint8_t plane0 = 0;
      uint8_t plane1 = 0;
      uint16_t packed = rows[row];
      for (int col = 0; col < 8; col++) {
        uint16_t pair = (uint16_t)((packed >> ((7 - col) * 2)) & 0x3u);
        uint8_t mask = (uint8_t)(0x80u >> col);
        if ((pair & 0x1u) != 0)
          plane0 = (uint8_t)(plane0 | mask);
        if ((pair & 0x2u) != 0)
          plane1 = (uint8_t)(plane1 | mask);
      }
      sprite[row] = plane0;
      sprite[row + 8] = plane1;
    }
    return;
  }
  vimana_icn_bytes_from_val(value, sprite,
                            what ? what : "vimana sprite expects 1bpp or 2bpp rows");
}

static void vimana_frame_call(vimana_system *system, vimana_screen *screen,
                              void *user) {
  (void)system;
  (void)screen;
  VimanaFrameCtx *ctx = (VimanaFrameCtx *)user;
  if (!ctx)
    return;
  YisVal ret = yis_call(ctx->frame, 0, NULL);
  yis_release_val(ret);
}

static YisVal __vimana_system(void) {
  return vimana_wrap(vimana_system_new(), VIMANA_HANDLE_SYSTEM);
}

static void __vimana_system_run(YisVal sysv, YisVal scrv, YisVal framev) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.run expects system");
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.system.run expects screen");
  if (framev.tag != EVT_FN)
    yis_trap("vimana.system.run expects function");
  VimanaFrameCtx ctx = {.frame = framev};
  yis_retain_val(framev);
  vimana_system_run(system, screen, vimana_frame_call, &ctx);
  yis_release_val(framev);
}

static void __vimana_system_quit(YisVal sysv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.quit expects system");
  vimana_system_quit(system);
}

static YisVal __vimana_system_device(YisVal sysv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.device expects system");
  return vimana_wrap(system, VIMANA_HANDLE_DEVICE);
}

static YisVal __vimana_system_datetime(YisVal sysv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.datetime expects system");
  return vimana_wrap(system, VIMANA_HANDLE_DATETIME);
}

static YisVal __vimana_system_file(YisVal sysv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.file expects system");
  return vimana_wrap(system, VIMANA_HANDLE_FILE);
}

static YisVal __vimana_system_ticks(YisVal sysv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.ticks expects system");
  return YV_INT(vimana_system_ticks(system));
}

static void __vimana_system_sleep(YisVal sysv, YisVal msv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.sleep expects system");
  vimana_system_sleep(system, yis_as_int(msv));
}

static YisVal __vimana_system_clipboard_text(YisVal sysv) {
  vimana_system *system = vimana_system_from_val(
      sysv, "vimana.system.clipboard_text expects system");
  char *text = vimana_system_clipboard_text(system);
  if (!text)
    return YV_STR(stdr_str_lit(""));
  YisVal out = YV_STR(stdr_str_lit(text));
  free(text);
  return out;
}

static YisVal __vimana_system_set_clipboard_text(YisVal sysv, YisVal textv) {
  vimana_system *system = vimana_system_from_val(
      sysv, "vimana.system.set_clipboard_text expects system");
  YisStr *tmp = NULL;
  const char *text = vimana_required_cstr(textv, &tmp);
  bool ok = vimana_system_set_clipboard_text(system, text);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_system_home_dir(YisVal sysv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.home_dir expects system");
  char *text = vimana_system_home_dir(system);
  if (!text)
    return YV_STR(stdr_str_lit("/tmp"));
  YisVal out = YV_STR(stdr_str_lit(text));
  free(text);
  return out;
}

static YisVal __vimana_screen(YisVal titlev, YisVal widthv, YisVal heightv,
                              YisVal scalev) {
  YisStr *tmp = NULL;
  const char *title = vimana_required_cstr(titlev, &tmp);
  vimana_screen *screen = vimana_screen_new(title, (int)yis_as_int(widthv),
                                            (int)yis_as_int(heightv),
                                            (int)yis_as_int(scalev));
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return vimana_wrap(screen, VIMANA_HANDLE_SCREEN);
}

static void __vimana_screen_clear(YisVal scrv, YisVal bgv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.clear expects screen");
  vimana_screen_clear(screen, (int)yis_as_int(bgv));
}

static void __vimana_screen_resize(YisVal scrv, YisVal widthv, YisVal heightv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.resize expects screen");
  vimana_screen_resize(screen, (int)yis_as_int(widthv), (int)yis_as_int(heightv));
}

static void __vimana_screen_set_palette(YisVal scrv, YisVal slotv,
                                        YisVal colorv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_palette expects screen");
  YisStr *tmp = NULL;
  const char *color = vimana_required_cstr(colorv, &tmp);
  vimana_screen_set_palette(screen, (int)yis_as_int(slotv), color);
  if (tmp)
    yis_release_val(YV_STR(tmp));
}

static void __vimana_screen_set_font_glyph(YisVal scrv, YisVal codev,
                                           YisVal icnv) {
  vimana_screen *screen = vimana_screen_from_val(
      scrv, "vimana.screen.set_font_glyph expects screen");
  uint16_t rows[16] = {0};
  vimana_uf2_rows_from_val(
      icnv, rows,
      "vimana.screen.set_font_glyph expects 16 hex-short rows (UF2)");
  vimana_screen_set_font_glyph(screen, (int)yis_as_int(codev), rows);
}

static void __vimana_screen_set_font_chr(YisVal scrv, YisVal codev,
                                         YisVal chrv) {
  vimana_screen *screen = vimana_screen_from_val(
      scrv, "vimana.screen.set_font_chr expects screen");
  uint8_t bytes[72] = {0}; /* max 24 rows × 3 bytes/row for UF3 */
  if (chrv.tag != EVT_ARR)
    yis_trap("vimana.screen.set_font_chr expects byte array");
  YisArr *arr = (YisArr *)chrv.as.p;
  int len = 0;
  if (arr) {
    len = (int)(arr->len < 72 ? arr->len : 72);
    for (int i = 0; i < len; i++)
      bytes[i] = vimana_byte_from_val(arr->items[i]);
  }
  vimana_screen_set_font_chr(screen, (int)yis_as_int(codev), bytes, len);
}

static void __vimana_screen_set_font_width(YisVal scrv, YisVal codev,
                                            YisVal widthv) {
  vimana_screen *screen = vimana_screen_from_val(
      scrv, "vimana.screen.set_font_width expects screen");
  vimana_screen_set_font_width(screen, (int)yis_as_int(codev),
                               (int)yis_as_int(widthv));
}

static void __vimana_screen_set_font_size(YisVal scrv, YisVal sizev) {
  vimana_screen *screen = vimana_screen_from_val(
      scrv, "vimana.screen.set_font_size expects screen");
  vimana_screen_set_font_size(screen, (int)yis_as_int(sizev));
}

static void __vimana_screen_set_sprite(YisVal scrv, YisVal addrv, YisVal spritev,
                                       YisVal modev) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_sprite expects screen");
  int mode = (int)yis_as_int(modev);
  uint8_t sprite[16] = {0};
  vimana_sprite_bytes_from_val(spritev, mode, sprite,
                               "vimana.screen.set_sprite expects 8 byte rows or 8 hex-short 2bpp rows");
  vimana_screen_set_sprite(screen, (int)yis_as_int(addrv), sprite, mode);
}

static void __vimana_screen_set_x(YisVal scrv, YisVal xv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_x expects screen");
  vimana_screen_set_x(screen, (int)yis_as_int(xv));
}

static void __vimana_screen_set_y(YisVal scrv, YisVal yv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_y expects screen");
  vimana_screen_set_y(screen, (int)yis_as_int(yv));
}

static void __vimana_screen_set_addr(YisVal scrv, YisVal addrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_addr expects screen");
  vimana_screen_set_addr(screen, (int)yis_as_int(addrv));
}

static void __vimana_screen_set_auto(YisVal scrv, YisVal autov) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_auto expects screen");
  vimana_screen_set_auto(screen, (int)yis_as_int(autov));
}

static void __vimana_screen_put(YisVal scrv, YisVal xv, YisVal yv,
                                YisVal glyphv, YisVal fgv, YisVal bgv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.put expects screen");
  YisStr *tmp = NULL;
  const char *glyph = vimana_required_cstr(glyphv, &tmp);
  vimana_screen_put(screen, (int)yis_as_int(xv), (int)yis_as_int(yv), glyph,
                    (int)yis_as_int(fgv), (int)yis_as_int(bgv));
  if (tmp)
    yis_release_val(YV_STR(tmp));
}

static void __vimana_screen_put_icn(YisVal scrv, YisVal xv, YisVal yv,
                                    YisVal icnv, YisVal fgv, YisVal bgv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.put_icn expects screen");
  uint8_t rows[8] = {0};
  vimana_icn_bytes_from_val(
      icnv, rows,
    "vimana.screen.put_icn expects 4 hex words or 8 byte rows");
  vimana_screen_put_icn(screen, (int)yis_as_int(xv), (int)yis_as_int(yv), rows,
                        (int)yis_as_int(fgv), (int)yis_as_int(bgv));
}

static void __vimana_screen_put_text(YisVal scrv, YisVal xv, YisVal yv,
                                     YisVal textv, YisVal fgv, YisVal bgv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.put_text expects screen");
  YisStr *tmp = NULL;
  const char *text = vimana_required_cstr(textv, &tmp);
  vimana_screen_put_text(screen, (int)yis_as_int(xv), (int)yis_as_int(yv),
                         text, (int)yis_as_int(fgv), (int)yis_as_int(bgv));
  if (tmp)
    yis_release_val(YV_STR(tmp));
}

static void __vimana_screen_sprite(YisVal scrv, YisVal ctrlv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.sprite expects screen");
  vimana_screen_sprite(screen, (int)yis_as_int(ctrlv));
}

static void __vimana_screen_pixel(YisVal scrv, YisVal ctrlv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.pixel expects screen");
  vimana_screen_pixel(screen, (int)yis_as_int(ctrlv));
}

static void __vimana_screen_present(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.present expects screen");
  vimana_screen_present(screen);
}

static void __vimana_screen_draw_titlebar(YisVal scrv, YisVal bgv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.draw_titlebar expects screen");
  vimana_screen_draw_titlebar(screen, (int)yis_as_int(bgv));
}

static void __vimana_screen_set_titlebar_title(YisVal scrv, YisVal titlev) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_titlebar_title expects screen");
  YisStr *tmp = NULL;
  const char *title = vimana_required_cstr(titlev, &tmp);
  vimana_screen_set_titlebar_title(screen, title);
  if (tmp)
    yis_release_val(YV_STR(tmp));
}

static void __vimana_screen_set_titlebar_button(YisVal scrv, YisVal showv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_titlebar_button expects screen");
  vimana_screen_set_titlebar_button(screen, yis_as_int(showv) != 0);
}

static YisVal __vimana_screen_titlebar_button_pressed(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.titlebar_button_pressed expects screen");
  return YV_BOOL(vimana_screen_titlebar_button_pressed(screen));
}

static YisVal __vimana_screen_x(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.x expects screen");
  return YV_INT(vimana_screen_x(screen));
}

static YisVal __vimana_screen_y(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.y expects screen");
  return YV_INT(vimana_screen_y(screen));
}

static YisVal __vimana_screen_addr(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.addr expects screen");
  return YV_INT(vimana_screen_addr(screen));
}

static YisVal __vimana_screen_auto(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.auto expects screen");
  return YV_INT(vimana_screen_auto(screen));
}

static YisVal __vimana_screen_width(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.width expects screen");
  return YV_INT(vimana_screen_width(screen));
}

static YisVal __vimana_screen_height(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.height expects screen");
  return YV_INT(vimana_screen_height(screen));
}

static YisVal __vimana_screen_scale(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.scale expects screen");
  return YV_INT(vimana_screen_scale(screen));
}

static void __vimana_device_poll(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.poll expects device");
  vimana_device_poll(system);
}

static YisVal __vimana_device_key_down(YisVal devv, YisVal scancodev) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.key_down expects device");
  return YV_BOOL(vimana_device_key_down(system, (int)yis_as_int(scancodev)));
}

static YisVal __vimana_device_key_pressed(YisVal devv, YisVal scancodev) {
  vimana_system *system = vimana_system_from_val(
      devv, "vimana.device.key_pressed expects device");
  return YV_BOOL(
      vimana_device_key_pressed(system, (int)yis_as_int(scancodev)));
}

static YisVal __vimana_device_mouse_down(YisVal devv, YisVal buttonv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.mouse_down expects device");
  return YV_BOOL(vimana_device_mouse_down(system, (int)yis_as_int(buttonv)));
}

static YisVal __vimana_device_mouse_pressed(YisVal devv, YisVal buttonv) {
  vimana_system *system = vimana_system_from_val(
      devv, "vimana.device.mouse_pressed expects device");
  return YV_BOOL(
      vimana_device_mouse_pressed(system, (int)yis_as_int(buttonv)));
}

static YisVal __vimana_device_pointer_x(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.pointer_x expects device");
  return YV_INT(vimana_device_pointer_x(system));
}

static YisVal __vimana_device_pointer_y(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.pointer_y expects device");
  return YV_INT(vimana_device_pointer_y(system));
}

static YisVal __vimana_device_tile_x(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.tile_x expects device");
  return YV_INT(vimana_device_tile_x(system));
}

static YisVal __vimana_device_tile_y(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.tile_y expects device");
  return YV_INT(vimana_device_tile_y(system));
}

static YisVal __vimana_device_wheel_x(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.wheel_x expects device");
  return YV_INT(vimana_device_wheel_x(system));
}

static YisVal __vimana_device_wheel_y(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.wheel_y expects device");
  return YV_INT(vimana_device_wheel_y(system));
}

static YisVal __vimana_device_text_input(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.text_input expects device");
  const char *text = vimana_device_text_input(system);
  return YV_STR(stdr_str_lit(text ? text : ""));
}

static YisVal __vimana_datetime_now(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.now expects datetime");
  return YV_INT(vimana_datetime_now(system));
}

static YisVal __vimana_datetime_year(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.year expects datetime");
  return YV_INT(vimana_datetime_year(system));
}

static YisVal __vimana_datetime_month(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.month expects datetime");
  return YV_INT(vimana_datetime_month(system));
}

static YisVal __vimana_datetime_day(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.day expects datetime");
  return YV_INT(vimana_datetime_day(system));
}

static YisVal __vimana_datetime_hour(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.hour expects datetime");
  return YV_INT(vimana_datetime_hour(system));
}

static YisVal __vimana_datetime_minute(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.minute expects datetime");
  return YV_INT(vimana_datetime_minute(system));
}

static YisVal __vimana_datetime_second(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.second expects datetime");
  return YV_INT(vimana_datetime_second(system));
}

static YisVal __vimana_datetime_weekday(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.weekday expects datetime");
  return YV_INT(vimana_datetime_weekday(system));
}

static YisVal __vimana_datetime_year_at(YisVal datetimev, YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.year_at expects datetime");
  return YV_INT(vimana_datetime_year_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_datetime_month_at(YisVal datetimev,
                                         YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.month_at expects datetime");
  return YV_INT(vimana_datetime_month_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_datetime_day_at(YisVal datetimev, YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.day_at expects datetime");
  return YV_INT(vimana_datetime_day_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_datetime_hour_at(YisVal datetimev, YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.hour_at expects datetime");
  return YV_INT(vimana_datetime_hour_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_datetime_minute_at(YisVal datetimev,
                                          YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.minute_at expects datetime");
  return YV_INT(vimana_datetime_minute_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_datetime_second_at(YisVal datetimev,
                                          YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.second_at expects datetime");
  return YV_INT(vimana_datetime_second_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_datetime_weekday_at(YisVal datetimev,
                                           YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.weekday_at expects datetime");
  return YV_INT(vimana_datetime_weekday_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_file_read_text(YisVal filev, YisVal pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.read_text expects file");
  YisStr *tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &tmp);
  char *text = vimana_file_read_text(system, path);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  if (!text)
    return YV_STR(stdr_str_lit(""));
  YisVal out = YV_STR(stdr_str_lit(text));
  free(text);
  return out;
}

static YisVal __vimana_file_read_bytes(YisVal filev, YisVal pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.read_bytes expects file");
  YisStr *tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &tmp);
  size_t size = 0;
  unsigned char *bytes = vimana_file_read_bytes(system, path, &size);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  YisArr *arr = stdr_arr_new((int)size);
  if (arr)
    arr->len = size;
  for (size_t i = 0; i < size; i++)
    yis_arr_set(arr, i, YV_INT(bytes[i]));
  free(bytes);
  return YV_ARR(arr);
}

static YisVal __vimana_file_write_text(YisVal filev, YisVal pathv,
                                       YisVal textv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.write_text expects file");
  YisStr *path_tmp = NULL;
  YisStr *text_tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &path_tmp);
  const char *text = vimana_required_cstr(textv, &text_tmp);
  bool ok = vimana_file_write_text(system, path, text);
  if (path_tmp)
    yis_release_val(YV_STR(path_tmp));
  if (text_tmp)
    yis_release_val(YV_STR(text_tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_file_write_bytes(YisVal filev, YisVal pathv,
                                        YisVal bytesv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.write_bytes expects file");
  if (bytesv.tag != EVT_ARR)
    yis_trap("vimana.file.write_bytes expects byte array");
  YisStr *path_tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &path_tmp);
  YisArr *arr = (YisArr *)bytesv.as.p;
  size_t len = arr ? arr->len : 0;
  unsigned char *bytes = len > 0 ? (unsigned char *)malloc(len) : NULL;
  if (len > 0 && !bytes) {
    if (path_tmp)
      yis_release_val(YV_STR(path_tmp));
    return YV_BOOL(false);
  }
  for (size_t i = 0; i < len; i++)
    bytes[i] = vimana_byte_from_val(arr->items[i]);
  bool ok = vimana_file_write_bytes(system, path, bytes, len);
  free(bytes);
  if (path_tmp)
    yis_release_val(YV_STR(path_tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_file_exists(YisVal filev, YisVal pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.exists expects file");
  YisStr *tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &tmp);
  bool ok = vimana_file_exists(system, path);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_file_remove(YisVal filev, YisVal pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.remove expects file");
  YisStr *tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &tmp);
  bool ok = vimana_file_remove(system, path);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_file_rename(YisVal filev, YisVal pathv,
                                   YisVal new_pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.rename expects file");
  YisStr *path_tmp = NULL;
  YisStr *new_tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &path_tmp);
  const char *new_path = vimana_required_cstr(new_pathv, &new_tmp);
  bool ok = vimana_file_rename(system, path, new_path);
  if (path_tmp)
    yis_release_val(YV_STR(path_tmp));
  if (new_tmp)
    yis_release_val(YV_STR(new_tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_file_list(YisVal filev, YisVal pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.list expects file");
  YisStr *tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &tmp);
  int count = 0;
  char **items = vimana_file_list(system, path, &count);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  YisArr *arr = stdr_arr_new((int)(count > 0 ? count : 0));
  if (arr)
    arr->len = (size_t)(count > 0 ? count : 0);
  for (int i = 0; i < count; i++) {
    yis_arr_set(arr, (size_t)i,
                YV_STR(stdr_str_lit(items[i] ? items[i] : "")));
  }
  vimana_file_list_free(items, count);
  return YV_ARR(arr);
}

static YisVal __vimana_file_is_dir(YisVal filev, YisVal pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.is_dir expects file");
  YisStr *tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &tmp);
  bool ok = vimana_file_is_dir(system, path);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

/* ── Subprocess IPC bindings ──────────────────────────────────────────── */

static YisVal __vimana_system_spawn(YisVal sysv, YisVal cmdv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.spawn expects system");
  YisStr *tmp = NULL;
  const char *cmd = vimana_required_cstr(cmdv, &tmp);
  vimana_process *proc = vimana_process_spawn(system, cmd);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  if (!proc)
    return YV_INT(0);
  /* store proc pointer as a number (opaque handle) */
  return YV_INT((uintptr_t)proc);
}

static YisVal __vimana_system_proc_write(YisVal sysv, YisVal procv,
                                         YisVal textv) {
  (void)sysv;
  vimana_process *proc = (vimana_process *)(uintptr_t)yis_as_int(procv);
  YisStr *tmp = NULL;
  const char *text = vimana_required_cstr(textv, &tmp);
  bool ok = vimana_process_write(proc, text);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_system_proc_read_line(YisVal sysv, YisVal procv) {
  (void)sysv;
  vimana_process *proc = (vimana_process *)(uintptr_t)yis_as_int(procv);
  char *line = vimana_process_read_line(proc);
  if (!line)
    return YV_STR(stdr_str_lit(""));
  YisVal result = YV_STR(stdr_str_lit(line));
  free(line);
  return result;
}

static YisVal __vimana_system_proc_running(YisVal sysv, YisVal procv) {
  (void)sysv;
  vimana_process *proc = (vimana_process *)(uintptr_t)yis_as_int(procv);
  return YV_BOOL(vimana_process_running(proc));
}

static void __vimana_system_proc_kill(YisVal sysv, YisVal procv) {
  (void)sysv;
  vimana_process *proc = (vimana_process *)(uintptr_t)yis_as_int(procv);
  vimana_process_kill(proc);
}

static void __vimana_system_proc_free(YisVal sysv, YisVal procv) {
  (void)sysv;
  vimana_process *proc = (vimana_process *)(uintptr_t)yis_as_int(procv);
  vimana_process_free(proc);
}


/* begin embedded module: stdr */
static void yis_stdr_writef(YisVal, YisVal);
static YisVal yis_stdr_readf(YisVal, YisVal);
static void yis_stdr_write(YisVal);
static YisVal yis_stdr_is_null(YisVal);
static YisVal yis_stdr_str(YisVal);
static YisVal yis_stdr_len(YisVal);
static YisVal yis_stdr_num(YisVal);
static YisVal yis_stdr_slice(YisVal, YisVal, YisVal);
static YisVal yis_stdr_concat(YisVal, YisVal);
static YisVal yis_stdr_join(YisVal);
static void yis_stdr_push(YisVal, YisVal);
static YisVal yis_stdr_str_concat(YisVal, YisVal);
static YisVal yis_stdr_char_code(YisVal);
static YisVal yis_stdr_char_from_code(YisVal);
static YisVal yis_stdr_char_at(YisVal, YisVal);
static YisVal yis_stdr_substring(YisVal, YisVal);
static YisVal yis_stdr_substring_len(YisVal, YisVal, YisVal);
static YisVal yis_stdr_replace(YisVal, YisVal, YisVal);
static YisVal yis_stdr_parse_hex(YisVal);
static YisVal yis_stdr_floor(YisVal);
static YisVal yis_stdr_ceil(YisVal);
static YisVal yis_stdr_keys(YisVal);
static YisVal yis_stdr_read_text_file(YisVal);
static YisVal yis_stdr_write_text_file(YisVal, YisVal);
static YisVal yis_stdr_ensure_dir(YisVal);
static YisVal yis_stdr_remove_file(YisVal);
static YisVal yis_stdr_move_file(YisVal, YisVal);
static YisVal yis_stdr_find_files(YisVal, YisVal);
static YisVal yis_stdr_prune_files_older_than(YisVal, YisVal);
static YisVal yis_stdr_run_command(YisVal);
static YisVal yis_stdr_file_exists(YisVal);
static YisVal yis_stdr_file_mtime(YisVal);
static YisVal yis_stdr_getcwd(void);
static YisVal yis_stdr_home_dir(void);
static YisVal yis_stdr_unix_time(void);
static YisVal yis_stdr_current_year(void);
static YisVal yis_stdr_current_month(void);
static YisVal yis_stdr_current_day(void);
static YisVal yis_stdr_weekday(YisVal, YisVal, YisVal);
static YisVal yis_stdr_iso_to_epoch(YisVal, YisVal);
static YisVal yis_stdr_is_ws(YisVal);
static YisVal yis_stdr_trim(YisVal);
static YisVal yis_stdr_starts_with(YisVal, YisVal);
static YisVal yis_stdr_ends_with(YisVal, YisVal);
static YisVal yis_stdr_index_of(YisVal, YisVal);
static YisVal yis_stdr_contains(YisVal, YisVal);
static YisVal yis_stdr_last_index_of(YisVal, YisVal);
static YisVal yis_stdr_shell_quote(YisVal);
static YisVal yis_stdr_basename(YisVal);
static YisVal yis_stdr_dirname(YisVal);
static YisVal yis_stdr_stem(YisVal);
static YisVal yis_stdr_join_path(YisVal, YisVal);
static YisVal yis_stdr_args(void);
static YisVal yis_stdr_open_file_dialog(YisVal, YisVal);
static YisVal yis_stdr_open_folder_dialog(YisVal);
static YisVal yis_stdr_save_file_dialog(YisVal, YisVal, YisVal);
static YisVal yis_stdr_is_none(YisVal v_this);
static YisVal __fnwrap_stdr_writef(void*,int,YisVal*);
static YisVal __fnwrap_stdr_readf(void*,int,YisVal*);
static YisVal __fnwrap_stdr_write(void*,int,YisVal*);
static YisVal __fnwrap_stdr_is_null(void*,int,YisVal*);
static YisVal __fnwrap_stdr_str(void*,int,YisVal*);
static YisVal __fnwrap_stdr_len(void*,int,YisVal*);
static YisVal __fnwrap_stdr_num(void*,int,YisVal*);
static YisVal __fnwrap_stdr_slice(void*,int,YisVal*);
static YisVal __fnwrap_stdr_concat(void*,int,YisVal*);
static YisVal __fnwrap_stdr_join(void*,int,YisVal*);
static YisVal __fnwrap_stdr_push(void*,int,YisVal*);
static YisVal __fnwrap_stdr_str_concat(void*,int,YisVal*);
static YisVal __fnwrap_stdr_char_code(void*,int,YisVal*);
static YisVal __fnwrap_stdr_char_from_code(void*,int,YisVal*);
static YisVal __fnwrap_stdr_char_at(void*,int,YisVal*);
static YisVal __fnwrap_stdr_substring(void*,int,YisVal*);
static YisVal __fnwrap_stdr_substring_len(void*,int,YisVal*);
static YisVal __fnwrap_stdr_replace(void*,int,YisVal*);
static YisVal __fnwrap_stdr_parse_hex(void*,int,YisVal*);
static YisVal __fnwrap_stdr_floor(void*,int,YisVal*);
static YisVal __fnwrap_stdr_ceil(void*,int,YisVal*);
static YisVal __fnwrap_stdr_keys(void*,int,YisVal*);
static YisVal __fnwrap_stdr_read_text_file(void*,int,YisVal*);
static YisVal __fnwrap_stdr_write_text_file(void*,int,YisVal*);
static YisVal __fnwrap_stdr_ensure_dir(void*,int,YisVal*);
static YisVal __fnwrap_stdr_remove_file(void*,int,YisVal*);
static YisVal __fnwrap_stdr_move_file(void*,int,YisVal*);
static YisVal __fnwrap_stdr_find_files(void*,int,YisVal*);
static YisVal __fnwrap_stdr_prune_files_older_than(void*,int,YisVal*);
static YisVal __fnwrap_stdr_run_command(void*,int,YisVal*);
static YisVal __fnwrap_stdr_file_exists(void*,int,YisVal*);
static YisVal __fnwrap_stdr_file_mtime(void*,int,YisVal*);
static YisVal __fnwrap_stdr_getcwd(void*,int,YisVal*);
static YisVal __fnwrap_stdr_home_dir(void*,int,YisVal*);
static YisVal __fnwrap_stdr_unix_time(void*,int,YisVal*);
static YisVal __fnwrap_stdr_current_year(void*,int,YisVal*);
static YisVal __fnwrap_stdr_current_month(void*,int,YisVal*);
static YisVal __fnwrap_stdr_current_day(void*,int,YisVal*);
static YisVal __fnwrap_stdr_weekday(void*,int,YisVal*);
static YisVal __fnwrap_stdr_iso_to_epoch(void*,int,YisVal*);
static YisVal __fnwrap_stdr_is_ws(void*,int,YisVal*);
static YisVal __fnwrap_stdr_trim(void*,int,YisVal*);
static YisVal __fnwrap_stdr_starts_with(void*,int,YisVal*);
static YisVal __fnwrap_stdr_ends_with(void*,int,YisVal*);
static YisVal __fnwrap_stdr_index_of(void*,int,YisVal*);
static YisVal __fnwrap_stdr_contains(void*,int,YisVal*);
static YisVal __fnwrap_stdr_last_index_of(void*,int,YisVal*);
static YisVal __fnwrap_stdr_shell_quote(void*,int,YisVal*);
static YisVal __fnwrap_stdr_basename(void*,int,YisVal*);
static YisVal __fnwrap_stdr_dirname(void*,int,YisVal*);
static YisVal __fnwrap_stdr_stem(void*,int,YisVal*);
static YisVal __fnwrap_stdr_join_path(void*,int,YisVal*);
static YisVal __fnwrap_stdr_args(void*,int,YisVal*);
static YisVal __fnwrap_stdr_open_file_dialog(void*,int,YisVal*);
static YisVal __fnwrap_stdr_open_folder_dialog(void*,int,YisVal*);
static YisVal __fnwrap_stdr_save_file_dialog(void*,int,YisVal*);

// cask stdr
static YisVal yis_stdr_is_none(YisVal v_this) {
  return yis_eq(v_this, YV_NULLV);
}

static void yis_stdr_writef(YisVal v_fmt, YisVal v_args) {
#line 28 "../../../Yis/src/stdlib/stdr.yi"
  (void)(stdr_writef_args(v_fmt, v_args));
}

static YisVal yis_stdr_readf(YisVal v_fmt, YisVal v_args) {
#line 33 "../../../Yis/src/stdlib/stdr.yi"
  (void)(stdr_writef_args(v_fmt, v_args));
#line 34 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_line = YV_STR(stdr_read_line()); yis_retain_val(v_line);
#line 35 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_parsed = stdr_readf_parse(v_fmt, v_line, v_args); yis_retain_val(v_parsed);
#line 36 "../../../Yis/src/stdlib/stdr.yi"
  return v_line;
  (void)(v_parsed);
}

static void yis_stdr_write(YisVal v_x) {
#line 41 "../../../Yis/src/stdlib/stdr.yi"
  (void)(stdr_write(v_x));
}

static YisVal yis_stdr_is_null(YisVal v_x) {
#line 46 "../../../Yis/src/stdlib/stdr.yi"
  return yis_stdr_is_none(v_x);
}

static YisVal yis_stdr_str(YisVal v_x) {
  return YV_STR(stdr_to_string(v_x));
}

static YisVal yis_stdr_len(YisVal v_x) {
#line 60 "../../../Yis/src/stdlib/stdr.yi"
  return YV_INT(stdr_len(v_x));
}

static YisVal yis_stdr_num(YisVal v_x) {
#line 65 "../../../Yis/src/stdlib/stdr.yi"
  return YV_INT(stdr_num(v_x));
}

static YisVal yis_stdr_slice(YisVal v_s, YisVal v_start, YisVal v_end) {
#line 71 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_slice(v_s, yis_as_int(v_start), yis_as_int(v_end));
}

static YisVal yis_stdr_concat(YisVal v_a, YisVal v_b) {
#line 77 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_array_concat(v_a, v_b);
}

static YisVal yis_stdr_join(YisVal v_arr) {
#line 82 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_len = YV_INT(stdr_len(v_arr)); yis_retain_val(v_len);
#line 83 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_len, YV_INT(0)))) {
    return YV_STR(stdr_str_lit(""));
  }
#line 84 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_len, YV_INT(1)))) {
    return YV_STR(stdr_to_string(yis_index(v_arr, YV_INT(0))));
  }
#line 85 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_a = yis_arr_lit(0); yis_retain_val(v_a);
#line 86 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 87 "../../../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_lt(v_i, v_len)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
    (void)(yis_stdr_push(v_a, YV_STR(stdr_to_string(yis_index(v_arr, v_i)))));
  }
#line 88 "../../../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_gt(v_len, YV_INT(1)));) {
#line 89 "../../../Yis/src/stdlib/stdr.yi"
    YisVal v_j = YV_INT(0); yis_retain_val(v_j);
#line 90 "../../../Yis/src/stdlib/stdr.yi"
    YisVal v_k = YV_INT(0); yis_retain_val(v_k);
#line 91 "../../../Yis/src/stdlib/stdr.yi"
    for (; yis_as_bool(yis_lt(yis_add(v_j, YV_INT(1)), v_len)); (void)((yis_move_into(&v_j, yis_add(v_j, YV_INT(2))), v_j))) {
#line 92 "../../../Yis/src/stdlib/stdr.yi"
      (void)(yis_index_set(v_a, v_k, yis_stdr_str_concat(yis_index(v_a, v_j), yis_index(v_a, yis_add(v_j, YV_INT(1))))));
#line 93 "../../../Yis/src/stdlib/stdr.yi"
      (void)((yis_move_into(&v_k, yis_add(v_k, YV_INT(1))), v_k));
    }
#line 94 "../../../Yis/src/stdlib/stdr.yi"
    if (yis_as_bool(yis_lt(v_j, v_len))) {
      (void)(yis_index_set(v_a, v_k, yis_index(v_a, v_j)));
      (void)((yis_move_into(&v_k, yis_add(v_k, YV_INT(1))), v_k));
    }
#line 95 "../../../Yis/src/stdlib/stdr.yi"
    (void)((yis_move_into(&v_len, v_k), v_len));
  }
#line 96 "../../../Yis/src/stdlib/stdr.yi"
  return yis_index(v_a, YV_INT(0));
}

static void yis_stdr_push(YisVal v_arr, YisVal v_val) {
#line 101 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_a_ref = v_arr; yis_retain_val(v_a_ref);
#line 102 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_idx = YV_INT(stdr_len(v_a_ref)); yis_retain_val(v_idx);
#line 103 "../../../Yis/src/stdlib/stdr.yi"
  (void)(yis_index_set(v_a_ref, v_idx, v_val));
}

static YisVal yis_stdr_str_concat(YisVal v_a, YisVal v_b) {
#line 109 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_str_concat(v_a, v_b);
}

static YisVal yis_stdr_char_code(YisVal v_c) {
#line 115 "../../../Yis/src/stdlib/stdr.yi"
  return YV_INT(stdr_char_code(v_c));
}

static YisVal yis_stdr_char_from_code(YisVal v_code) {
#line 121 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_char_from_code(v_code);
}

static YisVal yis_stdr_char_at(YisVal v_s, YisVal v_idx) {
#line 126 "../../../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_s, v_idx, yis_add(v_idx, YV_INT(1)));
}

static YisVal yis_stdr_substring(YisVal v_s, YisVal v_start) {
#line 131 "../../../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_s, v_start, YV_INT(stdr_len(v_s)));
}

static YisVal yis_stdr_substring_len(YisVal v_s, YisVal v_start, YisVal v_n) {
#line 136 "../../../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_s, v_start, yis_add(v_start, v_n));
}

static YisVal yis_stdr_replace(YisVal v_text, YisVal v_from, YisVal v_to) {
#line 142 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_replace(v_text, v_from, v_to);
}

static YisVal yis_stdr_parse_hex(YisVal v_s) {
#line 148 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_parse_hex(v_s);
}

static YisVal yis_stdr_floor(YisVal v_x) {
#line 154 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_floor(v_x);
}

static YisVal yis_stdr_ceil(YisVal v_x) {
#line 160 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_ceil(v_x);
}

static YisVal yis_stdr_keys(YisVal v_d) {
#line 166 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_keys(v_d);
}

static YisVal yis_stdr_read_text_file(YisVal v_path) {
#line 171 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_read_text_file(v_path);
}

static YisVal yis_stdr_write_text_file(YisVal v_path, YisVal v_text) {
#line 176 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_write_text_file(v_path, v_text);
}

static YisVal yis_stdr_ensure_dir(YisVal v_path) {
#line 183 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_ensure_dir(v_path);
}

static YisVal yis_stdr_remove_file(YisVal v_path) {
#line 190 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_remove_file(v_path);
}

static YisVal yis_stdr_move_file(YisVal v_src, YisVal v_dst) {
#line 197 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_move_file(v_src, v_dst);
}

static YisVal yis_stdr_find_files(YisVal v_root, YisVal v_exts) {
#line 204 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_find_files(v_root, v_exts);
}

static YisVal yis_stdr_prune_files_older_than(YisVal v_dir, YisVal v_days) {
#line 211 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_prune_files_older_than(v_dir, v_days);
}

static YisVal yis_stdr_run_command(YisVal v_cmd) {
#line 218 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_run_command(v_cmd);
}

static YisVal yis_stdr_file_exists(YisVal v_path) {
#line 225 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_file_exists(v_path);
}

static YisVal yis_stdr_file_mtime(YisVal v_path) {
#line 232 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_file_mtime(v_path);
}

static YisVal yis_stdr_getcwd(void) {
#line 256 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_getcwd();
}

static YisVal yis_stdr_home_dir(void) {
#line 260 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_home_dir();
}

static YisVal yis_stdr_unix_time(void) {
#line 264 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_unix_time();
}

static YisVal yis_stdr_current_year(void) {
#line 268 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_current_year();
}

static YisVal yis_stdr_current_month(void) {
#line 272 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_current_month();
}

static YisVal yis_stdr_current_day(void) {
#line 276 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_current_day();
}

static YisVal yis_stdr_weekday(YisVal v_year, YisVal v_month, YisVal v_day) {
#line 280 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_weekday(v_year, v_month, v_day);
}

static YisVal yis_stdr_iso_to_epoch(YisVal v_iso, YisVal v_tz) {
#line 284 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_iso_to_epoch(v_iso, v_tz);
}

static YisVal yis_stdr_is_ws(YisVal v_ch) {
#line 288 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_c = yis_stdr_char_code(v_ch); yis_retain_val(v_c);
#line 289 "../../../Yis/src/stdlib/stdr.yi"
  return YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_c, YV_INT(32))) || yis_as_bool(yis_eq(v_c, YV_INT(9))))) || yis_as_bool(yis_eq(v_c, YV_INT(10))))) || yis_as_bool(yis_eq(v_c, YV_INT(13))));
}

static YisVal yis_stdr_trim(YisVal v_text) {
#line 293 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_n = YV_INT(stdr_len(v_text)); yis_retain_val(v_n);
#line 294 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_start = YV_INT(0); yis_retain_val(v_start);
#line 295 "../../../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_lt(v_start, v_n)); (void)((yis_move_into(&v_start, yis_add(v_start, YV_INT(1))), v_start))) {
#line 296 "../../../Yis/src/stdlib/stdr.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_is_ws(yis_stdr_slice(v_text, v_start, yis_add(v_start, YV_INT(1)))))))) {
#line 297 "../../../Yis/src/stdlib/stdr.yi"
      break;
    }
  }
#line 299 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_end = v_n; yis_retain_val(v_end);
#line 300 "../../../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_gt(v_end, v_start)); (void)((yis_move_into(&v_end, yis_sub(v_end, YV_INT(1))), v_end))) {
#line 301 "../../../Yis/src/stdlib/stdr.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_is_ws(yis_stdr_slice(v_text, yis_sub(v_end, YV_INT(1)), v_end)))))) {
#line 302 "../../../Yis/src/stdlib/stdr.yi"
      break;
    }
  }
#line 304 "../../../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_text, v_start, v_end);
}

static YisVal yis_stdr_starts_with(YisVal v_text, YisVal v_prefix) {
#line 308 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_tn = YV_INT(stdr_len(v_text)); yis_retain_val(v_tn);
#line 309 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_pn = YV_INT(stdr_len(v_prefix)); yis_retain_val(v_pn);
#line 310 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_pn, YV_INT(0)))) {
#line 311 "../../../Yis/src/stdlib/stdr.yi"
    return YV_BOOL(true);
  }
#line 312 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_gt(v_pn, v_tn))) {
#line 313 "../../../Yis/src/stdlib/stdr.yi"
    return YV_BOOL(false);
  }
#line 314 "../../../Yis/src/stdlib/stdr.yi"
  return yis_eq(yis_stdr_slice(v_text, YV_INT(0), v_pn), v_prefix);
}

static YisVal yis_stdr_ends_with(YisVal v_text, YisVal v_suffix) {
#line 318 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_tn = YV_INT(stdr_len(v_text)); yis_retain_val(v_tn);
#line 319 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_sn = YV_INT(stdr_len(v_suffix)); yis_retain_val(v_sn);
#line 320 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_sn, YV_INT(0)))) {
#line 321 "../../../Yis/src/stdlib/stdr.yi"
    return YV_BOOL(true);
  }
#line 322 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_gt(v_sn, v_tn))) {
#line 323 "../../../Yis/src/stdlib/stdr.yi"
    return YV_BOOL(false);
  }
#line 324 "../../../Yis/src/stdlib/stdr.yi"
  return yis_eq(yis_stdr_slice(v_text, yis_sub(v_tn, v_sn), v_tn), v_suffix);
}

static YisVal yis_stdr_index_of(YisVal v_text, YisVal v_needle) {
#line 328 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_tn = YV_INT(stdr_len(v_text)); yis_retain_val(v_tn);
#line 329 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_nn = YV_INT(stdr_len(v_needle)); yis_retain_val(v_nn);
#line 330 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_nn, YV_INT(0)))) {
#line 331 "../../../Yis/src/stdlib/stdr.yi"
    return YV_INT(0);
  }
#line 332 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_gt(v_nn, v_tn))) {
#line 333 "../../../Yis/src/stdlib/stdr.yi"
    return yis_neg(YV_INT(1));
  }
#line 335 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 336 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_end = yis_sub(v_tn, v_nn); yis_retain_val(v_end);
#line 337 "../../../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_le(v_i, v_end)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 338 "../../../Yis/src/stdlib/stdr.yi"
    if (yis_as_bool(yis_eq(yis_stdr_slice(v_text, v_i, yis_add(v_i, v_nn)), v_needle))) {
#line 339 "../../../Yis/src/stdlib/stdr.yi"
      return v_i;
    }
  }
#line 340 "../../../Yis/src/stdlib/stdr.yi"
  return yis_neg(YV_INT(1));
}

static YisVal yis_stdr_contains(YisVal v_text, YisVal v_needle) {
#line 344 "../../../Yis/src/stdlib/stdr.yi"
  return yis_ge(yis_stdr_index_of(v_text, v_needle), YV_INT(0));
}

static YisVal yis_stdr_last_index_of(YisVal v_text, YisVal v_needle) {
#line 348 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_tn = YV_INT(stdr_len(v_text)); yis_retain_val(v_tn);
#line 349 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_nn = YV_INT(stdr_len(v_needle)); yis_retain_val(v_nn);
#line 350 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_nn, YV_INT(0))) || yis_as_bool(yis_gt(v_nn, v_tn))))) {
#line 351 "../../../Yis/src/stdlib/stdr.yi"
    return yis_neg(YV_INT(1));
  }
#line 353 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_pos = yis_neg(YV_INT(1)); yis_retain_val(v_pos);
#line 354 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 355 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_end = yis_sub(v_tn, v_nn); yis_retain_val(v_end);
#line 356 "../../../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_le(v_i, v_end)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 357 "../../../Yis/src/stdlib/stdr.yi"
    if (yis_as_bool(yis_eq(yis_stdr_slice(v_text, v_i, yis_add(v_i, v_nn)), v_needle))) {
#line 358 "../../../Yis/src/stdlib/stdr.yi"
      (void)((yis_move_into(&v_pos, v_i), v_pos));
    }
  }
#line 359 "../../../Yis/src/stdlib/stdr.yi"
  return v_pos;
}

static YisVal yis_stdr_shell_quote(YisVal v_text) {
#line 363 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_p = yis_arr_lit(0); yis_retain_val(v_p);
#line 364 "../../../Yis/src/stdlib/stdr.yi"
  (void)(yis_stdr_push(v_p, YV_STR(stdr_str_lit("'"))));
#line 365 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_chunk_start = YV_INT(0); yis_retain_val(v_chunk_start);
#line 366 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_n = YV_INT(stdr_len(v_text)); yis_retain_val(v_n);
#line 367 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 368 "../../../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_lt(v_i, v_n)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 369 "../../../Yis/src/stdlib/stdr.yi"
    if (yis_as_bool(yis_eq(yis_stdr_slice(v_text, v_i, yis_add(v_i, YV_INT(1))), YV_STR(stdr_str_lit("'"))))) {
#line 370 "../../../Yis/src/stdlib/stdr.yi"
      if (yis_as_bool(yis_lt(v_chunk_start, v_i))) {
        (void)(yis_stdr_push(v_p, yis_stdr_slice(v_text, v_chunk_start, v_i)));
      }
#line 371 "../../../Yis/src/stdlib/stdr.yi"
      (void)(yis_stdr_push(v_p, YV_STR(stdr_str_lit("'\"'\"'"))));
#line 372 "../../../Yis/src/stdlib/stdr.yi"
      (void)((yis_move_into(&v_chunk_start, yis_add(v_i, YV_INT(1))), v_chunk_start));
    }
  }
#line 373 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_lt(v_chunk_start, v_n))) {
    (void)(yis_stdr_push(v_p, yis_stdr_slice(v_text, v_chunk_start, v_n)));
  }
#line 374 "../../../Yis/src/stdlib/stdr.yi"
  (void)(yis_stdr_push(v_p, YV_STR(stdr_str_lit("'"))));
#line 375 "../../../Yis/src/stdlib/stdr.yi"
  return yis_stdr_join(v_p);
}

static YisVal yis_stdr_basename(YisVal v_path) {
#line 379 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_p = yis_stdr_trim(v_path); yis_retain_val(v_p);
#line 380 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_idx = yis_stdr_last_index_of(v_p, YV_STR(stdr_str_lit("/"))); yis_retain_val(v_idx);
#line 381 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_lt(v_idx, YV_INT(0)))) {
#line 382 "../../../Yis/src/stdlib/stdr.yi"
    return v_p;
  }
#line 383 "../../../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_p, yis_add(v_idx, YV_INT(1)), YV_INT(stdr_len(v_p)));
}

static YisVal yis_stdr_dirname(YisVal v_path) {
#line 387 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_p = yis_stdr_trim(v_path); yis_retain_val(v_p);
#line 388 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_idx = yis_stdr_last_index_of(v_p, YV_STR(stdr_str_lit("/"))); yis_retain_val(v_idx);
#line 389 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_le(v_idx, YV_INT(0)))) {
#line 390 "../../../Yis/src/stdlib/stdr.yi"
    return YV_STR(stdr_str_lit(""));
  }
#line 391 "../../../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_p, YV_INT(0), v_idx);
}

static YisVal yis_stdr_stem(YisVal v_file_name) {
#line 395 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_idx = yis_stdr_last_index_of(v_file_name, YV_STR(stdr_str_lit("."))); yis_retain_val(v_idx);
#line 396 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_le(v_idx, YV_INT(0)))) {
#line 397 "../../../Yis/src/stdlib/stdr.yi"
    return v_file_name;
  }
#line 398 "../../../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_file_name, YV_INT(0), v_idx);
}

static YisVal yis_stdr_join_path(YisVal v_dir, YisVal v_name) {
#line 402 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_base = yis_stdr_trim(v_dir); yis_retain_val(v_base);
#line 403 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_leaf = yis_stdr_trim(v_name); yis_retain_val(v_leaf);
#line 405 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_base)), YV_INT(0)))) {
#line 406 "../../../Yis/src/stdlib/stdr.yi"
    return v_leaf;
  }
#line 407 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_leaf)), YV_INT(0)))) {
#line 408 "../../../Yis/src/stdlib/stdr.yi"
    return v_base;
  }
#line 410 "../../../Yis/src/stdlib/stdr.yi"
  YisVal v_out = v_base; yis_retain_val(v_out);
#line 411 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_ends_with(v_out, YV_STR(stdr_str_lit("/"))))))) {
#line 412 "../../../Yis/src/stdlib/stdr.yi"
    (void)((yis_move_into(&v_out, yis_stdr_str_concat(v_out, YV_STR(stdr_str_lit("/")))), v_out));
  }
#line 413 "../../../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_stdr_starts_with(v_leaf, YV_STR(stdr_str_lit("/"))))) {
#line 414 "../../../Yis/src/stdlib/stdr.yi"
    (void)((yis_move_into(&v_leaf, yis_stdr_slice(v_leaf, YV_INT(1), YV_INT(stdr_len(v_leaf)))), v_leaf));
  }
#line 415 "../../../Yis/src/stdlib/stdr.yi"
  return yis_stdr_str_concat(v_out, v_leaf);
}

static YisVal yis_stdr_args(void) {
#line 421 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_args();
}

static YisVal yis_stdr_open_file_dialog(YisVal v_prompt, YisVal v_extension) {
#line 426 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_open_file_dialog(v_prompt, v_extension);
}

static YisVal yis_stdr_open_folder_dialog(YisVal v_prompt) {
#line 431 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_open_folder_dialog(v_prompt);
}

static YisVal yis_stdr_save_file_dialog(YisVal v_prompt, YisVal v_default_name, YisVal v_extension) {
#line 436 "../../../Yis/src/stdlib/stdr.yi"
  return stdr_save_file_dialog(v_prompt, v_default_name, v_extension);
}

static YisVal __fnwrap_stdr_writef(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_stdr_writef(__a0, __a1);
  return YV_NULLV;
}

static YisVal __fnwrap_stdr_readf(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_readf(__a0, __a1);
}

static YisVal __fnwrap_stdr_write(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_stdr_write(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_stdr_is_null(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_is_null(__a0);
}

static YisVal __fnwrap_stdr_str(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_str(__a0);
}

static YisVal __fnwrap_stdr_len(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_len(__a0);
}

static YisVal __fnwrap_stdr_num(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_num(__a0);
}

static YisVal __fnwrap_stdr_slice(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  return yis_stdr_slice(__a0, __a1, __a2);
}

static YisVal __fnwrap_stdr_concat(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_concat(__a0, __a1);
}

static YisVal __fnwrap_stdr_join(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_join(__a0);
}

static YisVal __fnwrap_stdr_push(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_stdr_push(__a0, __a1);
  return YV_NULLV;
}

static YisVal __fnwrap_stdr_str_concat(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_str_concat(__a0, __a1);
}

static YisVal __fnwrap_stdr_char_code(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_char_code(__a0);
}

static YisVal __fnwrap_stdr_char_from_code(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_char_from_code(__a0);
}

static YisVal __fnwrap_stdr_char_at(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_char_at(__a0, __a1);
}

static YisVal __fnwrap_stdr_substring(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_substring(__a0, __a1);
}

static YisVal __fnwrap_stdr_substring_len(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  return yis_stdr_substring_len(__a0, __a1, __a2);
}

static YisVal __fnwrap_stdr_replace(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  return yis_stdr_replace(__a0, __a1, __a2);
}

static YisVal __fnwrap_stdr_parse_hex(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_parse_hex(__a0);
}

static YisVal __fnwrap_stdr_floor(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_floor(__a0);
}

static YisVal __fnwrap_stdr_ceil(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_ceil(__a0);
}

static YisVal __fnwrap_stdr_keys(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_keys(__a0);
}

static YisVal __fnwrap_stdr_read_text_file(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_read_text_file(__a0);
}

static YisVal __fnwrap_stdr_write_text_file(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_write_text_file(__a0, __a1);
}

static YisVal __fnwrap_stdr_ensure_dir(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_ensure_dir(__a0);
}

static YisVal __fnwrap_stdr_remove_file(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_remove_file(__a0);
}

static YisVal __fnwrap_stdr_move_file(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_move_file(__a0, __a1);
}

static YisVal __fnwrap_stdr_find_files(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_find_files(__a0, __a1);
}

static YisVal __fnwrap_stdr_prune_files_older_than(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_prune_files_older_than(__a0, __a1);
}

static YisVal __fnwrap_stdr_run_command(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_run_command(__a0);
}

static YisVal __fnwrap_stdr_file_exists(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_file_exists(__a0);
}

static YisVal __fnwrap_stdr_file_mtime(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_file_mtime(__a0);
}

static YisVal __fnwrap_stdr_getcwd(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_stdr_getcwd();
}

static YisVal __fnwrap_stdr_home_dir(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_stdr_home_dir();
}

static YisVal __fnwrap_stdr_unix_time(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_stdr_unix_time();
}

static YisVal __fnwrap_stdr_current_year(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_stdr_current_year();
}

static YisVal __fnwrap_stdr_current_month(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_stdr_current_month();
}

static YisVal __fnwrap_stdr_current_day(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_stdr_current_day();
}

static YisVal __fnwrap_stdr_weekday(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  return yis_stdr_weekday(__a0, __a1, __a2);
}

static YisVal __fnwrap_stdr_iso_to_epoch(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_iso_to_epoch(__a0, __a1);
}

static YisVal __fnwrap_stdr_is_ws(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_is_ws(__a0);
}

static YisVal __fnwrap_stdr_trim(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_trim(__a0);
}

static YisVal __fnwrap_stdr_starts_with(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_starts_with(__a0, __a1);
}

static YisVal __fnwrap_stdr_ends_with(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_ends_with(__a0, __a1);
}

static YisVal __fnwrap_stdr_index_of(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_index_of(__a0, __a1);
}

static YisVal __fnwrap_stdr_contains(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_contains(__a0, __a1);
}

static YisVal __fnwrap_stdr_last_index_of(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_last_index_of(__a0, __a1);
}

static YisVal __fnwrap_stdr_shell_quote(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_shell_quote(__a0);
}

static YisVal __fnwrap_stdr_basename(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_basename(__a0);
}

static YisVal __fnwrap_stdr_dirname(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_dirname(__a0);
}

static YisVal __fnwrap_stdr_stem(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_stem(__a0);
}

static YisVal __fnwrap_stdr_join_path(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_join_path(__a0, __a1);
}

static YisVal __fnwrap_stdr_args(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_stdr_args();
}

static YisVal __fnwrap_stdr_open_file_dialog(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_open_file_dialog(__a0, __a1);
}

static YisVal __fnwrap_stdr_open_folder_dialog(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_open_folder_dialog(__a0);
}

static YisVal __fnwrap_stdr_save_file_dialog(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  return yis_stdr_save_file_dialog(__a0, __a1, __a2);
}

/* end embedded module: stdr */

/* begin embedded module: vimana */
static YisVal yis_vimana_system(void);
static YisVal yis_vimana_screen(YisVal, YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_system_run(YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_system_quit(YisVal);
static YisVal yis_m_vimana_system_ticks(YisVal);
static YisVal yis_m_vimana_system_sleep(YisVal, YisVal);
static YisVal yis_m_vimana_system_clipboard_text(YisVal);
static YisVal yis_m_vimana_system_set_clipboard_text(YisVal, YisVal);
static YisVal yis_m_vimana_system_home_dir(YisVal);
static YisVal yis_m_vimana_system_spawn(YisVal, YisVal);
static YisVal yis_m_vimana_system_proc_write(YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_system_proc_read_line(YisVal, YisVal);
static YisVal yis_m_vimana_system_proc_running(YisVal, YisVal);
static YisVal yis_m_vimana_system_proc_kill(YisVal, YisVal);
static YisVal yis_m_vimana_system_proc_free(YisVal, YisVal);
static YisVal yis_m_vimana_system_device(YisVal);
static YisVal yis_m_vimana_system_datetime(YisVal);
static YisVal yis_m_vimana_system_file(YisVal);
static YisVal yis_m_vimana_screen_clear(YisVal, YisVal);
static YisVal yis_m_vimana_screen_resize(YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_screen_set_palette(YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_screen_set_font_glyph(YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_screen_set_font_chr(YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_screen_set_font_width(YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_screen_set_font_size(YisVal, YisVal);
static YisVal yis_m_vimana_screen_set_sprite(YisVal, YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_screen_set_x(YisVal, YisVal);
static YisVal yis_m_vimana_screen_set_y(YisVal, YisVal);
static YisVal yis_m_vimana_screen_set_addr(YisVal, YisVal);
static YisVal yis_m_vimana_screen_set_auto(YisVal, YisVal);
static YisVal yis_m_vimana_screen_put(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_screen_put_icn(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_screen_put_text(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_screen_sprite(YisVal, YisVal);
static YisVal yis_m_vimana_screen_pixel(YisVal, YisVal);
static YisVal yis_m_vimana_screen_present(YisVal);
static YisVal yis_m_vimana_screen_draw_titlebar(YisVal, YisVal);
static YisVal yis_m_vimana_screen_set_titlebar_title(YisVal, YisVal);
static YisVal yis_m_vimana_screen_set_titlebar_button(YisVal, YisVal);
static YisVal yis_m_vimana_screen_titlebar_button_pressed(YisVal);
static YisVal yis_m_vimana_screen_x(YisVal);
static YisVal yis_m_vimana_screen_y(YisVal);
static YisVal yis_m_vimana_screen_addr(YisVal);
static YisVal yis_m_vimana_screen_auto(YisVal);
static YisVal yis_m_vimana_screen_width(YisVal);
static YisVal yis_m_vimana_screen_height(YisVal);
static YisVal yis_m_vimana_screen_scale(YisVal);
static YisVal yis_m_vimana_device_poll(YisVal);
static YisVal yis_m_vimana_device_key_down(YisVal, YisVal);
static YisVal yis_m_vimana_device_key_pressed(YisVal, YisVal);
static YisVal yis_m_vimana_device_mouse_down(YisVal, YisVal);
static YisVal yis_m_vimana_device_mouse_pressed(YisVal, YisVal);
static YisVal yis_m_vimana_device_pointer_x(YisVal);
static YisVal yis_m_vimana_device_pointer_y(YisVal);
static YisVal yis_m_vimana_device_tile_x(YisVal);
static YisVal yis_m_vimana_device_tile_y(YisVal);
static YisVal yis_m_vimana_device_wheel_x(YisVal);
static YisVal yis_m_vimana_device_wheel_y(YisVal);
static YisVal yis_m_vimana_device_text_input(YisVal);
static YisVal yis_m_vimana_datetime_now(YisVal);
static YisVal yis_m_vimana_datetime_year(YisVal);
static YisVal yis_m_vimana_datetime_month(YisVal);
static YisVal yis_m_vimana_datetime_day(YisVal);
static YisVal yis_m_vimana_datetime_hour(YisVal);
static YisVal yis_m_vimana_datetime_minute(YisVal);
static YisVal yis_m_vimana_datetime_second(YisVal);
static YisVal yis_m_vimana_datetime_weekday(YisVal);
static YisVal yis_m_vimana_datetime_year_at(YisVal, YisVal);
static YisVal yis_m_vimana_datetime_month_at(YisVal, YisVal);
static YisVal yis_m_vimana_datetime_day_at(YisVal, YisVal);
static YisVal yis_m_vimana_datetime_hour_at(YisVal, YisVal);
static YisVal yis_m_vimana_datetime_minute_at(YisVal, YisVal);
static YisVal yis_m_vimana_datetime_second_at(YisVal, YisVal);
static YisVal yis_m_vimana_datetime_weekday_at(YisVal, YisVal);
static YisVal yis_m_vimana_file_read_text(YisVal, YisVal);
static YisVal yis_m_vimana_file_read_bytes(YisVal, YisVal);
static YisVal yis_m_vimana_file_write_text(YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_file_write_bytes(YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_file_exists(YisVal, YisVal);
static YisVal yis_m_vimana_file_remove(YisVal, YisVal);
static YisVal yis_m_vimana_file_rename(YisVal, YisVal, YisVal);
static YisVal yis_m_vimana_file_list(YisVal, YisVal);
static YisVal yis_m_vimana_file_is_dir(YisVal, YisVal);
static YisVal __fnwrap_vimana_system(void*,int,YisVal*);
static YisVal __fnwrap_vimana_screen(void*,int,YisVal*);
static YisVal v_key_escape = YV_NULLV;
static YisVal v_mouse_left = YV_NULLV;
static YisVal v_mouse_right = YV_NULLV;
static YisVal v_color_bg = YV_NULLV;
static YisVal v_color_fg = YV_NULLV;
static YisVal v_color_2 = YV_NULLV;
static YisVal v_color_3 = YV_NULLV;
static YisVal v_sprite_1bpp = YV_NULLV;
static YisVal v_sprite_2bpp = YV_NULLV;
static YisVal v_layer_bg = YV_NULLV;
static YisVal v_layer_fg = YV_NULLV;
static YisVal v_flip_x = YV_NULLV;
static YisVal v_flip_y = YV_NULLV;

// cask vimana
static YisVal yis_m_vimana_system_run(YisVal v_this, YisVal v_scr, YisVal v_frame) {
#line 109 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_system_run(v_this, v_scr, v_frame));
#line 110 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_system_quit(YisVal v_this) {
#line 114 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_system_quit(v_this));
#line 115 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_system_ticks(YisVal v_this) {
#line 118 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_ticks(v_this);
}

static YisVal yis_m_vimana_system_sleep(YisVal v_this, YisVal v_ms) {
#line 121 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_system_sleep(v_this, v_ms));
#line 122 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_system_clipboard_text(YisVal v_this) {
#line 125 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_clipboard_text(v_this);
}

static YisVal yis_m_vimana_system_set_clipboard_text(YisVal v_this, YisVal v_text) {
#line 126 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_set_clipboard_text(v_this, v_text);
}

static YisVal yis_m_vimana_system_home_dir(YisVal v_this) {
#line 127 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_home_dir(v_this);
}

static YisVal yis_m_vimana_system_spawn(YisVal v_this, YisVal v_cmd) {
#line 129 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_spawn(v_this, v_cmd);
}

static YisVal yis_m_vimana_system_proc_write(YisVal v_this, YisVal v_proc, YisVal v_text) {
#line 130 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_proc_write(v_this, v_proc, v_text);
}

static YisVal yis_m_vimana_system_proc_read_line(YisVal v_this, YisVal v_proc) {
#line 131 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_proc_read_line(v_this, v_proc);
}

static YisVal yis_m_vimana_system_proc_running(YisVal v_this, YisVal v_proc) {
#line 132 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_proc_running(v_this, v_proc);
}

static YisVal yis_m_vimana_system_proc_kill(YisVal v_this, YisVal v_proc) {
#line 134 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_system_proc_kill(v_this, v_proc));
#line 135 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_system_proc_free(YisVal v_this, YisVal v_proc) {
#line 138 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_system_proc_free(v_this, v_proc));
#line 139 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_system_device(YisVal v_this) {
#line 142 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_device(v_this);
}

static YisVal yis_m_vimana_system_datetime(YisVal v_this) {
#line 143 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_datetime(v_this);
}

static YisVal yis_m_vimana_system_file(YisVal v_this) {
#line 144 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_file(v_this);
}

static YisVal yis_m_vimana_screen_clear(YisVal v_this, YisVal v_bg) {
#line 149 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_clear(v_this, v_bg));
#line 150 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_resize(YisVal v_this, YisVal v_width, YisVal v_height) {
#line 154 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_resize(v_this, v_width, v_height));
#line 155 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_palette(YisVal v_this, YisVal v_slot, YisVal v_color) {
#line 159 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_palette(v_this, v_slot, v_color));
#line 160 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_font_glyph(YisVal v_this, YisVal v_code, YisVal v_icn) {
#line 164 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_font_glyph(v_this, v_code, v_icn));
#line 165 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_font_chr(YisVal v_this, YisVal v_code, YisVal v_chr) {
#line 169 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_font_chr(v_this, v_code, v_chr));
#line 170 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_font_width(YisVal v_this, YisVal v_code, YisVal v_width) {
#line 174 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_font_width(v_this, v_code, v_width));
#line 175 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_font_size(YisVal v_this, YisVal v_size) {
#line 179 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_font_size(v_this, v_size));
#line 180 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_sprite(YisVal v_this, YisVal v_addr, YisVal v_sprite, YisVal v_mode) {
#line 184 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_sprite(v_this, v_addr, v_sprite, v_mode));
#line 185 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_x(YisVal v_this, YisVal v_x) {
#line 189 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_x(v_this, v_x));
#line 190 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_y(YisVal v_this, YisVal v_y) {
#line 194 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_y(v_this, v_y));
#line 195 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_addr(YisVal v_this, YisVal v_addr) {
#line 199 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_addr(v_this, v_addr));
#line 200 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_auto(YisVal v_this, YisVal v_auto) {
#line 204 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_auto(v_this, v_auto));
#line 205 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_put(YisVal v_this, YisVal v_x, YisVal v_y, YisVal v_glyph, YisVal v_fg, YisVal v_bg) {
#line 209 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_put(v_this, v_x, v_y, v_glyph, v_fg, v_bg));
#line 210 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_put_icn(YisVal v_this, YisVal v_x, YisVal v_y, YisVal v_icn, YisVal v_fg, YisVal v_bg) {
#line 214 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_put_icn(v_this, v_x, v_y, v_icn, v_fg, v_bg));
#line 215 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_put_text(YisVal v_this, YisVal v_x, YisVal v_y, YisVal v_text, YisVal v_fg, YisVal v_bg) {
#line 219 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_put_text(v_this, v_x, v_y, v_text, v_fg, v_bg));
#line 220 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_sprite(YisVal v_this, YisVal v_ctrl) {
#line 224 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_sprite(v_this, v_ctrl));
#line 225 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_pixel(YisVal v_this, YisVal v_ctrl) {
#line 229 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_pixel(v_this, v_ctrl));
#line 230 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_present(YisVal v_this) {
#line 234 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_present(v_this));
#line 235 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_draw_titlebar(YisVal v_this, YisVal v_bg) {
#line 239 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_draw_titlebar(v_this, v_bg));
#line 240 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_titlebar_title(YisVal v_this, YisVal v_title) {
#line 244 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_titlebar_title(v_this, v_title));
#line 245 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_titlebar_button(YisVal v_this, YisVal v_show) {
#line 249 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_titlebar_button(v_this, v_show));
#line 250 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_titlebar_button_pressed(YisVal v_this) {
#line 254 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_titlebar_button_pressed(v_this);
}

static YisVal yis_m_vimana_screen_x(YisVal v_this) {
#line 257 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_x(v_this);
}

static YisVal yis_m_vimana_screen_y(YisVal v_this) {
#line 258 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_y(v_this);
}

static YisVal yis_m_vimana_screen_addr(YisVal v_this) {
#line 259 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_addr(v_this);
}

static YisVal yis_m_vimana_screen_auto(YisVal v_this) {
#line 260 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_auto(v_this);
}

static YisVal yis_m_vimana_screen_width(YisVal v_this) {
#line 262 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_width(v_this);
}

static YisVal yis_m_vimana_screen_height(YisVal v_this) {
#line 263 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_height(v_this);
}

static YisVal yis_m_vimana_screen_scale(YisVal v_this) {
#line 264 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_scale(v_this);
}

static YisVal yis_m_vimana_device_poll(YisVal v_this) {
#line 269 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_device_poll(v_this));
#line 270 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_device_key_down(YisVal v_this, YisVal v_scancode) {
#line 273 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_key_down(v_this, v_scancode);
}

static YisVal yis_m_vimana_device_key_pressed(YisVal v_this, YisVal v_scancode) {
#line 274 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_key_pressed(v_this, v_scancode);
}

static YisVal yis_m_vimana_device_mouse_down(YisVal v_this, YisVal v_button) {
#line 275 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_mouse_down(v_this, v_button);
}

static YisVal yis_m_vimana_device_mouse_pressed(YisVal v_this, YisVal v_button) {
#line 276 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_mouse_pressed(v_this, v_button);
}

static YisVal yis_m_vimana_device_pointer_x(YisVal v_this) {
#line 277 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_pointer_x(v_this);
}

static YisVal yis_m_vimana_device_pointer_y(YisVal v_this) {
#line 278 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_pointer_y(v_this);
}

static YisVal yis_m_vimana_device_tile_x(YisVal v_this) {
#line 279 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_tile_x(v_this);
}

static YisVal yis_m_vimana_device_tile_y(YisVal v_this) {
#line 280 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_tile_y(v_this);
}

static YisVal yis_m_vimana_device_wheel_x(YisVal v_this) {
#line 281 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_wheel_x(v_this);
}

static YisVal yis_m_vimana_device_wheel_y(YisVal v_this) {
#line 282 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_wheel_y(v_this);
}

static YisVal yis_m_vimana_device_text_input(YisVal v_this) {
#line 283 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_text_input(v_this);
}

static YisVal yis_m_vimana_datetime_now(YisVal v_this) {
#line 287 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_now(v_this);
}

static YisVal yis_m_vimana_datetime_year(YisVal v_this) {
#line 288 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_year(v_this);
}

static YisVal yis_m_vimana_datetime_month(YisVal v_this) {
#line 289 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_month(v_this);
}

static YisVal yis_m_vimana_datetime_day(YisVal v_this) {
#line 290 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_day(v_this);
}

static YisVal yis_m_vimana_datetime_hour(YisVal v_this) {
#line 291 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_hour(v_this);
}

static YisVal yis_m_vimana_datetime_minute(YisVal v_this) {
#line 292 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_minute(v_this);
}

static YisVal yis_m_vimana_datetime_second(YisVal v_this) {
#line 293 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_second(v_this);
}

static YisVal yis_m_vimana_datetime_weekday(YisVal v_this) {
#line 294 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_weekday(v_this);
}

static YisVal yis_m_vimana_datetime_year_at(YisVal v_this, YisVal v_timestamp) {
#line 295 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_year_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_datetime_month_at(YisVal v_this, YisVal v_timestamp) {
#line 296 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_month_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_datetime_day_at(YisVal v_this, YisVal v_timestamp) {
#line 297 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_day_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_datetime_hour_at(YisVal v_this, YisVal v_timestamp) {
#line 298 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_hour_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_datetime_minute_at(YisVal v_this, YisVal v_timestamp) {
#line 299 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_minute_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_datetime_second_at(YisVal v_this, YisVal v_timestamp) {
#line 300 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_second_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_datetime_weekday_at(YisVal v_this, YisVal v_timestamp) {
#line 301 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_weekday_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_file_read_text(YisVal v_this, YisVal v_path) {
#line 305 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_read_text(v_this, v_path);
}

static YisVal yis_m_vimana_file_read_bytes(YisVal v_this, YisVal v_path) {
#line 306 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_read_bytes(v_this, v_path);
}

static YisVal yis_m_vimana_file_write_text(YisVal v_this, YisVal v_path, YisVal v_text) {
#line 307 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_write_text(v_this, v_path, v_text);
}

static YisVal yis_m_vimana_file_write_bytes(YisVal v_this, YisVal v_path, YisVal v_bytes) {
#line 308 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_write_bytes(v_this, v_path, v_bytes);
}

static YisVal yis_m_vimana_file_exists(YisVal v_this, YisVal v_path) {
#line 309 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_exists(v_this, v_path);
}

static YisVal yis_m_vimana_file_remove(YisVal v_this, YisVal v_path) {
#line 310 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_remove(v_this, v_path);
}

static YisVal yis_m_vimana_file_rename(YisVal v_this, YisVal v_path, YisVal v_new_path) {
#line 311 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_rename(v_this, v_path, v_new_path);
}

static YisVal yis_m_vimana_file_list(YisVal v_this, YisVal v_path) {
#line 312 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_list(v_this, v_path);
}

static YisVal yis_m_vimana_file_is_dir(YisVal v_this, YisVal v_path) {
#line 313 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_is_dir(v_this, v_path);
}

static YisVal yis_vimana_system(void) {
#line 316 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system();
}

static YisVal yis_vimana_screen(YisVal v_title, YisVal v_width, YisVal v_height, YisVal v_scale) {
#line 317 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen(v_title, v_width, v_height, v_scale);
}

static YisVal __fnwrap_vimana_system(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_vimana_system();
}

static YisVal __fnwrap_vimana_screen(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  return yis_vimana_screen(__a0, __a1, __a2, __a3);
}

static void __yis_vimana_init(void) {
  yis_move_into(&v_key_escape, YV_INT(41));
  yis_move_into(&v_mouse_left, YV_INT(1));
  yis_move_into(&v_mouse_right, YV_INT(3));
  yis_move_into(&v_color_bg, YV_INT(0));
  yis_move_into(&v_color_fg, YV_INT(1));
  yis_move_into(&v_color_2, YV_INT(2));
  yis_move_into(&v_color_3, YV_INT(3));
  yis_move_into(&v_sprite_1bpp, YV_INT(0));
  yis_move_into(&v_sprite_2bpp, YV_INT(1));
  yis_move_into(&v_layer_bg, YV_INT(0));
  yis_move_into(&v_layer_fg, YV_INT(0));
  yis_move_into(&v_flip_x, YV_INT(0));
  yis_move_into(&v_flip_y, YV_INT(0));
}

/* end embedded module: vimana */

/* begin embedded module: outline_parser */
static YisVal yis_outline_parser_first_word(YisVal);
static YisVal yis_outline_parser_line_preview(YisVal);
static void yis_outline_parser_add_item(YisVal, YisVal, YisVal, YisVal);
static YisVal yis_outline_parser_parse_outline(YisVal);
static YisVal __fnwrap_outline_parser_first_word(void*,int,YisVal*);
static YisVal __fnwrap_outline_parser_line_preview(void*,int,YisVal*);
static YisVal __fnwrap_outline_parser_add_item(void*,int,YisVal*);
static YisVal __fnwrap_outline_parser_parse_outline(void*,int,YisVal*);

// cask outline_parser
// bring stdr
static YisVal yis_outline_parser_first_word(YisVal v_text) {
#line 6 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  YisVal v_t = yis_stdr_trim(v_text); yis_retain_val(v_t);
#line 7 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  YisVal v_n = YV_INT(stdr_len(v_t)); yis_retain_val(v_n);
#line 8 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  YisVal v_cut = v_n; yis_retain_val(v_cut);
#line 10 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_n)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 11 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    YisVal v_ch = stdr_slice(v_t, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_ch);
#line 12 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(" ")))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\t")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("(")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(":"))))))) {
#line 13 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)((yis_move_into(&v_cut, v_i), v_cut));
#line 14 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      break;
    }
  } }
#line 16 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  return stdr_slice(v_t, yis_as_int(YV_INT(0)), yis_as_int(v_cut));
}

static YisVal yis_outline_parser_line_preview(YisVal v_text) {
#line 20 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  YisVal v_t = yis_stdr_trim(v_text); yis_retain_val(v_t);
#line 21 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  YisVal v_n = YV_INT(stdr_len(v_t)); yis_retain_val(v_n);
#line 22 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  if (yis_as_bool(yis_le(v_n, YV_INT(48)))) {
#line 23 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    return v_t;
  }
#line 24 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  return yis_add(stdr_slice(v_t, yis_as_int(YV_INT(0)), yis_as_int(YV_INT(48))), YV_STR(stdr_str_lit("...")));
}

static void yis_outline_parser_add_item(YisVal v_items, YisVal v_label, YisVal v_line, YisVal v_kind) {
#line 28 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  YisVal v_item = yis_dict_lit(0); yis_retain_val(v_item);
#line 29 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  (void)(yis_index_set(v_item, YV_STR(stdr_str_lit("label")), v_label));
#line 30 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  (void)(yis_index_set(v_item, YV_STR(stdr_str_lit("line")), v_line));
#line 31 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  (void)(yis_index_set(v_item, YV_STR(stdr_str_lit("type")), v_kind));
#line 32 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  (void)(stdr_push(v_items, v_item));
}

static YisVal yis_outline_parser_parse_outline(YisVal v_code) {
#line 36 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  YisVal v_items = yis_arr_lit(0); yis_retain_val(v_items);
#line 37 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 38 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  YisVal v_start = YV_INT(0); yis_retain_val(v_start);
#line 39 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  YisVal v_ln = YV_INT(0); yis_retain_val(v_ln);
#line 41 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_le(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 42 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    YisVal v_at_end = yis_ge(v_i, v_total); yis_retain_val(v_at_end);
#line 43 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    YisVal v_is_nl = YV_BOOL(yis_as_bool(YV_BOOL(!yis_as_bool(v_at_end))) && yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))); yis_retain_val(v_is_nl);
#line 44 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(!yis_as_bool(v_at_end))) && yis_as_bool(YV_BOOL(!yis_as_bool(v_is_nl)))))) {
#line 45 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      continue;
    }
#line 47 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    YisVal v_line = stdr_slice(v_code, yis_as_int(v_start), yis_as_int(v_i)); yis_retain_val(v_line);
#line 48 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    YisVal v_trimmed = yis_stdr_trim(v_line); yis_retain_val(v_trimmed);
#line 49 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    YisVal v_n = YV_INT(stdr_len(v_trimmed)); yis_retain_val(v_n);
#line 51 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    if (yis_as_bool(yis_eq(v_n, YV_INT(0)))) {
#line 52 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)((yis_move_into(&v_start, yis_add(v_i, YV_INT(1))), v_start));
#line 53 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)((yis_move_into(&v_ln, yis_add(v_ln, YV_INT(1))), v_ln));
#line 54 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      continue;
    }
#line 56 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("cask "))))) {
#line 57 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(5)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("cask"))));
    } else if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("bring "))))) {
#line 59 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(6)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("bring"))));
    } else if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("pub ,: "))))) {
#line 61 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(6)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("struct"))));
    } else if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit(",: "))))) {
#line 63 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(3)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("struct"))));
    } else if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("!: "))))) {
#line 65 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(3)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("pub_fn"))));
    } else if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit(":: "))))) {
#line 67 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(3)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("const"))));
    } else if (yis_as_bool(YV_BOOL(yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit(": ")))) && yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit(":: "))))))))) {
#line 69 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(2)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("fn"))));
    } else if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("-> "))))) {
#line 71 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, YV_STR(stdr_str_lit("main")), v_ln, YV_STR(stdr_str_lit("fn"))));
    } else if (yis_as_bool(YV_BOOL(yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("def ")))) && yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("def ?"))))))))) {
#line 73 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(4)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("const"))));
    }








#line 75 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    (void)((yis_move_into(&v_start, yis_add(v_i, YV_INT(1))), v_start));
#line 76 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    (void)((yis_move_into(&v_ln, yis_add(v_ln, YV_INT(1))), v_ln));
  } }
#line 78 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
  return v_items;
}

static YisVal __fnwrap_outline_parser_first_word(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_outline_parser_first_word(__a0);
}

static YisVal __fnwrap_outline_parser_line_preview(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_outline_parser_line_preview(__a0);
}

static YisVal __fnwrap_outline_parser_add_item(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  yis_outline_parser_add_item(__a0, __a1, __a2, __a3);
  return YV_NULLV;
}

static YisVal __fnwrap_outline_parser_parse_outline(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_outline_parser_parse_outline(__a0);
}

/* end embedded module: outline_parser */

/* begin embedded module: right_model */
static YisVal yis_right_model_line_count(YisVal);
static YisVal yis_right_model_line_col_from_index(YisVal, YisVal);
static YisVal yis_right_model_index_from_line_col(YisVal, YisVal, YisVal);
static YisVal yis_right_model_sel_normalized(void);
static YisVal yis_right_model_has_selection(void);
static YisVal yis_right_model_visible_lines(void);
static void yis_right_model_toggle_sidebar(void);
static YisVal yis_right_model_editor_left(void);
static YisVal yis_right_model_editor_width(void);
static YisVal yis_right_model_editor_cols(void);
static YisVal yis_right_model_line_to_char_pos(YisVal, YisVal);
static void yis_right_model_set_text(YisVal);
static void yis_right_model_set_caret(YisVal);
static void yis_right_model_set_selection(YisVal, YisVal);
static void yis_right_model_set_scroll(YisVal);
static void yis_right_model_set_file_path(YisVal);
static void yis_right_model_set_dirty(YisVal);
static void yis_right_model_set_outline_selected(YisVal);
static void yis_right_model_ensure_visible(void);
static void yis_right_model_clamp_scroll(void);
static void yis_right_model_rebuild_outline(void);
static void yis_right_model_insert_at_caret(YisVal);
static void yis_right_model_delete_selection(void);
static void yis_right_model_delete_backward(void);
static void yis_right_model_delete_forward(void);
static void yis_right_model_move_caret_left(YisVal);
static void yis_right_model_move_caret_right(YisVal);
static void yis_right_model_move_caret_up(YisVal);
static void yis_right_model_move_caret_down(YisVal);
static void yis_right_model_select_all(void);
static YisVal yis_right_model_selected_text(void);
static void yis_right_model_scroll_by(YisVal);
static YisVal yis_right_model_caret_from_editor_click(YisVal, YisVal);
static YisVal yis_right_model_outline_index_from_click(YisVal);
static void yis_right_model_move_caret_line_start(YisVal);
static void yis_right_model_move_caret_line_end(YisVal);
static void yis_right_model_move_caret_doc_start(YisVal);
static void yis_right_model_move_caret_doc_end(YisVal);
static YisVal yis_right_model_is_word_char(YisVal);
static void yis_right_model_select_word_at(YisVal);
static void yis_right_model_open_search(void);
static void yis_right_model_close_search(void);
static void yis_right_model_run_search(void);
static void yis_right_model_jump_to_match(void);
static void yis_right_model_search_next(void);
static void yis_right_model_search_prev(void);
static void yis_right_model_search_type(YisVal);
static void yis_right_model_search_backspace(void);
static void yis_right_model_reset_selection(void);
static YisVal __fnwrap_right_model_line_count(void*,int,YisVal*);
static YisVal __fnwrap_right_model_line_col_from_index(void*,int,YisVal*);
static YisVal __fnwrap_right_model_index_from_line_col(void*,int,YisVal*);
static YisVal __fnwrap_right_model_sel_normalized(void*,int,YisVal*);
static YisVal __fnwrap_right_model_has_selection(void*,int,YisVal*);
static YisVal __fnwrap_right_model_visible_lines(void*,int,YisVal*);
static YisVal __fnwrap_right_model_toggle_sidebar(void*,int,YisVal*);
static YisVal __fnwrap_right_model_editor_left(void*,int,YisVal*);
static YisVal __fnwrap_right_model_editor_width(void*,int,YisVal*);
static YisVal __fnwrap_right_model_editor_cols(void*,int,YisVal*);
static YisVal __fnwrap_right_model_line_to_char_pos(void*,int,YisVal*);
static YisVal __fnwrap_right_model_set_text(void*,int,YisVal*);
static YisVal __fnwrap_right_model_set_caret(void*,int,YisVal*);
static YisVal __fnwrap_right_model_set_selection(void*,int,YisVal*);
static YisVal __fnwrap_right_model_set_scroll(void*,int,YisVal*);
static YisVal __fnwrap_right_model_set_file_path(void*,int,YisVal*);
static YisVal __fnwrap_right_model_set_dirty(void*,int,YisVal*);
static YisVal __fnwrap_right_model_set_outline_selected(void*,int,YisVal*);
static YisVal __fnwrap_right_model_ensure_visible(void*,int,YisVal*);
static YisVal __fnwrap_right_model_clamp_scroll(void*,int,YisVal*);
static YisVal __fnwrap_right_model_rebuild_outline(void*,int,YisVal*);
static YisVal __fnwrap_right_model_insert_at_caret(void*,int,YisVal*);
static YisVal __fnwrap_right_model_delete_selection(void*,int,YisVal*);
static YisVal __fnwrap_right_model_delete_backward(void*,int,YisVal*);
static YisVal __fnwrap_right_model_delete_forward(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_left(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_right(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_up(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_down(void*,int,YisVal*);
static YisVal __fnwrap_right_model_select_all(void*,int,YisVal*);
static YisVal __fnwrap_right_model_selected_text(void*,int,YisVal*);
static YisVal __fnwrap_right_model_scroll_by(void*,int,YisVal*);
static YisVal __fnwrap_right_model_caret_from_editor_click(void*,int,YisVal*);
static YisVal __fnwrap_right_model_outline_index_from_click(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_line_start(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_line_end(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_doc_start(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_doc_end(void*,int,YisVal*);
static YisVal __fnwrap_right_model_is_word_char(void*,int,YisVal*);
static YisVal __fnwrap_right_model_select_word_at(void*,int,YisVal*);
static YisVal __fnwrap_right_model_open_search(void*,int,YisVal*);
static YisVal __fnwrap_right_model_close_search(void*,int,YisVal*);
static YisVal __fnwrap_right_model_run_search(void*,int,YisVal*);
static YisVal __fnwrap_right_model_jump_to_match(void*,int,YisVal*);
static YisVal __fnwrap_right_model_search_next(void*,int,YisVal*);
static YisVal __fnwrap_right_model_search_prev(void*,int,YisVal*);
static YisVal __fnwrap_right_model_search_type(void*,int,YisVal*);
static YisVal __fnwrap_right_model_search_backspace(void*,int,YisVal*);
static YisVal __fnwrap_right_model_reset_selection(void*,int,YisVal*);
static YisVal v_WIN_W = YV_NULLV;
static YisVal v_WIN_H = YV_NULLV;
static YisVal v_FONT_W = YV_NULLV;
static YisVal v_FONT_H = YV_NULLV;
static YisVal v_SCROLLBAR_W = YV_NULLV;
static YisVal v_SCROLLBAR_X = YV_NULLV;
static YisVal v_SIDEBAR_X = YV_NULLV;
static YisVal v_SIDEBAR_W = YV_NULLV;
static YisVal v_EDITOR_X = YV_NULLV;
static YisVal v_EDITOR_W = YV_NULLV;
static YisVal v_PAD_X = YV_NULLV;
static YisVal v_ROW_H = YV_NULLV;
static YisVal v_TITLEBAR_H = YV_NULLV;
static YisVal v_CONTENT_Y = YV_NULLV;
static YisVal v_COL_GUIDE = YV_NULLV;
static YisVal v_MAX_COLS = YV_NULLV;
static YisVal v_SIDEBAR_CHARS = YV_NULLV;
static YisVal v_MAX_LINES = YV_NULLV;
static YisVal v_STATUS_H = YV_NULLV;
static YisVal v_sidebar_visible = YV_NULLV;
static YisVal v_C_BG = YV_NULLV;
static YisVal v_C_FG = YV_NULLV;
static YisVal v_C_SEL = YV_NULLV;
static YisVal v_C_ACCENT = YV_NULLV;
static YisVal v_H_KEYWORD = YV_NULLV;
static YisVal v_H_COMMENT = YV_NULLV;
static YisVal v_H_STRING = YV_NULLV;
static YisVal v_H_TYPE = YV_NULLV;
static YisVal v_KEY_UP = YV_NULLV;
static YisVal v_KEY_DOWN = YV_NULLV;
static YisVal v_KEY_LEFT = YV_NULLV;
static YisVal v_KEY_RIGHT = YV_NULLV;
static YisVal v_KEY_RETURN = YV_NULLV;
static YisVal v_KEY_TAB = YV_NULLV;
static YisVal v_KEY_BACKSPACE = YV_NULLV;
static YisVal v_KEY_DELETE = YV_NULLV;
static YisVal v_KEY_ESCAPE = YV_NULLV;
static YisVal v_KEY_A = YV_NULLV;
static YisVal v_KEY_C = YV_NULLV;
static YisVal v_KEY_S = YV_NULLV;
static YisVal v_KEY_V = YV_NULLV;
static YisVal v_KEY_X = YV_NULLV;
static YisVal v_KEY_D = YV_NULLV;
static YisVal v_KEY_N = YV_NULLV;
static YisVal v_KEY_O = YV_NULLV;
static YisVal v_KEY_F = YV_NULLV;
static YisVal v_KEY_G = YV_NULLV;
static YisVal v_KEY_LGUI = YV_NULLV;
static YisVal v_KEY_RGUI = YV_NULLV;
static YisVal v_editor_text = YV_NULLV;
static YisVal v_caret = YV_NULLV;
static YisVal v_sel_start = YV_NULLV;
static YisVal v_sel_end = YV_NULLV;
static YisVal v_scroll_y = YV_NULLV;
static YisVal v_current_outline = YV_NULLV;
static YisVal v_outline_selected = YV_NULLV;
static YisVal v_outline_scroll = YV_NULLV;
static YisVal v_file_path = YV_NULLV;
static YisVal v_dirty = YV_NULLV;
static YisVal v_search_active = YV_NULLV;
static YisVal v_search_query = YV_NULLV;
static YisVal v_search_matches = YV_NULLV;
static YisVal v_search_index = YV_NULLV;

// cask right_model
// bring stdr
// bring outline_parser
static YisVal yis_right_model_line_count(YisVal v_code) {
#line 94 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 95 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_total, YV_INT(0)))) {
#line 96 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_INT(1);
  }
#line 97 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lines = YV_INT(1); yis_retain_val(v_lines);
#line 98 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 99 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 100 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_lines, yis_add(v_lines, YV_INT(1))), v_lines));
    }
  } }
#line 101 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return v_lines;
}

static YisVal yis_right_model_line_col_from_index(YisVal v_code, YisVal v_pos) {
#line 105 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 106 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_p = v_pos; yis_retain_val(v_p);
#line 107 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_p, YV_INT(0)))) {
#line 108 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_p, YV_INT(0)), v_p));
  }
#line 109 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_p, v_total))) {
#line 110 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_p, v_total), v_p));
  }
#line 111 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line = YV_INT(0); yis_retain_val(v_line);
#line 112 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_col = YV_INT(0); yis_retain_val(v_col);
#line 113 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_p)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 114 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 115 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_line, yis_add(v_line, YV_INT(1))), v_line));
#line 116 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_col, YV_INT(0)), v_col));
    } else {
#line 118 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(1))), v_col));
    }
  } }
#line 119 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_arr_lit(2, v_line, v_col);
}

static YisVal yis_right_model_index_from_line_col(YisVal v_code, YisVal v_target_line, YisVal v_target_col) {
#line 123 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 124 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line = YV_INT(0); yis_retain_val(v_line);
#line 125 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_start = YV_INT(0); yis_retain_val(v_start);
#line 126 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 127 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_ge(v_line, v_target_line))) {
#line 128 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
#line 129 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 130 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_line, yis_add(v_line, YV_INT(1))), v_line));
#line 131 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_start, yis_add(v_i, YV_INT(1))), v_start));
    }
  } }
#line 132 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_line, v_target_line))) {
#line 133 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_start, v_total), v_start));
  }
#line 134 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_end_pos = v_start; yis_retain_val(v_end_pos);
#line 135 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_j = v_start; yis_retain_val(v_j);
  for (; yis_as_bool(yis_lt(v_j, v_total)); (void)((yis_move_into(&v_j, yis_add(v_j, YV_INT(1))), v_j))) {
#line 136 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_j), yis_as_int(yis_add(v_j, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 137 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
#line 138 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_end_pos, yis_add(v_j, YV_INT(1))), v_end_pos));
  } }
#line 139 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line_len = yis_sub(v_end_pos, v_start); yis_retain_val(v_line_len);
#line 140 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_col = v_target_col; yis_retain_val(v_col);
#line 141 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_col, YV_INT(0)))) {
#line 142 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_col, YV_INT(0)), v_col));
  }
#line 143 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_col, v_line_len))) {
#line 144 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_col, v_line_len), v_col));
  }
#line 145 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_add(v_start, v_col);
}

static YisVal yis_right_model_sel_normalized(void) {
#line 149 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_le(v_sel_start, v_sel_end))) {
#line 150 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return yis_arr_lit(2, v_sel_start, v_sel_end);
  }
#line 151 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_arr_lit(2, v_sel_end, v_sel_start);
}

static YisVal yis_right_model_has_selection(void) {
#line 155 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_ne(v_sel_start, v_sel_end);
}

static YisVal yis_right_model_visible_lines(void) {
#line 159 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return v_MAX_LINES;
}

static void yis_right_model_toggle_sidebar(void) {
#line 163 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sidebar_visible, YV_BOOL(!yis_as_bool(v_sidebar_visible))), v_sidebar_visible));
}

static YisVal yis_right_model_editor_left(void) {
#line 167 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_sidebar_visible)) {
#line 168 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return v_EDITOR_X;
  }
#line 169 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return v_SIDEBAR_X;
}

static YisVal yis_right_model_editor_width(void) {
#line 173 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_sidebar_visible)) {
#line 174 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return v_EDITOR_W;
  }
#line 175 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_sub(v_WIN_W, v_SIDEBAR_X);
}

static YisVal yis_right_model_editor_cols(void) {
#line 179 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_sub(yis_stdr_floor(yis_div(yis_right_model_editor_width(), v_FONT_W)), YV_INT(1));
}

static YisVal yis_right_model_line_to_char_pos(YisVal v_code, YisVal v_target_line) {
#line 183 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_le(v_target_line, YV_INT(0)))) {
#line 184 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_INT(0);
  }
#line 185 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 186 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line = YV_INT(0); yis_retain_val(v_line);
#line 187 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lstart = YV_INT(0); yis_retain_val(v_lstart);
#line 188 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 189 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(v_line, v_target_line))) {
#line 190 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      return v_lstart;
    }
#line 191 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 192 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_line, yis_add(v_line, YV_INT(1))), v_line));
#line 193 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_lstart, yis_add(v_i, YV_INT(1))), v_lstart));
    }
  } }
#line 194 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return v_lstart;
}

static void yis_right_model_set_text(YisVal v_txt) {
#line 202 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_editor_text, v_txt), v_editor_text));
}

static void yis_right_model_set_caret(YisVal v_pos) {
#line 206 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 207 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_pos, YV_INT(0)))) {
#line 208 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_pos, YV_INT(0)), v_pos));
  }
#line 209 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_pos, v_total))) {
#line 210 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_pos, v_total), v_pos));
  }
#line 211 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_pos), v_caret));
}

static void yis_right_model_set_selection(YisVal v_a, YisVal v_b) {
#line 215 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_a), v_sel_start));
#line 216 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_b), v_sel_end));
}

static void yis_right_model_set_scroll(YisVal v_y) {
#line 220 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_scroll_y, v_y), v_scroll_y));
}

static void yis_right_model_set_file_path(YisVal v_p) {
#line 224 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_file_path, v_p), v_file_path));
}

static void yis_right_model_set_dirty(YisVal v_d) {
#line 228 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dirty, v_d), v_dirty));
}

static void yis_right_model_set_outline_selected(YisVal v_idx) {
#line 232 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_outline_selected, v_idx), v_outline_selected));
}

static void yis_right_model_ensure_visible(void) {
#line 240 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lc = yis_right_model_line_col_from_index(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 241 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_cur_line);
#line 242 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_cur_line, v_scroll_y))) {
#line 243 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_scroll_y, v_cur_line), v_scroll_y));
  } else if (yis_as_bool(yis_ge(v_cur_line, yis_add(v_scroll_y, v_MAX_LINES)))) {
#line 245 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_scroll_y, yis_add(yis_sub(v_cur_line, v_MAX_LINES), YV_INT(1))), v_scroll_y));
  }

}

static void yis_right_model_clamp_scroll(void) {
#line 249 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = yis_right_model_line_count(v_editor_text); yis_retain_val(v_total);
#line 250 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_max_scroll = yis_sub(v_total, v_MAX_LINES); yis_retain_val(v_max_scroll);
#line 251 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_max_scroll, YV_INT(0)))) {
#line 252 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_max_scroll, YV_INT(0)), v_max_scroll));
  }
#line 253 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_scroll_y, YV_INT(0)))) {
#line 254 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_scroll_y, YV_INT(0)), v_scroll_y));
  }
#line 255 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_scroll_y, v_max_scroll))) {
#line 256 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_scroll_y, v_max_scroll), v_scroll_y));
  }
}

static void yis_right_model_rebuild_outline(void) {
#line 260 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_current_outline, yis_outline_parser_parse_outline(v_editor_text)), v_current_outline));
#line 261 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_n = YV_INT(stdr_len(v_current_outline)); yis_retain_val(v_n);
#line 262 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_le(v_n, YV_INT(0)))) {
#line 263 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_outline_selected, yis_neg(YV_INT(1))), v_outline_selected));
  } else if (yis_as_bool(yis_ge(v_outline_selected, v_n))) {
#line 265 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_outline_selected, yis_sub(v_n, YV_INT(1))), v_outline_selected));
  }

}

static void yis_right_model_insert_at_caret(YisVal v_chunk) {
#line 273 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_pair = yis_right_model_sel_normalized(); yis_retain_val(v_pair);
#line 274 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_a = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(0))))))); yis_retain_val(v_a);
#line 275 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_b = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(1))))))); yis_retain_val(v_b);
#line 276 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_prefix = stdr_slice(v_editor_text, yis_as_int(YV_INT(0)), yis_as_int(v_a)); yis_retain_val(v_prefix);
#line 277 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_suffix = stdr_slice(v_editor_text, yis_as_int(v_b), yis_as_int(YV_INT(stdr_len(v_editor_text)))); yis_retain_val(v_suffix);
#line 278 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_editor_text, ({ YisVal __ip[3]; __ip[0] = YV_STR(stdr_to_string(v_prefix)); __ip[1] = YV_STR(stdr_to_string(v_chunk)); __ip[2] = YV_STR(stdr_to_string(v_suffix)); YV_STR(stdr_str_from_parts(3, __ip)); })), v_editor_text));
#line 279 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_pos = yis_add(v_a, YV_INT(stdr_len(v_chunk))); yis_retain_val(v_pos);
#line 280 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_pos), v_caret));
#line 281 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_pos), v_sel_start));
#line 282 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_pos), v_sel_end));
#line 283 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dirty, YV_BOOL(true)), v_dirty));
#line 284 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_rebuild_outline());
#line 285 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_delete_selection(void) {
#line 289 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_has_selection())))) {
#line 290 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 291 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_pair = yis_right_model_sel_normalized(); yis_retain_val(v_pair);
#line 292 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_a = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(0))))))); yis_retain_val(v_a);
#line 293 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_b = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(1))))))); yis_retain_val(v_b);
#line 294 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_prefix = stdr_slice(v_editor_text, yis_as_int(YV_INT(0)), yis_as_int(v_a)); yis_retain_val(v_prefix);
#line 295 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_suffix = stdr_slice(v_editor_text, yis_as_int(v_b), yis_as_int(YV_INT(stdr_len(v_editor_text)))); yis_retain_val(v_suffix);
#line 296 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_editor_text, ({ YisVal __ip[2]; __ip[0] = YV_STR(stdr_to_string(v_prefix)); __ip[1] = YV_STR(stdr_to_string(v_suffix)); YV_STR(stdr_str_from_parts(2, __ip)); })), v_editor_text));
#line 297 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_a), v_caret));
#line 298 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_a), v_sel_start));
#line 299 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_a), v_sel_end));
#line 300 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dirty, YV_BOOL(true)), v_dirty));
#line 301 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_rebuild_outline());
#line 302 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_delete_backward(void) {
#line 306 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_right_model_has_selection())) {
#line 307 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)(yis_right_model_delete_selection());
#line 308 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 309 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_le(v_caret, YV_INT(0)))) {
#line 310 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 311 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_prefix = stdr_slice(v_editor_text, yis_as_int(YV_INT(0)), yis_as_int(yis_sub(v_caret, YV_INT(1)))); yis_retain_val(v_prefix);
#line 312 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_suffix = stdr_slice(v_editor_text, yis_as_int(v_caret), yis_as_int(YV_INT(stdr_len(v_editor_text)))); yis_retain_val(v_suffix);
#line 313 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_editor_text, ({ YisVal __ip[2]; __ip[0] = YV_STR(stdr_to_string(v_prefix)); __ip[1] = YV_STR(stdr_to_string(v_suffix)); YV_STR(stdr_str_from_parts(2, __ip)); })), v_editor_text));
#line 314 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, yis_sub(v_caret, YV_INT(1))), v_caret));
#line 315 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 316 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
#line 317 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dirty, YV_BOOL(true)), v_dirty));
#line 318 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_rebuild_outline());
#line 319 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_delete_forward(void) {
#line 323 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_right_model_has_selection())) {
#line 324 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)(yis_right_model_delete_selection());
#line 325 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 326 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 327 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_ge(v_caret, v_total))) {
#line 328 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 329 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_prefix = stdr_slice(v_editor_text, yis_as_int(YV_INT(0)), yis_as_int(v_caret)); yis_retain_val(v_prefix);
#line 330 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_suffix = stdr_slice(v_editor_text, yis_as_int(yis_add(v_caret, YV_INT(1))), yis_as_int(v_total)); yis_retain_val(v_suffix);
#line 331 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_editor_text, ({ YisVal __ip[2]; __ip[0] = YV_STR(stdr_to_string(v_prefix)); __ip[1] = YV_STR(stdr_to_string(v_suffix)); YV_STR(stdr_str_from_parts(2, __ip)); })), v_editor_text));
#line 332 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 333 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
#line 334 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dirty, YV_BOOL(true)), v_dirty));
#line 335 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_rebuild_outline());
#line 336 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_left(YisVal v_shift) {
#line 344 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_caret, YV_INT(0)))) {
#line 345 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, yis_sub(v_caret, YV_INT(1))), v_caret));
  }
#line 346 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 347 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 349 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 350 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 351 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_right(YisVal v_shift) {
#line 355 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 356 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_caret, v_total))) {
#line 357 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, yis_add(v_caret, YV_INT(1))), v_caret));
  }
#line 358 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 359 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 361 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 362 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 363 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_up(YisVal v_shift) {
#line 367 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lc = yis_right_model_line_col_from_index(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 368 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_cur_line);
#line 369 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_col = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(1))))))); yis_retain_val(v_cur_col);
#line 370 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_cur_line, YV_INT(0)))) {
#line 371 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, yis_right_model_index_from_line_col(v_editor_text, yis_sub(v_cur_line, YV_INT(1)), v_cur_col)), v_caret));
  } else {
#line 373 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, YV_INT(0)), v_caret));
  }
#line 374 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 375 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 377 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 378 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 379 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_down(YisVal v_shift) {
#line 383 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lc = yis_right_model_line_col_from_index(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 384 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_cur_line);
#line 385 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_col = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(1))))))); yis_retain_val(v_cur_col);
#line 386 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total_lines = yis_right_model_line_count(v_editor_text); yis_retain_val(v_total_lines);
#line 387 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_cur_line, yis_sub(v_total_lines, YV_INT(1))))) {
#line 388 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, yis_right_model_index_from_line_col(v_editor_text, yis_add(v_cur_line, YV_INT(1)), v_cur_col)), v_caret));
  } else {
#line 390 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, YV_INT(stdr_len(v_editor_text))), v_caret));
  }
#line 391 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 392 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 394 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 395 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 396 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_select_all(void) {
#line 400 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, YV_INT(0)), v_sel_start));
#line 401 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, YV_INT(stdr_len(v_editor_text))), v_sel_end));
#line 402 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_sel_end), v_caret));
}

static YisVal yis_right_model_selected_text(void) {
#line 406 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_has_selection())))) {
#line 407 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_STR(stdr_str_lit(""));
  }
#line 408 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_pair = yis_right_model_sel_normalized(); yis_retain_val(v_pair);
#line 409 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_a = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(0))))))); yis_retain_val(v_a);
#line 410 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_b = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(1))))))); yis_retain_val(v_b);
#line 411 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return stdr_slice(v_editor_text, yis_as_int(v_a), yis_as_int(v_b));
}

static void yis_right_model_scroll_by(YisVal v_delta) {
#line 415 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_scroll_y, yis_sub(v_scroll_y, v_delta)), v_scroll_y));
#line 416 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_clamp_scroll());
}

static YisVal yis_right_model_caret_from_editor_click(YisVal v_px, YisVal v_py) {
#line 424 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_col = yis_stdr_floor(yis_div(yis_sub(yis_sub(v_px, yis_right_model_editor_left()), v_PAD_X), v_FONT_W)); yis_retain_val(v_col);
#line 425 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_col, YV_INT(0)))) {
#line 426 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_col, YV_INT(0)), v_col));
  }
#line 427 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_row = yis_add(yis_stdr_floor(yis_div(yis_sub(v_py, v_CONTENT_Y), v_ROW_H)), v_scroll_y); yis_retain_val(v_row);
#line 428 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_row, YV_INT(0)))) {
#line 429 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_row, YV_INT(0)), v_row));
  }
#line 430 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_right_model_index_from_line_col(v_editor_text, v_row, v_col);
}

static YisVal yis_right_model_outline_index_from_click(YisVal v_py) {
#line 434 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_row = yis_add(yis_stdr_floor(yis_div(yis_sub(v_py, v_CONTENT_Y), v_ROW_H)), v_outline_scroll); yis_retain_val(v_row);
#line 435 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_row, YV_INT(0)))) {
#line 436 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return yis_neg(YV_INT(1));
  }
#line 437 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_ge(v_row, YV_INT(stdr_len(v_current_outline))))) {
#line 438 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return yis_neg(YV_INT(1));
  }
#line 439 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return v_row;
}

static void yis_right_model_move_caret_line_start(YisVal v_shift) {
#line 447 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lc = yis_right_model_line_col_from_index(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 448 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_cur_line);
#line 449 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, yis_right_model_line_to_char_pos(v_editor_text, v_cur_line)), v_caret));
#line 450 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 451 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 453 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 454 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 455 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_line_end(YisVal v_shift) {
#line 459 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lc = yis_right_model_line_col_from_index(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 460 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_cur_line);
#line 461 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 462 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_pos = yis_right_model_line_to_char_pos(v_editor_text, v_cur_line); yis_retain_val(v_pos);
#line 463 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = v_pos; yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 464 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 465 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
#line 466 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_pos, yis_add(v_i, YV_INT(1))), v_pos));
  } }
#line 467 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_pos), v_caret));
#line 468 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 469 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 471 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 472 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 473 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_doc_start(YisVal v_shift) {
#line 477 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, YV_INT(0)), v_caret));
#line 478 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 479 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 481 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 482 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 483 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_doc_end(YisVal v_shift) {
#line 487 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, YV_INT(stdr_len(v_editor_text))), v_caret));
#line 488 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 489 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 491 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 492 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 493 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static YisVal yis_right_model_is_word_char(YisVal v_ch) {
#line 501 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(" ")))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\n")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\t"))))))) {
#line 502 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_BOOL(false);
  }
#line 503 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("(")))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(")")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("[")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("]"))))))) {
#line 504 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_BOOL(false);
  }
#line 505 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("{")))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("}")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(",")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(";"))))))) {
#line 506 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_BOOL(false);
  }
#line 507 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(".")))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(":")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\"")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("'"))))))) {
#line 508 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_BOOL(false);
  }
#line 509 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return YV_BOOL(true);
}

static void yis_right_model_select_word_at(YisVal v_pos) {
#line 513 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 514 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_total, YV_INT(0)))) {
#line 515 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 516 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_p = v_pos; yis_retain_val(v_p);
#line 517 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_ge(v_p, v_total))) {
#line 518 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_p, yis_sub(v_total, YV_INT(1))), v_p));
  }
#line 519 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_p, YV_INT(0)))) {
#line 520 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_p, YV_INT(0)), v_p));
  }
#line 521 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_ch = stdr_slice(v_editor_text, yis_as_int(v_p), yis_as_int(yis_add(v_p, YV_INT(1)))); yis_retain_val(v_ch);
#line 522 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_is_word_char(v_ch))))) {
#line 523 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, v_p), v_caret));
#line 524 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_p), v_sel_start));
#line 525 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, yis_add(v_p, YV_INT(1))), v_sel_end));
#line 526 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 528 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_a = v_p; yis_retain_val(v_a);
#line 529 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = yis_sub(v_p, YV_INT(1)); yis_retain_val(v_i);
  for (; yis_as_bool(yis_ge(v_i, YV_INT(0))); (void)((yis_move_into(&v_i, yis_sub(v_i, YV_INT(1))), v_i))) {
#line 530 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_c);
#line 531 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_is_word_char(v_c))))) {
#line 532 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
#line 533 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_a, v_i), v_a));
  } }
#line 535 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_b = yis_add(v_p, YV_INT(1)); yis_retain_val(v_b);
#line 536 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_j = yis_add(v_p, YV_INT(1)); yis_retain_val(v_j);
  for (; yis_as_bool(yis_lt(v_j, v_total)); (void)((yis_move_into(&v_j, yis_add(v_j, YV_INT(1))), v_j))) {
#line 537 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_j), yis_as_int(yis_add(v_j, YV_INT(1)))); yis_retain_val(v_c);
#line 538 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_is_word_char(v_c))))) {
#line 539 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
#line 540 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_b, yis_add(v_j, YV_INT(1))), v_b));
  } }
#line 541 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_a), v_sel_start));
#line 542 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_b), v_sel_end));
#line 543 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_b), v_caret));
}

static void yis_right_model_open_search(void) {
#line 551 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_active, YV_BOOL(true)), v_search_active));
#line 552 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_query, YV_STR(stdr_str_lit(""))), v_search_query));
#line 553 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_matches, yis_arr_lit(0)), v_search_matches));
#line 554 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_index, yis_neg(YV_INT(1))), v_search_index));
#line 556 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_right_model_has_selection())) {
#line 557 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_search_query, yis_right_model_selected_text()), v_search_query));
#line 558 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)(yis_right_model_run_search());
  }
}

static void yis_right_model_close_search(void) {
#line 562 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_active, YV_BOOL(false)), v_search_active));
#line 563 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_query, YV_STR(stdr_str_lit(""))), v_search_query));
#line 564 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_matches, yis_arr_lit(0)), v_search_matches));
#line 565 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_index, yis_neg(YV_INT(1))), v_search_index));
}

static void yis_right_model_run_search(void) {
#line 569 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_matches, yis_arr_lit(0)), v_search_matches));
#line 570 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_index, yis_neg(YV_INT(1))), v_search_index));
#line 571 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_qlen = YV_INT(stdr_len(v_search_query)); yis_retain_val(v_qlen);
#line 572 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_qlen, YV_INT(0)))) {
#line 573 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 574 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 575 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_le(v_i, yis_sub(v_total, v_qlen))); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 576 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_chunk = stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, v_qlen))); yis_retain_val(v_chunk);
#line 577 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(v_chunk, v_search_query))) {
#line 578 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_search_matches, yis_add(v_search_matches, yis_arr_lit(1, v_i))), v_search_matches));
    }
  } }
#line 579 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_search_matches)), YV_INT(0)))) {
#line 580 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_search_index, YV_INT(0)), v_search_index));
#line 581 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)(yis_right_model_jump_to_match());
  }
}

static void yis_right_model_jump_to_match(void) {
#line 585 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_count = YV_INT(stdr_len(v_search_matches)); yis_retain_val(v_count);
#line 586 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_count, YV_INT(0))) || yis_as_bool(yis_lt(v_search_index, YV_INT(0)))))) {
#line 587 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 588 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_pos = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_search_matches, v_search_index)).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_search_matches, v_search_index)))))); yis_retain_val(v_pos);
#line 589 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, yis_add(v_pos, YV_INT(stdr_len(v_search_query)))), v_caret));
#line 590 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_pos), v_sel_start));
#line 591 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
#line 592 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_search_next(void) {
#line 596 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_count = YV_INT(stdr_len(v_search_matches)); yis_retain_val(v_count);
#line 597 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_count, YV_INT(0)))) {
#line 598 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 599 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_index, yis_mod(yis_add(v_search_index, YV_INT(1)), v_count)), v_search_index));
#line 600 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_jump_to_match());
}

static void yis_right_model_search_prev(void) {
#line 604 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_count = YV_INT(stdr_len(v_search_matches)); yis_retain_val(v_count);
#line 605 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_count, YV_INT(0)))) {
#line 606 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 607 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_index, yis_mod(yis_add(yis_sub(v_search_index, YV_INT(1)), v_count), v_count)), v_search_index));
#line 608 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_jump_to_match());
}

static void yis_right_model_search_type(YisVal v_ch) {
#line 612 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_query, yis_add(v_search_query, v_ch)), v_search_query));
#line 613 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_run_search());
}

static void yis_right_model_search_backspace(void) {
#line 617 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_qlen = YV_INT(stdr_len(v_search_query)); yis_retain_val(v_qlen);
#line 618 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_qlen, YV_INT(0)))) {
#line 619 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_search_query, stdr_slice(v_search_query, yis_as_int(YV_INT(0)), yis_as_int(yis_sub(v_qlen, YV_INT(1))))), v_search_query));
#line 620 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)(yis_right_model_run_search());
  }
}

static void yis_right_model_reset_selection(void) {
#line 624 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 625 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
}

static YisVal __fnwrap_right_model_line_count(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_model_line_count(__a0);
}

static YisVal __fnwrap_right_model_line_col_from_index(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_right_model_line_col_from_index(__a0, __a1);
}

static YisVal __fnwrap_right_model_index_from_line_col(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  return yis_right_model_index_from_line_col(__a0, __a1, __a2);
}

static YisVal __fnwrap_right_model_sel_normalized(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_model_sel_normalized();
}

static YisVal __fnwrap_right_model_has_selection(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_model_has_selection();
}

static YisVal __fnwrap_right_model_visible_lines(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_model_visible_lines();
}

static YisVal __fnwrap_right_model_toggle_sidebar(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_toggle_sidebar();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_editor_left(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_model_editor_left();
}

static YisVal __fnwrap_right_model_editor_width(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_model_editor_width();
}

static YisVal __fnwrap_right_model_editor_cols(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_model_editor_cols();
}

static YisVal __fnwrap_right_model_line_to_char_pos(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_right_model_line_to_char_pos(__a0, __a1);
}

static YisVal __fnwrap_right_model_set_text(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_set_text(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_set_caret(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_set_caret(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_set_selection(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_right_model_set_selection(__a0, __a1);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_set_scroll(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_set_scroll(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_set_file_path(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_set_file_path(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_set_dirty(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_set_dirty(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_set_outline_selected(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_set_outline_selected(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_ensure_visible(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_ensure_visible();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_clamp_scroll(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_clamp_scroll();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_rebuild_outline(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_rebuild_outline();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_insert_at_caret(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_insert_at_caret(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_delete_selection(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_delete_selection();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_delete_backward(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_delete_backward();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_delete_forward(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_delete_forward();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_move_caret_left(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_move_caret_left(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_move_caret_right(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_move_caret_right(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_move_caret_up(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_move_caret_up(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_move_caret_down(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_move_caret_down(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_select_all(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_select_all();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_selected_text(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_model_selected_text();
}

static YisVal __fnwrap_right_model_scroll_by(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_scroll_by(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_caret_from_editor_click(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_right_model_caret_from_editor_click(__a0, __a1);
}

static YisVal __fnwrap_right_model_outline_index_from_click(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_model_outline_index_from_click(__a0);
}

static YisVal __fnwrap_right_model_move_caret_line_start(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_move_caret_line_start(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_move_caret_line_end(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_move_caret_line_end(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_move_caret_doc_start(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_move_caret_doc_start(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_move_caret_doc_end(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_move_caret_doc_end(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_is_word_char(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_model_is_word_char(__a0);
}

static YisVal __fnwrap_right_model_select_word_at(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_select_word_at(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_open_search(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_open_search();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_close_search(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_close_search();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_run_search(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_run_search();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_jump_to_match(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_jump_to_match();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_search_next(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_search_next();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_search_prev(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_search_prev();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_search_type(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_search_type(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_search_backspace(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_search_backspace();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_reset_selection(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_reset_selection();
  return YV_NULLV;
}

static void __yis_right_model_init(void) {
  yis_move_into(&v_WIN_W, YV_INT(1024));
  yis_move_into(&v_WIN_H, YV_INT(720));
  yis_move_into(&v_FONT_W, YV_INT(16));
  yis_move_into(&v_FONT_H, YV_INT(24));
  yis_move_into(&v_SCROLLBAR_W, YV_INT(12));
  yis_move_into(&v_SCROLLBAR_X, YV_INT(0));
  yis_move_into(&v_SIDEBAR_X, YV_INT(16));
  yis_move_into(&v_SIDEBAR_W, YV_INT(160));
  yis_move_into(&v_EDITOR_X, YV_INT(180));
  yis_move_into(&v_EDITOR_W, YV_INT(780));
  yis_move_into(&v_PAD_X, YV_INT(4));
  yis_move_into(&v_ROW_H, YV_INT(24));
  yis_move_into(&v_TITLEBAR_H, YV_INT(16));
  yis_move_into(&v_CONTENT_Y, YV_INT(40));
  yis_move_into(&v_COL_GUIDE, YV_INT(47));
  yis_move_into(&v_MAX_COLS, YV_INT(48));
  yis_move_into(&v_SIDEBAR_CHARS, YV_INT(9));
  yis_move_into(&v_MAX_LINES, YV_INT(28));
  yis_move_into(&v_STATUS_H, YV_INT(0));
  yis_move_into(&v_sidebar_visible, YV_BOOL(true));
  yis_move_into(&v_C_BG, YV_INT(0));
  yis_move_into(&v_C_FG, YV_INT(1));
  yis_move_into(&v_C_SEL, YV_INT(2));
  yis_move_into(&v_C_ACCENT, YV_INT(3));
  yis_move_into(&v_H_KEYWORD, YV_INT(3));
  yis_move_into(&v_H_COMMENT, YV_INT(2));
  yis_move_into(&v_H_STRING, YV_INT(2));
  yis_move_into(&v_H_TYPE, YV_INT(3));
  yis_move_into(&v_KEY_UP, YV_INT(82));
  yis_move_into(&v_KEY_DOWN, YV_INT(81));
  yis_move_into(&v_KEY_LEFT, YV_INT(80));
  yis_move_into(&v_KEY_RIGHT, YV_INT(79));
  yis_move_into(&v_KEY_RETURN, YV_INT(40));
  yis_move_into(&v_KEY_TAB, YV_INT(43));
  yis_move_into(&v_KEY_BACKSPACE, YV_INT(42));
  yis_move_into(&v_KEY_DELETE, YV_INT(76));
  yis_move_into(&v_KEY_ESCAPE, YV_INT(41));
  yis_move_into(&v_KEY_A, YV_INT(4));
  yis_move_into(&v_KEY_C, YV_INT(6));
  yis_move_into(&v_KEY_S, YV_INT(22));
  yis_move_into(&v_KEY_V, YV_INT(25));
  yis_move_into(&v_KEY_X, YV_INT(27));
  yis_move_into(&v_KEY_D, YV_INT(7));
  yis_move_into(&v_KEY_N, YV_INT(17));
  yis_move_into(&v_KEY_O, YV_INT(18));
  yis_move_into(&v_KEY_F, YV_INT(9));
  yis_move_into(&v_KEY_G, YV_INT(10));
  yis_move_into(&v_KEY_LGUI, YV_INT(227));
  yis_move_into(&v_KEY_RGUI, YV_INT(231));
  yis_move_into(&v_editor_text, YV_STR(stdr_str_lit("")));
  yis_move_into(&v_caret, YV_INT(0));
  yis_move_into(&v_sel_start, YV_INT(0));
  yis_move_into(&v_sel_end, YV_INT(0));
  yis_move_into(&v_scroll_y, YV_INT(0));
  yis_move_into(&v_current_outline, yis_arr_lit(0));
  yis_move_into(&v_outline_selected, yis_neg(YV_INT(1)));
  yis_move_into(&v_outline_scroll, YV_INT(0));
  yis_move_into(&v_file_path, YV_STR(stdr_str_lit("")));
  yis_move_into(&v_dirty, YV_BOOL(false));
  yis_move_into(&v_search_active, YV_BOOL(false));
  yis_move_into(&v_search_query, YV_STR(stdr_str_lit("")));
  yis_move_into(&v_search_matches, yis_arr_lit(0));
  yis_move_into(&v_search_index, yis_neg(YV_INT(1)));
}

/* end embedded module: right_model */

/* begin embedded module: right_menu */
static void yis_right_menu_fill(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static YisVal yis_right_menu_item_count(YisVal);
static YisVal yis_right_menu_cat_x(YisVal);
static YisVal yis_right_menu_item_label(YisVal, YisVal);
static YisVal yis_right_menu_item_shortcut(YisVal, YisVal);
static YisVal yis_right_menu_action_none(void);
static YisVal yis_right_menu_action_save(void);
static YisVal yis_right_menu_action_copy(void);
static YisVal yis_right_menu_action_paste(void);
static YisVal yis_right_menu_action_cut(void);
static YisVal yis_right_menu_action_select_all(void);
static YisVal yis_right_menu_action_toggle_sidebar(void);
static YisVal yis_right_menu_item_action(YisVal, YisVal);
static YisVal yis_right_menu_is_open(void);
static void yis_right_menu_close(void);
static void yis_right_menu_draw_menubar(YisVal);
static void yis_right_menu_draw_submenu(YisVal);
static YisVal yis_right_menu_cat_from_x(YisVal);
static YisVal yis_right_menu_handle_click(YisVal, YisVal);
static void yis_right_menu_handle_hover(YisVal, YisVal);
static YisVal __fnwrap_right_menu_fill(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_item_count(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_cat_x(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_item_label(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_item_shortcut(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_action_none(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_action_save(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_action_copy(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_action_paste(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_action_cut(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_action_select_all(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_action_toggle_sidebar(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_item_action(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_is_open(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_close(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_draw_menubar(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_draw_submenu(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_cat_from_x(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_handle_click(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_handle_hover(void*,int,YisVal*);
static YisVal v_menu_open = YV_NULLV;
static YisVal v_menu_hover = YV_NULLV;
static YisVal v_MENUBAR_Y = YV_NULLV;
static YisVal v_MENUBAR_H = YV_NULLV;
static YisVal v_MENU_START_X = YV_NULLV;
static YisVal v_MENU_GAP = YV_NULLV;
static YisVal v_SUBMENU_W = YV_NULLV;
static YisVal v_SUBMENU_ITEM_H = YV_NULLV;
static YisVal v_CAT_FILE_X = YV_NULLV;
static YisVal v_CAT_FILE_W = YV_NULLV;
static YisVal v_CAT_EDIT_X = YV_NULLV;
static YisVal v_CAT_EDIT_W = YV_NULLV;
static YisVal v_CAT_FIND_X = YV_NULLV;
static YisVal v_CAT_FIND_W = YV_NULLV;
static YisVal v_CAT_VIEW_X = YV_NULLV;
static YisVal v_CAT_VIEW_W = YV_NULLV;
static YisVal v_CAT_FILE = YV_NULLV;
static YisVal v_CAT_EDIT = YV_NULLV;
static YisVal v_CAT_FIND = YV_NULLV;
static YisVal v_CAT_VIEW = YV_NULLV;
static YisVal v_FILE_COUNT = YV_NULLV;
static YisVal v_EDIT_COUNT = YV_NULLV;
static YisVal v_FIND_COUNT = YV_NULLV;
static YisVal v_VIEW_COUNT = YV_NULLV;

// cask right_menu
// bring stdr
// bring vimana
// bring right_model
static void yis_right_menu_fill(YisVal v_scr, YisVal v_x, YisVal v_y, YisVal v_w, YisVal v_h, YisVal v_c) {
#line 48 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_auto(v_scr, YV_INT(1)));
#line 49 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  { YisVal v_row = YV_INT(0); yis_retain_val(v_row);
  for (; yis_as_bool(yis_lt(v_row, v_h)); (void)((yis_move_into(&v_row, yis_add(v_row, YV_INT(1))), v_row))) {
#line 50 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_y, v_row)));
#line 51 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_m_vimana_screen_set_x(v_scr, v_x));
#line 52 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    { YisVal v_col = YV_INT(0); yis_retain_val(v_col);
    for (; yis_as_bool(yis_lt(v_col, v_w)); (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(1))), v_col))) {
#line 53 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)(yis_m_vimana_screen_pixel(v_scr, v_c));
    } }
  } }
#line 54 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_auto(v_scr, YV_INT(0)));
}

static YisVal yis_right_menu_item_count(YisVal v_cat) {
#line 58 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FILE))) {
#line 59 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_FILE_COUNT;
  }
#line 60 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_EDIT))) {
#line 61 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_EDIT_COUNT;
  }
#line 62 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FIND))) {
#line 63 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_FIND_COUNT;
  }
#line 64 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_VIEW))) {
#line 65 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_VIEW_COUNT;
  }
#line 66 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_INT(0);
}

static YisVal yis_right_menu_cat_x(YisVal v_cat) {
#line 70 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FILE))) {
#line 71 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_FILE_X;
  }
#line 72 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_EDIT))) {
#line 73 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_EDIT_X;
  }
#line 74 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FIND))) {
#line 75 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_FIND_X;
  }
#line 76 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_VIEW))) {
#line 77 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_VIEW_X;
  }
#line 78 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_INT(0);
}

static YisVal yis_right_menu_item_label(YisVal v_cat, YisVal v_idx) {
#line 82 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FILE))) {
#line 83 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 84 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("New"));
    }
#line 85 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 86 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Open"));
    }
#line 87 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 88 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Save"));
    }
  }
#line 89 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_EDIT))) {
#line 90 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 91 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Copy"));
    }
#line 92 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 93 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Paste"));
    }
#line 94 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 95 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cut"));
    }
#line 96 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 97 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Select All"));
    }
  }
#line 98 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FIND))) {
#line 99 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 100 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Find"));
    }
#line 101 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 102 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Find Next"));
    }
  }
#line 103 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_VIEW))) {
#line 104 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 105 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Sidebar"));
    }
  }
#line 106 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit(""));
}

static YisVal yis_right_menu_item_shortcut(YisVal v_cat, YisVal v_idx) {
#line 110 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FILE))) {
#line 111 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 112 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+N"));
    }
#line 113 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 114 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+O"));
    }
#line 115 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 116 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+S"));
    }
  }
#line 117 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_EDIT))) {
#line 118 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 119 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+C"));
    }
#line 120 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 121 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+V"));
    }
#line 122 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 123 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+X"));
    }
#line 124 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 125 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+A"));
    }
  }
#line 126 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FIND))) {
#line 127 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 128 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+F"));
    }
#line 129 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 130 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+G"));
    }
  }
#line 131 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_VIEW))) {
#line 132 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 133 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+D"));
    }
  }
#line 134 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit(""));
}

static YisVal yis_right_menu_action_none(void) {
#line 139 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit(""));
}

static YisVal yis_right_menu_action_save(void) {
#line 142 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit("save"));
}

static YisVal yis_right_menu_action_copy(void) {
#line 145 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit("copy"));
}

static YisVal yis_right_menu_action_paste(void) {
#line 148 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit("paste"));
}

static YisVal yis_right_menu_action_cut(void) {
#line 151 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit("cut"));
}

static YisVal yis_right_menu_action_select_all(void) {
#line 154 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit("select_all"));
}

static YisVal yis_right_menu_action_toggle_sidebar(void) {
#line 157 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit("toggle_sidebar"));
}

static YisVal yis_right_menu_item_action(YisVal v_cat, YisVal v_idx) {
#line 161 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FILE))) {
#line 162 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 163 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("new"));
    }
#line 164 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 165 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("open"));
    }
#line 166 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 167 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("save"));
    }
  }
#line 168 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_EDIT))) {
#line 169 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 170 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("copy"));
    }
#line 171 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 172 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("paste"));
    }
#line 173 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 174 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("cut"));
    }
#line 175 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 176 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("select_all"));
    }
  }
#line 177 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FIND))) {
#line 178 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 179 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("find"));
    }
#line 180 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 181 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("find_next"));
    }
  }
#line 182 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_VIEW))) {
#line 183 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 184 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("toggle_sidebar"));
    }
  }
#line 185 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit(""));
}

static YisVal yis_right_menu_is_open(void) {
#line 193 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return yis_ge(v_menu_open, YV_INT(0));
}

static void yis_right_menu_close(void) {
#line 197 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_menu_open, yis_neg(YV_INT(1))), v_menu_open));
#line 198 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_menu_hover, yis_neg(YV_INT(1))), v_menu_hover));
}

static void yis_right_menu_draw_menubar(YisVal v_scr) {
#line 207 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_bx = YV_INT(0); yis_retain_val(v_bx);
#line 208 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_by = yis_sub(v_CONTENT_Y, YV_INT(1)); yis_retain_val(v_by);
#line 209 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  { YisVal v_col = YV_INT(0); yis_retain_val(v_col);
  for (; yis_as_bool(yis_lt(v_col, v_WIN_W)); (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(2))), v_col))) {
#line 210 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_m_vimana_screen_set_x(v_scr, v_col));
#line 211 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_m_vimana_screen_set_y(v_scr, v_by));
#line 212 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_m_vimana_screen_pixel(v_scr, v_C_SEL));
  } }
#line 215 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_ty = yis_add(v_MENUBAR_Y, YV_INT(2)); yis_retain_val(v_ty);
#line 216 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_fg = v_C_FG; yis_retain_val(v_fg);
#line 217 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_menu_open, v_CAT_FILE))) {
#line 218 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_fg, v_C_ACCENT), v_fg));
  }
#line 219 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, v_CAT_FILE_X, v_ty, YV_STR(stdr_str_lit("File")), v_fg, v_C_BG));
#line 221 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_fg, v_C_FG), v_fg));
#line 222 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_menu_open, v_CAT_EDIT))) {
#line 223 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_fg, v_C_ACCENT), v_fg));
  }
#line 224 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, v_CAT_EDIT_X, v_ty, YV_STR(stdr_str_lit("Edit")), v_fg, v_C_BG));
#line 226 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_fg, v_C_FG), v_fg));
#line 227 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_menu_open, v_CAT_FIND))) {
#line 228 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_fg, v_C_ACCENT), v_fg));
  }
#line 229 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, v_CAT_FIND_X, v_ty, YV_STR(stdr_str_lit("Find")), v_fg, v_C_BG));
#line 231 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_fg, v_C_FG), v_fg));
#line 232 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_menu_open, v_CAT_VIEW))) {
#line 233 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_fg, v_C_ACCENT), v_fg));
  }
#line 234 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, v_CAT_VIEW_X, v_ty, YV_STR(stdr_str_lit("View")), v_fg, v_C_BG));
}

static void yis_right_menu_draw_submenu(YisVal v_scr) {
#line 238 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_lt(v_menu_open, YV_INT(0)))) {
#line 239 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return;
  }
#line 241 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_cat = v_menu_open; yis_retain_val(v_cat);
#line 242 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_count = yis_right_menu_item_count(v_cat); yis_retain_val(v_count);
#line 243 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sx = yis_right_menu_cat_x(v_cat); yis_retain_val(v_sx);
#line 244 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sy = v_CONTENT_Y; yis_retain_val(v_sy);
#line 245 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sw = v_SUBMENU_W; yis_retain_val(v_sw);
#line 246 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sh = yis_mul(v_count, v_SUBMENU_ITEM_H); yis_retain_val(v_sh);
#line 247 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_th = yis_add(v_sh, YV_INT(2)); yis_retain_val(v_th);
#line 250 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, v_sx, v_sy, yis_add(v_sw, YV_INT(1)), v_th, v_C_BG));
#line 253 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, v_sx, v_sy, yis_add(v_sw, YV_INT(1)), YV_INT(1), v_C_FG));
#line 254 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, v_sx, yis_sub(yis_add(v_sy, v_th), YV_INT(1)), yis_add(v_sw, YV_INT(1)), YV_INT(1), v_C_FG));
#line 255 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, v_sx, v_sy, YV_INT(1), v_th, v_C_FG));
#line 256 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, yis_add(v_sx, v_sw), v_sy, YV_INT(1), v_th, v_C_FG));
#line 259 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_count)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 260 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_iy = yis_add(yis_add(v_sy, YV_INT(1)), yis_mul(v_i, v_SUBMENU_ITEM_H)); yis_retain_val(v_iy);
#line 261 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_label = yis_right_menu_item_label(v_cat, v_i); yis_retain_val(v_label);
#line 262 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_shortcut = yis_right_menu_item_shortcut(v_cat, v_i); yis_retain_val(v_shortcut);
#line 264 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_i, v_menu_hover))) {
#line 265 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)(yis_right_menu_fill(v_scr, yis_add(v_sx, YV_INT(1)), v_iy, yis_sub(v_sw, YV_INT(2)), v_SUBMENU_ITEM_H, v_C_ACCENT));
#line 266 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)(yis_m_vimana_screen_put_text(v_scr, yis_add(v_sx, YV_INT(8)), v_iy, v_label, v_C_FG, v_C_ACCENT));
#line 267 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      YisVal v_sc_x = yis_sub(yis_sub(yis_add(v_sx, v_sw), YV_INT(8)), yis_mul(YV_INT(stdr_len(v_shortcut)), v_FONT_W)); yis_retain_val(v_sc_x);
#line 268 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)(yis_m_vimana_screen_put_text(v_scr, v_sc_x, v_iy, v_shortcut, v_C_FG, v_C_ACCENT));
    } else {
#line 270 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)(yis_m_vimana_screen_put_text(v_scr, yis_add(v_sx, YV_INT(8)), v_iy, v_label, v_C_FG, v_C_BG));
#line 271 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      YisVal v_sc_x = yis_sub(yis_sub(yis_add(v_sx, v_sw), YV_INT(8)), yis_mul(YV_INT(stdr_len(v_shortcut)), v_FONT_W)); yis_retain_val(v_sc_x);
#line 272 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)(yis_m_vimana_screen_put_text(v_scr, v_sc_x, v_iy, v_shortcut, v_C_SEL, v_C_BG));
    }
  } }
}

static YisVal yis_right_menu_cat_from_x(YisVal v_mx) {
#line 280 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_CAT_FILE_X)) && yis_as_bool(yis_lt(v_mx, yis_add(v_CAT_FILE_X, v_CAT_FILE_W)))))) {
#line 281 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_FILE;
  }
#line 282 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_CAT_EDIT_X)) && yis_as_bool(yis_lt(v_mx, yis_add(v_CAT_EDIT_X, v_CAT_EDIT_W)))))) {
#line 283 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_EDIT;
  }
#line 284 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_CAT_FIND_X)) && yis_as_bool(yis_lt(v_mx, yis_add(v_CAT_FIND_X, v_CAT_FIND_W)))))) {
#line 285 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_FIND;
  }
#line 286 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_CAT_VIEW_X)) && yis_as_bool(yis_lt(v_mx, yis_add(v_CAT_VIEW_X, v_CAT_VIEW_W)))))) {
#line 287 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_VIEW;
  }
#line 288 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return yis_neg(YV_INT(1));
}

static YisVal yis_right_menu_handle_click(YisVal v_mx, YisVal v_my) {
#line 293 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_my, v_MENUBAR_Y)) && yis_as_bool(yis_lt(v_my, v_CONTENT_Y))))) {
#line 294 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_cat = yis_right_menu_cat_from_x(v_mx); yis_retain_val(v_cat);
#line 295 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_ge(v_cat, YV_INT(0)))) {
#line 296 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      if (yis_as_bool(yis_eq(v_menu_open, v_cat))) {
#line 297 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
        (void)(yis_right_menu_close());
      } else {
#line 299 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
        (void)((yis_move_into(&v_menu_open, v_cat), v_menu_open));
#line 300 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
        (void)((yis_move_into(&v_menu_hover, yis_neg(YV_INT(1))), v_menu_hover));
      }
#line 301 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit(""));
    }
#line 302 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_close());
#line 303 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return YV_STR(stdr_str_lit(""));
  }
#line 306 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_ge(v_menu_open, YV_INT(0)))) {
#line 307 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_cat = v_menu_open; yis_retain_val(v_cat);
#line 308 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_sx = yis_right_menu_cat_x(v_cat); yis_retain_val(v_sx);
#line 309 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_sy = yis_add(v_CONTENT_Y, YV_INT(1)); yis_retain_val(v_sy);
#line 310 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_sw = v_SUBMENU_W; yis_retain_val(v_sw);
#line 311 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_count = yis_right_menu_item_count(v_cat); yis_retain_val(v_count);
#line 312 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_sh = yis_mul(v_count, v_SUBMENU_ITEM_H); yis_retain_val(v_sh);
#line 314 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_sx)) && yis_as_bool(yis_lt(v_mx, yis_add(v_sx, v_sw))))) && yis_as_bool(yis_ge(v_my, v_sy)))) && yis_as_bool(yis_lt(v_my, yis_add(v_sy, v_sh)))))) {
#line 315 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      YisVal v_idx = yis_stdr_floor(yis_div(yis_sub(v_my, v_sy), v_SUBMENU_ITEM_H)); yis_retain_val(v_idx);
#line 316 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      YisVal v_action = yis_right_menu_item_action(v_cat, v_idx); yis_retain_val(v_action);
#line 317 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)(yis_right_menu_close());
#line 318 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return v_action;
    }
#line 321 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_close());
#line 322 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return YV_STR(stdr_str_lit(""));
  }
#line 324 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit("pass"));
}

static void yis_right_menu_handle_hover(YisVal v_mx, YisVal v_my) {
#line 328 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_lt(v_menu_open, YV_INT(0)))) {
#line 329 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return;
  }
#line 332 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_my, v_MENUBAR_Y)) && yis_as_bool(yis_lt(v_my, v_CONTENT_Y))))) {
#line 333 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_cat = yis_right_menu_cat_from_x(v_mx); yis_retain_val(v_cat);
#line 334 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_cat, YV_INT(0))) && yis_as_bool(yis_ne(v_cat, v_menu_open))))) {
#line 335 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_menu_open, v_cat), v_menu_open));
#line 336 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_menu_hover, yis_neg(YV_INT(1))), v_menu_hover));
    }
#line 337 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return;
  }
#line 340 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_cat = v_menu_open; yis_retain_val(v_cat);
#line 341 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sx = yis_right_menu_cat_x(v_cat); yis_retain_val(v_sx);
#line 342 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sy = yis_add(v_CONTENT_Y, YV_INT(1)); yis_retain_val(v_sy);
#line 343 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sw = v_SUBMENU_W; yis_retain_val(v_sw);
#line 344 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_count = yis_right_menu_item_count(v_cat); yis_retain_val(v_count);
#line 345 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sh = yis_mul(v_count, v_SUBMENU_ITEM_H); yis_retain_val(v_sh);
#line 347 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_sx)) && yis_as_bool(yis_lt(v_mx, yis_add(v_sx, v_sw))))) && yis_as_bool(yis_ge(v_my, v_sy)))) && yis_as_bool(yis_lt(v_my, yis_add(v_sy, v_sh)))))) {
#line 348 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_menu_hover, yis_stdr_floor(yis_div(yis_sub(v_my, v_sy), v_SUBMENU_ITEM_H))), v_menu_hover));
  } else {
#line 350 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_menu_hover, yis_neg(YV_INT(1))), v_menu_hover));
  }
}

static YisVal __fnwrap_right_menu_fill(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  YisVal __a4 = argc > 4 ? argv[4] : YV_NULLV;
  YisVal __a5 = argc > 5 ? argv[5] : YV_NULLV;
  yis_right_menu_fill(__a0, __a1, __a2, __a3, __a4, __a5);
  return YV_NULLV;
}

static YisVal __fnwrap_right_menu_item_count(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_menu_item_count(__a0);
}

static YisVal __fnwrap_right_menu_cat_x(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_menu_cat_x(__a0);
}

static YisVal __fnwrap_right_menu_item_label(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_right_menu_item_label(__a0, __a1);
}

static YisVal __fnwrap_right_menu_item_shortcut(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_right_menu_item_shortcut(__a0, __a1);
}

static YisVal __fnwrap_right_menu_action_none(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_menu_action_none();
}

static YisVal __fnwrap_right_menu_action_save(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_menu_action_save();
}

static YisVal __fnwrap_right_menu_action_copy(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_menu_action_copy();
}

static YisVal __fnwrap_right_menu_action_paste(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_menu_action_paste();
}

static YisVal __fnwrap_right_menu_action_cut(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_menu_action_cut();
}

static YisVal __fnwrap_right_menu_action_select_all(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_menu_action_select_all();
}

static YisVal __fnwrap_right_menu_action_toggle_sidebar(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_menu_action_toggle_sidebar();
}

static YisVal __fnwrap_right_menu_item_action(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_right_menu_item_action(__a0, __a1);
}

static YisVal __fnwrap_right_menu_is_open(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_menu_is_open();
}

static YisVal __fnwrap_right_menu_close(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_menu_close();
  return YV_NULLV;
}

static YisVal __fnwrap_right_menu_draw_menubar(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_menu_draw_menubar(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_menu_draw_submenu(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_menu_draw_submenu(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_menu_cat_from_x(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_menu_cat_from_x(__a0);
}

static YisVal __fnwrap_right_menu_handle_click(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_right_menu_handle_click(__a0, __a1);
}

static YisVal __fnwrap_right_menu_handle_hover(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_right_menu_handle_hover(__a0, __a1);
  return YV_NULLV;
}

static void __yis_right_menu_init(void) {
  yis_move_into(&v_menu_open, yis_neg(YV_INT(1)));
  yis_move_into(&v_menu_hover, yis_neg(YV_INT(1)));
  yis_move_into(&v_MENUBAR_Y, YV_INT(0));
  yis_move_into(&v_MENUBAR_H, YV_INT(24));
  yis_move_into(&v_MENU_START_X, YV_INT(16));
  yis_move_into(&v_MENU_GAP, YV_INT(8));
  yis_move_into(&v_SUBMENU_W, YV_INT(224));
  yis_move_into(&v_SUBMENU_ITEM_H, YV_INT(24));
  yis_move_into(&v_CAT_FILE_X, YV_INT(16));
  yis_move_into(&v_CAT_FILE_W, YV_INT(80));
  yis_move_into(&v_CAT_EDIT_X, YV_INT(96));
  yis_move_into(&v_CAT_EDIT_W, YV_INT(80));
  yis_move_into(&v_CAT_FIND_X, YV_INT(176));
  yis_move_into(&v_CAT_FIND_W, YV_INT(80));
  yis_move_into(&v_CAT_VIEW_X, YV_INT(256));
  yis_move_into(&v_CAT_VIEW_W, YV_INT(80));
  yis_move_into(&v_CAT_FILE, YV_INT(0));
  yis_move_into(&v_CAT_EDIT, YV_INT(1));
  yis_move_into(&v_CAT_FIND, YV_INT(2));
  yis_move_into(&v_CAT_VIEW, YV_INT(3));
  yis_move_into(&v_FILE_COUNT, YV_INT(3));
  yis_move_into(&v_EDIT_COUNT, YV_INT(4));
  yis_move_into(&v_FIND_COUNT, YV_INT(2));
  yis_move_into(&v_VIEW_COUNT, YV_INT(1));
}

/* end embedded module: right_menu */

/* begin embedded module: right_syntax */
static YisVal yis_right_syntax_starts(YisVal, YisVal);
static YisVal yis_right_syntax_line_color(YisVal);
static YisVal __fnwrap_right_syntax_starts(void*,int,YisVal*);
static YisVal __fnwrap_right_syntax_line_color(void*,int,YisVal*);

// cask right_syntax
// bring stdr
// bring right_model
static YisVal yis_right_syntax_starts(YisVal v_text, YisVal v_prefix) {
#line 12 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  return yis_stdr_starts_with(v_text, v_prefix);
}

static YisVal yis_right_syntax_line_color(YisVal v_line) {
#line 16 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  YisVal v_t = yis_stdr_trim(v_line); yis_retain_val(v_t);
#line 17 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  YisVal v_n = YV_INT(stdr_len(v_t)); yis_retain_val(v_n);
#line 18 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(yis_eq(v_n, YV_INT(0)))) {
#line 19 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_C_FG;
  }
#line 22 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit("-- "))))) {
#line 23 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_COMMENT;
  }
#line 24 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(yis_eq(v_t, YV_STR(stdr_str_lit("--"))))) {
#line 25 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_COMMENT;
  }
#line 28 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit("cask ")))) || yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit("bring "))))))) {
#line 29 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 30 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit("def ")))) || yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit("let "))))))) {
#line 31 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 32 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit("if ")))) || yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit("elif ")))))) || yis_as_bool(yis_eq(v_t, YV_STR(stdr_str_lit("else")))))) || yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit("else "))))))) {
#line 33 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 34 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit("for ")))) || yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit("while ")))))) || yis_as_bool(yis_eq(v_t, YV_STR(stdr_str_lit("break")))))) || yis_as_bool(yis_eq(v_t, YV_STR(stdr_str_lit("continue"))))))) {
#line 35 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 36 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit("<- ")))) || yis_as_bool(yis_eq(v_t, YV_STR(stdr_str_lit("<-"))))))) {
#line 37 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 38 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit(":: ")))) || yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit(": ")))))) || yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit("!: ")))))) || yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit(",: "))))))) {
#line 39 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 40 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(yis_right_syntax_starts(v_t, YV_STR(stdr_str_lit("-> "))))) {
#line 41 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 43 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  return v_C_FG;
}

static YisVal __fnwrap_right_syntax_starts(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_right_syntax_starts(__a0, __a1);
}

static YisVal __fnwrap_right_syntax_line_color(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_syntax_line_color(__a0);
}

/* end embedded module: right_syntax */

/* begin embedded module: right_view */
static void yis_right_view_fill(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static YisVal yis_right_view_clamp_text(YisVal, YisVal);
static void yis_right_view_fill_halftone(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static void yis_right_view_draw_scrollbar(YisVal);
static void yis_right_view_draw_separator(YisVal);
static YisVal yis_right_view_type_prefix(YisVal);
static YisVal yis_right_view_type_color(YisVal);
static void yis_right_view_draw_outline(YisVal);
static void yis_right_view_draw_col_guide(YisVal);
static void yis_right_view_draw_status(YisVal);
static void yis_right_view_draw_editor(YisVal);
static void yis_right_view_draw_search(YisVal);
static void yis_right_view_draw_all(YisVal);
static YisVal __fnwrap_right_view_fill(void*,int,YisVal*);
static YisVal __fnwrap_right_view_clamp_text(void*,int,YisVal*);
static YisVal __fnwrap_right_view_fill_halftone(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_scrollbar(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_separator(void*,int,YisVal*);
static YisVal __fnwrap_right_view_type_prefix(void*,int,YisVal*);
static YisVal __fnwrap_right_view_type_color(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_outline(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_col_guide(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_status(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_editor(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_search(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_all(void*,int,YisVal*);

// cask right_view
// bring stdr
// bring vimana
// bring right_model
// bring right_menu
// bring right_syntax
static void yis_right_view_fill(YisVal v_scr, YisVal v_x, YisVal v_y, YisVal v_w, YisVal v_h, YisVal v_c) {
#line 15 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_set_auto(v_scr, YV_INT(1)));
#line 16 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  { YisVal v_row = YV_INT(0); yis_retain_val(v_row);
  for (; yis_as_bool(yis_lt(v_row, v_h)); (void)((yis_move_into(&v_row, yis_add(v_row, YV_INT(1))), v_row))) {
#line 17 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_y, v_row)));
#line 18 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)(yis_m_vimana_screen_set_x(v_scr, v_x));
#line 19 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    { YisVal v_col = YV_INT(0); yis_retain_val(v_col);
    for (; yis_as_bool(yis_lt(v_col, v_w)); (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(1))), v_col))) {
#line 20 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)(yis_m_vimana_screen_pixel(v_scr, v_c));
    } }
  } }
#line 21 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_set_auto(v_scr, YV_INT(0)));
}

static YisVal yis_right_view_clamp_text(YisVal v_text, YisVal v_max_chars) {
#line 25 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_n = YV_INT(stdr_len(v_text)); yis_retain_val(v_n);
#line 26 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_le(v_n, v_max_chars))) {
#line 27 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return v_text;
  }
#line 28 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  return yis_add(stdr_slice(v_text, yis_as_int(YV_INT(0)), yis_as_int(yis_sub(v_max_chars, YV_INT(1)))), YV_STR(stdr_str_lit("~")));
}

static void yis_right_view_fill_halftone(YisVal v_scr, YisVal v_x, YisVal v_y, YisVal v_w, YisVal v_h, YisVal v_c) {
#line 36 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_set_auto(v_scr, YV_INT(0)));
#line 37 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  { YisVal v_row = YV_INT(0); yis_retain_val(v_row);
  for (; yis_as_bool(yis_lt(v_row, v_h)); (void)((yis_move_into(&v_row, yis_add(v_row, YV_INT(1))), v_row))) {
#line 38 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    { YisVal v_col = YV_INT(0); yis_retain_val(v_col);
    for (; yis_as_bool(yis_lt(v_col, v_w)); (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(1))), v_col))) {
#line 39 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_eq(yis_mod(yis_add(v_row, v_col), YV_INT(2)), YV_INT(0)))) {
#line 40 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_x, v_col)));
#line 41 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_y, v_row)));
#line 42 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_pixel(v_scr, v_c));
      }
    } }
  } }
}

static void yis_right_view_draw_scrollbar(YisVal v_scr) {
#line 50 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_bar_x = v_SCROLLBAR_X; yis_retain_val(v_bar_x);
#line 51 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_bar_w = v_SCROLLBAR_W; yis_retain_val(v_bar_w);
#line 52 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_bar_y = v_CONTENT_Y; yis_retain_val(v_bar_y);
#line 53 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_bar_h = yis_sub(yis_sub(v_WIN_H, v_CONTENT_Y), v_STATUS_H); yis_retain_val(v_bar_h);
#line 56 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_fill_halftone(v_scr, v_bar_x, v_bar_y, v_bar_w, v_bar_h, v_C_FG));
#line 59 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_total = yis_right_model_line_count(v_editor_text); yis_retain_val(v_total);
#line 60 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_visible = v_MAX_LINES; yis_retain_val(v_visible);
#line 61 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_le(v_total, v_visible))) {
#line 62 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)(yis_right_view_fill(v_scr, v_bar_x, v_bar_y, v_bar_w, v_bar_h, v_C_FG));
#line 63 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 65 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_thumb_h = yis_stdr_floor(yis_div(yis_mul(v_bar_h, v_visible), v_total)); yis_retain_val(v_thumb_h);
#line 66 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_lt(v_thumb_h, YV_INT(8)))) {
#line 67 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)((yis_move_into(&v_thumb_h, YV_INT(8)), v_thumb_h));
  }
#line 68 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_thumb_y = yis_add(v_bar_y, yis_stdr_floor(yis_div(yis_mul(yis_sub(v_bar_h, v_thumb_h), v_scroll_y), yis_sub(v_total, v_visible)))); yis_retain_val(v_thumb_y);
#line 69 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_fill(v_scr, v_bar_x, v_thumb_y, v_bar_w, v_thumb_h, v_C_FG));
}

static void yis_right_view_draw_separator(YisVal v_scr) {
#line 77 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(v_sidebar_visible)))) {
#line 78 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 79 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sep_x = yis_add(v_SIDEBAR_X, v_SIDEBAR_W); yis_retain_val(v_sep_x);
#line 80 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_fill(v_scr, v_sep_x, v_CONTENT_Y, YV_INT(1), yis_sub(yis_sub(v_WIN_H, v_CONTENT_Y), v_STATUS_H), v_C_FG));
}

static YisVal yis_right_view_type_prefix(YisVal v_kind) {
#line 90 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("cask"))))) {
#line 91 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return YV_STR(stdr_str_lit("@ "));
  }
#line 92 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("bring"))))) {
#line 93 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return YV_STR(stdr_str_lit("> "));
  }
#line 94 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("struct"))))) {
#line 95 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return YV_STR(stdr_str_lit(", "));
  }
#line 96 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("fn")))) || yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("pub_fn"))))))) {
#line 97 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return YV_STR(stdr_str_lit(": "));
  }
#line 98 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("const"))))) {
#line 99 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return YV_STR(stdr_str_lit("= "));
  }
#line 100 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  return YV_STR(stdr_str_lit("  "));
}

static YisVal yis_right_view_type_color(YisVal v_kind) {
#line 104 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("cask"))))) {
#line 105 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return v_C_ACCENT;
  }
#line 106 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("bring"))))) {
#line 107 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return v_C_SEL;
  }
#line 108 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("fn")))) || yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("pub_fn"))))))) {
#line 109 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return v_C_FG;
  }
#line 110 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("struct"))))) {
#line 111 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return v_C_ACCENT;
  }
#line 112 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("const"))))) {
#line 113 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return v_C_SEL;
  }
#line 114 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  return v_C_FG;
}

static void yis_right_view_draw_outline(YisVal v_scr) {
#line 118 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(v_sidebar_visible)))) {
#line 119 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 121 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_n = YV_INT(stdr_len(v_current_outline)); yis_retain_val(v_n);
#line 122 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_max_rows = yis_right_model_visible_lines(); yis_retain_val(v_max_rows);
#line 123 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sx = yis_add(v_SIDEBAR_X, v_PAD_X); yis_retain_val(v_sx);
#line 125 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_le(v_n, YV_INT(0)))) {
#line 126 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, v_CONTENT_Y, YV_STR(stdr_str_lit("(no outline)")), v_C_SEL, v_C_BG));
#line 128 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 130 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_max_rows)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 131 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_idx = yis_add(v_outline_scroll, v_i); yis_retain_val(v_idx);
#line 132 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(yis_ge(v_idx, v_n))) {
#line 133 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      break;
    }
#line 134 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_y = yis_add(v_CONTENT_Y, yis_mul(v_i, v_ROW_H)); yis_retain_val(v_y);
#line 135 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_item = yis_index(v_current_outline, v_idx); yis_retain_val(v_item);
#line 136 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_kind = YV_STR(stdr_to_string(((yis_index(v_item, YV_STR(stdr_str_lit("type")))).tag == EVT_NULL ? (YV_STR(stdr_str_lit(""))) : (yis_index(v_item, YV_STR(stdr_str_lit("type"))))))); yis_retain_val(v_kind);
#line 137 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_prefix = yis_right_view_type_prefix(v_kind); yis_retain_val(v_prefix);
#line 138 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_label = yis_right_view_clamp_text(YV_STR(stdr_to_string(((yis_index(v_item, YV_STR(stdr_str_lit("label")))).tag == EVT_NULL ? (YV_STR(stdr_str_lit(""))) : (yis_index(v_item, YV_STR(stdr_str_lit("label"))))))), yis_sub(v_SIDEBAR_CHARS, YV_INT(2))); yis_retain_val(v_label);
#line 139 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_display = yis_add(v_prefix, v_label); yis_retain_val(v_display);
#line 141 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(yis_eq(v_idx, v_outline_selected))) {
#line 142 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)(yis_right_view_fill(v_scr, v_SIDEBAR_X, v_y, yis_sub(v_SIDEBAR_W, YV_INT(1)), v_ROW_H, v_C_ACCENT));
#line 144 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, v_y, v_display, v_C_BG, v_C_ACCENT));
    } else {
#line 146 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_fg = yis_right_view_type_color(v_kind); yis_retain_val(v_fg);
#line 147 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, v_y, v_display, v_fg, v_C_BG));
    }
  } }
}

static void yis_right_view_draw_col_guide(YisVal v_scr) {
#line 155 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_ex = yis_add(yis_right_model_editor_left(), v_PAD_X); yis_retain_val(v_ex);
#line 156 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_guide_x = yis_add(v_ex, yis_mul(v_COL_GUIDE, v_FONT_W)); yis_retain_val(v_guide_x);
#line 157 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_ge(v_guide_x, v_WIN_W))) {
#line 158 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 159 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_fill_halftone(v_scr, v_guide_x, v_CONTENT_Y, YV_INT(1), yis_sub(yis_sub(v_WIN_H, v_CONTENT_Y), v_STATUS_H), v_C_SEL));
}

static void yis_right_view_draw_status(YisVal v_scr) {
#line 169 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_bytes = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_bytes);
#line 170 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_lc = yis_right_model_line_col_from_index(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 171 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_cur_line = yis_add(yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))), YV_INT(1)); yis_retain_val(v_cur_line);
#line 172 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_cur_col = yis_add(yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(1))))))), YV_INT(1)); yis_retain_val(v_cur_col);
#line 173 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_total = yis_right_model_line_count(v_editor_text); yis_retain_val(v_total);
#line 174 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_status = yis_add(yis_add(yis_add(yis_add(yis_add(yis_add(YV_STR(stdr_to_string(v_bytes)), YV_STR(stdr_str_lit(" bytes  Ln "))), YV_STR(stdr_to_string(v_cur_line))), YV_STR(stdr_str_lit("/"))), YV_STR(stdr_to_string(v_total))), YV_STR(stdr_str_lit(" Col "))), YV_STR(stdr_to_string(v_cur_col))); yis_retain_val(v_status);
#line 175 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sw = yis_mul(YV_INT(stdr_len(v_status)), v_FONT_W); yis_retain_val(v_sw);
#line 176 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sx = yis_sub(yis_sub(v_WIN_W, v_sw), YV_INT(8)); yis_retain_val(v_sx);
#line 177 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, YV_INT(0), v_status, v_C_SEL, v_C_BG));
}

static void yis_right_view_draw_editor(YisVal v_scr) {
#line 185 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_code = v_editor_text; yis_retain_val(v_code);
#line 186 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 187 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_max_rows = yis_right_model_visible_lines(); yis_retain_val(v_max_rows);
#line 188 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_scroll = v_scroll_y; yis_retain_val(v_scroll);
#line 189 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_ex = yis_add(yis_right_model_editor_left(), v_PAD_X); yis_retain_val(v_ex);
#line 190 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_max_cols = yis_right_model_editor_cols(); yis_retain_val(v_max_cols);
#line 193 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_pair = yis_right_model_sel_normalized(); yis_retain_val(v_pair);
#line 194 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sa = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(0))))))); yis_retain_val(v_sa);
#line 195 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sb = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(1))))))); yis_retain_val(v_sb);
#line 196 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_has_sel = yis_ne(v_sa, v_sb); yis_retain_val(v_has_sel);
#line 199 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_start = YV_INT(0); yis_retain_val(v_start);
#line 200 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_line_no = YV_INT(0); yis_retain_val(v_line_no);
#line 201 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_le(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 202 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_at_end = yis_ge(v_i, v_total); yis_retain_val(v_at_end);
#line 203 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_is_nl = YV_BOOL(yis_as_bool(YV_BOOL(!yis_as_bool(v_at_end))) && yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))); yis_retain_val(v_is_nl);
#line 204 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(!yis_as_bool(v_at_end))) && yis_as_bool(YV_BOOL(!yis_as_bool(v_is_nl)))))) {
#line 205 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      continue;
    }
#line 207 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_line_no, v_scroll)) && yis_as_bool(yis_lt(v_line_no, yis_add(v_scroll, v_max_rows)))))) {
#line 208 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_row = yis_sub(v_line_no, v_scroll); yis_retain_val(v_row);
#line 209 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_y = yis_add(v_CONTENT_Y, yis_mul(v_row, v_ROW_H)); yis_retain_val(v_y);
#line 210 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_line_start = v_start; yis_retain_val(v_line_start);
#line 211 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_line_end = v_i; yis_retain_val(v_line_end);
#line 212 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_raw_line = stdr_slice(v_code, yis_as_int(v_line_start), yis_as_int(v_line_end)); yis_retain_val(v_raw_line);
#line 213 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_line_text = yis_right_view_clamp_text(v_raw_line, v_max_cols); yis_retain_val(v_line_text);
#line 216 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_fg = yis_right_syntax_line_color(v_raw_line); yis_retain_val(v_fg);
#line 219 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)(yis_m_vimana_screen_put_text(v_scr, v_ex, v_y, v_line_text, v_fg, v_C_BG));
#line 222 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(YV_BOOL(yis_as_bool(v_search_active) && yis_as_bool(yis_gt(YV_INT(stdr_len(v_search_query)), YV_INT(0)))))) {
#line 223 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        YisVal v_qlen = YV_INT(stdr_len(v_search_query)); yis_retain_val(v_qlen);
#line 224 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        YisVal v_match_list = v_search_matches; yis_retain_val(v_match_list);
#line 225 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        YisVal v_mc = YV_INT(stdr_len(v_match_list)); yis_retain_val(v_mc);
#line 226 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        { YisVal v_mi = YV_INT(0); yis_retain_val(v_mi);
        for (; yis_as_bool(yis_lt(v_mi, v_mc)); (void)((yis_move_into(&v_mi, yis_add(v_mi, YV_INT(1))), v_mi))) {
#line 227 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          YisVal v_mpos = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_match_list, v_mi)).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_match_list, v_mi)))))); yis_retain_val(v_mpos);
#line 228 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          if (yis_as_bool(YV_BOOL(yis_as_bool(yis_gt(yis_add(v_mpos, v_qlen), v_line_start)) && yis_as_bool(yis_lt(v_mpos, v_line_end))))) {
#line 229 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
            YisVal v_ml = yis_sub(v_mpos, v_line_start); yis_retain_val(v_ml);
#line 230 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
            YisVal v_mr = yis_add(v_ml, v_qlen); yis_retain_val(v_mr);
#line 231 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
            if (yis_as_bool(yis_lt(v_ml, YV_INT(0)))) {
#line 232 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
              (void)((yis_move_into(&v_ml, YV_INT(0)), v_ml));
            }
#line 233 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
            if (yis_as_bool(yis_gt(v_mr, v_max_cols))) {
#line 234 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
              (void)((yis_move_into(&v_mr, v_max_cols), v_mr));
            }
#line 235 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
            if (yis_as_bool(yis_gt(v_mr, v_ml))) {
#line 236 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
              YisVal v_mt = stdr_slice(v_code, yis_as_int(yis_add(v_line_start, v_ml)), yis_as_int(yis_add(v_line_start, v_mr))); yis_retain_val(v_mt);
#line 237 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
              YisVal v_mx_pos = yis_add(v_ex, yis_mul(v_ml, v_FONT_W)); yis_retain_val(v_mx_pos);
#line 238 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
              (void)(yis_m_vimana_screen_put_text(v_scr, v_mx_pos, v_y, v_mt, v_C_FG, v_C_ACCENT));
            }
          }
        } }
      }
#line 241 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(v_has_sel) && yis_as_bool(yis_lt(v_sa, yis_add(v_line_end, YV_INT(1)))))) && yis_as_bool(yis_gt(v_sb, v_line_start))))) {
#line 242 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        YisVal v_left = v_sa; yis_retain_val(v_left);
#line 243 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        if (yis_as_bool(yis_lt(v_left, v_line_start))) {
#line 244 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          (void)((yis_move_into(&v_left, v_line_start), v_left));
        }
#line 245 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        YisVal v_right = v_sb; yis_retain_val(v_right);
#line 246 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        if (yis_as_bool(yis_gt(v_right, v_line_end))) {
#line 247 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          (void)((yis_move_into(&v_right, v_line_end), v_right));
        }
#line 248 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        if (yis_as_bool(yis_gt(v_right, v_left))) {
#line 249 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          YisVal v_local_left = yis_sub(v_left, v_line_start); yis_retain_val(v_local_left);
#line 250 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          YisVal v_local_right = yis_sub(v_right, v_line_start); yis_retain_val(v_local_right);
#line 251 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          if (yis_as_bool(yis_lt(v_local_left, YV_INT(0)))) {
#line 252 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
            (void)((yis_move_into(&v_local_left, YV_INT(0)), v_local_left));
          }
#line 253 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          if (yis_as_bool(yis_gt(v_local_right, v_max_cols))) {
#line 254 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
            (void)((yis_move_into(&v_local_right, v_max_cols), v_local_right));
          }
#line 255 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          if (yis_as_bool(yis_gt(v_local_right, v_local_left))) {
#line 256 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
            YisVal v_sel_text = stdr_slice(v_code, yis_as_int(v_left), yis_as_int(yis_sub(yis_add(v_left, v_local_right), v_local_left))); yis_retain_val(v_sel_text);
#line 257 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
            YisVal v_sx = yis_add(v_ex, yis_mul(v_local_left, v_FONT_W)); yis_retain_val(v_sx);
#line 258 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
            (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, v_y, v_sel_text, v_C_BG, v_C_SEL));
          }
        }
      }
    }
#line 260 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)((yis_move_into(&v_start, yis_add(v_i, YV_INT(1))), v_start));
#line 261 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)((yis_move_into(&v_line_no, yis_add(v_line_no, YV_INT(1))), v_line_no));
  } }
#line 264 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(v_has_sel)))) {
#line 265 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_lc = yis_right_model_line_col_from_index(v_code, v_caret); yis_retain_val(v_lc);
#line 266 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_caret_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_caret_line);
#line 267 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_caret_col = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(1))))))); yis_retain_val(v_caret_col);
#line 268 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_caret_line, v_scroll)) && yis_as_bool(yis_lt(v_caret_line, yis_add(v_scroll, v_max_rows)))))) {
#line 269 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_cy = yis_add(v_CONTENT_Y, yis_mul(yis_sub(v_caret_line, v_scroll), v_ROW_H)); yis_retain_val(v_cy);
#line 270 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_cx = yis_add(v_ex, yis_mul(v_caret_col, v_FONT_W)); yis_retain_val(v_cx);
#line 271 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)(yis_right_view_fill(v_scr, v_cx, v_cy, YV_INT(2), v_ROW_H, v_C_FG));
    }
  }
}

static void yis_right_view_draw_search(YisVal v_scr) {
#line 279 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(v_search_active)))) {
#line 280 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 282 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sx = yis_right_model_editor_left(); yis_retain_val(v_sx);
#line 283 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sy = v_CONTENT_Y; yis_retain_val(v_sy);
#line 284 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sw = yis_right_model_editor_width(); yis_retain_val(v_sw);
#line 285 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sh = v_ROW_H; yis_retain_val(v_sh);
#line 288 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_fill(v_scr, v_sx, v_sy, v_sw, v_sh, v_C_FG));
#line 291 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_label = yis_add(YV_STR(stdr_str_lit("Find: ")), v_search_query); yis_retain_val(v_label);
#line 292 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_match_count = YV_INT(stdr_len(v_search_matches)); yis_retain_val(v_match_count);
#line 293 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_gt(v_match_count, YV_INT(0)))) {
#line 294 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_idx = yis_add(v_search_index, YV_INT(1)); yis_retain_val(v_idx);
#line 295 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)((yis_move_into(&v_label, yis_add(yis_add(yis_add(yis_add(yis_add(v_label, YV_STR(stdr_str_lit("  ["))), YV_STR(stdr_to_string(v_idx))), YV_STR(stdr_str_lit("/"))), YV_STR(stdr_to_string(v_match_count))), YV_STR(stdr_str_lit("]")))), v_label));
  } else if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_search_query)), YV_INT(0)))) {
#line 297 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)((yis_move_into(&v_label, yis_add(v_label, YV_STR(stdr_str_lit("  [no match]")))), v_label));
  }

#line 299 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, yis_add(v_sx, YV_INT(8)), v_sy, v_label, v_C_BG, v_C_FG));
#line 302 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_qx = yis_add(yis_add(v_sx, YV_INT(8)), yis_mul(yis_add(YV_INT(6), YV_INT(stdr_len(v_search_query))), v_FONT_W)); yis_retain_val(v_qx);
#line 303 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_fill(v_scr, v_qx, yis_add(v_sy, YV_INT(2)), YV_INT(2), yis_sub(v_ROW_H, YV_INT(4)), v_C_ACCENT));
}

static void yis_right_view_draw_all(YisVal v_scr) {
#line 311 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_clear(v_scr, v_C_BG));
#line 312 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_draw_titlebar(v_scr, v_C_BG));
#line 313 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_scrollbar(v_scr));
#line 314 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_separator(v_scr));
#line 315 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_outline(v_scr));
#line 316 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_col_guide(v_scr));
#line 317 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_editor(v_scr));
#line 318 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_status(v_scr));
#line 319 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_search(v_scr));
#line 320 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_menu_draw_menubar(v_scr));
#line 321 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_menu_draw_submenu(v_scr));
#line 322 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_present(v_scr));
}

static YisVal __fnwrap_right_view_fill(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  YisVal __a4 = argc > 4 ? argv[4] : YV_NULLV;
  YisVal __a5 = argc > 5 ? argv[5] : YV_NULLV;
  yis_right_view_fill(__a0, __a1, __a2, __a3, __a4, __a5);
  return YV_NULLV;
}

static YisVal __fnwrap_right_view_clamp_text(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_right_view_clamp_text(__a0, __a1);
}

static YisVal __fnwrap_right_view_fill_halftone(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  YisVal __a4 = argc > 4 ? argv[4] : YV_NULLV;
  YisVal __a5 = argc > 5 ? argv[5] : YV_NULLV;
  yis_right_view_fill_halftone(__a0, __a1, __a2, __a3, __a4, __a5);
  return YV_NULLV;
}

static YisVal __fnwrap_right_view_draw_scrollbar(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_view_draw_scrollbar(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_view_draw_separator(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_view_draw_separator(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_view_type_prefix(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_view_type_prefix(__a0);
}

static YisVal __fnwrap_right_view_type_color(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_view_type_color(__a0);
}

static YisVal __fnwrap_right_view_draw_outline(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_view_draw_outline(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_view_draw_col_guide(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_view_draw_col_guide(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_view_draw_status(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_view_draw_status(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_view_draw_editor(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_view_draw_editor(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_view_draw_search(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_view_draw_search(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_view_draw_all(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_view_draw_all(__a0);
  return YV_NULLV;
}

/* end embedded module: right_view */

/* begin embedded module: right_assets */
static void yis_right_assets_setup(YisVal);
static YisVal __fnwrap_right_assets_setup(void*,int,YisVal*);

// cask right_assets
// bring vimana
static void yis_right_assets_setup(YisVal v_scr) {
#line 9 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_size(v_scr, YV_INT(3)));
#line 11 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(32), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 12 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(33), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 13 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(34), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 14 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(35), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(68), YV_INT(0), YV_INT(0), YV_INT(68), YV_INT(0), YV_INT(0), YV_INT(68), YV_INT(0), YV_INT(0), YV_INT(68), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(7), YV_INT(254), YV_INT(0), YV_INT(7), YV_INT(254), YV_INT(0), YV_INT(1), YV_INT(16), YV_INT(0), YV_INT(1), YV_INT(16), YV_INT(0), YV_INT(1), YV_INT(16), YV_INT(0), YV_INT(1), YV_INT(16), YV_INT(0), YV_INT(1), YV_INT(16), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 15 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(36), yis_arr_lit(72, YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(1), YV_INT(172), YV_INT(0), YV_INT(3), YV_INT(38), YV_INT(0), YV_INT(3), YV_INT(38), YV_INT(0), YV_INT(3), YV_INT(38), YV_INT(0), YV_INT(3), YV_INT(32), YV_INT(0), YV_INT(1), YV_INT(160), YV_INT(0), YV_INT(1), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(62), YV_INT(0), YV_INT(0), YV_INT(38), YV_INT(0), YV_INT(0), YV_INT(35), YV_INT(0), YV_INT(3), YV_INT(35), YV_INT(0), YV_INT(3), YV_INT(35), YV_INT(0), YV_INT(3), YV_INT(35), YV_INT(0), YV_INT(3), YV_INT(35), YV_INT(0), YV_INT(1), YV_INT(166), YV_INT(0), YV_INT(0), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 16 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(37), yis_arr_lit(72, YV_INT(0), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(194), YV_INT(0), YV_INT(6), YV_INT(102), YV_INT(0), YV_INT(6), YV_INT(100), YV_INT(0), YV_INT(6), YV_INT(108), YV_INT(0), YV_INT(6), YV_INT(104), YV_INT(0), YV_INT(6), YV_INT(104), YV_INT(0), YV_INT(6), YV_INT(88), YV_INT(0), YV_INT(3), YV_INT(144), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(78), YV_INT(0), YV_INT(0), YV_INT(83), YV_INT(0), YV_INT(0), YV_INT(243), YV_INT(0), YV_INT(0), YV_INT(179), YV_INT(0), YV_INT(1), YV_INT(179), YV_INT(0), YV_INT(1), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(2), YV_INT(30), YV_INT(0), YV_INT(2), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 17 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(38), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(240), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(144), YV_INT(0), YV_INT(0), YV_INT(176), YV_INT(0), YV_INT(0), YV_INT(224), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(3), YV_INT(68), YV_INT(0), YV_INT(3), YV_INT(68), YV_INT(0), YV_INT(6), YV_INT(100), YV_INT(0), YV_INT(6), YV_INT(44), YV_INT(0), YV_INT(6), YV_INT(56), YV_INT(0), YV_INT(6), YV_INT(56), YV_INT(0), YV_INT(6), YV_INT(25), YV_INT(0), YV_INT(3), YV_INT(63), YV_INT(0), YV_INT(1), YV_INT(230), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 18 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(39), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 19 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(40), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 20 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(41), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 21 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(42), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(2), YV_INT(114), YV_INT(0), YV_INT(7), YV_INT(39), YV_INT(0), YV_INT(3), YV_INT(174), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(174), YV_INT(0), YV_INT(7), YV_INT(39), YV_INT(0), YV_INT(2), YV_INT(114), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 22 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(43), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 23 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(44), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(56), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 24 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(45), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 25 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(46), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 26 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(47), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 27 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(48), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 28 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(49), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(16), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(1), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 29 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(50), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(16), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(64), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(130), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(252), YV_INT(0), YV_INT(3), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 30 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(51), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 31 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(52), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(28), YV_INT(0), YV_INT(0), YV_INT(28), YV_INT(0), YV_INT(0), YV_INT(60), YV_INT(0), YV_INT(0), YV_INT(44), YV_INT(0), YV_INT(0), YV_INT(108), YV_INT(0), YV_INT(0), YV_INT(76), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(0), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(2), YV_INT(12), YV_INT(0), YV_INT(7), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(127), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 32 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(53), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(255), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(1), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(120), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(1), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(131), YV_INT(0), YV_INT(3), YV_INT(131), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(142), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 33 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(54), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(124), YV_INT(0), YV_INT(0), YV_INT(198), YV_INT(0), YV_INT(1), YV_INT(135), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(124), YV_INT(0), YV_INT(3), YV_INT(198), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 34 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(55), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(255), YV_INT(0), YV_INT(1), YV_INT(255), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(4), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(8), YV_INT(0), YV_INT(0), YV_INT(8), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 35 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(56), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(0), YV_INT(196), YV_INT(0), YV_INT(0), YV_INT(236), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(220), YV_INT(0), YV_INT(1), YV_INT(142), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 36 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(57), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(7), YV_INT(0), YV_INT(1), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(251), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(1), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(131), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 37 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(58), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 38 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(59), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(56), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 39 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(60), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 40 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(61), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 41 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(62), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 42 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(63), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(252), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(131), YV_INT(0), YV_INT(3), YV_INT(135), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(16), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 43 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(64), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(1), YV_INT(0), YV_INT(6), YV_INT(121), YV_INT(0), YV_INT(6), YV_INT(205), YV_INT(0), YV_INT(6), YV_INT(205), YV_INT(0), YV_INT(6), YV_INT(205), YV_INT(0), YV_INT(6), YV_INT(205), YV_INT(0), YV_INT(6), YV_INT(205), YV_INT(0), YV_INT(6), YV_INT(205), YV_INT(0), YV_INT(6), YV_INT(223), YV_INT(0), YV_INT(6), YV_INT(118), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(1), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(0), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 44 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(65), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(88), YV_INT(0), YV_INT(0), YV_INT(88), YV_INT(0), YV_INT(0), YV_INT(88), YV_INT(0), YV_INT(0), YV_INT(216), YV_INT(0), YV_INT(0), YV_INT(152), YV_INT(0), YV_INT(0), YV_INT(140), YV_INT(0), YV_INT(0), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(252), YV_INT(0), YV_INT(1), YV_INT(12), YV_INT(0), YV_INT(1), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 45 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(66), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(7), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 46 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(67), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(122), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(6), YV_INT(2), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(1), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 47 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(68), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(240), YV_INT(0), YV_INT(3), YV_INT(28), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(28), YV_INT(0), YV_INT(7), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 48 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(69), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(255), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(252), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(7), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 49 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(70), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(255), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(252), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(7), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 50 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(71), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(122), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(6), YV_INT(2), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(31), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 51 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(72), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 52 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(73), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 53 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(74), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(4), YV_INT(48), YV_INT(0), YV_INT(14), YV_INT(48), YV_INT(0), YV_INT(14), YV_INT(48), YV_INT(0), YV_INT(6), YV_INT(96), YV_INT(0), YV_INT(3), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 54 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(75), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(159), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(4), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(16), YV_INT(0), YV_INT(3), YV_INT(48), YV_INT(0), YV_INT(3), YV_INT(96), YV_INT(0), YV_INT(3), YV_INT(96), YV_INT(0), YV_INT(3), YV_INT(224), YV_INT(0), YV_INT(3), YV_INT(176), YV_INT(0), YV_INT(3), YV_INT(48), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 55 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(76), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(224), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(7), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 56 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(77), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(7), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(142), YV_INT(0), YV_INT(3), YV_INT(142), YV_INT(0), YV_INT(3), YV_INT(142), YV_INT(0), YV_INT(3), YV_INT(142), YV_INT(0), YV_INT(2), YV_INT(214), YV_INT(0), YV_INT(2), YV_INT(214), YV_INT(0), YV_INT(2), YV_INT(214), YV_INT(0), YV_INT(2), YV_INT(214), YV_INT(0), YV_INT(2), YV_INT(118), YV_INT(0), YV_INT(2), YV_INT(118), YV_INT(0), YV_INT(2), YV_INT(102), YV_INT(0), YV_INT(2), YV_INT(102), YV_INT(0), YV_INT(2), YV_INT(38), YV_INT(0), YV_INT(2), YV_INT(38), YV_INT(0), YV_INT(2), YV_INT(38), YV_INT(0), YV_INT(7), YV_INT(47), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 57 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(78), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(7), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(130), YV_INT(0), YV_INT(3), YV_INT(130), YV_INT(0), YV_INT(2), YV_INT(194), YV_INT(0), YV_INT(2), YV_INT(194), YV_INT(0), YV_INT(2), YV_INT(98), YV_INT(0), YV_INT(2), YV_INT(98), YV_INT(0), YV_INT(2), YV_INT(98), YV_INT(0), YV_INT(2), YV_INT(50), YV_INT(0), YV_INT(2), YV_INT(50), YV_INT(0), YV_INT(2), YV_INT(26), YV_INT(0), YV_INT(2), YV_INT(26), YV_INT(0), YV_INT(2), YV_INT(14), YV_INT(0), YV_INT(2), YV_INT(14), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(2), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 58 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(79), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 59 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(80), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(128), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 60 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(81), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(243), YV_INT(0), YV_INT(7), YV_INT(155), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(0), YV_INT(253), YV_INT(0), YV_INT(0), YV_INT(15), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 61 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(82), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(48), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(135), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 62 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(83), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(250), YV_INT(0), YV_INT(1), YV_INT(142), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(60), YV_INT(0), YV_INT(0), YV_INT(14), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(2), YV_INT(3), YV_INT(0), YV_INT(2), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(131), YV_INT(0), YV_INT(2), YV_INT(198), YV_INT(0), YV_INT(2), YV_INT(124), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 63 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(84), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(2), YV_INT(49), YV_INT(0), YV_INT(2), YV_INT(49), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 64 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(85), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(135), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(204), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 65 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(86), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(135), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(216), YV_INT(0), YV_INT(0), YV_INT(80), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 66 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(87), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(119), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(118), YV_INT(0), YV_INT(1), YV_INT(84), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 67 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(88), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(4), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(216), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(80), YV_INT(0), YV_INT(0), YV_INT(216), YV_INT(0), YV_INT(0), YV_INT(152), YV_INT(0), YV_INT(0), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 68 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(89), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(135), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(0), YV_INT(196), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(104), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 69 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(90), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(3), YV_INT(1), YV_INT(0), YV_INT(3), YV_INT(1), YV_INT(0), YV_INT(7), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 70 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(91), yis_arr_lit(72, YV_INT(0), YV_INT(62), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(62), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 71 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(92), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 72 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(93), yis_arr_lit(72, YV_INT(3), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(1), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 73 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(94), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 74 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(95), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 75 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(96), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 76 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(97), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(240), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(1), YV_INT(252), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(6), YV_INT(12), YV_INT(0), YV_INT(6), YV_INT(12), YV_INT(0), YV_INT(6), YV_INT(13), YV_INT(0), YV_INT(3), YV_INT(31), YV_INT(0), YV_INT(1), YV_INT(246), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 77 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(98), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(120), YV_INT(0), YV_INT(3), YV_INT(204), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(206), YV_INT(0), YV_INT(2), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 78 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(99), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(124), YV_INT(0), YV_INT(1), YV_INT(198), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(1), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(1), YV_INT(198), YV_INT(0), YV_INT(0), YV_INT(124), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 79 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(100), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(30), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(246), YV_INT(0), YV_INT(1), YV_INT(158), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(158), YV_INT(0), YV_INT(0), YV_INT(247), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 80 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(101), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(1), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(1), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(1), YV_INT(198), YV_INT(0), YV_INT(0), YV_INT(124), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 81 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(102), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(30), YV_INT(0), YV_INT(0), YV_INT(51), YV_INT(0), YV_INT(0), YV_INT(35), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(7), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(3), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 82 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(103), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(243), YV_INT(0), YV_INT(1), YV_INT(159), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(3), YV_INT(240), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(240), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(7), YV_INT(15), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 83 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(104), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(120), YV_INT(0), YV_INT(3), YV_INT(204), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 84 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(105), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 85 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(106), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(4), YV_INT(48), YV_INT(0), YV_INT(14), YV_INT(48), YV_INT(0), YV_INT(14), YV_INT(48), YV_INT(0), YV_INT(6), YV_INT(96), YV_INT(0), YV_INT(3), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 86 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(107), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(30), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(48), YV_INT(0), YV_INT(3), YV_INT(112), YV_INT(0), YV_INT(3), YV_INT(240), YV_INT(0), YV_INT(3), YV_INT(152), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 87 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(108), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 88 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(109), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(118), YV_INT(0), YV_INT(3), YV_INT(187), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(7), YV_INT(119), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 89 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(110), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(120), YV_INT(0), YV_INT(3), YV_INT(204), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 90 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(111), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 91 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(112), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(120), YV_INT(0), YV_INT(3), YV_INT(206), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(206), YV_INT(0), YV_INT(3), YV_INT(120), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(128), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 92 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(113), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(242), YV_INT(0), YV_INT(3), YV_INT(158), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(158), YV_INT(0), YV_INT(0), YV_INT(246), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(15), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 93 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(114), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(206), YV_INT(0), YV_INT(0), YV_INT(219), YV_INT(0), YV_INT(0), YV_INT(227), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(3), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 94 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(115), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(250), YV_INT(0), YV_INT(1), YV_INT(142), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(126), YV_INT(0), YV_INT(0), YV_INT(15), YV_INT(0), YV_INT(2), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(2), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 95 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(116), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(224), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(97), YV_INT(0), YV_INT(0), YV_INT(51), YV_INT(0), YV_INT(0), YV_INT(30), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 96 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(117), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(158), YV_INT(0), YV_INT(0), YV_INT(247), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 97 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(118), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(199), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(0), YV_INT(196), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(104), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 98 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(119), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(119), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(118), YV_INT(0), YV_INT(1), YV_INT(84), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 99 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(120), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(207), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(216), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(216), YV_INT(0), YV_INT(0), YV_INT(156), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 100 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(121), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(135), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(104), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(6), YV_INT(32), YV_INT(0), YV_INT(6), YV_INT(96), YV_INT(0), YV_INT(3), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 101 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(122), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(12), YV_INT(0), YV_INT(2), YV_INT(28), YV_INT(0), YV_INT(0), YV_INT(56), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(224), YV_INT(0), YV_INT(1), YV_INT(194), YV_INT(0), YV_INT(1), YV_INT(130), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 102 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(123), yis_arr_lit(72, YV_INT(0), YV_INT(7), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 103 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(124), yis_arr_lit(72, YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 104 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(125), yis_arr_lit(72, YV_INT(14), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(14), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 105 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(126), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(128), YV_INT(0), YV_INT(4), YV_INT(192), YV_INT(0), YV_INT(8), YV_INT(97), YV_INT(0), YV_INT(0), YV_INT(50), YV_INT(0), YV_INT(0), YV_INT(28), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 106 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(127), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(7), YV_INT(3), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(133), YV_INT(128), YV_INT(6), YV_INT(73), YV_INT(128), YV_INT(6), YV_INT(49), YV_INT(128), YV_INT(6), YV_INT(49), YV_INT(128), YV_INT(6), YV_INT(73), YV_INT(128), YV_INT(6), YV_INT(133), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(7), YV_INT(3), YV_INT(128), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 109 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(32), YV_INT(16)));
#line 110 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(33), YV_INT(16)));
#line 111 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(34), YV_INT(16)));
#line 112 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(35), YV_INT(16)));
#line 113 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(36), YV_INT(16)));
#line 114 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(37), YV_INT(16)));
#line 115 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(38), YV_INT(16)));
#line 116 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(39), YV_INT(16)));
#line 117 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(40), YV_INT(16)));
#line 118 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(41), YV_INT(16)));
#line 119 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(42), YV_INT(16)));
#line 120 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(43), YV_INT(16)));
#line 121 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(44), YV_INT(16)));
#line 122 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(45), YV_INT(16)));
#line 123 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(46), YV_INT(16)));
#line 124 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(47), YV_INT(16)));
#line 125 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(48), YV_INT(16)));
#line 126 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(49), YV_INT(16)));
#line 127 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(50), YV_INT(16)));
#line 128 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(51), YV_INT(16)));
#line 129 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(52), YV_INT(16)));
#line 130 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(53), YV_INT(16)));
#line 131 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(54), YV_INT(16)));
#line 132 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(55), YV_INT(16)));
#line 133 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(56), YV_INT(16)));
#line 134 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(57), YV_INT(16)));
#line 135 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(58), YV_INT(16)));
#line 136 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(59), YV_INT(16)));
#line 137 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(60), YV_INT(16)));
#line 138 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(61), YV_INT(16)));
#line 139 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(62), YV_INT(16)));
#line 140 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(63), YV_INT(16)));
#line 141 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(64), YV_INT(16)));
#line 142 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(65), YV_INT(16)));
#line 143 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(66), YV_INT(16)));
#line 144 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(67), YV_INT(16)));
#line 145 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(68), YV_INT(16)));
#line 146 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(69), YV_INT(16)));
#line 147 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(70), YV_INT(16)));
#line 148 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(71), YV_INT(16)));
#line 149 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(72), YV_INT(16)));
#line 150 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(73), YV_INT(16)));
#line 151 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(74), YV_INT(16)));
#line 152 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(75), YV_INT(16)));
#line 153 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(76), YV_INT(16)));
#line 154 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(77), YV_INT(16)));
#line 155 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(78), YV_INT(16)));
#line 156 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(79), YV_INT(16)));
#line 157 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(80), YV_INT(16)));
#line 158 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(81), YV_INT(16)));
#line 159 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(82), YV_INT(16)));
#line 160 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(83), YV_INT(16)));
#line 161 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(84), YV_INT(16)));
#line 162 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(85), YV_INT(16)));
#line 163 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(86), YV_INT(16)));
#line 164 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(87), YV_INT(16)));
#line 165 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(88), YV_INT(16)));
#line 166 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(89), YV_INT(16)));
#line 167 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(90), YV_INT(16)));
#line 168 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(91), YV_INT(16)));
#line 169 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(92), YV_INT(16)));
#line 170 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(93), YV_INT(16)));
#line 171 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(94), YV_INT(16)));
#line 172 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(95), YV_INT(16)));
#line 173 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(96), YV_INT(16)));
#line 174 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(97), YV_INT(16)));
#line 175 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(98), YV_INT(16)));
#line 176 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(99), YV_INT(16)));
#line 177 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(100), YV_INT(16)));
#line 178 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(101), YV_INT(16)));
#line 179 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(102), YV_INT(16)));
#line 180 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(103), YV_INT(16)));
#line 181 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(104), YV_INT(16)));
#line 182 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(105), YV_INT(16)));
#line 183 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(106), YV_INT(16)));
#line 184 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(107), YV_INT(16)));
#line 185 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(108), YV_INT(16)));
#line 186 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(109), YV_INT(16)));
#line 187 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(110), YV_INT(16)));
#line 188 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(111), YV_INT(16)));
#line 189 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(112), YV_INT(16)));
#line 190 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(113), YV_INT(16)));
#line 191 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(114), YV_INT(16)));
#line 192 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(115), YV_INT(16)));
#line 193 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(116), YV_INT(16)));
#line 194 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(117), YV_INT(16)));
#line 195 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(118), YV_INT(16)));
#line 196 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(119), YV_INT(16)));
#line 197 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(120), YV_INT(16)));
#line 198 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(121), YV_INT(16)));
#line 199 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(122), YV_INT(16)));
#line 200 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(123), YV_INT(16)));
#line 201 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(124), YV_INT(16)));
#line 202 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(125), YV_INT(16)));
#line 203 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(126), YV_INT(16)));
#line 204 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(127), YV_INT(16)));
}

static YisVal __fnwrap_right_assets_setup(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_assets_setup(__a0);
  return YV_NULLV;
}

/* end embedded module: right_assets */

/* begin main unit */
static YisVal yis_main_in_rect(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static void yis_main_update_title(YisVal);
static void yis_main_load_file(YisVal, YisVal);
static void yis_main_save_file(YisVal, YisVal);
static void yis_main_new_file(YisVal);
static void yis_main_open_file(YisVal, YisVal);
static void yis_main_exec_action(YisVal, YisVal, YisVal, YisVal);
static void yis_main_on_mouse(YisVal, YisVal, YisVal, YisVal);
static void yis_main_on_key(YisVal, YisVal, YisVal, YisVal);
static void yis_main_on_text(YisVal, YisVal);
static void yis_main_on_scroll(YisVal);
static void yis_entry(void);
static YisVal __fnwrap_main_in_rect(void*,int,YisVal*);
static YisVal __fnwrap_main_update_title(void*,int,YisVal*);
static YisVal __fnwrap_main_load_file(void*,int,YisVal*);
static YisVal __fnwrap_main_save_file(void*,int,YisVal*);
static YisVal __fnwrap_main_new_file(void*,int,YisVal*);
static YisVal __fnwrap_main_open_file(void*,int,YisVal*);
static YisVal __fnwrap_main_exec_action(void*,int,YisVal*);
static YisVal __fnwrap_main_on_mouse(void*,int,YisVal*);
static YisVal __fnwrap_main_on_key(void*,int,YisVal*);
static YisVal __fnwrap_main_on_text(void*,int,YisVal*);
static YisVal __fnwrap_main_on_scroll(void*,int,YisVal*);
static YisVal v_last_click_time = YV_NULLV;
static YisVal v_last_click_x = YV_NULLV;
static YisVal v_last_click_y = YV_NULLV;
static YisVal v_DBLCLICK_MS = YV_NULLV;
static YisVal v_DBLCLICK_DIST = YV_NULLV;

// cask main
// bring stdr
// bring vimana
// bring right_model
// bring right_view
// bring right_assets
// bring right_menu
static YisVal yis_main_in_rect(YisVal v_sx, YisVal v_sy, YisVal v_rx, YisVal v_ry, YisVal v_rw, YisVal v_rh) {
  return YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_sx, v_rx)) && yis_as_bool(yis_lt(v_sx, yis_add(v_rx, v_rw))))) && yis_as_bool(yis_ge(v_sy, v_ry)))) && yis_as_bool(yis_lt(v_sy, yis_add(v_ry, v_rh))));
}

static void yis_main_update_title(YisVal v_scr) {
  YisVal v_title = YV_STR(stdr_str_lit("Right")); yis_retain_val(v_title);
  if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_file_path)), YV_INT(0)))) {
    (void)((yis_move_into(&v_title, yis_add(YV_STR(stdr_str_lit("Right - ")), v_file_path)), v_title));
  }
  if (yis_as_bool(v_dirty)) {
    (void)((yis_move_into(&v_title, yis_add(v_title, YV_STR(stdr_str_lit(" *")))), v_title));
  }
  (void)(yis_m_vimana_screen_set_titlebar_title(v_scr, v_title));
}

static void yis_main_load_file(YisVal v_fsys, YisVal v_scr) {
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_file_path)), YV_INT(0)))) {
    return;
  }
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_m_vimana_file_exists(v_fsys, v_file_path))))) {
    return;
  }
  YisVal v_txt = yis_m_vimana_file_read_text(v_fsys, v_file_path); yis_retain_val(v_txt);
  (void)(yis_right_model_set_text(v_txt));
  (void)(yis_right_model_set_caret(YV_INT(0)));
  (void)(yis_right_model_set_selection(YV_INT(0), YV_INT(0)));
  (void)(yis_right_model_set_scroll(YV_INT(0)));
  (void)(yis_right_model_rebuild_outline());
  (void)(yis_right_model_set_dirty(YV_BOOL(false)));
  (void)(yis_main_update_title(v_scr));
}

static void yis_main_save_file(YisVal v_fsys, YisVal v_scr) {
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_file_path)), YV_INT(0)))) {
    return;
  }
  (void)(yis_m_vimana_file_write_text(v_fsys, v_file_path, v_editor_text));
  (void)(yis_right_model_set_dirty(YV_BOOL(false)));
  (void)(yis_main_update_title(v_scr));
}

static void yis_main_new_file(YisVal v_scr) {
  (void)(yis_right_model_set_file_path(YV_STR(stdr_str_lit(""))));
  (void)(yis_right_model_set_text(YV_STR(stdr_str_lit("cask main\n\nbring stdr\n\n-- entry point\n-> ()\n    -- your code here\n;\n"))));
  (void)(yis_right_model_set_caret(YV_INT(0)));
  (void)(yis_right_model_set_selection(YV_INT(0), YV_INT(0)));
  (void)(yis_right_model_set_scroll(YV_INT(0)));
  (void)(yis_right_model_rebuild_outline());
  (void)(yis_right_model_set_dirty(YV_BOOL(false)));
  (void)(yis_main_update_title(v_scr));
}

static void yis_main_open_file(YisVal v_fsys, YisVal v_scr) {
  YisVal v_picked = yis_stdr_open_file_dialog(YV_STR(stdr_str_lit("Open file")), YV_STR(stdr_str_lit("yi"))); yis_retain_val(v_picked);
  if (yis_as_bool(YV_BOOL(stdr_is_null(v_picked)))) {
    return;
  }
  YisVal v_path = YV_STR(stdr_to_string(v_picked)); yis_retain_val(v_path);
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_path)), YV_INT(0)))) {
    return;
  }
  (void)(yis_right_model_set_file_path(v_path));
  (void)(yis_main_load_file(v_fsys, v_scr));
}

static void yis_main_exec_action(YisVal v_action, YisVal v_sys, YisVal v_fsys, YisVal v_scr) {
  if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("new"))))) {
    (void)(yis_main_new_file(v_scr));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("open"))))) {
    (void)(yis_main_open_file(v_fsys, v_scr));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("save"))))) {
    (void)(yis_main_save_file(v_fsys, v_scr));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("copy"))))) {
    YisVal v_sel = yis_right_model_selected_text(); yis_retain_val(v_sel);
    if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_sel)), YV_INT(0)))) {
      (void)(yis_m_vimana_system_set_clipboard_text(v_sys, v_sel));
    }
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("paste"))))) {
    YisVal v_clip = yis_m_vimana_system_clipboard_text(v_sys); yis_retain_val(v_clip);
    if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_clip)), YV_INT(0)))) {
      (void)(yis_right_model_insert_at_caret(v_clip));
      (void)(yis_main_update_title(v_scr));
    }
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("cut"))))) {
    YisVal v_sel = yis_right_model_selected_text(); yis_retain_val(v_sel);
    if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_sel)), YV_INT(0)))) {
      (void)(yis_m_vimana_system_set_clipboard_text(v_sys, v_sel));
      (void)(yis_right_model_delete_selection());
      (void)(yis_main_update_title(v_scr));
    }
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("select_all"))))) {
    (void)(yis_right_model_select_all());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("toggle_sidebar"))))) {
    (void)(yis_right_model_toggle_sidebar());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("find"))))) {
    (void)(yis_right_model_open_search());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("find_next"))))) {
    (void)(yis_right_model_search_next());
  }









}

static void yis_main_on_mouse(YisVal v_dev, YisVal v_sys, YisVal v_fsys, YisVal v_scr) {
  YisVal v_mx = yis_m_vimana_device_pointer_x(v_dev); yis_retain_val(v_mx);
  YisVal v_my = yis_m_vimana_device_pointer_y(v_dev); yis_retain_val(v_my);
  (void)(yis_right_menu_handle_hover(v_mx, v_my));
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_m_vimana_device_mouse_pressed(v_dev, v_mouse_left))))) {
    return;
  }
  YisVal v_action = yis_right_menu_handle_click(v_mx, v_my); yis_retain_val(v_action);
  if (yis_as_bool(yis_ne(v_action, YV_STR(stdr_str_lit("pass"))))) {
    if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_action)), YV_INT(0)))) {
      (void)(yis_main_exec_action(v_action, v_sys, v_fsys, v_scr));
    }
    return;
  }
  if (yis_as_bool(yis_lt(v_mx, v_SIDEBAR_X))) {
    YisVal v_bar_h = yis_sub(yis_sub(v_WIN_H, v_CONTENT_Y), v_STATUS_H); yis_retain_val(v_bar_h);
    YisVal v_total = yis_right_model_line_count(v_editor_text); yis_retain_val(v_total);
    YisVal v_ratio = yis_div(yis_sub(v_my, v_CONTENT_Y), v_bar_h); yis_retain_val(v_ratio);
    YisVal v_target = yis_stdr_floor(yis_mul(v_ratio, v_total)); yis_retain_val(v_target);
    (void)(yis_right_model_set_scroll(v_target));
    (void)(yis_right_model_clamp_scroll());
    return;
  }
  YisVal v_ed_left = yis_right_model_editor_left(); yis_retain_val(v_ed_left);
  if (yis_as_bool(yis_ge(v_mx, v_ed_left))) {
    YisVal v_now = yis_m_vimana_system_ticks(v_sys); yis_retain_val(v_now);
    YisVal v_dx = yis_sub(v_mx, v_last_click_x); yis_retain_val(v_dx);
    YisVal v_dy = yis_sub(v_my, v_last_click_y); yis_retain_val(v_dy);
    YisVal v_dist = yis_add(yis_mul(v_dx, v_dx), yis_mul(v_dy, v_dy)); yis_retain_val(v_dist);
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_lt(yis_sub(v_now, v_last_click_time), v_DBLCLICK_MS)) && yis_as_bool(yis_lt(v_dist, yis_mul(v_DBLCLICK_DIST, v_DBLCLICK_DIST)))))) {
      YisVal v_pos = yis_right_model_caret_from_editor_click(v_mx, v_my); yis_retain_val(v_pos);
      (void)(yis_right_model_select_word_at(v_pos));
      (void)((yis_move_into(&v_last_click_time, YV_INT(0)), v_last_click_time));
      return;
    }
    (void)((yis_move_into(&v_last_click_time, v_now), v_last_click_time));
    (void)((yis_move_into(&v_last_click_x, v_mx), v_last_click_x));
    (void)((yis_move_into(&v_last_click_y, v_my), v_last_click_y));
    YisVal v_pos = yis_right_model_caret_from_editor_click(v_mx, v_my); yis_retain_val(v_pos);
    (void)(yis_right_model_set_caret(v_pos));
    (void)(yis_right_model_set_selection(v_pos, v_pos));
    return;
  }
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(v_sidebar_visible) && yis_as_bool(yis_ge(v_mx, v_SIDEBAR_X)))) && yis_as_bool(yis_lt(v_mx, yis_add(v_SIDEBAR_X, v_SIDEBAR_W)))))) {
    YisVal v_idx = yis_right_model_outline_index_from_click(v_my); yis_retain_val(v_idx);
    if (yis_as_bool(yis_ge(v_idx, YV_INT(0)))) {
      (void)(yis_right_model_set_outline_selected(v_idx));
      YisVal v_item = yis_index(v_current_outline, v_idx); yis_retain_val(v_item);
      YisVal v_line_any = ((yis_index(v_item, YV_STR(stdr_str_lit("line")))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_item, YV_STR(stdr_str_lit("line"))))); yis_retain_val(v_line_any);
      YisVal v_pos = yis_right_model_line_to_char_pos(v_editor_text, v_line_any); yis_retain_val(v_pos);
      (void)(yis_right_model_set_caret(v_pos));
      (void)(yis_right_model_set_selection(v_pos, v_pos));
      (void)(yis_right_model_ensure_visible());
    }
  }
}

static void yis_main_on_key(YisVal v_dev, YisVal v_sys, YisVal v_fsys, YisVal v_scr) {
  YisVal v_cmd = YV_BOOL(yis_as_bool(yis_m_vimana_device_key_down(v_dev, v_KEY_LGUI)) || yis_as_bool(yis_m_vimana_device_key_down(v_dev, v_KEY_RGUI))); yis_retain_val(v_cmd);
  YisVal v_shift = YV_BOOL(yis_as_bool(yis_m_vimana_device_key_down(v_dev, YV_INT(225))) || yis_as_bool(yis_m_vimana_device_key_down(v_dev, YV_INT(229)))); yis_retain_val(v_shift);
  if (yis_as_bool(v_search_active)) {
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_ESCAPE))) {
      (void)(yis_right_model_close_search());
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_RETURN))) {
      (void)(yis_right_model_search_next());
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_BACKSPACE))) {
      (void)(yis_right_model_search_backspace());
      return;
    }
    if (yis_as_bool(YV_BOOL(yis_as_bool(v_cmd) && yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_F))))) {
      (void)(yis_right_model_close_search());
      return;
    }
    return;
  }
  if (yis_as_bool(v_cmd)) {
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_S))) {
      (void)(yis_main_save_file(v_fsys, v_scr));
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_N))) {
      (void)(yis_main_new_file(v_scr));
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_O))) {
      (void)(yis_main_open_file(v_fsys, v_scr));
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_F))) {
      (void)(yis_right_model_open_search());
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_G))) {
      (void)(yis_right_model_search_next());
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_A))) {
      (void)(yis_right_model_select_all());
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_C))) {
      YisVal v_sel = yis_right_model_selected_text(); yis_retain_val(v_sel);
      if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_sel)), YV_INT(0)))) {
        (void)(yis_m_vimana_system_set_clipboard_text(v_sys, v_sel));
      }
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_X))) {
      YisVal v_sel = yis_right_model_selected_text(); yis_retain_val(v_sel);
      if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_sel)), YV_INT(0)))) {
        (void)(yis_m_vimana_system_set_clipboard_text(v_sys, v_sel));
        (void)(yis_right_model_delete_selection());
        (void)(yis_main_update_title(v_scr));
      }
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_V))) {
      YisVal v_clip = yis_m_vimana_system_clipboard_text(v_sys); yis_retain_val(v_clip);
      if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_clip)), YV_INT(0)))) {
        (void)(yis_right_model_insert_at_caret(v_clip));
        (void)(yis_main_update_title(v_scr));
      }
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_D))) {
      (void)(yis_right_model_toggle_sidebar());
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_LEFT))) {
      (void)(yis_right_model_move_caret_line_start(v_shift));
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_RIGHT))) {
      (void)(yis_right_model_move_caret_line_end(v_shift));
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_UP))) {
      (void)(yis_right_model_move_caret_doc_start(v_shift));
      return;
    }
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_DOWN))) {
      (void)(yis_right_model_move_caret_doc_end(v_shift));
      return;
    }
    return;
  }
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_LEFT))) {
    (void)(yis_right_model_move_caret_left(v_shift));
    return;
  }
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_RIGHT))) {
    (void)(yis_right_model_move_caret_right(v_shift));
    return;
  }
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_UP))) {
    (void)(yis_right_model_move_caret_up(v_shift));
    return;
  }
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_DOWN))) {
    (void)(yis_right_model_move_caret_down(v_shift));
    return;
  }
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_BACKSPACE))) {
    (void)(yis_right_model_delete_backward());
    (void)(yis_main_update_title(v_scr));
    return;
  }
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_DELETE))) {
    (void)(yis_right_model_delete_forward());
    (void)(yis_main_update_title(v_scr));
    return;
  }
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_RETURN))) {
    (void)(yis_right_model_insert_at_caret(YV_STR(stdr_str_lit("\n"))));
    (void)(yis_main_update_title(v_scr));
    return;
  }
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_TAB))) {
    (void)(yis_right_model_insert_at_caret(YV_STR(stdr_str_lit("    "))));
    (void)(yis_main_update_title(v_scr));
    return;
  }
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_ESCAPE))) {
    if (yis_as_bool(yis_right_menu_is_open())) {
      (void)(yis_right_menu_close());
      return;
    }
    if (yis_as_bool(yis_right_model_has_selection())) {
      (void)(yis_right_model_reset_selection());
      return;
    }
    (void)(yis_m_vimana_system_quit(v_sys));
    return;
  }
}

static void yis_main_on_text(YisVal v_dev, YisVal v_scr) {
  YisVal v_txt = yis_m_vimana_device_text_input(v_dev); yis_retain_val(v_txt);
  if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_txt)), YV_INT(0)))) {
    if (yis_as_bool(v_search_active)) {
      (void)(yis_right_model_search_type(v_txt));
      return;
    }
    (void)(yis_right_model_insert_at_caret(v_txt));
    (void)(yis_main_update_title(v_scr));
  }
}

static void yis_main_on_scroll(YisVal v_dev) {
  YisVal v_dy = yis_m_vimana_device_wheel_y(v_dev); yis_retain_val(v_dy);
  if (yis_as_bool(yis_eq(v_dy, YV_INT(0)))) {
    return;
  }
  (void)(yis_right_model_scroll_by(v_dy));
}

static void yis_entry(void) {
#line 331 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_sys = yis_vimana_system(); yis_retain_val(v_sys);
#line 332 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_scr = yis_vimana_screen(YV_STR(stdr_str_lit("Right")), v_WIN_W, v_WIN_H, YV_INT(1)); yis_retain_val(v_scr);
#line 333 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_assets_setup(v_scr));
#line 334 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_m_vimana_screen_set_palette(yis_m_vimana_screen_set_palette(yis_m_vimana_screen_set_palette(yis_m_vimana_screen_set_palette(v_scr, v_color_bg, YV_STR(stdr_str_lit("#ffffee"))), v_color_fg, YV_STR(stdr_str_lit("#222222"))), v_color_2, YV_STR(stdr_str_lit("#777777"))), v_color_3, YV_STR(stdr_str_lit("#008888"))));
#line 338 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_dev = yis_m_vimana_system_device(v_sys); yis_retain_val(v_dev);
#line 339 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_fsys = yis_m_vimana_system_file(v_sys); yis_retain_val(v_fsys);
#line 342 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_file_path(YV_STR(stdr_str_lit(""))));
#line 343 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_text(YV_STR(stdr_str_lit("cask main\n\nbring stdr\n\n-- entry point\n-> ()\n    -- your code here\n;\n"))));
#line 344 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_rebuild_outline());
#line 345 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_update_title(v_scr));
#line 347 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_m_vimana_system_run(v_sys, v_scr, ({ YisVal __lambda_1(void*,int,YisVal*); YisVal* __caps = NULL; __caps = (YisVal*)calloc(4, sizeof(YisVal)); __caps[0] = v_sys; yis_retain_val(__caps[0]); __caps[1] = v_scr; yis_retain_val(__caps[1]); __caps[2] = v_dev; yis_retain_val(__caps[2]); __caps[3] = v_fsys; yis_retain_val(__caps[3]); YV_FN(yi_fn_new_with_env(__lambda_1, 0, __caps, 4)); })));
}

static YisVal __fnwrap_main_in_rect(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  YisVal __a4 = argc > 4 ? argv[4] : YV_NULLV;
  YisVal __a5 = argc > 5 ? argv[5] : YV_NULLV;
  return yis_main_in_rect(__a0, __a1, __a2, __a3, __a4, __a5);
}

static YisVal __fnwrap_main_update_title(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_main_update_title(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_main_load_file(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_main_load_file(__a0, __a1);
  return YV_NULLV;
}

static YisVal __fnwrap_main_save_file(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_main_save_file(__a0, __a1);
  return YV_NULLV;
}

static YisVal __fnwrap_main_new_file(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_main_new_file(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_main_open_file(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_main_open_file(__a0, __a1);
  return YV_NULLV;
}

static YisVal __fnwrap_main_exec_action(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  yis_main_exec_action(__a0, __a1, __a2, __a3);
  return YV_NULLV;
}

static YisVal __fnwrap_main_on_mouse(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  yis_main_on_mouse(__a0, __a1, __a2, __a3);
  return YV_NULLV;
}

static YisVal __fnwrap_main_on_key(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  yis_main_on_key(__a0, __a1, __a2, __a3);
  return YV_NULLV;
}

static YisVal __fnwrap_main_on_text(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_main_on_text(__a0, __a1);
  return YV_NULLV;
}

static YisVal __fnwrap_main_on_scroll(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_main_on_scroll(__a0);
  return YV_NULLV;
}

YisVal __lambda_1(void* env, int argc, YisVal* argv) {
  YisVal* __caps = (YisVal*)env;
  YisVal v_sys = __caps[0];
  YisVal v_scr = __caps[1];
  YisVal v_dev = __caps[2];
  YisVal v_fsys = __caps[3];
#line 348 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_m_vimana_device_poll(v_dev));
#line 351 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_on_mouse(v_dev, v_sys, v_fsys, v_scr));
#line 352 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_on_key(v_dev, v_sys, v_fsys, v_scr));
#line 353 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_on_text(v_dev, v_scr));
#line 354 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_on_scroll(v_dev));
#line 357 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_view_draw_all(v_scr));
  return YV_NULLV;
}

static void __yis_main_init(void) {
  yis_move_into(&v_last_click_time, YV_INT(0));
  yis_move_into(&v_last_click_x, YV_INT(0));
  yis_move_into(&v_last_click_y, YV_INT(0));
  yis_move_into(&v_DBLCLICK_MS, YV_INT(400));
  yis_move_into(&v_DBLCLICK_DIST, YV_INT(8));
}

/* end main unit */

int main(int argc, char **argv) {
  yis_set_args(argc, argv);
  yis_runtime_init();
  __yis_vimana_init();
  __yis_right_model_init();
  __yis_right_menu_init();
  __yis_main_init();
  yis_entry();
  return 0;
}
