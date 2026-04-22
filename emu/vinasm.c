/* vinasm.c — vinasm: Yis source assembler → .rom bytecode
 *
 *  Usage:  vinasm [-o out.rom] app_main.yi [file.yi ...]
 *
 *  Handles: globals (def/def?/const), functions (:/::/->),
 *           if/elif/else, C-style for, foreach (for … in …),
 *           break/continue, return (<-), let/let?,
 *           arithmetic, comparison, logical, bitwise,
 *           array literals/indexing, module-qualified calls, ??
 *
 *  External modules (stdr, math, vimana) are compiled as OP_DEI.
 *  User modules listed on the command line become OP_FUN.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include "vinrom.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ═══════════════════════════════ Lexer ═════════════════════════════ */

typedef enum {
    T_EOF, T_IDENT, T_INT, T_FLOAT, T_STRING,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_EQ2, T_NEQ, T_LT, T_LE, T_GT, T_GE,
    T_AMPAMP, T_PIPEPIPE, T_BANG, T_AMP, T_PIPE, T_SHL, T_SHR,
    T_ASSIGN, T_PLUSEQ, T_MINUSEQ, T_STAREQ, T_SLASHEQ, T_PERCENTEQ,
    T_LPAREN, T_RPAREN, T_LBRACKET, T_RBRACKET,
    T_LBRACE, T_RBRACE,
    T_COMMA, T_DOT, T_COLON, T_HASH, T_QMARK, T_QMQM,
    T_SEMI, T_ARROW, T_LARROW, T_DCOLON, T_LPAREN2, T_RPAREN2,
    KW_LET, KW_IF, KW_ELIF, KW_ELSE, KW_FOR, KW_IN,
    KW_BREAK, KW_CONTINUE,
    KW_DEF, KW_CONST, KW_PUB, KW_BRING, KW_CASK,
    KW_NEW, KW_MATCH, KW_TRUE, KW_FALSE, KW_NULL,
} TK;

#define MAX_TOKEN_TEXT 8192

typedef struct { TK kind; char str[MAX_TOKEN_TEXT]; int64_t ival; double fval; int line, col; } Tok;
typedef struct { const char *src; int pos, line, last_nl; Tok cur; TK pending; } Lex;

static bool lex_kw(const char *s, TK *k) {
    static const struct { const char *n; TK k; } T[] = {
        {"let",KW_LET},{"if",KW_IF},{"elif",KW_ELIF},{"else",KW_ELSE},
        {"for",KW_FOR},{"in",KW_IN},{"break",KW_BREAK},{"continue",KW_CONTINUE},
        {"def",KW_DEF},{"const",KW_CONST},{"pub",KW_PUB},{"bring",KW_BRING},
        {"cask",KW_CASK},{"new",KW_NEW},{"match",KW_MATCH},
        {"true",KW_TRUE},{"false",KW_FALSE},{"null",KW_NULL},{NULL,0}
    };
    for (int i = 0; T[i].n; i++)
        if (!strcmp(s, T[i].n)) { *k = T[i].k; return true; }
    return false;
}

static void lex_step(Lex *l) {
    if (l->pending) {
        l->cur.kind=l->pending;
        l->cur.str[0]=0;
        l->cur.line=l->line;
        l->cur.col=l->pos - l->last_nl + 1;
        l->pending=0;
        return;
    }
    const char *s = l->src;
    int p = l->pos;
    Tok *t = &l->cur;
again:
    while (s[p]==' '||s[p]=='\t'||s[p]=='\r'||s[p]=='\n') {
        if (s[p]=='\n') { l->line++; l->last_nl=p+1; }
        p++;
    }
    if (s[p]=='-'&&s[p+1]=='-') {
        int q=p+2;
        while(s[q]==' '||s[q]=='\t') q++;
        if (s[q]==')'&&s[q+1]==')') { t->kind=T_MINUS; l->pos=p+1; return; }
        while(s[p]&&s[p]!='\n') p++;
        goto again;
    }
    if (s[p]=='-'&&s[p+1]=='|') {
        p+=2; int d=1;
        while(s[p]&&d>0) {
            if(s[p]=='-'&&s[p+1]=='|'){d++;p+=2;}
            else if(s[p]=='|'&&s[p+1]=='-'){d--;p+=2;}
            else{if(s[p]=='\n'){l->line++;l->last_nl=p+1;}p++;}
        }
        goto again;
    }
    t->line = l->line;
    t->col  = p - l->last_nl + 1;
    if (!s[p]) { t->kind=T_EOF; l->pos=p; return; }
    if (isdigit(s[p])) {
        bool hex = s[p]=='0'&&(s[p+1]=='x'||s[p+1]=='X');
        if (hex) p+=2;
        char buf[64]; int bi=0; bool dot=false;
        while ((hex?isxdigit(s[p]):isdigit(s[p]))||(s[p]=='.'&&!hex&&!dot))
            { if(s[p]=='.')dot=true; buf[bi++]=s[p++]; }
        buf[bi]=0;
        if (dot) { t->kind=T_FLOAT; t->fval=atof(buf); }
        else if (hex) { t->kind=T_INT; t->ival=(int64_t)strtoull(buf,NULL,16); }
        else { t->kind=T_INT; t->ival=atoll(buf); }
        l->pos=p; return;
    }
    if (s[p]=='"') {
        p++; int i=0;
        while (s[p]&&s[p]!='"') {
            char ch;
            if(s[p]=='\\'){p++;switch(s[p]){case 'n':ch='\n';break;case 't':ch='\t';break;case '"':ch='"';break;case '\\':ch='\\';break;default:ch=s[p];}
            }else ch=s[p];
            if (i<MAX_TOKEN_TEXT-1) t->str[i++]=ch;
            p++;
        }
        if(s[p]=='"')p++; t->str[i]=0; t->kind=T_STRING; l->pos=p; return;
    }
    if (isalpha(s[p])||s[p]=='_') {
        int i=0;
        while(isalnum(s[p])||s[p]=='_') {
            if (i<MAX_TOKEN_TEXT-1) t->str[i++]=s[p];
            p++;
        }
        t->str[i]=0;
        TK k; t->kind = lex_kw(t->str,&k)?k:T_IDENT; l->pos=p; return;
    }
    char c=s[p], n=s[p+1];
    switch(c){
    case '(':t->kind=T_LPAREN;if(n=='('){l->pending=T_LPAREN;p+=2;}else p++;break;
    case ')':t->kind=T_RPAREN;if(n==')'){l->pending=T_RPAREN;p+=2;}else p++;break;
    case '-':if(n=='>'){t->kind=T_ARROW;p+=2;}else if(n=='='){t->kind=T_MINUSEQ;p+=2;}else{t->kind=T_MINUS;p++;}break;
    case '<':if(n=='-'){t->kind=T_LARROW;p+=2;}else if(n=='='){t->kind=T_LE;p+=2;}else if(n=='<'){t->kind=T_SHL;p+=2;}else{t->kind=T_LT;p++;}break;
    case '>':if(n=='='){t->kind=T_GE;p+=2;}else if(n=='>'){t->kind=T_SHR;p+=2;}else{t->kind=T_GT;p++;}break;
    case '=':if(n=='='){t->kind=T_EQ2;p+=2;}else{t->kind=T_ASSIGN;p++;}break;
    case '!':if(n=='='){t->kind=T_NEQ;p+=2;}else{t->kind=T_BANG;p++;}break;
    case ':':if(n==':'){t->kind=T_DCOLON;p+=2;}else{t->kind=T_COLON;p++;}break;
    case '&':if(n=='&'){t->kind=T_AMPAMP;p+=2;}else{t->kind=T_AMP;p++;}break;
    case '|':if(n=='|'){t->kind=T_PIPEPIPE;p+=2;}else{t->kind=T_PIPE;p++;}break;
    case '+':if(n=='='){t->kind=T_PLUSEQ;p+=2;}else{t->kind=T_PLUS;p++;}break;
    case '*':if(n=='='){t->kind=T_STAREQ;p+=2;}else{t->kind=T_STAR;p++;}break;
    case '/':if(n=='='){t->kind=T_SLASHEQ;p+=2;}else{t->kind=T_SLASH;p++;}break;
    case '%':if(n=='='){t->kind=T_PERCENTEQ;p+=2;}else{t->kind=T_PERCENT;p++;}break;
    case '?':if(n=='?'){t->kind=T_QMQM;p+=2;}else{t->kind=T_QMARK;p++;}break;
    case '[':t->kind=T_LBRACKET;p++;break;
    case ']':t->kind=T_RBRACKET;p++;break;
    case '{':t->kind=T_LBRACE;p++;break;
    case '}':t->kind=T_RBRACE;p++;break;
    case ',':t->kind=T_COMMA;p++;break;
    case '.':t->kind=T_DOT;p++;break;
    case ';':t->kind=T_SEMI;p++;break;
    case '#':t->kind=T_HASH;p++;break;
    default: p++; goto again;
    }
    l->pos=p;
}

/* ════════════════════════════ Compiler state ═══════════════════════ */

#define MAX_CONSTS 16384
#define MAX_FUNCS   2048
#define MAX_GLOBALS 4096
#define MAX_EXTS    2048
#define MAX_PATCHES 16384
#define MAX_LOCALS   256
#define MAX_LOOPS     64

typedef struct { uint8_t type; int64_t ival; double fval; char *sval; int slen; } Con;
typedef struct { int name_idx, init_idx; } Glob;
typedef struct { int name_idx; } Ext;
typedef struct {
    int     name_idx;
    char    fullname[128];
    int     arity, nlocals;
    uint8_t *code;
    int     code_sz, code_cap;
    char    locals[MAX_LOCALS][64];
} Func;
typedef struct { char name[128]; int func_idx, off; } Patch;
typedef struct { int breaks[64], nbreaks, conts[64], nconts; } Loop;

typedef struct {
    Con    consts[MAX_CONSTS]; int nconsts;
    Func   funcs[MAX_FUNCS];   int nfuncs;
    Glob   globals[MAX_GLOBALS]; int nglobals;
    Ext    exts[MAX_EXTS];     int nexts;
    Patch  patches[MAX_PATCHES]; int npatches;
    int    entry_func, init_func, cur_func;
    int    lambda_id;
    Loop   loops[MAX_LOOPS];   int nloops;
    char   ext_mods[32][64];   int n_ext_mods;
    char   cur_mod[64];
    Lex    lex;
} Cmp;

/* ════════════════════════════ Emit helpers ══════════════════════════ */

static void die(Cmp *c, const char *msg) {
    fprintf(stderr,"error line %d col %d: %s (near '%s')\n",c->lex.cur.line,c->lex.cur.col,msg,c->lex.cur.str);
    exit(1);
}
static Tok peek(Cmp *c)       { return c->lex.cur; }
static Tok consume(Cmp *c)    { Tok t=c->lex.cur; lex_step(&c->lex); return t; }
static bool check(Cmp *c, TK k) { return c->lex.cur.kind==k; }
static bool eat(Cmp *c, TK k) {
    if(!check(c,k)) return false;
    consume(c);
    return true;
}
static void expect(Cmp *c, TK k, const char *w) { if(!eat(c,k)) die(c,w); }

static int add_int(Cmp *c, int64_t v) {
    for(int i=0;i<c->nconsts;i++) if(c->consts[i].type==YCON_INT&&c->consts[i].ival==v) return i;
    Con *k=&c->consts[c->nconsts]; k->type=YCON_INT; k->ival=v; return c->nconsts++;
}
static int add_flt(Cmp *c, double v) {
    for(int i=0;i<c->nconsts;i++) if(c->consts[i].type==YCON_FLT&&c->consts[i].fval==v) return i;
    Con *k=&c->consts[c->nconsts]; k->type=YCON_FLT; k->fval=v; return c->nconsts++;
}
static int add_str(Cmp *c, const char *s) {
    for(int i=0;i<c->nconsts;i++) if(c->consts[i].type==YCON_STR&&!strcmp(c->consts[i].sval,s)) return i;
    Con *k=&c->consts[c->nconsts]; k->type=YCON_STR; k->sval=strdup(s); k->slen=(int)strlen(s); return c->nconsts++;
}
static int add_bool(Cmp *c, int v) {
    for(int i=0;i<c->nconsts;i++) if(c->consts[i].type==YCON_BOOL&&c->consts[i].ival==v) return i;
    Con *k=&c->consts[c->nconsts]; k->type=YCON_BOOL; k->ival=v; return c->nconsts++;
}
static int add_null(Cmp *c) {
    for(int i=0;i<c->nconsts;i++) if(c->consts[i].type==YCON_NULL) return i;
    Con *k=&c->consts[c->nconsts]; k->type=YCON_NULL; return c->nconsts++;
}

static Func *cf(Cmp *c) { return &c->funcs[c->cur_func]; }
static void reserve_code(Cmp *c, int need) {
    Func *f=cf(c);
    if (need<=f->code_cap) return;
    int cap=f->code_cap?f->code_cap:256;
    while (cap<need) cap*=2;
    uint8_t *code=realloc(f->code,(size_t)cap);
    if (!code) die(c,"out of memory for function bytecode");
    f->code=code;
    f->code_cap=cap;
}
static void emit1(Cmp *c, uint8_t b) {
    Func *f=cf(c);
    reserve_code(c,f->code_sz+1);
    f->code[f->code_sz++]=b;
}
static void emit_u16(Cmp *c, uint16_t u){ emit1(c,u&0xFF); emit1(c,(u>>8)&0xFF); }
static void emit2(Cmp *c, uint8_t op, uint16_t u) { emit1(c,op); emit_u16(c,u); }
static void emit_i32(Cmp *c, int32_t v) {
    uint32_t u=(uint32_t)v;
    emit1(c,u&0xFF);emit1(c,(u>>8)&0xFF);emit1(c,(u>>16)&0xFF);emit1(c,(u>>24)&0xFF);
}
static int emit_jmp(Cmp *c, uint8_t op) {
    emit1(c,op); int pos=cf(c)->code_sz; emit_i32(c,0); return pos;
}
static int emit_jmp_true(Cmp *c) {
    return emit_jmp(c,OP_JMP);
}
static int emit_jmp_false(Cmp *c) {
    emit1(c,OP_NOT);
    return emit_jmp(c,OP_JCN);
}
static void patch_jmp(Cmp *c, int pos) {
    int32_t off=(int32_t)(cf(c)->code_sz-(pos+4));
    uint8_t *p=cf(c)->code+pos;
    p[0]=off&0xFF;p[1]=(off>>8)&0xFF;p[2]=(off>>16)&0xFF;p[3]=(off>>24)&0xFF;
}
/* Patch a JMP at pos to jump to an absolute code position */
static void patch_jmp_to(Cmp *c, int pos, int target) {
    int32_t off=(int32_t)(target-(pos+4));
    uint8_t *p=cf(c)->code+pos; p[0]=off&0xFF;p[1]=(off>>8)&0xFF;p[2]=(off>>16)&0xFF;p[3]=(off>>24)&0xFF;
}

static int find_local(Cmp *c, const char *n) {
    Func *f=cf(c);
    for(int i=f->arity+f->nlocals-1;i>=0;i--) if(!strcmp(f->locals[i],n)) return i;
    return -1;
}
static int declare_local(Cmp *c, const char *n) {
    Func *f=cf(c); int s=f->arity+f->nlocals; strncpy(f->locals[s],n,63); f->nlocals++; return s;
}
static int find_global(Cmp *c, const char *n) {
    int ni=add_str(c,n);
    for(int i=0;i<c->nglobals;i++) if(c->globals[i].name_idx==ni) return i;
    return -1;
}
static int declare_global(Cmp *c, const char *n) {
    int i=c->nglobals++; c->globals[i].name_idx=add_str(c,n); c->globals[i].init_idx=-1; return i;
}
static int find_or_add_ext(Cmp *c, const char *n) {
    int ni=add_str(c,n);
    for(int i=0;i<c->nexts;i++) if(c->exts[i].name_idx==ni) return i;
    int i=c->nexts++; c->exts[i].name_idx=ni; return i;
}
static int find_func(Cmp *c, const char *n) {
    for(int i=0;i<c->nfuncs;i++) if(!strcmp(c->funcs[i].fullname,n)) return i;
    return -1;
}
static bool is_ext_mod(Cmp *c, const char *m) {
    for(int i=0;i<c->n_ext_mods;i++) if(!strcmp(c->ext_mods[i],m)) return true;
    return false;
}
static void emit_call(Cmp *c, const char *fn, int argc) {
    int fi=find_func(c,fn);
    emit1(c,OP_FUN);
    if (fi>=0) emit_u16(c,(uint16_t)fi);
    else {
        Patch *p=&c->patches[c->npatches++];
        strncpy(p->name,fn,127); p->func_idx=c->cur_func; p->off=cf(c)->code_sz;
        emit_u16(c,0xFFFF);
    }
    emit1(c,(uint8_t)argc);
}
static void resolve_patches(Cmp *c) {
    for(int i=0;i<c->npatches;i++) {
        Patch *p=&c->patches[i]; int fi=find_func(c,p->name);
        if(fi<0){fprintf(stderr,"warning: unresolved '%s'\n",p->name);continue;}
        uint8_t *code=c->funcs[p->func_idx].code;
        code[p->off]=(uint8_t)(fi&0xFF); code[p->off+1]=(uint8_t)((fi>>8)&0xFF);
    }
}

static bool line_has_top_comma(Cmp *c) {
    Lex sv=c->lex;
    int line=c->lex.cur.line;
    int depth=0;
    while(!check(c,T_EOF)&&c->lex.cur.line==line&&!check(c,T_SEMI)) {
        if (depth==0&&check(c,T_COMMA)) { c->lex=sv; return true; }
        if (check(c,T_RBRACE)&&depth==0) break;
        if (check(c,T_LPAREN)||check(c,T_LBRACKET)||check(c,T_LBRACE)) depth++;
        else if (check(c,T_RPAREN)||check(c,T_RBRACKET)||check(c,T_RBRACE)) depth--;
        consume(c);
    }
    c->lex=sv;
    return false;
}

static void skip_type_ann(Cmp *c) {
    if (check(c,T_LPAREN)) {
        int start_line=c->lex.cur.line;
        consume(c);
        if (!check(c,T_LPAREN)) return;
        consume(c);
        if (c->lex.cur.line>start_line) return;
        int d=2;
        while(!check(c,T_EOF)) {
            if (c->lex.cur.line>start_line) break;
            if(check(c,T_LPAREN)) { d++; consume(c); continue; }
            if(check(c,T_RPAREN)) { consume(c); if(--d<=0) break; continue; }
            consume(c);
        }
    } else if (check(c,T_LBRACKET)) {
        int d=1; consume(c); while(!check(c,T_EOF)&&d>0){if(check(c,T_LBRACKET))d++;if(check(c,T_RBRACKET))d--;consume(c);}
    } else if (check(c,T_IDENT)) {
        consume(c);
        while(eat(c,T_DOT)) {
            if (check(c,T_IDENT)) consume(c);
            else break;
        }
    }
}

/* Emit load/store for a named lvalue */
static void emit_load(Cmp *c, const char *base, const char *full) {
    int sl=find_local(c,base);
    if (sl>=0&&!strchr(full,'.')) { emit2(c,OP_LDA,(uint16_t)sl); return; }
    char q[256]; if(!strchr(full,'.')) snprintf(q,255,"%s.%s",c->cur_mod,full); else strncpy(q,full,255);
    int gi=find_global(c,q); if(gi<0) gi=find_global(c,full);
    if (gi>=0) { emit2(c,OP_LDZ,(uint16_t)gi); return; }
    /* Treat as 0-arg external (module constant like vimana.color_fg) */
    int ei=find_or_add_ext(c,q); emit1(c,OP_DEI); emit_u16(c,(uint16_t)ei); emit1(c,0);
}
static void emit_store(Cmp *c, const char *base, const char *full) {
    int sl=find_local(c,base);
    if (sl>=0&&!strchr(full,'.')) { emit2(c,OP_STA,(uint16_t)sl); return; }
    char q[256]; if(!strchr(full,'.')) snprintf(q,255,"%s.%s",c->cur_mod,full); else strncpy(q,full,255);
    int gi=find_global(c,q); if(gi<0) gi=find_global(c,full);
    if (gi>=0) { emit2(c,OP_STZ,(uint16_t)gi); return; }
    fprintf(stderr,"warning: store to unknown var '%s'\n",full); emit1(c,OP_POP);
}

/* ════════════════════════════ Expression parser ════════════════════ */

static void parse_expr(Cmp *c);
static void parse_block(Cmp *c, int until_col, bool stop_at_semi);
static void parse_stmt(Cmp *c);
static int new_func(Cmp *c, const char *fullname, int arity);

static int parse_args(Cmp *c) {
    int n=0;
    if (!check(c,T_RPAREN)) do { parse_expr(c); n++; } while(eat(c,T_COMMA));
    return n;
}

static void compile_lambda(Cmp *c);

static void emit_chain_call(Cmp *c, const char *mname, int argc) {
    char chain_name[128]; snprintf(chain_name,127,"__chain__.%s",mname);
    int ei=find_or_add_ext(c,chain_name);
    emit1(c,OP_DEI); emit_u16(c,(uint16_t)ei); emit1(c,(uint8_t)(argc+1));
}
static void emit_ext_call(Cmp *c, const char *name, int argc) {
    int ei=find_or_add_ext(c,name);
    emit1(c,OP_DEI); emit_u16(c,(uint16_t)ei); emit1(c,(uint8_t)argc);
}
static void emit_ext_output(Cmp *c, const char *name, int argc) {
    int ei=find_or_add_ext(c,name);
    emit1(c,OP_DEO); emit_u16(c,(uint16_t)ei); emit1(c,(uint8_t)argc);
}

static bool paren_has_top_comma(Cmp *c) {
    Lex sv=c->lex;
    int depth=0;
    while(!check(c,T_EOF)) {
        if (depth==0&&check(c,T_COMMA)) { c->lex=sv; return true; }
        if (depth==0&&check(c,T_RPAREN)) { c->lex=sv; return false; }
        if (check(c,T_LPAREN)||check(c,T_LBRACKET)||check(c,T_LBRACE)) depth++;
        else if (check(c,T_RPAREN)||check(c,T_RBRACKET)||check(c,T_RBRACE)) depth--;
        consume(c);
    }
    c->lex=sv;
    return false;
}

static void parse_primary(Cmp *c) {
    Tok t=peek(c);
    switch(t.kind) {
    case T_INT:    consume(c); emit2(c,OP_LIT,(uint16_t)add_int(c,t.ival)); return;
    case T_FLOAT:  consume(c); emit2(c,OP_LIT,(uint16_t)add_flt(c,t.fval)); return;
    case T_STRING: consume(c); emit2(c,OP_LIT,(uint16_t)add_str(c,t.str)); return;
    case KW_TRUE:  consume(c); emit2(c,OP_LIT,(uint16_t)add_bool(c,1));  return;
    case KW_FALSE: consume(c); emit2(c,OP_LIT,(uint16_t)add_bool(c,0)); return;
    case KW_NULL:  consume(c); emit2(c,OP_LIT,(uint16_t)add_null(c));  return;
    case KW_IF: {
        consume(c);
        parse_expr(c);
        int jf=emit_jmp_false(c);
        expect(c,T_LBRACE,"expected '{' in if expression");
        parse_expr(c);
        expect(c,T_RBRACE,"expected '}' in if expression");
        int end=emit_jmp_true(c);
        patch_jmp(c,jf);
        if (eat(c,KW_ELSE)) {
            expect(c,T_LBRACE,"expected '{' in else expression");
            parse_expr(c);
            expect(c,T_RBRACE,"expected '}' in else expression");
        } else {
            emit2(c,OP_LIT, add_null(c));
        }
        patch_jmp(c,end);
        return;
    }
    case T_LPAREN:
        consume(c);
        if (eat(c,T_RPAREN)) {
            if (eat(c,T_ASSIGN)&&eat(c,T_GT)) compile_lambda(c);
            else emit2(c,OP_LIT, add_null(c));
            return;
        }
        if (paren_has_top_comma(c)) {
            emit_ext_call(c,"stdr.array",0);
            do {
                parse_expr(c);
                emit_ext_call(c,"stdr.push",2);
            } while(eat(c,T_COMMA));
            expect(c,T_RPAREN,"expected ')'");
            return;
        }
        parse_expr(c); expect(c,T_RPAREN,"expected ')'"); return;
    case T_LBRACKET:
        consume(c); emit_ext_call(c,"stdr.array",0);
        while (!check(c,T_RBRACKET)&&!check(c,T_EOF)) {
            parse_expr(c); emit_ext_call(c,"stdr.push",2);
            eat(c,T_COMMA);
        }
        int close_line=c->lex.cur.line;
        expect(c,T_RBRACKET,"expected ']'");
        if (check(c,T_COLON)&&c->lex.cur.line==close_line) { consume(c); skip_type_ann(c); }
        return;
    case T_IDENT: {
        consume(c);
        char base[64], full[256];
        strncpy(base,t.str,63); strncpy(full,t.str,255);
        int base_local=find_local(c,base);
        /* Collect dotted chain: name.member.member */
        while (check(c,T_DOT)) {
            Lex sv=c->lex; consume(c);
            if (!check(c,T_IDENT)) { c->lex=sv; break; }
            Tok m=consume(c);
            if (base_local>=0&&check(c,T_LPAREN)) {
                emit_load(c,base,base);
                consume(c);
                int argc=parse_args(c);
                expect(c,T_RPAREN,"expected ')'");
                emit_chain_call(c,m.str,argc);
                strncpy(full,base,255);
                goto postfix;
            }
            char tmp[256]; snprintf(tmp,255,"%s.%s",full,m.str); strncpy(full,tmp,255);
        }
        if (check(c,T_LPAREN)) {
            bool is_method=strchr(full,'.')&&base_local>=0;
            if (is_method) emit_load(c,base,base);
            consume(c);
            int argc=parse_args(c);
            expect(c,T_RPAREN,"expected ')'");
            if (is_method) {
                const char *mname=strrchr(full,'.')+1;
                emit_chain_call(c,mname,argc);
            } else if (!strcmp(full,"len")||!strcmp(full,"stdr.len")) {
                int ei=find_or_add_ext(c,"stdr.len"); emit1(c,OP_DEI); emit_u16(c,(uint16_t)ei); emit1(c,(uint8_t)argc);
            } else if (!strcmp(full,"str")||!strcmp(full,"stdr.str")) {
                int ei=find_or_add_ext(c,"stdr.str"); emit1(c,OP_DEI); emit_u16(c,(uint16_t)ei); emit1(c,(uint8_t)argc);
            } else if (!strcmp(full,"num")||!strcmp(full,"stdr.num")) {
                int ei=find_or_add_ext(c,"stdr.num"); emit1(c,OP_DEI); emit_u16(c,(uint16_t)ei); emit1(c,(uint8_t)argc);
            } else if (!strcmp(full,"push")||!strcmp(full,"stdr.push")) {
                int ei=find_or_add_ext(c,"stdr.push"); emit1(c,OP_DEI); emit_u16(c,(uint16_t)ei); emit1(c,(uint8_t)argc);
            } else if (!strcmp(full,"floor")||!strcmp(full,"stdr.floor")) {
                int ei=find_or_add_ext(c,"stdr.floor"); emit1(c,OP_DEI); emit_u16(c,(uint16_t)ei); emit1(c,(uint8_t)argc);
            } else if (!strcmp(full,"is_null")||!strcmp(full,"stdr.is_null")) {
                int ei=find_or_add_ext(c,"stdr.is_null"); emit1(c,OP_DEI); emit_u16(c,(uint16_t)ei); emit1(c,(uint8_t)argc);
            } else if (strchr(full,'.') && is_ext_mod(c,base)) {
                int ei=find_or_add_ext(c,full); emit1(c,OP_DEI); emit_u16(c,(uint16_t)ei); emit1(c,(uint8_t)argc);
            } else {
                char q[256];
                if (!strchr(full,'.')) snprintf(q,255,"%s.%s",c->cur_mod,full);
                else strncpy(q,full,255);
                emit_call(c,q,argc);
            }
        } else {
            emit_load(c,base,full);
        }
postfix:
        /* Postfix index: arr[idx] */
        while (check(c,T_LBRACKET)) {
            consume(c); parse_expr(c); expect(c,T_RBRACKET,"expected ']'"); emit_ext_call(c,"stdr.at",2);
        }
        /* Method chaining on return value: .method(args)... */
        while (check(c,T_DOT)) {
            Lex sv=c->lex; consume(c);
            if (!check(c,T_IDENT)) { c->lex=sv; break; }
            char mname[64]; strncpy(mname,consume(c).str,63);
            if (!check(c,T_LPAREN)) {
                /* field access on object — not supported, drop value */
                emit1(c,OP_POP); emit2(c,OP_LIT, add_null(c)); break;
            }
            consume(c);
            /* Push extra args; first arg (receiver) already on stack */
            /* Build ext name as _chain.method so VM can dispatch */
            char chain_name[128]; snprintf(chain_name,127,"__chain__.%s",mname);
            int extra=parse_args(c); expect(c,T_RPAREN,"expected ')'");
            int ei=find_or_add_ext(c,chain_name);
            emit1(c,OP_DEI); emit_u16(c,(uint16_t)ei); emit1(c,(uint8_t)(extra+1));
        }
        return;
    }
    default:
        fprintf(stderr,"unexpected token kind=%d line=%d\n",t.kind,t.line);
        consume(c); emit2(c,OP_LIT, add_null(c));
    }
}

static void parse_unary(Cmp *c) {
    if(eat(c,T_BANG)){parse_unary(c);emit1(c,OP_NOT);return;}
    if(eat(c,T_MINUS)){ emit2(c,OP_LIT, add_int(c,0)); parse_unary(c); emit1(c,OP_SUB); return;}
    if(eat(c,T_HASH)){parse_unary(c);int ei=find_or_add_ext(c,"stdr.len");emit1(c,OP_DEI);emit_u16(c,(uint16_t)ei);emit1(c,1);return;}
    parse_primary(c);
}
static void emit_mod(Cmp *c){emit1(c,OP_OVR);emit1(c,OP_OVR);emit1(c,OP_DIV);emit1(c,OP_MUL);emit1(c,OP_SUB);}
static void parse_mul(Cmp *c){parse_unary(c);for(;;){if(eat(c,T_STAR)){parse_unary(c);emit1(c,OP_MUL);}else if(eat(c,T_SLASH)){parse_unary(c);emit1(c,OP_DIV);}else if(eat(c,T_PERCENT)){parse_unary(c);emit_mod(c);}else break;}}
static void parse_add(Cmp *c){parse_mul(c);for(;;){if(eat(c,T_PLUS)){parse_mul(c);emit1(c,OP_ADD);}else if(eat(c,T_MINUS)){parse_mul(c);emit1(c,OP_SUB);}else break;}}
static void parse_shift(Cmp *c){parse_add(c);for(;;){if(eat(c,T_SHL)){parse_add(c);emit1(c,OP_SFT);}else if(eat(c,T_SHR)){parse_add(c);emit2(c,OP_LIT,add_int(c,0));emit1(c,OP_SWP);emit1(c,OP_SUB);emit1(c,OP_SFT);}else break;}}
static void parse_cmp(Cmp *c){parse_shift(c);for(;;){if(eat(c,T_EQ2)){parse_shift(c);emit1(c,OP_EQU);}else if(eat(c,T_NEQ)){parse_shift(c);emit1(c,OP_NEQ);}else if(eat(c,T_LT)){parse_shift(c);emit1(c,OP_LTH);}else if(eat(c,T_LE)){parse_shift(c);emit1(c,OP_GTH);emit1(c,OP_NOT);}else if(eat(c,T_GT)){parse_shift(c);emit1(c,OP_GTH);}else if(eat(c,T_GE)){parse_shift(c);emit1(c,OP_LTH);emit1(c,OP_NOT);}else break;}}
static void parse_and(Cmp *c){parse_cmp(c);while(eat(c,T_AMPAMP)){parse_cmp(c);emit1(c,OP_AND);}}
static void parse_or(Cmp *c) {parse_and(c);while(eat(c,T_PIPEPIPE)){parse_and(c);emit1(c,OP_ORA);}}
static void parse_expr(Cmp *c) {
    parse_or(c);
    if (eat(c,T_QMARK)) {
        int jf=emit_jmp_false(c);
        parse_expr(c);
        int end=emit_jmp_true(c);
        expect(c,T_COLON,"expected ':' in ternary");
        patch_jmp(c,jf);
        parse_expr(c);
        patch_jmp(c,end);
        return;
    }
    /* Null coalesce: a ?? b */
    if (eat(c,T_QMQM)) {
        emit1(c,OP_DUP); emit2(c,OP_LIT, add_null(c)); emit1(c,OP_EQU);
        int rhs=emit_jmp(c,OP_JCN);
        int done=emit_jmp_true(c);
        patch_jmp(c,rhs);
        emit1(c,OP_POP); parse_expr(c);
        patch_jmp(c,done);
    }
}

/* ════════════════════════════ Statement parser ═════════════════════ */

/* Peek ahead to see if we're looking at an assignment statement.
 * Saves and restores lexer state; does not emit any code. */
static bool peek_is_assign(Cmp *c) {
    if (!check(c,T_IDENT)) return false;
    Lex sv=c->lex;
    consume(c);
    while(check(c,T_DOT)){consume(c);if(check(c,T_IDENT))consume(c);}
    if(check(c,T_LBRACKET)){int d=1;consume(c);while(d>0&&!check(c,T_EOF)){if(check(c,T_LBRACKET))d++;if(check(c,T_RBRACKET))d--;consume(c);}}
    TK k=peek(c).kind;
    c->lex=sv;
    return k==T_ASSIGN||k==T_PLUSEQ||k==T_MINUSEQ||k==T_STAREQ||k==T_SLASHEQ||k==T_PERCENTEQ;
}

static void parse_assign_stmt(Cmp *c) {
    char base[64], full[256];
    Tok t=consume(c); strncpy(base,t.str,63); strncpy(full,t.str,255);
    while(check(c,T_DOT)){Lex sv=c->lex;consume(c);if(!check(c,T_IDENT)){c->lex=sv;break;}Tok m=consume(c);char tmp[256];snprintf(tmp,255,"%s.%s",full,m.str);strncpy(full,tmp,255);}
    if (check(c,T_LBRACKET)) {
        /* arr[idx] = val */
        emit_load(c,base,full);
        consume(c); parse_expr(c); expect(c,T_RBRACKET,"expected ']'");
        expect(c,T_ASSIGN,"expected '=' for array element assign");
        parse_expr(c);
        emit_ext_output(c,"stdr.set",3);
    } else {
        TK op=peek(c).kind; consume(c);
        if (op!=T_ASSIGN) emit_load(c,base,full);
        parse_expr(c);
        switch(op){case T_PLUSEQ:emit1(c,OP_ADD);break;case T_MINUSEQ:emit1(c,OP_SUB);break;
            case T_STAREQ:emit1(c,OP_MUL);break;case T_SLASHEQ:emit1(c,OP_DIV);break;
            case T_PERCENTEQ:emit_mod(c);break;default:break;}
        emit_store(c,base,full);
    }
}

static void parse_control_body(Cmp *c, int header_col) {
    if (eat(c,T_LBRACE)) {
        parse_block(c,0,false);
        expect(c,T_RBRACE,"expected '}'");
    } else if (check(c,T_COLON)) {
        int colon_line=peek(c).line;
        consume(c);
        if (!check(c,T_EOF)&&c->lex.cur.line==colon_line)
            parse_stmt(c);
        else
            parse_block(c,header_col,false);
    } else {
        parse_block(c,header_col,false);
    }
}

static void parse_stmt(Cmp *c) {
    Tok t=peek(c);
    if (t.kind==T_LARROW) {
        int la_line=t.line;
        consume(c);
        if (!check(c,T_EOF)&&!check(c,T_SEMI)&&!check(c,T_RBRACE)&&c->lex.cur.line==la_line) {
            if (line_has_top_comma(c)) {
                emit_ext_call(c,"stdr.array",0);
                do {
                    parse_expr(c);
                    emit_ext_call(c,"stdr.push",2);
                } while(eat(c,T_COMMA));
            } else {
                parse_expr(c);
            }
            emit1(c,OP_RET);
        } else {
            emit2(c,OP_LIT, add_null(c));
            emit1(c,OP_RET); /* empty return */
        }
        return;
    }
    if (t.kind==KW_LET||t.kind==KW_CONST) {
        consume(c); eat(c,T_QMARK);
        if (!check(c,T_IDENT)) die(c,"expected name after let");
        char nm[64]; strncpy(nm,consume(c).str,63);
        int sl=declare_local(c,nm);
        expect(c,T_ASSIGN,"expected '=' in let");
        parse_expr(c); emit2(c,OP_STA,(uint16_t)sl);
        return;
    }
    if (t.kind==KW_IF) {
        int header_col=t.col;
        consume(c); parse_expr(c);
        int jf=emit_jmp_false(c);
        parse_control_body(c,header_col);
        if ((check(c,KW_ELIF)||check(c,KW_ELSE))&&c->lex.cur.col==header_col) {
            int ends[256], nends=0;
            ends[nends++]=emit_jmp_true(c);
            patch_jmp(c,jf);
            while (check(c,KW_ELIF)&&c->lex.cur.col==header_col) {
                int ec=peek(c).col; consume(c); parse_expr(c);
                (void)ec;
                int jf2=emit_jmp_false(c);
                parse_control_body(c,header_col);
                if (nends>=256) die(c,"too many elif branches");
                ends[nends++]=emit_jmp_true(c);
                patch_jmp(c,jf2);
            }
            if (check(c,KW_ELSE)&&c->lex.cur.col==header_col) {
                consume(c);
                parse_control_body(c,header_col);
            }
            for(int i=0;i<nends;i++) patch_jmp(c,ends[i]);
        } else { patch_jmp(c,jf); }
        return;
    }
    if (t.kind==KW_FOR) {
        int header_col=t.col;
        consume(c); expect(c,T_LPAREN,"expected '(' after for");
        if (c->nloops>=MAX_LOOPS) die(c,"too many nested loops");
        Loop *lp=&c->loops[c->nloops++]; memset(lp,0,sizeof(*lp));
        /* Detect foreach: for (let x in ...) */
        bool is_foreach=false;
        {
            Lex sv=c->lex;
            if(check(c,KW_LET)){consume(c);eat(c,T_QMARK);if(check(c,T_IDENT)){consume(c);if(check(c,KW_IN))is_foreach=true;}}
            c->lex=sv;
        }
        if (is_foreach) {
            consume(c); eat(c,T_QMARK);
            char ivar[64]; strncpy(ivar,consume(c).str,63);
            expect(c,KW_IN,"expected 'in'");
            parse_expr(c); expect(c,T_RPAREN,"expected ')'");
            int arr_sl=declare_local(c,"__for_arr__"); emit2(c,OP_STA,(uint16_t)arr_sl);
            emit2(c,OP_LIT,(uint16_t)add_int(c,0));
            int idx_sl=declare_local(c,"__for_idx__"); emit2(c,OP_STA,(uint16_t)idx_sl);
            int item_sl=declare_local(c,ivar); emit2(c,OP_LIT, add_null(c)); emit2(c,OP_STA,(uint16_t)item_sl);
            int cond=cf(c)->code_sz;
            emit2(c,OP_LDA,(uint16_t)idx_sl); emit2(c,OP_LDA,(uint16_t)arr_sl); emit1(c,OP_DEI); emit_u16(c, find_or_add_ext(c,"stdr.len")); emit1(c,1); emit1(c,OP_LTH);
            int jend=emit_jmp_false(c);
            emit2(c,OP_LDA,(uint16_t)arr_sl); emit2(c,OP_LDA,(uint16_t)idx_sl); emit_ext_call(c,"stdr.at",2); emit2(c,OP_STA,(uint16_t)item_sl);
            parse_block(c,header_col,false);
            int cont_here=cf(c)->code_sz;
            for(int i=0;i<lp->nconts;i++) patch_jmp_to(c,lp->conts[i],cont_here);
            emit2(c,OP_LDA,(uint16_t)idx_sl); emit2(c,OP_LIT,(uint16_t)add_int(c,1)); emit1(c,OP_ADD); emit2(c,OP_STA,(uint16_t)idx_sl);
            emit1(c,OP_JMP); emit_i32(c,(int32_t)(cond-(cf(c)->code_sz+4)));
            patch_jmp(c,jend);
        } else {
            /* C-style for: (init; cond; step) */
            if (check(c,KW_LET)||check(c,KW_CONST)) {
                consume(c); eat(c,T_QMARK);
                char nm[64]; strncpy(nm,consume(c).str,63);
                int sl=declare_local(c,nm);
                expect(c,T_ASSIGN,"expected '=' in for init");
                parse_expr(c); emit2(c,OP_STA,(uint16_t)sl);
            } else if (!check(c,T_SEMI)) {
                parse_expr(c); emit1(c,OP_POP);
            }
            expect(c,T_SEMI,"expected ';' in for");
            int cond_start=cf(c)->code_sz;
            int jend=-1;
            if (!check(c,T_SEMI)) {
                parse_expr(c); /* condition */
                jend=emit_jmp_false(c);
            }
            expect(c,T_SEMI,"expected ';' in for");
            /* Save lex at step start; skip past ')' */
            Lex step_lex=c->lex;
            int paren=0;
            while(!check(c,T_EOF)){if(check(c,T_LPAREN))paren++;if(check(c,T_RPAREN)){if(paren==0)break;paren--;}consume(c);}
            expect(c,T_RPAREN,"expected ')' in for");
            /* Parse body */
            parse_block(c,header_col,false);
            int cont_here=cf(c)->code_sz;
            for(int i=0;i<lp->nconts;i++) patch_jmp_to(c,lp->conts[i],cont_here);
            /* Emit step from saved lex */
            Lex body_end_lex=c->lex; c->lex=step_lex;
            if (peek_is_assign(c)) parse_assign_stmt(c);
            else if (!check(c,T_RPAREN)&&!check(c,T_EOF)) { parse_expr(c); emit1(c,OP_POP); }
            c->lex=body_end_lex;
            /* Jump back */
            emit1(c,OP_JMP); emit_i32(c,(int32_t)(cond_start-(cf(c)->code_sz+4)));
            if (jend>=0) patch_jmp(c,jend);
        }
        for(int i=0;i<lp->nbreaks;i++) patch_jmp(c,lp->breaks[i]);
        c->nloops--;
        return;
    }
    if (t.kind==KW_BREAK) {
        consume(c);
        if (!c->nloops) die(c,"break outside loop");
        Loop *lp=&c->loops[c->nloops-1];
        lp->breaks[lp->nbreaks++]=emit_jmp_true(c);
        return;
    }
    if (t.kind==KW_CONTINUE) {
        consume(c);
        if (!c->nloops) die(c,"continue outside loop");
        Loop *lp=&c->loops[c->nloops-1];
        lp->conts[lp->nconts++]=emit_jmp_true(c);
        return;
    }
    if (check(c,T_IDENT) && peek_is_assign(c)) { parse_assign_stmt(c); return; }
    /* Expression statement (function call, etc.) */
    parse_expr(c); emit1(c,OP_POP);
}

/* Parse a block of statements.
 * until_col: stop when next statement's column <= until_col (0 = parse until ';').
 * stop_at_semi: if true, stop and consume ';' when seen (used for function bodies). */
static void parse_block(Cmp *c, int until_col, bool stop_at_semi) {
    int block_col=-1;
    while (!check(c,T_EOF)) {
        if (stop_at_semi && check(c,T_SEMI)) { consume(c); break; }
        if (!stop_at_semi && check(c,T_SEMI)) { consume(c); continue; }
        if (check(c,T_RBRACE)) break;
        if ((check(c,KW_ELSE)||check(c,KW_ELIF)) && (until_col<=0 || c->lex.cur.col<=until_col)) break;
        if (block_col<0) {
            /* First statement in block: set block indentation */
            if (until_col>0 && c->lex.cur.col<=until_col) break;
            block_col=c->lex.cur.col;
        } else {
            if (c->lex.cur.col<block_col) break;
        }
        parse_stmt(c);
    }
}

static void compile_lambda(Cmp *c) {
    Func *outer=cf(c);
    int captures=outer->arity+outer->nlocals;
    char name[128]; snprintf(name,127,"%s.__lambda_%d",c->cur_mod,c->lambda_id++);
    int fi=new_func(c,name,captures);
    Func *lf=&c->funcs[fi];
    for(int i=0;i<captures;i++) strncpy(lf->locals[i],outer->locals[i],63);

    int sv=c->cur_func; c->cur_func=fi;
    expect(c,T_LBRACE,"expected '{' in lambda");
    parse_block(c,0,false);
    expect(c,T_RBRACE,"expected '}' in lambda");
    emit2(c,OP_LIT, add_null(c)); emit1(c,OP_RET);
    c->cur_func=sv;

    for(int i=0;i<captures;i++) emit2(c,OP_LDA,(uint16_t)i);
    emit1(c,OP_CLO);
    emit_u16(c,(uint16_t)fi);
    emit1(c,(uint8_t)captures);
}

/* ════════════════════════════ Declaration parser ════════════════════ */

static int new_func(Cmp *c, const char *fullname, int arity) {
    int idx=c->nfuncs++;
    Func *f=&c->funcs[idx]; memset(f,0,sizeof(*f));
    strncpy(f->fullname,fullname,127); f->name_idx=add_str(c,fullname); f->arity=arity;
    return idx;
}

static int begin_func(Cmp *c, const char *fullname) {
    int idx=find_func(c,fullname);
    if (idx<0) return new_func(c,fullname,0);
    Func *f=&c->funcs[idx];
    int name_idx=f->name_idx;
    free(f->code);
    memset(f,0,sizeof(*f));
    strncpy(f->fullname,fullname,127);
    f->name_idx=name_idx;
    return idx;
}
static void parse_params(Cmp *c, int fi) {
    Func *f=&c->funcs[fi];
    if (check(c,T_RPAREN)) return;
    do {
        eat(c,T_QMARK);
        if (!check(c,T_IDENT)) break;
        Tok nm=consume(c); strncpy(f->locals[f->arity],nm.str,63); f->arity++;
        if (eat(c,T_ASSIGN)) skip_type_ann(c); /* optional type hint */
    } while(eat(c,T_COMMA));
}

static void compile_func(Cmp *c, const char *fullname, bool is_entry) {
    int fi=begin_func(c,fullname);
    int sv=c->cur_func; c->cur_func=fi;
    expect(c,T_LPAREN,"expected '('");
    parse_params(c,fi);
    expect(c,T_RPAREN,"expected ')'");
    if (check(c,T_LPAREN)) skip_type_ann(c);
    parse_block(c,0,true); /* stop at ';' */
    if (c->funcs[fi].arity==0&&c->funcs[fi].code_sz==0)
        fprintf(stderr,"warning: empty function '%s'\n",fullname);
    emit2(c,OP_LIT, add_null(c)); emit1(c,OP_RET);
    if (is_entry) c->entry_func=fi;
    c->cur_func=sv;
}

static void parse_top(Cmp *c) {
    while (!check(c,T_EOF)) {
        eat(c,KW_PUB);
        Tok t=peek(c);
        if (t.kind==KW_CASK) { consume(c); if(check(c,T_IDENT)) strncpy(c->cur_mod,consume(c).str,63); continue; }
        if (t.kind==KW_BRING) {
            consume(c);
            char mn[64]; strncpy(mn,check(c,T_IDENT)?peek(c).str:"",63); consume(c);
            while(eat(c,T_DOT)){if(check(c,T_IDENT))consume(c);}
            if (!strcmp(mn,"stdr")||!strcmp(mn,"math")||!strcmp(mn,"vimana"))
                strncpy(c->ext_mods[c->n_ext_mods++],mn,63);
            continue;
        }
        if (t.kind==KW_DEF||t.kind==KW_CONST) {
            consume(c); eat(c,T_QMARK);
            if (!check(c,T_IDENT)) die(c,"expected name");
            char nm[128]; snprintf(nm,127,"%s.%s",c->cur_mod,consume(c).str);
            int gi=find_global(c,nm);
            if (gi<0) gi=declare_global(c,nm);
            if (eat(c,T_ASSIGN)) {
                int sv=c->cur_func; c->cur_func=c->init_func;
                parse_expr(c); emit2(c,OP_STZ,(uint16_t)gi);
                c->cur_func=sv;
            }
            continue;
        }
        if (t.kind==T_DCOLON||t.kind==T_COLON) {
            consume(c);
            if (!check(c,T_IDENT)) die(c,"expected function name");
            char fn[128]; snprintf(fn,127,"%s.%s",c->cur_mod,consume(c).str);
            compile_func(c,fn,false);
            continue;
        }
        if (t.kind==T_ARROW) {
            consume(c);
            char fn[128]; snprintf(fn,127,"%s.__entry__",c->cur_mod);
            compile_func(c,fn,true);
            continue;
        }
        /* Skip unknown tokens (class, enum, struct, interface, etc.) */
        consume(c);
    }
}

/* ════════════════════════════════ ROM writer ═══════════════════════ */

static void wu8 (FILE *f,uint8_t  v){fwrite(&v,1,1,f);}
static void wu16(FILE *f,uint16_t v){uint8_t b[2]={v&0xFF,(v>>8)&0xFF};fwrite(b,1,2,f);}
static void wu32(FILE *f,uint32_t v){uint8_t b[4]={v,v>>8,v>>16,v>>24};fwrite(b,1,4,f);}
static void wi64(FILE *f,int64_t  v){fwrite(&v,8,1,f);}
static void wf64(FILE *f,double   v){fwrite(&v,8,1,f);}

static void write_rom(Cmp *c, FILE *f) {
    wu32(f,YROM_MAGIC); wu16(f,YROM_VERSION); wu16(f,0);
    wu32(f,(uint32_t)c->nconsts); wu32(f,(uint32_t)c->nfuncs);
    wu32(f,(uint32_t)c->nglobals); wu32(f,(uint32_t)c->nexts);
    wu32(f,c->entry_func<0?0xFFFFFFFFu:(uint32_t)c->entry_func);
    for(int i=0;i<c->nconsts;i++){
        Con *k=&c->consts[i]; wu8(f,k->type);
        switch(k->type){case YCON_INT:wi64(f,k->ival);break;case YCON_FLT:wf64(f,k->fval);break;
            case YCON_STR:wu32(f,(uint32_t)k->slen);fwrite(k->sval,1,(size_t)k->slen,f);break;
            case YCON_BOOL:wu8(f,(uint8_t)k->ival);break;case YCON_NULL:break;}
    }
    for(int i=0;i<c->nglobals;i++){wu32(f,(uint32_t)c->globals[i].name_idx);wu32(f,c->globals[i].init_idx<0?0xFFFFFFFFu:(uint32_t)c->globals[i].init_idx);}
    for(int i=0;i<c->nexts;i++) wu32(f,(uint32_t)c->exts[i].name_idx);
    for(int i=0;i<c->nfuncs;i++){
        Func *fn=&c->funcs[i];
        wu32(f,(uint32_t)fn->name_idx); wu16(f,(uint16_t)fn->arity); wu16(f,(uint16_t)fn->nlocals); wu32(f,(uint32_t)fn->code_sz);
        if (fn->code_sz>0) fwrite(fn->code,1,(size_t)fn->code_sz,f);
    }
}

/* ══════════════════════════════════ Main ═══════════════════════════ */

static char *read_file(const char *path) {
    FILE *f=fopen(path,"r"); if(!f){fprintf(stderr,"cannot open '%s'\n",path);return NULL;}
    fseek(f,0,SEEK_END); long sz=ftell(f); rewind(f);
    char *buf=malloc((size_t)sz+2); (void)fread(buf,1,(size_t)sz,f); buf[sz]='\n'; buf[sz+1]=0; fclose(f); return buf;
}

static bool is_builtin_module(const char *name) {
    return !strcmp(name,"stdr")||!strcmp(name,"math")||!strcmp(name,"vimana");
}

static void path_dirname(const char *path, char *out, size_t outsz) {
    const char *slash=strrchr(path,'/');
    if (!slash) { snprintf(out,outsz,"."); return; }
    size_t len=(size_t)(slash-path);
    if (len>=outsz) len=outsz-1;
    memcpy(out,path,len); out[len]=0;
}

static void path_join_module(char *out, size_t outsz, const char *dir, const char *module) {
    if (!strcmp(dir,".")) snprintf(out,outsz,"%s.yi",module);
    else snprintf(out,outsz,"%s/%s.yi",dir,module);
}

static bool file_exists(const char *path) {
    FILE *f=fopen(path,"rb");
    if (!f) return false;
    fclose(f);
    return true;
}

#define MAX_INPUT_FILES 2048

typedef struct {
    char paths[MAX_INPUT_FILES][PATH_MAX];
    int  state[MAX_INPUT_FILES];
    int  order[MAX_INPUT_FILES];
    int  norder;
    int  count;
} FileSet;

static int fileset_find(FileSet *fs, const char *path) {
    for(int i=0;i<fs->count;i++) if(!strcmp(fs->paths[i],path)) return i;
    return -1;
}

static int fileset_add(FileSet *fs, const char *path) {
    int i=fileset_find(fs,path);
    if (i>=0) return i;
    if (fs->count>=MAX_INPUT_FILES) { fprintf(stderr,"too many input files\n"); exit(1); }
    i=fs->count++;
    strncpy(fs->paths[i],path,PATH_MAX-1);
    fs->paths[i][PATH_MAX-1]=0;
    fs->state[i]=0;
    return i;
}

static void collect_file(FileSet *fs, int idx) {
    if (fs->state[idx]==2) return;
    if (fs->state[idx]==1) return;
    fs->state[idx]=1;

    char *src=read_file(fs->paths[idx]);
    if (!src) exit(1);

    char dir[PATH_MAX];
    path_dirname(fs->paths[idx],dir,sizeof(dir));

    Lex lex={.src=src,.line=1};
    lex_step(&lex);
    while (lex.cur.kind!=T_EOF) {
        if (lex.cur.kind==KW_BRING) {
            lex_step(&lex);
            if (lex.cur.kind==T_IDENT) {
                char module[64];
                strncpy(module,lex.cur.str,63);
                module[63]=0;
                lex_step(&lex);
                bool dotted=false;
                while (lex.cur.kind==T_DOT) {
                    dotted=true;
                    lex_step(&lex);
                    if (lex.cur.kind==T_IDENT) lex_step(&lex);
                }
                if (!dotted&&!is_builtin_module(module)) {
                    char dep[PATH_MAX];
                    path_join_module(dep,sizeof(dep),dir,module);
                    if (file_exists(dep)) collect_file(fs,fileset_add(fs,dep));
                }
                continue;
            }
        }
        lex_step(&lex);
    }

    free(src);
    fs->state[idx]=2;
    fs->order[fs->norder++]=idx;
}

static void compile_file(Cmp *c, const char *path) {
    char *src=read_file(path); if(!src) exit(1);
    c->lex=(Lex){.src=src,.line=1};
    lex_step(&c->lex);
    const char *base=strrchr(path,'/'); base=base?base+1:path;
    char mod[64]; strncpy(mod,base,63); mod[63]=0; char *dot=strrchr(mod,'.'); if(dot)*dot=0;
    strncpy(c->cur_mod,mod,63);
    parse_top(c);
    free(src);
}

static void predeclare_file(Cmp *c, const char *path) {
    char *src=read_file(path); if(!src) exit(1);
    Lex lex={.src=src,.line=1};
    const char *base=strrchr(path,'/'); base=base?base+1:path;
    char mod[64]; strncpy(mod,base,63); mod[63]=0; char *dot=strrchr(mod,'.'); if(dot)*dot=0;
    lex_step(&lex);
    while (lex.cur.kind!=T_EOF) {
        if (lex.cur.col==1&&lex.cur.kind==KW_CASK) {
            lex_step(&lex);
            if (lex.cur.kind==T_IDENT) { strncpy(mod,lex.cur.str,63); mod[63]=0; }
        } else if (lex.cur.col==1&&(lex.cur.kind==KW_DEF||lex.cur.kind==KW_CONST)) {
            lex_step(&lex); if (lex.cur.kind==T_QMARK) lex_step(&lex);
            if (lex.cur.kind==T_IDENT) {
                char nm[128]; snprintf(nm,127,"%s.%s",mod,lex.cur.str);
                if (find_global(c,nm)<0) declare_global(c,nm);
            }
        } else if (lex.cur.col==1&&(lex.cur.kind==T_COLON||lex.cur.kind==T_DCOLON)) {
            lex_step(&lex);
            if (lex.cur.kind==T_IDENT) {
                char fn[128]; snprintf(fn,127,"%s.%s",mod,lex.cur.str);
                if (find_func(c,fn)<0) new_func(c,fn,0);
            }
        } else if (lex.cur.col==1&&lex.cur.kind==T_ARROW) {
            char fn[128]; snprintf(fn,127,"%s.__entry__",mod);
            int fi=find_func(c,fn);
            if (fi<0) fi=new_func(c,fn,0);
            c->entry_func=fi;
        }
        lex_step(&lex);
    }
    free(src);
}

int main(int argc, char **argv) {
    const char *out="out.rom"; int i=1;
    if (i<argc&&!strcmp(argv[i],"-o")){
        if (++i>=argc){fprintf(stderr,"usage: vinasm [-o out.rom] app_main.yi [file.yi ...]\n");return 1;}
        out=argv[i++];
    }
    if (i>=argc){fprintf(stderr,"usage: vinasm [-o out.rom] app_main.yi [file.yi ...]\n");return 1;}
    Cmp *c=calloc(1,sizeof(Cmp));
    c->entry_func=0xFFFFFFFFu;
    strncpy(c->ext_mods[c->n_ext_mods++],"stdr",63);
    strncpy(c->ext_mods[c->n_ext_mods++],"math",63);
    strncpy(c->ext_mods[c->n_ext_mods++],"vimana",63);
    /* func 0 = implicit global initialiser */
    c->init_func=new_func(c,"__init__",0); c->cur_func=c->init_func;
    FileSet fs={0};
    for(;i<argc;i++) collect_file(&fs,fileset_add(&fs,argv[i]));
    for(int n=0;n<fs.norder;n++) predeclare_file(c,fs.paths[fs.order[n]]);
    for(int n=0;n<fs.norder;n++) compile_file(c,fs.paths[fs.order[n]]);
    for(int n=1;n<c->nfuncs;n++)
        if (c->funcs[n].code_sz==0)
            fprintf(stderr,"warning: declared but not compiled '%s'\n",c->funcs[n].fullname);
    /* Finish init function */
    int sv=c->cur_func; c->cur_func=c->init_func; emit2(c,OP_LIT, add_null(c)); emit1(c,OP_RET); c->cur_func=sv;
    resolve_patches(c);
    FILE *f=fopen(out,"wb"); if(!f){fprintf(stderr,"cannot write '%s'\n",out);return 1;}
    write_rom(c,f); fclose(f);
    fprintf(stderr,"wrote %s  [%d consts  %d funcs  %d globals  %d exts]\n",out,c->nconsts,c->nfuncs,c->nglobals,c->nexts);
    free(c); return 0;
}
