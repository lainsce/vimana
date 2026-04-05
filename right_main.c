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
#if defined(__APPLE__)
#include <dlfcn.h>
typedef void *yis_objc_id;
typedef void *yis_objc_sel;
typedef void *yis_objc_cls;
typedef signed char yis_objc_bool;
typedef yis_objc_id  (*yis_objc_msgSend_t)(void);
typedef yis_objc_cls (*yis_objc_getClass_t)(const char*);
typedef yis_objc_sel (*yis_objc_selRegister_t)(const char*);
static yis_objc_msgSend_t     yis_objc_msgSend_fn;
static yis_objc_getClass_t    yis_objc_getClass_fn;
static yis_objc_selRegister_t yis_objc_selRegister_fn;
static int yis_objc_loaded = 0;
static int yis_objc_load(void) {
  if (yis_objc_loaded) return yis_objc_msgSend_fn != NULL;
  yis_objc_loaded = 1;
  void *lib = dlopen("/usr/lib/libobjc.A.dylib", RTLD_LAZY);
  if (!lib) return 0;
  yis_objc_msgSend_fn     = (yis_objc_msgSend_t)dlsym(lib, "objc_msgSend");
  yis_objc_getClass_fn    = (yis_objc_getClass_t)dlsym(lib, "objc_getClass");
  yis_objc_selRegister_fn = (yis_objc_selRegister_t)dlsym(lib, "sel_registerName");
  return yis_objc_msgSend_fn && yis_objc_getClass_fn && yis_objc_selRegister_fn;
}
#define OBJC_CLS(name)       yis_objc_getClass_fn(name)
#define OBJC_SEL(name)       yis_objc_selRegister_fn(name)
#define OBJC_SEND(ret, ...) ((ret(*)(__VA_ARGS__))yis_objc_msgSend_fn)
#endif

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
  if (!yis_objc_load()) return YV_NULLV;
  yis_objc_id cls = (yis_objc_id)OBJC_CLS("NSOpenPanel");
  if (!cls) return YV_NULLV;
  yis_objc_id panel = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(cls, OBJC_SEL("openPanel"));
  if (!panel) return YV_NULLV;
  if (prompt && prompt->data && prompt->data[0]) {
    yis_objc_id ns = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, const char*)(
        (yis_objc_id)OBJC_CLS("NSString"), OBJC_SEL("stringWithUTF8String:"), prompt->data);
    OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_id)(panel, OBJC_SEL("setMessage:"), ns);
  }
  if (ext && ext->data && ext->data[0]) {
    yis_objc_id es = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, const char*)(
        (yis_objc_id)OBJC_CLS("NSString"), OBJC_SEL("stringWithUTF8String:"), ext->data);
    yis_objc_id UTType = (yis_objc_id)OBJC_CLS("UTType");
    if (UTType) {
      yis_objc_id ut = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, yis_objc_id)(UTType, OBJC_SEL("typeWithFilenameExtension:"), es);
      if (ut) {
        yis_objc_id arr = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, yis_objc_id)((yis_objc_id)OBJC_CLS("NSArray"), OBJC_SEL("arrayWithObject:"), ut);
        OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_id)(panel, OBJC_SEL("setAllowedContentTypes:"), arr);
      }
    }
  }
  long result = OBJC_SEND(long, yis_objc_id, yis_objc_sel)(panel, OBJC_SEL("runModal"));
  if (result != 1) return YV_NULLV;
  yis_objc_id urls = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(panel, OBJC_SEL("URLs"));
  yis_objc_id url = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(urls, OBJC_SEL("firstObject"));
  if (!url) return YV_NULLV;
  yis_objc_id path = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(url, OBJC_SEL("path"));
  if (!path) return YV_NULLV;
  const char *cp = OBJC_SEND(const char*, yis_objc_id, yis_objc_sel)(path, OBJC_SEL("UTF8String"));
  if (!cp || !cp[0]) return YV_NULLV;
  return YV_STR(stdr_str_from_slice(cp, strlen(cp)));
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
  if (!yis_objc_load()) return YV_NULLV;
  yis_objc_id cls = (yis_objc_id)OBJC_CLS("NSOpenPanel");
  if (!cls) return YV_NULLV;
  yis_objc_id panel = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(cls, OBJC_SEL("openPanel"));
  if (!panel) return YV_NULLV;
  OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_bool)(panel, OBJC_SEL("setCanChooseDirectories:"), (yis_objc_bool)1);
  OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_bool)(panel, OBJC_SEL("setCanChooseFiles:"), (yis_objc_bool)0);
  if (prompt && prompt->data && prompt->data[0]) {
    yis_objc_id ns = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, const char*)(
        (yis_objc_id)OBJC_CLS("NSString"), OBJC_SEL("stringWithUTF8String:"), prompt->data);
    OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_id)(panel, OBJC_SEL("setMessage:"), ns);
  }
  long result = OBJC_SEND(long, yis_objc_id, yis_objc_sel)(panel, OBJC_SEL("runModal"));
  if (result != 1) return YV_NULLV;
  yis_objc_id urls = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(panel, OBJC_SEL("URLs"));
  yis_objc_id url = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(urls, OBJC_SEL("firstObject"));
  if (!url) return YV_NULLV;
  yis_objc_id path = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(url, OBJC_SEL("path"));
  if (!path) return YV_NULLV;
  const char *cp = OBJC_SEND(const char*, yis_objc_id, yis_objc_sel)(path, OBJC_SEL("UTF8String"));
  if (!cp || !cp[0]) return YV_NULLV;
  return YV_STR(stdr_str_from_slice(cp, strlen(cp)));
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
  if (!yis_objc_load()) return YV_NULLV;
  yis_objc_id cls = (yis_objc_id)OBJC_CLS("NSSavePanel");
  if (!cls) return YV_NULLV;
  yis_objc_id panel = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(cls, OBJC_SEL("savePanel"));
  if (!panel) return YV_NULLV;
  if (prompt && prompt->data && prompt->data[0]) {
    yis_objc_id ns = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, const char*)(
        (yis_objc_id)OBJC_CLS("NSString"), OBJC_SEL("stringWithUTF8String:"), prompt->data);
    OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_id)(panel, OBJC_SEL("setMessage:"), ns);
  }
  if (def && def->data && def->data[0]) {
    yis_objc_id ns = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, const char*)(
        (yis_objc_id)OBJC_CLS("NSString"), OBJC_SEL("stringWithUTF8String:"), def->data);
    OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_id)(panel, OBJC_SEL("setNameFieldStringValue:"), ns);
  }
  if (ext && ext->data && ext->data[0]) {
    yis_objc_id es = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, const char*)(
        (yis_objc_id)OBJC_CLS("NSString"), OBJC_SEL("stringWithUTF8String:"), ext->data);
    yis_objc_id UTType = (yis_objc_id)OBJC_CLS("UTType");
    if (UTType) {
      yis_objc_id ut = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, yis_objc_id)(UTType, OBJC_SEL("typeWithFilenameExtension:"), es);
      if (ut) {
        yis_objc_id arr = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, yis_objc_id)((yis_objc_id)OBJC_CLS("NSArray"), OBJC_SEL("arrayWithObject:"), ut);
        OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_id)(panel, OBJC_SEL("setAllowedContentTypes:"), arr);
      }
    }
  }
  long result = OBJC_SEND(long, yis_objc_id, yis_objc_sel)(panel, OBJC_SEL("runModal"));
  if (result != 1) return YV_NULLV;
  yis_objc_id url = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(panel, OBJC_SEL("URL"));
  if (!url) return YV_NULLV;
  yis_objc_id path = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(url, OBJC_SEL("path"));
  if (!path) return YV_NULLV;
  const char *cp = OBJC_SEND(const char*, yis_objc_id, yis_objc_sel)(path, OBJC_SEL("UTF8String"));
  if (!cp || !cp[0]) return YV_NULLV;
  return YV_STR(stdr_str_from_slice(cp, strlen(cp)));
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

static void __vimana_screen_set_theme_swap(YisVal scrv, YisVal swapv) {
  vimana_screen *screen = vimana_screen_from_val(
      scrv, "vimana.screen.set_theme_swap expects screen");
  vimana_screen_set_theme_swap(screen, yis_as_int(swapv) != 0);
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
#line 28 "../Yis/src/stdlib/stdr.yi"
  (void)(stdr_writef_args(v_fmt, v_args));
}

static YisVal yis_stdr_readf(YisVal v_fmt, YisVal v_args) {
#line 33 "../Yis/src/stdlib/stdr.yi"
  (void)(stdr_writef_args(v_fmt, v_args));
#line 34 "../Yis/src/stdlib/stdr.yi"
  YisVal v_line = YV_STR(stdr_read_line()); yis_retain_val(v_line);
#line 35 "../Yis/src/stdlib/stdr.yi"
  YisVal v_parsed = stdr_readf_parse(v_fmt, v_line, v_args); yis_retain_val(v_parsed);
#line 36 "../Yis/src/stdlib/stdr.yi"
  return v_line;
  (void)(v_parsed);
}

static void yis_stdr_write(YisVal v_x) {
#line 41 "../Yis/src/stdlib/stdr.yi"
  (void)(stdr_write(v_x));
}

static YisVal yis_stdr_is_null(YisVal v_x) {
#line 46 "../Yis/src/stdlib/stdr.yi"
  return yis_stdr_is_none(v_x);
}

static YisVal yis_stdr_str(YisVal v_x) {
  return YV_STR(stdr_to_string(v_x));
}

static YisVal yis_stdr_len(YisVal v_x) {
#line 60 "../Yis/src/stdlib/stdr.yi"
  return YV_INT(stdr_len(v_x));
}

static YisVal yis_stdr_num(YisVal v_x) {
#line 65 "../Yis/src/stdlib/stdr.yi"
  return YV_INT(stdr_num(v_x));
}

static YisVal yis_stdr_slice(YisVal v_s, YisVal v_start, YisVal v_end) {
#line 71 "../Yis/src/stdlib/stdr.yi"
  return stdr_slice(v_s, yis_as_int(v_start), yis_as_int(v_end));
}

static YisVal yis_stdr_concat(YisVal v_a, YisVal v_b) {
#line 77 "../Yis/src/stdlib/stdr.yi"
  return stdr_array_concat(v_a, v_b);
}

static YisVal yis_stdr_join(YisVal v_arr) {
#line 82 "../Yis/src/stdlib/stdr.yi"
  YisVal v_len = YV_INT(stdr_len(v_arr)); yis_retain_val(v_len);
#line 83 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_len, YV_INT(0)))) {
    return YV_STR(stdr_str_lit(""));
  }
#line 84 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_len, YV_INT(1)))) {
    return YV_STR(stdr_to_string(yis_index(v_arr, YV_INT(0))));
  }
#line 85 "../Yis/src/stdlib/stdr.yi"
  YisVal v_a = yis_arr_lit(0); yis_retain_val(v_a);
#line 86 "../Yis/src/stdlib/stdr.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 87 "../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_lt(v_i, v_len)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
    (void)(yis_stdr_push(v_a, YV_STR(stdr_to_string(yis_index(v_arr, v_i)))));
  }
#line 88 "../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_gt(v_len, YV_INT(1)));) {
#line 89 "../Yis/src/stdlib/stdr.yi"
    YisVal v_j = YV_INT(0); yis_retain_val(v_j);
#line 90 "../Yis/src/stdlib/stdr.yi"
    YisVal v_k = YV_INT(0); yis_retain_val(v_k);
#line 91 "../Yis/src/stdlib/stdr.yi"
    for (; yis_as_bool(yis_lt(yis_add(v_j, YV_INT(1)), v_len)); (void)((yis_move_into(&v_j, yis_add(v_j, YV_INT(2))), v_j))) {
#line 92 "../Yis/src/stdlib/stdr.yi"
      (void)(yis_index_set(v_a, v_k, yis_stdr_str_concat(yis_index(v_a, v_j), yis_index(v_a, yis_add(v_j, YV_INT(1))))));
#line 93 "../Yis/src/stdlib/stdr.yi"
      (void)((yis_move_into(&v_k, yis_add(v_k, YV_INT(1))), v_k));
    }
#line 94 "../Yis/src/stdlib/stdr.yi"
    if (yis_as_bool(yis_lt(v_j, v_len))) {
      (void)(yis_index_set(v_a, v_k, yis_index(v_a, v_j)));
      (void)((yis_move_into(&v_k, yis_add(v_k, YV_INT(1))), v_k));
    }
#line 95 "../Yis/src/stdlib/stdr.yi"
    (void)((yis_move_into(&v_len, v_k), v_len));
  }
#line 96 "../Yis/src/stdlib/stdr.yi"
  return yis_index(v_a, YV_INT(0));
}

static void yis_stdr_push(YisVal v_arr, YisVal v_val) {
#line 101 "../Yis/src/stdlib/stdr.yi"
  YisVal v_a_ref = v_arr; yis_retain_val(v_a_ref);
#line 102 "../Yis/src/stdlib/stdr.yi"
  YisVal v_idx = YV_INT(stdr_len(v_a_ref)); yis_retain_val(v_idx);
#line 103 "../Yis/src/stdlib/stdr.yi"
  (void)(yis_index_set(v_a_ref, v_idx, v_val));
}

static YisVal yis_stdr_str_concat(YisVal v_a, YisVal v_b) {
#line 109 "../Yis/src/stdlib/stdr.yi"
  return stdr_str_concat(v_a, v_b);
}

static YisVal yis_stdr_char_code(YisVal v_c) {
#line 115 "../Yis/src/stdlib/stdr.yi"
  return YV_INT(stdr_char_code(v_c));
}

static YisVal yis_stdr_char_from_code(YisVal v_code) {
#line 121 "../Yis/src/stdlib/stdr.yi"
  return stdr_char_from_code(v_code);
}

static YisVal yis_stdr_char_at(YisVal v_s, YisVal v_idx) {
#line 126 "../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_s, v_idx, yis_add(v_idx, YV_INT(1)));
}

static YisVal yis_stdr_substring(YisVal v_s, YisVal v_start) {
#line 131 "../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_s, v_start, YV_INT(stdr_len(v_s)));
}

static YisVal yis_stdr_substring_len(YisVal v_s, YisVal v_start, YisVal v_n) {
#line 136 "../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_s, v_start, yis_add(v_start, v_n));
}

static YisVal yis_stdr_replace(YisVal v_text, YisVal v_from, YisVal v_to) {
#line 142 "../Yis/src/stdlib/stdr.yi"
  return stdr_replace(v_text, v_from, v_to);
}

static YisVal yis_stdr_parse_hex(YisVal v_s) {
#line 148 "../Yis/src/stdlib/stdr.yi"
  return stdr_parse_hex(v_s);
}

static YisVal yis_stdr_floor(YisVal v_x) {
#line 154 "../Yis/src/stdlib/stdr.yi"
  return stdr_floor(v_x);
}

static YisVal yis_stdr_ceil(YisVal v_x) {
#line 160 "../Yis/src/stdlib/stdr.yi"
  return stdr_ceil(v_x);
}

static YisVal yis_stdr_keys(YisVal v_d) {
#line 166 "../Yis/src/stdlib/stdr.yi"
  return stdr_keys(v_d);
}

static YisVal yis_stdr_read_text_file(YisVal v_path) {
#line 171 "../Yis/src/stdlib/stdr.yi"
  return stdr_read_text_file(v_path);
}

static YisVal yis_stdr_write_text_file(YisVal v_path, YisVal v_text) {
#line 176 "../Yis/src/stdlib/stdr.yi"
  return stdr_write_text_file(v_path, v_text);
}

static YisVal yis_stdr_ensure_dir(YisVal v_path) {
#line 183 "../Yis/src/stdlib/stdr.yi"
  return stdr_ensure_dir(v_path);
}

static YisVal yis_stdr_remove_file(YisVal v_path) {
#line 190 "../Yis/src/stdlib/stdr.yi"
  return stdr_remove_file(v_path);
}

static YisVal yis_stdr_move_file(YisVal v_src, YisVal v_dst) {
#line 197 "../Yis/src/stdlib/stdr.yi"
  return stdr_move_file(v_src, v_dst);
}

static YisVal yis_stdr_find_files(YisVal v_root, YisVal v_exts) {
#line 204 "../Yis/src/stdlib/stdr.yi"
  return stdr_find_files(v_root, v_exts);
}

static YisVal yis_stdr_prune_files_older_than(YisVal v_dir, YisVal v_days) {
#line 211 "../Yis/src/stdlib/stdr.yi"
  return stdr_prune_files_older_than(v_dir, v_days);
}

static YisVal yis_stdr_run_command(YisVal v_cmd) {
#line 218 "../Yis/src/stdlib/stdr.yi"
  return stdr_run_command(v_cmd);
}

static YisVal yis_stdr_file_exists(YisVal v_path) {
#line 225 "../Yis/src/stdlib/stdr.yi"
  return stdr_file_exists(v_path);
}

static YisVal yis_stdr_file_mtime(YisVal v_path) {
#line 232 "../Yis/src/stdlib/stdr.yi"
  return stdr_file_mtime(v_path);
}

static YisVal yis_stdr_getcwd(void) {
#line 256 "../Yis/src/stdlib/stdr.yi"
  return stdr_getcwd();
}

static YisVal yis_stdr_home_dir(void) {
#line 260 "../Yis/src/stdlib/stdr.yi"
  return stdr_home_dir();
}

static YisVal yis_stdr_unix_time(void) {
#line 264 "../Yis/src/stdlib/stdr.yi"
  return stdr_unix_time();
}

static YisVal yis_stdr_current_year(void) {
#line 268 "../Yis/src/stdlib/stdr.yi"
  return stdr_current_year();
}

static YisVal yis_stdr_current_month(void) {
#line 272 "../Yis/src/stdlib/stdr.yi"
  return stdr_current_month();
}

static YisVal yis_stdr_current_day(void) {
#line 276 "../Yis/src/stdlib/stdr.yi"
  return stdr_current_day();
}

static YisVal yis_stdr_weekday(YisVal v_year, YisVal v_month, YisVal v_day) {
#line 280 "../Yis/src/stdlib/stdr.yi"
  return stdr_weekday(v_year, v_month, v_day);
}

static YisVal yis_stdr_iso_to_epoch(YisVal v_iso, YisVal v_tz) {
#line 284 "../Yis/src/stdlib/stdr.yi"
  return stdr_iso_to_epoch(v_iso, v_tz);
}

static YisVal yis_stdr_is_ws(YisVal v_ch) {
#line 288 "../Yis/src/stdlib/stdr.yi"
  YisVal v_c = yis_stdr_char_code(v_ch); yis_retain_val(v_c);
#line 289 "../Yis/src/stdlib/stdr.yi"
  return YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_c, YV_INT(32))) || yis_as_bool(yis_eq(v_c, YV_INT(9))))) || yis_as_bool(yis_eq(v_c, YV_INT(10))))) || yis_as_bool(yis_eq(v_c, YV_INT(13))));
}

static YisVal yis_stdr_trim(YisVal v_text) {
#line 293 "../Yis/src/stdlib/stdr.yi"
  YisVal v_n = YV_INT(stdr_len(v_text)); yis_retain_val(v_n);
#line 294 "../Yis/src/stdlib/stdr.yi"
  YisVal v_start = YV_INT(0); yis_retain_val(v_start);
#line 295 "../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_lt(v_start, v_n)); (void)((yis_move_into(&v_start, yis_add(v_start, YV_INT(1))), v_start))) {
#line 296 "../Yis/src/stdlib/stdr.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_is_ws(yis_stdr_slice(v_text, v_start, yis_add(v_start, YV_INT(1)))))))) {
#line 297 "../Yis/src/stdlib/stdr.yi"
      break;
    }
  }
#line 299 "../Yis/src/stdlib/stdr.yi"
  YisVal v_end = v_n; yis_retain_val(v_end);
#line 300 "../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_gt(v_end, v_start)); (void)((yis_move_into(&v_end, yis_sub(v_end, YV_INT(1))), v_end))) {
#line 301 "../Yis/src/stdlib/stdr.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_is_ws(yis_stdr_slice(v_text, yis_sub(v_end, YV_INT(1)), v_end)))))) {
#line 302 "../Yis/src/stdlib/stdr.yi"
      break;
    }
  }
#line 304 "../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_text, v_start, v_end);
}

static YisVal yis_stdr_starts_with(YisVal v_text, YisVal v_prefix) {
#line 308 "../Yis/src/stdlib/stdr.yi"
  YisVal v_tn = YV_INT(stdr_len(v_text)); yis_retain_val(v_tn);
#line 309 "../Yis/src/stdlib/stdr.yi"
  YisVal v_pn = YV_INT(stdr_len(v_prefix)); yis_retain_val(v_pn);
#line 310 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_pn, YV_INT(0)))) {
#line 311 "../Yis/src/stdlib/stdr.yi"
    return YV_BOOL(true);
  }
#line 312 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_gt(v_pn, v_tn))) {
#line 313 "../Yis/src/stdlib/stdr.yi"
    return YV_BOOL(false);
  }
#line 314 "../Yis/src/stdlib/stdr.yi"
  return yis_eq(yis_stdr_slice(v_text, YV_INT(0), v_pn), v_prefix);
}

static YisVal yis_stdr_ends_with(YisVal v_text, YisVal v_suffix) {
#line 318 "../Yis/src/stdlib/stdr.yi"
  YisVal v_tn = YV_INT(stdr_len(v_text)); yis_retain_val(v_tn);
#line 319 "../Yis/src/stdlib/stdr.yi"
  YisVal v_sn = YV_INT(stdr_len(v_suffix)); yis_retain_val(v_sn);
#line 320 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_sn, YV_INT(0)))) {
#line 321 "../Yis/src/stdlib/stdr.yi"
    return YV_BOOL(true);
  }
#line 322 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_gt(v_sn, v_tn))) {
#line 323 "../Yis/src/stdlib/stdr.yi"
    return YV_BOOL(false);
  }
#line 324 "../Yis/src/stdlib/stdr.yi"
  return yis_eq(yis_stdr_slice(v_text, yis_sub(v_tn, v_sn), v_tn), v_suffix);
}

static YisVal yis_stdr_index_of(YisVal v_text, YisVal v_needle) {
#line 328 "../Yis/src/stdlib/stdr.yi"
  YisVal v_tn = YV_INT(stdr_len(v_text)); yis_retain_val(v_tn);
#line 329 "../Yis/src/stdlib/stdr.yi"
  YisVal v_nn = YV_INT(stdr_len(v_needle)); yis_retain_val(v_nn);
#line 330 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_nn, YV_INT(0)))) {
#line 331 "../Yis/src/stdlib/stdr.yi"
    return YV_INT(0);
  }
#line 332 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_gt(v_nn, v_tn))) {
#line 333 "../Yis/src/stdlib/stdr.yi"
    return yis_neg(YV_INT(1));
  }
#line 335 "../Yis/src/stdlib/stdr.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 336 "../Yis/src/stdlib/stdr.yi"
  YisVal v_end = yis_sub(v_tn, v_nn); yis_retain_val(v_end);
#line 337 "../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_le(v_i, v_end)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 338 "../Yis/src/stdlib/stdr.yi"
    if (yis_as_bool(yis_eq(yis_stdr_slice(v_text, v_i, yis_add(v_i, v_nn)), v_needle))) {
#line 339 "../Yis/src/stdlib/stdr.yi"
      return v_i;
    }
  }
#line 340 "../Yis/src/stdlib/stdr.yi"
  return yis_neg(YV_INT(1));
}

static YisVal yis_stdr_contains(YisVal v_text, YisVal v_needle) {
#line 344 "../Yis/src/stdlib/stdr.yi"
  return yis_ge(yis_stdr_index_of(v_text, v_needle), YV_INT(0));
}

static YisVal yis_stdr_last_index_of(YisVal v_text, YisVal v_needle) {
#line 348 "../Yis/src/stdlib/stdr.yi"
  YisVal v_tn = YV_INT(stdr_len(v_text)); yis_retain_val(v_tn);
#line 349 "../Yis/src/stdlib/stdr.yi"
  YisVal v_nn = YV_INT(stdr_len(v_needle)); yis_retain_val(v_nn);
#line 350 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_nn, YV_INT(0))) || yis_as_bool(yis_gt(v_nn, v_tn))))) {
#line 351 "../Yis/src/stdlib/stdr.yi"
    return yis_neg(YV_INT(1));
  }
#line 353 "../Yis/src/stdlib/stdr.yi"
  YisVal v_pos = yis_neg(YV_INT(1)); yis_retain_val(v_pos);
#line 354 "../Yis/src/stdlib/stdr.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 355 "../Yis/src/stdlib/stdr.yi"
  YisVal v_end = yis_sub(v_tn, v_nn); yis_retain_val(v_end);
#line 356 "../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_le(v_i, v_end)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 357 "../Yis/src/stdlib/stdr.yi"
    if (yis_as_bool(yis_eq(yis_stdr_slice(v_text, v_i, yis_add(v_i, v_nn)), v_needle))) {
#line 358 "../Yis/src/stdlib/stdr.yi"
      (void)((yis_move_into(&v_pos, v_i), v_pos));
    }
  }
#line 359 "../Yis/src/stdlib/stdr.yi"
  return v_pos;
}

static YisVal yis_stdr_shell_quote(YisVal v_text) {
#line 363 "../Yis/src/stdlib/stdr.yi"
  YisVal v_p = yis_arr_lit(0); yis_retain_val(v_p);
#line 364 "../Yis/src/stdlib/stdr.yi"
  (void)(yis_stdr_push(v_p, YV_STR(stdr_str_lit("'"))));
#line 365 "../Yis/src/stdlib/stdr.yi"
  YisVal v_chunk_start = YV_INT(0); yis_retain_val(v_chunk_start);
#line 366 "../Yis/src/stdlib/stdr.yi"
  YisVal v_n = YV_INT(stdr_len(v_text)); yis_retain_val(v_n);
#line 367 "../Yis/src/stdlib/stdr.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 368 "../Yis/src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_lt(v_i, v_n)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 369 "../Yis/src/stdlib/stdr.yi"
    if (yis_as_bool(yis_eq(yis_stdr_slice(v_text, v_i, yis_add(v_i, YV_INT(1))), YV_STR(stdr_str_lit("'"))))) {
#line 370 "../Yis/src/stdlib/stdr.yi"
      if (yis_as_bool(yis_lt(v_chunk_start, v_i))) {
        (void)(yis_stdr_push(v_p, yis_stdr_slice(v_text, v_chunk_start, v_i)));
      }
#line 371 "../Yis/src/stdlib/stdr.yi"
      (void)(yis_stdr_push(v_p, YV_STR(stdr_str_lit("'\"'\"'"))));
#line 372 "../Yis/src/stdlib/stdr.yi"
      (void)((yis_move_into(&v_chunk_start, yis_add(v_i, YV_INT(1))), v_chunk_start));
    }
  }
#line 373 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_lt(v_chunk_start, v_n))) {
    (void)(yis_stdr_push(v_p, yis_stdr_slice(v_text, v_chunk_start, v_n)));
  }
#line 374 "../Yis/src/stdlib/stdr.yi"
  (void)(yis_stdr_push(v_p, YV_STR(stdr_str_lit("'"))));
#line 375 "../Yis/src/stdlib/stdr.yi"
  return yis_stdr_join(v_p);
}

static YisVal yis_stdr_basename(YisVal v_path) {
#line 379 "../Yis/src/stdlib/stdr.yi"
  YisVal v_p = yis_stdr_trim(v_path); yis_retain_val(v_p);
#line 380 "../Yis/src/stdlib/stdr.yi"
  YisVal v_idx = yis_stdr_last_index_of(v_p, YV_STR(stdr_str_lit("/"))); yis_retain_val(v_idx);
#line 381 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_lt(v_idx, YV_INT(0)))) {
#line 382 "../Yis/src/stdlib/stdr.yi"
    return v_p;
  }
#line 383 "../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_p, yis_add(v_idx, YV_INT(1)), YV_INT(stdr_len(v_p)));
}

static YisVal yis_stdr_dirname(YisVal v_path) {
#line 387 "../Yis/src/stdlib/stdr.yi"
  YisVal v_p = yis_stdr_trim(v_path); yis_retain_val(v_p);
#line 388 "../Yis/src/stdlib/stdr.yi"
  YisVal v_idx = yis_stdr_last_index_of(v_p, YV_STR(stdr_str_lit("/"))); yis_retain_val(v_idx);
#line 389 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_le(v_idx, YV_INT(0)))) {
#line 390 "../Yis/src/stdlib/stdr.yi"
    return YV_STR(stdr_str_lit(""));
  }
#line 391 "../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_p, YV_INT(0), v_idx);
}

static YisVal yis_stdr_stem(YisVal v_file_name) {
#line 395 "../Yis/src/stdlib/stdr.yi"
  YisVal v_idx = yis_stdr_last_index_of(v_file_name, YV_STR(stdr_str_lit("."))); yis_retain_val(v_idx);
#line 396 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_le(v_idx, YV_INT(0)))) {
#line 397 "../Yis/src/stdlib/stdr.yi"
    return v_file_name;
  }
#line 398 "../Yis/src/stdlib/stdr.yi"
  return yis_stdr_slice(v_file_name, YV_INT(0), v_idx);
}

static YisVal yis_stdr_join_path(YisVal v_dir, YisVal v_name) {
#line 402 "../Yis/src/stdlib/stdr.yi"
  YisVal v_base = yis_stdr_trim(v_dir); yis_retain_val(v_base);
#line 403 "../Yis/src/stdlib/stdr.yi"
  YisVal v_leaf = yis_stdr_trim(v_name); yis_retain_val(v_leaf);
#line 405 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_base)), YV_INT(0)))) {
#line 406 "../Yis/src/stdlib/stdr.yi"
    return v_leaf;
  }
#line 407 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_leaf)), YV_INT(0)))) {
#line 408 "../Yis/src/stdlib/stdr.yi"
    return v_base;
  }
#line 410 "../Yis/src/stdlib/stdr.yi"
  YisVal v_out = v_base; yis_retain_val(v_out);
#line 411 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_ends_with(v_out, YV_STR(stdr_str_lit("/"))))))) {
#line 412 "../Yis/src/stdlib/stdr.yi"
    (void)((yis_move_into(&v_out, yis_stdr_str_concat(v_out, YV_STR(stdr_str_lit("/")))), v_out));
  }
#line 413 "../Yis/src/stdlib/stdr.yi"
  if (yis_as_bool(yis_stdr_starts_with(v_leaf, YV_STR(stdr_str_lit("/"))))) {
#line 414 "../Yis/src/stdlib/stdr.yi"
    (void)((yis_move_into(&v_leaf, yis_stdr_slice(v_leaf, YV_INT(1), YV_INT(stdr_len(v_leaf)))), v_leaf));
  }
#line 415 "../Yis/src/stdlib/stdr.yi"
  return yis_stdr_str_concat(v_out, v_leaf);
}

static YisVal yis_stdr_args(void) {
#line 421 "../Yis/src/stdlib/stdr.yi"
  return stdr_args();
}

static YisVal yis_stdr_open_file_dialog(YisVal v_prompt, YisVal v_extension) {
#line 426 "../Yis/src/stdlib/stdr.yi"
  return stdr_open_file_dialog(v_prompt, v_extension);
}

static YisVal yis_stdr_open_folder_dialog(YisVal v_prompt) {
#line 431 "../Yis/src/stdlib/stdr.yi"
  return stdr_open_folder_dialog(v_prompt);
}

static YisVal yis_stdr_save_file_dialog(YisVal v_prompt, YisVal v_default_name, YisVal v_extension) {
#line 436 "../Yis/src/stdlib/stdr.yi"
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
static YisVal yis_m_vimana_screen_set_theme_swap(YisVal, YisVal);
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
#line 110 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_system_run(v_this, v_scr, v_frame));
#line 111 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_system_quit(YisVal v_this) {
#line 115 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_system_quit(v_this));
#line 116 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_system_ticks(YisVal v_this) {
#line 119 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_ticks(v_this);
}

static YisVal yis_m_vimana_system_sleep(YisVal v_this, YisVal v_ms) {
#line 122 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_system_sleep(v_this, v_ms));
#line 123 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_system_clipboard_text(YisVal v_this) {
#line 126 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_clipboard_text(v_this);
}

static YisVal yis_m_vimana_system_set_clipboard_text(YisVal v_this, YisVal v_text) {
#line 127 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_set_clipboard_text(v_this, v_text);
}

static YisVal yis_m_vimana_system_home_dir(YisVal v_this) {
#line 128 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_home_dir(v_this);
}

static YisVal yis_m_vimana_system_spawn(YisVal v_this, YisVal v_cmd) {
#line 130 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_spawn(v_this, v_cmd);
}

static YisVal yis_m_vimana_system_proc_write(YisVal v_this, YisVal v_proc, YisVal v_text) {
#line 131 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_proc_write(v_this, v_proc, v_text);
}

static YisVal yis_m_vimana_system_proc_read_line(YisVal v_this, YisVal v_proc) {
#line 132 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_proc_read_line(v_this, v_proc);
}

static YisVal yis_m_vimana_system_proc_running(YisVal v_this, YisVal v_proc) {
#line 133 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_proc_running(v_this, v_proc);
}

static YisVal yis_m_vimana_system_proc_kill(YisVal v_this, YisVal v_proc) {
#line 135 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_system_proc_kill(v_this, v_proc));
#line 136 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_system_proc_free(YisVal v_this, YisVal v_proc) {
#line 139 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_system_proc_free(v_this, v_proc));
#line 140 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_system_device(YisVal v_this) {
#line 143 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_device(v_this);
}

static YisVal yis_m_vimana_system_datetime(YisVal v_this) {
#line 144 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_datetime(v_this);
}

static YisVal yis_m_vimana_system_file(YisVal v_this) {
#line 145 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system_file(v_this);
}

static YisVal yis_m_vimana_screen_clear(YisVal v_this, YisVal v_bg) {
#line 150 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_clear(v_this, v_bg));
#line 151 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_resize(YisVal v_this, YisVal v_width, YisVal v_height) {
#line 155 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_resize(v_this, v_width, v_height));
#line 156 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_palette(YisVal v_this, YisVal v_slot, YisVal v_color) {
#line 160 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_palette(v_this, v_slot, v_color));
#line 161 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_font_glyph(YisVal v_this, YisVal v_code, YisVal v_icn) {
#line 165 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_font_glyph(v_this, v_code, v_icn));
#line 166 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_font_chr(YisVal v_this, YisVal v_code, YisVal v_chr) {
#line 170 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_font_chr(v_this, v_code, v_chr));
#line 171 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_font_width(YisVal v_this, YisVal v_code, YisVal v_width) {
#line 175 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_font_width(v_this, v_code, v_width));
#line 176 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_font_size(YisVal v_this, YisVal v_size) {
#line 180 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_font_size(v_this, v_size));
#line 181 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_theme_swap(YisVal v_this, YisVal v_swap) {
#line 185 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_theme_swap(v_this, v_swap));
#line 186 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_sprite(YisVal v_this, YisVal v_addr, YisVal v_sprite, YisVal v_mode) {
#line 190 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_sprite(v_this, v_addr, v_sprite, v_mode));
#line 191 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_x(YisVal v_this, YisVal v_x) {
#line 195 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_x(v_this, v_x));
#line 196 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_y(YisVal v_this, YisVal v_y) {
#line 200 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_y(v_this, v_y));
#line 201 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_addr(YisVal v_this, YisVal v_addr) {
#line 205 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_addr(v_this, v_addr));
#line 206 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_auto(YisVal v_this, YisVal v_auto) {
#line 210 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_auto(v_this, v_auto));
#line 211 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_put(YisVal v_this, YisVal v_x, YisVal v_y, YisVal v_glyph, YisVal v_fg, YisVal v_bg) {
#line 215 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_put(v_this, v_x, v_y, v_glyph, v_fg, v_bg));
#line 216 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_put_icn(YisVal v_this, YisVal v_x, YisVal v_y, YisVal v_icn, YisVal v_fg, YisVal v_bg) {
#line 220 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_put_icn(v_this, v_x, v_y, v_icn, v_fg, v_bg));
#line 221 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_put_text(YisVal v_this, YisVal v_x, YisVal v_y, YisVal v_text, YisVal v_fg, YisVal v_bg) {
#line 225 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_put_text(v_this, v_x, v_y, v_text, v_fg, v_bg));
#line 226 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_sprite(YisVal v_this, YisVal v_ctrl) {
#line 230 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_sprite(v_this, v_ctrl));
#line 231 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_pixel(YisVal v_this, YisVal v_ctrl) {
#line 235 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_pixel(v_this, v_ctrl));
#line 236 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_present(YisVal v_this) {
#line 240 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_present(v_this));
#line 241 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_draw_titlebar(YisVal v_this, YisVal v_bg) {
#line 245 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_draw_titlebar(v_this, v_bg));
#line 246 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_titlebar_title(YisVal v_this, YisVal v_title) {
#line 250 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_titlebar_title(v_this, v_title));
#line 251 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_set_titlebar_button(YisVal v_this, YisVal v_show) {
#line 255 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_screen_set_titlebar_button(v_this, v_show));
#line 256 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_screen_titlebar_button_pressed(YisVal v_this) {
#line 260 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_titlebar_button_pressed(v_this);
}

static YisVal yis_m_vimana_screen_x(YisVal v_this) {
#line 263 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_x(v_this);
}

static YisVal yis_m_vimana_screen_y(YisVal v_this) {
#line 264 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_y(v_this);
}

static YisVal yis_m_vimana_screen_addr(YisVal v_this) {
#line 265 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_addr(v_this);
}

static YisVal yis_m_vimana_screen_auto(YisVal v_this) {
#line 266 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_auto(v_this);
}

static YisVal yis_m_vimana_screen_width(YisVal v_this) {
#line 268 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_width(v_this);
}

static YisVal yis_m_vimana_screen_height(YisVal v_this) {
#line 269 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_height(v_this);
}

static YisVal yis_m_vimana_screen_scale(YisVal v_this) {
#line 270 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_screen_scale(v_this);
}

static YisVal yis_m_vimana_device_poll(YisVal v_this) {
#line 275 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  (void)(__vimana_device_poll(v_this));
#line 276 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return v_this;
}

static YisVal yis_m_vimana_device_key_down(YisVal v_this, YisVal v_scancode) {
#line 279 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_key_down(v_this, v_scancode);
}

static YisVal yis_m_vimana_device_key_pressed(YisVal v_this, YisVal v_scancode) {
#line 280 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_key_pressed(v_this, v_scancode);
}

static YisVal yis_m_vimana_device_mouse_down(YisVal v_this, YisVal v_button) {
#line 281 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_mouse_down(v_this, v_button);
}

static YisVal yis_m_vimana_device_mouse_pressed(YisVal v_this, YisVal v_button) {
#line 282 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_mouse_pressed(v_this, v_button);
}

static YisVal yis_m_vimana_device_pointer_x(YisVal v_this) {
#line 283 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_pointer_x(v_this);
}

static YisVal yis_m_vimana_device_pointer_y(YisVal v_this) {
#line 284 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_pointer_y(v_this);
}

static YisVal yis_m_vimana_device_tile_x(YisVal v_this) {
#line 285 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_tile_x(v_this);
}

static YisVal yis_m_vimana_device_tile_y(YisVal v_this) {
#line 286 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_tile_y(v_this);
}

static YisVal yis_m_vimana_device_wheel_x(YisVal v_this) {
#line 287 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_wheel_x(v_this);
}

static YisVal yis_m_vimana_device_wheel_y(YisVal v_this) {
#line 288 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_wheel_y(v_this);
}

static YisVal yis_m_vimana_device_text_input(YisVal v_this) {
#line 289 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_device_text_input(v_this);
}

static YisVal yis_m_vimana_datetime_now(YisVal v_this) {
#line 293 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_now(v_this);
}

static YisVal yis_m_vimana_datetime_year(YisVal v_this) {
#line 294 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_year(v_this);
}

static YisVal yis_m_vimana_datetime_month(YisVal v_this) {
#line 295 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_month(v_this);
}

static YisVal yis_m_vimana_datetime_day(YisVal v_this) {
#line 296 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_day(v_this);
}

static YisVal yis_m_vimana_datetime_hour(YisVal v_this) {
#line 297 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_hour(v_this);
}

static YisVal yis_m_vimana_datetime_minute(YisVal v_this) {
#line 298 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_minute(v_this);
}

static YisVal yis_m_vimana_datetime_second(YisVal v_this) {
#line 299 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_second(v_this);
}

static YisVal yis_m_vimana_datetime_weekday(YisVal v_this) {
#line 300 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_weekday(v_this);
}

static YisVal yis_m_vimana_datetime_year_at(YisVal v_this, YisVal v_timestamp) {
#line 301 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_year_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_datetime_month_at(YisVal v_this, YisVal v_timestamp) {
#line 302 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_month_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_datetime_day_at(YisVal v_this, YisVal v_timestamp) {
#line 303 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_day_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_datetime_hour_at(YisVal v_this, YisVal v_timestamp) {
#line 304 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_hour_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_datetime_minute_at(YisVal v_this, YisVal v_timestamp) {
#line 305 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_minute_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_datetime_second_at(YisVal v_this, YisVal v_timestamp) {
#line 306 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_second_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_datetime_weekday_at(YisVal v_this, YisVal v_timestamp) {
#line 307 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_datetime_weekday_at(v_this, v_timestamp);
}

static YisVal yis_m_vimana_file_read_text(YisVal v_this, YisVal v_path) {
#line 311 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_read_text(v_this, v_path);
}

static YisVal yis_m_vimana_file_read_bytes(YisVal v_this, YisVal v_path) {
#line 312 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_read_bytes(v_this, v_path);
}

static YisVal yis_m_vimana_file_write_text(YisVal v_this, YisVal v_path, YisVal v_text) {
#line 313 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_write_text(v_this, v_path, v_text);
}

static YisVal yis_m_vimana_file_write_bytes(YisVal v_this, YisVal v_path, YisVal v_bytes) {
#line 314 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_write_bytes(v_this, v_path, v_bytes);
}

static YisVal yis_m_vimana_file_exists(YisVal v_this, YisVal v_path) {
#line 315 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_exists(v_this, v_path);
}

static YisVal yis_m_vimana_file_remove(YisVal v_this, YisVal v_path) {
#line 316 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_remove(v_this, v_path);
}

static YisVal yis_m_vimana_file_rename(YisVal v_this, YisVal v_path, YisVal v_new_path) {
#line 317 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_rename(v_this, v_path, v_new_path);
}

static YisVal yis_m_vimana_file_list(YisVal v_this, YisVal v_path) {
#line 318 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_list(v_this, v_path);
}

static YisVal yis_m_vimana_file_is_dir(YisVal v_this, YisVal v_path) {
#line 319 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_file_is_dir(v_this, v_path);
}

static YisVal yis_vimana_system(void) {
#line 322 "/opt/homebrew/share/yis/stdlib/vimana.yi"
  return __vimana_system();
}

static YisVal yis_vimana_screen(YisVal v_title, YisVal v_width, YisVal v_height, YisVal v_scale) {
#line 323 "/opt/homebrew/share/yis/stdlib/vimana.yi"
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
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(" ")))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("(")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(":"))))))) {
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
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("--|")))) && yis_as_bool(yis_ge(v_n, YV_INT(6)))))) {
#line 58 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      YisVal v_se = v_n; yis_retain_val(v_se);
#line 59 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      { YisVal v_j = YV_INT(3); yis_retain_val(v_j);
      for (; yis_as_bool(yis_lt(v_j, yis_sub(v_n, YV_INT(1)))); (void)((yis_move_into(&v_j, yis_add(v_j, YV_INT(1))), v_j))) {
#line 60 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
        if (yis_as_bool(yis_eq(stdr_slice(v_trimmed, yis_as_int(v_j), yis_as_int(yis_add(v_j, YV_INT(3)))), YV_STR(stdr_str_lit("|--"))))) {
#line 61 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
          (void)((yis_move_into(&v_se, v_j), v_se));
#line 62 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
          break;
        }
      } }
#line 63 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      if (yis_as_bool(yis_gt(v_se, YV_INT(3)))) {
#line 64 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
        YisVal v_sec_label = yis_stdr_trim(stdr_slice(v_trimmed, yis_as_int(YV_INT(3)), yis_as_int(v_se))); yis_retain_val(v_sec_label);
#line 65 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
        if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_sec_label)), YV_INT(0)))) {
#line 66 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
          (void)(yis_outline_parser_add_item(v_items, v_sec_label, v_ln, YV_STR(stdr_str_lit("section"))));
        }
      }
    } else if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("cask "))))) {
#line 68 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(5)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("cask"))));
    } else if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("bring "))))) {
#line 70 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(6)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("bring"))));
    } else if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("pub ,: "))))) {
#line 72 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(6)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("struct"))));
    } else if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit(",: "))))) {
#line 74 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(3)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("struct"))));
    } else if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("!: "))))) {
#line 76 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(3)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("pub_fn"))));
    } else if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit(":: "))))) {
#line 78 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(3)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("const"))));
    } else if (yis_as_bool(YV_BOOL(yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit(": ")))) && yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit(":: "))))))))) {
#line 80 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(2)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("fn"))));
    } else if (yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("-> "))))) {
#line 82 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, YV_STR(stdr_str_lit("entry")), v_ln, YV_STR(stdr_str_lit("entry"))));
    } else if (yis_as_bool(YV_BOOL(yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("def ")))) && yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_starts_with(v_trimmed, YV_STR(stdr_str_lit("def ?"))))))))) {
#line 84 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
      (void)(yis_outline_parser_add_item(v_items, yis_outline_parser_first_word(stdr_slice(v_trimmed, yis_as_int(YV_INT(4)), yis_as_int(v_n))), v_ln, YV_STR(stdr_str_lit("const"))));
    }









#line 86 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    (void)((yis_move_into(&v_start, yis_add(v_i, YV_INT(1))), v_start));
#line 87 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
    (void)((yis_move_into(&v_ln, yis_add(v_ln, YV_INT(1))), v_ln));
  } }
#line 89 "/Users/nayu/Developer/Cogito/extras/Right/outline_parser.yi"
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
static void yis_right_model_open_dir_listing(YisVal);
static void yis_right_model_close_dir_listing(void);
static void yis_right_model_dir_scroll_by(YisVal);
static YisVal yis_right_model_line_count(YisVal);
static YisVal yis_right_model_line_col_from_index(YisVal, YisVal);
static YisVal yis_right_model_index_from_line_col(YisVal, YisVal, YisVal);
static YisVal yis_right_model_sel_normalized(void);
static YisVal yis_right_model_vline_count(YisVal);
static YisVal yis_right_model_vline_col(YisVal, YisVal);
static YisVal yis_right_model_vindex_from_line_col(YisVal, YisVal, YisVal);
static YisVal yis_right_model_has_selection(void);
static YisVal yis_right_model_visible_lines(void);
static void yis_right_model_sync_screen_size(YisVal, YisVal);
static void yis_right_model_toggle_sidebar(void);
static void yis_right_model_toggle_highlight(void);
static void yis_right_model_toggle_line_endings(void);
static void yis_right_model_toggle_tabs(void);
static YisVal yis_right_model_sidebar_chars(void);
static YisVal yis_right_model_sidebar_right(void);
static YisVal yis_right_model_editor_left(void);
static YisVal yis_right_model_editor_width(void);
static YisVal yis_right_model_editor_cols(void);
static void yis_right_model_set_sidebar_width(YisVal);
static void yis_right_model_start_resize(void);
static void yis_right_model_stop_resize(void);
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
static void yis_right_model_clamp_outline_scroll(void);
static void yis_right_model_outline_scroll_by(YisVal);
static YisVal yis_right_model_caret_from_editor_click(YisVal, YisVal);
static YisVal yis_right_model_outline_index_from_click(YisVal);
static void yis_right_model_move_caret_line_start(YisVal);
static void yis_right_model_move_caret_line_end(YisVal);
static void yis_right_model_move_caret_doc_start(YisVal);
static void yis_right_model_move_caret_doc_end(YisVal);
static void yis_right_model_move_caret_word_left(YisVal);
static void yis_right_model_move_caret_word_right(YisVal);
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
static YisVal yis_right_model_is_open_bracket(YisVal);
static YisVal yis_right_model_is_close_bracket(YisVal);
static YisVal yis_right_model_matching_bracket(YisVal);
static YisVal yis_right_model_find_matching_bracket(void);
static YisVal yis_right_model_word_before_caret(void);
static void yis_right_model_update_autocomplete(void);
static void yis_right_model_accept_autocomplete(void);
static void yis_right_model_dismiss_autocomplete(void);
static void yis_right_model_select_word(void);
static void yis_right_model_select_line(void);
static void yis_right_model_strip_trailing(void);
static void yis_right_model_trim_trailing_lines(void);
static void yis_right_model_tab_indent(void);
static void yis_right_model_untab_indent(void);
static YisVal __fnwrap_right_model_open_dir_listing(void*,int,YisVal*);
static YisVal __fnwrap_right_model_close_dir_listing(void*,int,YisVal*);
static YisVal __fnwrap_right_model_dir_scroll_by(void*,int,YisVal*);
static YisVal __fnwrap_right_model_line_count(void*,int,YisVal*);
static YisVal __fnwrap_right_model_line_col_from_index(void*,int,YisVal*);
static YisVal __fnwrap_right_model_index_from_line_col(void*,int,YisVal*);
static YisVal __fnwrap_right_model_sel_normalized(void*,int,YisVal*);
static YisVal __fnwrap_right_model_vline_count(void*,int,YisVal*);
static YisVal __fnwrap_right_model_vline_col(void*,int,YisVal*);
static YisVal __fnwrap_right_model_vindex_from_line_col(void*,int,YisVal*);
static YisVal __fnwrap_right_model_has_selection(void*,int,YisVal*);
static YisVal __fnwrap_right_model_visible_lines(void*,int,YisVal*);
static YisVal __fnwrap_right_model_sync_screen_size(void*,int,YisVal*);
static YisVal __fnwrap_right_model_toggle_sidebar(void*,int,YisVal*);
static YisVal __fnwrap_right_model_toggle_highlight(void*,int,YisVal*);
static YisVal __fnwrap_right_model_toggle_line_endings(void*,int,YisVal*);
static YisVal __fnwrap_right_model_toggle_tabs(void*,int,YisVal*);
static YisVal __fnwrap_right_model_sidebar_chars(void*,int,YisVal*);
static YisVal __fnwrap_right_model_sidebar_right(void*,int,YisVal*);
static YisVal __fnwrap_right_model_editor_left(void*,int,YisVal*);
static YisVal __fnwrap_right_model_editor_width(void*,int,YisVal*);
static YisVal __fnwrap_right_model_editor_cols(void*,int,YisVal*);
static YisVal __fnwrap_right_model_set_sidebar_width(void*,int,YisVal*);
static YisVal __fnwrap_right_model_start_resize(void*,int,YisVal*);
static YisVal __fnwrap_right_model_stop_resize(void*,int,YisVal*);
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
static YisVal __fnwrap_right_model_clamp_outline_scroll(void*,int,YisVal*);
static YisVal __fnwrap_right_model_outline_scroll_by(void*,int,YisVal*);
static YisVal __fnwrap_right_model_caret_from_editor_click(void*,int,YisVal*);
static YisVal __fnwrap_right_model_outline_index_from_click(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_line_start(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_line_end(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_doc_start(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_doc_end(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_word_left(void*,int,YisVal*);
static YisVal __fnwrap_right_model_move_caret_word_right(void*,int,YisVal*);
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
static YisVal __fnwrap_right_model_is_open_bracket(void*,int,YisVal*);
static YisVal __fnwrap_right_model_is_close_bracket(void*,int,YisVal*);
static YisVal __fnwrap_right_model_matching_bracket(void*,int,YisVal*);
static YisVal __fnwrap_right_model_find_matching_bracket(void*,int,YisVal*);
static YisVal __fnwrap_right_model_word_before_caret(void*,int,YisVal*);
static YisVal __fnwrap_right_model_update_autocomplete(void*,int,YisVal*);
static YisVal __fnwrap_right_model_accept_autocomplete(void*,int,YisVal*);
static YisVal __fnwrap_right_model_dismiss_autocomplete(void*,int,YisVal*);
static YisVal __fnwrap_right_model_select_word(void*,int,YisVal*);
static YisVal __fnwrap_right_model_select_line(void*,int,YisVal*);
static YisVal __fnwrap_right_model_strip_trailing(void*,int,YisVal*);
static YisVal __fnwrap_right_model_trim_trailing_lines(void*,int,YisVal*);
static YisVal __fnwrap_right_model_tab_indent(void*,int,YisVal*);
static YisVal __fnwrap_right_model_untab_indent(void*,int,YisVal*);
static YisVal v_DEFAULT_W = YV_NULLV;
static YisVal v_DEFAULT_H = YV_NULLV;
static YisVal v_FONT_W = YV_NULLV;
static YisVal v_FONT_H = YV_NULLV;
static YisVal v_SCROLLBAR_W = YV_NULLV;
static YisVal v_SCROLLBAR_X = YV_NULLV;
static YisVal v_SIDEBAR_X = YV_NULLV;
static YisVal v_SIDEBAR_MIN = YV_NULLV;
static YisVal v_SIDEBAR_MAX = YV_NULLV;
static YisVal v_PAD_X = YV_NULLV;
static YisVal v_ROW_H = YV_NULLV;
static YisVal v_CONTENT_Y = YV_NULLV;
static YisVal v_COL_GUIDE = YV_NULLV;
static YisVal v_WRAP_COL = YV_NULLV;
static YisVal v_STATUS_H = YV_NULLV;
static YisVal v_RESIZE_ZONE = YV_NULLV;
static YisVal v_screen_w = YV_NULLV;
static YisVal v_screen_h = YV_NULLV;
static YisVal v_sidebar_visible = YV_NULLV;
static YisVal v_sidebar_width = YV_NULLV;
static YisVal v_resizing_sidebar = YV_NULLV;
static YisVal v_C_BG = YV_NULLV;
static YisVal v_C_FG = YV_NULLV;
static YisVal v_C_SEL = YV_NULLV;
static YisVal v_C_ACCENT = YV_NULLV;
static YisVal v_H_KEYWORD = YV_NULLV;
static YisVal v_H_COMMENT = YV_NULLV;
static YisVal v_H_STRING = YV_NULLV;
static YisVal v_H_TYPE = YV_NULLV;
static YisVal v_GLYPH_TAB = YV_NULLV;
static YisVal v_GLYPH_BRK = YV_NULLV;
static YisVal v_GLYPH_ARROW = YV_NULLV;
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
static YisVal v_KEY_H = YV_NULLV;
static YisVal v_KEY_I = YV_NULLV;
static YisVal v_KEY_L = YV_NULLV;
static YisVal v_KEY_Q = YV_NULLV;
static YisVal v_KEY_R = YV_NULLV;
static YisVal v_KEY_T = YV_NULLV;
static YisVal v_KEY_W = YV_NULLV;
static YisVal v_KEY_Z = YV_NULLV;
static YisVal v_KEY_SLASH = YV_NULLV;
static YisVal v_KEY_LGUI = YV_NULLV;
static YisVal v_KEY_RGUI = YV_NULLV;
static YisVal v_KEY_LALT = YV_NULLV;
static YisVal v_KEY_RALT = YV_NULLV;
static YisVal v_KEY_LCTRL = YV_NULLV;
static YisVal v_KEY_RCTRL = YV_NULLV;
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
static YisVal v_highlight_on = YV_NULLV;
static YisVal v_show_line_endings = YV_NULLV;
static YisVal v_show_tabs = YV_NULLV;
static YisVal v_dir_listing_active = YV_NULLV;
static YisVal v_dir_entries = YV_NULLV;
static YisVal v_dir_selected = YV_NULLV;
static YisVal v_dir_scroll = YV_NULLV;
static YisVal v_search_active = YV_NULLV;
static YisVal v_search_query = YV_NULLV;
static YisVal v_search_matches = YV_NULLV;
static YisVal v_search_index = YV_NULLV;
static YisVal v_ROM_SIZE = YV_NULLV;
static YisVal v_autocomplete_word = YV_NULLV;
static YisVal v_autocomplete_visible = YV_NULLV;

// cask right_model
// bring stdr
// bring outline_parser
static void yis_right_model_open_dir_listing(YisVal v_entries) {
#line 104 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dir_entries, v_entries), v_dir_entries));
#line 105 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dir_listing_active, YV_BOOL(true)), v_dir_listing_active));
#line 106 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dir_selected, yis_neg(YV_INT(1))), v_dir_selected));
#line 107 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dir_scroll, YV_INT(0)), v_dir_scroll));
}

static void yis_right_model_close_dir_listing(void) {
#line 111 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dir_listing_active, YV_BOOL(false)), v_dir_listing_active));
#line 112 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dir_entries, yis_arr_lit(0)), v_dir_entries));
#line 113 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dir_selected, yis_neg(YV_INT(1))), v_dir_selected));
#line 114 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dir_scroll, YV_INT(0)), v_dir_scroll));
}

static void yis_right_model_dir_scroll_by(YisVal v_delta) {
#line 118 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dir_scroll, yis_sub(v_dir_scroll, yis_stdr_floor(v_delta))), v_dir_scroll));
#line 119 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_max_scroll = yis_sub(YV_INT(stdr_len(v_dir_entries)), yis_right_model_visible_lines()); yis_retain_val(v_max_scroll);
#line 120 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_max_scroll, YV_INT(0)))) {
#line 121 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_max_scroll, YV_INT(0)), v_max_scroll));
  }
#line 122 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_dir_scroll, YV_INT(0)))) {
#line 123 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_dir_scroll, YV_INT(0)), v_dir_scroll));
  }
#line 124 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_dir_scroll, v_max_scroll))) {
#line 125 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_dir_scroll, v_max_scroll), v_dir_scroll));
  }
}

static YisVal yis_right_model_line_count(YisVal v_code) {
#line 137 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 138 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_total, YV_INT(0)))) {
#line 139 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_INT(1);
  }
#line 140 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lines = YV_INT(1); yis_retain_val(v_lines);
#line 141 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 142 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 143 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_lines, yis_add(v_lines, YV_INT(1))), v_lines));
    }
  } }
#line 144 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return v_lines;
}

static YisVal yis_right_model_line_col_from_index(YisVal v_code, YisVal v_pos) {
#line 148 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 149 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_p = v_pos; yis_retain_val(v_p);
#line 150 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_p, YV_INT(0)))) {
#line 151 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_p, YV_INT(0)), v_p));
  }
#line 152 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_p, v_total))) {
#line 153 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_p, v_total), v_p));
  }
#line 154 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line = YV_INT(0); yis_retain_val(v_line);
#line 155 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_col = YV_INT(0); yis_retain_val(v_col);
#line 156 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_p)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 157 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 158 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_line, yis_add(v_line, YV_INT(1))), v_line));
#line 159 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_col, YV_INT(0)), v_col));
    } else {
#line 161 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(1))), v_col));
    }
  } }
#line 162 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_arr_lit(2, v_line, v_col);
}

static YisVal yis_right_model_index_from_line_col(YisVal v_code, YisVal v_target_line, YisVal v_target_col) {
#line 166 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 167 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line = YV_INT(0); yis_retain_val(v_line);
#line 168 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_start = YV_INT(0); yis_retain_val(v_start);
#line 169 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 170 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_ge(v_line, v_target_line))) {
#line 171 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
#line 172 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 173 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_line, yis_add(v_line, YV_INT(1))), v_line));
#line 174 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_start, yis_add(v_i, YV_INT(1))), v_start));
    }
  } }
#line 175 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_line, v_target_line))) {
#line 176 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_start, v_total), v_start));
  }
#line 177 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_end_pos = v_start; yis_retain_val(v_end_pos);
#line 178 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_j = v_start; yis_retain_val(v_j);
  for (; yis_as_bool(yis_lt(v_j, v_total)); (void)((yis_move_into(&v_j, yis_add(v_j, YV_INT(1))), v_j))) {
#line 179 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_j), yis_as_int(yis_add(v_j, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 180 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
#line 181 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_end_pos, yis_add(v_j, YV_INT(1))), v_end_pos));
  } }
#line 182 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line_len = yis_sub(v_end_pos, v_start); yis_retain_val(v_line_len);
#line 183 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_col = v_target_col; yis_retain_val(v_col);
#line 184 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_col, YV_INT(0)))) {
#line 185 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_col, YV_INT(0)), v_col));
  }
#line 186 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_col, v_line_len))) {
#line 187 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_col, v_line_len), v_col));
  }
#line 188 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_add(v_start, v_col);
}

static YisVal yis_right_model_sel_normalized(void) {
#line 192 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_le(v_sel_start, v_sel_end))) {
#line 193 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return yis_arr_lit(2, v_sel_start, v_sel_end);
  }
#line 194 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_arr_lit(2, v_sel_end, v_sel_start);
}

static YisVal yis_right_model_vline_count(YisVal v_code) {
#line 200 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 201 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_total, YV_INT(0)))) {
#line 202 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_INT(1);
  }
#line 203 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lines = YV_INT(1); yis_retain_val(v_lines);
#line 204 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_col = YV_INT(0); yis_retain_val(v_col);
#line 205 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 206 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 207 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_lines, yis_add(v_lines, YV_INT(1))), v_lines));
#line 208 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_col, YV_INT(0)), v_col));
    } else {
#line 210 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(1))), v_col));
#line 211 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(yis_ge(v_col, v_WRAP_COL))) {
#line 212 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_lines, yis_add(v_lines, YV_INT(1))), v_lines));
#line 213 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_col, YV_INT(0)), v_col));
      }
    }
  } }
#line 214 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return v_lines;
}

static YisVal yis_right_model_vline_col(YisVal v_code, YisVal v_pos) {
#line 218 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 219 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_p = v_pos; yis_retain_val(v_p);
#line 220 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_p, YV_INT(0)))) {
#line 221 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_p, YV_INT(0)), v_p));
  }
#line 222 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_p, v_total))) {
#line 223 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_p, v_total), v_p));
  }
#line 224 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_vl = YV_INT(0); yis_retain_val(v_vl);
#line 225 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_vc = YV_INT(0); yis_retain_val(v_vc);
#line 226 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_p)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 227 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 228 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_vl, yis_add(v_vl, YV_INT(1))), v_vl));
#line 229 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_vc, YV_INT(0)), v_vc));
    } else {
#line 231 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_vc, yis_add(v_vc, YV_INT(1))), v_vc));
#line 232 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(yis_ge(v_vc, v_WRAP_COL))) {
#line 233 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_vl, yis_add(v_vl, YV_INT(1))), v_vl));
#line 234 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_vc, YV_INT(0)), v_vc));
      }
    }
  } }
#line 235 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_arr_lit(2, v_vl, v_vc);
}

static YisVal yis_right_model_vindex_from_line_col(YisVal v_code, YisVal v_target_row, YisVal v_target_col) {
#line 239 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 240 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_vl = YV_INT(0); yis_retain_val(v_vl);
#line 241 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_vc = YV_INT(0); yis_retain_val(v_vc);
#line 243 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 244 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 245 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_ge(v_vl, v_target_row))) {
#line 246 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
#line 247 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 248 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_vl, yis_add(v_vl, YV_INT(1))), v_vl));
#line 249 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_vc, YV_INT(0)), v_vc));
    } else {
#line 251 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_vc, yis_add(v_vc, YV_INT(1))), v_vc));
#line 252 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(yis_ge(v_vc, v_WRAP_COL))) {
#line 253 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_vl, yis_add(v_vl, YV_INT(1))), v_vl));
#line 254 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_vc, YV_INT(0)), v_vc));
      }
    }
  }
#line 255 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_vl, v_target_row))) {
#line 256 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return v_total;
  }
#line 258 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_col = YV_INT(0); yis_retain_val(v_col);
#line 259 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 260 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_ge(v_col, v_target_col))) {
#line 261 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      return v_i;
    }
#line 262 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_ch = stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_ch);
#line 263 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\n"))))) {
#line 264 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      return v_i;
    }
#line 265 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(1))), v_col));
#line 266 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_ge(v_col, v_WRAP_COL))) {
#line 267 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(yis_gt(v_col, v_target_col))) {
#line 268 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        return v_i;
      }
#line 269 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      return yis_add(v_i, YV_INT(1));
    }
  }
#line 270 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return v_i;
}

static YisVal yis_right_model_has_selection(void) {
#line 274 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_ne(v_sel_start, v_sel_end);
}

static YisVal yis_right_model_visible_lines(void) {
#line 278 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_stdr_floor(yis_div(yis_sub(yis_sub(v_screen_h, v_CONTENT_Y), v_STATUS_H), v_ROW_H));
}

static void yis_right_model_sync_screen_size(YisVal v_w, YisVal v_h) {
#line 282 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_screen_w, v_w), v_screen_w));
#line 283 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_screen_h, v_h), v_screen_h));
}

static void yis_right_model_toggle_sidebar(void) {
#line 287 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sidebar_visible, YV_BOOL(!yis_as_bool(v_sidebar_visible))), v_sidebar_visible));
}

static void yis_right_model_toggle_highlight(void) {
#line 291 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_highlight_on, YV_BOOL(!yis_as_bool(v_highlight_on))), v_highlight_on));
}

static void yis_right_model_toggle_line_endings(void) {
#line 295 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_show_line_endings, YV_BOOL(!yis_as_bool(v_show_line_endings))), v_show_line_endings));
}

static void yis_right_model_toggle_tabs(void) {
#line 299 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_show_tabs, YV_BOOL(!yis_as_bool(v_show_tabs))), v_show_tabs));
}

static YisVal yis_right_model_sidebar_chars(void) {
#line 303 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_sub(yis_stdr_floor(yis_div(v_sidebar_width, v_FONT_W)), YV_INT(1));
}

static YisVal yis_right_model_sidebar_right(void) {
#line 307 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_add(v_SIDEBAR_X, v_sidebar_width);
}

static YisVal yis_right_model_editor_left(void) {
#line 311 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_sidebar_visible)) {
#line 312 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return yis_add(yis_right_model_sidebar_right(), v_PAD_X);
  }
#line 313 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return v_SIDEBAR_X;
}

static YisVal yis_right_model_editor_width(void) {
#line 317 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_sidebar_visible)) {
#line 318 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return yis_sub(v_screen_w, yis_right_model_editor_left());
  }
#line 319 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_sub(v_screen_w, v_SIDEBAR_X);
}

static YisVal yis_right_model_editor_cols(void) {
#line 323 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_sub(yis_stdr_floor(yis_div(yis_right_model_editor_width(), v_FONT_W)), YV_INT(1));
}

static void yis_right_model_set_sidebar_width(YisVal v_w) {
#line 327 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_w, v_SIDEBAR_MIN))) {
#line 328 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sidebar_width, v_SIDEBAR_MIN), v_sidebar_width));
  } else if (yis_as_bool(yis_gt(v_w, v_SIDEBAR_MAX))) {
#line 330 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sidebar_width, v_SIDEBAR_MAX), v_sidebar_width));
  } else {
#line 332 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sidebar_width, v_w), v_sidebar_width));
  }

}

static void yis_right_model_start_resize(void) {
#line 336 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_resizing_sidebar, YV_BOOL(true)), v_resizing_sidebar));
}

static void yis_right_model_stop_resize(void) {
#line 340 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_resizing_sidebar, YV_BOOL(false)), v_resizing_sidebar));
}

static YisVal yis_right_model_line_to_char_pos(YisVal v_code, YisVal v_target_line) {
#line 344 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_le(v_target_line, YV_INT(0)))) {
#line 345 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_INT(0);
  }
#line 346 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 347 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line = YV_INT(0); yis_retain_val(v_line);
#line 348 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lstart = YV_INT(0); yis_retain_val(v_lstart);
#line 349 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 350 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(v_line, v_target_line))) {
#line 351 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      return v_lstart;
    }
#line 352 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 353 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_line, yis_add(v_line, YV_INT(1))), v_line));
#line 354 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_lstart, yis_add(v_i, YV_INT(1))), v_lstart));
    }
  } }
#line 355 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return v_lstart;
}

static void yis_right_model_set_text(YisVal v_txt) {
#line 361 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_editor_text, v_txt), v_editor_text));
}

static void yis_right_model_set_caret(YisVal v_pos) {
#line 365 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 366 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_pos, YV_INT(0)))) {
#line 367 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_pos, YV_INT(0)), v_pos));
  }
#line 368 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_pos, v_total))) {
#line 369 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_pos, v_total), v_pos));
  }
#line 370 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_pos), v_caret));
}

static void yis_right_model_set_selection(YisVal v_a, YisVal v_b) {
#line 374 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_a), v_sel_start));
#line 375 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_b), v_sel_end));
}

static void yis_right_model_set_scroll(YisVal v_y) {
#line 379 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_scroll_y, v_y), v_scroll_y));
}

static void yis_right_model_set_file_path(YisVal v_p) {
#line 383 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_file_path, v_p), v_file_path));
}

static void yis_right_model_set_dirty(YisVal v_d) {
#line 387 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dirty, v_d), v_dirty));
}

static void yis_right_model_set_outline_selected(YisVal v_idx) {
#line 391 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_outline_selected, v_idx), v_outline_selected));
}

static void yis_right_model_ensure_visible(void) {
#line 397 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lc = yis_right_model_vline_col(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 398 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_cur_line);
#line 399 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_cur_line, v_scroll_y))) {
#line 400 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_scroll_y, v_cur_line), v_scroll_y));
  } else if (yis_as_bool(yis_ge(v_cur_line, yis_add(v_scroll_y, yis_right_model_visible_lines())))) {
#line 402 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_scroll_y, yis_add(yis_sub(v_cur_line, yis_right_model_visible_lines()), YV_INT(1))), v_scroll_y));
  }

}

static void yis_right_model_clamp_scroll(void) {
#line 406 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = yis_right_model_vline_count(v_editor_text); yis_retain_val(v_total);
#line 407 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_max_scroll = yis_sub(v_total, yis_right_model_visible_lines()); yis_retain_val(v_max_scroll);
#line 408 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_max_scroll, YV_INT(0)))) {
#line 409 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_max_scroll, YV_INT(0)), v_max_scroll));
  }
#line 410 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_scroll_y, YV_INT(0)))) {
#line 411 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_scroll_y, YV_INT(0)), v_scroll_y));
  }
#line 412 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_scroll_y, v_max_scroll))) {
#line 413 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_scroll_y, v_max_scroll), v_scroll_y));
  }
}

static void yis_right_model_rebuild_outline(void) {
#line 417 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_current_outline, yis_outline_parser_parse_outline(v_editor_text)), v_current_outline));
#line 418 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_n = YV_INT(stdr_len(v_current_outline)); yis_retain_val(v_n);
#line 419 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_le(v_n, YV_INT(0)))) {
#line 420 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_outline_selected, yis_neg(YV_INT(1))), v_outline_selected));
  } else if (yis_as_bool(yis_ge(v_outline_selected, v_n))) {
#line 422 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_outline_selected, yis_sub(v_n, YV_INT(1))), v_outline_selected));
  }

}

static void yis_right_model_insert_at_caret(YisVal v_chunk) {
#line 428 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_pair = yis_right_model_sel_normalized(); yis_retain_val(v_pair);
#line 429 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_a = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(0))))))); yis_retain_val(v_a);
#line 430 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_b = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(1))))))); yis_retain_val(v_b);
#line 431 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_prefix = stdr_slice(v_editor_text, yis_as_int(YV_INT(0)), yis_as_int(v_a)); yis_retain_val(v_prefix);
#line 432 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_suffix = stdr_slice(v_editor_text, yis_as_int(v_b), yis_as_int(YV_INT(stdr_len(v_editor_text)))); yis_retain_val(v_suffix);
#line 433 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_editor_text, ({ YisVal __ip[3]; __ip[0] = YV_STR(stdr_to_string(v_prefix)); __ip[1] = YV_STR(stdr_to_string(v_chunk)); __ip[2] = YV_STR(stdr_to_string(v_suffix)); YV_STR(stdr_str_from_parts(3, __ip)); })), v_editor_text));
#line 434 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_pos = yis_add(v_a, YV_INT(stdr_len(v_chunk))); yis_retain_val(v_pos);
#line 435 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_pos), v_caret));
#line 436 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_pos), v_sel_start));
#line 437 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_pos), v_sel_end));
#line 438 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dirty, YV_BOOL(true)), v_dirty));
#line 439 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_rebuild_outline());
#line 440 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_delete_selection(void) {
#line 444 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_has_selection())))) {
#line 445 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 446 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_pair = yis_right_model_sel_normalized(); yis_retain_val(v_pair);
#line 447 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_a = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(0))))))); yis_retain_val(v_a);
#line 448 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_b = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(1))))))); yis_retain_val(v_b);
#line 449 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_prefix = stdr_slice(v_editor_text, yis_as_int(YV_INT(0)), yis_as_int(v_a)); yis_retain_val(v_prefix);
#line 450 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_suffix = stdr_slice(v_editor_text, yis_as_int(v_b), yis_as_int(YV_INT(stdr_len(v_editor_text)))); yis_retain_val(v_suffix);
#line 451 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_editor_text, ({ YisVal __ip[2]; __ip[0] = YV_STR(stdr_to_string(v_prefix)); __ip[1] = YV_STR(stdr_to_string(v_suffix)); YV_STR(stdr_str_from_parts(2, __ip)); })), v_editor_text));
#line 452 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_a), v_caret));
#line 453 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_a), v_sel_start));
#line 454 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_a), v_sel_end));
#line 455 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dirty, YV_BOOL(true)), v_dirty));
#line 456 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_rebuild_outline());
#line 457 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_delete_backward(void) {
#line 461 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_right_model_has_selection())) {
#line 462 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)(yis_right_model_delete_selection());
#line 463 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 464 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_le(v_caret, YV_INT(0)))) {
#line 465 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 466 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_prefix = stdr_slice(v_editor_text, yis_as_int(YV_INT(0)), yis_as_int(yis_sub(v_caret, YV_INT(1)))); yis_retain_val(v_prefix);
#line 467 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_suffix = stdr_slice(v_editor_text, yis_as_int(v_caret), yis_as_int(YV_INT(stdr_len(v_editor_text)))); yis_retain_val(v_suffix);
#line 468 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_editor_text, ({ YisVal __ip[2]; __ip[0] = YV_STR(stdr_to_string(v_prefix)); __ip[1] = YV_STR(stdr_to_string(v_suffix)); YV_STR(stdr_str_from_parts(2, __ip)); })), v_editor_text));
#line 469 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, yis_sub(v_caret, YV_INT(1))), v_caret));
#line 470 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 471 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
#line 472 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dirty, YV_BOOL(true)), v_dirty));
#line 473 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_rebuild_outline());
#line 474 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_delete_forward(void) {
#line 478 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_right_model_has_selection())) {
#line 479 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)(yis_right_model_delete_selection());
#line 480 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 481 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 482 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_ge(v_caret, v_total))) {
#line 483 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 484 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_prefix = stdr_slice(v_editor_text, yis_as_int(YV_INT(0)), yis_as_int(v_caret)); yis_retain_val(v_prefix);
#line 485 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_suffix = stdr_slice(v_editor_text, yis_as_int(yis_add(v_caret, YV_INT(1))), yis_as_int(v_total)); yis_retain_val(v_suffix);
#line 486 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_editor_text, ({ YisVal __ip[2]; __ip[0] = YV_STR(stdr_to_string(v_prefix)); __ip[1] = YV_STR(stdr_to_string(v_suffix)); YV_STR(stdr_str_from_parts(2, __ip)); })), v_editor_text));
#line 487 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 488 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
#line 489 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dirty, YV_BOOL(true)), v_dirty));
#line 490 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_rebuild_outline());
#line 491 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_left(YisVal v_shift) {
#line 497 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_caret, YV_INT(0)))) {
#line 498 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, yis_sub(v_caret, YV_INT(1))), v_caret));
  }
#line 499 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 500 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 502 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 503 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 504 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_right(YisVal v_shift) {
#line 508 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 509 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_caret, v_total))) {
#line 510 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, yis_add(v_caret, YV_INT(1))), v_caret));
  }
#line 511 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 512 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 514 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 515 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 516 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_up(YisVal v_shift) {
#line 520 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lc = yis_right_model_vline_col(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 521 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_cur_line);
#line 522 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_col = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(1))))))); yis_retain_val(v_cur_col);
#line 523 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_cur_line, YV_INT(0)))) {
#line 524 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, yis_right_model_vindex_from_line_col(v_editor_text, yis_sub(v_cur_line, YV_INT(1)), v_cur_col)), v_caret));
  } else {
#line 526 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, YV_INT(0)), v_caret));
  }
#line 527 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 528 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 530 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 531 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 532 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_down(YisVal v_shift) {
#line 536 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lc = yis_right_model_vline_col(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 537 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_cur_line);
#line 538 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_col = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(1))))))); yis_retain_val(v_cur_col);
#line 539 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total_lines = yis_right_model_vline_count(v_editor_text); yis_retain_val(v_total_lines);
#line 540 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_cur_line, yis_sub(v_total_lines, YV_INT(1))))) {
#line 541 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, yis_right_model_vindex_from_line_col(v_editor_text, yis_add(v_cur_line, YV_INT(1)), v_cur_col)), v_caret));
  } else {
#line 543 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, YV_INT(stdr_len(v_editor_text))), v_caret));
  }
#line 544 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 545 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 547 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 548 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 549 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_select_all(void) {
#line 553 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, YV_INT(0)), v_sel_start));
#line 554 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, YV_INT(stdr_len(v_editor_text))), v_sel_end));
#line 555 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_sel_end), v_caret));
}

static YisVal yis_right_model_selected_text(void) {
#line 559 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_has_selection())))) {
#line 560 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_STR(stdr_str_lit(""));
  }
#line 561 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_pair = yis_right_model_sel_normalized(); yis_retain_val(v_pair);
#line 562 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_a = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(0))))))); yis_retain_val(v_a);
#line 563 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_b = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(1))))))); yis_retain_val(v_b);
#line 564 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return stdr_slice(v_editor_text, yis_as_int(v_a), yis_as_int(v_b));
}

static void yis_right_model_scroll_by(YisVal v_delta) {
#line 568 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_scroll_y, yis_sub(v_scroll_y, v_delta)), v_scroll_y));
#line 569 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_clamp_scroll());
}

static void yis_right_model_clamp_outline_scroll(void) {
#line 573 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_current_outline)); yis_retain_val(v_total);
#line 574 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_max_scroll = yis_sub(v_total, yis_right_model_visible_lines()); yis_retain_val(v_max_scroll);
#line 575 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_max_scroll, YV_INT(0)))) {
#line 576 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_max_scroll, YV_INT(0)), v_max_scroll));
  }
#line 577 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_outline_scroll, YV_INT(0)))) {
#line 578 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_outline_scroll, YV_INT(0)), v_outline_scroll));
  }
#line 579 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_outline_scroll, v_max_scroll))) {
#line 580 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_outline_scroll, v_max_scroll), v_outline_scroll));
  }
}

static void yis_right_model_outline_scroll_by(YisVal v_delta) {
#line 584 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_outline_scroll, yis_sub(v_outline_scroll, v_delta)), v_outline_scroll));
#line 585 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_clamp_outline_scroll());
}

static YisVal yis_right_model_caret_from_editor_click(YisVal v_px, YisVal v_py) {
#line 591 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_col = yis_stdr_floor(yis_div(yis_sub(yis_sub(v_px, yis_right_model_editor_left()), v_PAD_X), v_FONT_W)); yis_retain_val(v_col);
#line 592 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_col, YV_INT(0)))) {
#line 593 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_col, YV_INT(0)), v_col));
  }
#line 594 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_row = yis_add(yis_stdr_floor(yis_div(yis_sub(v_py, v_CONTENT_Y), v_ROW_H)), v_scroll_y); yis_retain_val(v_row);
#line 595 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_row, YV_INT(0)))) {
#line 596 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_row, YV_INT(0)), v_row));
  }
#line 597 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return yis_right_model_vindex_from_line_col(v_editor_text, v_row, v_col);
}

static YisVal yis_right_model_outline_index_from_click(YisVal v_py) {
#line 601 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_row = yis_add(yis_stdr_floor(yis_div(yis_sub(v_py, v_CONTENT_Y), v_ROW_H)), v_outline_scroll); yis_retain_val(v_row);
#line 602 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_row, YV_INT(0)))) {
#line 603 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return yis_neg(YV_INT(1));
  }
#line 604 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_ge(v_row, YV_INT(stdr_len(v_current_outline))))) {
#line 605 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return yis_neg(YV_INT(1));
  }
#line 606 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return v_row;
}

static void yis_right_model_move_caret_line_start(YisVal v_shift) {
#line 612 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lc = yis_right_model_line_col_from_index(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 613 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_cur_line);
#line 614 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, yis_right_model_line_to_char_pos(v_editor_text, v_cur_line)), v_caret));
#line 615 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 616 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 618 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 619 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 620 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_line_end(YisVal v_shift) {
#line 624 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lc = yis_right_model_line_col_from_index(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 625 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_cur_line);
#line 626 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 627 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_pos = yis_right_model_line_to_char_pos(v_editor_text, v_cur_line); yis_retain_val(v_pos);
#line 628 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = v_pos; yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 629 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 630 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
#line 631 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_pos, yis_add(v_i, YV_INT(1))), v_pos));
  } }
#line 632 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_pos), v_caret));
#line 633 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 634 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 636 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 637 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 638 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_doc_start(YisVal v_shift) {
#line 642 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, YV_INT(0)), v_caret));
#line 643 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 644 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 646 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 647 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 648 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_doc_end(YisVal v_shift) {
#line 652 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, YV_INT(stdr_len(v_editor_text))), v_caret));
#line 653 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 654 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 656 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 657 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 658 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_word_left(YisVal v_shift) {
#line 664 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 665 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_le(v_caret, YV_INT(0)))) {
#line 666 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(v_shift)))) {
#line 667 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_sel_start, YV_INT(0)), v_sel_start));
#line 668 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_sel_end, YV_INT(0)), v_sel_end));
    }
#line 669 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 670 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_p = yis_sub(v_caret, YV_INT(1)); yis_retain_val(v_p);
#line 672 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  for (; yis_as_bool(yis_gt(v_p, YV_INT(0))); (void)((yis_move_into(&v_p, yis_sub(v_p, YV_INT(1))), v_p))) {
#line 673 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_p), yis_as_int(yis_add(v_p, YV_INT(1)))); yis_retain_val(v_c);
#line 674 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_right_model_is_word_char(v_c))) {
#line 675 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
  }
#line 677 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  for (; yis_as_bool(yis_gt(v_p, YV_INT(0))); (void)((yis_move_into(&v_p, yis_sub(v_p, YV_INT(1))), v_p))) {
#line 678 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_c = stdr_slice(v_editor_text, yis_as_int(yis_sub(v_p, YV_INT(1))), yis_as_int(v_p)); yis_retain_val(v_c);
#line 679 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_is_word_char(v_c))))) {
#line 680 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
  }
#line 681 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_p), v_caret));
#line 682 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 683 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 685 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 686 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 687 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_move_caret_word_right(YisVal v_shift) {
#line 691 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 692 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_ge(v_caret, v_total))) {
#line 693 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(v_shift)))) {
#line 694 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_sel_start, v_total), v_sel_start));
#line 695 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_sel_end, v_total), v_sel_end));
    }
#line 696 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 697 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_p = v_caret; yis_retain_val(v_p);
#line 699 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  for (; yis_as_bool(yis_lt(v_p, v_total)); (void)((yis_move_into(&v_p, yis_add(v_p, YV_INT(1))), v_p))) {
#line 700 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_p), yis_as_int(yis_add(v_p, YV_INT(1)))); yis_retain_val(v_c);
#line 701 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_is_word_char(v_c))))) {
#line 702 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
  }
#line 704 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  for (; yis_as_bool(yis_lt(v_p, v_total)); (void)((yis_move_into(&v_p, yis_add(v_p, YV_INT(1))), v_p))) {
#line 705 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_p), yis_as_int(yis_add(v_p, YV_INT(1)))); yis_retain_val(v_c);
#line 706 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_right_model_is_word_char(v_c))) {
#line 707 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
  }
#line 708 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_p), v_caret));
#line 709 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(v_shift)) {
#line 710 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  } else {
#line 712 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 713 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
  }
#line 714 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static YisVal yis_right_model_is_word_char(YisVal v_ch) {
#line 720 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(" ")))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\n"))))))) {
#line 721 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_BOOL(false);
  }
#line 722 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("(")))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(")")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("[")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("]"))))))) {
#line 723 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_BOOL(false);
  }
#line 724 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("{")))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("}")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(",")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(";"))))))) {
#line 725 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_BOOL(false);
  }
#line 726 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(".")))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(":")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\"")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("'"))))))) {
#line 727 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_BOOL(false);
  }
#line 728 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return YV_BOOL(true);
}

static void yis_right_model_select_word_at(YisVal v_pos) {
#line 732 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 733 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_total, YV_INT(0)))) {
#line 734 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 735 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_p = v_pos; yis_retain_val(v_p);
#line 736 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_ge(v_p, v_total))) {
#line 737 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_p, yis_sub(v_total, YV_INT(1))), v_p));
  }
#line 738 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_p, YV_INT(0)))) {
#line 739 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_p, YV_INT(0)), v_p));
  }
#line 740 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_ch = stdr_slice(v_editor_text, yis_as_int(v_p), yis_as_int(yis_add(v_p, YV_INT(1)))); yis_retain_val(v_ch);
#line 741 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_is_word_char(v_ch))))) {
#line 742 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, v_p), v_caret));
#line 743 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_p), v_sel_start));
#line 744 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, yis_add(v_p, YV_INT(1))), v_sel_end));
#line 745 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 747 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_a = v_p; yis_retain_val(v_a);
#line 748 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = yis_sub(v_p, YV_INT(1)); yis_retain_val(v_i);
  for (; yis_as_bool(yis_ge(v_i, YV_INT(0))); (void)((yis_move_into(&v_i, yis_sub(v_i, YV_INT(1))), v_i))) {
#line 749 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_c);
#line 750 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_is_word_char(v_c))))) {
#line 751 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
#line 752 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_a, v_i), v_a));
  } }
#line 754 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_b = yis_add(v_p, YV_INT(1)); yis_retain_val(v_b);
#line 755 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_j = yis_add(v_p, YV_INT(1)); yis_retain_val(v_j);
  for (; yis_as_bool(yis_lt(v_j, v_total)); (void)((yis_move_into(&v_j, yis_add(v_j, YV_INT(1))), v_j))) {
#line 756 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_j), yis_as_int(yis_add(v_j, YV_INT(1)))); yis_retain_val(v_c);
#line 757 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_is_word_char(v_c))))) {
#line 758 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
#line 759 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_b, yis_add(v_j, YV_INT(1))), v_b));
  } }
#line 760 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_a), v_sel_start));
#line 761 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_b), v_sel_end));
#line 762 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_b), v_caret));
}

static void yis_right_model_open_search(void) {
#line 768 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_active, YV_BOOL(true)), v_search_active));
#line 769 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_query, YV_STR(stdr_str_lit(""))), v_search_query));
#line 770 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_matches, yis_arr_lit(0)), v_search_matches));
#line 771 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_index, yis_neg(YV_INT(1))), v_search_index));
#line 773 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_right_model_has_selection())) {
#line 774 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_search_query, yis_right_model_selected_text()), v_search_query));
#line 775 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)(yis_right_model_run_search());
  }
}

static void yis_right_model_close_search(void) {
#line 779 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_active, YV_BOOL(false)), v_search_active));
#line 780 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_query, YV_STR(stdr_str_lit(""))), v_search_query));
#line 781 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_matches, yis_arr_lit(0)), v_search_matches));
#line 782 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_index, yis_neg(YV_INT(1))), v_search_index));
}

static void yis_right_model_run_search(void) {
#line 786 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_matches, yis_arr_lit(0)), v_search_matches));
#line 787 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_index, yis_neg(YV_INT(1))), v_search_index));
#line 788 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_qlen = YV_INT(stdr_len(v_search_query)); yis_retain_val(v_qlen);
#line 789 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_qlen, YV_INT(0)))) {
#line 790 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 791 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 792 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_le(v_i, yis_sub(v_total, v_qlen))); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 793 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_chunk = stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, v_qlen))); yis_retain_val(v_chunk);
#line 794 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(v_chunk, v_search_query))) {
#line 795 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_search_matches, yis_add(v_search_matches, yis_arr_lit(1, v_i))), v_search_matches));
    }
  } }
#line 796 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_search_matches)), YV_INT(0)))) {
#line 797 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_search_index, YV_INT(0)), v_search_index));
#line 798 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)(yis_right_model_jump_to_match());
  }
}

static void yis_right_model_jump_to_match(void) {
#line 802 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_count = YV_INT(stdr_len(v_search_matches)); yis_retain_val(v_count);
#line 803 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_count, YV_INT(0))) || yis_as_bool(yis_lt(v_search_index, YV_INT(0)))))) {
#line 804 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 805 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_pos = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_search_matches, v_search_index)).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_search_matches, v_search_index)))))); yis_retain_val(v_pos);
#line 806 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, yis_add(v_pos, YV_INT(stdr_len(v_search_query)))), v_caret));
#line 807 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_pos), v_sel_start));
#line 808 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
#line 809 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_ensure_visible());
}

static void yis_right_model_search_next(void) {
#line 813 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_count = YV_INT(stdr_len(v_search_matches)); yis_retain_val(v_count);
#line 814 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_count, YV_INT(0)))) {
#line 815 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 816 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_index, yis_mod(yis_add(v_search_index, YV_INT(1)), v_count)), v_search_index));
#line 817 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_jump_to_match());
}

static void yis_right_model_search_prev(void) {
#line 821 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_count = YV_INT(stdr_len(v_search_matches)); yis_retain_val(v_count);
#line 822 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_count, YV_INT(0)))) {
#line 823 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 824 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_index, yis_mod(yis_add(yis_sub(v_search_index, YV_INT(1)), v_count), v_count)), v_search_index));
#line 825 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_jump_to_match());
}

static void yis_right_model_search_type(YisVal v_ch) {
#line 829 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_search_query, yis_add(v_search_query, v_ch)), v_search_query));
#line 830 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_run_search());
}

static void yis_right_model_search_backspace(void) {
#line 834 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_qlen = YV_INT(stdr_len(v_search_query)); yis_retain_val(v_qlen);
#line 835 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_qlen, YV_INT(0)))) {
#line 836 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_search_query, stdr_slice(v_search_query, yis_as_int(YV_INT(0)), yis_as_int(yis_sub(v_qlen, YV_INT(1))))), v_search_query));
#line 837 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)(yis_right_model_run_search());
  }
}

static void yis_right_model_reset_selection(void) {
#line 841 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 842 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
}

static YisVal yis_right_model_is_open_bracket(YisVal v_ch) {
#line 851 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("(")))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("[")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("{")))));
}

static YisVal yis_right_model_is_close_bracket(YisVal v_ch) {
#line 855 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(")")))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("]")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("}")))));
}

static YisVal yis_right_model_matching_bracket(YisVal v_ch) {
#line 859 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("("))))) {
#line 860 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_STR(stdr_str_lit(")"));
  }
#line 861 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(")"))))) {
#line 862 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_STR(stdr_str_lit("("));
  }
#line 863 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("["))))) {
#line 864 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_STR(stdr_str_lit("]"));
  }
#line 865 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("]"))))) {
#line 866 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_STR(stdr_str_lit("["));
  }
#line 867 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("{"))))) {
#line 868 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_STR(stdr_str_lit("}"));
  }
#line 869 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("}"))))) {
#line 870 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_STR(stdr_str_lit("{"));
  }
#line 871 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return YV_STR(stdr_str_lit(""));
}

static YisVal yis_right_model_find_matching_bracket(void) {
#line 875 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 876 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_lt(v_caret, YV_INT(0))) || yis_as_bool(yis_ge(v_caret, v_total))))) {
#line 877 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return yis_neg(YV_INT(1));
  }
#line 878 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_ch = stdr_slice(v_editor_text, yis_as_int(v_caret), yis_as_int(yis_add(v_caret, YV_INT(1)))); yis_retain_val(v_ch);
#line 879 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_target = yis_right_model_matching_bracket(v_ch); yis_retain_val(v_target);
#line 880 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_target)), YV_INT(0)))) {
#line 882 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_gt(v_caret, YV_INT(0)))) {
#line 883 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_ch, stdr_slice(v_editor_text, yis_as_int(yis_sub(v_caret, YV_INT(1))), yis_as_int(v_caret))), v_ch));
#line 884 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_target, yis_right_model_matching_bracket(v_ch)), v_target));
#line 885 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_target)), YV_INT(0)))) {
#line 886 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        return yis_neg(YV_INT(1));
      }
#line 888 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      YisVal v_pos = yis_sub(v_caret, YV_INT(1)); yis_retain_val(v_pos);
#line 889 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(yis_right_model_is_open_bracket(v_ch))) {
#line 891 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        YisVal v_depth = YV_INT(1); yis_retain_val(v_depth);
#line 892 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        { YisVal v_i = yis_add(v_pos, YV_INT(1)); yis_retain_val(v_i);
        for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 893 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
          YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_c);
#line 894 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
          if (yis_as_bool(yis_eq(v_c, v_ch))) {
#line 895 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
            (void)((yis_move_into(&v_depth, yis_add(v_depth, YV_INT(1))), v_depth));
          } else if (yis_as_bool(yis_eq(v_c, v_target))) {
#line 897 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
            (void)((yis_move_into(&v_depth, yis_sub(v_depth, YV_INT(1))), v_depth));
#line 898 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
            if (yis_as_bool(yis_eq(v_depth, YV_INT(0)))) {
#line 899 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
              return v_i;
            }
          }

        } }
#line 900 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        return yis_neg(YV_INT(1));
      } else {
#line 903 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        YisVal v_depth = YV_INT(1); yis_retain_val(v_depth);
#line 904 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        { YisVal v_i = yis_sub(v_pos, YV_INT(1)); yis_retain_val(v_i);
        for (; yis_as_bool(yis_ge(v_i, YV_INT(0))); (void)((yis_move_into(&v_i, yis_sub(v_i, YV_INT(1))), v_i))) {
#line 905 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
          YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_c);
#line 906 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
          if (yis_as_bool(yis_eq(v_c, v_ch))) {
#line 907 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
            (void)((yis_move_into(&v_depth, yis_add(v_depth, YV_INT(1))), v_depth));
          } else if (yis_as_bool(yis_eq(v_c, v_target))) {
#line 909 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
            (void)((yis_move_into(&v_depth, yis_sub(v_depth, YV_INT(1))), v_depth));
#line 910 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
            if (yis_as_bool(yis_eq(v_depth, YV_INT(0)))) {
#line 911 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
              return v_i;
            }
          }

        } }
#line 912 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        return yis_neg(YV_INT(1));
      }
    }
#line 913 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return yis_neg(YV_INT(1));
  }
#line 914 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_right_model_is_open_bracket(v_ch))) {
#line 916 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_depth = YV_INT(1); yis_retain_val(v_depth);
#line 917 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    { YisVal v_i = yis_add(v_caret, YV_INT(1)); yis_retain_val(v_i);
    for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 918 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_c);
#line 919 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(yis_eq(v_c, v_ch))) {
#line 920 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_depth, yis_add(v_depth, YV_INT(1))), v_depth));
      } else if (yis_as_bool(yis_eq(v_c, v_target))) {
#line 922 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_depth, yis_sub(v_depth, YV_INT(1))), v_depth));
#line 923 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        if (yis_as_bool(yis_eq(v_depth, YV_INT(0)))) {
#line 924 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
          return v_i;
        }
      }

    } }
#line 925 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return yis_neg(YV_INT(1));
  } else {
#line 928 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_depth = YV_INT(1); yis_retain_val(v_depth);
#line 929 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    { YisVal v_i = yis_sub(v_caret, YV_INT(1)); yis_retain_val(v_i);
    for (; yis_as_bool(yis_ge(v_i, YV_INT(0))); (void)((yis_move_into(&v_i, yis_sub(v_i, YV_INT(1))), v_i))) {
#line 930 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_c);
#line 931 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(yis_eq(v_c, v_ch))) {
#line 932 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_depth, yis_add(v_depth, YV_INT(1))), v_depth));
      } else if (yis_as_bool(yis_eq(v_c, v_target))) {
#line 934 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_depth, yis_sub(v_depth, YV_INT(1))), v_depth));
#line 935 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        if (yis_as_bool(yis_eq(v_depth, YV_INT(0)))) {
#line 936 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
          return v_i;
        }
      }

    } }
#line 937 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return yis_neg(YV_INT(1));
  }
}

static YisVal yis_right_model_word_before_caret(void) {
#line 945 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_end = v_caret; yis_retain_val(v_end);
#line 946 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_start = v_end; yis_retain_val(v_start);
#line 947 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = yis_sub(v_end, YV_INT(1)); yis_retain_val(v_i);
  for (; yis_as_bool(yis_ge(v_i, YV_INT(0))); (void)((yis_move_into(&v_i, yis_sub(v_i, YV_INT(1))), v_i))) {
#line 948 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_c);
#line 949 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_is_word_char(v_c))))) {
#line 950 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
#line 951 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_start, v_i), v_start));
  } }
#line 952 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_ge(v_start, v_end))) {
#line 953 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return YV_STR(stdr_str_lit(""));
  }
#line 954 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  return stdr_slice(v_editor_text, yis_as_int(v_start), yis_as_int(v_end));
}

static void yis_right_model_update_autocomplete(void) {
#line 958 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_prefix = yis_right_model_word_before_caret(); yis_retain_val(v_prefix);
#line 959 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(YV_INT(stdr_len(v_prefix)), YV_INT(3)))) {
#line 960 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_autocomplete_word, YV_STR(stdr_str_lit(""))), v_autocomplete_word));
#line 961 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_autocomplete_visible, YV_BOOL(false)), v_autocomplete_visible));
#line 962 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 964 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 965 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_plen = YV_INT(stdr_len(v_prefix)); yis_retain_val(v_plen);
#line 966 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_best = YV_STR(stdr_str_lit("")); yis_retain_val(v_best);
#line 967 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 968 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 969 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_c);
#line 970 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_is_word_char(v_c))))) {
#line 971 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      continue;
    }
#line 973 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_j = v_i; yis_retain_val(v_j);
#line 974 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    for (; yis_as_bool(yis_lt(v_j, v_total)); (void)((yis_move_into(&v_j, yis_add(v_j, YV_INT(1))), v_j))) {
#line 975 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      YisVal v_c2 = stdr_slice(v_editor_text, yis_as_int(v_j), yis_as_int(yis_add(v_j, YV_INT(1)))); yis_retain_val(v_c2);
#line 976 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_right_model_is_word_char(v_c2))))) {
#line 977 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        break;
      }
    }
#line 978 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_word = stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(v_j)); yis_retain_val(v_word);
#line 979 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_wlen = YV_INT(stdr_len(v_word)); yis_retain_val(v_wlen);
#line 980 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_gt(v_wlen, v_plen)) && yis_as_bool(yis_stdr_starts_with(v_word, v_prefix))))) {
#line 982 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ne(v_j, v_caret)) && yis_as_bool(yis_ne(v_i, yis_sub(v_caret, v_plen)))))) {
#line 983 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_best, v_word), v_best));
#line 984 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        break;
      }
    }
#line 985 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_i, v_j), v_i));
  }
#line 986 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_best)), YV_INT(0)))) {
#line 987 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_autocomplete_word, v_best), v_autocomplete_word));
#line 988 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_autocomplete_visible, YV_BOOL(true)), v_autocomplete_visible));
  } else {
#line 990 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_autocomplete_word, YV_STR(stdr_str_lit(""))), v_autocomplete_word));
#line 991 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_autocomplete_visible, YV_BOOL(false)), v_autocomplete_visible));
  }
}

static void yis_right_model_accept_autocomplete(void) {
#line 995 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(v_autocomplete_visible)))) {
#line 996 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    return;
  }
#line 997 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_prefix = yis_right_model_word_before_caret(); yis_retain_val(v_prefix);
#line 998 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_plen = YV_INT(stdr_len(v_prefix)); yis_retain_val(v_plen);
#line 999 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_rest = stdr_slice(v_autocomplete_word, yis_as_int(v_plen), yis_as_int(YV_INT(stdr_len(v_autocomplete_word)))); yis_retain_val(v_rest);
#line 1000 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_insert_at_caret(v_rest));
#line 1001 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_autocomplete_word, YV_STR(stdr_str_lit(""))), v_autocomplete_word));
#line 1002 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_autocomplete_visible, YV_BOOL(false)), v_autocomplete_visible));
}

static void yis_right_model_dismiss_autocomplete(void) {
#line 1006 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_autocomplete_word, YV_STR(stdr_str_lit(""))), v_autocomplete_word));
#line 1007 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_autocomplete_visible, YV_BOOL(false)), v_autocomplete_visible));
}

static void yis_right_model_select_word(void) {
#line 1013 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)(yis_right_model_select_word_at(v_caret));
}

static void yis_right_model_select_line(void) {
#line 1017 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_lc = yis_right_model_line_col_from_index(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 1018 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line = yis_index(v_lc, YV_INT(0)); yis_retain_val(v_line);
#line 1019 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line_start = yis_right_model_line_to_char_pos(v_editor_text, v_line); yis_retain_val(v_line_start);
#line 1020 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 1021 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line_end = v_total; yis_retain_val(v_line_end);
#line 1022 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = v_line_start; yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 1023 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_eq(stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 1024 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_line_end, yis_add(v_i, YV_INT(1))), v_line_end));
#line 1025 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
  } }
#line 1026 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_line_start), v_sel_start));
#line 1027 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_line_end), v_sel_end));
#line 1028 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, v_line_end), v_caret));
}

static void yis_right_model_strip_trailing(void) {
#line 1035 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 1036 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_result = YV_STR(stdr_str_lit("")); yis_retain_val(v_result);
#line 1037 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line_start = YV_INT(0); yis_retain_val(v_line_start);
#line 1038 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_le(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 1039 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_at_end = yis_ge(v_i, v_total); yis_retain_val(v_at_end);
#line 1040 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_is_nl = YV_BOOL(yis_as_bool(YV_BOOL(!yis_as_bool(v_at_end))) && yis_as_bool(yis_eq(stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))); yis_retain_val(v_is_nl);
#line 1041 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(v_at_end) || yis_as_bool(v_is_nl)))) {
#line 1043 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      YisVal v_trail = v_i; yis_retain_val(v_trail);
#line 1044 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      { YisVal v_j = yis_sub(v_i, YV_INT(1)); yis_retain_val(v_j);
      for (; yis_as_bool(yis_ge(v_j, v_line_start)); (void)((yis_move_into(&v_j, yis_sub(v_j, YV_INT(1))), v_j))) {
#line 1045 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_j), yis_as_int(yis_add(v_j, YV_INT(1)))); yis_retain_val(v_c);
#line 1046 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ne(v_c, YV_STR(stdr_str_lit(" ")))) && yis_as_bool(yis_ne(v_c, YV_STR(stdr_str_lit("\t"))))))) {
#line 1047 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
          break;
        }
#line 1048 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_trail, v_j), v_trail));
      } }
#line 1049 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_result, yis_add(v_result, stdr_slice(v_editor_text, yis_as_int(v_line_start), yis_as_int(v_trail)))), v_result));
#line 1050 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(v_is_nl)) {
#line 1051 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_result, yis_add(v_result, YV_STR(stdr_str_lit("\n")))), v_result));
      }
#line 1052 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_line_start, yis_add(v_i, YV_INT(1))), v_line_start));
    }
  } }
#line 1053 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_editor_text, v_result), v_editor_text));
#line 1054 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_gt(v_caret, YV_INT(stdr_len(v_editor_text))))) {
#line 1055 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, YV_INT(stdr_len(v_editor_text))), v_caret));
  }
#line 1056 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 1057 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
#line 1058 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dirty, YV_BOOL(true)), v_dirty));
}

static void yis_right_model_trim_trailing_lines(void) {
#line 1063 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 1064 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_end_pos = v_total; yis_retain_val(v_end_pos);
#line 1065 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = yis_sub(v_total, YV_INT(1)); yis_retain_val(v_i);
  for (; yis_as_bool(yis_ge(v_i, YV_INT(0))); (void)((yis_move_into(&v_i, yis_sub(v_i, YV_INT(1))), v_i))) {
#line 1066 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_c = stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_c);
#line 1067 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_ne(v_c, YV_STR(stdr_str_lit("\n")))) && yis_as_bool(yis_ne(v_c, YV_STR(stdr_str_lit(" ")))))) && yis_as_bool(yis_ne(v_c, YV_STR(stdr_str_lit("\t"))))))) {
#line 1068 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_end_pos, yis_add(v_i, YV_INT(1))), v_end_pos));
#line 1069 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      break;
    }
  } }
#line 1070 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_end_pos, v_total))) {
#line 1071 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_editor_text, yis_add(stdr_slice(v_editor_text, yis_as_int(YV_INT(0)), yis_as_int(v_end_pos)), YV_STR(stdr_str_lit("\n")))), v_editor_text));
#line 1072 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(yis_gt(v_caret, YV_INT(stdr_len(v_editor_text))))) {
#line 1073 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_caret, YV_INT(stdr_len(v_editor_text))), v_caret));
    }
#line 1074 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, v_caret), v_sel_start));
#line 1075 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, v_caret), v_sel_end));
#line 1076 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_dirty, YV_BOOL(true)), v_dirty));
  }
}

static void yis_right_model_tab_indent(void) {
#line 1081 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_norm = yis_right_model_sel_normalized(); yis_retain_val(v_norm);
#line 1082 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_s = yis_index(v_norm, YV_INT(0)); yis_retain_val(v_s);
#line 1083 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_e = yis_index(v_norm, YV_INT(1)); yis_retain_val(v_e);
#line 1084 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_ge(v_s, v_e))) {
#line 1086 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_lc = yis_right_model_line_col_from_index(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 1087 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_line = yis_index(v_lc, YV_INT(0)); yis_retain_val(v_line);
#line 1088 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_s, yis_right_model_line_to_char_pos(v_editor_text, v_line)), v_s));
#line 1089 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_e, v_s), v_e));
  }
#line 1091 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_first_lc = yis_right_model_line_col_from_index(v_editor_text, v_s); yis_retain_val(v_first_lc);
#line 1092 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_last_lc = yis_right_model_line_col_from_index(v_editor_text, yis_sub(v_e, YV_INT(1))); yis_retain_val(v_last_lc);
#line 1093 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_first_line = yis_index(v_first_lc, YV_INT(0)); yis_retain_val(v_first_line);
#line 1094 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_last_line = yis_index(v_last_lc, YV_INT(0)); yis_retain_val(v_last_line);
#line 1095 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_gt(v_e, v_s)) && yis_as_bool(yis_eq(yis_index(v_last_lc, YV_INT(1)), YV_INT(0))))) && yis_as_bool(yis_gt(v_last_line, v_first_line))))) {
#line 1096 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_last_line, yis_sub(v_last_line, YV_INT(1))), v_last_line));
  }
#line 1097 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 1098 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_result = YV_STR(stdr_str_lit("")); yis_retain_val(v_result);
#line 1099 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_line = YV_INT(0); yis_retain_val(v_cur_line);
#line 1100 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line_start = YV_INT(0); yis_retain_val(v_line_start);
#line 1101 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_le(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 1102 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_at_end = yis_ge(v_i, v_total); yis_retain_val(v_at_end);
#line 1103 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_is_nl = YV_BOOL(yis_as_bool(YV_BOOL(!yis_as_bool(v_at_end))) && yis_as_bool(yis_eq(stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))); yis_retain_val(v_is_nl);
#line 1104 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(v_at_end) || yis_as_bool(v_is_nl)))) {
#line 1105 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_cur_line, v_first_line)) && yis_as_bool(yis_le(v_cur_line, v_last_line))))) {
#line 1106 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_result, yis_add(v_result, YV_STR(stdr_str_lit("    ")))), v_result));
      }
#line 1107 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_result, yis_add(v_result, stdr_slice(v_editor_text, yis_as_int(v_line_start), yis_as_int(v_i)))), v_result));
#line 1108 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(v_is_nl)) {
#line 1109 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_result, yis_add(v_result, YV_STR(stdr_str_lit("\n")))), v_result));
      }
#line 1110 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_line_start, yis_add(v_i, YV_INT(1))), v_line_start));
#line 1111 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_cur_line, yis_add(v_cur_line, YV_INT(1))), v_cur_line));
    }
  } }
#line 1112 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_editor_text, v_result), v_editor_text));
#line 1113 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, yis_add(v_caret, YV_INT(4))), v_caret));
#line 1114 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, yis_add(v_sel_start, YV_INT(4))), v_sel_start));
#line 1115 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, yis_add(v_sel_end, yis_mul(yis_add(yis_sub(v_last_line, v_first_line), YV_INT(1)), YV_INT(4)))), v_sel_end));
#line 1116 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dirty, YV_BOOL(true)), v_dirty));
}

static void yis_right_model_untab_indent(void) {
#line 1121 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_norm = yis_right_model_sel_normalized(); yis_retain_val(v_norm);
#line 1122 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_s = yis_index(v_norm, YV_INT(0)); yis_retain_val(v_s);
#line 1123 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_e = yis_index(v_norm, YV_INT(1)); yis_retain_val(v_e);
#line 1124 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_ge(v_s, v_e))) {
#line 1125 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_lc = yis_right_model_line_col_from_index(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 1126 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_line = yis_index(v_lc, YV_INT(0)); yis_retain_val(v_line);
#line 1127 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_s, yis_right_model_line_to_char_pos(v_editor_text, v_line)), v_s));
#line 1128 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_e, v_s), v_e));
  }
#line 1129 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_first_lc = yis_right_model_line_col_from_index(v_editor_text, v_s); yis_retain_val(v_first_lc);
#line 1130 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_last_lc = yis_right_model_line_col_from_index(v_editor_text, yis_sub(v_e, YV_INT(1))); yis_retain_val(v_last_lc);
#line 1131 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_first_line = yis_index(v_first_lc, YV_INT(0)); yis_retain_val(v_first_line);
#line 1132 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_last_line = yis_index(v_last_lc, YV_INT(0)); yis_retain_val(v_last_line);
#line 1133 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_gt(v_e, v_s)) && yis_as_bool(yis_eq(yis_index(v_last_lc, YV_INT(1)), YV_INT(0))))) && yis_as_bool(yis_gt(v_last_line, v_first_line))))) {
#line 1134 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_last_line, yis_sub(v_last_line, YV_INT(1))), v_last_line));
  }
#line 1135 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_total = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_total);
#line 1136 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_result = YV_STR(stdr_str_lit("")); yis_retain_val(v_result);
#line 1137 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_cur_line = YV_INT(0); yis_retain_val(v_cur_line);
#line 1138 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_line_start = YV_INT(0); yis_retain_val(v_line_start);
#line 1139 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  YisVal v_removed = YV_INT(0); yis_retain_val(v_removed);
#line 1140 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_le(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 1141 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_at_end = yis_ge(v_i, v_total); yis_retain_val(v_at_end);
#line 1142 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    YisVal v_is_nl = YV_BOOL(yis_as_bool(YV_BOOL(!yis_as_bool(v_at_end))) && yis_as_bool(yis_eq(stdr_slice(v_editor_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))); yis_retain_val(v_is_nl);
#line 1143 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(v_at_end) || yis_as_bool(v_is_nl)))) {
#line 1144 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      YisVal v_line_text = stdr_slice(v_editor_text, yis_as_int(v_line_start), yis_as_int(v_i)); yis_retain_val(v_line_text);
#line 1145 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_cur_line, v_first_line)) && yis_as_bool(yis_le(v_cur_line, v_last_line))))) {
#line 1147 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        YisVal v_llen = YV_INT(stdr_len(v_line_text)); yis_retain_val(v_llen);
#line 1148 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        YisVal v_spaces = YV_INT(0); yis_retain_val(v_spaces);
#line 1149 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        { YisVal v_k = YV_INT(0); yis_retain_val(v_k);
        for (; yis_as_bool(YV_BOOL(yis_as_bool(yis_lt(v_k, v_llen)) && yis_as_bool(yis_lt(v_k, YV_INT(4))))); (void)((yis_move_into(&v_k, yis_add(v_k, YV_INT(1))), v_k))) {
#line 1150 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
          if (yis_as_bool(yis_eq(stdr_slice(v_line_text, yis_as_int(v_k), yis_as_int(yis_add(v_k, YV_INT(1)))), YV_STR(stdr_str_lit(" "))))) {
#line 1151 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
            (void)((yis_move_into(&v_spaces, yis_add(v_spaces, YV_INT(1))), v_spaces));
          } else {
#line 1153 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
            break;
          }
        } }
#line 1154 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_result, yis_add(v_result, stdr_slice(v_line_text, yis_as_int(v_spaces), yis_as_int(v_llen)))), v_result));
#line 1155 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_removed, yis_add(v_removed, v_spaces)), v_removed));
      } else {
#line 1157 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_result, yis_add(v_result, v_line_text)), v_result));
      }
#line 1158 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      if (yis_as_bool(v_is_nl)) {
#line 1159 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
        (void)((yis_move_into(&v_result, yis_add(v_result, YV_STR(stdr_str_lit("\n")))), v_result));
      }
#line 1160 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_line_start, yis_add(v_i, YV_INT(1))), v_line_start));
#line 1161 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
      (void)((yis_move_into(&v_cur_line, yis_add(v_cur_line, YV_INT(1))), v_cur_line));
    }
  } }
#line 1162 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_editor_text, v_result), v_editor_text));
#line 1163 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_caret, yis_sub(v_caret, v_removed)), v_caret));
#line 1164 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_caret, YV_INT(0)))) {
#line 1165 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_caret, YV_INT(0)), v_caret));
  }
#line 1166 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_start, yis_sub(v_sel_start, v_removed)), v_sel_start));
#line 1167 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_sel_start, YV_INT(0)))) {
#line 1168 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_start, YV_INT(0)), v_sel_start));
  }
#line 1169 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_sel_end, yis_sub(v_sel_end, v_removed)), v_sel_end));
#line 1170 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  if (yis_as_bool(yis_lt(v_sel_end, YV_INT(0)))) {
#line 1171 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
    (void)((yis_move_into(&v_sel_end, YV_INT(0)), v_sel_end));
  }
#line 1172 "/Users/nayu/Developer/Cogito/extras/Right/right_model.yi"
  (void)((yis_move_into(&v_dirty, YV_BOOL(true)), v_dirty));
}

static YisVal __fnwrap_right_model_open_dir_listing(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_open_dir_listing(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_close_dir_listing(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_close_dir_listing();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_dir_scroll_by(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_dir_scroll_by(__a0);
  return YV_NULLV;
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

static YisVal __fnwrap_right_model_vline_count(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_model_vline_count(__a0);
}

static YisVal __fnwrap_right_model_vline_col(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_right_model_vline_col(__a0, __a1);
}

static YisVal __fnwrap_right_model_vindex_from_line_col(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  return yis_right_model_vindex_from_line_col(__a0, __a1, __a2);
}

static YisVal __fnwrap_right_model_has_selection(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_model_has_selection();
}

static YisVal __fnwrap_right_model_visible_lines(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_model_visible_lines();
}

static YisVal __fnwrap_right_model_sync_screen_size(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_right_model_sync_screen_size(__a0, __a1);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_toggle_sidebar(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_toggle_sidebar();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_toggle_highlight(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_toggle_highlight();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_toggle_line_endings(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_toggle_line_endings();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_toggle_tabs(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_toggle_tabs();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_sidebar_chars(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_model_sidebar_chars();
}

static YisVal __fnwrap_right_model_sidebar_right(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_model_sidebar_right();
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

static YisVal __fnwrap_right_model_set_sidebar_width(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_set_sidebar_width(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_start_resize(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_start_resize();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_stop_resize(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_stop_resize();
  return YV_NULLV;
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

static YisVal __fnwrap_right_model_clamp_outline_scroll(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_clamp_outline_scroll();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_outline_scroll_by(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_outline_scroll_by(__a0);
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

static YisVal __fnwrap_right_model_move_caret_word_left(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_move_caret_word_left(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_move_caret_word_right(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_model_move_caret_word_right(__a0);
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

static YisVal __fnwrap_right_model_is_open_bracket(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_model_is_open_bracket(__a0);
}

static YisVal __fnwrap_right_model_is_close_bracket(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_model_is_close_bracket(__a0);
}

static YisVal __fnwrap_right_model_matching_bracket(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_model_matching_bracket(__a0);
}

static YisVal __fnwrap_right_model_find_matching_bracket(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_model_find_matching_bracket();
}

static YisVal __fnwrap_right_model_word_before_caret(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_right_model_word_before_caret();
}

static YisVal __fnwrap_right_model_update_autocomplete(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_update_autocomplete();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_accept_autocomplete(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_accept_autocomplete();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_dismiss_autocomplete(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_dismiss_autocomplete();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_select_word(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_select_word();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_select_line(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_select_line();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_strip_trailing(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_strip_trailing();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_trim_trailing_lines(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_trim_trailing_lines();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_tab_indent(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_tab_indent();
  return YV_NULLV;
}

static YisVal __fnwrap_right_model_untab_indent(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  yis_right_model_untab_indent();
  return YV_NULLV;
}

static void __yis_right_model_init(void) {
  yis_move_into(&v_DEFAULT_W, YV_INT(1360));
  yis_move_into(&v_DEFAULT_H, YV_INT(716));
  yis_move_into(&v_FONT_W, YV_INT(16));
  yis_move_into(&v_FONT_H, YV_INT(24));
  yis_move_into(&v_SCROLLBAR_W, YV_INT(12));
  yis_move_into(&v_SCROLLBAR_X, YV_INT(0));
  yis_move_into(&v_SIDEBAR_X, YV_INT(16));
  yis_move_into(&v_SIDEBAR_MIN, YV_INT(128));
  yis_move_into(&v_SIDEBAR_MAX, YV_INT(480));
  yis_move_into(&v_PAD_X, YV_INT(4));
  yis_move_into(&v_ROW_H, YV_INT(24));
  yis_move_into(&v_CONTENT_Y, YV_INT(28));
  yis_move_into(&v_COL_GUIDE, YV_INT(51));
  yis_move_into(&v_WRAP_COL, YV_INT(80));
  yis_move_into(&v_STATUS_H, YV_INT(0));
  yis_move_into(&v_RESIZE_ZONE, YV_INT(8));
  yis_move_into(&v_screen_w, YV_INT(1360));
  yis_move_into(&v_screen_h, YV_INT(716));
  yis_move_into(&v_sidebar_visible, YV_BOOL(true));
  yis_move_into(&v_sidebar_width, YV_INT(376));
  yis_move_into(&v_resizing_sidebar, YV_BOOL(false));
  yis_move_into(&v_C_BG, YV_INT(0));
  yis_move_into(&v_C_FG, YV_INT(1));
  yis_move_into(&v_C_SEL, YV_INT(2));
  yis_move_into(&v_C_ACCENT, YV_INT(3));
  yis_move_into(&v_H_KEYWORD, YV_INT(3));
  yis_move_into(&v_H_COMMENT, YV_INT(2));
  yis_move_into(&v_H_STRING, YV_INT(2));
  yis_move_into(&v_H_TYPE, YV_INT(3));
  yis_move_into(&v_GLYPH_TAB, YV_INT(1));
  yis_move_into(&v_GLYPH_BRK, YV_INT(2));
  yis_move_into(&v_GLYPH_ARROW, YV_INT(3));
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
  yis_move_into(&v_KEY_H, YV_INT(11));
  yis_move_into(&v_KEY_I, YV_INT(12));
  yis_move_into(&v_KEY_L, YV_INT(15));
  yis_move_into(&v_KEY_Q, YV_INT(20));
  yis_move_into(&v_KEY_R, YV_INT(21));
  yis_move_into(&v_KEY_T, YV_INT(23));
  yis_move_into(&v_KEY_W, YV_INT(26));
  yis_move_into(&v_KEY_Z, YV_INT(29));
  yis_move_into(&v_KEY_SLASH, YV_INT(56));
  yis_move_into(&v_KEY_LGUI, YV_INT(227));
  yis_move_into(&v_KEY_RGUI, YV_INT(231));
  yis_move_into(&v_KEY_LALT, YV_INT(226));
  yis_move_into(&v_KEY_RALT, YV_INT(230));
  yis_move_into(&v_KEY_LCTRL, YV_INT(224));
  yis_move_into(&v_KEY_RCTRL, YV_INT(228));
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
  yis_move_into(&v_highlight_on, YV_BOOL(false));
  yis_move_into(&v_show_line_endings, YV_BOOL(false));
  yis_move_into(&v_show_tabs, YV_BOOL(true));
  yis_move_into(&v_dir_listing_active, YV_BOOL(false));
  yis_move_into(&v_dir_entries, yis_arr_lit(0));
  yis_move_into(&v_dir_selected, yis_neg(YV_INT(1)));
  yis_move_into(&v_dir_scroll, YV_INT(0));
  yis_move_into(&v_search_active, YV_BOOL(false));
  yis_move_into(&v_search_query, YV_STR(stdr_str_lit("")));
  yis_move_into(&v_search_matches, yis_arr_lit(0));
  yis_move_into(&v_search_index, yis_neg(YV_INT(1)));
  yis_move_into(&v_ROM_SIZE, YV_INT(65536));
  yis_move_into(&v_autocomplete_word, YV_STR(stdr_str_lit("")));
  yis_move_into(&v_autocomplete_visible, YV_BOOL(false));
}

/* end embedded module: right_model */

/* begin embedded module: right_menu */
static void yis_right_menu_fill(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static YisVal yis_right_menu_item_count(YisVal);
static YisVal yis_right_menu_cat_x(YisVal);
static YisVal yis_right_menu_shortcut_px(YisVal);
static YisVal yis_right_menu_submenu_width(YisVal);
static YisVal yis_right_menu_item_label(YisVal, YisVal);
static YisVal yis_right_menu_item_shortcut(YisVal, YisVal);
static YisVal yis_right_menu_item_action(YisVal, YisVal);
static YisVal yis_right_menu_is_open(void);
static void yis_right_menu_close(void);
static void yis_right_menu_draw_diamond(YisVal, YisVal, YisVal, YisVal);
static void yis_right_menu_draw_menubar(YisVal);
static YisVal yis_right_menu_draw_key_cap(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static void yis_right_menu_draw_key_shortcut(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static void yis_right_menu_draw_submenu(YisVal);
static YisVal yis_right_menu_cat_from_x(YisVal);
static YisVal yis_right_menu_handle_click(YisVal, YisVal);
static void yis_right_menu_handle_hover(YisVal, YisVal);
static YisVal __fnwrap_right_menu_fill(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_item_count(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_cat_x(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_shortcut_px(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_submenu_width(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_item_label(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_item_shortcut(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_item_action(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_is_open(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_close(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_draw_diamond(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_draw_menubar(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_draw_key_cap(void*,int,YisVal*);
static YisVal __fnwrap_right_menu_draw_key_shortcut(void*,int,YisVal*);
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
static YisVal v_SUBMENU_PAD = YV_NULLV;
static YisVal v_SUBMENU_ITEM_H = YV_NULLV;
static YisVal v_CAT_FILE_X = YV_NULLV;
static YisVal v_CAT_FILE_W = YV_NULLV;
static YisVal v_CAT_EDIT_X = YV_NULLV;
static YisVal v_CAT_EDIT_W = YV_NULLV;
static YisVal v_CAT_SELECT_X = YV_NULLV;
static YisVal v_CAT_SELECT_W = YV_NULLV;
static YisVal v_CAT_FIND_X = YV_NULLV;
static YisVal v_CAT_FIND_W = YV_NULLV;
static YisVal v_CAT_FORMAT_X = YV_NULLV;
static YisVal v_CAT_FORMAT_W = YV_NULLV;
static YisVal v_CAT_VIEW_X = YV_NULLV;
static YisVal v_CAT_VIEW_W = YV_NULLV;
static YisVal v_CAT_INSERT_X = YV_NULLV;
static YisVal v_CAT_INSERT_W = YV_NULLV;
static YisVal v_CAT_FILE = YV_NULLV;
static YisVal v_CAT_EDIT = YV_NULLV;
static YisVal v_CAT_SELECT = YV_NULLV;
static YisVal v_CAT_FIND = YV_NULLV;
static YisVal v_CAT_FORMAT = YV_NULLV;
static YisVal v_CAT_VIEW = YV_NULLV;
static YisVal v_CAT_INSERT = YV_NULLV;
static YisVal v_FILE_COUNT = YV_NULLV;
static YisVal v_EDIT_COUNT = YV_NULLV;
static YisVal v_SELECT_COUNT = YV_NULLV;
static YisVal v_FIND_COUNT = YV_NULLV;
static YisVal v_FORMAT_COUNT = YV_NULLV;
static YisVal v_VIEW_COUNT = YV_NULLV;
static YisVal v_INSERT_COUNT = YV_NULLV;

// cask right_menu
// bring stdr
// bring vimana
// bring right_model
static void yis_right_menu_fill(YisVal v_scr, YisVal v_x, YisVal v_y, YisVal v_w, YisVal v_h, YisVal v_c) {
#line 56 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_auto(v_scr, YV_INT(1)));
#line 57 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  { YisVal v_row = YV_INT(0); yis_retain_val(v_row);
  for (; yis_as_bool(yis_lt(v_row, v_h)); (void)((yis_move_into(&v_row, yis_add(v_row, YV_INT(1))), v_row))) {
#line 58 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_y, v_row)));
#line 59 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_m_vimana_screen_set_x(v_scr, v_x));
#line 60 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    { YisVal v_col = YV_INT(0); yis_retain_val(v_col);
    for (; yis_as_bool(yis_lt(v_col, v_w)); (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(1))), v_col))) {
#line 61 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)(yis_m_vimana_screen_pixel(v_scr, v_c));
    } }
#line 62 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_y, v_row)));
#line 63 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_m_vimana_screen_set_x(v_scr, v_x));
#line 64 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    { YisVal v_col = YV_INT(0); yis_retain_val(v_col);
    for (; yis_as_bool(yis_lt(v_col, v_w)); (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(1))), v_col))) {
#line 65 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)(yis_m_vimana_screen_pixel(v_scr, yis_add(v_c, YV_INT(64))));
    } }
  } }
#line 66 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_auto(v_scr, YV_INT(0)));
}

static YisVal yis_right_menu_item_count(YisVal v_cat) {
#line 70 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FILE))) {
#line 71 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_FILE_COUNT;
  }
#line 72 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_EDIT))) {
#line 73 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_EDIT_COUNT;
  }
#line 74 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_SELECT))) {
#line 75 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_SELECT_COUNT;
  }
#line 76 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FIND))) {
#line 77 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_FIND_COUNT;
  }
#line 78 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FORMAT))) {
#line 79 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_FORMAT_COUNT;
  }
#line 80 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_VIEW))) {
#line 81 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_VIEW_COUNT;
  }
#line 82 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_INSERT))) {
#line 83 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_INSERT_COUNT;
  }
#line 84 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_INT(0);
}

static YisVal yis_right_menu_cat_x(YisVal v_cat) {
#line 88 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FILE))) {
#line 89 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_FILE_X;
  }
#line 90 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_EDIT))) {
#line 91 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_EDIT_X;
  }
#line 92 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_SELECT))) {
#line 93 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_SELECT_X;
  }
#line 94 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FIND))) {
#line 95 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_FIND_X;
  }
#line 96 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FORMAT))) {
#line 97 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_FORMAT_X;
  }
#line 98 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_VIEW))) {
#line 99 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_VIEW_X;
  }
#line 100 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_INSERT))) {
#line 101 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_INSERT_X;
  }
#line 102 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_INT(0);
}

static YisVal yis_right_menu_shortcut_px(YisVal v_shortcut) {
#line 107 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_slen = YV_INT(stdr_len(v_shortcut)); yis_retain_val(v_slen);
#line 108 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_slen, YV_INT(0)))) {
#line 109 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return YV_INT(0);
  }
#line 110 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_total = YV_INT(0); yis_retain_val(v_total);
#line 111 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_part_start = YV_INT(0); yis_retain_val(v_part_start);
#line 112 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_parts = YV_INT(0); yis_retain_val(v_parts);
#line 113 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  { YisVal v_k = YV_INT(0); yis_retain_val(v_k);
  for (; yis_as_bool(yis_le(v_k, v_slen)); (void)((yis_move_into(&v_k, yis_add(v_k, YV_INT(1))), v_k))) {
#line 114 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_at_end = yis_ge(v_k, v_slen); yis_retain_val(v_at_end);
#line 115 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_is_plus = YV_BOOL(yis_as_bool(YV_BOOL(!yis_as_bool(v_at_end))) && yis_as_bool(yis_eq(stdr_slice(v_shortcut, yis_as_int(v_k), yis_as_int(yis_add(v_k, YV_INT(1)))), YV_STR(stdr_str_lit("+"))))); yis_retain_val(v_is_plus);
#line 116 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(v_at_end) || yis_as_bool(v_is_plus)))) {
#line 117 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      YisVal v_part = stdr_slice(v_shortcut, yis_as_int(v_part_start), yis_as_int(v_k)); yis_retain_val(v_part);
#line 118 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      YisVal v_pw = yis_add(yis_mul(YV_INT(stdr_len(v_part)), v_FONT_W), YV_INT(6)); yis_retain_val(v_pw);
#line 119 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_total, yis_add(v_total, v_pw)), v_total));
#line 120 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      if (yis_as_bool(yis_gt(v_parts, YV_INT(0)))) {
#line 121 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
        (void)((yis_move_into(&v_total, yis_add(v_total, YV_INT(4))), v_total));
      }
#line 122 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_parts, yis_add(v_parts, YV_INT(1))), v_parts));
#line 123 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_part_start, yis_add(v_k, YV_INT(1))), v_part_start));
    }
  } }
#line 124 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return v_total;
}

static YisVal yis_right_menu_submenu_width(YisVal v_cat) {
#line 129 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_count = yis_right_menu_item_count(v_cat); yis_retain_val(v_count);
#line 130 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_max_w = YV_INT(0); yis_retain_val(v_max_w);
#line 131 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_count)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 132 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_label = yis_right_menu_item_label(v_cat, v_i); yis_retain_val(v_label);
#line 133 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_shortcut = yis_right_menu_item_shortcut(v_cat, v_i); yis_retain_val(v_shortcut);
#line 134 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_lw = yis_mul(YV_INT(stdr_len(v_label)), v_FONT_W); yis_retain_val(v_lw);
#line 135 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_sw = yis_right_menu_shortcut_px(v_shortcut); yis_retain_val(v_sw);
#line 136 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_row_w = yis_add(v_lw, yis_mul(v_SUBMENU_PAD, YV_INT(2))); yis_retain_val(v_row_w);
#line 137 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_gt(v_sw, YV_INT(0)))) {
#line 138 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_row_w, yis_add(yis_add(v_lw, v_sw), yis_mul(v_SUBMENU_PAD, YV_INT(3)))), v_row_w));
    }
#line 139 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_gt(v_row_w, v_max_w))) {
#line 140 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_max_w, v_row_w), v_max_w));
    }
  } }
#line 141 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return v_max_w;
}

static YisVal yis_right_menu_item_label(YisVal v_cat, YisVal v_idx) {
#line 145 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FILE))) {
#line 146 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 147 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("New"));
    }
#line 148 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 149 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Open"));
    }
#line 150 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 151 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Save"));
    }
#line 152 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 153 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Rename"));
    }
#line 154 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(4)))) {
#line 155 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Exit"));
    }
  }
#line 156 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_EDIT))) {
#line 157 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 158 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Copy"));
    }
#line 159 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 160 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Paste"));
    }
#line 161 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 162 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cut"));
    }
#line 163 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 164 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Delete"));
    }
  }
#line 165 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_SELECT))) {
#line 166 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 167 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("All"));
    }
#line 168 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 169 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Word"));
    }
#line 170 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 171 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Line"));
    }
#line 172 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 173 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Reset"));
    }
  }
#line 174 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FIND))) {
#line 175 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 176 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Find"));
    }
#line 177 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 178 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Find Next"));
    }
#line 179 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 180 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Find Prev"));
    }
  }
#line 181 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FORMAT))) {
#line 182 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 183 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Strip"));
    }
#line 184 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 185 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Trim"));
    }
#line 186 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 187 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Tab"));
    }
#line 188 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 189 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Untab"));
    }
  }
#line 190 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_VIEW))) {
#line 191 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 192 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Sidebar"));
    }
#line 193 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 194 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Highlight"));
    }
#line 195 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 196 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Line Ends"));
    }
#line 197 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 198 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Tabs"));
    }
  }
#line 199 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_INSERT))) {
#line 200 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 201 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Date"));
    }
#line 202 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 203 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Path"));
    }
#line 204 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 205 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Section"));
    }
  }
#line 206 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit(""));
}

static YisVal yis_right_menu_item_shortcut(YisVal v_cat, YisVal v_idx) {
#line 210 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FILE))) {
#line 211 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 212 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+N"));
    }
#line 213 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 214 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+O"));
    }
#line 215 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 216 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+S"));
    }
#line 217 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 218 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+R"));
    }
#line 219 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(4)))) {
#line 220 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+Q"));
    }
  }
#line 221 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_EDIT))) {
#line 222 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 223 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+C"));
    }
#line 224 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 225 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+V"));
    }
#line 226 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 227 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+X"));
    }
#line 228 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 229 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Del"));
    }
  }
#line 230 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_SELECT))) {
#line 231 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 232 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+A"));
    }
#line 233 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 234 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+W"));
    }
#line 235 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 236 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+L"));
    }
#line 237 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 238 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Esc"));
    }
  }
#line 239 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FIND))) {
#line 240 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 241 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+F"));
    }
#line 242 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 243 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+G"));
    }
#line 244 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 245 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+Shift+G"));
    }
  }
#line 246 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FORMAT))) {
#line 247 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 248 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+T"));
    }
#line 249 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 250 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+I"));
    }
#line 251 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 252 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Tab"));
    }
#line 253 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 254 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Shift+Tab"));
    }
  }
#line 255 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_VIEW))) {
#line 256 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 257 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+D"));
    }
#line 258 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 259 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("Cmd+H"));
    }
#line 260 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 261 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit(""));
    }
#line 262 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 263 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit(""));
    }
  }
#line 264 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_INSERT))) {
#line 265 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 266 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit(""));
    }
#line 267 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 268 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit(""));
    }
#line 269 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 270 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit(""));
    }
  }
#line 271 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit(""));
}

static YisVal yis_right_menu_item_action(YisVal v_cat, YisVal v_idx) {
#line 277 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FILE))) {
#line 278 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 279 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("new"));
    }
#line 280 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 281 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("open"));
    }
#line 282 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 283 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("save"));
    }
#line 284 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 285 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("rename"));
    }
#line 286 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(4)))) {
#line 287 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("exit"));
    }
  }
#line 288 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_EDIT))) {
#line 289 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 290 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("copy"));
    }
#line 291 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 292 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("paste"));
    }
#line 293 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 294 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("cut"));
    }
#line 295 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 296 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("delete"));
    }
  }
#line 297 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_SELECT))) {
#line 298 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 299 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("select_all"));
    }
#line 300 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 301 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("select_word"));
    }
#line 302 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 303 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("select_line"));
    }
#line 304 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 305 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("reset_selection"));
    }
  }
#line 306 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FIND))) {
#line 307 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 308 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("find"));
    }
#line 309 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 310 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("find_next"));
    }
#line 311 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 312 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("find_prev"));
    }
  }
#line 313 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_FORMAT))) {
#line 314 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 315 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("strip"));
    }
#line 316 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 317 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("trim"));
    }
#line 318 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 319 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("tab"));
    }
#line 320 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 321 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("untab"));
    }
  }
#line 322 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_VIEW))) {
#line 323 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 324 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("toggle_sidebar"));
    }
#line 325 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 326 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("toggle_highlight"));
    }
#line 327 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 328 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("toggle_line_endings"));
    }
#line 329 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(3)))) {
#line 330 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("toggle_tabs"));
    }
  }
#line 331 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_cat, v_CAT_INSERT))) {
#line 332 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(0)))) {
#line 333 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("insert_date"));
    }
#line 334 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(1)))) {
#line 335 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("insert_path"));
    }
#line 336 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_idx, YV_INT(2)))) {
#line 337 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit("insert_section"));
    }
  }
#line 338 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit(""));
}

static YisVal yis_right_menu_is_open(void) {
#line 344 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return yis_ge(v_menu_open, YV_INT(0));
}

static void yis_right_menu_close(void) {
#line 348 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_menu_open, yis_neg(YV_INT(1))), v_menu_open));
#line 349 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_menu_hover, yis_neg(YV_INT(1))), v_menu_hover));
}

static void yis_right_menu_draw_diamond(YisVal v_scr, YisVal v_dx, YisVal v_dy, YisVal v_color) {
#line 355 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_bg = yis_add(v_color, YV_INT(64)); yis_retain_val(v_bg);
#line 357 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(0))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(0))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 358 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(1))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(1))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 360 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(2))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(2))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 361 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(2))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(2))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 362 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(2))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(2))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 363 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(3))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(3))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 364 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(3))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(3))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 365 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(3))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(3))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 367 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(4))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(4))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(4))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(4))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 368 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(4))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(4))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 369 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(4))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(4))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 370 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(4))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(4))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 371 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(8))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(4))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(8))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(4))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 372 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(4))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(5))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(4))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(5))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 373 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(5))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(5))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 374 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(5))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(5))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 375 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(5))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(5))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 376 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(8))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(5))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(8))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(5))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 378 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(3))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(3))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 379 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(4))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(4))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 380 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 381 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 382 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 383 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(8))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(8))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 384 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(9))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(9))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(6))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 385 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(3))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(3))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 386 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(4))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(4))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 387 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 388 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 389 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 390 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(8))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(8))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 391 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(9))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(9))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(7))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 393 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(4))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(8))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(4))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(8))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 394 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(8))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(8))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 395 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(8))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(8))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 396 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(8))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(8))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 397 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(8))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(8))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(8))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(8))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 398 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(4))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(9))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(4))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(9))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 399 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(9))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(9))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 400 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(9))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(9))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 401 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(9))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(9))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 402 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(8))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(9))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(8))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(9))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 404 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(10))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(10))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 405 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(10))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(10))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 406 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(10))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(10))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 407 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(11))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(5))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(11))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 408 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(11))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(11))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 409 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(11))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(7))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(11))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 411 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(12))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(12))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 412 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(13))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_color));
  (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_dx, YV_INT(6))));
  (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_dy, YV_INT(13))));
  (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
}

static void yis_right_menu_draw_menubar(YisVal v_scr) {
#line 417 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_bx = YV_INT(0); yis_retain_val(v_bx);
#line 418 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_by = yis_sub(v_CONTENT_Y, YV_INT(1)); yis_retain_val(v_by);
#line 419 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  { YisVal v_col = YV_INT(0); yis_retain_val(v_col);
  for (; yis_as_bool(yis_lt(v_col, v_screen_w)); (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(2))), v_col))) {
#line 420 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_m_vimana_screen_set_x(v_scr, v_col));
#line 421 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_m_vimana_screen_set_y(v_scr, v_by));
#line 422 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_m_vimana_screen_pixel(v_scr, v_C_FG));
  } }
#line 425 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_dx = YV_INT(1); yis_retain_val(v_dx);
#line 426 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_dy = yis_add(v_MENUBAR_Y, YV_INT(6)); yis_retain_val(v_dy);
#line 427 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(v_dirty)) {
#line 429 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_fill(v_scr, YV_INT(0), YV_INT(0), YV_INT(14), yis_add(v_MENUBAR_H, YV_INT(3)), v_C_ACCENT));
#line 430 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_draw_diamond(v_scr, v_dx, v_dy, v_C_BG));
  } else {
#line 432 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_draw_diamond(v_scr, v_dx, v_dy, v_C_FG));
  }
#line 435 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_ty = yis_add(v_MENUBAR_Y, YV_INT(2)); yis_retain_val(v_ty);
#line 436 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_fg = v_C_FG; yis_retain_val(v_fg);
#line 437 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_bg = v_C_BG; yis_retain_val(v_bg);
#line 438 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_menu_open, v_CAT_FILE))) {
#line 439 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_fg, v_C_BG), v_fg));
#line 440 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_bg, v_C_FG), v_bg));
#line 441 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_fill(v_scr, v_CAT_FILE_X, v_MENUBAR_Y, v_CAT_FILE_W, v_MENUBAR_H, v_bg));
  }
#line 442 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, v_CAT_FILE_X, v_ty, YV_STR(stdr_str_lit("File")), v_fg, v_bg));
#line 444 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_fg, v_C_FG), v_fg));
#line 445 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_bg, v_C_BG), v_bg));
#line 446 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_menu_open, v_CAT_EDIT))) {
#line 447 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_fg, v_C_BG), v_fg));
#line 448 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_bg, v_C_FG), v_bg));
#line 449 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_fill(v_scr, v_CAT_EDIT_X, v_MENUBAR_Y, v_CAT_EDIT_W, v_MENUBAR_H, v_bg));
  }
#line 450 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, v_CAT_EDIT_X, v_ty, YV_STR(stdr_str_lit("Edit")), v_fg, v_bg));
#line 452 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_fg, v_C_FG), v_fg));
#line 453 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_bg, v_C_BG), v_bg));
#line 454 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_menu_open, v_CAT_SELECT))) {
#line 455 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_fg, v_C_BG), v_fg));
#line 456 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_bg, v_C_FG), v_bg));
#line 457 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_fill(v_scr, v_CAT_SELECT_X, v_MENUBAR_Y, v_CAT_SELECT_W, v_MENUBAR_H, v_bg));
  }
#line 458 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, v_CAT_SELECT_X, v_ty, YV_STR(stdr_str_lit("Select")), v_fg, v_bg));
#line 460 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_fg, v_C_FG), v_fg));
#line 461 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_bg, v_C_BG), v_bg));
#line 462 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_menu_open, v_CAT_FIND))) {
#line 463 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_fg, v_C_BG), v_fg));
#line 464 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_bg, v_C_FG), v_bg));
#line 465 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_fill(v_scr, v_CAT_FIND_X, v_MENUBAR_Y, v_CAT_FIND_W, v_MENUBAR_H, v_bg));
  }
#line 466 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, v_CAT_FIND_X, v_ty, YV_STR(stdr_str_lit("Find")), v_fg, v_bg));
#line 468 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_fg, v_C_FG), v_fg));
#line 469 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_bg, v_C_BG), v_bg));
#line 470 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_menu_open, v_CAT_FORMAT))) {
#line 471 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_fg, v_C_BG), v_fg));
#line 472 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_bg, v_C_FG), v_bg));
#line 473 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_fill(v_scr, v_CAT_FORMAT_X, v_MENUBAR_Y, v_CAT_FORMAT_W, v_MENUBAR_H, v_bg));
  }
#line 474 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, v_CAT_FORMAT_X, v_ty, YV_STR(stdr_str_lit("Format")), v_fg, v_bg));
#line 476 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_fg, v_C_FG), v_fg));
#line 477 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_bg, v_C_BG), v_bg));
#line 478 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_menu_open, v_CAT_VIEW))) {
#line 479 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_fg, v_C_BG), v_fg));
#line 480 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_bg, v_C_FG), v_bg));
#line 481 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_fill(v_scr, v_CAT_VIEW_X, v_MENUBAR_Y, v_CAT_VIEW_W, v_MENUBAR_H, v_bg));
  }
#line 482 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, v_CAT_VIEW_X, v_ty, YV_STR(stdr_str_lit("View")), v_fg, v_bg));
#line 484 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_fg, v_C_FG), v_fg));
#line 485 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_bg, v_C_BG), v_bg));
#line 486 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_eq(v_menu_open, v_CAT_INSERT))) {
#line 487 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_fg, v_C_BG), v_fg));
#line 488 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_bg, v_C_FG), v_bg));
#line 489 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_fill(v_scr, v_CAT_INSERT_X, v_MENUBAR_Y, v_CAT_INSERT_W, v_MENUBAR_H, v_bg));
  }
#line 490 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, v_CAT_INSERT_X, v_ty, YV_STR(stdr_str_lit("Insert")), v_fg, v_bg));
}

static YisVal yis_right_menu_draw_key_cap(YisVal v_scr, YisVal v_x, YisVal v_y, YisVal v_text, YisVal v_fg, YisVal v_bg) {
#line 495 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_tw = yis_mul(YV_INT(stdr_len(v_text)), v_FONT_W); yis_retain_val(v_tw);
#line 496 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_kw = yis_add(v_tw, YV_INT(6)); yis_retain_val(v_kw);
#line 497 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_kh = yis_sub(v_ROW_H, YV_INT(4)); yis_retain_val(v_kh);
#line 498 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_ky = yis_add(v_y, YV_INT(2)); yis_retain_val(v_ky);
#line 500 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, v_x, v_ky, v_kw, YV_INT(1), v_fg));
#line 501 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, v_x, yis_sub(yis_add(v_ky, v_kh), YV_INT(1)), v_kw, YV_INT(1), v_fg));
#line 502 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, v_x, v_ky, YV_INT(1), v_kh, v_fg));
#line 503 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, yis_sub(yis_add(v_x, v_kw), YV_INT(1)), v_ky, YV_INT(1), v_kh, v_fg));
#line 505 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, yis_add(v_x, YV_INT(1)), yis_add(v_ky, YV_INT(1)), yis_sub(v_kw, YV_INT(2)), yis_sub(v_kh, YV_INT(2)), v_bg));
#line 507 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, yis_add(v_x, YV_INT(3)), v_y, v_text, v_fg, v_bg));
#line 508 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return v_kw;
}

static void yis_right_menu_draw_key_shortcut(YisVal v_scr, YisVal v_rx, YisVal v_y, YisVal v_shortcut, YisVal v_fg, YisVal v_bg) {
#line 514 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_slen = YV_INT(stdr_len(v_shortcut)); yis_retain_val(v_slen);
#line 516 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_total_w = YV_INT(0); yis_retain_val(v_total_w);
#line 517 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_part_start = YV_INT(0); yis_retain_val(v_part_start);
#line 518 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_parts = YV_INT(0); yis_retain_val(v_parts);
#line 519 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  { YisVal v_k = YV_INT(0); yis_retain_val(v_k);
  for (; yis_as_bool(yis_le(v_k, v_slen)); (void)((yis_move_into(&v_k, yis_add(v_k, YV_INT(1))), v_k))) {
#line 520 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_at_end = yis_ge(v_k, v_slen); yis_retain_val(v_at_end);
#line 521 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_is_plus = YV_BOOL(yis_as_bool(YV_BOOL(!yis_as_bool(v_at_end))) && yis_as_bool(yis_eq(stdr_slice(v_shortcut, yis_as_int(v_k), yis_as_int(yis_add(v_k, YV_INT(1)))), YV_STR(stdr_str_lit("+"))))); yis_retain_val(v_is_plus);
#line 522 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(v_at_end) || yis_as_bool(v_is_plus)))) {
#line 523 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      YisVal v_part = stdr_slice(v_shortcut, yis_as_int(v_part_start), yis_as_int(v_k)); yis_retain_val(v_part);
#line 524 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      YisVal v_pw = yis_add(yis_mul(YV_INT(stdr_len(v_part)), v_FONT_W), YV_INT(6)); yis_retain_val(v_pw);
#line 525 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_total_w, yis_add(v_total_w, v_pw)), v_total_w));
#line 526 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      if (yis_as_bool(yis_gt(v_parts, YV_INT(0)))) {
#line 527 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
        (void)((yis_move_into(&v_total_w, yis_add(v_total_w, YV_INT(4))), v_total_w));
      }
#line 528 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_parts, yis_add(v_parts, YV_INT(1))), v_parts));
#line 529 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_part_start, yis_add(v_k, YV_INT(1))), v_part_start));
    }
  } }
#line 531 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_cx = yis_sub(v_rx, v_total_w); yis_retain_val(v_cx);
#line 532 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)((yis_move_into(&v_part_start, YV_INT(0)), v_part_start));
#line 533 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_drawn = YV_INT(0); yis_retain_val(v_drawn);
#line 534 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  { YisVal v_k = YV_INT(0); yis_retain_val(v_k);
  for (; yis_as_bool(yis_le(v_k, v_slen)); (void)((yis_move_into(&v_k, yis_add(v_k, YV_INT(1))), v_k))) {
#line 535 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_at_end = yis_ge(v_k, v_slen); yis_retain_val(v_at_end);
#line 536 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_is_plus = YV_BOOL(yis_as_bool(YV_BOOL(!yis_as_bool(v_at_end))) && yis_as_bool(yis_eq(stdr_slice(v_shortcut, yis_as_int(v_k), yis_as_int(yis_add(v_k, YV_INT(1)))), YV_STR(stdr_str_lit("+"))))); yis_retain_val(v_is_plus);
#line 537 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(v_at_end) || yis_as_bool(v_is_plus)))) {
#line 538 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      YisVal v_part = stdr_slice(v_shortcut, yis_as_int(v_part_start), yis_as_int(v_k)); yis_retain_val(v_part);
#line 539 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      if (yis_as_bool(yis_gt(v_drawn, YV_INT(0)))) {
#line 540 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
        (void)((yis_move_into(&v_cx, yis_add(v_cx, YV_INT(4))), v_cx));
      }
#line 541 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      YisVal v_w = yis_right_menu_draw_key_cap(v_scr, v_cx, v_y, v_part, v_fg, v_bg); yis_retain_val(v_w);
#line 542 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_cx, yis_add(v_cx, v_w)), v_cx));
#line 543 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_drawn, yis_add(v_drawn, YV_INT(1))), v_drawn));
#line 544 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_part_start, yis_add(v_k, YV_INT(1))), v_part_start));
    }
  } }
}

static void yis_right_menu_draw_submenu(YisVal v_scr) {
#line 548 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_lt(v_menu_open, YV_INT(0)))) {
#line 549 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return;
  }
#line 551 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_cat = v_menu_open; yis_retain_val(v_cat);
#line 552 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_count = yis_right_menu_item_count(v_cat); yis_retain_val(v_count);
#line 553 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sx = yis_right_menu_cat_x(v_cat); yis_retain_val(v_sx);
#line 554 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sy = v_MENUBAR_H; yis_retain_val(v_sy);
#line 555 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sw = yis_right_menu_submenu_width(v_cat); yis_retain_val(v_sw);
#line 556 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sh = yis_mul(v_count, v_SUBMENU_ITEM_H); yis_retain_val(v_sh);
#line 557 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_th = yis_add(v_sh, YV_INT(1)); yis_retain_val(v_th);
#line 560 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, v_sx, v_sy, yis_add(v_sw, YV_INT(1)), v_th, v_C_FG));
#line 563 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, v_sx, yis_sub(yis_add(v_sy, v_th), YV_INT(1)), yis_add(v_sw, YV_INT(1)), YV_INT(1), v_C_BG));
#line 564 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, v_sx, v_sy, YV_INT(1), v_th, v_C_BG));
#line 565 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  (void)(yis_right_menu_fill(v_scr, yis_add(v_sx, v_sw), v_sy, YV_INT(1), v_th, v_C_BG));
#line 568 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_count)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 569 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_iy = yis_add(v_sy, yis_mul(v_i, v_SUBMENU_ITEM_H)); yis_retain_val(v_iy);
#line 570 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_label = yis_right_menu_item_label(v_cat, v_i); yis_retain_val(v_label);
#line 571 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_shortcut = yis_right_menu_item_shortcut(v_cat, v_i); yis_retain_val(v_shortcut);
#line 572 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_bg_c = v_C_FG; yis_retain_val(v_bg_c);
#line 573 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_fg_c = v_C_BG; yis_retain_val(v_fg_c);
#line 574 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_sc_c = v_C_SEL; yis_retain_val(v_sc_c);
#line 576 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_eq(v_i, v_menu_hover))) {
#line 577 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_bg_c, v_C_ACCENT), v_bg_c));
#line 578 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_fg_c, v_C_FG), v_fg_c));
#line 579 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_sc_c, v_C_FG), v_sc_c));
#line 580 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)(yis_right_menu_fill(v_scr, yis_add(v_sx, YV_INT(1)), v_iy, yis_sub(v_sw, YV_INT(2)), v_SUBMENU_ITEM_H, v_bg_c));
    }
#line 582 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_m_vimana_screen_put_text(v_scr, yis_add(v_sx, YV_INT(8)), v_iy, v_label, v_fg_c, v_bg_c));
#line 585 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_shortcut)), YV_INT(0)))) {
#line 586 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)(yis_right_menu_draw_key_shortcut(v_scr, yis_sub(yis_add(v_sx, v_sw), YV_INT(8)), v_iy, v_shortcut, v_sc_c, v_bg_c));
    }
  } }
}

static YisVal yis_right_menu_cat_from_x(YisVal v_mx) {
#line 592 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_CAT_FILE_X)) && yis_as_bool(yis_lt(v_mx, yis_add(v_CAT_FILE_X, v_CAT_FILE_W)))))) {
#line 593 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_FILE;
  }
#line 594 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_CAT_EDIT_X)) && yis_as_bool(yis_lt(v_mx, yis_add(v_CAT_EDIT_X, v_CAT_EDIT_W)))))) {
#line 595 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_EDIT;
  }
#line 596 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_CAT_SELECT_X)) && yis_as_bool(yis_lt(v_mx, yis_add(v_CAT_SELECT_X, v_CAT_SELECT_W)))))) {
#line 597 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_SELECT;
  }
#line 598 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_CAT_FIND_X)) && yis_as_bool(yis_lt(v_mx, yis_add(v_CAT_FIND_X, v_CAT_FIND_W)))))) {
#line 599 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_FIND;
  }
#line 600 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_CAT_FORMAT_X)) && yis_as_bool(yis_lt(v_mx, yis_add(v_CAT_FORMAT_X, v_CAT_FORMAT_W)))))) {
#line 601 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_FORMAT;
  }
#line 602 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_CAT_VIEW_X)) && yis_as_bool(yis_lt(v_mx, yis_add(v_CAT_VIEW_X, v_CAT_VIEW_W)))))) {
#line 603 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_VIEW;
  }
#line 604 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_CAT_INSERT_X)) && yis_as_bool(yis_lt(v_mx, yis_add(v_CAT_INSERT_X, v_CAT_INSERT_W)))))) {
#line 605 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return v_CAT_INSERT;
  }
#line 606 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return yis_neg(YV_INT(1));
}

static YisVal yis_right_menu_handle_click(YisVal v_mx, YisVal v_my) {
#line 611 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_my, v_MENUBAR_Y)) && yis_as_bool(yis_lt(v_my, v_CONTENT_Y))))) {
#line 612 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_cat = yis_right_menu_cat_from_x(v_mx); yis_retain_val(v_cat);
#line 613 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(yis_ge(v_cat, YV_INT(0)))) {
#line 614 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      if (yis_as_bool(yis_eq(v_menu_open, v_cat))) {
#line 615 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
        (void)(yis_right_menu_close());
      } else {
#line 617 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
        (void)((yis_move_into(&v_menu_open, v_cat), v_menu_open));
#line 618 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
        (void)((yis_move_into(&v_menu_hover, yis_neg(YV_INT(1))), v_menu_hover));
      }
#line 619 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return YV_STR(stdr_str_lit(""));
    }
#line 620 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_close());
#line 621 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return YV_STR(stdr_str_lit(""));
  }
#line 624 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_ge(v_menu_open, YV_INT(0)))) {
#line 625 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_cat = v_menu_open; yis_retain_val(v_cat);
#line 626 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_sx = yis_right_menu_cat_x(v_cat); yis_retain_val(v_sx);
#line 627 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_sy = v_MENUBAR_H; yis_retain_val(v_sy);
#line 628 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_sw = yis_right_menu_submenu_width(v_cat); yis_retain_val(v_sw);
#line 629 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_count = yis_right_menu_item_count(v_cat); yis_retain_val(v_count);
#line 630 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_sh = yis_mul(v_count, v_SUBMENU_ITEM_H); yis_retain_val(v_sh);
#line 632 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_sx)) && yis_as_bool(yis_lt(v_mx, yis_add(v_sx, v_sw))))) && yis_as_bool(yis_ge(v_my, v_sy)))) && yis_as_bool(yis_lt(v_my, yis_add(v_sy, v_sh)))))) {
#line 633 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      YisVal v_idx = yis_stdr_floor(yis_div(yis_sub(v_my, v_sy), v_SUBMENU_ITEM_H)); yis_retain_val(v_idx);
#line 634 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      YisVal v_action = yis_right_menu_item_action(v_cat, v_idx); yis_retain_val(v_action);
#line 635 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)(yis_right_menu_close());
#line 636 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      return v_action;
    }
#line 639 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)(yis_right_menu_close());
#line 640 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return YV_STR(stdr_str_lit(""));
  }
#line 642 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  return YV_STR(stdr_str_lit("pass"));
}

static void yis_right_menu_handle_hover(YisVal v_mx, YisVal v_my) {
#line 646 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(yis_lt(v_menu_open, YV_INT(0)))) {
#line 647 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return;
  }
#line 650 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_my, v_MENUBAR_Y)) && yis_as_bool(yis_lt(v_my, v_CONTENT_Y))))) {
#line 651 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    YisVal v_cat = yis_right_menu_cat_from_x(v_mx); yis_retain_val(v_cat);
#line 652 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_cat, YV_INT(0))) && yis_as_bool(yis_ne(v_cat, v_menu_open))))) {
#line 653 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_menu_open, v_cat), v_menu_open));
#line 654 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
      (void)((yis_move_into(&v_menu_hover, yis_neg(YV_INT(1))), v_menu_hover));
    }
#line 655 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    return;
  }
#line 658 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_cat = v_menu_open; yis_retain_val(v_cat);
#line 659 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sx = yis_right_menu_cat_x(v_cat); yis_retain_val(v_sx);
#line 660 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sy = v_MENUBAR_H; yis_retain_val(v_sy);
#line 661 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sw = yis_right_menu_submenu_width(v_cat); yis_retain_val(v_sw);
#line 662 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_count = yis_right_menu_item_count(v_cat); yis_retain_val(v_count);
#line 663 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  YisVal v_sh = yis_mul(v_count, v_SUBMENU_ITEM_H); yis_retain_val(v_sh);
#line 665 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, v_sx)) && yis_as_bool(yis_lt(v_mx, yis_add(v_sx, v_sw))))) && yis_as_bool(yis_ge(v_my, v_sy)))) && yis_as_bool(yis_lt(v_my, yis_add(v_sy, v_sh)))))) {
#line 666 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
    (void)((yis_move_into(&v_menu_hover, yis_stdr_floor(yis_div(yis_sub(v_my, v_sy), v_SUBMENU_ITEM_H))), v_menu_hover));
  } else {
#line 668 "/Users/nayu/Developer/Cogito/extras/Right/right_menu.yi"
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

static YisVal __fnwrap_right_menu_shortcut_px(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_menu_shortcut_px(__a0);
}

static YisVal __fnwrap_right_menu_submenu_width(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_menu_submenu_width(__a0);
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

static YisVal __fnwrap_right_menu_draw_diamond(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  yis_right_menu_draw_diamond(__a0, __a1, __a2, __a3);
  return YV_NULLV;
}

static YisVal __fnwrap_right_menu_draw_menubar(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_menu_draw_menubar(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_menu_draw_key_cap(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  YisVal __a4 = argc > 4 ? argv[4] : YV_NULLV;
  YisVal __a5 = argc > 5 ? argv[5] : YV_NULLV;
  return yis_right_menu_draw_key_cap(__a0, __a1, __a2, __a3, __a4, __a5);
}

static YisVal __fnwrap_right_menu_draw_key_shortcut(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  YisVal __a4 = argc > 4 ? argv[4] : YV_NULLV;
  YisVal __a5 = argc > 5 ? argv[5] : YV_NULLV;
  yis_right_menu_draw_key_shortcut(__a0, __a1, __a2, __a3, __a4, __a5);
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
  yis_move_into(&v_SUBMENU_PAD, YV_INT(16));
  yis_move_into(&v_SUBMENU_ITEM_H, YV_INT(24));
  yis_move_into(&v_CAT_FILE_X, YV_INT(16));
  yis_move_into(&v_CAT_FILE_W, YV_INT(80));
  yis_move_into(&v_CAT_EDIT_X, YV_INT(96));
  yis_move_into(&v_CAT_EDIT_W, YV_INT(80));
  yis_move_into(&v_CAT_SELECT_X, YV_INT(176));
  yis_move_into(&v_CAT_SELECT_W, YV_INT(112));
  yis_move_into(&v_CAT_FIND_X, YV_INT(288));
  yis_move_into(&v_CAT_FIND_W, YV_INT(80));
  yis_move_into(&v_CAT_FORMAT_X, YV_INT(368));
  yis_move_into(&v_CAT_FORMAT_W, YV_INT(112));
  yis_move_into(&v_CAT_VIEW_X, YV_INT(480));
  yis_move_into(&v_CAT_VIEW_W, YV_INT(80));
  yis_move_into(&v_CAT_INSERT_X, YV_INT(560));
  yis_move_into(&v_CAT_INSERT_W, YV_INT(112));
  yis_move_into(&v_CAT_FILE, YV_INT(0));
  yis_move_into(&v_CAT_EDIT, YV_INT(1));
  yis_move_into(&v_CAT_SELECT, YV_INT(2));
  yis_move_into(&v_CAT_FIND, YV_INT(3));
  yis_move_into(&v_CAT_FORMAT, YV_INT(4));
  yis_move_into(&v_CAT_VIEW, YV_INT(5));
  yis_move_into(&v_CAT_INSERT, YV_INT(6));
  yis_move_into(&v_FILE_COUNT, YV_INT(5));
  yis_move_into(&v_EDIT_COUNT, YV_INT(4));
  yis_move_into(&v_SELECT_COUNT, YV_INT(4));
  yis_move_into(&v_FIND_COUNT, YV_INT(3));
  yis_move_into(&v_FORMAT_COUNT, YV_INT(4));
  yis_move_into(&v_VIEW_COUNT, YV_INT(4));
  yis_move_into(&v_INSERT_COUNT, YV_INT(3));
}

/* end embedded module: right_menu */

/* begin embedded module: right_syntax */
static YisVal yis_right_syntax_is_alpha(YisVal);
static YisVal yis_right_syntax_is_digit(YisVal);
static YisVal yis_right_syntax_is_alnum(YisVal);
static YisVal yis_right_syntax_is_keyword(YisVal);
static YisVal yis_right_syntax_is_type(YisVal);
static YisVal yis_right_syntax_is_constant(YisVal);
static void yis_right_syntax_fill_colors(YisVal, YisVal, YisVal, YisVal);
static YisVal yis_right_syntax_highlight_line(YisVal);
static YisVal yis_right_syntax_line_color(YisVal);
static YisVal __fnwrap_right_syntax_is_alpha(void*,int,YisVal*);
static YisVal __fnwrap_right_syntax_is_digit(void*,int,YisVal*);
static YisVal __fnwrap_right_syntax_is_alnum(void*,int,YisVal*);
static YisVal __fnwrap_right_syntax_is_keyword(void*,int,YisVal*);
static YisVal __fnwrap_right_syntax_is_type(void*,int,YisVal*);
static YisVal __fnwrap_right_syntax_is_constant(void*,int,YisVal*);
static YisVal __fnwrap_right_syntax_fill_colors(void*,int,YisVal*);
static YisVal __fnwrap_right_syntax_highlight_line(void*,int,YisVal*);
static YisVal __fnwrap_right_syntax_line_color(void*,int,YisVal*);
static YisVal v_COL_FG = YV_NULLV;
static YisVal v_COL_KW = YV_NULLV;
static YisVal v_COL_CM = YV_NULLV;
static YisVal v_COL_ST = YV_NULLV;
static YisVal v_COL_NM = YV_NULLV;

// cask right_syntax
// bring stdr
// bring right_model
static YisVal yis_right_syntax_is_alpha(YisVal v_ch) {
#line 17 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  YisVal v_c = YV_INT(stdr_char_code(v_ch)); yis_retain_val(v_c);
#line 18 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_c, YV_INT(65))) && yis_as_bool(yis_le(v_c, YV_INT(90)))))) {
#line 19 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return YV_BOOL(true);
  }
#line 20 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_c, YV_INT(97))) && yis_as_bool(yis_le(v_c, YV_INT(122)))))) {
#line 21 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return YV_BOOL(true);
  }
#line 22 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  return yis_eq(v_ch, YV_STR(stdr_str_lit("_")));
}

static YisVal yis_right_syntax_is_digit(YisVal v_ch) {
#line 26 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  YisVal v_c = YV_INT(stdr_char_code(v_ch)); yis_retain_val(v_c);
#line 27 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  return YV_BOOL(yis_as_bool(yis_ge(v_c, YV_INT(48))) && yis_as_bool(yis_le(v_c, YV_INT(57))));
}

static YisVal yis_right_syntax_is_alnum(YisVal v_ch) {
#line 31 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  return YV_BOOL(yis_as_bool(yis_right_syntax_is_alpha(v_ch)) || yis_as_bool(yis_right_syntax_is_digit(v_ch)));
}

static YisVal yis_right_syntax_is_keyword(YisVal v_w) {
#line 35 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("if")))) || yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("elif")))))) || yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("else"))))))) {
#line 36 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return YV_BOOL(true);
  }
#line 37 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("for")))) || yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("break")))))) || yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("continue"))))))) {
#line 38 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return YV_BOOL(true);
  }
#line 39 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("cask")))) || yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("bring"))))))) {
#line 40 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return YV_BOOL(true);
  }
#line 41 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("def")))) || yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("let")))))) || yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("pub"))))))) {
#line 42 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return YV_BOOL(true);
  }
#line 43 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("this")))) || yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("struct"))))))) {
#line 44 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return YV_BOOL(true);
  }
#line 45 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  return YV_BOOL(false);
}

static YisVal yis_right_syntax_is_type(YisVal v_w) {
#line 49 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  return YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("num")))) || yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("string")))))) || yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("bool")))))) || yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("any")))));
}

static YisVal yis_right_syntax_is_constant(YisVal v_w) {
#line 53 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("true")))) || yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("false")))))) || yis_as_bool(yis_eq(v_w, YV_STR(stdr_str_lit("null"))))))) {
#line 54 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return YV_BOOL(true);
  }
#line 56 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  YisVal v_n = YV_INT(stdr_len(v_w)); yis_retain_val(v_n);
#line 57 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(yis_lt(v_n, YV_INT(2)))) {
#line 58 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return YV_BOOL(false);
  }
#line 59 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  YisVal v_all_upper = YV_BOOL(true); yis_retain_val(v_all_upper);
#line 60 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_n)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 61 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    YisVal v_c = stdr_slice(v_w, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_c);
#line 62 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    YisVal v_code = YV_INT(stdr_char_code(v_c)); yis_retain_val(v_code);
#line 63 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_code, YV_INT(65))) && yis_as_bool(yis_le(v_code, YV_INT(90))))) || yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_code, YV_INT(48))) && yis_as_bool(yis_le(v_code, YV_INT(57))))))) || yis_as_bool(yis_eq(v_c, YV_STR(stdr_str_lit("_"))))))) {
#line 64 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      continue;
    }
#line 65 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    (void)((yis_move_into(&v_all_upper, YV_BOOL(false)), v_all_upper));
#line 66 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    break;
  } }
#line 68 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(v_all_upper)) {
#line 69 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    YisVal v_first = YV_INT(stdr_char_code(stdr_slice(v_w, yis_as_int(YV_INT(0)), yis_as_int(YV_INT(1))))); yis_retain_val(v_first);
#line 70 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_first, YV_INT(65))) && yis_as_bool(yis_le(v_first, YV_INT(90)))))) {
#line 71 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      return YV_BOOL(true);
    }
  }
#line 72 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  return YV_BOOL(false);
}

static void yis_right_syntax_fill_colors(YisVal v_colors, YisVal v_from, YisVal v_to, YisVal v_c) {
#line 76 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  { YisVal v_i = v_from; yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_to)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 77 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    (void)(yis_index_set(v_colors, v_i, v_c));
  } }
}

static YisVal yis_right_syntax_highlight_line(YisVal v_line) {
#line 83 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  YisVal v_n = YV_INT(stdr_len(v_line)); yis_retain_val(v_n);
#line 84 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  YisVal v_colors = yis_arr_lit(0); yis_retain_val(v_colors);
#line 86 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  { YisVal v_k = YV_INT(0); yis_retain_val(v_k);
  for (; yis_as_bool(yis_lt(v_k, v_n)); (void)((yis_move_into(&v_k, yis_add(v_k, YV_INT(1))), v_k))) {
#line 87 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    (void)(stdr_push(v_colors, v_COL_FG));
  } }
#line 88 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(yis_eq(v_n, YV_INT(0)))) {
#line 89 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_colors;
  }
#line 91 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  YisVal v_pos = YV_INT(0); yis_retain_val(v_pos);
#line 92 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  for (; yis_as_bool(yis_lt(v_pos, v_n));) {
#line 93 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    YisVal v_ch = stdr_slice(v_line, yis_as_int(v_pos), yis_as_int(yis_add(v_pos, YV_INT(1)))); yis_retain_val(v_ch);
#line 96 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("-")))) && yis_as_bool(yis_lt(yis_add(v_pos, YV_INT(1)), v_n)))) && yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(yis_add(v_pos, YV_INT(1))), yis_as_int(yis_add(v_pos, YV_INT(2)))), YV_STR(stdr_str_lit("-"))))))) {
#line 97 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      if (yis_as_bool(YV_BOOL(yis_as_bool(yis_lt(yis_add(v_pos, YV_INT(2)), v_n)) && yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(yis_add(v_pos, YV_INT(2))), yis_as_int(yis_add(v_pos, YV_INT(3)))), YV_STR(stdr_str_lit("|"))))))) {
#line 98 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        (void)(yis_right_syntax_fill_colors(v_colors, v_pos, v_n, v_COL_KW));
#line 99 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        return v_colors;
      }
#line 100 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)(yis_right_syntax_fill_colors(v_colors, v_pos, v_n, v_COL_CM));
#line 101 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      return v_colors;
    }
#line 104 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\""))))) {
#line 105 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      YisVal v_end = yis_add(v_pos, YV_INT(1)); yis_retain_val(v_end);
#line 106 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      for (; yis_as_bool(yis_lt(v_end, v_n));) {
#line 107 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        YisVal v_sc = stdr_slice(v_line, yis_as_int(v_end), yis_as_int(yis_add(v_end, YV_INT(1)))); yis_retain_val(v_sc);
#line 108 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        if (yis_as_bool(yis_eq(v_sc, YV_STR(stdr_str_lit("\\"))))) {
#line 109 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          (void)((yis_move_into(&v_end, yis_add(v_end, YV_INT(2))), v_end));
#line 110 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          continue;
        }
#line 111 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        if (yis_as_bool(yis_eq(v_sc, YV_STR(stdr_str_lit("\""))))) {
#line 112 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          (void)((yis_move_into(&v_end, yis_add(v_end, YV_INT(1))), v_end));
#line 113 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          break;
        }
#line 114 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        (void)((yis_move_into(&v_end, yis_add(v_end, YV_INT(1))), v_end));
      }
#line 115 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)(yis_right_syntax_fill_colors(v_colors, v_pos, v_end, v_COL_ST));
#line 117 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      YisVal v_si = yis_add(v_pos, YV_INT(1)); yis_retain_val(v_si);
#line 118 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      for (; yis_as_bool(yis_lt(v_si, yis_sub(v_end, YV_INT(2))));) {
#line 119 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(v_si), yis_as_int(yis_add(v_si, YV_INT(1)))), YV_STR(stdr_str_lit("$")))) && yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(yis_add(v_si, YV_INT(1))), yis_as_int(yis_add(v_si, YV_INT(2)))), YV_STR(stdr_str_lit("$"))))))) {
#line 120 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          YisVal v_ci = yis_add(v_si, YV_INT(2)); yis_retain_val(v_ci);
#line 121 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          for (; yis_as_bool(yis_lt(v_ci, yis_sub(v_end, YV_INT(1))));) {
#line 122 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
            if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(v_ci), yis_as_int(yis_add(v_ci, YV_INT(1)))), YV_STR(stdr_str_lit("$")))) && yis_as_bool(yis_lt(yis_add(v_ci, YV_INT(1)), v_end)))) && yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(yis_add(v_ci, YV_INT(1))), yis_as_int(yis_add(v_ci, YV_INT(2)))), YV_STR(stdr_str_lit("$"))))))) {
#line 123 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
              (void)(yis_right_syntax_fill_colors(v_colors, v_si, yis_add(v_si, YV_INT(2)), v_COL_KW));
#line 124 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
              (void)(yis_right_syntax_fill_colors(v_colors, yis_add(v_si, YV_INT(2)), v_ci, v_COL_FG));
#line 125 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
              (void)(yis_right_syntax_fill_colors(v_colors, v_ci, yis_add(v_ci, YV_INT(2)), v_COL_KW));
#line 126 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
              (void)((yis_move_into(&v_si, yis_add(v_ci, YV_INT(2))), v_si));
#line 127 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
              break;
            }
#line 128 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
            (void)((yis_move_into(&v_ci, yis_add(v_ci, YV_INT(1))), v_ci));
          }
#line 129 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          if (yis_as_bool(yis_ge(v_ci, yis_sub(v_end, YV_INT(1))))) {
#line 130 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
            (void)((yis_move_into(&v_si, yis_add(v_si, YV_INT(1))), v_si));
          }
        } else {
#line 132 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          (void)((yis_move_into(&v_si, yis_add(v_si, YV_INT(1))), v_si));
        }
      }
#line 133 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)((yis_move_into(&v_pos, v_end), v_pos));
#line 134 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      continue;
    }
#line 136 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("@")))) && yis_as_bool(yis_lt(yis_add(v_pos, YV_INT(1)), v_n)))) && yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(yis_add(v_pos, YV_INT(1))), yis_as_int(yis_add(v_pos, YV_INT(2)))), YV_STR(stdr_str_lit("\""))))))) {
#line 137 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      YisVal v_end = yis_add(v_pos, YV_INT(2)); yis_retain_val(v_end);
#line 138 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      for (; yis_as_bool(yis_lt(v_end, v_n));) {
#line 139 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        YisVal v_sc = stdr_slice(v_line, yis_as_int(v_end), yis_as_int(yis_add(v_end, YV_INT(1)))); yis_retain_val(v_sc);
#line 140 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        if (yis_as_bool(yis_eq(v_sc, YV_STR(stdr_str_lit("\\"))))) {
#line 141 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          (void)((yis_move_into(&v_end, yis_add(v_end, YV_INT(2))), v_end));
#line 142 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          continue;
        }
#line 143 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        if (yis_as_bool(yis_eq(v_sc, YV_STR(stdr_str_lit("\""))))) {
#line 144 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          (void)((yis_move_into(&v_end, yis_add(v_end, YV_INT(1))), v_end));
#line 145 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          break;
        }
#line 146 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        (void)((yis_move_into(&v_end, yis_add(v_end, YV_INT(1))), v_end));
      }
#line 147 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)(yis_right_syntax_fill_colors(v_colors, v_pos, v_end, v_COL_ST));
#line 148 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)((yis_move_into(&v_pos, v_end), v_pos));
#line 149 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      continue;
    }
#line 152 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(yis_right_syntax_is_digit(v_ch))) {
#line 153 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      YisVal v_end = yis_add(v_pos, YV_INT(1)); yis_retain_val(v_end);
#line 154 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      for (; yis_as_bool(YV_BOOL(yis_as_bool(yis_lt(v_end, v_n)) && yis_as_bool(YV_BOOL(yis_as_bool(yis_right_syntax_is_digit(stdr_slice(v_line, yis_as_int(v_end), yis_as_int(yis_add(v_end, YV_INT(1)))))) || yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(v_end), yis_as_int(yis_add(v_end, YV_INT(1)))), YV_STR(stdr_str_lit(".")))))))); (void)((yis_move_into(&v_end, yis_add(v_end, YV_INT(1))), v_end))) {
      }
#line 156 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)(yis_right_syntax_fill_colors(v_colors, v_pos, v_end, v_COL_NM));
#line 157 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)((yis_move_into(&v_pos, v_end), v_pos));
#line 158 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      continue;
    }
#line 161 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("<")))) && yis_as_bool(yis_lt(yis_add(v_pos, YV_INT(1)), v_n)))) && yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(yis_add(v_pos, YV_INT(1))), yis_as_int(yis_add(v_pos, YV_INT(2)))), YV_STR(stdr_str_lit("-"))))))) {
#line 162 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)(yis_right_syntax_fill_colors(v_colors, v_pos, yis_add(v_pos, YV_INT(2)), v_COL_KW));
#line 163 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)((yis_move_into(&v_pos, yis_add(v_pos, YV_INT(2))), v_pos));
#line 164 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      continue;
    }
#line 165 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("-")))) && yis_as_bool(yis_lt(yis_add(v_pos, YV_INT(1)), v_n)))) && yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(yis_add(v_pos, YV_INT(1))), yis_as_int(yis_add(v_pos, YV_INT(2)))), YV_STR(stdr_str_lit(">"))))))) {
#line 166 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)(yis_right_syntax_fill_colors(v_colors, v_pos, yis_add(v_pos, YV_INT(2)), v_COL_KW));
#line 167 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)((yis_move_into(&v_pos, yis_add(v_pos, YV_INT(2))), v_pos));
#line 168 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      continue;
    }
#line 171 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("(")))) && yis_as_bool(yis_lt(yis_add(v_pos, YV_INT(1)), v_n)))) && yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(yis_add(v_pos, YV_INT(1))), yis_as_int(yis_add(v_pos, YV_INT(2)))), YV_STR(stdr_str_lit("("))))))) {
#line 172 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      YisVal v_end = yis_add(v_pos, YV_INT(2)); yis_retain_val(v_end);
#line 173 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      YisVal v_depth = YV_INT(1); yis_retain_val(v_depth);
#line 174 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      for (; yis_as_bool(YV_BOOL(yis_as_bool(yis_lt(v_end, v_n)) && yis_as_bool(yis_gt(v_depth, YV_INT(0)))));) {
#line 175 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        YisVal v_rc = stdr_slice(v_line, yis_as_int(v_end), yis_as_int(yis_add(v_end, YV_INT(1)))); yis_retain_val(v_rc);
#line 176 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_rc, YV_STR(stdr_str_lit(")")))) && yis_as_bool(yis_lt(yis_add(v_end, YV_INT(1)), v_n)))) && yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(yis_add(v_end, YV_INT(1))), yis_as_int(yis_add(v_end, YV_INT(2)))), YV_STR(stdr_str_lit(")"))))))) {
#line 177 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          (void)((yis_move_into(&v_depth, yis_sub(v_depth, YV_INT(1))), v_depth));
#line 178 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          (void)((yis_move_into(&v_end, yis_add(v_end, YV_INT(2))), v_end));
        } else {
#line 180 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
          (void)((yis_move_into(&v_end, yis_add(v_end, YV_INT(1))), v_end));
        }
      }
#line 181 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)(yis_right_syntax_fill_colors(v_colors, v_pos, v_end, v_COL_CM));
#line 182 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)((yis_move_into(&v_pos, v_end), v_pos));
#line 183 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      continue;
    }
#line 186 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_right_syntax_is_alpha(v_ch)) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("?"))))))) {
#line 187 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      YisVal v_end = v_pos; yis_retain_val(v_end);
#line 188 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("?"))))) {
#line 189 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        (void)((yis_move_into(&v_end, yis_add(v_pos, YV_INT(1))), v_end));
      }
#line 190 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      for (; yis_as_bool(YV_BOOL(yis_as_bool(yis_lt(v_end, v_n)) && yis_as_bool(yis_right_syntax_is_alnum(stdr_slice(v_line, yis_as_int(v_end), yis_as_int(yis_add(v_end, YV_INT(1)))))))); (void)((yis_move_into(&v_end, yis_add(v_end, YV_INT(1))), v_end))) {
      }
#line 192 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      YisVal v_word = stdr_slice(v_line, yis_as_int(v_pos), yis_as_int(v_end)); yis_retain_val(v_word);
#line 194 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("?"))))) {
#line 195 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        (void)((yis_move_into(&v_pos, v_end), v_pos));
#line 196 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        continue;
      }
#line 197 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      if (yis_as_bool(yis_right_syntax_is_keyword(v_word))) {
#line 198 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        (void)(yis_right_syntax_fill_colors(v_colors, v_pos, v_end, v_COL_KW));
      } else if (yis_as_bool(yis_right_syntax_is_type(v_word))) {
#line 200 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        (void)(yis_right_syntax_fill_colors(v_colors, v_pos, v_end, v_COL_KW));
      } else if (yis_as_bool(yis_right_syntax_is_constant(v_word))) {
#line 202 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
        (void)(yis_right_syntax_fill_colors(v_colors, v_pos, v_end, v_COL_KW));
      }


#line 203 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)((yis_move_into(&v_pos, v_end), v_pos));
#line 204 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      continue;
    }
#line 207 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(":")))) && yis_as_bool(yis_lt(yis_add(v_pos, YV_INT(1)), v_n)))) && yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(yis_add(v_pos, YV_INT(1))), yis_as_int(yis_add(v_pos, YV_INT(2)))), YV_STR(stdr_str_lit(":"))))))) {
#line 208 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)(yis_right_syntax_fill_colors(v_colors, v_pos, yis_add(v_pos, YV_INT(2)), v_COL_KW));
#line 209 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)((yis_move_into(&v_pos, yis_add(v_pos, YV_INT(2))), v_pos));
#line 210 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      continue;
    }
#line 211 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("!")))) && yis_as_bool(yis_lt(yis_add(v_pos, YV_INT(1)), v_n)))) && yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(yis_add(v_pos, YV_INT(1))), yis_as_int(yis_add(v_pos, YV_INT(2)))), YV_STR(stdr_str_lit(":"))))))) {
#line 212 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)(yis_right_syntax_fill_colors(v_colors, v_pos, yis_add(v_pos, YV_INT(2)), v_COL_KW));
#line 213 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)((yis_move_into(&v_pos, yis_add(v_pos, YV_INT(2))), v_pos));
#line 214 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      continue;
    }
#line 215 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(",")))) && yis_as_bool(yis_lt(yis_add(v_pos, YV_INT(1)), v_n)))) && yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(yis_add(v_pos, YV_INT(1))), yis_as_int(yis_add(v_pos, YV_INT(2)))), YV_STR(stdr_str_lit(":"))))))) {
#line 216 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)(yis_right_syntax_fill_colors(v_colors, v_pos, yis_add(v_pos, YV_INT(2)), v_COL_KW));
#line 217 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)((yis_move_into(&v_pos, yis_add(v_pos, YV_INT(2))), v_pos));
#line 218 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      continue;
    }
#line 220 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(";"))))) {
#line 221 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)(yis_right_syntax_fill_colors(v_colors, v_pos, yis_add(v_pos, YV_INT(1)), v_COL_CM));
#line 222 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)((yis_move_into(&v_pos, yis_add(v_pos, YV_INT(1))), v_pos));
#line 223 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      continue;
    }
#line 226 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("?")))) && yis_as_bool(yis_lt(yis_add(v_pos, YV_INT(1)), v_n)))) && yis_as_bool(yis_eq(stdr_slice(v_line, yis_as_int(yis_add(v_pos, YV_INT(1))), yis_as_int(yis_add(v_pos, YV_INT(2)))), YV_STR(stdr_str_lit("?"))))))) {
#line 227 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)(yis_right_syntax_fill_colors(v_colors, v_pos, yis_add(v_pos, YV_INT(2)), v_COL_KW));
#line 228 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      (void)((yis_move_into(&v_pos, yis_add(v_pos, YV_INT(2))), v_pos));
#line 229 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
      continue;
    }
#line 232 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    (void)((yis_move_into(&v_pos, yis_add(v_pos, YV_INT(1))), v_pos));
  }
#line 234 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  return v_colors;
}

static YisVal yis_right_syntax_line_color(YisVal v_line) {
#line 240 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  YisVal v_t = yis_stdr_trim(v_line); yis_retain_val(v_t);
#line 241 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  YisVal v_n = YV_INT(stdr_len(v_t)); yis_retain_val(v_n);
#line 242 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(yis_eq(v_n, YV_INT(0)))) {
#line 243 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_C_FG;
  }
#line 244 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit("--| "))))) {
#line 245 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 246 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit("-- "))))) {
#line 247 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_COMMENT;
  }
#line 248 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(yis_eq(v_t, YV_STR(stdr_str_lit("--"))))) {
#line 249 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_COMMENT;
  }
#line 250 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit("cask ")))) || yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit("bring "))))))) {
#line 251 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 252 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit("def ")))) || yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit("let "))))))) {
#line 253 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 254 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit("if ")))) || yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit("elif ")))))) || yis_as_bool(yis_eq(v_t, YV_STR(stdr_str_lit("else"))))))) {
#line 255 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 256 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit("for ")))) || yis_as_bool(yis_eq(v_t, YV_STR(stdr_str_lit("break")))))) || yis_as_bool(yis_eq(v_t, YV_STR(stdr_str_lit("continue"))))))) {
#line 257 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 258 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit("<- ")))) || yis_as_bool(yis_eq(v_t, YV_STR(stdr_str_lit("<-"))))))) {
#line 259 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 260 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit(":: ")))) || yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit(": ")))))) || yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit("!: ")))))) || yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit(",: "))))))) {
#line 261 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 262 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  if (yis_as_bool(yis_stdr_starts_with(v_t, YV_STR(stdr_str_lit("-> "))))) {
#line 263 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
    return v_H_KEYWORD;
  }
#line 264 "/Users/nayu/Developer/Cogito/extras/Right/right_syntax.yi"
  return v_C_FG;
}

static YisVal __fnwrap_right_syntax_is_alpha(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_syntax_is_alpha(__a0);
}

static YisVal __fnwrap_right_syntax_is_digit(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_syntax_is_digit(__a0);
}

static YisVal __fnwrap_right_syntax_is_alnum(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_syntax_is_alnum(__a0);
}

static YisVal __fnwrap_right_syntax_is_keyword(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_syntax_is_keyword(__a0);
}

static YisVal __fnwrap_right_syntax_is_type(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_syntax_is_type(__a0);
}

static YisVal __fnwrap_right_syntax_is_constant(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_syntax_is_constant(__a0);
}

static YisVal __fnwrap_right_syntax_fill_colors(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  yis_right_syntax_fill_colors(__a0, __a1, __a2, __a3);
  return YV_NULLV;
}

static YisVal __fnwrap_right_syntax_highlight_line(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_syntax_highlight_line(__a0);
}

static YisVal __fnwrap_right_syntax_line_color(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_right_syntax_line_color(__a0);
}

static void __yis_right_syntax_init(void) {
  yis_move_into(&v_COL_FG, YV_INT(1));
  yis_move_into(&v_COL_KW, YV_INT(3));
  yis_move_into(&v_COL_CM, YV_INT(2));
  yis_move_into(&v_COL_ST, YV_INT(2));
  yis_move_into(&v_COL_NM, YV_INT(2));
}

/* end embedded module: right_syntax */

/* begin embedded module: right_view */
static void yis_right_view_fill(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static YisVal yis_right_view_clamp_text(YisVal, YisVal);
static void yis_right_view_draw_truncation_dither(YisVal, YisVal, YisVal, YisVal);
static void yis_right_view_fill_halftone(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static void yis_right_view_draw_icn_col(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static void yis_right_view_draw_scrollbar(YisVal);
static void yis_right_view_draw_separator(YisVal);
static YisVal yis_right_view_type_prefix(YisVal);
static YisVal yis_right_view_type_color(YisVal);
static void yis_right_view_draw_outline(YisVal);
static void yis_right_view_draw_col_guide(YisVal);
static void yis_right_view_draw_status(YisVal);
static void yis_right_view_draw_editor(YisVal);
static void yis_right_view_draw_visual_row(YisVal, YisVal, YisVal, YisVal, YisVal, YisVal, YisVal, YisVal, YisVal, YisVal, YisVal, YisVal, YisVal, YisVal, YisVal, YisVal);
static void yis_right_view_draw_search(YisVal);
static void yis_right_view_draw_selection_widget(YisVal);
static void yis_right_view_draw_autocomplete(YisVal);
static void yis_right_view_draw_all(YisVal);
static YisVal __fnwrap_right_view_fill(void*,int,YisVal*);
static YisVal __fnwrap_right_view_clamp_text(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_truncation_dither(void*,int,YisVal*);
static YisVal __fnwrap_right_view_fill_halftone(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_icn_col(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_scrollbar(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_separator(void*,int,YisVal*);
static YisVal __fnwrap_right_view_type_prefix(void*,int,YisVal*);
static YisVal __fnwrap_right_view_type_color(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_outline(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_col_guide(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_status(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_editor(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_visual_row(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_search(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_selection_widget(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_autocomplete(void*,int,YisVal*);
static YisVal __fnwrap_right_view_draw_all(void*,int,YisVal*);
static YisVal v_ICN_HALFTONE = YV_NULLV;
static YisVal v_ICN_SOLID = YV_NULLV;
static YisVal v_ICN_CARET = YV_NULLV;

// cask right_view
// bring stdr
// bring vimana
// bring right_model
// bring right_menu
// bring right_syntax
static void yis_right_view_fill(YisVal v_scr, YisVal v_x, YisVal v_y, YisVal v_w, YisVal v_h, YisVal v_c) {
#line 13 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_set_auto(v_scr, YV_INT(1)));
#line 14 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  { YisVal v_row = YV_INT(0); yis_retain_val(v_row);
  for (; yis_as_bool(yis_lt(v_row, v_h)); (void)((yis_move_into(&v_row, yis_add(v_row, YV_INT(1))), v_row))) {
#line 15 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_y, v_row)));
#line 16 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)(yis_m_vimana_screen_set_x(v_scr, v_x));
#line 17 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    { YisVal v_col = YV_INT(0); yis_retain_val(v_col);
    for (; yis_as_bool(yis_lt(v_col, v_w)); (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(1))), v_col))) {
#line 18 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)(yis_m_vimana_screen_pixel(v_scr, v_c));
    } }
  } }
#line 19 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_set_auto(v_scr, YV_INT(0)));
}

static YisVal yis_right_view_clamp_text(YisVal v_text, YisVal v_max_chars) {
#line 23 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_n = YV_INT(stdr_len(v_text)); yis_retain_val(v_n);
#line 24 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_le(v_n, v_max_chars))) {
#line 25 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return v_text;
  }
#line 26 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  return stdr_slice(v_text, yis_as_int(YV_INT(0)), yis_as_int(v_max_chars));
}

static void yis_right_view_draw_truncation_dither(YisVal v_scr, YisVal v_x, YisVal v_y, YisVal v_bg) {
#line 32 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_set_auto(v_scr, YV_INT(0)));
#line 33 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  { YisVal v_row = YV_INT(0); yis_retain_val(v_row);
  for (; yis_as_bool(yis_lt(v_row, v_ROW_H)); (void)((yis_move_into(&v_row, yis_add(v_row, YV_INT(1))), v_row))) {
#line 34 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    { YisVal v_col = YV_INT(0); yis_retain_val(v_col);
    for (; yis_as_bool(yis_lt(v_col, v_FONT_W)); (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(1))), v_col))) {
#line 35 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_eq(yis_mod(yis_add(v_row, v_col), YV_INT(2)), YV_INT(0)))) {
#line 36 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_x, v_col)));
#line 37 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_y, v_row)));
#line 38 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_pixel(v_scr, v_bg));
#line 39 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_x, v_col)));
#line 40 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_y, v_row)));
#line 41 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_pixel(v_scr, yis_add(v_bg, YV_INT(64))));
      }
    } }
  } }
}

static void yis_right_view_fill_halftone(YisVal v_scr, YisVal v_x, YisVal v_y, YisVal v_w, YisVal v_h, YisVal v_c) {
#line 53 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_set_auto(v_scr, YV_INT(0)));
#line 54 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  { YisVal v_row = YV_INT(0); yis_retain_val(v_row);
  for (; yis_as_bool(yis_lt(v_row, v_h)); (void)((yis_move_into(&v_row, yis_add(v_row, YV_INT(1))), v_row))) {
#line 55 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    { YisVal v_col = YV_INT(0); yis_retain_val(v_col);
    for (; yis_as_bool(yis_lt(v_col, v_w)); (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(1))), v_col))) {
#line 56 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_eq(yis_mod(yis_add(v_row, v_col), YV_INT(2)), YV_INT(0)))) {
#line 57 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_x, v_col)));
#line 58 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_y, v_row)));
#line 59 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_pixel(v_scr, v_c));
      }
    } }
  } }
}

static void yis_right_view_draw_icn_col(YisVal v_scr, YisVal v_x, YisVal v_y, YisVal v_n, YisVal v_icn, YisVal v_fg, YisVal v_bg) {
#line 63 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_n)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 64 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)(yis_m_vimana_screen_put_icn(v_scr, v_x, yis_add(v_y, yis_mul(v_i, YV_INT(8))), v_icn, v_fg, v_bg));
  } }
}

static void yis_right_view_draw_scrollbar(YisVal v_scr) {
#line 70 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_bar_x = yis_add(v_SCROLLBAR_X, YV_INT(4)); yis_retain_val(v_bar_x);
#line 71 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_bar_y = v_CONTENT_Y; yis_retain_val(v_bar_y);
#line 72 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_bar_h = yis_sub(yis_sub(v_screen_h, v_CONTENT_Y), v_STATUS_H); yis_retain_val(v_bar_h);
#line 73 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_bar_tiles = yis_stdr_floor(yis_div(v_bar_h, YV_INT(8))); yis_retain_val(v_bar_tiles);
#line 76 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_icn_col(v_scr, v_bar_x, v_bar_y, v_bar_tiles, v_ICN_HALFTONE, v_C_FG, v_C_BG));
#line 79 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_total = yis_right_model_vline_count(v_editor_text); yis_retain_val(v_total);
#line 80 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_visible = yis_right_model_visible_lines(); yis_retain_val(v_visible);
#line 81 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_le(v_total, v_visible))) {
#line 82 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)(yis_right_view_draw_icn_col(v_scr, v_bar_x, v_bar_y, v_bar_tiles, v_ICN_SOLID, v_C_FG, v_C_BG));
#line 83 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 85 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_thumb_tiles = yis_stdr_floor(yis_div(yis_mul(v_bar_tiles, v_visible), v_total)); yis_retain_val(v_thumb_tiles);
#line 86 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_lt(v_thumb_tiles, YV_INT(1)))) {
#line 87 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)((yis_move_into(&v_thumb_tiles, YV_INT(1)), v_thumb_tiles));
  }
#line 88 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_thumb_off = yis_stdr_floor(yis_div(yis_mul(yis_sub(v_bar_tiles, v_thumb_tiles), v_scroll_y), yis_sub(v_total, v_visible))); yis_retain_val(v_thumb_off);
#line 89 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_icn_col(v_scr, v_bar_x, yis_add(v_bar_y, yis_mul(v_thumb_off, YV_INT(8))), v_thumb_tiles, v_ICN_SOLID, v_C_FG, v_C_BG));
}

static void yis_right_view_draw_separator(YisVal v_scr) {
#line 95 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(v_sidebar_visible)))) {
#line 96 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 97 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sep_x = yis_right_model_sidebar_right(); yis_retain_val(v_sep_x);
#line 98 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_fill(v_scr, v_sep_x, v_CONTENT_Y, YV_INT(1), yis_sub(yis_sub(v_screen_h, v_CONTENT_Y), v_STATUS_H), v_C_FG));
}

static YisVal yis_right_view_type_prefix(YisVal v_kind) {
#line 106 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("cask"))))) {
#line 107 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return YV_STR(stdr_str_lit("%"));
  }
#line 108 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("bring"))))) {
#line 109 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return YV_STR(stdr_str_lit("@"));
  }
#line 110 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("struct"))))) {
#line 111 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return YV_STR(stdr_str_lit("|"));
  }
#line 112 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("fn")))) || yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("pub_fn"))))))) {
#line 113 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return YV_STR(stdr_str_lit(":"));
  }
#line 114 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("entry"))))) {
#line 115 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return YV_STR(stdr_str_lit(">"));
  }
#line 116 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("const"))))) {
#line 117 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return YV_STR(stdr_str_lit("="));
  }
#line 118 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("section"))))) {
#line 119 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return YV_STR(stdr_str_lit(""));
  }
#line 120 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  return YV_STR(stdr_str_lit(" "));
}

static YisVal yis_right_view_type_color(YisVal v_kind) {
#line 124 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("cask"))))) {
#line 125 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return v_C_ACCENT;
  }
#line 126 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("bring"))))) {
#line 127 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return v_C_ACCENT;
  }
#line 128 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("fn")))) || yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("pub_fn")))))) || yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("entry"))))))) {
#line 129 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return v_C_SEL;
  }
#line 130 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("struct"))))) {
#line 131 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return v_C_SEL;
  }
#line 132 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("const"))))) {
#line 133 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return v_C_SEL;
  }
#line 134 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_kind, YV_STR(stdr_str_lit("section"))))) {
#line 135 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return v_C_BG;
  }
#line 136 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  return v_C_FG;
}

static void yis_right_view_draw_outline(YisVal v_scr) {
#line 140 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(v_sidebar_visible)))) {
#line 141 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 143 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_max_rows = yis_right_model_visible_lines(); yis_retain_val(v_max_rows);
#line 144 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sx = yis_add(v_SIDEBAR_X, v_PAD_X); yis_retain_val(v_sx);
#line 147 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(v_dir_listing_active)) {
#line 148 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_dn = YV_INT(stdr_len(v_dir_entries)); yis_retain_val(v_dn);
#line 149 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(yis_le(v_dn, YV_INT(0)))) {
#line 150 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, v_CONTENT_Y, YV_STR(stdr_str_lit("(empty)")), v_C_SEL, v_C_BG));
#line 152 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      return;
    }
#line 153 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
    for (; yis_as_bool(yis_lt(v_i, v_max_rows)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 154 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_idx = yis_add(v_dir_scroll, v_i); yis_retain_val(v_idx);
#line 155 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_ge(v_idx, v_dn))) {
#line 156 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        break;
      }
#line 157 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_y = yis_add(v_CONTENT_Y, yis_mul(v_i, v_ROW_H)); yis_retain_val(v_y);
#line 158 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_entry = YV_STR(stdr_to_string(((yis_index(v_dir_entries, v_idx)).tag == EVT_NULL ? (YV_STR(stdr_str_lit(""))) : (yis_index(v_dir_entries, v_idx))))); yis_retain_val(v_entry);
#line 159 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_max_c = yis_right_model_sidebar_chars(); yis_retain_val(v_max_c);
#line 160 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_label = yis_right_view_clamp_text(v_entry, v_max_c); yis_retain_val(v_label);
#line 161 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_truncated = yis_gt(YV_INT(stdr_len(v_entry)), v_max_c); yis_retain_val(v_truncated);
#line 162 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_text_w = yis_mul(YV_INT(stdr_len(v_label)), v_FONT_W); yis_retain_val(v_text_w);
#line 163 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_eq(v_idx, v_dir_selected))) {
#line 164 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_right_view_fill(v_scr, v_SIDEBAR_X, v_y, v_sidebar_width, v_ROW_H, v_C_ACCENT));
#line 166 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, v_y, v_label, v_C_BG, v_C_ACCENT));
      } else {
#line 168 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, v_y, v_label, v_C_FG, v_C_BG));
      }
#line 169 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(v_truncated)) {
#line 170 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_right_view_draw_truncation_dither(v_scr, yis_sub(yis_add(v_sx, v_text_w), v_FONT_W), v_y, v_C_BG));
      }
    } }
#line 171 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 173 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_n = YV_INT(stdr_len(v_current_outline)); yis_retain_val(v_n);
#line 175 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_le(v_n, YV_INT(0)))) {
#line 176 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, v_CONTENT_Y, YV_STR(stdr_str_lit("(no outline)")), v_C_SEL, v_C_BG));
#line 178 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 180 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_lt(v_i, v_max_rows)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 181 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_idx = yis_add(v_outline_scroll, v_i); yis_retain_val(v_idx);
#line 182 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(yis_ge(v_idx, v_n))) {
#line 183 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      break;
    }
#line 184 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_y = yis_add(v_CONTENT_Y, yis_mul(v_i, v_ROW_H)); yis_retain_val(v_y);
#line 185 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_item = yis_index(v_current_outline, v_idx); yis_retain_val(v_item);
#line 186 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_kind = YV_STR(stdr_to_string(((yis_index(v_item, YV_STR(stdr_str_lit("type")))).tag == EVT_NULL ? (YV_STR(stdr_str_lit(""))) : (yis_index(v_item, YV_STR(stdr_str_lit("type"))))))); yis_retain_val(v_kind);
#line 187 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_prefix = yis_right_view_type_prefix(v_kind); yis_retain_val(v_prefix);
#line 188 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_raw_label = YV_STR(stdr_to_string(((yis_index(v_item, YV_STR(stdr_str_lit("label")))).tag == EVT_NULL ? (YV_STR(stdr_str_lit(""))) : (yis_index(v_item, YV_STR(stdr_str_lit("label"))))))); yis_retain_val(v_raw_label);
#line 189 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_is_section = yis_eq(v_kind, YV_STR(stdr_str_lit("section"))); yis_retain_val(v_is_section);
#line 191 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(v_is_section)) {
#line 193 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_max_sc = yis_right_model_sidebar_chars(); yis_retain_val(v_max_sc);
#line 194 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_sec_label = yis_right_view_clamp_text(v_raw_label, v_max_sc); yis_retain_val(v_sec_label);
#line 195 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_truncated = yis_gt(YV_INT(stdr_len(v_raw_label)), v_max_sc); yis_retain_val(v_truncated);
#line 196 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_text_w = yis_mul(YV_INT(stdr_len(v_sec_label)), v_FONT_W); yis_retain_val(v_text_w);
#line 197 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_arrow_str = yis_stdr_char_from_code(v_GLYPH_ARROW); yis_retain_val(v_arrow_str);
#line 198 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_eq(v_idx, v_outline_selected))) {
#line 199 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_right_view_fill(v_scr, v_SIDEBAR_X, v_y, yis_sub(yis_add(v_sx, v_text_w), v_SIDEBAR_X), v_ROW_H, v_C_ACCENT));
#line 201 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, v_y, v_sec_label, v_C_BG, v_C_ACCENT));
#line 202 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        if (yis_as_bool(v_truncated)) {
#line 203 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          (void)(yis_right_view_draw_truncation_dither(v_scr, yis_sub(yis_add(v_sx, v_text_w), v_FONT_W), v_y, v_C_BG));
        }
#line 204 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_put_text(v_scr, yis_add(v_sx, v_text_w), v_y, v_arrow_str, v_C_ACCENT, v_C_BG));
      } else {
#line 206 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_right_view_fill(v_scr, v_SIDEBAR_X, v_y, yis_sub(yis_add(v_sx, v_text_w), v_SIDEBAR_X), v_ROW_H, v_C_FG));
#line 208 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, v_y, v_sec_label, v_C_BG, v_C_FG));
#line 209 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        if (yis_as_bool(v_truncated)) {
#line 210 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          (void)(yis_right_view_draw_truncation_dither(v_scr, yis_sub(yis_add(v_sx, v_text_w), v_FONT_W), v_y, v_C_BG));
        }
#line 211 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_put_text(v_scr, yis_add(v_sx, v_text_w), v_y, v_arrow_str, v_C_FG, v_C_BG));
      }
    } else {
#line 213 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_max_lc = yis_sub(yis_right_model_sidebar_chars(), YV_INT(2)); yis_retain_val(v_max_lc);
#line 214 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_label = yis_right_view_clamp_text(v_raw_label, v_max_lc); yis_retain_val(v_label);
#line 215 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_truncated = yis_gt(YV_INT(stdr_len(v_raw_label)), v_max_lc); yis_retain_val(v_truncated);
#line 216 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_display = yis_add(v_prefix, v_label); yis_retain_val(v_display);
#line 217 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_text_w = yis_mul(YV_INT(stdr_len(v_display)), v_FONT_W); yis_retain_val(v_text_w);
#line 218 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_arrow_str = yis_stdr_char_from_code(v_GLYPH_ARROW); yis_retain_val(v_arrow_str);
#line 220 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_eq(v_idx, v_outline_selected))) {
#line 221 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_right_view_fill(v_scr, v_SIDEBAR_X, v_y, yis_sub(yis_add(v_sx, v_text_w), v_SIDEBAR_X), v_ROW_H, v_C_ACCENT));
#line 223 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, v_y, v_display, v_C_BG, v_C_ACCENT));
#line 224 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        if (yis_as_bool(v_truncated)) {
#line 225 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          (void)(yis_right_view_draw_truncation_dither(v_scr, yis_sub(yis_add(v_sx, v_text_w), v_FONT_W), v_y, v_C_BG));
        }
#line 226 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_put_text(v_scr, yis_add(v_sx, v_text_w), v_y, v_arrow_str, v_C_ACCENT, v_C_BG));
      } else {
#line 228 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        YisVal v_fg = yis_right_view_type_color(v_kind); yis_retain_val(v_fg);
#line 229 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, v_y, v_display, v_fg, v_C_BG));
#line 230 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        if (yis_as_bool(v_truncated)) {
#line 231 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          (void)(yis_right_view_draw_truncation_dither(v_scr, yis_sub(yis_add(v_sx, v_text_w), v_FONT_W), v_y, v_C_BG));
        }
      }
    }
  } }
}

static void yis_right_view_draw_col_guide(YisVal v_scr) {
#line 237 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_ex = yis_add(yis_right_model_editor_left(), v_PAD_X); yis_retain_val(v_ex);
#line 238 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_guide_x = yis_add(v_ex, yis_mul(v_COL_GUIDE, v_FONT_W)); yis_retain_val(v_guide_x);
#line 239 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_ge(v_guide_x, v_screen_w))) {
#line 240 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 241 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_fill_halftone(v_scr, v_guide_x, v_CONTENT_Y, YV_INT(1), yis_sub(yis_sub(v_screen_h, v_CONTENT_Y), v_STATUS_H), v_C_SEL));
}

static void yis_right_view_draw_status(YisVal v_scr) {
#line 249 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_bytes = YV_INT(stdr_len(v_editor_text)); yis_retain_val(v_bytes);
#line 250 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_lc = yis_right_model_line_col_from_index(v_editor_text, v_caret); yis_retain_val(v_lc);
#line 251 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_cur_line = yis_add(yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))), YV_INT(1)); yis_retain_val(v_cur_line);
#line 252 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_cur_col = yis_add(yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(1))))))), YV_INT(1)); yis_retain_val(v_cur_col);
#line 253 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_total = yis_right_model_line_count(v_editor_text); yis_retain_val(v_total);
#line 254 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_free = yis_sub(v_ROM_SIZE, v_bytes); yis_retain_val(v_free);
#line 255 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_status = yis_add(yis_add(yis_add(yis_add(yis_add(yis_add(YV_STR(stdr_to_string(v_free)), YV_STR(stdr_str_lit("  Ln "))), YV_STR(stdr_to_string(v_cur_line))), YV_STR(stdr_str_lit("/"))), YV_STR(stdr_to_string(v_total))), YV_STR(stdr_str_lit(" Col "))), YV_STR(stdr_to_string(v_cur_col))); yis_retain_val(v_status);
#line 256 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sw = yis_mul(YV_INT(stdr_len(v_status)), v_FONT_W); yis_retain_val(v_sw);
#line 257 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sx = yis_sub(yis_sub(v_screen_w, v_sw), YV_INT(8)); yis_retain_val(v_sx);
#line 258 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, YV_INT(2), v_status, v_C_SEL, v_C_BG));
}

static void yis_right_view_draw_editor(YisVal v_scr) {
#line 264 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_code = v_editor_text; yis_retain_val(v_code);
#line 265 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_total = YV_INT(stdr_len(v_code)); yis_retain_val(v_total);
#line 266 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_max_rows = yis_right_model_visible_lines(); yis_retain_val(v_max_rows);
#line 267 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_scroll = v_scroll_y; yis_retain_val(v_scroll);
#line 268 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_ex = yis_add(yis_right_model_editor_left(), v_PAD_X); yis_retain_val(v_ex);
#line 269 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_max_cols = yis_right_model_editor_cols(); yis_retain_val(v_max_cols);
#line 270 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_wrap_col = v_WRAP_COL; yis_retain_val(v_wrap_col);
#line 273 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_pair = yis_right_model_sel_normalized(); yis_retain_val(v_pair);
#line 274 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sa = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(0))))))); yis_retain_val(v_sa);
#line 275 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sb = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(1))))))); yis_retain_val(v_sb);
#line 276 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_has_sel = yis_ne(v_sa, v_sb); yis_retain_val(v_has_sel);
#line 279 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_match_pos = yis_right_model_find_matching_bracket(); yis_retain_val(v_match_pos);
#line 282 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_row_start = YV_INT(0); yis_retain_val(v_row_start);
#line 283 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_vis_row = YV_INT(0); yis_retain_val(v_vis_row);
#line 284 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_col = YV_INT(0); yis_retain_val(v_col);
#line 285 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_logical_start = YV_INT(0); yis_retain_val(v_logical_start);
#line 287 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  { YisVal v_i = YV_INT(0); yis_retain_val(v_i);
  for (; yis_as_bool(yis_le(v_i, v_total)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 288 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_at_end = yis_ge(v_i, v_total); yis_retain_val(v_at_end);
#line 289 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_is_nl = YV_BOOL(yis_as_bool(YV_BOOL(!yis_as_bool(v_at_end))) && yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))); yis_retain_val(v_is_nl);
#line 292 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(!yis_as_bool(v_at_end))) && yis_as_bool(YV_BOOL(!yis_as_bool(v_is_nl))))) && yis_as_bool(yis_ge(v_col, v_wrap_col))))) {
#line 293 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_vis_row, v_scroll)) && yis_as_bool(yis_lt(v_vis_row, yis_add(v_scroll, v_max_rows)))))) {
#line 294 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_right_view_draw_visual_row(v_scr, v_code, v_total, v_row_start, v_i, v_logical_start, v_vis_row, v_scroll, v_max_rows, v_max_cols, v_ex, YV_BOOL(false), v_has_sel, v_sa, v_sb, v_match_pos));
      }
#line 297 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)((yis_move_into(&v_vis_row, yis_add(v_vis_row, YV_INT(1))), v_vis_row));
#line 298 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)((yis_move_into(&v_row_start, v_i), v_row_start));
#line 299 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)((yis_move_into(&v_col, YV_INT(0)), v_col));
    }
#line 301 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(v_at_end) || yis_as_bool(v_is_nl)))) {
#line 302 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_vis_row, v_scroll)) && yis_as_bool(yis_lt(v_vis_row, yis_add(v_scroll, v_max_rows)))))) {
#line 303 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_right_view_draw_visual_row(v_scr, v_code, v_total, v_row_start, v_i, v_logical_start, v_vis_row, v_scroll, v_max_rows, v_max_cols, v_ex, v_is_nl, v_has_sel, v_sa, v_sb, v_match_pos));
      }
#line 306 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)((yis_move_into(&v_vis_row, yis_add(v_vis_row, YV_INT(1))), v_vis_row));
#line 307 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)((yis_move_into(&v_row_start, yis_add(v_i, YV_INT(1))), v_row_start));
#line 308 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)((yis_move_into(&v_col, YV_INT(0)), v_col));
#line 309 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)((yis_move_into(&v_logical_start, yis_add(v_i, YV_INT(1))), v_logical_start));
    } else {
#line 311 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)((yis_move_into(&v_col, yis_add(v_col, YV_INT(1))), v_col));
    }
  } }
#line 314 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(v_has_sel)))) {
#line 315 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_lc = yis_right_model_vline_col(v_code, v_caret); yis_retain_val(v_lc);
#line 316 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_caret_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_caret_line);
#line 317 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_caret_col = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(1))))))); yis_retain_val(v_caret_col);
#line 318 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_caret_line, v_scroll)) && yis_as_bool(yis_lt(v_caret_line, yis_add(v_scroll, v_max_rows)))))) {
#line 319 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_cy = yis_add(v_CONTENT_Y, yis_mul(yis_sub(v_caret_line, v_scroll), v_ROW_H)); yis_retain_val(v_cy);
#line 320 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_cx = yis_add(v_ex, yis_mul(v_caret_col, v_FONT_W)); yis_retain_val(v_cx);
#line 322 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)(yis_m_vimana_screen_set_auto(v_scr, YV_INT(0)));
#line 323 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      { YisVal v_cr = YV_INT(0); yis_retain_val(v_cr);
      for (; yis_as_bool(yis_lt(v_cr, v_ROW_H)); (void)((yis_move_into(&v_cr, yis_add(v_cr, YV_INT(1))), v_cr))) {
#line 324 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_set_y(v_scr, yis_add(v_cy, v_cr)));
#line 325 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_set_x(v_scr, v_cx));
        (void)(yis_m_vimana_screen_pixel(v_scr, v_C_ACCENT));
#line 326 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_set_x(v_scr, yis_add(v_cx, YV_INT(1))));
        (void)(yis_m_vimana_screen_pixel(v_scr, v_C_ACCENT));
      } }
    }
  }
}

static void yis_right_view_draw_visual_row(YisVal v_scr, YisVal v_code, YisVal v_total, YisVal v_row_start, YisVal v_row_end, YisVal v_logical_start, YisVal v_vis_row, YisVal v_scroll, YisVal v_max_rows, YisVal v_max_cols, YisVal v_ex, YisVal v_is_nl, YisVal v_has_sel, YisVal v_sa, YisVal v_sb, YisVal v_match_pos) {
#line 337 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_row = yis_sub(v_vis_row, v_scroll); yis_retain_val(v_row);
#line 338 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_y = yis_add(v_CONTENT_Y, yis_mul(v_row, v_ROW_H)); yis_retain_val(v_y);
#line 339 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_raw_line = stdr_slice(v_code, yis_as_int(v_row_start), yis_as_int(v_row_end)); yis_retain_val(v_raw_line);
#line 340 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_line_text = yis_right_view_clamp_text(v_raw_line, v_max_cols); yis_retain_val(v_line_text);
#line 341 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_is_first_visual = yis_eq(v_row_start, v_logical_start); yis_retain_val(v_is_first_visual);
#line 344 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(v_highlight_on)))) {
#line 346 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_logical_end = v_row_start; yis_retain_val(v_logical_end);
#line 347 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    { YisVal v_k = v_row_start; yis_retain_val(v_k);
    for (; yis_as_bool(yis_lt(v_k, v_total)); (void)((yis_move_into(&v_k, yis_add(v_k, YV_INT(1))), v_k))) {
#line 348 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_eq(stdr_slice(v_code, yis_as_int(v_k), yis_as_int(yis_add(v_k, YV_INT(1)))), YV_STR(stdr_str_lit("\n"))))) {
#line 349 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)((yis_move_into(&v_logical_end, v_k), v_logical_end));
#line 350 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)((yis_move_into(&v_k, v_total), v_k));
      } else {
#line 352 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)((yis_move_into(&v_logical_end, yis_add(v_k, YV_INT(1))), v_logical_end));
      }
    } }
#line 353 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_full_logical = stdr_slice(v_code, yis_as_int(v_logical_start), yis_as_int(v_logical_end)); yis_retain_val(v_full_logical);
#line 354 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_colors = yis_right_syntax_highlight_line(v_full_logical); yis_retain_val(v_colors);
#line 355 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_color_offset = yis_sub(v_row_start, v_logical_start); yis_retain_val(v_color_offset);
#line 356 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_clen = YV_INT(stdr_len(v_line_text)); yis_retain_val(v_clen);
#line 357 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    { YisVal v_ci = YV_INT(0); yis_retain_val(v_ci);
    for (; yis_as_bool(yis_lt(v_ci, v_clen)); (void)((yis_move_into(&v_ci, yis_add(v_ci, YV_INT(1))), v_ci))) {
#line 358 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_cfx = yis_add(v_ex, yis_mul(v_ci, v_FONT_W)); yis_retain_val(v_cfx);
#line 359 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_cc = v_C_FG; yis_retain_val(v_cc);
#line 360 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_cidx = yis_add(v_color_offset, v_ci); yis_retain_val(v_cidx);
#line 361 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_lt(v_cidx, YV_INT(stdr_len(v_colors))))) {
#line 362 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)((yis_move_into(&v_cc, YV_INT(stdr_num(((yis_index(v_colors, v_cidx)).tag == EVT_NULL ? (YV_INT(1)) : (yis_index(v_colors, v_cidx)))))), v_cc));
      }
#line 363 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)(yis_m_vimana_screen_put_text(v_scr, v_cfx, v_y, stdr_slice(v_line_text, yis_as_int(v_ci), yis_as_int(yis_add(v_ci, YV_INT(1)))), v_cc, v_C_BG));
    } }
  } else {
#line 365 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_fg = yis_right_syntax_line_color(v_raw_line); yis_retain_val(v_fg);
#line 366 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)(yis_m_vimana_screen_put_text(v_scr, v_ex, v_y, v_line_text, v_fg, v_C_BG));
  }
#line 369 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(v_is_first_visual) && yis_as_bool(v_show_tabs)))) {
#line 370 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_tab_str = yis_stdr_char_from_code(v_GLYPH_TAB); yis_retain_val(v_tab_str);
#line 371 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_line_len = YV_INT(stdr_len(v_raw_line)); yis_retain_val(v_line_len);
#line 372 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_indent = YV_INT(0); yis_retain_val(v_indent);
#line 373 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    { YisVal v_ti = YV_INT(0); yis_retain_val(v_ti);
    for (; yis_as_bool(yis_lt(v_ti, v_line_len)); (void)((yis_move_into(&v_ti, yis_add(v_ti, YV_INT(1))), v_ti))) {
#line 374 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_eq(stdr_slice(v_raw_line, yis_as_int(v_ti), yis_as_int(yis_add(v_ti, YV_INT(1)))), YV_STR(stdr_str_lit(" "))))) {
#line 375 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)((yis_move_into(&v_indent, yis_add(v_indent, YV_INT(1))), v_indent));
      } else {
#line 377 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)((yis_move_into(&v_ti, v_line_len), v_ti));
      }
    } }
#line 378 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    { YisVal v_ti = YV_INT(0); yis_retain_val(v_ti);
    for (; yis_as_bool(yis_lt(v_ti, yis_div(v_indent, YV_INT(4)))); (void)((yis_move_into(&v_ti, yis_add(v_ti, YV_INT(1))), v_ti))) {
#line 379 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_tcol = yis_mul(v_ti, YV_INT(4)); yis_retain_val(v_tcol);
#line 380 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_lt(v_tcol, v_max_cols))) {
#line 381 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        YisVal v_tx = yis_add(v_ex, yis_mul(v_tcol, v_FONT_W)); yis_retain_val(v_tx);
#line 382 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_put_text(v_scr, v_tx, v_y, v_tab_str, v_C_SEL, v_C_BG));
      }
    } }
  }
#line 385 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(v_is_nl) && yis_as_bool(v_show_line_endings)))) {
#line 386 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_lb_col = YV_INT(stdr_len(v_line_text)); yis_retain_val(v_lb_col);
#line 387 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(yis_lt(v_lb_col, v_max_cols))) {
#line 388 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_lbx = yis_add(v_ex, yis_mul(v_lb_col, v_FONT_W)); yis_retain_val(v_lbx);
#line 389 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_brk_str = yis_stdr_char_from_code(v_GLYPH_BRK); yis_retain_val(v_brk_str);
#line 390 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)(yis_m_vimana_screen_put_text(v_scr, v_lbx, v_y, v_brk_str, v_C_SEL, v_C_BG));
    }
  }
#line 393 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_match_pos, v_row_start)) && yis_as_bool(yis_lt(v_match_pos, v_row_end))))) {
#line 394 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_bcol = yis_sub(v_match_pos, v_row_start); yis_retain_val(v_bcol);
#line 395 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(yis_lt(v_bcol, v_max_cols))) {
#line 396 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_bx = yis_add(v_ex, yis_mul(v_bcol, v_FONT_W)); yis_retain_val(v_bx);
#line 397 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_bch = stdr_slice(v_code, yis_as_int(v_match_pos), yis_as_int(yis_add(v_match_pos, YV_INT(1)))); yis_retain_val(v_bch);
#line 398 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)(yis_m_vimana_screen_put_text(v_scr, v_bx, v_y, v_bch, v_C_ACCENT, v_C_BG));
    }
  }
#line 401 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(v_search_active) && yis_as_bool(yis_gt(YV_INT(stdr_len(v_search_query)), YV_INT(0)))))) {
#line 402 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_qlen = YV_INT(stdr_len(v_search_query)); yis_retain_val(v_qlen);
#line 403 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_match_list = v_search_matches; yis_retain_val(v_match_list);
#line 404 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_mc = YV_INT(stdr_len(v_match_list)); yis_retain_val(v_mc);
#line 405 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    { YisVal v_mi = YV_INT(0); yis_retain_val(v_mi);
    for (; yis_as_bool(yis_lt(v_mi, v_mc)); (void)((yis_move_into(&v_mi, yis_add(v_mi, YV_INT(1))), v_mi))) {
#line 406 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_mpos = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_match_list, v_mi)).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_match_list, v_mi)))))); yis_retain_val(v_mpos);
#line 407 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(YV_BOOL(yis_as_bool(yis_gt(yis_add(v_mpos, v_qlen), v_row_start)) && yis_as_bool(yis_lt(v_mpos, v_row_end))))) {
#line 408 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        YisVal v_ml = yis_sub(v_mpos, v_row_start); yis_retain_val(v_ml);
#line 409 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        YisVal v_mr = yis_add(v_ml, v_qlen); yis_retain_val(v_mr);
#line 410 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        if (yis_as_bool(yis_lt(v_ml, YV_INT(0)))) {
#line 411 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          (void)((yis_move_into(&v_ml, YV_INT(0)), v_ml));
        }
#line 412 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        if (yis_as_bool(yis_gt(v_mr, v_max_cols))) {
#line 413 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          (void)((yis_move_into(&v_mr, v_max_cols), v_mr));
        }
#line 414 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        if (yis_as_bool(yis_gt(v_mr, v_ml))) {
#line 415 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          YisVal v_mt = stdr_slice(v_code, yis_as_int(yis_add(v_row_start, v_ml)), yis_as_int(yis_add(v_row_start, v_mr))); yis_retain_val(v_mt);
#line 416 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          YisVal v_mx_pos = yis_add(v_ex, yis_mul(v_ml, v_FONT_W)); yis_retain_val(v_mx_pos);
#line 417 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
          (void)(yis_m_vimana_screen_put_text(v_scr, v_mx_pos, v_y, v_mt, v_C_FG, v_C_ACCENT));
        }
      }
    } }
  }
#line 420 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(v_has_sel) && yis_as_bool(yis_lt(v_sa, yis_add(v_row_end, YV_INT(1)))))) && yis_as_bool(yis_gt(v_sb, v_row_start))))) {
#line 421 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_left = v_sa; yis_retain_val(v_left);
#line 422 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(yis_lt(v_left, v_row_start))) {
#line 423 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)((yis_move_into(&v_left, v_row_start), v_left));
    }
#line 424 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_right = v_sb; yis_retain_val(v_right);
#line 425 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(yis_gt(v_right, v_row_end))) {
#line 426 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      (void)((yis_move_into(&v_right, v_row_end), v_right));
    }
#line 427 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    if (yis_as_bool(yis_gt(v_right, v_left))) {
#line 428 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_local_left = yis_sub(v_left, v_row_start); yis_retain_val(v_local_left);
#line 429 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      YisVal v_local_right = yis_sub(v_right, v_row_start); yis_retain_val(v_local_right);
#line 430 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_lt(v_local_left, YV_INT(0)))) {
#line 431 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)((yis_move_into(&v_local_left, YV_INT(0)), v_local_left));
      }
#line 432 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_gt(v_local_right, v_max_cols))) {
#line 433 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)((yis_move_into(&v_local_right, v_max_cols), v_local_right));
      }
#line 434 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
      if (yis_as_bool(yis_gt(v_local_right, v_local_left))) {
#line 435 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        YisVal v_sel_text = stdr_slice(v_code, yis_as_int(v_left), yis_as_int(yis_sub(yis_add(v_left, v_local_right), v_local_left))); yis_retain_val(v_sel_text);
#line 436 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        YisVal v_sx = yis_add(v_ex, yis_mul(v_local_left, v_FONT_W)); yis_retain_val(v_sx);
#line 437 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
        (void)(yis_m_vimana_screen_put_text(v_scr, v_sx, v_y, v_sel_text, v_C_BG, v_C_SEL));
      }
    }
  }
}

static void yis_right_view_draw_search(YisVal v_scr) {
#line 443 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(v_search_active)))) {
#line 444 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 446 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sx = yis_right_model_editor_left(); yis_retain_val(v_sx);
#line 447 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sy = v_CONTENT_Y; yis_retain_val(v_sy);
#line 448 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sw = yis_right_model_editor_width(); yis_retain_val(v_sw);
#line 449 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sh = v_ROW_H; yis_retain_val(v_sh);
#line 452 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_fill(v_scr, v_sx, v_sy, v_sw, v_sh, v_C_FG));
#line 455 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_label = yis_add(YV_STR(stdr_str_lit("Find: ")), v_search_query); yis_retain_val(v_label);
#line 456 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_match_count = YV_INT(stdr_len(v_search_matches)); yis_retain_val(v_match_count);
#line 457 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_gt(v_match_count, YV_INT(0)))) {
#line 458 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    YisVal v_idx = yis_add(v_search_index, YV_INT(1)); yis_retain_val(v_idx);
#line 459 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)((yis_move_into(&v_label, yis_add(yis_add(yis_add(yis_add(yis_add(v_label, YV_STR(stdr_str_lit("  ["))), YV_STR(stdr_to_string(v_idx))), YV_STR(stdr_str_lit("/"))), YV_STR(stdr_to_string(v_match_count))), YV_STR(stdr_str_lit("]")))), v_label));
  } else if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_search_query)), YV_INT(0)))) {
#line 461 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)((yis_move_into(&v_label, yis_add(v_label, YV_STR(stdr_str_lit("  [no match]")))), v_label));
  }

#line 463 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, yis_add(v_sx, YV_INT(8)), v_sy, v_label, v_C_BG, v_C_FG));
#line 466 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_qx = yis_add(yis_add(v_sx, YV_INT(8)), yis_mul(yis_add(YV_INT(6), YV_INT(stdr_len(v_search_query))), v_FONT_W)); yis_retain_val(v_qx);
#line 467 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_fill(v_scr, v_qx, yis_add(v_sy, YV_INT(2)), YV_INT(2), yis_sub(v_ROW_H, YV_INT(4)), v_C_ACCENT));
}

static void yis_right_view_draw_selection_widget(YisVal v_scr) {
#line 473 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_pair = yis_right_model_sel_normalized(); yis_retain_val(v_pair);
#line 474 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sa = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(0))))))); yis_retain_val(v_sa);
#line 475 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_sb = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_pair, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_pair, YV_INT(1))))))); yis_retain_val(v_sb);
#line 476 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_sa, v_sb))) {
#line 477 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 478 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_count = yis_sub(v_sb, v_sa); yis_retain_val(v_count);
#line 479 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_label = YV_STR(stdr_to_string(v_count)); yis_retain_val(v_label);
#line 480 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_label_len = YV_INT(stdr_len(v_label)); yis_retain_val(v_label_len);
#line 483 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_code = v_editor_text; yis_retain_val(v_code);
#line 484 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_lc = yis_right_model_vline_col(v_code, v_caret); yis_retain_val(v_lc);
#line 485 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_caret_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_caret_line);
#line 486 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_caret_col = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(1))))))); yis_retain_val(v_caret_col);
#line 487 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_scroll = v_scroll_y; yis_retain_val(v_scroll);
#line 488 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_lt(v_caret_line, v_scroll)) || yis_as_bool(yis_ge(v_caret_line, yis_add(v_scroll, yis_right_model_visible_lines())))))) {
#line 489 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 491 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_ex = yis_add(yis_right_model_editor_left(), v_PAD_X); yis_retain_val(v_ex);
#line 492 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_px = yis_add(v_ex, yis_mul(v_caret_col, v_FONT_W)); yis_retain_val(v_px);
#line 493 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_py = yis_sub(yis_add(v_CONTENT_Y, yis_mul(yis_sub(v_caret_line, v_scroll), v_ROW_H)), v_ROW_H); yis_retain_val(v_py);
#line 494 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_lt(v_py, v_CONTENT_Y))) {
#line 495 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)((yis_move_into(&v_py, yis_add(v_CONTENT_Y, yis_mul(yis_add(yis_sub(v_caret_line, v_scroll), YV_INT(1)), v_ROW_H))), v_py));
  }
#line 497 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_pw = yis_add(yis_mul(v_label_len, v_FONT_W), YV_INT(8)); yis_retain_val(v_pw);
#line 498 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_ph = v_ROW_H; yis_retain_val(v_ph);
#line 499 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_fill(v_scr, v_px, v_py, v_pw, v_ph, v_C_ACCENT));
#line 500 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, yis_add(v_px, YV_INT(4)), v_py, v_label, v_C_BG, v_C_ACCENT));
}

static void yis_right_view_draw_autocomplete(YisVal v_scr) {
#line 506 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(v_autocomplete_visible)))) {
#line 507 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 508 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_word = v_autocomplete_word; yis_retain_val(v_word);
#line 509 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_wlen = YV_INT(stdr_len(v_word)); yis_retain_val(v_wlen);
#line 510 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(yis_eq(v_wlen, YV_INT(0)))) {
#line 511 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 514 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_code = v_editor_text; yis_retain_val(v_code);
#line 515 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_lc = yis_right_model_vline_col(v_code, v_caret); yis_retain_val(v_lc);
#line 516 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_caret_line = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(0))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(0))))))); yis_retain_val(v_caret_line);
#line 517 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_caret_col = yis_stdr_floor(YV_INT(stdr_num(((yis_index(v_lc, YV_INT(1))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_lc, YV_INT(1))))))); yis_retain_val(v_caret_col);
#line 518 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_scroll = v_scroll_y; yis_retain_val(v_scroll);
#line 519 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_lt(v_caret_line, v_scroll)) || yis_as_bool(yis_ge(v_caret_line, yis_add(v_scroll, yis_right_model_visible_lines())))))) {
#line 520 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    return;
  }
#line 522 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_prefix = yis_right_model_word_before_caret(); yis_retain_val(v_prefix);
#line 523 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_plen = YV_INT(stdr_len(v_prefix)); yis_retain_val(v_plen);
#line 524 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_start_col = yis_sub(v_caret_col, v_plen); yis_retain_val(v_start_col);
#line 526 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_ex = yis_add(yis_right_model_editor_left(), v_PAD_X); yis_retain_val(v_ex);
#line 527 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_px = yis_add(v_ex, yis_mul(v_start_col, v_FONT_W)); yis_retain_val(v_px);
#line 528 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_py = yis_add(v_CONTENT_Y, yis_mul(yis_add(yis_sub(v_caret_line, v_scroll), YV_INT(1)), v_ROW_H)); yis_retain_val(v_py);
#line 530 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_pw = yis_add(yis_mul(v_wlen, v_FONT_W), YV_INT(8)); yis_retain_val(v_pw);
#line 531 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  YisVal v_ph = v_ROW_H; yis_retain_val(v_ph);
#line 534 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_fill(v_scr, v_px, v_py, v_pw, v_ph, v_C_FG));
#line 536 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_put_text(v_scr, yis_add(v_px, YV_INT(4)), v_py, v_word, v_C_SEL, v_C_FG));
#line 537 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_gt(v_plen, YV_INT(0))) && yis_as_bool(yis_lt(v_plen, v_wlen))))) {
#line 538 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
    (void)(yis_m_vimana_screen_put_text(v_scr, yis_add(v_px, YV_INT(4)), v_py, v_prefix, v_C_ACCENT, v_C_FG));
  }
}

static void yis_right_view_draw_all(YisVal v_scr) {
#line 544 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_clear(v_scr, v_C_BG));
#line 545 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_m_vimana_screen_draw_titlebar(v_scr, v_C_ACCENT));
#line 546 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_scrollbar(v_scr));
#line 547 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_separator(v_scr));
#line 548 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_outline(v_scr));
#line 549 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_col_guide(v_scr));
#line 550 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_editor(v_scr));
#line 551 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_selection_widget(v_scr));
#line 552 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_autocomplete(v_scr));
#line 553 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_status(v_scr));
#line 554 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_view_draw_search(v_scr));
#line 555 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_menu_draw_menubar(v_scr));
#line 556 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
  (void)(yis_right_menu_draw_submenu(v_scr));
#line 557 "/Users/nayu/Developer/Cogito/extras/Right/right_view.yi"
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

static YisVal __fnwrap_right_view_draw_truncation_dither(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  yis_right_view_draw_truncation_dither(__a0, __a1, __a2, __a3);
  return YV_NULLV;
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

static YisVal __fnwrap_right_view_draw_icn_col(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  YisVal __a4 = argc > 4 ? argv[4] : YV_NULLV;
  YisVal __a5 = argc > 5 ? argv[5] : YV_NULLV;
  YisVal __a6 = argc > 6 ? argv[6] : YV_NULLV;
  yis_right_view_draw_icn_col(__a0, __a1, __a2, __a3, __a4, __a5, __a6);
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

static YisVal __fnwrap_right_view_draw_visual_row(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  YisVal __a3 = argc > 3 ? argv[3] : YV_NULLV;
  YisVal __a4 = argc > 4 ? argv[4] : YV_NULLV;
  YisVal __a5 = argc > 5 ? argv[5] : YV_NULLV;
  YisVal __a6 = argc > 6 ? argv[6] : YV_NULLV;
  YisVal __a7 = argc > 7 ? argv[7] : YV_NULLV;
  YisVal __a8 = argc > 8 ? argv[8] : YV_NULLV;
  YisVal __a9 = argc > 9 ? argv[9] : YV_NULLV;
  YisVal __a10 = argc > 10 ? argv[10] : YV_NULLV;
  YisVal __a11 = argc > 11 ? argv[11] : YV_NULLV;
  YisVal __a12 = argc > 12 ? argv[12] : YV_NULLV;
  YisVal __a13 = argc > 13 ? argv[13] : YV_NULLV;
  YisVal __a14 = argc > 14 ? argv[14] : YV_NULLV;
  YisVal __a15 = argc > 15 ? argv[15] : YV_NULLV;
  yis_right_view_draw_visual_row(__a0, __a1, __a2, __a3, __a4, __a5, __a6, __a7, __a8, __a9, __a10, __a11, __a12, __a13, __a14, __a15);
  return YV_NULLV;
}

static YisVal __fnwrap_right_view_draw_search(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_view_draw_search(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_view_draw_selection_widget(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_view_draw_selection_widget(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_view_draw_autocomplete(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_view_draw_autocomplete(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_right_view_draw_all(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_right_view_draw_all(__a0);
  return YV_NULLV;
}

static void __yis_right_view_init(void) {
  yis_move_into(&v_ICN_HALFTONE, yis_arr_lit(8, YV_INT(170), YV_INT(85), YV_INT(170), YV_INT(85), YV_INT(170), YV_INT(85), YV_INT(170), YV_INT(85)));
  yis_move_into(&v_ICN_SOLID, yis_arr_lit(8, YV_INT(255), YV_INT(255), YV_INT(255), YV_INT(255), YV_INT(255), YV_INT(255), YV_INT(255), YV_INT(255)));
  yis_move_into(&v_ICN_CARET, yis_arr_lit(8, YV_INT(192), YV_INT(192), YV_INT(192), YV_INT(192), YV_INT(192), YV_INT(192), YV_INT(192), YV_INT(192)));
}

/* end embedded module: right_view */

/* begin embedded module: right_assets */
static void yis_right_assets_setup(YisVal);
static YisVal __fnwrap_right_assets_setup(void*,int,YisVal*);

// cask right_assets
// bring vimana
static void yis_right_assets_setup(YisVal v_scr) {
#line 8 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_size(v_scr, YV_INT(3)));
#line 10 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(32), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 11 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(33), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 12 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(34), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 13 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(35), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(68), YV_INT(0), YV_INT(0), YV_INT(68), YV_INT(0), YV_INT(0), YV_INT(68), YV_INT(0), YV_INT(0), YV_INT(68), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(7), YV_INT(254), YV_INT(0), YV_INT(7), YV_INT(254), YV_INT(0), YV_INT(1), YV_INT(16), YV_INT(0), YV_INT(1), YV_INT(16), YV_INT(0), YV_INT(1), YV_INT(16), YV_INT(0), YV_INT(1), YV_INT(16), YV_INT(0), YV_INT(1), YV_INT(16), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 14 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(36), yis_arr_lit(72, YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(1), YV_INT(172), YV_INT(0), YV_INT(3), YV_INT(38), YV_INT(0), YV_INT(3), YV_INT(38), YV_INT(0), YV_INT(3), YV_INT(38), YV_INT(0), YV_INT(3), YV_INT(32), YV_INT(0), YV_INT(1), YV_INT(160), YV_INT(0), YV_INT(1), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(62), YV_INT(0), YV_INT(0), YV_INT(38), YV_INT(0), YV_INT(0), YV_INT(35), YV_INT(0), YV_INT(3), YV_INT(35), YV_INT(0), YV_INT(3), YV_INT(35), YV_INT(0), YV_INT(3), YV_INT(35), YV_INT(0), YV_INT(3), YV_INT(35), YV_INT(0), YV_INT(1), YV_INT(166), YV_INT(0), YV_INT(0), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 15 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(37), yis_arr_lit(72, YV_INT(0), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(194), YV_INT(0), YV_INT(6), YV_INT(102), YV_INT(0), YV_INT(6), YV_INT(100), YV_INT(0), YV_INT(6), YV_INT(108), YV_INT(0), YV_INT(6), YV_INT(104), YV_INT(0), YV_INT(6), YV_INT(104), YV_INT(0), YV_INT(6), YV_INT(88), YV_INT(0), YV_INT(3), YV_INT(144), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(78), YV_INT(0), YV_INT(0), YV_INT(83), YV_INT(0), YV_INT(0), YV_INT(243), YV_INT(0), YV_INT(0), YV_INT(179), YV_INT(0), YV_INT(1), YV_INT(179), YV_INT(0), YV_INT(1), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(2), YV_INT(30), YV_INT(0), YV_INT(2), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 16 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(38), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(240), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(1), YV_INT(144), YV_INT(0), YV_INT(0), YV_INT(176), YV_INT(0), YV_INT(0), YV_INT(224), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(3), YV_INT(68), YV_INT(0), YV_INT(3), YV_INT(68), YV_INT(0), YV_INT(6), YV_INT(100), YV_INT(0), YV_INT(6), YV_INT(44), YV_INT(0), YV_INT(6), YV_INT(56), YV_INT(0), YV_INT(6), YV_INT(56), YV_INT(0), YV_INT(6), YV_INT(25), YV_INT(0), YV_INT(3), YV_INT(63), YV_INT(0), YV_INT(1), YV_INT(230), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 17 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(39), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 18 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(40), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 19 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(41), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 20 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(42), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(2), YV_INT(114), YV_INT(0), YV_INT(7), YV_INT(39), YV_INT(0), YV_INT(3), YV_INT(174), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(174), YV_INT(0), YV_INT(7), YV_INT(39), YV_INT(0), YV_INT(2), YV_INT(114), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 21 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(43), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 22 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(44), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(56), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 23 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(45), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 24 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(46), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 25 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(47), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 26 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(48), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 27 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(49), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(16), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(1), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 28 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(50), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(16), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(64), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(130), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(252), YV_INT(0), YV_INT(3), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 29 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(51), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 30 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(52), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(28), YV_INT(0), YV_INT(0), YV_INT(28), YV_INT(0), YV_INT(0), YV_INT(60), YV_INT(0), YV_INT(0), YV_INT(44), YV_INT(0), YV_INT(0), YV_INT(108), YV_INT(0), YV_INT(0), YV_INT(76), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(0), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(2), YV_INT(12), YV_INT(0), YV_INT(7), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(127), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 31 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(53), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(255), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(1), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(120), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(1), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(131), YV_INT(0), YV_INT(3), YV_INT(131), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(142), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 32 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(54), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(124), YV_INT(0), YV_INT(0), YV_INT(198), YV_INT(0), YV_INT(1), YV_INT(135), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(124), YV_INT(0), YV_INT(3), YV_INT(198), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 33 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(55), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(255), YV_INT(0), YV_INT(1), YV_INT(255), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(4), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(8), YV_INT(0), YV_INT(0), YV_INT(8), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 34 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(56), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(0), YV_INT(196), YV_INT(0), YV_INT(0), YV_INT(236), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(220), YV_INT(0), YV_INT(1), YV_INT(142), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 35 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(57), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(7), YV_INT(0), YV_INT(1), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(251), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(1), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(131), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 36 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(58), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 37 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(59), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(56), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 38 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(60), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 39 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(61), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 40 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(62), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 41 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(63), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(252), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(131), YV_INT(0), YV_INT(3), YV_INT(135), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(16), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 42 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(64), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(1), YV_INT(0), YV_INT(6), YV_INT(121), YV_INT(0), YV_INT(6), YV_INT(205), YV_INT(0), YV_INT(6), YV_INT(205), YV_INT(0), YV_INT(6), YV_INT(205), YV_INT(0), YV_INT(6), YV_INT(205), YV_INT(0), YV_INT(6), YV_INT(205), YV_INT(0), YV_INT(6), YV_INT(205), YV_INT(0), YV_INT(6), YV_INT(223), YV_INT(0), YV_INT(6), YV_INT(118), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(1), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(0), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 43 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(65), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(88), YV_INT(0), YV_INT(0), YV_INT(88), YV_INT(0), YV_INT(0), YV_INT(88), YV_INT(0), YV_INT(0), YV_INT(216), YV_INT(0), YV_INT(0), YV_INT(152), YV_INT(0), YV_INT(0), YV_INT(140), YV_INT(0), YV_INT(0), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(252), YV_INT(0), YV_INT(1), YV_INT(12), YV_INT(0), YV_INT(1), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 44 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(66), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(7), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 45 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(67), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(122), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(6), YV_INT(2), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(1), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 46 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(68), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(240), YV_INT(0), YV_INT(3), YV_INT(28), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(28), YV_INT(0), YV_INT(7), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 47 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(69), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(255), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(252), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(7), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 48 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(70), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(255), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(252), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(7), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 49 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(71), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(122), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(6), YV_INT(2), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(31), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 50 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(72), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 51 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(73), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 52 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(74), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(4), YV_INT(48), YV_INT(0), YV_INT(14), YV_INT(48), YV_INT(0), YV_INT(14), YV_INT(48), YV_INT(0), YV_INT(6), YV_INT(96), YV_INT(0), YV_INT(3), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 53 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(75), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(159), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(4), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(16), YV_INT(0), YV_INT(3), YV_INT(48), YV_INT(0), YV_INT(3), YV_INT(96), YV_INT(0), YV_INT(3), YV_INT(96), YV_INT(0), YV_INT(3), YV_INT(224), YV_INT(0), YV_INT(3), YV_INT(176), YV_INT(0), YV_INT(3), YV_INT(48), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 54 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(76), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(224), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(7), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 55 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(77), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(7), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(142), YV_INT(0), YV_INT(3), YV_INT(142), YV_INT(0), YV_INT(3), YV_INT(142), YV_INT(0), YV_INT(3), YV_INT(142), YV_INT(0), YV_INT(2), YV_INT(214), YV_INT(0), YV_INT(2), YV_INT(214), YV_INT(0), YV_INT(2), YV_INT(214), YV_INT(0), YV_INT(2), YV_INT(214), YV_INT(0), YV_INT(2), YV_INT(118), YV_INT(0), YV_INT(2), YV_INT(118), YV_INT(0), YV_INT(2), YV_INT(102), YV_INT(0), YV_INT(2), YV_INT(102), YV_INT(0), YV_INT(2), YV_INT(38), YV_INT(0), YV_INT(2), YV_INT(38), YV_INT(0), YV_INT(2), YV_INT(38), YV_INT(0), YV_INT(7), YV_INT(47), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 56 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(78), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(7), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(130), YV_INT(0), YV_INT(3), YV_INT(130), YV_INT(0), YV_INT(2), YV_INT(194), YV_INT(0), YV_INT(2), YV_INT(194), YV_INT(0), YV_INT(2), YV_INT(98), YV_INT(0), YV_INT(2), YV_INT(98), YV_INT(0), YV_INT(2), YV_INT(98), YV_INT(0), YV_INT(2), YV_INT(50), YV_INT(0), YV_INT(2), YV_INT(50), YV_INT(0), YV_INT(2), YV_INT(26), YV_INT(0), YV_INT(2), YV_INT(26), YV_INT(0), YV_INT(2), YV_INT(14), YV_INT(0), YV_INT(2), YV_INT(14), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(2), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 57 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(79), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 58 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(80), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(128), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 59 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(81), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(243), YV_INT(0), YV_INT(7), YV_INT(155), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(0), YV_INT(253), YV_INT(0), YV_INT(0), YV_INT(15), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 60 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(82), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(248), YV_INT(0), YV_INT(3), YV_INT(48), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(135), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 61 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(83), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(250), YV_INT(0), YV_INT(1), YV_INT(142), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(60), YV_INT(0), YV_INT(0), YV_INT(14), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(2), YV_INT(3), YV_INT(0), YV_INT(2), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(131), YV_INT(0), YV_INT(2), YV_INT(198), YV_INT(0), YV_INT(2), YV_INT(124), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 62 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(84), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(2), YV_INT(49), YV_INT(0), YV_INT(2), YV_INT(49), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 63 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(85), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(135), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(204), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 64 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(86), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(135), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(216), YV_INT(0), YV_INT(0), YV_INT(80), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 65 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(87), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(119), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(118), YV_INT(0), YV_INT(1), YV_INT(84), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 66 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(88), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(4), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(216), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(80), YV_INT(0), YV_INT(0), YV_INT(216), YV_INT(0), YV_INT(0), YV_INT(152), YV_INT(0), YV_INT(0), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 67 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(89), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(135), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(0), YV_INT(196), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(104), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 68 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(90), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(129), YV_INT(0), YV_INT(3), YV_INT(1), YV_INT(0), YV_INT(3), YV_INT(1), YV_INT(0), YV_INT(7), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 69 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(91), yis_arr_lit(72, YV_INT(0), YV_INT(62), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(62), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 70 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(92), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 71 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(93), yis_arr_lit(72, YV_INT(3), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(1), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 72 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(94), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 73 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(95), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 74 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(96), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 75 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(97), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(240), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(1), YV_INT(252), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(6), YV_INT(12), YV_INT(0), YV_INT(6), YV_INT(12), YV_INT(0), YV_INT(6), YV_INT(13), YV_INT(0), YV_INT(3), YV_INT(31), YV_INT(0), YV_INT(1), YV_INT(246), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 76 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(98), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(120), YV_INT(0), YV_INT(3), YV_INT(204), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(206), YV_INT(0), YV_INT(2), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 77 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(99), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(124), YV_INT(0), YV_INT(1), YV_INT(198), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(1), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(1), YV_INT(198), YV_INT(0), YV_INT(0), YV_INT(124), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 78 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(100), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(30), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(246), YV_INT(0), YV_INT(1), YV_INT(158), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(158), YV_INT(0), YV_INT(0), YV_INT(247), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 79 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(101), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(1), YV_INT(206), YV_INT(0), YV_INT(1), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(1), YV_INT(0), YV_INT(1), YV_INT(131), YV_INT(0), YV_INT(1), YV_INT(198), YV_INT(0), YV_INT(0), YV_INT(124), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 80 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(102), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(30), YV_INT(0), YV_INT(0), YV_INT(51), YV_INT(0), YV_INT(0), YV_INT(35), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(7), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(3), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 81 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(103), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(243), YV_INT(0), YV_INT(1), YV_INT(159), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(1), YV_INT(152), YV_INT(0), YV_INT(3), YV_INT(240), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(240), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(7), YV_INT(15), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 82 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(104), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(120), YV_INT(0), YV_INT(3), YV_INT(204), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 83 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(105), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 84 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(106), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(4), YV_INT(48), YV_INT(0), YV_INT(14), YV_INT(48), YV_INT(0), YV_INT(14), YV_INT(48), YV_INT(0), YV_INT(6), YV_INT(96), YV_INT(0), YV_INT(3), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 85 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(107), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(30), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(48), YV_INT(0), YV_INT(3), YV_INT(112), YV_INT(0), YV_INT(3), YV_INT(240), YV_INT(0), YV_INT(3), YV_INT(152), YV_INT(0), YV_INT(3), YV_INT(24), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(12), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 86 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(108), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(1), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 87 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(109), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(118), YV_INT(0), YV_INT(3), YV_INT(187), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(3), YV_INT(51), YV_INT(0), YV_INT(7), YV_INT(119), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 88 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(110), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(120), YV_INT(0), YV_INT(3), YV_INT(204), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 89 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(111), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(6), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 90 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(112), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(120), YV_INT(0), YV_INT(3), YV_INT(206), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(206), YV_INT(0), YV_INT(3), YV_INT(120), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(128), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 91 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(113), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(242), YV_INT(0), YV_INT(3), YV_INT(158), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(6), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(158), YV_INT(0), YV_INT(0), YV_INT(246), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(6), YV_INT(0), YV_INT(0), YV_INT(15), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 92 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(114), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(206), YV_INT(0), YV_INT(0), YV_INT(219), YV_INT(0), YV_INT(0), YV_INT(227), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(3), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 93 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(115), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(250), YV_INT(0), YV_INT(1), YV_INT(142), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(126), YV_INT(0), YV_INT(0), YV_INT(15), YV_INT(0), YV_INT(2), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(3), YV_INT(0), YV_INT(3), YV_INT(134), YV_INT(0), YV_INT(2), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 94 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(116), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(224), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(97), YV_INT(0), YV_INT(0), YV_INT(51), YV_INT(0), YV_INT(0), YV_INT(30), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 95 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(117), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(14), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(158), YV_INT(0), YV_INT(0), YV_INT(247), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 96 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(118), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(199), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(0), YV_INT(196), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(104), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 97 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(119), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(119), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(50), YV_INT(0), YV_INT(3), YV_INT(118), YV_INT(0), YV_INT(1), YV_INT(84), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(1), YV_INT(220), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(136), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 98 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(120), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(207), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(1), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(216), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(216), YV_INT(0), YV_INT(0), YV_INT(156), YV_INT(0), YV_INT(1), YV_INT(140), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(7), YV_INT(143), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 99 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(121), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(135), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(3), YV_INT(2), YV_INT(0), YV_INT(1), YV_INT(134), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(1), YV_INT(132), YV_INT(0), YV_INT(0), YV_INT(204), YV_INT(0), YV_INT(0), YV_INT(200), YV_INT(0), YV_INT(0), YV_INT(104), YV_INT(0), YV_INT(0), YV_INT(120), YV_INT(0), YV_INT(0), YV_INT(112), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(6), YV_INT(32), YV_INT(0), YV_INT(6), YV_INT(96), YV_INT(0), YV_INT(3), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 100 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(122), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(2), YV_INT(12), YV_INT(0), YV_INT(2), YV_INT(28), YV_INT(0), YV_INT(0), YV_INT(56), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(224), YV_INT(0), YV_INT(1), YV_INT(194), YV_INT(0), YV_INT(1), YV_INT(130), YV_INT(0), YV_INT(3), YV_INT(6), YV_INT(0), YV_INT(3), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 101 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(123), yis_arr_lit(72, YV_INT(0), YV_INT(7), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(48), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(24), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(12), YV_INT(0), YV_INT(0), YV_INT(7), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 102 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(124), yis_arr_lit(72, YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(96), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 103 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(125), yis_arr_lit(72, YV_INT(14), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(1), YV_INT(128), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(0), YV_INT(0), YV_INT(14), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 104 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(126), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(128), YV_INT(0), YV_INT(4), YV_INT(192), YV_INT(0), YV_INT(8), YV_INT(97), YV_INT(0), YV_INT(0), YV_INT(50), YV_INT(0), YV_INT(0), YV_INT(28), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 105 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(127), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(7), YV_INT(3), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(133), YV_INT(128), YV_INT(6), YV_INT(73), YV_INT(128), YV_INT(6), YV_INT(49), YV_INT(128), YV_INT(6), YV_INT(49), YV_INT(128), YV_INT(6), YV_INT(73), YV_INT(128), YV_INT(6), YV_INT(133), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(6), YV_INT(1), YV_INT(128), YV_INT(7), YV_INT(3), YV_INT(128), YV_INT(3), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 108 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(32), YV_INT(16)));
#line 109 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(33), YV_INT(16)));
#line 110 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(34), YV_INT(16)));
#line 111 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(35), YV_INT(16)));
#line 112 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(36), YV_INT(16)));
#line 113 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(37), YV_INT(16)));
#line 114 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(38), YV_INT(16)));
#line 115 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(39), YV_INT(16)));
#line 116 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(40), YV_INT(16)));
#line 117 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(41), YV_INT(16)));
#line 118 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(42), YV_INT(16)));
#line 119 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(43), YV_INT(16)));
#line 120 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(44), YV_INT(16)));
#line 121 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(45), YV_INT(16)));
#line 122 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(46), YV_INT(16)));
#line 123 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(47), YV_INT(16)));
#line 124 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(48), YV_INT(16)));
#line 125 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(49), YV_INT(16)));
#line 126 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(50), YV_INT(16)));
#line 127 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(51), YV_INT(16)));
#line 128 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(52), YV_INT(16)));
#line 129 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(53), YV_INT(16)));
#line 130 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(54), YV_INT(16)));
#line 131 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(55), YV_INT(16)));
#line 132 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(56), YV_INT(16)));
#line 133 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(57), YV_INT(16)));
#line 134 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(58), YV_INT(16)));
#line 135 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(59), YV_INT(16)));
#line 136 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(60), YV_INT(16)));
#line 137 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(61), YV_INT(16)));
#line 138 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(62), YV_INT(16)));
#line 139 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(63), YV_INT(16)));
#line 140 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(64), YV_INT(16)));
#line 141 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(65), YV_INT(16)));
#line 142 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(66), YV_INT(16)));
#line 143 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(67), YV_INT(16)));
#line 144 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(68), YV_INT(16)));
#line 145 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(69), YV_INT(16)));
#line 146 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(70), YV_INT(16)));
#line 147 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(71), YV_INT(16)));
#line 148 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(72), YV_INT(16)));
#line 149 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(73), YV_INT(16)));
#line 150 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(74), YV_INT(16)));
#line 151 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(75), YV_INT(16)));
#line 152 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(76), YV_INT(16)));
#line 153 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(77), YV_INT(16)));
#line 154 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(78), YV_INT(16)));
#line 155 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(79), YV_INT(16)));
#line 156 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(80), YV_INT(16)));
#line 157 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(81), YV_INT(16)));
#line 158 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(82), YV_INT(16)));
#line 159 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(83), YV_INT(16)));
#line 160 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(84), YV_INT(16)));
#line 161 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(85), YV_INT(16)));
#line 162 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(86), YV_INT(16)));
#line 163 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(87), YV_INT(16)));
#line 164 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(88), YV_INT(16)));
#line 165 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(89), YV_INT(16)));
#line 166 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(90), YV_INT(16)));
#line 167 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(91), YV_INT(16)));
#line 168 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(92), YV_INT(16)));
#line 169 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(93), YV_INT(16)));
#line 170 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(94), YV_INT(16)));
#line 171 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(95), YV_INT(16)));
#line 172 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(96), YV_INT(16)));
#line 173 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(97), YV_INT(16)));
#line 174 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(98), YV_INT(16)));
#line 175 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(99), YV_INT(16)));
#line 176 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(100), YV_INT(16)));
#line 177 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(101), YV_INT(16)));
#line 178 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(102), YV_INT(16)));
#line 179 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(103), YV_INT(16)));
#line 180 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(104), YV_INT(16)));
#line 181 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(105), YV_INT(16)));
#line 182 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(106), YV_INT(16)));
#line 183 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(107), YV_INT(16)));
#line 184 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(108), YV_INT(16)));
#line 185 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(109), YV_INT(16)));
#line 186 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(110), YV_INT(16)));
#line 187 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(111), YV_INT(16)));
#line 188 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(112), YV_INT(16)));
#line 189 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(113), YV_INT(16)));
#line 190 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(114), YV_INT(16)));
#line 191 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(115), YV_INT(16)));
#line 192 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(116), YV_INT(16)));
#line 193 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(117), YV_INT(16)));
#line 194 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(118), YV_INT(16)));
#line 195 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(119), YV_INT(16)));
#line 196 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(120), YV_INT(16)));
#line 197 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(121), YV_INT(16)));
#line 198 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(122), YV_INT(16)));
#line 199 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(123), YV_INT(16)));
#line 200 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(124), YV_INT(16)));
#line 201 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(125), YV_INT(16)));
#line 202 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(126), YV_INT(16)));
#line 203 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(127), YV_INT(16)));
#line 206 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(1), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(2), YV_INT(0), YV_INT(0), YV_INT(5), YV_INT(0), YV_INT(0), YV_INT(2), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 207 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(1), YV_INT(16)));
#line 209 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(2), yis_arr_lit(72, YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(0), YV_INT(32), YV_INT(0), YV_INT(2), YV_INT(32), YV_INT(0), YV_INT(4), YV_INT(32), YV_INT(0), YV_INT(15), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0), YV_INT(0))));
#line 210 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(2), YV_INT(16)));
#line 212 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_chr(v_scr, YV_INT(3), yis_arr_lit(72, YV_INT(128), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(255), YV_INT(128), YV_INT(0), YV_INT(255), YV_INT(192), YV_INT(0), YV_INT(255), YV_INT(224), YV_INT(0), YV_INT(255), YV_INT(240), YV_INT(0), YV_INT(255), YV_INT(240), YV_INT(0), YV_INT(255), YV_INT(224), YV_INT(0), YV_INT(255), YV_INT(192), YV_INT(0), YV_INT(255), YV_INT(128), YV_INT(0), YV_INT(255), YV_INT(0), YV_INT(0), YV_INT(254), YV_INT(0), YV_INT(0), YV_INT(252), YV_INT(0), YV_INT(0), YV_INT(248), YV_INT(0), YV_INT(0), YV_INT(240), YV_INT(0), YV_INT(0), YV_INT(224), YV_INT(0), YV_INT(0), YV_INT(192), YV_INT(0), YV_INT(0), YV_INT(128), YV_INT(0), YV_INT(0))));
#line 213 "/Users/nayu/Developer/Cogito/extras/Right/right_assets.yi"
  (void)(yis_m_vimana_screen_set_font_width(v_scr, YV_INT(3), YV_INT(16)));
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
static void yis_main_rename_file(YisVal, YisVal);
static void yis_main_open_from_selection(YisVal, YisVal);
static void yis_main_open_dir_listing(YisVal);
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
static YisVal __fnwrap_main_rename_file(void*,int,YisVal*);
static YisVal __fnwrap_main_open_from_selection(void*,int,YisVal*);
static YisVal __fnwrap_main_open_dir_listing(void*,int,YisVal*);
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
#line 20 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  return YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_sx, v_rx)) && yis_as_bool(yis_lt(v_sx, yis_add(v_rx, v_rw))))) && yis_as_bool(yis_ge(v_sy, v_ry)))) && yis_as_bool(yis_lt(v_sy, yis_add(v_ry, v_rh))));
}

static void yis_main_update_title(YisVal v_scr) {
#line 24 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_title = YV_STR(stdr_str_lit("Untitled")); yis_retain_val(v_title);
#line 25 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_file_path)), YV_INT(0)))) {
#line 26 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)((yis_move_into(&v_title, yis_stdr_basename(v_file_path)), v_title));
  }
#line 27 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_m_vimana_screen_set_titlebar_title(v_scr, v_title));
}

static void yis_main_load_file(YisVal v_fsys, YisVal v_scr) {
#line 33 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_file_path)), YV_INT(0)))) {
#line 34 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 35 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_m_vimana_file_exists(v_fsys, v_file_path))))) {
#line 36 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 37 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_txt = yis_m_vimana_file_read_text(v_fsys, v_file_path); yis_retain_val(v_txt);
#line 38 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_text(v_txt));
#line 39 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_caret(YV_INT(0)));
#line 40 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_selection(YV_INT(0), YV_INT(0)));
#line 41 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_scroll(YV_INT(0)));
#line 42 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_rebuild_outline());
#line 43 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_dirty(YV_BOOL(false)));
#line 44 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(v_dir_listing_active)) {
#line 45 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_close_dir_listing());
  }
#line 46 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_update_title(v_scr));
}

static void yis_main_save_file(YisVal v_fsys, YisVal v_scr) {
#line 50 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_file_path)), YV_INT(0)))) {
#line 51 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 52 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_m_vimana_file_write_text(v_fsys, v_file_path, v_editor_text));
#line 53 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_dirty(YV_BOOL(false)));
#line 54 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_update_title(v_scr));
}

static void yis_main_new_file(YisVal v_scr) {
#line 58 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_file_path(YV_STR(stdr_str_lit(""))));
#line 59 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_text(YV_STR(stdr_str_lit("cask main\n\nbring stdr\n\n--| Entry |--\n-> ()\n    -- your code here\n;\n"))));
#line 60 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_caret(YV_INT(0)));
#line 61 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_selection(YV_INT(0), YV_INT(0)));
#line 62 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_scroll(YV_INT(0)));
#line 63 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_rebuild_outline());
#line 64 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_dirty(YV_BOOL(false)));
#line 65 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_update_title(v_scr));
}

static void yis_main_open_file(YisVal v_fsys, YisVal v_scr) {
#line 69 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_picked = yis_stdr_open_file_dialog(YV_STR(stdr_str_lit("Open file")), YV_STR(stdr_str_lit("yi"))); yis_retain_val(v_picked);
#line 70 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(YV_BOOL(stdr_is_null(v_picked)))) {
#line 71 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 72 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_path = YV_STR(stdr_to_string(v_picked)); yis_retain_val(v_path);
#line 73 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_path)), YV_INT(0)))) {
#line 74 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 75 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_file_path(v_path));
#line 76 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_load_file(v_fsys, v_scr));
}

static void yis_main_rename_file(YisVal v_fsys, YisVal v_scr) {
#line 80 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_default_name = YV_STR(stdr_str_lit("Untitled.yi")); yis_retain_val(v_default_name);
#line 81 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_file_path)), YV_INT(0)))) {
#line 82 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)((yis_move_into(&v_default_name, yis_stdr_basename(v_file_path)), v_default_name));
  }
#line 83 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_picked = yis_stdr_save_file_dialog(YV_STR(stdr_str_lit("Rename / Save As")), v_default_name, YV_STR(stdr_str_lit("yi"))); yis_retain_val(v_picked);
#line 84 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(YV_BOOL(stdr_is_null(v_picked)))) {
#line 85 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 86 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_path = YV_STR(stdr_to_string(v_picked)); yis_retain_val(v_path);
#line 87 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_path)), YV_INT(0)))) {
#line 88 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 89 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_file_path(v_path));
#line 90 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_save_file(v_fsys, v_scr));
}

static void yis_main_open_from_selection(YisVal v_fsys, YisVal v_scr) {
#line 96 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_name = yis_right_model_selected_text(); yis_retain_val(v_name);
#line 97 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_name)), YV_INT(0)))) {
#line 98 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)((yis_move_into(&v_name, yis_right_model_word_before_caret()), v_name));
  }
#line 99 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_name)), YV_INT(0)))) {
#line 100 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 102 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_stdr_starts_with(v_name, YV_STR(stdr_str_lit("bring "))))) {
#line 103 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)((yis_move_into(&v_name, stdr_slice(v_name, yis_as_int(YV_INT(6)), yis_as_int(YV_INT(stdr_len(v_name))))), v_name));
  }
#line 105 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_ends_with(v_name, YV_STR(stdr_str_lit(".yi"))))))) {
#line 106 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)((yis_move_into(&v_name, yis_add(v_name, YV_STR(stdr_str_lit(".yi")))), v_name));
  }
#line 108 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_dir = yis_stdr_dirname(v_file_path); yis_retain_val(v_dir);
#line 109 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_dir)), YV_INT(0)))) {
#line 110 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)((yis_move_into(&v_dir, yis_stdr_getcwd()), v_dir));
  }
#line 111 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_path = yis_add(yis_add(v_dir, YV_STR(stdr_str_lit("/"))), v_name); yis_retain_val(v_path);
#line 112 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_m_vimana_file_exists(v_fsys, v_path))) {
#line 113 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_set_file_path(v_path));
#line 114 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_load_file(v_fsys, v_scr));
  }
}

static void yis_main_open_dir_listing(YisVal v_fsys) {
#line 120 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(v_dir_listing_active)) {
#line 121 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_close_dir_listing());
#line 122 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 123 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_dir = yis_stdr_dirname(v_file_path); yis_retain_val(v_dir);
#line 124 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_dir)), YV_INT(0)))) {
#line 125 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)((yis_move_into(&v_dir, yis_stdr_getcwd()), v_dir));
  }
#line 126 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_entries = yis_m_vimana_file_list(v_fsys, v_dir); yis_retain_val(v_entries);
#line 127 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_open_dir_listing(v_entries));
}

static void yis_main_exec_action(YisVal v_action, YisVal v_sys, YisVal v_fsys, YisVal v_scr) {
#line 131 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("new"))))) {
#line 132 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_new_file(v_scr));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("open"))))) {
#line 134 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_open_file(v_fsys, v_scr));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("save"))))) {
#line 136 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_save_file(v_fsys, v_scr));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("rename"))))) {
#line 138 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_rename_file(v_fsys, v_scr));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("exit"))))) {
#line 140 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_m_vimana_system_quit(v_sys));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("copy"))))) {
#line 142 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_sel = yis_right_model_selected_text(); yis_retain_val(v_sel);
#line 143 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_sel)), YV_INT(0)))) {
#line 144 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_m_vimana_system_set_clipboard_text(v_sys, v_sel));
    }
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("paste"))))) {
#line 146 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_clip = yis_m_vimana_system_clipboard_text(v_sys); yis_retain_val(v_clip);
#line 147 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_clip)), YV_INT(0)))) {
#line 148 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_insert_at_caret(v_clip));
#line 149 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_update_title(v_scr));
    }
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("cut"))))) {
#line 151 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_sel = yis_right_model_selected_text(); yis_retain_val(v_sel);
#line 152 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_sel)), YV_INT(0)))) {
#line 153 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_m_vimana_system_set_clipboard_text(v_sys, v_sel));
#line 154 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_delete_selection());
#line 155 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_update_title(v_scr));
    }
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("delete"))))) {
#line 157 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_delete_forward());
#line 158 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_update_title(v_scr));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("select_all"))))) {
#line 160 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_select_all());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("select_word"))))) {
#line 162 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_select_word());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("select_line"))))) {
#line 164 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_select_line());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("reset_selection"))))) {
#line 166 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_reset_selection());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("toggle_sidebar"))))) {
#line 168 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_toggle_sidebar());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("toggle_highlight"))))) {
#line 170 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_toggle_highlight());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("toggle_line_endings"))))) {
#line 172 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_toggle_line_endings());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("toggle_tabs"))))) {
#line 174 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_toggle_tabs());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("find"))))) {
#line 176 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_open_search());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("find_next"))))) {
#line 178 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_search_next());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("find_prev"))))) {
#line 180 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_search_prev());
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("strip"))))) {
#line 182 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_strip_trailing());
#line 183 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_update_title(v_scr));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("trim"))))) {
#line 185 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_trim_trailing_lines());
#line 186 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_update_title(v_scr));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("tab"))))) {
#line 188 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_tab_indent());
#line 189 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_update_title(v_scr));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("untab"))))) {
#line 191 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_untab_indent());
#line 192 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_update_title(v_scr));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("insert_date"))))) {
#line 194 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_y = yis_stdr_current_year(); yis_retain_val(v_y);
#line 195 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_m = yis_stdr_current_month(); yis_retain_val(v_m);
#line 196 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_d = yis_stdr_current_day(); yis_retain_val(v_d);
#line 197 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_ms = YV_STR(stdr_to_string(v_m)); yis_retain_val(v_ms);
#line 198 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_ds = YV_STR(stdr_to_string(v_d)); yis_retain_val(v_ds);
#line 199 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_lt(v_m, YV_INT(10)))) {
#line 200 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)((yis_move_into(&v_ms, yis_add(YV_STR(stdr_str_lit("0")), v_ms)), v_ms));
    }
#line 201 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_lt(v_d, YV_INT(10)))) {
#line 202 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)((yis_move_into(&v_ds, yis_add(YV_STR(stdr_str_lit("0")), v_ds)), v_ds));
    }
#line 203 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_insert_at_caret(yis_add(yis_add(yis_add(yis_add(YV_STR(stdr_to_string(v_y)), YV_STR(stdr_str_lit("-"))), v_ms), YV_STR(stdr_str_lit("-"))), v_ds)));
#line 204 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_update_title(v_scr));
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("insert_path"))))) {
#line 206 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_file_path)), YV_INT(0)))) {
#line 207 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_insert_at_caret(v_file_path));
#line 208 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_update_title(v_scr));
    }
  } else if (yis_as_bool(yis_eq(v_action, YV_STR(stdr_str_lit("insert_section"))))) {
#line 210 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_insert_at_caret(YV_STR(stdr_str_lit("--| Section |--\n"))));
#line 211 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_update_title(v_scr));
  }


























}

static void yis_main_on_mouse(YisVal v_dev, YisVal v_sys, YisVal v_fsys, YisVal v_scr) {
#line 218 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_mx = yis_m_vimana_device_pointer_x(v_dev); yis_retain_val(v_mx);
#line 219 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_my = yis_m_vimana_device_pointer_y(v_dev); yis_retain_val(v_my);
#line 220 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_ctrl = YV_BOOL(yis_as_bool(yis_m_vimana_device_key_down(v_dev, v_KEY_LCTRL)) || yis_as_bool(yis_m_vimana_device_key_down(v_dev, v_KEY_RCTRL))); yis_retain_val(v_ctrl);
#line 221 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_menu_handle_hover(v_mx, v_my));
#line 224 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(v_resizing_sidebar)) {
#line 225 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_mouse_down(v_dev, v_mouse_left))) {
#line 226 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_set_sidebar_width(yis_sub(v_mx, v_SIDEBAR_X)));
#line 227 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 228 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_stop_resize());
#line 229 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 231 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_m_vimana_device_mouse_pressed(v_dev, v_mouse_left))))) {
#line 232 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 235 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_action = yis_right_menu_handle_click(v_mx, v_my); yis_retain_val(v_action);
#line 236 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_ne(v_action, YV_STR(stdr_str_lit("pass"))))) {
#line 237 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_action)), YV_INT(0)))) {
#line 238 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_exec_action(v_action, v_sys, v_fsys, v_scr));
    }
#line 239 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 242 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(v_sidebar_visible) && yis_as_bool(yis_ge(v_my, v_CONTENT_Y))))) {
#line 243 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_sep = yis_right_model_sidebar_right(); yis_retain_val(v_sep);
#line 244 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_mx, yis_sub(v_sep, v_RESIZE_ZONE))) && yis_as_bool(yis_le(v_mx, yis_add(v_sep, v_RESIZE_ZONE)))))) {
#line 245 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_start_resize());
#line 246 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
  }
#line 249 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_lt(v_mx, v_SIDEBAR_X))) {
#line 250 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_bar_h = yis_sub(yis_sub(v_screen_h, v_CONTENT_Y), v_STATUS_H); yis_retain_val(v_bar_h);
#line 251 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_total = yis_right_model_vline_count(v_editor_text); yis_retain_val(v_total);
#line 252 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_ratio = yis_div(yis_sub(v_my, v_CONTENT_Y), v_bar_h); yis_retain_val(v_ratio);
#line 253 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_target = yis_stdr_floor(yis_mul(v_ratio, v_total)); yis_retain_val(v_target);
#line 254 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_set_scroll(v_target));
#line 255 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_clamp_scroll());
#line 256 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 259 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_ed_left = yis_right_model_editor_left(); yis_retain_val(v_ed_left);
#line 260 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_ge(v_mx, v_ed_left))) {
#line 262 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(v_ctrl)) {
#line 263 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      YisVal v_pos = yis_right_model_caret_from_editor_click(v_mx, v_my); yis_retain_val(v_pos);
#line 264 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_select_word_at(v_pos));
#line 265 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 267 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_now = yis_m_vimana_system_ticks(v_sys); yis_retain_val(v_now);
#line 268 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_dx = yis_sub(v_mx, v_last_click_x); yis_retain_val(v_dx);
#line 269 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_dy = yis_sub(v_my, v_last_click_y); yis_retain_val(v_dy);
#line 270 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_dist = yis_add(yis_mul(v_dx, v_dx), yis_mul(v_dy, v_dy)); yis_retain_val(v_dist);
#line 271 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_lt(yis_sub(v_now, v_last_click_time), v_DBLCLICK_MS)) && yis_as_bool(yis_lt(v_dist, yis_mul(v_DBLCLICK_DIST, v_DBLCLICK_DIST)))))) {
#line 272 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      YisVal v_pos = yis_right_model_caret_from_editor_click(v_mx, v_my); yis_retain_val(v_pos);
#line 273 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_select_word_at(v_pos));
#line 274 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)((yis_move_into(&v_last_click_time, YV_INT(0)), v_last_click_time));
#line 275 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 276 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)((yis_move_into(&v_last_click_time, v_now), v_last_click_time));
#line 277 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)((yis_move_into(&v_last_click_x, v_mx), v_last_click_x));
#line 278 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)((yis_move_into(&v_last_click_y, v_my), v_last_click_y));
#line 279 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_pos = yis_right_model_caret_from_editor_click(v_mx, v_my); yis_retain_val(v_pos);
#line 280 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_set_caret(v_pos));
#line 281 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_set_selection(v_pos, v_pos));
#line 282 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 285 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(v_sidebar_visible) && yis_as_bool(yis_ge(v_mx, v_SIDEBAR_X)))) && yis_as_bool(yis_lt(v_mx, yis_right_model_sidebar_right()))))) {
#line 287 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(v_dir_listing_active)) {
#line 288 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      YisVal v_row = yis_add(yis_stdr_floor(yis_div(yis_sub(v_my, v_CONTENT_Y), v_ROW_H)), v_dir_scroll); yis_retain_val(v_row);
#line 289 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_row, YV_INT(0))) && yis_as_bool(yis_lt(v_row, YV_INT(stdr_len(v_dir_entries))))))) {
#line 290 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
        YisVal v_entry = YV_STR(stdr_to_string(((yis_index(v_dir_entries, v_row)).tag == EVT_NULL ? (YV_STR(stdr_str_lit(""))) : (yis_index(v_dir_entries, v_row))))); yis_retain_val(v_entry);
#line 291 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
        YisVal v_dir = yis_stdr_dirname(v_file_path); yis_retain_val(v_dir);
#line 292 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
        if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_dir)), YV_INT(0)))) {
#line 293 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
          (void)((yis_move_into(&v_dir, yis_stdr_getcwd()), v_dir));
        }
#line 294 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
        YisVal v_path = yis_add(yis_add(v_dir, YV_STR(stdr_str_lit("/"))), v_entry); yis_retain_val(v_path);
#line 295 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
        if (yis_as_bool(yis_m_vimana_file_exists(v_fsys, v_path))) {
#line 296 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
          (void)(yis_right_model_set_file_path(v_path));
#line 297 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
          (void)(yis_main_load_file(v_fsys, v_scr));
#line 298 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
          (void)(yis_right_model_close_dir_listing());
        }
      }
#line 299 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 301 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    YisVal v_idx = yis_right_model_outline_index_from_click(v_my); yis_retain_val(v_idx);
#line 302 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_ge(v_idx, YV_INT(0)))) {
#line 303 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_set_outline_selected(v_idx));
#line 304 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      YisVal v_item = yis_index(v_current_outline, v_idx); yis_retain_val(v_item);
#line 305 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      YisVal v_line_any = ((yis_index(v_item, YV_STR(stdr_str_lit("line")))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_item, YV_STR(stdr_str_lit("line"))))); yis_retain_val(v_line_any);
#line 306 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      YisVal v_pos = yis_right_model_line_to_char_pos(v_editor_text, v_line_any); yis_retain_val(v_pos);
#line 307 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_set_caret(v_pos));
#line 308 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_set_selection(v_pos, v_pos));
#line 309 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_ensure_visible());
    }
  }
}

static void yis_main_on_key(YisVal v_dev, YisVal v_sys, YisVal v_fsys, YisVal v_scr) {
#line 315 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_cmd = YV_BOOL(yis_as_bool(yis_m_vimana_device_key_down(v_dev, v_KEY_LGUI)) || yis_as_bool(yis_m_vimana_device_key_down(v_dev, v_KEY_RGUI))); yis_retain_val(v_cmd);
#line 316 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_shift = YV_BOOL(yis_as_bool(yis_m_vimana_device_key_down(v_dev, YV_INT(225))) || yis_as_bool(yis_m_vimana_device_key_down(v_dev, YV_INT(229)))); yis_retain_val(v_shift);
#line 317 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_alt = YV_BOOL(yis_as_bool(yis_m_vimana_device_key_down(v_dev, v_KEY_LALT)) || yis_as_bool(yis_m_vimana_device_key_down(v_dev, v_KEY_RALT))); yis_retain_val(v_alt);
#line 318 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_ctrl = YV_BOOL(yis_as_bool(yis_m_vimana_device_key_down(v_dev, v_KEY_LCTRL)) || yis_as_bool(yis_m_vimana_device_key_down(v_dev, v_KEY_RCTRL))); yis_retain_val(v_ctrl);
#line 321 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(v_search_active)) {
#line 322 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_ESCAPE))) {
#line 323 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_close_search());
#line 324 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 325 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_RETURN))) {
#line 326 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_search_next());
#line 327 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 328 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_BACKSPACE))) {
#line 329 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_search_backspace());
#line 330 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 331 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(v_cmd) && yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_F))))) {
#line 332 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_close_search());
#line 333 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 334 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 337 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(v_cmd)) {
#line 338 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_S))) {
#line 339 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_save_file(v_fsys, v_scr));
#line 340 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 341 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_N))) {
#line 342 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_new_file(v_scr));
#line 343 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 344 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_O))) {
#line 345 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_open_file(v_fsys, v_scr));
#line 346 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 347 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_R))) {
#line 348 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_rename_file(v_fsys, v_scr));
#line 349 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 350 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_Q))) {
#line 351 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_m_vimana_system_quit(v_sys));
#line 352 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 353 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_F))) {
#line 354 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_open_search());
#line 355 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 356 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_G))) {
#line 357 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      if (yis_as_bool(v_shift)) {
#line 358 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
        (void)(yis_right_model_search_prev());
      } else {
#line 360 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
        (void)(yis_right_model_search_next());
      }
#line 361 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 362 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_A))) {
#line 363 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_select_all());
#line 364 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 365 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_W))) {
#line 366 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_select_word());
#line 367 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 368 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_L))) {
#line 369 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_select_line());
#line 370 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 371 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_T))) {
#line 372 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_strip_trailing());
#line 373 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_update_title(v_scr));
#line 374 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 375 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_I))) {
#line 376 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_trim_trailing_lines());
#line 377 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_update_title(v_scr));
#line 378 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 379 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_C))) {
#line 380 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      YisVal v_sel = yis_right_model_selected_text(); yis_retain_val(v_sel);
#line 381 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_sel)), YV_INT(0)))) {
#line 382 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
        (void)(yis_m_vimana_system_set_clipboard_text(v_sys, v_sel));
      }
#line 383 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 384 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_X))) {
#line 385 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      YisVal v_sel = yis_right_model_selected_text(); yis_retain_val(v_sel);
#line 386 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_sel)), YV_INT(0)))) {
#line 387 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
        (void)(yis_m_vimana_system_set_clipboard_text(v_sys, v_sel));
#line 388 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
        (void)(yis_right_model_delete_selection());
#line 389 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
        (void)(yis_main_update_title(v_scr));
      }
#line 390 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 391 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_V))) {
#line 392 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      YisVal v_clip = yis_m_vimana_system_clipboard_text(v_sys); yis_retain_val(v_clip);
#line 393 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_clip)), YV_INT(0)))) {
#line 394 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
        (void)(yis_right_model_insert_at_caret(v_clip));
#line 395 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
        (void)(yis_main_update_title(v_scr));
      }
#line 396 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 397 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_D))) {
#line 398 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_toggle_sidebar());
#line 399 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 400 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_H))) {
#line 401 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_toggle_highlight());
#line 402 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 404 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_LEFT))) {
#line 405 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_move_caret_line_start(v_shift));
#line 406 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 407 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_RIGHT))) {
#line 408 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_move_caret_line_end(v_shift));
#line 409 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 410 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_UP))) {
#line 411 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_move_caret_doc_start(v_shift));
#line 412 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 413 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_DOWN))) {
#line 414 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_move_caret_doc_end(v_shift));
#line 415 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 416 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_SLASH))) {
#line 417 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_open_dir_listing(v_fsys));
#line 418 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 419 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 422 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(v_alt)) {
#line 423 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_LEFT))) {
#line 424 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_move_caret_word_left(v_shift));
#line 425 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 426 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_RIGHT))) {
#line 427 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_move_caret_word_right(v_shift));
#line 428 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
  }
#line 431 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(v_ctrl) && yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_RETURN))))) {
#line 432 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_open_from_selection(v_fsys, v_scr));
#line 433 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 436 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_LEFT))) {
#line 437 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_move_caret_left(v_shift));
#line 438 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 439 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_RIGHT))) {
#line 440 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_move_caret_right(v_shift));
#line 441 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 442 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_UP))) {
#line 443 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_move_caret_up(v_shift));
#line 444 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 445 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_DOWN))) {
#line 446 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_move_caret_down(v_shift));
#line 447 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 450 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_BACKSPACE))) {
#line 451 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_delete_backward());
#line 452 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_update_autocomplete());
#line 453 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_update_title(v_scr));
#line 454 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 455 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_DELETE))) {
#line 456 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_delete_forward());
#line 457 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_update_autocomplete());
#line 458 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_update_title(v_scr));
#line 459 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 460 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_RETURN))) {
#line 461 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_dismiss_autocomplete());
#line 462 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_insert_at_caret(YV_STR(stdr_str_lit("\n"))));
#line 463 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_update_title(v_scr));
#line 464 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 465 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_TAB))) {
#line 466 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(v_shift)) {
#line 467 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_untab_indent());
#line 468 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_update_title(v_scr));
#line 469 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 470 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(v_autocomplete_visible)) {
#line 471 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_accept_autocomplete());
#line 472 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_update_title(v_scr));
#line 473 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 474 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_right_model_has_selection())) {
#line 475 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_tab_indent());
#line 476 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_main_update_title(v_scr));
#line 477 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 478 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_insert_at_caret(YV_STR(stdr_str_lit("    "))));
#line 479 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_update_title(v_scr));
#line 480 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 481 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_m_vimana_device_key_pressed(v_dev, v_KEY_ESCAPE))) {
#line 482 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(v_autocomplete_visible)) {
#line 483 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_dismiss_autocomplete());
#line 484 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 485 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_right_menu_is_open())) {
#line 486 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_menu_close());
#line 487 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 488 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(yis_right_model_has_selection())) {
#line 489 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_reset_selection());
#line 490 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 491 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_m_vimana_system_quit(v_sys));
#line 492 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
}

static void yis_main_on_text(YisVal v_dev, YisVal v_scr) {
#line 498 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_txt = yis_m_vimana_device_text_input(v_dev); yis_retain_val(v_txt);
#line 499 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_txt)), YV_INT(0)))) {
#line 500 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(v_search_active)) {
#line 501 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_search_type(v_txt));
#line 502 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      return;
    }
#line 503 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_insert_at_caret(v_txt));
#line 504 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_update_autocomplete());
#line 505 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_main_update_title(v_scr));
  }
}

static void yis_main_on_scroll(YisVal v_dev) {
#line 511 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_dy = yis_m_vimana_device_wheel_y(v_dev); yis_retain_val(v_dy);
#line 512 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(yis_eq(v_dy, YV_INT(0)))) {
#line 513 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    return;
  }
#line 514 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_mx = yis_m_vimana_device_pointer_x(v_dev); yis_retain_val(v_mx);
#line 515 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(v_sidebar_visible) && yis_as_bool(yis_lt(v_mx, yis_right_model_sidebar_right()))))) {
#line 516 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    if (yis_as_bool(v_dir_listing_active)) {
#line 517 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_dir_scroll_by(v_dy));
    } else {
#line 519 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
      (void)(yis_right_model_outline_scroll_by(v_dy));
    }
  } else {
#line 521 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
    (void)(yis_right_model_scroll_by(v_dy));
  }
}

static void yis_entry(void) {
#line 527 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_sys = yis_vimana_system(); yis_retain_val(v_sys);
#line 528 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_scr = yis_vimana_screen(YV_STR(stdr_str_lit("Right")), v_DEFAULT_W, v_DEFAULT_H, YV_INT(1)); yis_retain_val(v_scr);
#line 529 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_assets_setup(v_scr));
#line 530 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_dev = yis_m_vimana_system_device(v_sys); yis_retain_val(v_dev);
#line 531 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  YisVal v_fsys = yis_m_vimana_system_file(v_sys); yis_retain_val(v_fsys);
#line 533 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_m_vimana_screen_set_palette(yis_m_vimana_screen_set_palette(yis_m_vimana_screen_set_palette(yis_m_vimana_screen_set_palette(v_scr, v_color_bg, YV_STR(stdr_str_lit("#eeeedd"))), v_color_fg, YV_STR(stdr_str_lit("#000011"))), v_color_2, YV_STR(stdr_str_lit("#887777"))), v_color_3, YV_STR(stdr_str_lit("#44aa99"))));
#line 539 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_file_path(YV_STR(stdr_str_lit(""))));
#line 540 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_set_text(YV_STR(stdr_str_lit("cask main\n\nbring stdr\n\n--| Entry |--\n-> ()\n    -- your code here\n;\n"))));
#line 541 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_rebuild_outline());
#line 542 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_update_title(v_scr));
#line 544 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
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

static YisVal __fnwrap_main_rename_file(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_main_rename_file(__a0, __a1);
  return YV_NULLV;
}

static YisVal __fnwrap_main_open_from_selection(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_main_open_from_selection(__a0, __a1);
  return YV_NULLV;
}

static YisVal __fnwrap_main_open_dir_listing(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_main_open_dir_listing(__a0);
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
#line 545 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_m_vimana_device_poll(v_dev));
#line 546 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_right_model_sync_screen_size(yis_m_vimana_screen_width(v_scr), yis_m_vimana_screen_height(v_scr)));
#line 549 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_on_mouse(v_dev, v_sys, v_fsys, v_scr));
#line 550 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_on_key(v_dev, v_sys, v_fsys, v_scr));
#line 551 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_on_text(v_dev, v_scr));
#line 552 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
  (void)(yis_main_on_scroll(v_dev));
#line 555 "/Users/nayu/Developer/Cogito/extras/Right/right_main.yi"
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
  __yis_right_view_init();
  __yis_right_menu_init();
  __yis_main_init();
  yis_entry();
  return 0;
}
