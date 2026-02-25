// Minimal Ergo-compatible runtime for Cogito; mirrors Ergo's runtime layout.
#include "ergo_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ergo_obj_release(ErgoObj* o) {
  if (!o) return;
  o->ref--;
  if (o->ref == 0) {
    if (o->drop) o->drop(o);
    free(o);
  }
}

void* cogito_compat_obj_new(size_t size, void (*drop)(ErgoObj* o)) {
  ErgoObj* o = (ErgoObj*)calloc(1, size);
  if (!o) return NULL;
  o->ref = 1;
  o->drop = drop;
  return o;
}

void cogito_compat_retain_val(ErgoVal v) {
  if (v.tag == EVT_STR) ((ErgoStr*)v.as.p)->ref++;
  else if (v.tag == EVT_ARR) ((ErgoArr*)v.as.p)->ref++;
  else if (v.tag == EVT_OBJ) ((ErgoObj*)v.as.p)->ref++;
  else if (v.tag == EVT_FN) ((ErgoFn*)v.as.p)->ref++;
}

void cogito_compat_release_val(ErgoVal v) {
  if (v.tag == EVT_STR) {
    ErgoStr* s = (ErgoStr*)v.as.p;
    if (--s->ref == 0) {
      free(s->data);
      free(s);
    }
  } else if (v.tag == EVT_ARR) {
    ErgoArr* a = (ErgoArr*)v.as.p;
    if (--a->ref == 0) {
      for (size_t i = 0; i < a->len; i++) cogito_compat_release_val(a->items[i]);
      free(a->items);
      free(a);
    }
  } else if (v.tag == EVT_OBJ) {
    ergo_obj_release((ErgoObj*)v.as.p);
  } else if (v.tag == EVT_FN) {
    ErgoFn* f = (ErgoFn*)v.as.p;
    if (--f->ref == 0) free(f);
  }
}

void cogito_compat_trap(const char* msg) {
  fprintf(stderr, "cogito error: %s\n", msg ? msg : "unknown error");
  fprintf(stderr, "  (run with debugger for stack trace)\n");
  abort();
}

int64_t cogito_compat_as_int(ErgoVal v) {
  switch (v.tag) {
    case EVT_INT: return v.as.i;
    case EVT_FLOAT: return (int64_t)v.as.f;
    case EVT_BOOL: return v.as.b ? 1 : 0;
    default: return 0;
  }
}

double cogito_compat_as_float(ErgoVal v) {
  switch (v.tag) {
    case EVT_FLOAT: return v.as.f;
    case EVT_INT: return (double)v.as.i;
    case EVT_BOOL: return v.as.b ? 1.0 : 0.0;
    default: return 0.0;
  }
}

bool cogito_compat_as_bool(ErgoVal v) {
  switch (v.tag) {
    case EVT_BOOL: return v.as.b;
    case EVT_INT: return v.as.i != 0;
    case EVT_FLOAT: return v.as.f != 0.0;
    default: return false;
  }
}

ErgoVal cogito_compat_call(ErgoVal fnv, int argc, ErgoVal* argv) {
  if (fnv.tag != EVT_FN) return EV_NULLV;
  ErgoFn* fn = (ErgoFn*)fnv.as.p;
  if (!fn || !fn->fn) return EV_NULLV;
  return fn->fn(fn->env, argc, argv);
}

ErgoStr* cogito_compat_str_from_slice(const char* s, size_t len) {
  ErgoStr* out = (ErgoStr*)calloc(1, sizeof(ErgoStr));
  if (!out) return NULL;
  out->ref = 1;
  out->data = (char*)malloc(len + 1);
  if (!out->data) return out;
  memcpy(out->data, s ? s : "", len);
  out->data[len] = 0;
  out->len = len;
  return out;
}

ErgoStr* cogito_compat_str_lit(const char* s) {
  size_t len = s ? strlen(s) : 0;
  return cogito_compat_str_from_slice(s ? s : "", len);
}

ErgoStr* cogito_compat_to_string(ErgoVal v) {
  if (v.tag == EVT_STR) return (ErgoStr*)v.as.p;
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

ErgoArr* cogito_compat_arr_new(size_t len) {
  ErgoArr* a = (ErgoArr*)calloc(1, sizeof(ErgoArr));
  if (!a) return NULL;
  a->ref = 1;
  a->len = 0;
  a->cap = (len > 0) ? len : 4;
  a->items = (ErgoVal*)calloc(a->cap, sizeof(ErgoVal));
  return a;
}

void cogito_compat_arr_set(ErgoArr* a, size_t idx, ErgoVal v) {
  if (!a || idx >= a->len) return;
  cogito_compat_release_val(a->items[idx]);
  a->items[idx] = v;
}

ErgoVal cogito_compat_arr_get(ErgoArr* a, int64_t idx) {
  if (!a || idx < 0 || (size_t)idx >= a->len) return EV_NULLV;
  ErgoVal v = a->items[idx];
  cogito_compat_retain_val(v);
  return v;
}
