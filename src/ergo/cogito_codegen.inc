                    if (str_eq_c(fname,
                    "__cogito_app")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_app_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_window")) {
                        GenExpr title, w, h;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &title, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &w, err)) { gen_expr_free(&title); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &h, err)) { gen_expr_free(&title); gen_expr_free(&w); return false;
                        }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_window_new(%s, %s, %s);", t, title.tmp, w.tmp, h.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", title.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", w.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", h.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_release_except(cg, &w, w.tmp);
                        gen_expr_release_except(cg, &h, h.tmp);
                        gen_expr_free(&title);
                        gen_expr_free(&w);
                        gen_expr_free(&h);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_button")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_button_new(%s);", t, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn")) {
                        GenExpr icon;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &icon, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_iconbtn_new(%s);", t, icon.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_free(&icon);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_label")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_label_new(%s);", t, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_dialog")) {
                        GenExpr title;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &title, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_dialog_new(%s);", t, title.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", title.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_free(&title);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_dialog_slot")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_dialog_slot_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_image")) {
                        GenExpr icon;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &icon, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_image_new(%s);", t, icon.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_free(&icon);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_checkbox")) {
                        GenExpr text, group;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &text, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &group, err)) { gen_expr_free(&text); return false;
                        }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_checkbox_new(%s, %s);", t, text.tmp, group.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", group.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_release_except(cg, &group, group.tmp);
                        gen_expr_free(&text);
                        gen_expr_free(&group);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_switch")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_switch_new(%s);", t, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_textfield")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_textfield_new(%s);", t, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_searchfield")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_searchfield_new(%s);", t, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_textview")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_textview_new(%s);", t, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_dropdown")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_dropdown_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_datepicker")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_datepicker_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_stepper")) {
                        GenExpr minv, maxv, valv, stepv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &minv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &maxv, err)) { gen_expr_free(&minv); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &valv, err)) { gen_expr_free(&minv); gen_expr_free(&maxv); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            3
                        ], &stepv, err)) { gen_expr_free(&minv); gen_expr_free(&maxv); gen_expr_free(&valv); return false;
                        }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_stepper_new(%s, %s, %s, %s);", t, minv.tmp, maxv.tmp, valv.tmp, stepv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", minv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", maxv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", valv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", stepv.tmp);
                        gen_expr_release_except(cg, &minv, minv.tmp);
                        gen_expr_release_except(cg, &maxv, maxv.tmp);
                        gen_expr_release_except(cg, &valv, valv.tmp);
                        gen_expr_release_except(cg, &stepv, stepv.tmp);
                        gen_expr_free(&minv);
                        gen_expr_free(&maxv);
                        gen_expr_free(&valv);
                        gen_expr_free(&stepv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider")) {
                        GenExpr minv, maxv, valv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &minv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &maxv, err)) { gen_expr_free(&minv); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &valv, err)) { gen_expr_free(&minv); gen_expr_free(&maxv); return false;
                        }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_slider_new(%s, %s, %s);", t, minv.tmp, maxv.tmp, valv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", minv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", maxv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", valv.tmp);
                        gen_expr_release_except(cg, &minv, minv.tmp);
                        gen_expr_release_except(cg, &maxv, maxv.tmp);
                        gen_expr_release_except(cg, &valv, valv.tmp);
                        gen_expr_free(&minv);
                        gen_expr_free(&maxv);
                        gen_expr_free(&valv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_range")) {
                        GenExpr minv, maxv, startv, endv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &minv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &maxv, err)) { gen_expr_free(&minv); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &startv, err)) { gen_expr_free(&minv); gen_expr_free(&maxv); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            3
                        ], &endv, err)) { gen_expr_free(&minv); gen_expr_free(&maxv); gen_expr_free(&startv); return false;
                        }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_slider_range_new(%s, %s, %s, %s);", t, minv.tmp, maxv.tmp, startv.tmp, endv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", minv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", maxv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", startv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", endv.tmp);
                        gen_expr_release_except(cg, &minv, minv.tmp);
                        gen_expr_release_except(cg, &maxv, maxv.tmp);
                        gen_expr_release_except(cg, &startv, startv.tmp);
                        gen_expr_release_except(cg, &endv, endv.tmp);
                        gen_expr_free(&minv);
                        gen_expr_free(&maxv);
                        gen_expr_free(&startv);
                        gen_expr_free(&endv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_tabs")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_tabs_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_buttongroup")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_buttongroup_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_view_switcher")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_view_switcher_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_progress")) {
                        GenExpr val;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &val, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_progress_new(%s);", t, val.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", val.tmp);
                        gen_expr_release_except(cg, &val, val.tmp);
                        gen_expr_free(&val);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_treeview")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_treeview_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_colorpicker")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_colorpicker_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_toasts")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_toasts_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_toast")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_toast_new(%s);", t, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_appbar")) {
                        GenExpr title, subtitle;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &title, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &subtitle, err)) { gen_expr_free(&title); return false;
                        }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_appbar_new(%s, %s);", t, title.tmp, subtitle.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", title.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", subtitle.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_release_except(cg, &subtitle, subtitle.tmp);
                        gen_expr_free(&title);
                        gen_expr_free(&subtitle);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_toolbar")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_toolbar_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_toolbar_set_vibrant")) {
                        GenExpr toolbar, vibrant;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &toolbar, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &vibrant, err)) { gen_expr_free(&toolbar); return false;
                        }
                        w_line(&cg->w,
                        "cogito_toolbar_set_vibrant(%s, %s);", toolbar.tmp, vibrant.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", toolbar.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", vibrant.tmp);
                        gen_expr_release_except(cg, &toolbar, toolbar.tmp);
                        gen_expr_release_except(cg, &vibrant, vibrant.tmp);
                        gen_expr_free(&toolbar);
                        gen_expr_free(&vibrant);
                        out->tmp = NULL;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_toolbar_get_vibrant")) {
                        GenExpr toolbar;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &toolbar, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_BOOL(cogito_toolbar_get_vibrant(%s));", t, toolbar.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", toolbar.tmp);
                        gen_expr_release_except(cg, &toolbar, toolbar.tmp);
                        gen_expr_free(&toolbar);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_toolbar_set_vertical")) {
                        GenExpr toolbar, vertical;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &toolbar, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &vertical, err)) { gen_expr_free(&toolbar); return false;
                        }
                        w_line(&cg->w,
                        "cogito_toolbar_set_vertical(%s, %s);", toolbar.tmp, vertical.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", toolbar.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", vertical.tmp);
                        gen_expr_release_except(cg, &toolbar, toolbar.tmp);
                        gen_expr_release_except(cg, &vertical, vertical.tmp);
                        gen_expr_free(&toolbar);
                        gen_expr_free(&vertical);
                        out->tmp = NULL;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_toolbar_get_vertical")) {
                        GenExpr toolbar;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &toolbar, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_BOOL(cogito_toolbar_get_vertical(%s));", t, toolbar.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", toolbar.tmp);
                        gen_expr_release_except(cg, &toolbar, toolbar.tmp);
                        gen_expr_free(&toolbar);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_vstack")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_vstack_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_hstack")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_hstack_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_zstack")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_zstack_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_fixed")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_fixed_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_scroller")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_scroller_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_carousel")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_carousel_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_carousel_item")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_carousel_item_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_carousel_set_active_index")) {
                        GenExpr carousel, idx;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &carousel, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &idx, err)) { gen_expr_free(&carousel); return false;
                        }
                        w_line(&cg->w,
                        "cogito_carousel_set_active_index(%s, %s);", carousel.tmp, idx.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", carousel.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", idx.tmp);
                        gen_expr_release_except(cg, &carousel, carousel.tmp);
                        gen_expr_release_except(cg, &idx, idx.tmp);
                        gen_expr_free(&carousel);
                        gen_expr_free(&idx);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_carousel_get_active_index")) {
                        GenExpr carousel;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &carousel, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_carousel_get_active_index(%s);", t, carousel.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", carousel.tmp);
                        gen_expr_release_except(cg, &carousel, carousel.tmp);
                        gen_expr_free(&carousel);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_carousel_item_set_text")) {
                        GenExpr item, text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &item, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &text, err)) { gen_expr_free(&item); return false;
                        }
                        w_line(&cg->w,
                        "cogito_carousel_item_set_text(%s, %s);", item.tmp, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", item.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &item, item.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&item);
                        gen_expr_free(&text);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_carousel_item_set_halign")) {
                        GenExpr item, align;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &item, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &align, err)) { gen_expr_free(&item); return false;
                        }
                        w_line(&cg->w,
                        "cogito_carousel_item_set_halign(%s, %s);", item.tmp, align.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", item.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", align.tmp);
                        gen_expr_release_except(cg, &item, item.tmp);
                        gen_expr_release_except(cg, &align, align.tmp);
                        gen_expr_free(&item);
                        gen_expr_free(&align);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_carousel_item_set_valign")) {
                        GenExpr item, align;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &item, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &align, err)) { gen_expr_free(&item); return false;
                        }
                        w_line(&cg->w,
                        "cogito_carousel_item_set_valign(%s, %s);", item.tmp, align.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", item.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", align.tmp);
                        gen_expr_release_except(cg, &item, item.tmp);
                        gen_expr_release_except(cg, &align, align.tmp);
                        gen_expr_free(&item);
                        gen_expr_free(&align);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_list")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_list_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_grid")) {
                        GenExpr cols;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &cols, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_grid_new(%s);", t, cols.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", cols.tmp);
                        gen_expr_release_except(cg, &cols, cols.tmp);
                        gen_expr_free(&cols);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_container_add")) {
                        GenExpr parent, child;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &parent, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &child, err)) { gen_expr_free(&parent); return false;
                        }
                        w_line(&cg->w,
                        "cogito_container_add(%s, %s);", parent.tmp, child.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", parent.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", child.tmp);
                        gen_expr_release_except(cg, &parent, parent.tmp);
                        gen_expr_release_except(cg, &child, child.tmp);
                        gen_expr_free(&parent);
                        gen_expr_free(&child);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_container_set_margins")) {
                        GenExpr node, top, right, bottom, left;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &top, err)) { gen_expr_free(&node); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &right, err)) { gen_expr_free(&node); gen_expr_free(&top); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            3
                        ], &bottom, err)) { gen_expr_free(&node); gen_expr_free(&top); gen_expr_free(&right); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            4
                        ], &left, err)) { gen_expr_free(&node); gen_expr_free(&top); gen_expr_free(&right); gen_expr_free(&bottom); return false;
                        }
                        w_line(&cg->w,
                        "cogito_container_set_margins(%s, %s, %s, %s, %s);", node.tmp, top.tmp, right.tmp, bottom.tmp, left.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", top.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", right.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", bottom.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", left.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &top, top.tmp);
                        gen_expr_release_except(cg, &right, right.tmp);
                        gen_expr_release_except(cg, &bottom, bottom.tmp);
                        gen_expr_release_except(cg, &left, left.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&top);
                        gen_expr_free(&right);
                        gen_expr_free(&bottom);
                        gen_expr_free(&left);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_container_set_align")) {
                        GenExpr node, align;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &align, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_container_set_align(%s, %s);", node.tmp, align.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", align.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &align, align.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&align);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_container_set_halign")) {
                        GenExpr node, align;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &align, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_container_set_halign(%s, %s);", node.tmp, align.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", align.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &align, align.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&align);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_container_set_valign")) {
                        GenExpr node, align;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &align, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_container_set_valign(%s, %s);", node.tmp, align.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", align.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &align, align.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&align);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_container_set_hexpand")) {
                        GenExpr node, expand;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &expand, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_container_set_hexpand(%s, %s);", node.tmp, expand.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", expand.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &expand, expand.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&expand);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_container_set_vexpand")) {
                        GenExpr node, expand;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &expand, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_container_set_vexpand(%s, %s);", node.tmp, expand.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", expand.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &expand, expand.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&expand);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_container_set_gap")) {
                        GenExpr node, gap;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &gap, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_container_set_gap(%s, %s);", node.tmp, gap.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", gap.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &gap, gap.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&gap);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_dialog_slot_show")) {
                        GenExpr slot, dialog;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &slot, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &dialog, err)) { gen_expr_free(&slot); return false;
                        }
                        w_line(&cg->w,
                        "cogito_dialog_slot_show(%s, %s);", slot.tmp, dialog.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", slot.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", dialog.tmp);
                        gen_expr_release_except(cg, &slot, slot.tmp);
                        gen_expr_release_except(cg, &dialog, dialog.tmp);
                        gen_expr_free(&slot);
                        gen_expr_free(&dialog);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_dialog_slot_clear")) {
                        GenExpr slot;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &slot, err)) return false;
                        w_line(&cg->w,
                        "cogito_dialog_slot_clear(%s);", slot.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", slot.tmp);
                        gen_expr_release_except(cg, &slot, slot.tmp);
                        gen_expr_free(&slot);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_container_set_padding")) {
                        GenExpr node, top, right, bottom, left;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &top, err)) { gen_expr_free(&node); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &right, err)) { gen_expr_free(&node); gen_expr_free(&top); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            3
                        ], &bottom, err)) { gen_expr_free(&node); gen_expr_free(&top); gen_expr_free(&right); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            4
                        ], &left, err)) { gen_expr_free(&node); gen_expr_free(&top); gen_expr_free(&right); gen_expr_free(&bottom); return false;
                        }
                        w_line(&cg->w,
                        "cogito_container_set_padding(%s, %s, %s, %s, %s);", node.tmp, top.tmp, right.tmp, bottom.tmp, left.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", top.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", right.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", bottom.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", left.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &top, top.tmp);
                        gen_expr_release_except(cg, &right, right.tmp);
                        gen_expr_release_except(cg, &bottom, bottom.tmp);
                        gen_expr_release_except(cg, &left, left.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&top);
                        gen_expr_free(&right);
                        gen_expr_free(&bottom);
                        gen_expr_free(&left);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_fixed_set_pos")) {
                        GenExpr fixedv, child, x, y;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &fixedv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &child, err)) { gen_expr_free(&fixedv); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &x, err)) { gen_expr_free(&fixedv); gen_expr_free(&child); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            3
                        ], &y, err)) { gen_expr_free(&fixedv); gen_expr_free(&child); gen_expr_free(&x); return false;
                        }
                        w_line(&cg->w,
                        "cogito_fixed_set_pos(%s, %s, %s, %s);", fixedv.tmp, child.tmp, x.tmp, y.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fixedv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", child.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", x.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", y.tmp);
                        gen_expr_release_except(cg, &fixedv, fixedv.tmp);
                        gen_expr_release_except(cg, &child, child.tmp);
                        gen_expr_release_except(cg, &x, x.tmp);
                        gen_expr_release_except(cg, &y, y.tmp);
                        gen_expr_free(&fixedv);
                        gen_expr_free(&child);
                        gen_expr_free(&x);
                        gen_expr_free(&y);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_scroller_set_axes")) {
                        GenExpr sc, horz, vert;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &sc, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &horz, err)) { gen_expr_free(&sc); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &vert, err)) { gen_expr_free(&sc); gen_expr_free(&horz); return false;
                        }
                        w_line(&cg->w,
                        "cogito_scroller_set_axes(%s, %s, %s);", sc.tmp, horz.tmp, vert.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", sc.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", horz.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", vert.tmp);
                        gen_expr_release_except(cg, &sc, sc.tmp);
                        gen_expr_release_except(cg, &horz, horz.tmp);
                        gen_expr_release_except(cg, &vert, vert.tmp);
                        gen_expr_free(&sc);
                        gen_expr_free(&horz);
                        gen_expr_free(&vert);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_grid_set_gap")) {
                        GenExpr grid, x, y;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &grid, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &x, err)) { gen_expr_free(&grid); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &y, err)) { gen_expr_free(&grid); gen_expr_free(&x); return false;
                        }
                        w_line(&cg->w,
                        "cogito_grid_set_gap(%s, %s, %s);", grid.tmp, x.tmp, y.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", grid.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", x.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", y.tmp);
                        gen_expr_release_except(cg, &grid, grid.tmp);
                        gen_expr_release_except(cg, &x, x.tmp);
                        gen_expr_release_except(cg, &y, y.tmp);
                        gen_expr_free(&grid);
                        gen_expr_free(&x);
                        gen_expr_free(&y);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_grid_set_span")) {
                        GenExpr child, cols, rows;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &child, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &cols, err)) { gen_expr_free(&child); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &rows, err)) { gen_expr_free(&child); gen_expr_free(&cols); return false;
                        }
                        w_line(&cg->w,
                        "cogito_grid_set_span(%s, %s, %s);", child.tmp, cols.tmp, rows.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", child.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", cols.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", rows.tmp);
                        gen_expr_release_except(cg, &child, child.tmp);
                        gen_expr_release_except(cg, &cols, cols.tmp);
                        gen_expr_release_except(cg, &rows, rows.tmp);
                        gen_expr_free(&child);
                        gen_expr_free(&cols);
                        gen_expr_free(&rows);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_grid_set_align")) {
                        GenExpr child, hx, hy;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &child, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &hx, err)) { gen_expr_free(&child); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &hy, err)) { gen_expr_free(&child); gen_expr_free(&hx); return false;
                        }
                        w_line(&cg->w,
                        "cogito_grid_set_align(%s, %s, %s);", child.tmp, hx.tmp, hy.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", child.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", hx.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", hy.tmp);
                        gen_expr_release_except(cg, &child, child.tmp);
                        gen_expr_release_except(cg, &hx, hx.tmp);
                        gen_expr_release_except(cg, &hy, hy.tmp);
                        gen_expr_free(&child);
                        gen_expr_free(&hx);
                        gen_expr_free(&hy);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_label_set_class")) {
                        GenExpr label, classv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &label, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &classv, err)) { gen_expr_free(&label); return false;
                        }
                        w_line(&cg->w,
                        "cogito_label_set_class(%s, %s);", label.tmp, classv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", classv.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &classv, classv.tmp);
                        gen_expr_free(&label);
                        gen_expr_free(&classv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_label_set_text")) {
                        GenExpr label, textv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &label, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &textv, err)) { gen_expr_free(&label); return false;
                        }
                        w_line(&cg->w,
                        "cogito_label_set_text(%s, %s);", label.tmp, textv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", textv.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &textv, textv.tmp);
                        gen_expr_free(&label);
                        gen_expr_free(&textv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_label_set_wrap")) {
                        GenExpr label, onv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &label, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &onv, err)) { gen_expr_free(&label); return false;
                        }
                        w_line(&cg->w,
                        "cogito_label_set_wrap(%s, %s);", label.tmp, onv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", onv.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &onv, onv.tmp);
                        gen_expr_free(&label);
                        gen_expr_free(&onv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_label_set_ellipsis")) {
                        GenExpr label, onv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &label, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &onv, err)) { gen_expr_free(&label); return false;
                        }
                        w_line(&cg->w,
                        "cogito_label_set_ellipsis(%s, %s);", label.tmp, onv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", onv.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &onv, onv.tmp);
                        gen_expr_free(&label);
                        gen_expr_free(&onv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_label_set_align")) {
                        GenExpr label, alignv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &label, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &alignv, err)) { gen_expr_free(&label); return false;
                        }
                        w_line(&cg->w,
                        "cogito_label_set_align(%s, %s);", label.tmp, alignv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", alignv.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &alignv, alignv.tmp);
                        gen_expr_free(&label);
                        gen_expr_free(&alignv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_node_set_disabled")) {
                        GenExpr node, onv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &onv, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_node_set_disabled(%s, %s);", node.tmp, onv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", onv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &onv, onv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&onv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_node_set_editable")) {
                        GenExpr node, onv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &onv, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_node_set_editable(%s, %s);", node.tmp, onv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", onv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &onv, onv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&onv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_node_get_editable")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_node_get_editable(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_node_set_id")) {
                        GenExpr node, idv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &idv, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_node_set_id(%s, %s);", node.tmp, idv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", idv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &idv, idv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&idv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_node_set_class")) {
                        GenExpr node, classv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &classv, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_node_set_class(%s, %s);", node.tmp, classv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", classv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &classv, classv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&classv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_node_set_a11y_label")) {
                        GenExpr node, labelv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &labelv, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_node_set_a11y_label(%s, %s);", node.tmp, labelv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", labelv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &labelv, labelv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&labelv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_node_set_a11y_role")) {
                        GenExpr node, rolev;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &rolev, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_node_set_a11y_role(%s, %s);", node.tmp, rolev.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", rolev.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &rolev, rolev.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&rolev);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_node_set_tooltip")) {
                        GenExpr node, textv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &textv, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_node_set_tooltip_val(%s, %s);", node.tmp, textv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", textv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &textv, textv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&textv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_app_set_appid")) {
                        GenExpr app, idv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &idv, err)) { gen_expr_free(&app); return false;
                        }
                        w_line(&cg->w,
                        "cogito_app_set_appid(%s, %s);", app.tmp, idv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", idv.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &idv, idv.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&idv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_app_set_app_name")) {
                        GenExpr app, namev;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &namev, err)) { gen_expr_free(&app); return false;
                        }
                        w_line(&cg->w,
                        "cogito_app_set_app_name(%s, %s);", app.tmp, namev.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", namev.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &namev, namev.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&namev);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_app_set_accent_color")) {
                        GenExpr app, colorv, ov;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &colorv, err)) { gen_expr_free(&app); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &ov, err)) { gen_expr_free(&app); gen_expr_free(&colorv); return false;
                        }
                        w_line(&cg->w,
                        "cogito_app_set_accent_color(%s, %s, %s);", app.tmp, colorv.tmp, ov.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", colorv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", ov.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &colorv, colorv.tmp);
                        gen_expr_release_except(cg, &ov, ov.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&colorv);
                        gen_expr_free(&ov);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_app_set_ensor_variant")) {
                        GenExpr app, variantv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &variantv, err)) { gen_expr_free(&app); return false;
                        }
                        w_line(&cg->w,
                        "cogito_app_set_ensor_variant(%s, %s);", app.tmp, variantv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", variantv.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &variantv, variantv.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&variantv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_pointer_capture")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        w_line(&cg->w,
                        "cogito_pointer_capture_set(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_pointer_release")) {
                        w_line(&cg->w,
                        "cogito_pointer_capture_clear();");
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_view_switcher_set_active")) {
                        GenExpr node, idv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &idv, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_view_switcher_set_active(%s, %s);", node.tmp, idv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", idv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &idv, idv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&idv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_view_switcher_add_lazy")) {
                        GenExpr node, idv, builderv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &idv, err)) { gen_expr_free(&node); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &builderv, err)) { gen_expr_free(&node); gen_expr_free(&idv); return false;
                        }
                        w_line(&cg->w,
                        "cogito_view_switcher_add(%s, %s, %s);", node.tmp, idv.tmp, builderv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", idv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", builderv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &idv, idv.tmp);
                        gen_expr_release_except(cg, &builderv, builderv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&idv);
                        gen_expr_free(&builderv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_textfield_set_text")) {
                        GenExpr node, text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &text, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_textfield_set_text(%s, %s);", node.tmp, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&text);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_textfield_get_text")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_textfield_get_text(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_searchfield_set_text")) {
                        GenExpr node, text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &text, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_searchfield_set_text(%s, %s);", node.tmp, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&text);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_searchfield_get_text")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_searchfield_get_text(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_searchfield_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &fn, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_searchfield_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_textfield_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &fn, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_textfield_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_textview_set_text")) {
                        GenExpr node, text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &text, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_textview_set_text(%s, %s);", node.tmp, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&text);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_textview_get_text")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_textview_get_text(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_textview_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &fn, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_textview_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_datepicker_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &fn, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_datepicker_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_dropdown_set_items")) {
                        GenExpr node, arr;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &arr, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_dropdown_set_items(%s, %s);", node.tmp, arr.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", arr.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &arr, arr.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&arr);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_dropdown_set_selected")) {
                        GenExpr node, idx;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &idx, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_dropdown_set_selected(%s, %s);", node.tmp, idx.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", idx.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &idx, idx.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&idx);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_dropdown_get_selected")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_dropdown_get_selected(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_dropdown_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &fn, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_dropdown_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_set_value")) {
                        GenExpr node, val;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &val, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_slider_set_value(%s, %s);", node.tmp, val.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", val.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &val, val.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&val);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_get_value")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_slider_get_value(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_set_size")) {
                        GenExpr node, sizev;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &sizev, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_slider_set_size(%s, %s);", node.tmp, sizev.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", sizev.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &sizev, sizev.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&sizev);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_get_size")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_slider_get_size(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_set_icon")) {
                        GenExpr node, iconv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &iconv, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_slider_set_icon(%s, %s);", node.tmp, iconv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", iconv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &iconv, iconv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&iconv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_set_centered")) {
                        GenExpr node, onv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &onv, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_slider_set_centered(%s, %s);", node.tmp, onv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", onv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &onv, onv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&onv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_get_centered")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_slider_get_centered(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_set_range")) {
                        GenExpr node, startv, endv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &startv, err)) { gen_expr_free(&node); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &endv, err)) { gen_expr_free(&node); gen_expr_free(&startv); return false;
                        }
                        w_line(&cg->w,
                        "cogito_slider_set_range(%s, %s, %s);", node.tmp, startv.tmp, endv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", startv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", endv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &startv, startv.tmp);
                        gen_expr_release_except(cg, &endv, endv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&startv);
                        gen_expr_free(&endv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_set_range_start")) {
                        GenExpr node, startv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &startv, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_slider_set_range_start(%s, %s);", node.tmp, startv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", startv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &startv, startv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&startv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_set_range_end")) {
                        GenExpr node, endv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &endv, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_slider_set_range_end(%s, %s);", node.tmp, endv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", endv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &endv, endv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&endv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_get_range_start")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_slider_get_range_start(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_get_range_end")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_slider_get_range_end(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_slider_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &fn, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_slider_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_colorpicker_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &fn, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_colorpicker_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_tabs_set_items")) {
                        GenExpr node, arr;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &arr, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_tabs_set_items(%s, %s);", node.tmp, arr.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", arr.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &arr, arr.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&arr);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_tabs_set_ids")) {
                        GenExpr node, arr;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &arr, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_tabs_set_ids(%s, %s);", node.tmp, arr.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", arr.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &arr, arr.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&arr);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_tabs_set_selected")) {
                        GenExpr node, idx;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &idx, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_tabs_set_selected(%s, %s);", node.tmp, idx.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", idx.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &idx, idx.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&idx);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_tabs_get_selected")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_tabs_get_selected(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_tabs_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &fn, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_tabs_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_tabs_bind")) {
                        GenExpr tabs, vs;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &tabs, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &vs, err)) { gen_expr_free(&tabs); return false;
                        }
                        w_line(&cg->w,
                        "cogito_tabs_bind(%s, %s);", tabs.tmp, vs.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", tabs.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", vs.tmp);
                        gen_expr_release_except(cg, &tabs, tabs.tmp);
                        gen_expr_release_except(cg, &vs, vs.tmp);
                        gen_expr_free(&tabs);
                        gen_expr_free(&vs);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_progress_set_value")) {
                        GenExpr node, val;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &val, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_progress_set_value(%s, %s);", node.tmp, val.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", val.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &val, val.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&val);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_progress_get_value")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_progress_get_value(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_progress_set_indeterminate")) {
                        GenExpr node, on;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &on, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_progress_set_indeterminate(%s, %s);", node.tmp, on.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", on.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &on, on.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&on);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_progress_get_indeterminate")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_progress_get_indeterminate(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_progress_set_thickness")) {
                        GenExpr node, px;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &px, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_progress_set_thickness(%s, %s);", node.tmp, px.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", px.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &px, px.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&px);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_progress_get_thickness")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_progress_get_thickness(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_progress_set_wavy")) {
                        GenExpr node, on;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &on, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_progress_set_wavy(%s, %s);", node.tmp, on.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", on.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &on, on.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&on);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_progress_get_wavy")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_progress_get_wavy(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_progress_set_circular")) {
                        GenExpr node, on;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &on, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_progress_set_circular(%s, %s);", node.tmp, on.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", on.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &on, on.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&on);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_progress_get_circular")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_progress_get_circular(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_toast_set_text")) {
                        GenExpr node, text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &text, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_toast_set_text(%s, %s);", node.tmp, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&text);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_toast_on_click")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &fn, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_toast_on_click(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_window_set_autosize")) {
                        GenExpr win, on;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &win, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &on, err)) { gen_expr_free(&win); return false;
                        }
                        w_line(&cg->w,
                        "cogito_window_set_autosize(%s, %s);", win.tmp, on.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", win.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", on.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_release_except(cg, &on, on.tmp);
                        gen_expr_free(&win);
                        gen_expr_free(&on);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_window_set_resizable")) {
                        GenExpr win, on;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &win, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &on, err)) { gen_expr_free(&win); return false;
                        }
                        w_line(&cg->w,
                        "cogito_window_set_resizable(%s, %s);", win.tmp, on.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", win.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", on.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_release_except(cg, &on, on.tmp);
                        gen_expr_free(&win);
                        gen_expr_free(&on);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_window_set_dialog")) {
                        GenExpr win, dialog;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &win, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &dialog, err)) { gen_expr_free(&win); return false;
                        }
                        w_line(&cg->w,
                        "cogito_window_set_dialog(%s, %s);", win.tmp, dialog.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", win.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", dialog.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_release_except(cg, &dialog, dialog.tmp);
                        gen_expr_free(&win);
                        gen_expr_free(&dialog);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_window_clear_dialog")) {
                        GenExpr win;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &win, err)) return false;
                        w_line(&cg->w,
                        "cogito_window_clear_dialog(%s);", win.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", win.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_free(&win);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_window_set_side_sheet")) {
                        GenExpr win, side_sheet;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &win, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &side_sheet, err)) { gen_expr_free(&win); return false;
                        }
                        w_line(&cg->w,
                        "cogito_window_set_side_sheet(%s, %s);", win.tmp, side_sheet.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", win.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", side_sheet.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_release_except(cg, &side_sheet, side_sheet.tmp);
                        gen_expr_free(&win);
                        gen_expr_free(&side_sheet);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_window_clear_side_sheet")) {
                        GenExpr win;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &win, err)) return false;
                        w_line(&cg->w,
                        "cogito_window_clear_side_sheet(%s);", win.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", win.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_free(&win);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_node_window")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_node_window_val(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_build")) {
                        GenExpr node, builder;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &builder, err)) { gen_expr_free(&node); return false;
                        }
                        w_line(&cg->w,
                        "cogito_build(%s, %s);", node.tmp, builder.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", builder.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &builder, builder.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&builder);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_window_set_builder")) {
                        GenExpr win, builder;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &win, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &builder, err)) { gen_expr_free(&win); return false;
                        }
                        w_line(&cg->w,
                        "cogito_window_set_builder(%s, %s);", win.tmp, builder.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", win.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", builder.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_release_except(cg, &builder, builder.tmp);
                        gen_expr_free(&win);
                        gen_expr_free(&builder);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_state_new")) {
                        GenExpr val;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &val, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_state_new(%s);", t, val.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", val.tmp);
                        gen_expr_release_except(cg, &val, val.tmp);
                        gen_expr_free(&val);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_state_get")) {
                        GenExpr st;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &st, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_state_get(%s);", t, st.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", st.tmp);
                        gen_expr_release_except(cg, &st, st.tmp);
                        gen_expr_free(&st);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_state_set")) {
                        GenExpr st, val;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &st, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &val, err)) { gen_expr_free(&st); return false;
                        }
                        w_line(&cg->w,
                        "cogito_state_set(%s, %s);", st.tmp, val.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", st.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", val.tmp);
                        gen_expr_release_except(cg, &st, st.tmp);
                        gen_expr_release_except(cg, &val, val.tmp);
                        gen_expr_free(&st);
                        gen_expr_free(&val);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_button_set_text")) {
                        GenExpr btn, text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &text, err)) { gen_expr_free(&btn); return false;
                        }
                        w_line(&cg->w,
                        "cogito_button_set_text(%s, %s);", btn.tmp, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&text);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_image_set_icon")) {
                        GenExpr img, icon;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &img, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &icon, err)) { gen_expr_free(&img); return false;
                        }
                        w_line(&cg->w,
                        "cogito_image_set_icon(%s, %s);", img.tmp, icon.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", img.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &img, img.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_free(&img);
                        gen_expr_free(&icon);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_image_set_size")) {
                        GenExpr img, w, h;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &img, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &w, err)) { gen_expr_free(&img); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &h, err)) { gen_expr_free(&img); gen_expr_free(&w); return false;
                        }
                        w_line(&cg->w,
                        "cogito_image_set_size(%s, %s, %s);", img.tmp, w.tmp, h.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", img.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", w.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", h.tmp);
                        gen_expr_release_except(cg, &img, img.tmp);
                        gen_expr_release_except(cg, &w, w.tmp);
                        gen_expr_release_except(cg, &h, h.tmp);
                        gen_expr_free(&img);
                        gen_expr_free(&w);
                        gen_expr_free(&h);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_image_set_radius")) {
                        GenExpr img, r;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &img, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &r, err)) { gen_expr_free(&img); return false;
                        }
                        w_line(&cg->w,
                        "cogito_image_set_radius(%s, %s);", img.tmp, r.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", img.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", r.tmp);
                        gen_expr_release_except(cg, &img, img.tmp);
                        gen_expr_release_except(cg, &r, r.tmp);
                        gen_expr_free(&img);
                        gen_expr_free(&r);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_image_set_alt_text")) {
                        GenExpr img, alt;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &img, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &alt, err)) { gen_expr_free(&img); return false;
                        }
                        w_line(&cg->w,
                        "cogito_image_set_alt_text(%s, %s);", img.tmp, alt.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", img.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", alt.tmp);
                        gen_expr_release_except(cg, &img, img.tmp);
                        gen_expr_release_except(cg, &alt, alt.tmp);
                        gen_expr_free(&img);
                        gen_expr_free(&alt);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_checkbox_set_checked")) {
                        GenExpr cb, checked;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &cb, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &checked, err)) { gen_expr_free(&cb); return false;
                        }
                        w_line(&cg->w,
                        "cogito_checkbox_set_checked(%s, %s);", cb.tmp, checked.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", cb.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", checked.tmp);
                        gen_expr_release_except(cg, &cb, cb.tmp);
                        gen_expr_release_except(cg, &checked, checked.tmp);
                        gen_expr_free(&cb);
                        gen_expr_free(&checked);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_checkbox_get_checked")) {
                        GenExpr cb;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &cb, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_checkbox_get_checked(%s);", t, cb.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", cb.tmp);
                        gen_expr_release_except(cg, &cb, cb.tmp);
                        gen_expr_free(&cb);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_switch_set_checked")) {
                        GenExpr sw, checked;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &sw, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &checked, err)) { gen_expr_free(&sw); return false;
                        }
                        w_line(&cg->w,
                        "cogito_switch_set_checked(%s, %s);", sw.tmp, checked.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", sw.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", checked.tmp);
                        gen_expr_release_except(cg, &sw, sw.tmp);
                        gen_expr_release_except(cg, &checked, checked.tmp);
                        gen_expr_free(&sw);
                        gen_expr_free(&checked);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_switch_get_checked")) {
                        GenExpr sw;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &sw, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_switch_get_checked(%s);", t, sw.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", sw.tmp);
                        gen_expr_release_except(cg, &sw, sw.tmp);
                        gen_expr_free(&sw);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_checkbox_on_change")) {
                        GenExpr cb, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &cb, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&cb); return false;
                        }
                        w_line(&cg->w,
                        "cogito_checkbox_on_change(%s, %s);", cb.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", cb.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &cb, cb.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&cb);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_switch_on_change")) {
                        GenExpr sw, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &sw, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&sw); return false;
                        }
                        w_line(&cg->w,
                        "cogito_switch_on_change(%s, %s);", sw.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", sw.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &sw, sw.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&sw);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_buttongroup_on_select")) {
                        GenExpr button_group, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &button_group, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&button_group); return false;
                        }
                        w_line(&cg->w,
                        "cogito_buttongroup_on_select(%s, %s);", button_group.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", button_group.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &button_group, button_group.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&button_group);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_list_on_select")) {
                        GenExpr list, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &list, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&list); return false;
                        }
                        w_line(&cg->w,
                        "cogito_list_on_select(%s, %s);", list.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", list.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &list, list.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&list);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_list_on_activate")) {
                        GenExpr list, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &list, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&list); return false;
                        }
                        w_line(&cg->w,
                        "cogito_list_on_activate(%s, %s);", list.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", list.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &list, list.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&list);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_grid_on_select")) {
                        GenExpr grid, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &grid, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&grid); return false;
                        }
                        w_line(&cg->w,
                        "cogito_grid_on_select(%s, %s);", grid.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", grid.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &grid, grid.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&grid);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_grid_on_activate")) {
                        GenExpr grid, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &grid, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&grid); return false;
                        }
                        w_line(&cg->w,
                        "cogito_grid_on_activate(%s, %s);", grid.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", grid.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &grid, grid.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&grid);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_button_on_click")) {
                        GenExpr btn, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&btn); return false;
                        }
                        w_line(&cg->w,
                        "cogito_button_on_click(%s, %s);", btn.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_button_add_menu")) {
                        GenExpr btn, label, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &label, err)) { gen_expr_free(&btn); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &handler, err)) { gen_expr_free(&btn); gen_expr_free(&label); return false;
                        }
                        w_line(&cg->w,
                        "cogito_button_add_menu(%s, %s, %s);", btn.tmp, label.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&label);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_set_menu_divider")) {
                        GenExpr btn, divider;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &divider, err)) { gen_expr_free(&btn); return false;
                        }
                        w_line(&cg->w,
                        "cogito_iconbtn_set_menu_divider(%s, %s);", btn.tmp, divider.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", divider.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &divider, divider.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&divider);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_get_menu_divider")) {
                        GenExpr btn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_BOOL(cogito_iconbtn_get_menu_divider(%s));", t, btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_free(&btn);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_set_menu_item_gap")) {
                        GenExpr btn, gap;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &gap, err)) { gen_expr_free(&btn); return false;
                        }
                        w_line(&cg->w,
                        "cogito_iconbtn_set_menu_item_gap(%s, %s);", btn.tmp, gap.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", gap.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &gap, gap.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&gap);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_get_menu_item_gap")) {
                        GenExpr btn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_INT(cogito_iconbtn_get_menu_item_gap(%s));", t, btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_free(&btn);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_button_set_menu_divider")) {
                        GenExpr btn, divider;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &divider, err)) { gen_expr_free(&btn); return false;
                        }
                        w_line(&cg->w,
                        "cogito_button_set_menu_divider(%s, %s);", btn.tmp, divider.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", divider.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &divider, divider.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&divider);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_button_get_menu_divider")) {
                        GenExpr btn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_BOOL(cogito_button_get_menu_divider(%s));", t, btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_free(&btn);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_button_set_menu_item_gap")) {
                        GenExpr btn, gap;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &gap, err)) { gen_expr_free(&btn); return false;
                        }
                        w_line(&cg->w,
                        "cogito_button_set_menu_item_gap(%s, %s);", btn.tmp, gap.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", gap.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &gap, gap.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&gap);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_button_get_menu_item_gap")) {
                        GenExpr btn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_INT(cogito_button_get_menu_item_gap(%s));", t, btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_free(&btn);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_set_menu_vibrant")) {
                        GenExpr btn, vibrant;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &vibrant, err)) { gen_expr_free(&btn); return false;
                        }
                        w_line(&cg->w,
                        "cogito_iconbtn_set_menu_vibrant(%s, %s);", btn.tmp, vibrant.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", vibrant.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &vibrant, vibrant.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&vibrant);
                        out->tmp = NULL;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_get_menu_vibrant")) {
                        GenExpr btn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_BOOL(cogito_iconbtn_get_menu_vibrant(%s));", t, btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_free(&btn);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_button_set_menu_vibrant")) {
                        GenExpr btn, vibrant;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &vibrant, err)) { gen_expr_free(&btn); return false;
                        }
                        w_line(&cg->w,
                        "cogito_button_set_menu_vibrant(%s, %s);", btn.tmp, vibrant.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", vibrant.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &vibrant, vibrant.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&vibrant);
                        out->tmp = NULL;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_button_get_menu_vibrant")) {
                        GenExpr btn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_BOOL(cogito_button_get_menu_vibrant(%s));", t, btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_free(&btn);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_menu_set_icon")) {
                        GenExpr node, icon;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &icon, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_menu_set_icon(%s, %s);", node.tmp, icon.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&icon);
                        out->tmp = NULL;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_menu_set_shortcut")) {
                        GenExpr node, shortcut;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &shortcut, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_menu_set_shortcut(%s, %s);", node.tmp, shortcut.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", shortcut.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &shortcut, shortcut.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&shortcut);
                        out->tmp = NULL;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_menu_set_submenu")) {
                        GenExpr node, submenu;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &submenu, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_menu_set_submenu(%s, %s);", node.tmp, submenu.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", submenu.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &submenu, submenu.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&submenu);
                        out->tmp = NULL;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_menu_set_toggled")) {
                        GenExpr node, toggled;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &toggled, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_menu_set_toggled(%s, %s);", node.tmp, toggled.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", toggled.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &toggled, toggled.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&toggled);
                        out->tmp = NULL;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_appbar_add_button")) {
                        GenExpr app, text, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &text, err)) { gen_expr_free(&app); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &handler, err)) { gen_expr_free(&app); gen_expr_free(&text); return false;
                        }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_appbar_add_button(%s, %s, %s);", t, app.tmp, text.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&text);
                        gen_expr_free(&handler);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_appbar_set_controls")) {
                        GenExpr app, layout;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &layout, err)) { gen_expr_free(&app); return false;
                        }
                        w_line(&cg->w,
                        "cogito_appbar_set_controls(%s, %s);", app.tmp, layout.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", layout.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &layout, layout.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&layout);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_appbar_set_title")) {
                        GenExpr app, title;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &title, err)) { gen_expr_free(&app); return false;
                        }
                        w_line(&cg->w,
                        "cogito_appbar_set_title(%s, %s);", app.tmp, title.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", title.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&title);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_appbar_set_subtitle")) {
                        GenExpr app, subtitle;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &subtitle, err)) { gen_expr_free(&app); return false;
                        }
                        w_line(&cg->w,
                        "cogito_appbar_set_subtitle(%s, %s);", app.tmp, subtitle.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", subtitle.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &subtitle, subtitle.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&subtitle);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_add_menu")) {
                        GenExpr btn, label, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &label, err)) { gen_expr_free(&btn); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &handler, err)) { gen_expr_free(&btn); gen_expr_free(&label); return false;
                        }
                        w_line(&cg->w,
                        "cogito_iconbtn_add_menu(%s, %s, %s);", btn.tmp, label.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&label);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_set_shape")) {
                        GenExpr btn, shape;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &shape, err)) { gen_expr_free(&btn); return false;
                        }
                        w_line(&cg->w,
                        "cogito_iconbtn_set_shape(%s, %s);", btn.tmp, shape.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", shape.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &shape, shape.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&shape);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_get_shape")) {
                        GenExpr btn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_iconbtn_get_shape(%s);", t, btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_free(&btn);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_set_color_style")) {
                        GenExpr btn, style;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &style, err)) { gen_expr_free(&btn); return false;
                        }
                        w_line(&cg->w,
                        "cogito_iconbtn_set_color_style(%s, %s);", btn.tmp, style.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", style.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &style, style.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&style);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_get_color_style")) {
                        GenExpr btn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_iconbtn_get_color_style(%s);", t, btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_free(&btn);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_set_width")) {
                        GenExpr btn, width;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &width, err)) { gen_expr_free(&btn); return false;
                        }
                        w_line(&cg->w,
                        "cogito_iconbtn_set_width(%s, %s);", btn.tmp, width.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", width.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &width, width.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&width);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_get_width")) {
                        GenExpr btn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_iconbtn_get_width(%s);", t, btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_free(&btn);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_set_toggle")) {
                        GenExpr btn, toggle;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &toggle, err)) { gen_expr_free(&btn); return false;
                        }
                        w_line(&cg->w,
                        "cogito_iconbtn_set_toggle(%s, %s);", btn.tmp, toggle.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", toggle.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &toggle, toggle.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&toggle);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_get_toggle")) {
                        GenExpr btn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_iconbtn_get_toggle(%s);", t, btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_free(&btn);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_set_checked")) {
                        GenExpr btn, checked;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &checked, err)) { gen_expr_free(&btn); return false;
                        }
                        w_line(&cg->w,
                        "cogito_iconbtn_set_checked(%s, %s);", btn.tmp, checked.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", checked.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &checked, checked.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&checked);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_iconbtn_get_checked")) {
                        GenExpr btn;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &btn, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_iconbtn_get_checked(%s);", t, btn.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", btn.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_free(&btn);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_run")) {
                        GenExpr app, win;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &win, err)) { gen_expr_free(&app); return false;
                        }
                        w_line(&cg->w,
                        "cogito_run(%s, %s);", app.tmp, win.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", win.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&win);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_load_sum")) {
                        Str lit_path = {
                            0
                        };
                        if (codegen_try_text_literal(e->as.call.args[
                            0
                        ], &lit_path) && lit_path.len > 0) {
                            size_t sum_len = 0;
                            char *sum_src = codegen_read_literal_file(cg->arena, path, lit_path, &sum_len);
                            if (sum_src) {
                                char *esc = c_escape(cg->arena, (Str){sum_src, sum_len
                                });
                                if (!esc) return cg_set_err(err, path,
                                "out of memory");
                                w_line(&cg->w,
                                "cogito_load_sum_inline(\"%s\");", esc);
                                char *t = codegen_new_tmp(cg);
                                w_line(&cg->w,
                                "ErgoVal %s = EV_NULLV;", t);
                                gen_expr_add(out, t);
                                out->tmp = t;
                                return true;
                            }
                        }
                        GenExpr pathv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &pathv, err)) return false;
                        w_line(&cg->w,
                        "cogito_load_sum(%s);", pathv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", pathv.tmp);
                        gen_expr_release_except(cg, &pathv, pathv.tmp);
                        gen_expr_free(&pathv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_open_url")) {
                        GenExpr urlv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &urlv, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_open_url(%s);", t, urlv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", urlv.tmp);
                        gen_expr_release_except(cg, &urlv, urlv.tmp);
                        gen_expr_free(&urlv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // Chip
                    if (str_eq_c(fname,
                    "__cogito_chip")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_chip_new(%s);", t, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_chip_set_selected")) {
                        GenExpr chip, sel;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &chip, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &sel, err)) { gen_expr_free(&chip); return false;
                        }
                        w_line(&cg->w,
                        "cogito_chip_set_selected(%s, %s);", chip.tmp, sel.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", chip.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", sel.tmp);
                        gen_expr_release_except(cg, &chip, chip.tmp);
                        gen_expr_release_except(cg, &sel, sel.tmp);
                        gen_expr_free(&chip);
                        gen_expr_free(&sel);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_chip_get_selected")) {
                        GenExpr chip;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &chip, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_chip_get_selected(%s);", t, chip.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", chip.tmp);
                        gen_expr_release_except(cg, &chip, chip.tmp);
                        gen_expr_free(&chip);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_chip_set_closable")) {
                        GenExpr chip, closable;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &chip, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &closable, err)) { gen_expr_free(&chip); return false;
                        }
                        w_line(&cg->w,
                        "cogito_chip_set_closable(%s, %s);", chip.tmp, closable.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", chip.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", closable.tmp);
                        gen_expr_release_except(cg, &chip, chip.tmp);
                        gen_expr_release_except(cg, &closable, closable.tmp);
                        gen_expr_free(&chip);
                        gen_expr_free(&closable);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_chip_on_click")) {
                        GenExpr chip, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &chip, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&chip); return false;
                        }
                        w_line(&cg->w,
                        "cogito_chip_on_click(%s, %s);", chip.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", chip.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &chip, chip.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&chip);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_chip_on_close")) {
                        GenExpr chip, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &chip, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&chip); return false;
                        }
                        w_line(&cg->w,
                        "cogito_chip_on_close(%s, %s);", chip.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", chip.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &chip, chip.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&chip);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // FAB
                    if (str_eq_c(fname,
                    "__cogito_fab")) {
                        GenExpr icon;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &icon, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_fab_new(%s);", t, icon.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_free(&icon);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_fab_set_size")) {
                        GenExpr fab, sizev;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &fab, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &sizev, err)) { gen_expr_free(&fab); return false;
                        }
                        w_line(&cg->w,
                        "cogito_fab_set_size(%s, %s);", fab.tmp, sizev.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fab.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", sizev.tmp);
                        gen_expr_release_except(cg, &fab, fab.tmp);
                        gen_expr_release_except(cg, &sizev, sizev.tmp);
                        gen_expr_free(&fab);
                        gen_expr_free(&sizev);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_fab_set_color")) {
                        GenExpr fab, colorv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &fab, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &colorv, err)) { gen_expr_free(&fab); return false;
                        }
                        w_line(&cg->w,
                        "cogito_fab_set_color(%s, %s);", fab.tmp, colorv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fab.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", colorv.tmp);
                        gen_expr_release_except(cg, &fab, fab.tmp);
                        gen_expr_release_except(cg, &colorv, colorv.tmp);
                        gen_expr_free(&fab);
                        gen_expr_free(&colorv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_fab_set_extended")) {
                        GenExpr fab, ext, label;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &fab, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &ext, err)) { gen_expr_free(&fab); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &label, err)) { gen_expr_free(&fab); gen_expr_free(&ext); return false;
                        }
                        w_line(&cg->w,
                        "cogito_fab_set_extended(%s, %s, %s);", fab.tmp, ext.tmp, label.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fab.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", ext.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", label.tmp);
                        gen_expr_release_except(cg, &fab, fab.tmp);
                        gen_expr_release_except(cg, &ext, ext.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_free(&fab);
                        gen_expr_free(&ext);
                        gen_expr_free(&label);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_fab_on_click")) {
                        GenExpr fab, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &fab, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&fab); return false;
                        }
                        w_line(&cg->w,
                        "cogito_fab_on_click(%s, %s);", fab.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fab.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &fab, fab.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&fab);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_fab_menu")) {
                        GenExpr icon;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &icon, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_fab_menu_new(%s);", t, icon.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_fab_menu_add_item")) {
                        GenExpr fab, label, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &fab, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &label, err)) { gen_expr_free(&fab); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &handler, err)) { gen_expr_free(&fab); gen_expr_free(&label); return false;
                        }
                        w_line(&cg->w,
                        "cogito_fab_menu_add_item(%s, %s, %s);", fab.tmp, label.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fab.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &fab, fab.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&fab);
                        gen_expr_free(&label);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_fab_menu_set_color")) {
                        GenExpr fab, colorv;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &fab, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &colorv, err)) { gen_expr_free(&fab); return false;
                        }
                        w_line(&cg->w,
                        "cogito_fab_menu_set_color(%s, %s);", fab.tmp, colorv.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", fab.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", colorv.tmp);
                        gen_expr_release_except(cg, &fab, fab.tmp);
                        gen_expr_release_except(cg, &colorv, colorv.tmp);
                        gen_expr_free(&fab);
                        gen_expr_free(&colorv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // NavRail
                    if (str_eq_c(fname,
                    "__cogito_nav_rail")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_nav_rail_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_nav_rail_set_items")) {
                        GenExpr rail, labels, icons;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &rail, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &labels, err)) { gen_expr_free(&rail); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &icons, err)) { gen_expr_free(&rail); gen_expr_free(&labels); return false;
                        }
                        w_line(&cg->w,
                        "cogito_nav_rail_set_items(%s, %s, %s);", rail.tmp, labels.tmp, icons.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", rail.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", labels.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", icons.tmp);
                        gen_expr_release_except(cg, &rail, rail.tmp);
                        gen_expr_release_except(cg, &labels, labels.tmp);
                        gen_expr_release_except(cg, &icons, icons.tmp);
                        gen_expr_free(&rail);
                        gen_expr_free(&labels);
                        gen_expr_free(&icons);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_nav_rail_set_badges")) {
                        GenExpr rail, badges;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &rail, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &badges, err)) { gen_expr_free(&rail); return false;
                        }
                        w_line(&cg->w,
                        "cogito_nav_rail_set_badges(%s, %s);", rail.tmp, badges.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", rail.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", badges.tmp);
                        gen_expr_release_except(cg, &rail, rail.tmp);
                        gen_expr_release_except(cg, &badges, badges.tmp);
                        gen_expr_free(&rail);
                        gen_expr_free(&badges);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_nav_rail_set_toggle")) {
                        GenExpr rail, on;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &rail, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &on, err)) { gen_expr_free(&rail); return false;
                        }
                        w_line(&cg->w,
                        "cogito_nav_rail_set_toggle(%s, %s);", rail.tmp, on.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", rail.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", on.tmp);
                        gen_expr_release_except(cg, &rail, rail.tmp);
                        gen_expr_release_except(cg, &on, on.tmp);
                        gen_expr_free(&rail);
                        gen_expr_free(&on);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_nav_rail_get_selected")) {
                        GenExpr rail;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &rail, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_nav_rail_get_selected(%s);", t, rail.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", rail.tmp);
                        gen_expr_release_except(cg, &rail, rail.tmp);
                        gen_expr_free(&rail);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_nav_rail_set_selected")) {
                        GenExpr rail, idx;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &rail, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &idx, err)) { gen_expr_free(&rail); return false;
                        }
                        w_line(&cg->w,
                        "cogito_nav_rail_set_selected(%s, %s);", rail.tmp, idx.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", rail.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", idx.tmp);
                        gen_expr_release_except(cg, &rail, rail.tmp);
                        gen_expr_release_except(cg, &idx, idx.tmp);
                        gen_expr_free(&rail);
                        gen_expr_free(&idx);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_nav_rail_on_change")) {
                        GenExpr rail, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &rail, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&rail); return false;
                        }
                        w_line(&cg->w,
                        "cogito_nav_rail_on_change(%s, %s);", rail.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", rail.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &rail, rail.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&rail);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_nav_rail_set_no_label")) {
                        GenExpr rail, on;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &rail, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &on, err)) { gen_expr_free(&rail); return false;
                        }
                        w_line(&cg->w,
                        "cogito_nav_rail_set_no_label(%s, %s);", rail.tmp, on.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", rail.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", on.tmp);
                        gen_expr_release_except(cg, &rail, rail.tmp);
                        gen_expr_release_except(cg, &on, on.tmp);
                        gen_expr_free(&rail);
                        gen_expr_free(&on);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_nav_rail_get_no_label")) {
                        GenExpr rail;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &rail, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_BOOL(cogito_nav_rail_get_no_label(%s));", t, rail.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", rail.tmp);
                        gen_expr_release_except(cg, &rail, rail.tmp);
                        gen_expr_free(&rail);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // BottomNav
                    if (str_eq_c(fname,
                    "__cogito_bottom_nav")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_bottom_nav_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_bottom_nav_set_items")) {
                        GenExpr nav, labels, icons;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &nav, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &labels, err)) { gen_expr_free(&nav); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &icons, err)) { gen_expr_free(&nav); gen_expr_free(&labels); return false;
                        }
                        w_line(&cg->w,
                        "cogito_bottom_nav_set_items(%s, %s, %s);", nav.tmp, labels.tmp, icons.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", nav.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", labels.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", icons.tmp);
                        gen_expr_release_except(cg, &nav, nav.tmp);
                        gen_expr_release_except(cg, &labels, labels.tmp);
                        gen_expr_release_except(cg, &icons, icons.tmp);
                        gen_expr_free(&nav);
                        gen_expr_free(&labels);
                        gen_expr_free(&icons);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_bottom_nav_get_selected")) {
                        GenExpr nav;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &nav, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_bottom_nav_get_selected(%s);", t, nav.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", nav.tmp);
                        gen_expr_release_except(cg, &nav, nav.tmp);
                        gen_expr_free(&nav);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_bottom_nav_set_selected")) {
                        GenExpr nav, idx;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &nav, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &idx, err)) { gen_expr_free(&nav); return false;
                        }
                        w_line(&cg->w,
                        "cogito_bottom_nav_set_selected(%s, %s);", nav.tmp, idx.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", nav.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", idx.tmp);
                        gen_expr_release_except(cg, &nav, nav.tmp);
                        gen_expr_release_except(cg, &idx, idx.tmp);
                        gen_expr_free(&nav);
                        gen_expr_free(&idx);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_bottom_nav_on_change")) {
                        GenExpr nav, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &nav, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&nav); return false;
                        }
                        w_line(&cg->w,
                        "cogito_bottom_nav_on_change(%s, %s);", nav.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", nav.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &nav, nav.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&nav);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // Node tree traversal
                    if (str_eq_c(fname,
                    "__cogito_find_parent")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_find_parent(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_find_children")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_find_children(%s);", t, node.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // Divider
                    if (str_eq_c(fname,
                    "__cogito_divider")) {
                        GenExpr orient, inset;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &orient, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &inset, err)) { gen_expr_free(&orient); return false;
                        }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_divider_new(%s, %s);", t, orient.tmp, inset.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", orient.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", inset.tmp);
                        gen_expr_release_except(cg, &orient, orient.tmp);
                        gen_expr_release_except(cg, &inset, inset.tmp);
                        gen_expr_free(&orient);
                        gen_expr_free(&inset);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // Card
                    if (str_eq_c(fname,
                    "__cogito_card")) {
                        GenExpr title;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &title, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_card_new(%s);", t, title.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", title.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_free(&title);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // Avatar
                    if (str_eq_c(fname,
                    "__cogito_avatar")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_avatar_new(%s);", t, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_avatar_set_image")) {
                        GenExpr avatar, path_arg;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &avatar, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &path_arg, err)) { gen_expr_free(&avatar); return false;
                        }
                        w_line(&cg->w,
                        "cogito_avatar_set_image(%s, %s);", avatar.tmp, path_arg.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", avatar.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", path_arg.tmp);
                        gen_expr_release_except(cg, &avatar, avatar.tmp);
                        gen_expr_release_except(cg, &path_arg, path_arg.tmp);
                        gen_expr_free(&avatar);
                        gen_expr_free(&path_arg);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // Badge
                    if (str_eq_c(fname,
                    "__cogito_badge")) {
                        GenExpr count;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &count, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_badge_new(%s);", t, count.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", count.tmp);
                        gen_expr_release_except(cg, &count, count.tmp);
                        gen_expr_free(&count);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_badge_set_count")) {
                        GenExpr badge, count;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &badge, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &count, err)) { gen_expr_free(&badge); return false;
                        }
                        w_line(&cg->w,
                        "cogito_badge_set_count(%s, %s);", badge.tmp, count.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", badge.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", count.tmp);
                        gen_expr_release_except(cg, &badge, badge.tmp);
                        gen_expr_release_except(cg, &count, count.tmp);
                        gen_expr_free(&badge);
                        gen_expr_free(&count);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_badge_get_count")) {
                        GenExpr badge;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &badge, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_badge_get_count(%s);", t, badge.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", badge.tmp);
                        gen_expr_release_except(cg, &badge, badge.tmp);
                        gen_expr_free(&badge);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // Banner
                    if (str_eq_c(fname,
                    "__cogito_banner")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_banner_new(%s);", t, text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_banner_set_action")) {
                        GenExpr banner, text, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &banner, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &text, err)) { gen_expr_free(&banner); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &handler, err)) { gen_expr_free(&banner); gen_expr_free(&text); return false;
                        }
                        w_line(&cg->w,
                        "cogito_banner_set_action(%s, %s, %s);", banner.tmp, text.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", banner.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", text.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &banner, banner.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&banner);
                        gen_expr_free(&text);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_banner_set_icon")) {
                        GenExpr banner, icon;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &banner, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &icon, err)) { gen_expr_free(&banner); return false;
                        }
                        w_line(&cg->w,
                        "cogito_banner_set_icon(%s, %s);", banner.tmp, icon.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", banner.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &banner, banner.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_free(&banner);
                        gen_expr_free(&icon);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // BottomSheet
                    if (str_eq_c(fname,
                    "__cogito_bottom_sheet")) {
                        GenExpr title;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &title, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_bottom_sheet_new(%s);", t, title.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", title.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_free(&title);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // SideSheet
                    if (str_eq_c(fname,
                    "__cogito_side_sheet")) {
                        GenExpr title;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &title, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_side_sheet_new(%s);", t, title.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", title.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_free(&title);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // TimePicker
                    if (str_eq_c(fname,
                    "__cogito_timepicker")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_timepicker_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_timepicker_on_change")) {
                        GenExpr tp, handler;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &tp, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &handler, err)) { gen_expr_free(&tp); return false;
                        }
                        w_line(&cg->w,
                        "cogito_timepicker_on_change(%s, %s);", tp.tmp, handler.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", tp.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &tp, tp.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&tp);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_timepicker_get_hour")) {
                        GenExpr tp;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &tp, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_timepicker_get_hour(%s);", t, tp.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", tp.tmp);
                        gen_expr_release_except(cg, &tp, tp.tmp);
                        gen_expr_free(&tp);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_timepicker_get_minute")) {
                        GenExpr tp;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &tp, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = cogito_timepicker_get_minute(%s);", t, tp.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", tp.tmp);
                        gen_expr_release_except(cg, &tp, tp.tmp);
                        gen_expr_free(&tp);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname,
                    "__cogito_timepicker_set_time")) {
                        GenExpr tp, hour, min;
                        if (!gen_expr(cg, path, e->as.call.args[
                            0
                        ], &tp, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[
                            1
                        ], &hour, err)) { gen_expr_free(&tp); return false;
                        }
                        if (!gen_expr(cg, path, e->as.call.args[
                            2
                        ], &min, err)) { gen_expr_free(&tp); gen_expr_free(&hour); return false;
                        }
                        w_line(&cg->w,
                        "cogito_timepicker_set_time(%s, %s, %s);", tp.tmp, hour.tmp, min.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", tp.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", hour.tmp);
                        w_line(&cg->w,
                        "ergo_release_val(%s);", min.tmp);
                        gen_expr_release_except(cg, &tp, tp.tmp);
                        gen_expr_release_except(cg, &hour, hour.tmp);
                        gen_expr_release_except(cg, &min, min.tmp);
                        gen_expr_free(&tp);
                        gen_expr_free(&hour);
                        gen_expr_free(&min);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w,
                        "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    // --- New Widgets Codegen ---
                    #include "cogito_codegen_new_widgets.inc"
