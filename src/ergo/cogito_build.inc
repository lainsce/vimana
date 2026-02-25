static bool program_uses_cogito(Program *prog) {
    if (!prog) return false;
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        for (size_t j = 0; j < m->imports_len; j++) {
            if (str_eq_c(m->imports[j]->name, "cogito")) {
                return true;
            }
        }
    }
    return false;
}

static void program_find_cogito_appid_stmt(Stmt *s, char *out, size_t out_cap, bool *found);

static void program_find_cogito_appid_expr(Expr *e, char *out, size_t out_cap, bool *found) {
    if (!e) return;
    switch (e->kind) {
        case EXPR_CALL: {
            Expr *fn = e->as.call.fn;
            bool matches_set_appid = false;
            int arg_index = 0;
            if (fn && fn->kind == EXPR_MEMBER && str_eq_c(fn->as.member.name, "set_appid")) {
                matches_set_appid = true;
                arg_index = 0;
            } else if (fn && fn->kind == EXPR_IDENT && str_eq_c(fn->as.ident.name, "__cogito_app_set_appid")) {
                matches_set_appid = true;
                arg_index = 1;
            }
            if (matches_set_appid && e->as.call.args_len > (size_t)arg_index) {
                char candidate[256];
                if (expr_string_literal_as_filename(e->as.call.args[arg_index], candidate, sizeof(candidate))) {
                    snprintf(out, out_cap, "%s", candidate);
                    *found = true;
                }
            }
            program_find_cogito_appid_expr(fn, out, out_cap, found);
            for (size_t i = 0; i < e->as.call.args_len; i++) {
                program_find_cogito_appid_expr(e->as.call.args[i], out, out_cap, found);
            }
            break;
        }
        case EXPR_UNARY:
            program_find_cogito_appid_expr(e->as.unary.x, out, out_cap, found);
            break;
        case EXPR_BINARY:
            program_find_cogito_appid_expr(e->as.binary.a, out, out_cap, found);
            program_find_cogito_appid_expr(e->as.binary.b, out, out_cap, found);
            break;
        case EXPR_ASSIGN:
            program_find_cogito_appid_expr(e->as.assign.target, out, out_cap, found);
            program_find_cogito_appid_expr(e->as.assign.value, out, out_cap, found);
            break;
        case EXPR_INDEX:
            program_find_cogito_appid_expr(e->as.index.a, out, out_cap, found);
            program_find_cogito_appid_expr(e->as.index.i, out, out_cap, found);
            break;
        case EXPR_MEMBER:
            program_find_cogito_appid_expr(e->as.member.a, out, out_cap, found);
            break;
        case EXPR_PAREN:
            program_find_cogito_appid_expr(e->as.paren.x, out, out_cap, found);
            break;
        case EXPR_MATCH:
            program_find_cogito_appid_expr(e->as.match_expr.scrut, out, out_cap, found);
            for (size_t i = 0; i < e->as.match_expr.arms_len; i++) {
                MatchArm *arm = e->as.match_expr.arms[i];
                if (arm) program_find_cogito_appid_expr(arm->expr, out, out_cap, found);
            }
            break;
        case EXPR_LAMBDA:
            program_find_cogito_appid_expr(e->as.lambda.body, out, out_cap, found);
            break;
        case EXPR_BLOCK:
            program_find_cogito_appid_stmt(e->as.block_expr.block, out, out_cap, found);
            break;
        case EXPR_NEW:
            for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                program_find_cogito_appid_expr(e->as.new_expr.args[i], out, out_cap, found);
            }
            break;
        case EXPR_IF:
            for (size_t i = 0; i < e->as.if_expr.arms_len; i++) {
                ExprIfArm *arm = e->as.if_expr.arms[i];
                if (!arm) continue;
                program_find_cogito_appid_expr(arm->cond, out, out_cap, found);
                program_find_cogito_appid_expr(arm->value, out, out_cap, found);
            }
            break;
        case EXPR_TERNARY:
            program_find_cogito_appid_expr(e->as.ternary.cond, out, out_cap, found);
            program_find_cogito_appid_expr(e->as.ternary.then_expr, out, out_cap, found);
            program_find_cogito_appid_expr(e->as.ternary.else_expr, out, out_cap, found);
            break;
        case EXPR_MOVE:
            program_find_cogito_appid_expr(e->as.move.x, out, out_cap, found);
            break;
        case EXPR_TUPLE:
            for (size_t i = 0; i < e->as.tuple_lit.items_len; i++) {
                program_find_cogito_appid_expr(e->as.tuple_lit.items[i], out, out_cap, found);
            }
            break;
        case EXPR_ARRAY:
            for (size_t i = 0; i < e->as.array_lit.items_len; i++) {
                program_find_cogito_appid_expr(e->as.array_lit.items[i], out, out_cap, found);
            }
            break;
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_STR:
        case EXPR_IDENT:
        case EXPR_NULL:
        case EXPR_BOOL:
            break;
    }
}

static void program_find_cogito_appid_stmt(Stmt *s, char *out, size_t out_cap, bool *found) {
    if (!s) return;
    switch (s->kind) {
        case STMT_LET:
            program_find_cogito_appid_expr(s->as.let_s.expr, out, out_cap, found);
            break;
        case STMT_CONST:
            program_find_cogito_appid_expr(s->as.const_s.expr, out, out_cap, found);
            break;
        case STMT_IF:
            for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
                IfArm *arm = s->as.if_s.arms[i];
                if (!arm) continue;
                program_find_cogito_appid_expr(arm->cond, out, out_cap, found);
                program_find_cogito_appid_stmt(arm->body, out, out_cap, found);
            }
            break;
        case STMT_FOR:
            program_find_cogito_appid_stmt(s->as.for_s.init, out, out_cap, found);
            program_find_cogito_appid_expr(s->as.for_s.cond, out, out_cap, found);
            program_find_cogito_appid_expr(s->as.for_s.step, out, out_cap, found);
            program_find_cogito_appid_stmt(s->as.for_s.body, out, out_cap, found);
            break;
        case STMT_FOREACH:
            program_find_cogito_appid_expr(s->as.foreach_s.expr, out, out_cap, found);
            program_find_cogito_appid_stmt(s->as.foreach_s.body, out, out_cap, found);
            break;
        case STMT_RETURN:
            program_find_cogito_appid_expr(s->as.ret_s.expr, out, out_cap, found);
            break;
        case STMT_BREAK:
        case STMT_CONTINUE:
            break;
        case STMT_EXPR:
            program_find_cogito_appid_expr(s->as.expr_s.expr, out, out_cap, found);
            break;
        case STMT_BLOCK:
            for (size_t i = 0; i < s->as.block_s.stmts_len; i++) {
                program_find_cogito_appid_stmt(s->as.block_s.stmts[i], out, out_cap, found);
            }
            break;
    }
}

static bool program_find_cogito_appid_name(Program *prog, char *out, size_t out_cap) {
    if (!prog || !out || out_cap == 0) return false;
    bool found = false;
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        if (!m) continue;
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (!d) continue;
            switch (d->kind) {
                case DECL_ENTRY:
                    program_find_cogito_appid_stmt(d->as.entry.body, out, out_cap, &found);
                    break;
                case DECL_FUN:
                    program_find_cogito_appid_stmt(d->as.fun.body, out, out_cap, &found);
                    break;
                case DECL_CONST:
                    program_find_cogito_appid_expr(d->as.const_decl.expr, out, out_cap, &found);
                    break;
                case DECL_DEF:
                    program_find_cogito_appid_expr(d->as.def_decl.expr, out, out_cap, &found);
                    break;
                case DECL_CLASS:
                    for (size_t k = 0; k < d->as.class_decl.methods_len; k++) {
                        FunDecl *meth = d->as.class_decl.methods[k];
                        if (meth) program_find_cogito_appid_stmt(meth->body, out, out_cap, &found);
                    }
                    break;
            }
        }
    }
    return found;
}

static const char *cogito_default_cflags(void) {
    // Try cwd-relative paths first
    if (path_is_file("cogito/src/cogito.h")) {
        return "-Icogito/src";
    }
    if (path_is_file("../cogito/src/cogito.h")) {
        return "-I../cogito/src";
    }
    if (path_is_file("../../cogito/src/cogito.h")) {
        return "-I../../cogito/src";
    }
    if (path_is_file("cogito/include/cogito.h")) {
        return "-Icogito/include";
    }
    if (path_is_file("../cogito/include/cogito.h")) {
        return "-I../cogito/include";
    }
    if (path_is_file("../../cogito/include/cogito.h")) {
        return "-I../../cogito/include";
    }
    // Try relative to executable (binary at <project>/ergo/build/ergo)
    char *exe_dir = ergo_exe_dir();
    if (exe_dir) {
        static char cflag_buf[512];
        const char *rel[] = {
            "../../cogito/src/cogito.h",
            "../../cogito/include/cogito.h",
        };
        for (size_t i = 0; i < sizeof(rel) / sizeof(rel[0]); i++) {
            char *p = path_join(exe_dir, rel[i]);
            if (p && path_is_file(p)) {
                char *dir = path_dirname(p);
                if (dir) {
                    snprintf(cflag_buf, sizeof(cflag_buf), "-I%s", dir);
                    free(dir);
                    free(p);
                    free(exe_dir);
                    return cflag_buf;
                }
                free(dir);
            }
            free(p);
        }
        free(exe_dir);
    }
    return "";
}

static const char *cogito_default_ldflags(void) {
#if defined(__APPLE__)
    const char *libname = "libcogito.dylib";
#elif defined(_WIN32)
    const char *libname = "cogito.dll";
#else
    const char *libname = "libcogito.so";
#endif
    static char buf[1024];

    // 1) Check development build directories first (cwd-relative, resolved to absolute)
    {
        const char *dev_dirs[] = {
            "cogito/_build",
            "cogito/build",
            "../cogito/_build",
            "../cogito/build",
            "../../cogito/_build",
            "../../cogito/build",
        };
        for (size_t i = 0; i < sizeof(dev_dirs) / sizeof(dev_dirs[0]); i++) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", dev_dirs[i], libname);
            if (path_is_file(path)) {
#if defined(__APPLE__) || defined(__linux__)
                char *abs_dir = path_abs(dev_dirs[i]);
                if (abs_dir) {
                    snprintf(buf, sizeof(buf), "-L%s -lcogito -Wl,-rpath,%s", abs_dir, abs_dir);
                    free(abs_dir);
                } else {
                    snprintf(buf, sizeof(buf), "-L%s -lcogito -Wl,-rpath,%s", dev_dirs[i], dev_dirs[i]);
                }
#else
                snprintf(buf, sizeof(buf), "-L%s -lcogito", dev_dirs[i]);
#endif
                return buf;
            }
        }
    }

    // 2) Try relative to the executable (binary at <project>/ergo/build/ergo)
    {
        char *exe_dir = ergo_exe_dir();
        if (exe_dir) {
            const char *rel_dirs[] = {
                "../../cogito/_build",
                "../../cogito/build",
            };
            for (size_t i = 0; i < sizeof(rel_dirs) / sizeof(rel_dirs[0]); i++) {
                char *dir = path_join(exe_dir, rel_dirs[i]);
                if (dir) {
                    char path[512];
                    snprintf(path, sizeof(path), "%s/%s", dir, libname);
                    if (path_is_file(path)) {
#if defined(__APPLE__) || defined(__linux__)
                        char *abs_dir = path_abs(dir);
                        const char *rpath_dir = abs_dir ? abs_dir : dir;
                        snprintf(buf, sizeof(buf), "-L%s -lcogito -Wl,-rpath,%s", rpath_dir, rpath_dir);
                        free(abs_dir);
#else
                        snprintf(buf, sizeof(buf), "-L%s -lcogito", dir);
#endif
                        free(dir);
                        free(exe_dir);
                        return buf;
                    }
                    free(dir);
                }
            }
            free(exe_dir);
        }
    }

    // 3) Fall back to system install paths
    {
        const char *sys_dirs[] = {
#if defined(__APPLE__)
            "/opt/homebrew/lib",
            "/usr/local/lib",
#endif
            "/usr/lib",
        };
        for (size_t i = 0; i < sizeof(sys_dirs) / sizeof(sys_dirs[0]); i++) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", sys_dirs[i], libname);
            if (path_is_file(path)) {
#if defined(__APPLE__) || defined(__linux__)
                snprintf(buf, sizeof(buf), "-L%s -lcogito -Wl,-rpath,%s", sys_dirs[i], sys_dirs[i]);
#else
                snprintf(buf, sizeof(buf), "-L%s -lcogito", sys_dirs[i]);
#endif
                return buf;
            }
        }
    }

    return "-lcogito";
}
