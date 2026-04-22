/* vinemu.c — vinemu: Yis ROM virtual machine
 *
 *  Usage:  vinemu [-v] file.rom [args...]
 *
 *  Executes the bytecode produced by vinasm. Provides the standard
 *  library (stdr.*) and Vimana as built-in external functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include "vinrom.h"
#include "../src/c/vimana.h"

/* ═══════════════════════════════ Value type ════════════════════════ */

typedef enum { VT_NULL=0, VT_INT, VT_FLT, VT_BOOL, VT_STR, VT_ARR, VT_OBJ, VT_FUNC } VType;
typedef enum {
    OBJ_SYSTEM=1, OBJ_SCREEN, OBJ_DEVICE, OBJ_FILE, OBJ_DATETIME,
    OBJ_CONSOLE, OBJ_PROCESS
} ObjKind;

typedef struct Val Val;
typedef struct { Val *items; int len, cap; int ref; } Arr;
typedef struct { ObjKind kind; void *ptr; } Obj;
typedef struct { int func_idx; Val *captures; int ncaptures; } FnRef;

struct Val {
    VType type;
    union {
        int64_t i;
        double  f;
        bool    b;
        struct  { char *s; int len; int ref; } str;
        Arr    *arr;
        Obj     obj;
        FnRef   fn;
    };
};

static Val V_NULL  = {VT_NULL, .i=0};
static Val V_TRUE  = {VT_BOOL, .b=true};
static Val V_FALSE = {VT_BOOL, .b=false};
static Val V_INT(int64_t v) { return (Val){VT_INT,.i=v}; }
static Val V_FLT(double  v) { return (Val){VT_FLT,.f=v}; }
static Val V_BOOL(bool   v) { return v?V_TRUE:V_FALSE; }
static Val V_OBJ(ObjKind kind, void *ptr) { return ptr ? (Val){VT_OBJ,.obj={kind,ptr}} : V_NULL; }
static Val V_FUNC(int func_idx, Val *captures, int ncaptures) {
    Val v; v.type=VT_FUNC; v.fn.func_idx=func_idx; v.fn.ncaptures=ncaptures;
    v.fn.captures=NULL;
    if (ncaptures>0) {
        v.fn.captures=malloc(sizeof(Val)*(size_t)ncaptures);
        memcpy(v.fn.captures,captures,sizeof(Val)*(size_t)ncaptures);
    }
    return v;
}
static Val V_STR_COPY(const char *s, int len) {
    Val v; v.type=VT_STR; v.str.s=malloc((size_t)len+1);
    memcpy(v.str.s,s,(size_t)len); v.str.s[len]=0;
    v.str.len=len; v.str.ref=1; return v;
}
static Val V_STR_CSTR(const char *s) { return V_STR_COPY(s,(int)strlen(s)); }
static Val V_ARR_NEW(void) {
    Val v; v.type=VT_ARR; v.arr=malloc(sizeof(Arr));
    v.arr->items=NULL; v.arr->len=0; v.arr->cap=0; v.arr->ref=1; return v;
}
static Val val_clone(Val v);
static void val_release(Val v);

static Val val_clone(Val v) {
    switch(v.type) {
    case VT_STR: return V_STR_COPY(v.str.s,v.str.len);
    case VT_ARR: if(v.arr) v.arr->ref++; return v;
    case VT_FUNC: {
        Val r; r.type=VT_FUNC; r.fn.func_idx=v.fn.func_idx; r.fn.ncaptures=v.fn.ncaptures;
        r.fn.captures=NULL;
        if (v.fn.ncaptures>0) {
            r.fn.captures=malloc(sizeof(Val)*(size_t)v.fn.ncaptures);
            for(int i=0;i<v.fn.ncaptures;i++) r.fn.captures[i]=val_clone(v.fn.captures[i]);
        }
        return r;
    }
    default: return v;
    }
}

static void val_release(Val v) {
    switch(v.type) {
    case VT_STR:
        free(v.str.s);
        break;
    case VT_ARR:
        if(v.arr&&--v.arr->ref<=0) {
            for(int i=0;i<v.arr->len;i++) val_release(v.arr->items[i]);
            free(v.arr->items);
            free(v.arr);
        }
        break;
    case VT_FUNC:
        for(int i=0;v.fn.captures&&i<v.fn.ncaptures;i++) val_release(v.fn.captures[i]);
        free(v.fn.captures);
        break;
    default:
        break;
    }
}

static bool val_truthy(Val v) {
    switch(v.type){
    case VT_NULL: return false;
    case VT_BOOL: return v.b;
    case VT_INT:  return v.i!=0;
    case VT_FLT:  return v.f!=0.0;
    case VT_STR:  return v.str.len>0;
    case VT_ARR:  return v.arr->len>0;
    case VT_OBJ:  return v.obj.ptr!=NULL;
    case VT_FUNC: return v.fn.func_idx>=0;
    }
    return false;
}
static bool val_isnull(Val v) { return v.type==VT_NULL; }

static bool val_eq(Val a, Val b) {
    if (a.type!=b.type) {
        if (a.type==VT_INT&&b.type==VT_FLT) return (double)a.i==b.f;
        if (a.type==VT_FLT&&b.type==VT_INT) return a.f==(double)b.i;
        return false;
    }
    switch(a.type){
    case VT_NULL: return true;
    case VT_BOOL: return a.b==b.b;
    case VT_INT:  return a.i==b.i;
    case VT_FLT:  return a.f==b.f;
    case VT_STR:  return a.str.len==b.str.len&&!memcmp(a.str.s,b.str.s,(size_t)a.str.len);
    case VT_ARR:  return a.arr==b.arr;
    case VT_OBJ:  return a.obj.kind==b.obj.kind&&a.obj.ptr==b.obj.ptr;
    case VT_FUNC: return a.fn.func_idx==b.fn.func_idx&&a.fn.captures==b.fn.captures;
    }
    return false;
}

static double val_to_f64(Val v) {
    if (v.type==VT_INT) return (double)v.i;
    if (v.type==VT_FLT) return v.f;
    return 0.0;
}
static int64_t val_to_i64(Val v) {
    if (v.type==VT_INT) return v.i;
    if (v.type==VT_FLT) return (int64_t)v.f;
    return 0;
}
static char *val_to_str(Val v, char *buf, int bufsz);

static Val val_add(Val a, Val b) {
    if (a.type==VT_STR||b.type==VT_STR) {
        char abuf[256], bbuf[256];
        const char *as = a.type==VT_STR ? a.str.s : val_to_str(a,abuf,sizeof(abuf));
        const char *bs = b.type==VT_STR ? b.str.s : val_to_str(b,bbuf,sizeof(bbuf));
        int la = a.type==VT_STR ? a.str.len : (int)strlen(as);
        int lb = b.type==VT_STR ? b.str.len : (int)strlen(bs);
        Val r; r.type=VT_STR; r.str.s=malloc((size_t)(la+lb+1));
        memcpy(r.str.s,as,(size_t)la); memcpy(r.str.s+la,bs,(size_t)lb); r.str.s[la+lb]=0;
        r.str.len=la+lb; r.str.ref=1; return r;
    }
    if (a.type==VT_FLT||b.type==VT_FLT) return V_FLT(val_to_f64(a)+val_to_f64(b));
    return V_INT(a.i+val_to_i64(b));
}
static Val val_sub(Val a,Val b){if(a.type==VT_FLT||b.type==VT_FLT)return V_FLT(val_to_f64(a)-val_to_f64(b));return V_INT(val_to_i64(a)-val_to_i64(b));}
static Val val_mul(Val a,Val b){if(a.type==VT_FLT||b.type==VT_FLT)return V_FLT(val_to_f64(a)*val_to_f64(b));return V_INT(val_to_i64(a)*val_to_i64(b));}
static Val val_div(Val a,Val b){
    double db=val_to_f64(b); if(db==0.0){fprintf(stderr,"div by zero\n");return V_INT(0);}
    if(a.type==VT_FLT||b.type==VT_FLT)return V_FLT(val_to_f64(a)/db);
    int64_t ib=val_to_i64(b); if(ib==0){fprintf(stderr,"div by zero\n");return V_INT(0);}
    return V_INT(val_to_i64(a)/ib);
}
static bool val_lt(Val a,Val b){
    if(a.type==VT_STR&&b.type==VT_STR)return strcmp(a.str.s,b.str.s)<0;
    return val_to_f64(a)<val_to_f64(b);
}

static void arr_push(Arr *a, Val v) {
    if (a->len>=a->cap) { a->cap=a->cap?a->cap*2:8; a->items=realloc(a->items,sizeof(Val)*(size_t)a->cap); }
    a->items[a->len++]=v;
}

static char *val_to_str(Val v, char *buf, int bufsz) {
    switch(v.type){
    case VT_NULL: snprintf(buf,bufsz,"null"); break;
    case VT_BOOL: snprintf(buf,bufsz,"%s",v.b?"true":"false"); break;
    case VT_INT:  snprintf(buf,bufsz,"%lld",(long long)v.i); break;
    case VT_FLT:  snprintf(buf,bufsz,"%g",v.f); break;
    case VT_STR:  snprintf(buf,bufsz,"%s",v.str.s); break;
    case VT_ARR:  snprintf(buf,bufsz,"[array len=%d]",v.arr->len); break;
    case VT_OBJ:  snprintf(buf,bufsz,"[object %d]",v.obj.kind); break;
    case VT_FUNC: snprintf(buf,bufsz,"[function %d]",v.fn.func_idx); break;
    }
    return buf;
}

/* ════════════════════════════════ ROM loader ═══════════════════════ */

typedef struct { uint8_t type; int64_t ival; double fval; char *sval; int slen; } RCon;
typedef struct { int name_idx; int init_idx; } RGlob;
typedef struct { int name_idx; } RExt;
typedef struct { int name_idx, arity, nlocals; uint8_t *code; int code_sz; } RFunc;

typedef struct {
    RCon  *consts; int nconsts;
    RFunc *funcs;  int nfuncs;
    RGlob *globals; int nglobals;
    RExt  *exts;   int nexts;
    int    entry_func;
    Val   *glob_vals;   /* runtime global values */
} ROM;

static uint8_t  ru8 (const uint8_t *b,int *p){return b[(*p)++];}
static uint16_t ru16(const uint8_t *b,int *p){uint16_t v=b[*p]|(b[*p+1]<<8);(*p)+=2;return v;}
static uint32_t ru32(const uint8_t *b,int *p){uint32_t v=b[*p]|(b[*p+1]<<8)|(b[*p+2]<<16)|((uint32_t)b[*p+3]<<24);(*p)+=4;return v;}
static int64_t  ri64(const uint8_t *b,int *p){int64_t v;memcpy(&v,b+*p,8);(*p)+=8;return v;}
static double   rf64(const uint8_t *b,int *p){double v;memcpy(&v,b+*p,8);(*p)+=8;return v;}

static ROM *rom_load(const char *path) {
    FILE *f=fopen(path,"rb"); if(!f){fprintf(stderr,"cannot open '%s'\n",path);return NULL;}
    fseek(f,0,SEEK_END); long fsz=ftell(f); rewind(f);
    uint8_t *buf=malloc((size_t)fsz); (void)fread(buf,1,(size_t)fsz,f); fclose(f);
    int p=0;
    uint32_t magic=ru32(buf,&p); if(magic!=YROM_MAGIC){fprintf(stderr,"bad magic\n");return NULL;}
    uint16_t ver=ru16(buf,&p); (void)ver; ru16(buf,&p); /* flags */
    ROM *r=calloc(1,sizeof(ROM));
    r->nconsts=(int)ru32(buf,&p); r->nfuncs=(int)ru32(buf,&p);
    r->nglobals=(int)ru32(buf,&p); r->nexts=(int)ru32(buf,&p);
    r->entry_func=(int)(int32_t)ru32(buf,&p);
    r->consts=calloc((size_t)r->nconsts,sizeof(RCon));
    r->funcs=calloc((size_t)r->nfuncs,sizeof(RFunc));
    r->globals=calloc((size_t)r->nglobals,sizeof(RGlob));
    r->exts=calloc((size_t)r->nexts,sizeof(RExt));
    r->glob_vals=calloc((size_t)r->nglobals,sizeof(Val));
    /* constants */
    for(int i=0;i<r->nconsts;i++){
        RCon *k=&r->consts[i]; k->type=ru8(buf,&p);
        switch(k->type){
        case YCON_INT:k->ival=ri64(buf,&p);break;
        case YCON_FLT:k->fval=rf64(buf,&p);break;
        case YCON_STR:k->slen=(int)ru32(buf,&p);k->sval=malloc((size_t)k->slen+1);memcpy(k->sval,buf+p,(size_t)k->slen);k->sval[k->slen]=0;p+=k->slen;break;
        case YCON_BOOL:k->ival=ru8(buf,&p);break;
        case YCON_NULL:break;
        }
    }
    /* globals */
    for(int i=0;i<r->nglobals;i++){r->globals[i].name_idx=(int)ru32(buf,&p);r->globals[i].init_idx=(int)(int32_t)ru32(buf,&p);}
    /* exts */
    for(int i=0;i<r->nexts;i++) r->exts[i].name_idx=(int)ru32(buf,&p);
    /* functions */
    for(int i=0;i<r->nfuncs;i++){
        RFunc *fn=&r->funcs[i];
        fn->name_idx=(int)ru32(buf,&p); fn->arity=(int)ru16(buf,&p); fn->nlocals=(int)ru16(buf,&p);
        fn->code_sz=(int)ru32(buf,&p); fn->code=malloc((size_t)fn->code_sz);
        memcpy(fn->code,buf+p,(size_t)fn->code_sz); p+=fn->code_sz;
    }
    free(buf); return r;
}

/* ══════════════════════════════ VM execution ═══════════════════════ */

#define STACK_SIZE  4096
#define FRAME_SIZE   256

typedef struct {
    int     func_idx;
    Val    *locals;   /* parameter + local slots */
    int     nlocs;
    uint8_t *code;
    int     code_sz;
    int     pc;
} Frame;

typedef struct {
    ROM    *rom;
    int     argc;
    char  **argv;
    Val     wst[STACK_SIZE]; int wsp;
    Frame   rst[FRAME_SIZE]; int rsp;
} VM;

typedef struct { VM *vm; Val frame; } FrameCtx;

static Val wpop(VM *vm) {
    if (vm->wsp<=0){fprintf(stderr,"working stack underflow\n");return V_NULL;}
    Val v=vm->wst[--vm->wsp];
    vm->wst[vm->wsp]=V_NULL;
    return v;
}
static void wpush(VM *vm, Val v) {
    if (vm->wsp>=STACK_SIZE){fprintf(stderr,"working stack overflow\n");exit(1);}
    vm->wst[vm->wsp++]=v;
}
static Val wpeek(VM *vm) { return vm->wsp>0?vm->wst[vm->wsp-1]:V_NULL; }

static void frame_release(Frame *f) {
    if (!f) return;
    for(int i=0;f->locals&&i<f->nlocs;i++) val_release(f->locals[i]);
    free(f->locals);
    memset(f,0,sizeof(*f));
}

static bool rpush_frame(VM *vm, int func_idx, Val *args, int argc, bool clone_args) {
    if (func_idx<0||func_idx>=vm->rom->nfuncs) { fprintf(stderr,"bad func %d\n",func_idx); return false; }
    if (vm->rsp>=FRAME_SIZE) { fprintf(stderr,"return stack overflow\n"); exit(1); }
    RFunc *fn=&vm->rom->funcs[func_idx];
    Frame *f=&vm->rst[vm->rsp++];
    memset(f,0,sizeof(*f));
    f->func_idx=func_idx;
    f->nlocs=fn->arity+fn->nlocals;
    f->locals=calloc((size_t)f->nlocs,sizeof(Val));
    for(int i=0;i<argc&&i<fn->arity;i++) f->locals[i]=clone_args?val_clone(args[i]):args[i];
    if(!clone_args) for(int i=fn->arity;i<argc;i++) val_release(args[i]);
    f->code=fn->code;
    f->code_sz=fn->code_sz;
    f->pc=0;
    return true;
}

/* Forward declaration for recursive calls */
static Val vm_call(VM *vm, int func_idx, Val *args, int argc);

/* ══════════════════════════════ Built-ins ══════════════════════════ */

static void vimana_frame_bridge(vimana_system *system, vimana_screen *screen, void *user) {
    (void)system; (void)screen;
    FrameCtx *ctx=(FrameCtx *)user;
    if (!ctx||ctx->frame.type!=VT_FUNC) return;
    Val r=vm_call(ctx->vm,ctx->frame.fn.func_idx,ctx->frame.fn.captures,ctx->frame.fn.ncaptures);
    val_release(r);
}

static bool obj_is(Val v, ObjKind kind) {
    return v.type==VT_OBJ&&v.obj.kind==kind&&v.obj.ptr;
}

static vimana_system *obj_system(Val v) {
    if (v.type!=VT_OBJ) return NULL;
    if (v.obj.kind==OBJ_SYSTEM||v.obj.kind==OBJ_DEVICE||
        v.obj.kind==OBJ_FILE||v.obj.kind==OBJ_DATETIME||
        v.obj.kind==OBJ_CONSOLE)
        return (vimana_system *)v.obj.ptr;
    return NULL;
}

static const char *val_cstr(Val v, char *buf, int bufsz) {
    if (v.type==VT_STR) return v.str.s;
    return val_to_str(v,buf,bufsz);
}

static int val_u16_array(Val v, uint16_t *out, int max) {
    if (v.type!=VT_ARR) return 0;
    int n=v.arr->len<max?v.arr->len:max;
    for(int i=0;i<n;i++) out[i]=(uint16_t)val_to_i64(v.arr->items[i]);
    return n;
}

static int val_byte_array(Val v, uint8_t *out, int max) {
    if (v.type!=VT_ARR) return 0;
    int arr_len=v.arr->len;
    if (arr_len==0) return 0;
    int all_small=1;
    for(int i=0;i<arr_len;i++) {
        int64_t val=val_to_i64(v.arr->items[i]);
        if (val>=0x100) { all_small=0; }
    }
    if (all_small) {
        int n=0;
        for(int i=0;i<arr_len && n<max; i++)
            out[n++]=(uint8_t)val_to_i64(v.arr->items[i]);
        return n;
    }
    int n=0;
    for(int i=0;i<arr_len&&n<max;i++) {
        uint16_t word=(uint16_t)val_to_i64(v.arr->items[i]);
        if (n<max) out[n++]=(uint8_t)(word>>8);
        if (n<max) out[n++]=(uint8_t)(word&0xff);
    }
    return n;
}

static Val bytes_to_array(const unsigned char *bytes, size_t len) {
    Val a=V_ARR_NEW();
    for(size_t i=0;i<len;i++) arr_push(a.arr,V_INT(bytes[i]));
    return a;
}

static Val strings_to_array(char **items, int count) {
    Val a=V_ARR_NEW();
    for(int i=0;i<count;i++) arr_push(a.arr,V_STR_CSTR(items[i]?items[i]:""));
    return a;
}

static Val read_text_file_val(const char *path) {
    FILE *f=fopen(path,"rb");
    if(!f) return V_NULL;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return V_NULL;}
    long sz=ftell(f);
    if(sz<0){fclose(f);return V_NULL;}
    rewind(f);
    char *buf=malloc((size_t)sz+1);
    if(!buf){fclose(f);return V_NULL;}
    size_t n=sz>0?fread(buf,1,(size_t)sz,f):0;
    bool failed=sz>0&&n!=(size_t)sz&&ferror(f);
    fclose(f);
    if(failed){free(buf);return V_NULL;}
    Val v=V_STR_COPY(buf,(int)n);
    free(buf);
    return v;
}

static bool write_text_file_val(const char *path, Val text) {
    FILE *f=fopen(path,"wb");
    if(!f) return false;
    size_t len=0, n=0;
    if(text.type==VT_STR) {
        len=(size_t)text.str.len;
        n=fwrite(text.str.s,1,len,f);
    }
    else {
        char buf[1024];
        const char *s=val_to_str(text,buf,sizeof(buf));
        len=strlen(s);
        n=fwrite(s,1,len,f);
    }
    fclose(f);
    return n==len;
}

static Val prompt_path_dialog(const char *kind, Val titlev, Val defaultv) {
    char title[256], def[4096], line[4096];
    const char *t=val_cstr(titlev,title,sizeof(title));
    const char *d=val_cstr(defaultv,def,sizeof(def));
    fprintf(stderr,"%s",kind);
    if(t&&t[0]) fprintf(stderr," [%s]",t);
    if(d&&d[0]) fprintf(stderr," default='%s'",d);
    fprintf(stderr,": ");
    fflush(stderr);
    if(!fgets(line,sizeof(line),stdin)) return d&&d[0]?V_STR_CSTR(d):V_STR_CSTR("");
    size_t n=strlen(line);
    while(n>0&&(line[n-1]=='\n'||line[n-1]=='\r')) line[--n]=0;
    if(n==0&&d&&d[0]) return V_STR_CSTR(d);
    return V_STR_CSTR(line);
}

static Val vimana_const(const char *name) {
    if (!strcmp(name,"vimana.key_escape")) return V_INT(41);
    if (!strcmp(name,"vimana.mouse_left")) return V_INT(1);
    if (!strcmp(name,"vimana.mouse_right")) return V_INT(3);
    if (!strcmp(name,"vimana.controller_a")) return V_INT(0x01);
    if (!strcmp(name,"vimana.controller_b")) return V_INT(0x02);
    if (!strcmp(name,"vimana.controller_select")) return V_INT(0x04);
    if (!strcmp(name,"vimana.controller_start")) return V_INT(0x08);
    if (!strcmp(name,"vimana.controller_up")) return V_INT(0x10);
    if (!strcmp(name,"vimana.controller_down")) return V_INT(0x20);
    if (!strcmp(name,"vimana.controller_left")) return V_INT(0x40);
    if (!strcmp(name,"vimana.controller_right")) return V_INT(0x80);
    if (!strcmp(name,"vimana.console_std")) return V_INT(0x01);
    if (!strcmp(name,"vimana.console_arg")) return V_INT(0x02);
    if (!strcmp(name,"vimana.console_eoa")) return V_INT(0x03);
    if (!strcmp(name,"vimana.console_end")) return V_INT(0x04);
    if (!strcmp(name,"vimana.color_bg")) return V_INT(0);
    if (!strcmp(name,"vimana.color_fg")) return V_INT(1);
    if (!strncmp(name,"vimana.color_",13)) return V_INT(atoi(name+13));
    if (!strcmp(name,"vimana.sprite_1bpp")) return V_INT(0);
    if (!strcmp(name,"vimana.sprite_2bpp")) return V_INT(1);
    if (!strcmp(name,"vimana.sprite_3bpp")) return V_INT(2);
    if (!strcmp(name,"vimana.sprite_4bpp")) return V_INT(3);
    if (!strcmp(name,"vimana.layer_bg")) return V_INT(0);
    if (!strcmp(name,"vimana.layer_fg")) return V_INT(0x40);
    if (!strcmp(name,"vimana.flip_x")) return V_INT(0x10);
    if (!strcmp(name,"vimana.flip_y")) return V_INT(0x20);
    if (!strcmp(name,"vimana.sprite_bank_count")) return V_INT(16);
    if (!strcmp(name,"vimana.gfx_size")) return V_INT(0x60000);
    if (!strcmp(name,"vimana.wave_triangle")) return V_INT(0);
    if (!strcmp(name,"vimana.wave_sawtooth")) return V_INT(1);
    if (!strcmp(name,"vimana.wave_pulse")) return V_INT(2);
    if (!strcmp(name,"vimana.wave_noise")) return V_INT(3);
    if (!strcmp(name,"vimana.wave_psg")) return V_INT(4);
    if (!strcmp(name,"vimana.voice_count")) return V_INT(8);
    if (!strcmp(name,"vimana.filter_lp")) return V_INT(1);
    if (!strcmp(name,"vimana.filter_bp")) return V_INT(2);
    if (!strcmp(name,"vimana.filter_hp")) return V_INT(4);
    if (!strcmp(name,"vimana.audio_clock")) return V_INT(1024000);
    if (!strcmp(name,"vimana.paddle_count")) return V_INT(2);
    return V_NULL;
}

static Val builtin_vimana(const char *name, Val *args, int argc) {
    if (!strcmp(name,"vimana.system")) return V_OBJ(OBJ_SYSTEM,vimana_system_new());
    if (!strcmp(name,"vimana.screen")) {
        char title[256];
        const char *t=argc>0?val_cstr(args[0],title,sizeof(title)):"Vimana";
        unsigned int w=argc>1?(unsigned int)val_to_i64(args[1]):320;
        unsigned int h=argc>2?(unsigned int)val_to_i64(args[2]):240;
        unsigned int s=argc>3?(unsigned int)val_to_i64(args[3]):1;
        return V_OBJ(OBJ_SCREEN,vimana_screen_new(t,w,h,s));
    }
    return vimana_const(name);
}

typedef enum {
    VMT_NONE=0,
    VMT_RUN, VMT_QUIT, VMT_FILE, VMT_DEVICE, VMT_DATETIME,
    VMT_CONSOLE, VMT_TICKS, VMT_SLEEP, VMT_CLIPBOARD_TEXT,
    VMT_SET_CLIPBOARD_TEXT, VMT_HOME_DIR, VMT_PLAY_TONE,
    VMT_SET_VOICE, VMT_SET_ENVELOPE, VMT_SET_PULSE_WIDTH,
    VMT_PLAY_VOICE, VMT_STOP_VOICE, VMT_SET_FREQUENCY,
    VMT_SET_SYNC, VMT_SET_RING_MOD, VMT_SET_FILTER,
    VMT_SET_FILTER_ROUTE, VMT_SET_MASTER_VOLUME, VMT_BEGIN_AUDIO,
    VMT_END_AUDIO, VMT_SET_PADDLE, VMT_PADDLE, VMT_SPAWN,
    VMT_PROC_WRITE, VMT_PROC_READ_LINE, VMT_PROC_RUNNING,
    VMT_PROC_KILL, VMT_PROC_FREE,
    VMT_CLEAR, VMT_SET_PALETTE, VMT_SET_FONT, VMT_SET_FONT_WIDTH,
    VMT_SET_FONT_SIZE, VMT_SET_THEME_SWAP, VMT_SET_SPRITE,
    VMT_SET_GFX, VMT_GFX, VMT_SET_X, VMT_SET_Y, VMT_SET_ADDR,
    VMT_SET_AUTO, VMT_SET_SPRITE_BANK, VMT_SPRITE_BANK,
    VMT_SPRITE, VMT_PIXEL, VMT_PUT_TEXT, VMT_PUT_ICN,
    VMT_PRESENT, VMT_SET_DRAG_REGION, VMT_SET_CURSOR,
    VMT_HIDE_CURSOR, VMT_SHOW_CURSOR, VMT_X, VMT_Y, VMT_ADDR,
    VMT_AUTO, VMT_WIDTH, VMT_HEIGHT, VMT_SCALE,
    VMT_EXISTS, VMT_READ_BYTES, VMT_READ_TEXT, VMT_WRITE_BYTES,
    VMT_WRITE_TEXT, VMT_REMOVE, VMT_RENAME, VMT_LIST, VMT_IS_DIR,
    VMT_POLL, VMT_CONTROLLER, VMT_CONTROLLER_DOWN,
    VMT_CONTROLLER_PRESSED, VMT_KEY_DOWN, VMT_KEY_PRESSED,
    VMT_MOUSE_DOWN, VMT_MOUSE_PRESSED, VMT_POINTER_X, VMT_POINTER_Y,
    VMT_TILE_X, VMT_TILE_Y, VMT_WHEEL_X, VMT_WHEEL_Y,
    VMT_TEXT_INPUT,
    VMT_NOW, VMT_YEAR, VMT_MONTH, VMT_DAY, VMT_HOUR,
    VMT_MINUTE, VMT_SECOND, VMT_WEEKDAY, VMT_YDAY, VMT_DST,
    VMT_YEAR_AT, VMT_MONTH_AT, VMT_DAY_AT, VMT_HOUR_AT,
    VMT_MINUTE_AT, VMT_SECOND_AT, VMT_WEEKDAY_AT, VMT_YDAY_AT,
    VMT_DST_AT,
    VMT_PENDING, VMT_INPUT, VMT_TYPE, VMT_NEXT, VMT_PUSH,
    VMT_STDOUT, VMT_STDERR, VMT_STDERR_HEX
} VimanaMethodId;

typedef struct { ObjKind kind; const char *name; VimanaMethodId id; } VimanaMethod;

static const VimanaMethod VIMANA_METHODS[] = {
    {OBJ_SYSTEM,"run",VMT_RUN},{OBJ_SYSTEM,"quit",VMT_QUIT},
    {OBJ_SYSTEM,"file",VMT_FILE},{OBJ_SYSTEM,"device",VMT_DEVICE},
    {OBJ_SYSTEM,"datetime",VMT_DATETIME},{OBJ_SYSTEM,"console",VMT_CONSOLE},
    {OBJ_SYSTEM,"ticks",VMT_TICKS},{OBJ_SYSTEM,"sleep",VMT_SLEEP},
    {OBJ_SYSTEM,"clipboard_text",VMT_CLIPBOARD_TEXT},
    {OBJ_SYSTEM,"set_clipboard_text",VMT_SET_CLIPBOARD_TEXT},
    {OBJ_SYSTEM,"home_dir",VMT_HOME_DIR},{OBJ_SYSTEM,"play_tone",VMT_PLAY_TONE},
    {OBJ_SYSTEM,"set_voice",VMT_SET_VOICE},{OBJ_SYSTEM,"set_envelope",VMT_SET_ENVELOPE},
    {OBJ_SYSTEM,"set_pulse_width",VMT_SET_PULSE_WIDTH},
    {OBJ_SYSTEM,"play_voice",VMT_PLAY_VOICE},{OBJ_SYSTEM,"stop_voice",VMT_STOP_VOICE},
    {OBJ_SYSTEM,"set_frequency",VMT_SET_FREQUENCY},{OBJ_SYSTEM,"set_sync",VMT_SET_SYNC},
    {OBJ_SYSTEM,"set_ring_mod",VMT_SET_RING_MOD},{OBJ_SYSTEM,"set_filter",VMT_SET_FILTER},
    {OBJ_SYSTEM,"set_filter_route",VMT_SET_FILTER_ROUTE},
    {OBJ_SYSTEM,"set_master_volume",VMT_SET_MASTER_VOLUME},
    {OBJ_SYSTEM,"begin_audio",VMT_BEGIN_AUDIO},{OBJ_SYSTEM,"end_audio",VMT_END_AUDIO},
    {OBJ_SYSTEM,"set_paddle",VMT_SET_PADDLE},{OBJ_SYSTEM,"paddle",VMT_PADDLE},
    {OBJ_SYSTEM,"spawn",VMT_SPAWN},{OBJ_SYSTEM,"proc_write",VMT_PROC_WRITE},
    {OBJ_SYSTEM,"proc_read_line",VMT_PROC_READ_LINE},
    {OBJ_SYSTEM,"proc_running",VMT_PROC_RUNNING},{OBJ_SYSTEM,"proc_kill",VMT_PROC_KILL},
    {OBJ_SYSTEM,"proc_free",VMT_PROC_FREE},
    {OBJ_SCREEN,"clear",VMT_CLEAR},{OBJ_SCREEN,"set_palette",VMT_SET_PALETTE},
    {OBJ_SCREEN,"set_font",VMT_SET_FONT},{OBJ_SCREEN,"set_font_width",VMT_SET_FONT_WIDTH},
    {OBJ_SCREEN,"set_font_size",VMT_SET_FONT_SIZE},
    {OBJ_SCREEN,"set_theme_swap",VMT_SET_THEME_SWAP},
    {OBJ_SCREEN,"set_sprite",VMT_SET_SPRITE},{OBJ_SCREEN,"set_gfx",VMT_SET_GFX},
    {OBJ_SCREEN,"gfx",VMT_GFX},{OBJ_SCREEN,"set_x",VMT_SET_X},
    {OBJ_SCREEN,"set_y",VMT_SET_Y},{OBJ_SCREEN,"set_addr",VMT_SET_ADDR},
    {OBJ_SCREEN,"set_auto",VMT_SET_AUTO},{OBJ_SCREEN,"set_sprite_bank",VMT_SET_SPRITE_BANK},
    {OBJ_SCREEN,"sprite_bank",VMT_SPRITE_BANK},{OBJ_SCREEN,"sprite",VMT_SPRITE},
    {OBJ_SCREEN,"pixel",VMT_PIXEL},{OBJ_SCREEN,"put_text",VMT_PUT_TEXT},
    {OBJ_SCREEN,"put_icn",VMT_PUT_ICN},{OBJ_SCREEN,"present",VMT_PRESENT},
    {OBJ_SCREEN,"set_drag_region",VMT_SET_DRAG_REGION},
    {OBJ_SCREEN,"set_cursor",VMT_SET_CURSOR},{OBJ_SCREEN,"hide_cursor",VMT_HIDE_CURSOR},
    {OBJ_SCREEN,"show_cursor",VMT_SHOW_CURSOR},{OBJ_SCREEN,"x",VMT_X},
    {OBJ_SCREEN,"y",VMT_Y},{OBJ_SCREEN,"addr",VMT_ADDR},
    {OBJ_SCREEN,"auto",VMT_AUTO},{OBJ_SCREEN,"width",VMT_WIDTH},
    {OBJ_SCREEN,"height",VMT_HEIGHT},{OBJ_SCREEN,"scale",VMT_SCALE},
    {OBJ_FILE,"exists",VMT_EXISTS},{OBJ_FILE,"read_bytes",VMT_READ_BYTES},
    {OBJ_FILE,"read_text",VMT_READ_TEXT},{OBJ_FILE,"write_bytes",VMT_WRITE_BYTES},
    {OBJ_FILE,"write_text",VMT_WRITE_TEXT},{OBJ_FILE,"remove",VMT_REMOVE},
    {OBJ_FILE,"rename",VMT_RENAME},{OBJ_FILE,"list",VMT_LIST},
    {OBJ_FILE,"is_dir",VMT_IS_DIR},
    {OBJ_DEVICE,"poll",VMT_POLL},{OBJ_DEVICE,"controller",VMT_CONTROLLER},
    {OBJ_DEVICE,"controller_down",VMT_CONTROLLER_DOWN},
    {OBJ_DEVICE,"controller_pressed",VMT_CONTROLLER_PRESSED},
    {OBJ_DEVICE,"key_down",VMT_KEY_DOWN},{OBJ_DEVICE,"key_pressed",VMT_KEY_PRESSED},
    {OBJ_DEVICE,"mouse_down",VMT_MOUSE_DOWN},{OBJ_DEVICE,"mouse_pressed",VMT_MOUSE_PRESSED},
    {OBJ_DEVICE,"pointer_x",VMT_POINTER_X},{OBJ_DEVICE,"pointer_y",VMT_POINTER_Y},
    {OBJ_DEVICE,"tile_x",VMT_TILE_X},{OBJ_DEVICE,"tile_y",VMT_TILE_Y},
    {OBJ_DEVICE,"wheel_x",VMT_WHEEL_X},{OBJ_DEVICE,"wheel_y",VMT_WHEEL_Y},
    {OBJ_DEVICE,"text_input",VMT_TEXT_INPUT},
    {OBJ_DATETIME,"now",VMT_NOW},{OBJ_DATETIME,"year",VMT_YEAR},
    {OBJ_DATETIME,"month",VMT_MONTH},{OBJ_DATETIME,"day",VMT_DAY},
    {OBJ_DATETIME,"hour",VMT_HOUR},{OBJ_DATETIME,"minute",VMT_MINUTE},
    {OBJ_DATETIME,"second",VMT_SECOND},{OBJ_DATETIME,"weekday",VMT_WEEKDAY},
    {OBJ_DATETIME,"yday",VMT_YDAY},{OBJ_DATETIME,"dst",VMT_DST},
    {OBJ_DATETIME,"year_at",VMT_YEAR_AT},{OBJ_DATETIME,"month_at",VMT_MONTH_AT},
    {OBJ_DATETIME,"day_at",VMT_DAY_AT},{OBJ_DATETIME,"hour_at",VMT_HOUR_AT},
    {OBJ_DATETIME,"minute_at",VMT_MINUTE_AT},{OBJ_DATETIME,"second_at",VMT_SECOND_AT},
    {OBJ_DATETIME,"weekday_at",VMT_WEEKDAY_AT},{OBJ_DATETIME,"yday_at",VMT_YDAY_AT},
    {OBJ_DATETIME,"dst_at",VMT_DST_AT},
    {OBJ_CONSOLE,"pending",VMT_PENDING},{OBJ_CONSOLE,"input",VMT_INPUT},
    {OBJ_CONSOLE,"type",VMT_TYPE},{OBJ_CONSOLE,"next",VMT_NEXT},
    {OBJ_CONSOLE,"push",VMT_PUSH},{OBJ_CONSOLE,"stdout",VMT_STDOUT},
    {OBJ_CONSOLE,"stderr",VMT_STDERR},{OBJ_CONSOLE,"stderr_hex",VMT_STDERR_HEX},
};

static VimanaMethodId vimana_method_id(ObjKind kind, const char *name) {
    size_t n=sizeof(VIMANA_METHODS)/sizeof(VIMANA_METHODS[0]);
    for(size_t i=0;i<n;i++)
        if(VIMANA_METHODS[i].kind==kind&&!strcmp(VIMANA_METHODS[i].name,name))
            return VIMANA_METHODS[i].id;
    return VMT_NONE;
}

static Val builtin_chain(VM *vm, const char *name, Val *args, int argc) {
    if (argc<=0) return V_NULL;
    const char *m=name+10;
    Val self=args[0];
    if (self.type!=VT_OBJ) {
        fprintf(stderr,"warning: method '%s' on non-object (argc=%d)\n",m,argc);
        return val_clone(self);
    }

    vimana_system *sys=obj_system(self);
    vimana_screen *scr=obj_is(self,OBJ_SCREEN)?(vimana_screen *)self.obj.ptr:NULL;
    VimanaMethodId id=vimana_method_id(self.obj.kind,m);

    switch(id) {
    case VMT_RUN: {
        vimana_screen *s=(argc>1&&obj_is(args[1],OBJ_SCREEN))?(vimana_screen *)args[1].obj.ptr:NULL;
        FrameCtx ctx={vm,argc>2?val_clone(args[2]):V_NULL};
        vimana_system_run(sys,s,vimana_frame_bridge,&ctx);
        val_release(ctx.frame);
        return val_clone(self);
    }
    case VMT_QUIT: vimana_system_quit(sys); return val_clone(self);
    case VMT_FILE: return V_OBJ(OBJ_FILE,sys);
    case VMT_DEVICE: return V_OBJ(OBJ_DEVICE,sys);
    case VMT_DATETIME: return V_OBJ(OBJ_DATETIME,sys);
    case VMT_CONSOLE: return V_OBJ(OBJ_CONSOLE,sys);
    case VMT_TICKS: return V_INT(vimana_system_ticks(sys));
    case VMT_SLEEP: if(argc>1)vimana_system_sleep(sys,val_to_i64(args[1])); return val_clone(self);
    case VMT_CLIPBOARD_TEXT: { char *s=vimana_system_clipboard_text(sys); Val r=s?V_STR_CSTR(s):V_STR_CSTR(""); free(s); return r; }
    case VMT_SET_CLIPBOARD_TEXT: { char b[4096]; return V_BOOL(argc>1&&vimana_system_set_clipboard_text(sys,val_cstr(args[1],b,sizeof(b)))); }
    case VMT_HOME_DIR: { char *s=vimana_system_home_dir(sys); Val r=s?V_STR_CSTR(s):V_STR_CSTR(""); free(s); return r; }
    case VMT_PLAY_TONE: if(argc>3)vimana_system_play_tone(sys,(int)val_to_i64(args[1]),(int)val_to_i64(args[2]),(int)val_to_i64(args[3])); return val_clone(self);
    case VMT_SET_VOICE: if(argc>2)vimana_system_set_voice(sys,(int)val_to_i64(args[1]),(int)val_to_i64(args[2])); return val_clone(self);
    case VMT_SET_ENVELOPE: if(argc>5)vimana_system_set_envelope(sys,(int)val_to_i64(args[1]),(int)val_to_i64(args[2]),(int)val_to_i64(args[3]),(int)val_to_i64(args[4]),(int)val_to_i64(args[5])); return val_clone(self);
    case VMT_SET_PULSE_WIDTH: if(argc>2)vimana_system_set_pulse_width(sys,(int)val_to_i64(args[1]),(int)val_to_i64(args[2])); return val_clone(self);
    case VMT_PLAY_VOICE: if(argc>3)vimana_system_play_voice(sys,(int)val_to_i64(args[1]),(int)val_to_i64(args[2]),(int)val_to_i64(args[3])); return val_clone(self);
    case VMT_STOP_VOICE: if(argc>1)vimana_system_stop_voice(sys,(int)val_to_i64(args[1])); return val_clone(self);
    case VMT_SET_FREQUENCY: if(argc>2)vimana_system_set_frequency(sys,(int)val_to_i64(args[1]),(int)val_to_i64(args[2])); return val_clone(self);
    case VMT_SET_SYNC: if(argc>2)vimana_system_set_sync(sys,(int)val_to_i64(args[1]),(int)val_to_i64(args[2])); return val_clone(self);
    case VMT_SET_RING_MOD: if(argc>2)vimana_system_set_ring_mod(sys,(int)val_to_i64(args[1]),(int)val_to_i64(args[2])); return val_clone(self);
    case VMT_SET_FILTER: if(argc>3)vimana_system_set_filter(sys,(int)val_to_i64(args[1]),(int)val_to_i64(args[2]),(int)val_to_i64(args[3])); return val_clone(self);
    case VMT_SET_FILTER_ROUTE: if(argc>2)vimana_system_set_filter_route(sys,(int)val_to_i64(args[1]),(int)val_to_i64(args[2])); return val_clone(self);
    case VMT_SET_MASTER_VOLUME: if(argc>1)vimana_system_set_master_volume(sys,(int)val_to_i64(args[1])); return val_clone(self);
    case VMT_BEGIN_AUDIO: vimana_system_begin_audio(sys); return val_clone(self);
    case VMT_END_AUDIO: vimana_system_end_audio(sys); return val_clone(self);
    case VMT_SET_PADDLE: if(argc>2)vimana_system_set_paddle(sys,(int)val_to_i64(args[1]),(int)val_to_i64(args[2])); return val_clone(self);
    case VMT_PADDLE: return argc>1?V_INT(vimana_system_get_paddle(sys,(int)val_to_i64(args[1]))):V_INT(0);
    case VMT_SPAWN: { char b[4096]; return argc>1?V_OBJ(OBJ_PROCESS,vimana_process_spawn(sys,val_cstr(args[1],b,sizeof(b)))):V_NULL; }
    case VMT_PROC_WRITE: { char b[4096]; return V_BOOL(argc>2&&obj_is(args[1],OBJ_PROCESS)&&vimana_process_write((vimana_process *)args[1].obj.ptr,val_cstr(args[2],b,sizeof(b)))); }
    case VMT_PROC_READ_LINE: { char *s=(argc>1&&obj_is(args[1],OBJ_PROCESS))?vimana_process_read_line((vimana_process *)args[1].obj.ptr):NULL; Val r=s?V_STR_CSTR(s):V_STR_CSTR(""); free(s); return r; }
    case VMT_PROC_RUNNING: return V_BOOL(argc>1&&obj_is(args[1],OBJ_PROCESS)&&vimana_process_running((vimana_process *)args[1].obj.ptr));
    case VMT_PROC_KILL: if(argc>1&&obj_is(args[1],OBJ_PROCESS))vimana_process_kill((vimana_process *)args[1].obj.ptr); return val_clone(self);
    case VMT_PROC_FREE: if(argc>1&&obj_is(args[1],OBJ_PROCESS))vimana_process_free((vimana_process *)args[1].obj.ptr); return val_clone(self);

    case VMT_CLEAR: vimana_screen_clear(scr,argc>1?(unsigned int)val_to_i64(args[1]):0); return val_clone(self);
    case VMT_SET_PALETTE: { char b[64]; if(argc>2)vimana_screen_set_palette(scr,(unsigned int)val_to_i64(args[1]),val_cstr(args[2],b,sizeof(b))); return val_clone(self); }
    case VMT_SET_FONT: { uint16_t data[256]; int n=argc>2?val_u16_array(args[2],data,256):0; if(argc>2)vimana_screen_set_font(scr,(unsigned int)val_to_i64(args[1]),data,(unsigned int)n); return val_clone(self); }
    case VMT_SET_FONT_WIDTH: if(argc>2)vimana_screen_set_font_width(scr,(unsigned int)val_to_i64(args[1]),(unsigned int)val_to_i64(args[2])); return val_clone(self);
    case VMT_SET_FONT_SIZE: if(argc>1)vimana_screen_set_font_size(scr,(unsigned int)val_to_i64(args[1])); return val_clone(self);
    case VMT_SET_THEME_SWAP: if(argc>1)vimana_screen_set_theme_swap(scr,val_truthy(args[1])); return val_clone(self);
    case VMT_SET_SPRITE: { uint8_t data[4096]; int n=argc>2?val_byte_array(args[2],data,4096):0; unsigned int mode=argc>3?(unsigned int)val_to_i64(args[3]):0; if(argc>2)vimana_screen_set_sprite(scr,(unsigned int)val_to_i64(args[1]),data,mode,(size_t)n); return val_clone(self); }
    case VMT_SET_GFX: { uint8_t data[4096]; int n=argc>2?val_byte_array(args[2],data,4096):0; if(argc>2)vimana_screen_set_gfx(scr,(unsigned int)val_to_i64(args[1]),data,(unsigned int)n); return val_clone(self); }
    case VMT_GFX: { const uint8_t *p=argc>1?vimana_screen_gfx(scr,(unsigned int)val_to_i64(args[1])):NULL; return V_INT(p?*p:0); }
    case VMT_SET_X: if(argc>1)vimana_screen_set_x(scr,(unsigned int)val_to_i64(args[1])); return val_clone(self);
    case VMT_SET_Y: if(argc>1)vimana_screen_set_y(scr,(unsigned int)val_to_i64(args[1])); return val_clone(self);
    case VMT_SET_ADDR: if(argc>1)vimana_screen_set_addr(scr,(unsigned int)val_to_i64(args[1])); return val_clone(self);
    case VMT_SET_AUTO: if(argc>1)vimana_screen_set_auto(scr,(unsigned int)val_to_i64(args[1])); return val_clone(self);
    case VMT_SET_SPRITE_BANK: if(argc>1)vimana_screen_set_sprite_bank(scr,(unsigned int)val_to_i64(args[1])); return val_clone(self);
    case VMT_SPRITE_BANK: return V_INT(vimana_screen_sprite_bank(scr));
    case VMT_SPRITE: if(argc>1)vimana_screen_sprite(scr,(unsigned int)val_to_i64(args[1])); return val_clone(self);
    case VMT_PIXEL: if(argc>1)vimana_screen_pixel(scr,(unsigned int)val_to_i64(args[1])); return val_clone(self);
    case VMT_PUT_TEXT: { char text[1024]; if(argc>5)vimana_screen_put_text(scr,(unsigned int)val_to_i64(args[1]),(unsigned int)val_to_i64(args[2]),val_cstr(args[3],text,sizeof(text)),(unsigned int)val_to_i64(args[4]),(unsigned int)val_to_i64(args[5])); return val_clone(self); }
    case VMT_PUT_ICN: { uint8_t data[8]; if(argc>5){val_byte_array(args[3],data,8); vimana_screen_put_icn(scr,(unsigned int)val_to_i64(args[1]),(unsigned int)val_to_i64(args[2]),data,(unsigned int)val_to_i64(args[4]),(unsigned int)val_to_i64(args[5]));} return val_clone(self); }
    case VMT_PRESENT: vimana_screen_present(scr); return val_clone(self);
    case VMT_SET_DRAG_REGION: if(argc>1)vimana_screen_set_drag_region(scr,(unsigned int)val_to_i64(args[1])); return val_clone(self);
    case VMT_SET_CURSOR: { uint8_t data[8]; if(argc>1){val_byte_array(args[1],data,8); vimana_screen_set_cursor(scr,data);} return val_clone(self); }
    case VMT_HIDE_CURSOR: vimana_screen_hide_cursor(scr); return val_clone(self);
    case VMT_SHOW_CURSOR: vimana_screen_show_cursor(scr); return val_clone(self);
    case VMT_X: return V_INT(vimana_screen_x(scr));
    case VMT_Y: return V_INT(vimana_screen_y(scr));
    case VMT_ADDR: return V_INT(vimana_screen_addr(scr));
    case VMT_AUTO: return V_INT(vimana_screen_auto(scr));
    case VMT_WIDTH: return V_INT(vimana_screen_width(scr));
    case VMT_HEIGHT: return V_INT(vimana_screen_height(scr));
    case VMT_SCALE: return V_INT(vimana_screen_scale(scr));

    case VMT_EXISTS: { char path[1024]; return V_BOOL(argc>1&&vimana_file_exists(sys,val_cstr(args[1],path,sizeof(path)))); }
    case VMT_READ_BYTES: { char path[1024]; size_t n=0; unsigned char *bytes=argc>1?vimana_file_read_bytes(sys,val_cstr(args[1],path,sizeof(path)),&n):NULL; Val r=bytes?bytes_to_array(bytes,n):V_ARR_NEW(); free(bytes); return r; }
    case VMT_READ_TEXT: { char path[1024]; char *s=argc>1?vimana_file_read_text(sys,val_cstr(args[1],path,sizeof(path))):NULL; Val r=s?V_STR_CSTR(s):V_STR_CSTR(""); free(s); return r; }
    case VMT_WRITE_BYTES: { char path[1024]; uint8_t data[65536]; int n=argc>2?val_byte_array(args[2],data,65536):0; return V_BOOL(argc>2&&vimana_file_write_bytes(sys,val_cstr(args[1],path,sizeof(path)),data,(size_t)n)); }
    case VMT_WRITE_TEXT: { char path[1024], text[4096]; return V_BOOL(argc>2&&vimana_file_write_text(sys,val_cstr(args[1],path,sizeof(path)),val_cstr(args[2],text,sizeof(text)))); }
    case VMT_REMOVE: { char path[1024]; return V_BOOL(argc>1&&vimana_file_remove(sys,val_cstr(args[1],path,sizeof(path)))); }
    case VMT_RENAME: { char path[1024], to[1024]; return V_BOOL(argc>2&&vimana_file_rename(sys,val_cstr(args[1],path,sizeof(path)),val_cstr(args[2],to,sizeof(to)))); }
    case VMT_LIST: { char path[1024]; int count=0; char **items=argc>1?vimana_file_list(sys,val_cstr(args[1],path,sizeof(path)),&count):NULL; Val r=items?strings_to_array(items,count):V_ARR_NEW(); if(items)vimana_file_list_free(items,count); return r; }
    case VMT_IS_DIR: { char path[1024]; return V_BOOL(argc>1&&vimana_file_is_dir(sys,val_cstr(args[1],path,sizeof(path)))); }

    case VMT_POLL: vimana_device_poll(sys); return val_clone(self);
    case VMT_CONTROLLER: return V_INT(vimana_device_controller(sys));
    case VMT_CONTROLLER_DOWN: return V_BOOL(argc>1&&vimana_device_controller_down(sys,(unsigned int)val_to_i64(args[1])));
    case VMT_CONTROLLER_PRESSED: return V_BOOL(argc>1&&vimana_device_controller_pressed(sys,(unsigned int)val_to_i64(args[1])));
    case VMT_KEY_DOWN: return V_BOOL(argc>1&&vimana_device_key_down(sys,(int)val_to_i64(args[1])));
    case VMT_KEY_PRESSED: return V_BOOL(argc>1&&vimana_device_key_pressed(sys,(int)val_to_i64(args[1])));
    case VMT_MOUSE_DOWN: return V_BOOL(argc>1&&vimana_device_mouse_down(sys,(int)val_to_i64(args[1])));
    case VMT_MOUSE_PRESSED: return V_BOOL(argc>1&&vimana_device_mouse_pressed(sys,(int)val_to_i64(args[1])));
    case VMT_POINTER_X: return V_INT(vimana_device_pointer_x(sys));
    case VMT_POINTER_Y: return V_INT(vimana_device_pointer_y(sys));
    case VMT_TILE_X: return V_INT(vimana_device_tile_x(sys));
    case VMT_TILE_Y: return V_INT(vimana_device_tile_y(sys));
    case VMT_WHEEL_X: return V_INT(vimana_device_wheel_x(sys));
    case VMT_WHEEL_Y: return V_INT(vimana_device_wheel_y(sys));
    case VMT_TEXT_INPUT: return V_STR_CSTR(vimana_device_text_input(sys));

    case VMT_NOW: return V_INT(vimana_datetime_now(sys));
    case VMT_YEAR: return V_INT(vimana_datetime_year(sys));
    case VMT_MONTH: return V_INT(vimana_datetime_month(sys));
    case VMT_DAY: return V_INT(vimana_datetime_day(sys));
    case VMT_HOUR: return V_INT(vimana_datetime_hour(sys));
    case VMT_MINUTE: return V_INT(vimana_datetime_minute(sys));
    case VMT_SECOND: return V_INT(vimana_datetime_second(sys));
    case VMT_WEEKDAY: return V_INT(vimana_datetime_weekday(sys));
    case VMT_YDAY: return V_INT(vimana_datetime_yday(sys));
    case VMT_DST: return V_INT(vimana_datetime_dst(sys));
    case VMT_YEAR_AT: return argc>1?V_INT(vimana_datetime_year_at(sys,val_to_i64(args[1]))):V_INT(0);
    case VMT_MONTH_AT: return argc>1?V_INT(vimana_datetime_month_at(sys,val_to_i64(args[1]))):V_INT(0);
    case VMT_DAY_AT: return argc>1?V_INT(vimana_datetime_day_at(sys,val_to_i64(args[1]))):V_INT(0);
    case VMT_HOUR_AT: return argc>1?V_INT(vimana_datetime_hour_at(sys,val_to_i64(args[1]))):V_INT(0);
    case VMT_MINUTE_AT: return argc>1?V_INT(vimana_datetime_minute_at(sys,val_to_i64(args[1]))):V_INT(0);
    case VMT_SECOND_AT: return argc>1?V_INT(vimana_datetime_second_at(sys,val_to_i64(args[1]))):V_INT(0);
    case VMT_WEEKDAY_AT: return argc>1?V_INT(vimana_datetime_weekday_at(sys,val_to_i64(args[1]))):V_INT(0);
    case VMT_YDAY_AT: return argc>1?V_INT(vimana_datetime_yday_at(sys,val_to_i64(args[1]))):V_INT(0);
    case VMT_DST_AT: return argc>1?V_INT(vimana_datetime_dst_at(sys,val_to_i64(args[1]))):V_INT(0);

    case VMT_PENDING: return V_BOOL(vimana_console_pending(sys));
    case VMT_INPUT: return V_INT(vimana_console_input(sys));
    case VMT_TYPE: return V_INT(vimana_console_type(sys));
    case VMT_NEXT: vimana_console_next(sys); return val_clone(self);
    case VMT_PUSH: return V_BOOL(argc>2&&vimana_console_push(sys,(int)val_to_i64(args[1]),(int)val_to_i64(args[2])));
    case VMT_STDOUT: if(argc>1)vimana_console_stdout(sys,(int)val_to_i64(args[1])); return val_clone(self);
    case VMT_STDERR: if(argc>1)vimana_console_stderr(sys,(int)val_to_i64(args[1])); return val_clone(self);
    case VMT_STDERR_HEX: if(argc>1)vimana_console_stderr_hex(sys,(int)val_to_i64(args[1])); return val_clone(self);
    case VMT_NONE: break;
    }

    fprintf(stderr,"warning: unhandled method '%s' on object %d (argc=%d)\n",m,self.obj.kind,argc);
    return val_clone(self);
}

static Val builtin(VM *vm, const char *name, Val *args, int argc) {
    (void)vm;
    /* stdr.write / stdr.writef */
    if (!strcmp(name,"stdr.write")) {
        char buf[1024]; for(int i=0;i<argc;i++) printf("%s",val_to_str(args[i],buf,1024)); printf("\n");
        return V_NULL;
    }
    if (!strcmp(name,"stdr.args")||!strcmp(name,"args")) {
        Val a=V_ARR_NEW();
        for(int i=0;i<vm->argc;i++) arr_push(a.arr,V_STR_CSTR(vm->argv[i]));
        return a;
    }
    if (!strcmp(name,"stdr.getcwd")||!strcmp(name,"getcwd")) {
        char path[4096];
        return getcwd(path,sizeof(path)) ? V_STR_CSTR(path) : V_STR_CSTR(".");
    }
    if (!strcmp(name,"stdr.dirname")||!strcmp(name,"dirname")) {
        char path[4096];
        const char *s=argc>0?val_cstr(args[0],path,sizeof(path)):"";
        size_t n=strlen(s);
        while(n>1&&s[n-1]=='/') n--;
        while(n>0&&s[n-1]!='/') n--;
        if(n==0) return V_STR_CSTR("");
        while(n>1&&s[n-1]=='/') n--;
        return V_STR_COPY(s,(int)n);
    }
    if (!strcmp(name,"stdr.basename")||!strcmp(name,"basename")) {
        char path[4096];
        const char *s=argc>0?val_cstr(args[0],path,sizeof(path)):"";
        size_t n=strlen(s);
        while(n>1&&s[n-1]=='/') n--;
        size_t start=n;
        while(start>0&&s[start-1]!='/') start--;
        return V_STR_COPY(s+start,(int)(n-start));
    }
    if (!strcmp(name,"stdr.home_dir")||!strcmp(name,"home_dir")) {
        const char *home=getenv("HOME");
        return V_STR_CSTR((home&&home[0])?home:"/tmp");
    }
    if (!strcmp(name,"stdr.file_exists")||!strcmp(name,"file_exists")) {
        char path[4096];
        const char *s=argc>0?val_cstr(args[0],path,sizeof(path)):"";
        FILE *f=fopen(s,"rb");
        if(!f) return V_FALSE;
        fclose(f);
        return V_TRUE;
    }
    if (!strcmp(name,"stdr.read_text_file")||!strcmp(name,"read_text_file")) {
        char path[4096];
        const char *s=argc>0?val_cstr(args[0],path,sizeof(path)):"";
        return read_text_file_val(s);
    }
    if (!strcmp(name,"stdr.write_text_file")||!strcmp(name,"write_text_file")) {
        char path[4096];
        const char *s=argc>0?val_cstr(args[0],path,sizeof(path)):"";
        return V_BOOL(argc>1&&write_text_file_val(s,args[1]));
    }
    if (!strcmp(name,"stdr.len")||!strcmp(name,"len")) {
        if (!argc) return V_INT(0);
        Val v=args[0];
        if (v.type==VT_STR) return V_INT(v.str.len);
        if (v.type==VT_ARR) return V_INT(v.arr->len);
        return V_INT(0);
    }
    if (!strcmp(name,"stdr.array")||!strcmp(name,"array")) {
        return V_ARR_NEW();
    }
    if (!strcmp(name,"stdr.push")||!strcmp(name,"push")) {
        if (argc>=2&&args[0].type==VT_ARR) { arr_push(args[0].arr,val_clone(args[1])); return val_clone(args[0]); }
        return V_NULL;
    }
    if (!strcmp(name,"stdr.at")||!strcmp(name,"at")) {
        if (argc<2) return V_NULL;
        Val a=args[0]; int i=(int)val_to_i64(args[1]);
        if (a.type==VT_ARR) return i>=0&&i<a.arr->len?val_clone(a.arr->items[i]):V_NULL;
        if (a.type==VT_STR) return i>=0&&i<a.str.len?V_STR_COPY(a.str.s+i,1):V_NULL;
        return V_NULL;
    }
    if (!strcmp(name,"stdr.set")||!strcmp(name,"set")) {
        if (argc>=3&&args[0].type==VT_ARR) {
            int i=(int)val_to_i64(args[1]);
            if(i>=0&&i<args[0].arr->len) {
                val_release(args[0].arr->items[i]);
                args[0].arr->items[i]=val_clone(args[2]);
            }
            return val_clone(args[0]);
        }
        return V_NULL;
    }
    if (!strcmp(name,"stdr.floor")||!strcmp(name,"math.floor")||!strcmp(name,"floor")) {
        if (!argc) return V_INT(0);
        double v=val_to_f64(args[0]); return V_INT((int64_t)floor(v));
    }
    if (!strcmp(name,"stdr.join")||!strcmp(name,"join")) {
        if(!argc||args[0].type!=VT_ARR) return V_STR_CSTR("");
        int total=0;
        for(int i=0;i<args[0].arr->len;i++){
            char b[256];
            Val item=args[0].arr->items[i];
            total+=item.type==VT_STR?item.str.len:(int)strlen(val_to_str(item,b,sizeof(b)));
        }
        Val out; out.type=VT_STR; out.str.s=malloc((size_t)total+1); out.str.len=0; out.str.ref=1;
        for(int i=0;i<args[0].arr->len;i++){
            char b[256];
            Val item=args[0].arr->items[i];
            const char *s=item.type==VT_STR?item.str.s:val_to_str(item,b,sizeof(b));
            int n=item.type==VT_STR?item.str.len:(int)strlen(s);
            memcpy(out.str.s+out.str.len,s,(size_t)n);
            out.str.len+=n;
        }
        out.str.s[out.str.len]=0;
        return out;
    }
    if (!strcmp(name,"stdr.str_concat")||!strcmp(name,"str_concat")) {
        return argc>=2?val_add(args[0],args[1]):(argc==1?val_clone(args[0]):V_STR_CSTR(""));
    }
    if (!strcmp(name,"stdr.str")||!strcmp(name,"str")) {
        char buf[256]; val_to_str(argc?args[0]:V_NULL,buf,256); return V_STR_CSTR(buf);
    }
    if (!strcmp(name,"stdr.num")||!strcmp(name,"num")) {
        if (!argc) return V_INT(0);
        Val v=args[0];
        if (v.type==VT_INT) return v;
        if (v.type==VT_FLT) return v;
        if (v.type==VT_STR) {
            char *e; long long iv=strtoll(v.str.s,&e,10);
            if (*e==0) return V_INT(iv);
            return V_FLT(atof(v.str.s));
        }
        return V_INT(0);
    }
    if (!strcmp(name,"stdr.char_at")||!strcmp(name,"char_at")) {
        if (argc>=2&&args[0].type==VT_STR) {
            int idx=(int)val_to_i64(args[1]);
            if (idx>=0&&idx<args[0].str.len) return V_STR_COPY(args[0].str.s+idx,1);
        }
        return V_STR_CSTR("");
    }
    if (!strcmp(name,"stdr.str_at")||!strcmp(name,"str_at")) {
        if (argc>=2&&args[0].type==VT_STR) {
            int idx=(int)val_to_i64(args[1]);
            if (idx>=0&&idx<args[0].str.len) return V_INT((unsigned char)args[0].str.s[idx]);
        }
        return V_INT(0);
    }
    if (!strcmp(name,"stdr.char_code")||!strcmp(name,"char_code")) {
        if (argc&&args[0].type==VT_STR&&args[0].str.len>0) return V_INT((unsigned char)args[0].str.s[0]);
        return V_INT(0);
    }
    if (!strcmp(name,"stdr.char_from_code")||!strcmp(name,"char_from_code")) {
        char ch=(char)(val_to_i64(argc?args[0]:V_INT(0))&0xFF);
        return V_STR_COPY(&ch,1);
    }
    if (!strcmp(name,"stdr.slice")||!strcmp(name,"slice")) {
        if (argc>=3&&args[0].type==VT_STR) {
            int s=(int)val_to_i64(args[1]); int e=(int)val_to_i64(args[2]);
            if (s<0)s=0; if (e>args[0].str.len)e=args[0].str.len;
            if (s>e)s=e;
            return V_STR_COPY(args[0].str.s+s,e-s);
        }
        return V_STR_CSTR("");
    }
    if (!strcmp(name,"stdr.substring_len")||!strcmp(name,"substring_len")) {
        if (argc>=3&&args[0].type==VT_STR) {
            int s=(int)val_to_i64(args[1]); int n=(int)val_to_i64(args[2]);
            if (s<0)s=0; if (n<0)n=0; if (s>args[0].str.len)s=args[0].str.len;
            if (s+n>args[0].str.len)n=args[0].str.len-s;
            return V_STR_COPY(args[0].str.s+s,n);
        }
        return V_STR_CSTR("");
    }
    if (!strcmp(name,"stdr.trim")||!strcmp(name,"trim")) {
        if(!argc||args[0].type!=VT_STR) return V_STR_CSTR("");
        int s=0,e=args[0].str.len;
        while(s<e&&(unsigned char)args[0].str.s[s]<=32) s++;
        while(e>s&&(unsigned char)args[0].str.s[e-1]<=32) e--;
        return V_STR_COPY(args[0].str.s+s,e-s);
    }
    if (!strcmp(name,"stdr.index_of")||!strcmp(name,"index_of")) {
        if(argc>=2&&args[0].type==VT_STR&&args[1].type==VT_STR) {
            char *p=strstr(args[0].str.s,args[1].str.s);
            return p?V_INT(p-args[0].str.s):V_INT(-1);
        }
        return V_INT(-1);
    }
    if (!strcmp(name,"stdr.starts_with")||!strcmp(name,"starts_with")) {
        return V_BOOL(argc>=2&&args[0].type==VT_STR&&args[1].type==VT_STR&&
            args[1].str.len<=args[0].str.len&&!memcmp(args[0].str.s,args[1].str.s,(size_t)args[1].str.len));
    }
    if (!strcmp(name,"stdr.ends_with")||!strcmp(name,"ends_with")) {
        return V_BOOL(argc>=2&&args[0].type==VT_STR&&args[1].type==VT_STR&&
            args[1].str.len<=args[0].str.len&&
            !memcmp(args[0].str.s+args[0].str.len-args[1].str.len,args[1].str.s,(size_t)args[1].str.len));
    }
    if (!strcmp(name,"stdr.is_null")||!strcmp(name,"is_null")) {
        return V_BOOL(argc?val_isnull(args[0]):true);
    }
    if (!strcmp(name,"stdr.parse_hex")||!strcmp(name,"parse_hex")) {
        char buf[256];
        const char *s=argc>0?val_cstr(args[0],buf,sizeof(buf)):"";
        return V_INT(strtoll(s,NULL,16));
    }
    if (!strcmp(name,"stdr.current_year")||!strcmp(name,"current_year")||
        !strcmp(name,"stdr.current_month")||!strcmp(name,"current_month")||
        !strcmp(name,"stdr.current_day")||!strcmp(name,"current_day")) {
        time_t now=time(NULL);
        struct tm *tmv=localtime(&now);
        if(!tmv) return V_INT(0);
        if (strstr(name,"year")) return V_INT(tmv->tm_year+1900);
        if (strstr(name,"month")) return V_INT(tmv->tm_mon+1);
        return V_INT(tmv->tm_mday);
    }
    if (!strcmp(name,"stdr.open_file_dialog")||!strcmp(name,"open_file_dialog"))
        return prompt_path_dialog("open_file",argc>0?args[0]:V_NULL,argc>1?args[1]:V_NULL);
    if (!strcmp(name,"stdr.save_file_dialog")||!strcmp(name,"save_file_dialog"))
        return prompt_path_dialog("save_file",argc>0?args[0]:V_NULL,argc>1?args[1]:V_NULL);
    if (!strcmp(name,"math.sqrt")||!strcmp(name,"sqrt")) { if(argc)return V_FLT(sqrt(val_to_f64(args[0]))); return V_FLT(0); }
    if (!strcmp(name,"math.abs")||!strcmp(name,"abs"))   { if(argc)return V_FLT(fabs(val_to_f64(args[0]))); return V_FLT(0); }
    if (!strcmp(name,"math.min")||!strcmp(name,"min"))   { if(argc>=2)return val_clone(val_lt(args[0],args[1])?args[0]:args[1]); return V_NULL; }
    if (!strcmp(name,"math.max")||!strcmp(name,"max"))   { if(argc>=2)return val_clone(val_lt(args[0],args[1])?args[1]:args[0]); return V_NULL; }
    if (!strcmp(name,"math.sin")||!strcmp(name,"sin"))   { if(argc)return V_FLT(sin(val_to_f64(args[0]))); return V_FLT(0); }
    if (!strcmp(name,"math.cos")||!strcmp(name,"cos"))   { if(argc)return V_FLT(cos(val_to_f64(args[0]))); return V_FLT(0); }
    if (!strncmp(name,"vimana.",7)) return builtin_vimana(name,args,argc);
    if (!strncmp(name,"__chain__.",10)) return builtin_chain(vm,name,args,argc);
    fprintf(stderr,"warning: unknown external '%s' (argc=%d)\n",name,argc);
    return V_NULL;
}

/* ═════════════════════════════ Execution loop ═══════════════════════ */

static void release_vals(Val *vals, int n) {
    for(int i=0;i<n;i++) val_release(vals[i]);
}

static void wdrop_to(VM *vm, int depth) {
    while(vm->wsp>depth) val_release(wpop(vm));
}

static Val vm_run(VM *vm, int base_rsp) {
    ROM *r=vm->rom;

#define PUSH(v) wpush(vm,(v))
#define POP()   wpop(vm)
#define PEEK()  wpeek(vm)

    while(vm->rsp>base_rsp){
        Frame *fr=&vm->rst[vm->rsp-1];
        uint8_t *code=fr->code;
        uint8_t op=code[fr->pc++];
        switch(op){
        case OP_BRK: {
            frame_release(fr);
            vm->rsp--;
            if (vm->rsp<=base_rsp) return V_NULL;
            PUSH(V_NULL);
            break;
        }
         case OP_LIT: {
             uint16_t ci=(uint16_t)(code[fr->pc]|(code[fr->pc+1]<<8)); fr->pc+=2;
             RCon *k=&r->consts[ci];
             switch(k->type){
             case YCON_INT:  PUSH(V_INT(k->ival)); break;
             case YCON_FLT:  PUSH(V_FLT(k->fval)); break;
             case YCON_STR:  PUSH(V_STR_CSTR(k->sval)); break;
             case YCON_BOOL: PUSH(V_BOOL(k->ival)); break;
             case YCON_NULL: PUSH(V_NULL); break;
             }
             break;
         }
        case OP_LDA: { uint16_t s=(uint16_t)(code[fr->pc]|(code[fr->pc+1]<<8)); fr->pc+=2; PUSH(s<fr->nlocs?val_clone(fr->locals[s]):V_NULL); break; }
        case OP_STA: { uint16_t s=(uint16_t)(code[fr->pc]|(code[fr->pc+1]<<8)); fr->pc+=2; Val v=POP(); if(s<fr->nlocs){val_release(fr->locals[s]);fr->locals[s]=v;} else val_release(v); break; }
        case OP_LDZ:{ uint16_t g=(uint16_t)(code[fr->pc]|(code[fr->pc+1]<<8)); fr->pc+=2; PUSH(g<r->nglobals?val_clone(r->glob_vals[g]):V_NULL); break; }
        case OP_STZ:{ uint16_t g=(uint16_t)(code[fr->pc]|(code[fr->pc+1]<<8)); fr->pc+=2; Val v=POP(); if(g<r->nglobals){val_release(r->glob_vals[g]);r->glob_vals[g]=v;} else val_release(v); break; }
        case OP_POP:   val_release(POP()); break;
        case OP_DUP: { Val v=PEEK(); PUSH(val_clone(v)); break; }
        case OP_NIP: { Val b=POP(),a=POP(); val_release(a); PUSH(b); break; }
        /* Arithmetic */
        case OP_ADD: { Val b=POP(),a=POP(); Val v=val_add(a,b); val_release(a); val_release(b); PUSH(v); break; }
        case OP_SUB: { Val b=POP(),a=POP(); Val v=val_sub(a,b); val_release(a); val_release(b); PUSH(v); break; }
        case OP_MUL: { Val b=POP(),a=POP(); Val v=val_mul(a,b); val_release(a); val_release(b); PUSH(v); break; }
        case OP_DIV: { Val b=POP(),a=POP(); Val v=val_div(a,b); val_release(a); val_release(b); PUSH(v); break; }

        /* Comparison */
        case OP_EQU:  { Val b=POP(),a=POP(); bool v=val_eq(a,b); val_release(a); val_release(b); PUSH(V_BOOL(v)); break; }
        case OP_NEQ: { Val b=POP(),a=POP(); bool v=!val_eq(a,b); val_release(a); val_release(b); PUSH(V_BOOL(v)); break; }

        case OP_LTH:  { Val b=POP(),a=POP(); bool v=val_lt(a,b); val_release(a); val_release(b); PUSH(V_BOOL(v)); break; }
        case OP_GTH:  { Val b=POP(),a=POP(); bool v=val_lt(b,a); val_release(a); val_release(b); PUSH(V_BOOL(v)); break; }
        /* Logical */
        case OP_AND: { Val b=POP(),a=POP(); bool v=val_truthy(a)&&val_truthy(b); val_release(a); val_release(b); PUSH(V_BOOL(v)); break; }
        case OP_ORA:  { Val b=POP(),a=POP(); bool v=val_truthy(a)||val_truthy(b); val_release(a); val_release(b); PUSH(V_BOOL(v)); break; }
        case OP_EOR: { Val b=POP(),a=POP(); int64_t v=val_to_i64(a)^val_to_i64(b); val_release(a); val_release(b); PUSH(V_INT(v)); break; }
        case OP_NOT: { Val a=POP(); bool v=!val_truthy(a); val_release(a); PUSH(V_BOOL(v)); break; }
        /* Jumps */
        case OP_JMP: { int32_t off;memcpy(&off,code+fr->pc,4);fr->pc+=4; fr->pc+=off; break; }
        case OP_JCN: { int32_t off;memcpy(&off,code+fr->pc,4);fr->pc+=4; Val c=POP(); bool v=val_truthy(c); val_release(c); if(v) fr->pc+=off; break; }
        /* Function calls */
        case OP_FUN: {
            uint16_t fi=(uint16_t)(code[fr->pc]|(code[fr->pc+1]<<8)); uint8_t argc2=code[fr->pc+2]; fr->pc+=3;
            Val *cargs=argc2?alloca(sizeof(Val)*(size_t)argc2):NULL;
            for(int i=argc2-1;i>=0;i--) cargs[i]=POP();
            if(!rpush_frame(vm,fi,cargs,argc2,false)) release_vals(cargs,argc2);
            break;
        }
        case OP_DEI: {
            uint16_t ei=(uint16_t)(code[fr->pc]|(code[fr->pc+1]<<8)); uint8_t argc2=code[fr->pc+2]; fr->pc+=3;
            const char *ename = (ei<r->nexts) ? r->consts[r->exts[ei].name_idx].sval : "?";
            Val *cargs=argc2?alloca(sizeof(Val)*(size_t)argc2):NULL;
            for(int i=argc2-1;i>=0;i--) cargs[i]=POP();
            Val out=builtin(vm,ename,cargs,argc2);
            release_vals(cargs,argc2);
            PUSH(out);
            break;
        }
        case OP_DEO: {
            uint16_t ei=(uint16_t)(code[fr->pc]|(code[fr->pc+1]<<8)); uint8_t argc2=code[fr->pc+2]; fr->pc+=3;
            const char *ename = (ei<r->nexts) ? r->consts[r->exts[ei].name_idx].sval : "?";
            Val *cargs=argc2?alloca(sizeof(Val)*(size_t)argc2):NULL;
            for(int i=argc2-1;i>=0;i--) cargs[i]=POP();
            Val out=builtin(vm,ename,cargs,argc2);
            val_release(out);
            release_vals(cargs,argc2);
            break;
        }
        case OP_CLO: {
            uint16_t fi=(uint16_t)(code[fr->pc]|(code[fr->pc+1]<<8)); uint8_t argc2=code[fr->pc+2]; fr->pc+=3;
            Val *caps=argc2?alloca(sizeof(Val)*(size_t)argc2):NULL;
            for(int i=argc2-1;i>=0;i--) caps[i]=POP();
            PUSH(V_FUNC(fi,caps,argc2));
            break;
        }
        case OP_RET: {
            Val ret=POP();
            frame_release(fr);
            vm->rsp--;
            if (vm->rsp<=base_rsp) return ret;
            PUSH(ret);
            break;
        }
        /* Stack ops */
        case OP_SWP: { Val b=POP(),a=POP(); PUSH(b); PUSH(a); break; }
        case OP_OVR: { Val b=POP(),a=val_clone(PEEK()); PUSH(b); PUSH(a); break; }
        case OP_ROT: { Val c=POP(),b=POP(),a=POP(); PUSH(b); PUSH(c); PUSH(a); break; }
        case OP_SFT: {
            Val b=POP(),a=POP();
            int64_t s=val_to_i64(b);
            int sh=(int)((s<0?-s:s)&63);
            PUSH(s<0?V_INT(val_to_i64(a)>>sh):V_INT(val_to_i64(a)<<sh));
            val_release(a); val_release(b);
            break;
        }
        default:
            fprintf(stderr,"unknown opcode 0x%02X at pc=%d in func %d\n",op,fr->pc-1,fr->func_idx);
            while(vm->rsp>base_rsp) frame_release(&vm->rst[--vm->rsp]);
            return V_NULL;
        }
    }
    return V_NULL;
}

static Val vm_call(VM *vm, int func_idx, Val *args, int argc) {
    int base_wsp=vm->wsp;
    int base_rsp=vm->rsp;
    if(!rpush_frame(vm,func_idx,args,argc,true)) return V_NULL;
    Val ret=vm_run(vm,base_rsp);
    wdrop_to(vm,base_wsp);
    return ret;
}

/* ══════════════════════════════════ Main ═══════════════════════════ */

int main(int argc, char **argv) {
    bool verbose=false;
    int argi=1;
    if (argi<argc&&!strcmp(argv[argi],"-v")) { verbose=true; argi++; }
    if (argi>=argc){fprintf(stderr,"usage: vinemu [-v] file.rom\n");return 1;}
    ROM *r=rom_load(argv[argi]); if(!r) return 1;
    VM *vm=calloc(1,sizeof(VM)); vm->rom=r; vm->argc=argc-argi-1; vm->argv=argv+argi+1;
    if (verbose) {
        fprintf(stderr,"ROM: %d consts  %d funcs  %d globals  %d exts\n",r->nconsts,r->nfuncs,r->nglobals,r->nexts);
        fprintf(stderr,"Functions:\n");
        for(int i=0;i<r->nfuncs;i++) {
            const char *n=r->consts[r->funcs[i].name_idx].sval;
            fprintf(stderr,"  [%d] %s  (arity=%d locals=%d code=%d)\n",i,n,r->funcs[i].arity,r->funcs[i].nlocals,r->funcs[i].code_sz);
        }
    }
    /* Run global initialiser (always func 0) */
    Val init_ret=vm_call(vm,0,NULL,0);
    val_release(init_ret);
    /* Run entry point */
    if (r->entry_func>=0&&r->entry_func<r->nfuncs) {
        if (verbose) fprintf(stderr,"Running entry: %s\n",r->consts[r->funcs[r->entry_func].name_idx].sval);
        Val entry_ret=vm_call(vm,r->entry_func,NULL,0);
        val_release(entry_ret);
    } else {
        fprintf(stderr,"No entry point found.\n");
    }
    return 0;
}
