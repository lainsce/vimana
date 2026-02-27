// Minimal Yis-compatible runtime for Cogito; mirrors Yis's runtime layout.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  EVT_NULL = 0,
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

typedef struct YisObj {
  int ref;
  void (*drop)(struct YisObj* o);
} YisObj;

/* YisVal must be fully defined before YisDictEnt (which embeds it by value)
   and before YisFn (which uses it in a function-pointer signature). */
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

typedef struct YisFn {
  int ref;
  int arity;
  YisVal (*fn)(void* env, int argc, YisVal* argv);
  void* env;
} YisFn;

typedef YisVal (*YisFnImpl)(void* env, int argc, YisVal* argv);

#define YV_NULLV      ((YisVal){ .tag = EVT_NULL })
#define YV_INT(v)     ((YisVal){ .tag = EVT_INT,   .as.i = (int64_t)(v) })
#define YV_FLOAT(v)   ((YisVal){ .tag = EVT_FLOAT, .as.f = (double)(v)  })
#define YV_BOOL(v)    ((YisVal){ .tag = EVT_BOOL,  .as.b = (bool)(v)    })
#define YV_STR(v)     ((YisVal){ .tag = EVT_STR,   .as.p = (void*)(v)   })
#define YV_ARR(v)     ((YisVal){ .tag = EVT_ARR,   .as.p = (void*)(v)   })
#define YV_DICT(v)    ((YisVal){ .tag = EVT_DICT,  .as.p = (void*)(v)   })
#define YV_OBJ(v)     ((YisVal){ .tag = EVT_OBJ,   .as.p = (void*)(v)   })
#define YV_FN(v)      ((YisVal){ .tag = EVT_FN,    .as.p = (void*)(v)   })

void* cogito_compat_obj_new(size_t size, void (*drop)(YisObj* o));
void cogito_compat_retain_val(YisVal v);
void cogito_compat_release_val(YisVal v);
void cogito_compat_trap(const char* msg);

int64_t cogito_compat_as_int(YisVal v);
double  cogito_compat_as_float(YisVal v);
bool    cogito_compat_as_bool(YisVal v);

YisVal cogito_compat_call(YisVal fnv, int argc, YisVal* argv);

YisStr* cogito_compat_str_from_slice(const char* s, size_t len);
YisStr* cogito_compat_str_lit(const char* s);
YisStr* cogito_compat_to_string(YisVal v);

YisArr* cogito_compat_arr_new(size_t len);
void    cogito_compat_arr_set(YisArr* a, size_t idx, YisVal v);
YisVal  cogito_compat_arr_get(YisArr* a, int64_t idx);