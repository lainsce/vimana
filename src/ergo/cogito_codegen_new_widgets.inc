// --- ActiveIndicator ---
                    if (str_eq_c(fname, "__cogito_active_indicator")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_active_indicator_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- SwitchBar ---
                    if (str_eq_c(fname, "__cogito_switchbar")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_switchbar_new(%s);", t, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_switchbar_get_checked")) {
                        GenExpr sb;
                        if (!gen_expr(cg, path, e->as.call.args[0], &sb, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_switchbar_get_checked(%s);", t, sb.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", sb.tmp);
                        gen_expr_release_except(cg, &sb, sb.tmp);
                        gen_expr_free(&sb);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_switchbar_set_checked")) {
                        GenExpr sb, val;
                        if (!gen_expr(cg, path, e->as.call.args[0], &sb, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &val, err)) { gen_expr_free(&sb); return false; }
                        w_line(&cg->w, "cogito_switchbar_set_checked(%s, %s);", sb.tmp, val.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", sb.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", val.tmp);
                        gen_expr_release_except(cg, &sb, sb.tmp);
                        gen_expr_release_except(cg, &val, val.tmp);
                        gen_expr_free(&sb);
                        gen_expr_free(&val);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_switchbar_on_change")) {
                        GenExpr sb, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &sb, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &handler, err)) { gen_expr_free(&sb); return false; }
                        w_line(&cg->w, "cogito_switchbar_on_change(%s, %s);", sb.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", sb.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &sb, sb.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&sb);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- DrawingArea ---
                    if (str_eq_c(fname, "__cogito_drawing_area")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_drawing_area_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_drawing_area_on_press")) {
                        GenExpr da, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &da, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &handler, err)) { gen_expr_free(&da); return false; }
                        w_line(&cg->w, "cogito_drawing_area_on_press(%s, %s);", da.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", da.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &da, da.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&da);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_drawing_area_on_drag")) {
                        GenExpr da, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &da, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &handler, err)) { gen_expr_free(&da); return false; }
                        w_line(&cg->w, "cogito_drawing_area_on_drag(%s, %s);", da.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", da.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &da, da.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&da);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_drawing_area_on_release")) {
                        GenExpr da, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &da, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &handler, err)) { gen_expr_free(&da); return false; }
                        w_line(&cg->w, "cogito_drawing_area_on_release(%s, %s);", da.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", da.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &da, da.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&da);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_drawing_area_on_draw")) {
                        GenExpr da, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &da, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &handler, err)) { gen_expr_free(&da); return false; }
                        w_line(&cg->w, "cogito_drawing_area_on_draw(%s, %s);", da.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", da.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &da, da.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&da);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_drawing_area_get_x")) {
                        GenExpr da;
                        if (!gen_expr(cg, path, e->as.call.args[0], &da, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_drawing_area_get_x(%s);", t, da.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", da.tmp);
                        gen_expr_release_except(cg, &da, da.tmp);
                        gen_expr_free(&da);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_drawing_area_get_y")) {
                        GenExpr da;
                        if (!gen_expr(cg, path, e->as.call.args[0], &da, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_drawing_area_get_y(%s);", t, da.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", da.tmp);
                        gen_expr_release_except(cg, &da, da.tmp);
                        gen_expr_free(&da);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_drawing_area_get_pressed")) {
                        GenExpr da;
                        if (!gen_expr(cg, path, e->as.call.args[0], &da, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_drawing_area_get_pressed(%s);", t, da.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", da.tmp);
                        gen_expr_release_except(cg, &da, da.tmp);
                        gen_expr_free(&da);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_drawing_area_clear")) {
                        GenExpr da;
                        if (!gen_expr(cg, path, e->as.call.args[0], &da, err)) return false;
                        w_line(&cg->w, "cogito_drawing_area_clear(%s);", da.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", da.tmp);
                        gen_expr_release_except(cg, &da, da.tmp);
                        gen_expr_free(&da);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_canvas_set_color")) {
                        GenExpr ctx, colorv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &ctx, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &colorv, err)) { gen_expr_free(&ctx); return false; }
                        w_line(&cg->w, "cogito_canvas_set_color(%s, %s);", ctx.tmp, colorv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", ctx.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", colorv.tmp);
                        gen_expr_release_except(cg, &ctx, ctx.tmp);
                        gen_expr_release_except(cg, &colorv, colorv.tmp);
                        gen_expr_free(&ctx);
                        gen_expr_free(&colorv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_canvas_set_line_width")) {
                        GenExpr ctx, widthv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &ctx, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &widthv, err)) { gen_expr_free(&ctx); return false; }
                        w_line(&cg->w, "cogito_canvas_set_line_width(%s, %s);", ctx.tmp, widthv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", ctx.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", widthv.tmp);
                        gen_expr_release_except(cg, &ctx, ctx.tmp);
                        gen_expr_release_except(cg, &widthv, widthv.tmp);
                        gen_expr_free(&ctx);
                        gen_expr_free(&widthv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_canvas_line")) {
                        GenExpr ctx, x1, y1, x2, y2;
                        if (!gen_expr(cg, path, e->as.call.args[0], &ctx, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &x1, err)) { gen_expr_free(&ctx); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &y1, err)) { gen_expr_free(&ctx); gen_expr_free(&x1); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[3], &x2, err)) { gen_expr_free(&ctx); gen_expr_free(&x1); gen_expr_free(&y1); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[4], &y2, err)) { gen_expr_free(&ctx); gen_expr_free(&x1); gen_expr_free(&y1); gen_expr_free(&x2); return false; }
                        w_line(&cg->w, "cogito_canvas_line(%s, %s, %s, %s, %s);", ctx.tmp, x1.tmp, y1.tmp, x2.tmp, y2.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", ctx.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", x1.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", y1.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", x2.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", y2.tmp);
                        gen_expr_release_except(cg, &ctx, ctx.tmp);
                        gen_expr_release_except(cg, &x1, x1.tmp);
                        gen_expr_release_except(cg, &y1, y1.tmp);
                        gen_expr_release_except(cg, &x2, x2.tmp);
                        gen_expr_release_except(cg, &y2, y2.tmp);
                        gen_expr_free(&ctx);
                        gen_expr_free(&x1);
                        gen_expr_free(&y1);
                        gen_expr_free(&x2);
                        gen_expr_free(&y2);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_canvas_rect")) {
                        GenExpr ctx, xv, yv, wv, hv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &ctx, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &xv, err)) { gen_expr_free(&ctx); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &yv, err)) { gen_expr_free(&ctx); gen_expr_free(&xv); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[3], &wv, err)) { gen_expr_free(&ctx); gen_expr_free(&xv); gen_expr_free(&yv); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[4], &hv, err)) { gen_expr_free(&ctx); gen_expr_free(&xv); gen_expr_free(&yv); gen_expr_free(&wv); return false; }
                        w_line(&cg->w, "cogito_canvas_rect(%s, %s, %s, %s, %s);", ctx.tmp, xv.tmp, yv.tmp, wv.tmp, hv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", ctx.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", xv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", yv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", wv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", hv.tmp);
                        gen_expr_release_except(cg, &ctx, ctx.tmp);
                        gen_expr_release_except(cg, &xv, xv.tmp);
                        gen_expr_release_except(cg, &yv, yv.tmp);
                        gen_expr_release_except(cg, &wv, wv.tmp);
                        gen_expr_release_except(cg, &hv, hv.tmp);
                        gen_expr_free(&ctx);
                        gen_expr_free(&xv);
                        gen_expr_free(&yv);
                        gen_expr_free(&wv);
                        gen_expr_free(&hv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_canvas_fill_rect")) {
                        GenExpr ctx, xv, yv, wv, hv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &ctx, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &xv, err)) { gen_expr_free(&ctx); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &yv, err)) { gen_expr_free(&ctx); gen_expr_free(&xv); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[3], &wv, err)) { gen_expr_free(&ctx); gen_expr_free(&xv); gen_expr_free(&yv); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[4], &hv, err)) { gen_expr_free(&ctx); gen_expr_free(&xv); gen_expr_free(&yv); gen_expr_free(&wv); return false; }
                        w_line(&cg->w, "cogito_canvas_fill_rect(%s, %s, %s, %s, %s);", ctx.tmp, xv.tmp, yv.tmp, wv.tmp, hv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", ctx.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", xv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", yv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", wv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", hv.tmp);
                        gen_expr_release_except(cg, &ctx, ctx.tmp);
                        gen_expr_release_except(cg, &xv, xv.tmp);
                        gen_expr_release_except(cg, &yv, yv.tmp);
                        gen_expr_release_except(cg, &wv, wv.tmp);
                        gen_expr_release_except(cg, &hv, hv.tmp);
                        gen_expr_free(&ctx);
                        gen_expr_free(&xv);
                        gen_expr_free(&yv);
                        gen_expr_free(&wv);
                        gen_expr_free(&hv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- Shape ---
                    if (str_eq_c(fname, "__cogito_shape")) {
                        GenExpr preset;
                        if (!gen_expr(cg, path, e->as.call.args[0], &preset, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_shape_new(%s);", t, preset.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", preset.tmp);
                        gen_expr_release_except(cg, &preset, preset.tmp);
                        gen_expr_free(&preset);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_shape_set_preset")) {
                        GenExpr shape, preset;
                        if (!gen_expr(cg, path, e->as.call.args[0], &shape, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &preset, err)) { gen_expr_free(&shape); return false; }
                        w_line(&cg->w, "cogito_shape_set_preset(%s, %s);", shape.tmp, preset.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", shape.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", preset.tmp);
                        gen_expr_release_except(cg, &shape, shape.tmp);
                        gen_expr_release_except(cg, &preset, preset.tmp);
                        gen_expr_free(&shape);
                        gen_expr_free(&preset);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_shape_get_preset")) {
                        GenExpr shape;
                        if (!gen_expr(cg, path, e->as.call.args[0], &shape, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_shape_get_preset(%s);", t, shape.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", shape.tmp);
                        gen_expr_release_except(cg, &shape, shape.tmp);
                        gen_expr_free(&shape);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_shape_set_size")) {
                        GenExpr shape, sizev;
                        if (!gen_expr(cg, path, e->as.call.args[0], &shape, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &sizev, err)) { gen_expr_free(&shape); return false; }
                        w_line(&cg->w, "cogito_shape_set_size(%s, %s);", shape.tmp, sizev.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", shape.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", sizev.tmp);
                        gen_expr_release_except(cg, &shape, shape.tmp);
                        gen_expr_release_except(cg, &sizev, sizev.tmp);
                        gen_expr_free(&shape);
                        gen_expr_free(&sizev);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_shape_get_size")) {
                        GenExpr shape;
                        if (!gen_expr(cg, path, e->as.call.args[0], &shape, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_shape_get_size(%s);", t, shape.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", shape.tmp);
                        gen_expr_release_except(cg, &shape, shape.tmp);
                        gen_expr_free(&shape);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_shape_set_color")) {
                        GenExpr shape, colorv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &shape, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &colorv, err)) { gen_expr_free(&shape); return false; }
                        w_line(&cg->w, "cogito_shape_set_color(%s, %s);", shape.tmp, colorv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", shape.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", colorv.tmp);
                        gen_expr_release_except(cg, &shape, shape.tmp);
                        gen_expr_release_except(cg, &colorv, colorv.tmp);
                        gen_expr_free(&shape);
                        gen_expr_free(&colorv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_shape_set_color_style")) {
                        GenExpr shape, stylev;
                        if (!gen_expr(cg, path, e->as.call.args[0], &shape, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &stylev, err)) { gen_expr_free(&shape); return false; }
                        w_line(&cg->w, "cogito_shape_set_color_style(%s, %s);", shape.tmp, stylev.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", shape.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", stylev.tmp);
                        gen_expr_release_except(cg, &shape, shape.tmp);
                        gen_expr_release_except(cg, &stylev, stylev.tmp);
                        gen_expr_free(&shape);
                        gen_expr_free(&stylev);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_shape_get_color_style")) {
                        GenExpr shape;
                        if (!gen_expr(cg, path, e->as.call.args[0], &shape, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_shape_get_color_style(%s);", t, shape.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", shape.tmp);
                        gen_expr_release_except(cg, &shape, shape.tmp);
                        gen_expr_free(&shape);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_shape_set_vertex")) {
                        GenExpr shape, idx, x, y;
                        if (!gen_expr(cg, path, e->as.call.args[0], &shape, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &idx, err)) { gen_expr_free(&shape); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &x, err)) { gen_expr_free(&shape); gen_expr_free(&idx); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[3], &y, err)) { gen_expr_free(&shape); gen_expr_free(&idx); gen_expr_free(&x); return false; }
                        w_line(&cg->w, "cogito_shape_set_vertex(%s, %s, %s, %s);", shape.tmp, idx.tmp, x.tmp, y.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", shape.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", idx.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", x.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", y.tmp);
                        gen_expr_release_except(cg, &shape, shape.tmp);
                        gen_expr_release_except(cg, &idx, idx.tmp);
                        gen_expr_release_except(cg, &x, x.tmp);
                        gen_expr_release_except(cg, &y, y.tmp);
                        gen_expr_free(&shape);
                        gen_expr_free(&idx);
                        gen_expr_free(&x);
                        gen_expr_free(&y);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_shape_get_vertex_x")) {
                        GenExpr shape, idx;
                        if (!gen_expr(cg, path, e->as.call.args[0], &shape, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &idx, err)) { gen_expr_free(&shape); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_shape_get_vertex_x(%s, %s);", t, shape.tmp, idx.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", shape.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", idx.tmp);
                        gen_expr_release_except(cg, &shape, shape.tmp);
                        gen_expr_release_except(cg, &idx, idx.tmp);
                        gen_expr_free(&shape);
                        gen_expr_free(&idx);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_shape_get_vertex_y")) {
                        GenExpr shape, idx;
                        if (!gen_expr(cg, path, e->as.call.args[0], &shape, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &idx, err)) { gen_expr_free(&shape); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_shape_get_vertex_y(%s, %s);", t, shape.tmp, idx.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", shape.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", idx.tmp);
                        gen_expr_release_except(cg, &shape, shape.tmp);
                        gen_expr_release_except(cg, &idx, idx.tmp);
                        gen_expr_free(&shape);
                        gen_expr_free(&idx);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- ContentList ---
                    if (str_eq_c(fname, "__cogito_content_list")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_content_list_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- EmptyPage ---
                    if (str_eq_c(fname, "__cogito_empty_page")) {
                        GenExpr title;
                        if (!gen_expr(cg, path, e->as.call.args[0], &title, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_empty_page_new(%s);", t, title.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", title.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_free(&title);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_empty_page_set_description")) {
                        GenExpr ep, desc;
                        if (!gen_expr(cg, path, e->as.call.args[0], &ep, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &desc, err)) { gen_expr_free(&ep); return false; }
                        w_line(&cg->w, "cogito_empty_page_set_description(%s, %s);", ep.tmp, desc.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", ep.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", desc.tmp);
                        gen_expr_release_except(cg, &ep, ep.tmp);
                        gen_expr_release_except(cg, &desc, desc.tmp);
                        gen_expr_free(&ep);
                        gen_expr_free(&desc);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_empty_page_set_icon")) {
                        GenExpr ep, icon;
                        if (!gen_expr(cg, path, e->as.call.args[0], &ep, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &icon, err)) { gen_expr_free(&ep); return false; }
                        w_line(&cg->w, "cogito_empty_page_set_icon(%s, %s);", ep.tmp, icon.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", ep.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &ep, ep.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_free(&ep);
                        gen_expr_free(&icon);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_empty_page_set_action")) {
                        GenExpr ep, text, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &ep, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &text, err)) { gen_expr_free(&ep); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &handler, err)) { gen_expr_free(&ep); gen_expr_free(&text); return false; }
                        w_line(&cg->w, "cogito_empty_page_set_action(%s, %s, %s);", ep.tmp, text.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", ep.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &ep, ep.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&ep);
                        gen_expr_free(&text);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- TipView ---
                    if (str_eq_c(fname, "__cogito_tip_view")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_tip_view_new(%s);", t, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_tip_view_set_title")) {
                        GenExpr tv, title;
                        if (!gen_expr(cg, path, e->as.call.args[0], &tv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &title, err)) { gen_expr_free(&tv); return false; }
                        w_line(&cg->w, "cogito_tip_view_set_title(%s, %s);", tv.tmp, title.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", tv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", title.tmp);
                        gen_expr_release_except(cg, &tv, tv.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_free(&tv);
                        gen_expr_free(&title);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- SettingsWindow ---
                    if (str_eq_c(fname, "__cogito_settings_window")) {
                        GenExpr title;
                        if (!gen_expr(cg, path, e->as.call.args[0], &title, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_settings_window_new(%s);", t, title.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", title.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_free(&title);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- SettingsPage ---
                    if (str_eq_c(fname, "__cogito_settings_page")) {
                        GenExpr title;
                        if (!gen_expr(cg, path, e->as.call.args[0], &title, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_settings_page_new(%s);", t, title.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", title.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_free(&title);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- SettingsList ---
                    if (str_eq_c(fname, "__cogito_settings_list")) {
                        GenExpr title;
                        if (!gen_expr(cg, path, e->as.call.args[0], &title, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_settings_list_new(%s);", t, title.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", title.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_free(&title);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- SettingsRow ---
                    if (str_eq_c(fname, "__cogito_settings_row")) {
                        GenExpr label;
                        if (!gen_expr(cg, path, e->as.call.args[0], &label, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_settings_row_new(%s);", t, label.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", label.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_free(&label);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- WelcomeScreen ---
                    if (str_eq_c(fname, "__cogito_welcome_screen")) {
                        GenExpr title;
                        if (!gen_expr(cg, path, e->as.call.args[0], &title, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_welcome_screen_new(%s);", t, title.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", title.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_free(&title);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_welcome_screen_set_description")) {
                        GenExpr ws, desc;
                        if (!gen_expr(cg, path, e->as.call.args[0], &ws, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &desc, err)) { gen_expr_free(&ws); return false; }
                        w_line(&cg->w, "cogito_welcome_screen_set_description(%s, %s);", ws.tmp, desc.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", ws.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", desc.tmp);
                        gen_expr_release_except(cg, &ws, ws.tmp);
                        gen_expr_release_except(cg, &desc, desc.tmp);
                        gen_expr_free(&ws);
                        gen_expr_free(&desc);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_welcome_screen_set_icon")) {
                        GenExpr ws, icon;
                        if (!gen_expr(cg, path, e->as.call.args[0], &ws, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &icon, err)) { gen_expr_free(&ws); return false; }
                        w_line(&cg->w, "cogito_welcome_screen_set_icon(%s, %s);", ws.tmp, icon.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", ws.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &ws, ws.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_free(&ws);
                        gen_expr_free(&icon);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_welcome_screen_set_action")) {
                        GenExpr ws, text, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &ws, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &text, err)) { gen_expr_free(&ws); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &handler, err)) { gen_expr_free(&ws); gen_expr_free(&text); return false; }
                        w_line(&cg->w, "cogito_welcome_screen_set_action(%s, %s, %s);", ws.tmp, text.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", ws.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &ws, ws.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&ws);
                        gen_expr_free(&text);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- ViewDual ---
                    if (str_eq_c(fname, "__cogito_view_dual")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_view_dual_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_view_dual_set_ratio")) {
                        GenExpr vd, ratio;
                        if (!gen_expr(cg, path, e->as.call.args[0], &vd, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &ratio, err)) { gen_expr_free(&vd); return false; }
                        w_line(&cg->w, "cogito_view_dual_set_ratio(%s, %s);", vd.tmp, ratio.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", vd.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", ratio.tmp);
                        gen_expr_release_except(cg, &vd, vd.tmp);
                        gen_expr_release_except(cg, &ratio, ratio.tmp);
                        gen_expr_free(&vd);
                        gen_expr_free(&ratio);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- ViewChooser ---
                    if (str_eq_c(fname, "__cogito_view_chooser")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_view_chooser_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_view_chooser_set_items")) {
                        GenExpr vc, items;
                        if (!gen_expr(cg, path, e->as.call.args[0], &vc, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &items, err)) { gen_expr_free(&vc); return false; }
                        w_line(&cg->w, "cogito_view_chooser_set_items(%s, %s);", vc.tmp, items.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", vc.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", items.tmp);
                        gen_expr_release_except(cg, &vc, vc.tmp);
                        gen_expr_release_except(cg, &items, items.tmp);
                        gen_expr_free(&vc);
                        gen_expr_free(&items);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_view_chooser_bind")) {
                        GenExpr vc, vs;
                        if (!gen_expr(cg, path, e->as.call.args[0], &vc, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &vs, err)) { gen_expr_free(&vc); return false; }
                        w_line(&cg->w, "cogito_view_chooser_bind(%s, %s);", vc.tmp, vs.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", vc.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", vs.tmp);
                        gen_expr_release_except(cg, &vc, vc.tmp);
                        gen_expr_release_except(cg, &vs, vs.tmp);
                        gen_expr_free(&vc);
                        gen_expr_free(&vs);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- AboutWindow ---
                    if (str_eq_c(fname, "__cogito_about_window")) {
                        GenExpr name, version;
                        if (!gen_expr(cg, path, e->as.call.args[0], &name, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &version, err)) { gen_expr_free(&name); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_about_window_new(%s, %s);", t, name.tmp, version.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", name.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", version.tmp);
                        gen_expr_release_except(cg, &name, name.tmp);
                        gen_expr_release_except(cg, &version, version.tmp);
                        gen_expr_free(&name);
                        gen_expr_free(&version);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_about_window_set_icon")) {
                        GenExpr aw, icon;
                        if (!gen_expr(cg, path, e->as.call.args[0], &aw, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &icon, err)) { gen_expr_free(&aw); return false; }
                        w_line(&cg->w, "cogito_about_window_set_icon(%s, %s);", aw.tmp, icon.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", aw.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &aw, aw.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_free(&aw);
                        gen_expr_free(&icon);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_about_window_set_description")) {
                        GenExpr aw, desc;
                        if (!gen_expr(cg, path, e->as.call.args[0], &aw, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &desc, err)) { gen_expr_free(&aw); return false; }
                        w_line(&cg->w, "cogito_about_window_set_description(%s, %s);", aw.tmp, desc.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", aw.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", desc.tmp);
                        gen_expr_release_except(cg, &aw, aw.tmp);
                        gen_expr_release_except(cg, &desc, desc.tmp);
                        gen_expr_free(&aw);
                        gen_expr_free(&desc);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_about_window_set_website")) {
                        GenExpr aw, url;
                        if (!gen_expr(cg, path, e->as.call.args[0], &aw, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &url, err)) { gen_expr_free(&aw); return false; }
                        w_line(&cg->w, "cogito_about_window_set_website(%s, %s);", aw.tmp, url.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", aw.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", url.tmp);
                        gen_expr_release_except(cg, &aw, aw.tmp);
                        gen_expr_release_except(cg, &url, url.tmp);
                        gen_expr_free(&aw);
                        gen_expr_free(&url);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_about_window_set_issue_url")) {
                        GenExpr aw, url;
                        if (!gen_expr(cg, path, e->as.call.args[0], &aw, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &url, err)) { gen_expr_free(&aw); return false; }
                        w_line(&cg->w, "cogito_about_window_set_issue_url(%s, %s);", aw.tmp, url.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", aw.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", url.tmp);
                        gen_expr_release_except(cg, &aw, aw.tmp);
                        gen_expr_release_except(cg, &url, url.tmp);
                        gen_expr_free(&aw);
                        gen_expr_free(&url);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- MenuButton ---
                    if (str_eq_c(fname, "__cogito_menu_button")) {
                        GenExpr icon;
                        if (!gen_expr(cg, path, e->as.call.args[0], &icon, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_menu_button_new(%s);", t, icon.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_free(&icon);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- SplitButton ---
                    if (str_eq_c(fname, "__cogito_split_button")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_split_button_new(%s);", t, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_split_button_add_menu")) {
                        GenExpr btn, label, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &label, err)) { gen_expr_free(&btn); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &handler, err)) { gen_expr_free(&btn); gen_expr_free(&label); return false; }
                        w_line(&cg->w, "cogito_split_button_add_menu(%s, %s, %s);", btn.tmp, label.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&label);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
