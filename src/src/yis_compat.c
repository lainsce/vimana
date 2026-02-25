// Minimal Yis-compatible runtime for Cogito; mirrors Yis's runtime layout.
#include "yis_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void yis_obj_release(YisObj* o) {
  if (!o) return;
  o->ref--;
  if (o->ref == 0) {
    if (o->drop) o->drop(o);
    free(o);
  }
}

void* cogito_compat_obj_new(size_t size, void (*drop)(YisObj* o)) {
  YisObj* o = (YisObj*)calloc(1, size);
  if (!o) return NULL;
  o->ref = 1;
  o->drop = drop;
  return o;
}

void cogito_compat_retain_val(YisVal v) {
  if (v.tag == EVT_STR) ((YisStr*)v.as.p)->ref++;
  else if (v.tag == EVT_ARR) ((YisArr*)v.as.p)->ref++;
  else if (v.tag == EVT_OBJ) ((YisObj*)v.as.p)->ref++;
  else if (v.tag == EVT_FN) ((YisFn*)v.as.p)->ref++;
}

void cogito_compat_release_val(YisVal v) {
  if (v.tag == EVT_STR) {
    YisStr* s = (YisStr*)v.as.p;
    if (--s->ref == 0) {
      free(s->data);
      free(s);
    }
  } else if (v.tag == EVT_ARR) {
    YisArr* a = (YisArr*)v.as.p;
    if (--a->ref == 0) {
      for (size_t i = 0; i < a->len; i++) cogito_compat_release_val(a->items[i]);
      free(a->items);
      free(a);
    }
  } else if (v.tag == EVT_OBJ) {
    yis_obj_release((YisObj*)v.as.p);
  } else if (v.tag == EVT_FN) {
    YisFn* f = (YisFn*)v.as.p;
    if (--f->ref == 0) free(f);
  }
}

void cogito_compat_trap(const char* msg) {
  fprintf(stderr, "cogito error: %s\n", msg ? msg : "unknown error");
  fprintf(stderr, "  (run with debugger for stack trace)\n");
  abort();
}

int64_t cogito_compat_as_int(YisVal v) {
  switch (v.tag) {
    case EVT_INT: return v.as.i;
    case EVT_FLOAT: return (int64_t)v.as.f;
    case EVT_BOOL: return v.as.b ? 1 : 0;
    default: return 0;
  }
}

double cogito_compat_as_float(YisVal v) {
  switch (v.tag) {
    case EVT_FLOAT: return v.as.f;
    case EVT_INT: return (double)v.as.i;
    case EVT_BOOL: return v.as.b ? 1.0 : 0.0;
    default: return 0.0;
  }
}

bool cogito_compat_as_bool(YisVal v) {
  switch (v.tag) {
    case EVT_BOOL: return v.as.b;
    case EVT_INT: return v.as.i != 0;
    case EVT_FLOAT: return v.as.f != 0.0;
    default: return false;
  }
}

YisVal cogito_compat_call(YisVal fnv, int argc, YisVal* argv) {
  if (fnv.tag != EVT_FN) return EV_NULLV;
  YisFn* fn = (YisFn*)fnv.as.p;
  if (!fn || !fn->fn) return EV_NULLV;
  return fn->fn(fn->env, argc, argv);
}

YisStr* cogito_compat_str_from_slice(const char* s, size_t len) {
  YisStr* out = (YisStr*)calloc(1, sizeof(YisStr));
  if (!out) return NULL;
  out->ref = 1;
  out->data = (char*)malloc(len + 1);
  if (!out->data) return out;
  memcpy(out->data, s ? s : "", len);
  out->data[len] = 0;
  out->len = len;
  return out;
}

YisStr* cogito_compat_str_lit(const char* s) {
  size_t len = s ? strlen(s) : 0;
  return cogito_compat_str_from_slice(s ? s : "", len);
}

YisStr* cogito_compat_to_string(YisVal v) {
  if (v.tag == EVT_STR) return (YisStr*)v.as.p;
  if (v.tag == EVT_NULL) return NULL;
  char buf[64];
  if (v.tag == EVT_INT) {
    snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i);
  } else if (v.tag == EVT_FLOAT) {
    snprintf(buf, sizeof(buf), "%g", v.as.f);
  } else if (v.tag == EVT_BOOL) {
    snprintf(buf, sizeof(buf), "%s", v.as.b ? "true" : "false");
  } else {
    snprintf(buf, sizeof(buf), "");
  }
  return cogito_compat_str_from_slice(buf, strlen(buf));
}

YisArr* cogito_compat_arr_new(size_t len) {
  YisArr* a = (YisArr*)calloc(1, sizeof(YisArr));
  if (!a) return NULL;
  a->ref = 1;
  a->len = 0;
  a->cap = (len > 0) ? len : 4;
  a->items = (YisVal*)calloc(a->cap, sizeof(YisVal));
  return a;
}

void cogito_compat_arr_set(YisArr* a, size_t idx, YisVal v) {
  if (!a || idx >= a->len) return;
  cogito_compat_release_val(a->items[idx]);
  a->items[idx] = v;
}

YisVal cogito_compat_arr_get(YisArr* a, int64_t idx) {
  if (!a || idx < 0 || (size_t)idx >= a->len) return EV_NULLV;
  YisVal v = a->items[idx];
  cogito_compat_retain_val(v);
  return v;
}
