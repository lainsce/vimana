        if (str_eq_c(imp->name, "cogito")) {
            // Try multiple locations for cogito.ergo
            // 1) cwd-relative paths
            const char *cogito_paths[] = {
                "cogito/cogito.ergo",
                "cogito/_build/cogito.ergo",
                "cogito/build/cogito.ergo",
                "../cogito/cogito.ergo",
                "../cogito/_build/cogito.ergo",
                "../cogito/build/cogito.ergo",
                "../../cogito/cogito.ergo",
                "../../cogito/_build/cogito.ergo",
                "../../cogito/build/cogito.ergo",
            };
            char *p = NULL;
            for (size_t i = 0; i < sizeof(cogito_paths) / sizeof(cogito_paths[0]); i++) {
                p = strdup(cogito_paths[i]);
                if (p && path_is_file(p)) {
                    if (!load_file(p, root_dir, stdlib_dir, arena, visited, hash, err)) {
                        free(p);
                        return NULL;
                    }
                    free(p);
                    goto found_cogito;
                }
                free(p);
                p = NULL;
            }
            // 2) Try relative to the executable
            //    Binary is at <project>/ergo/build/ergo, so cogito.ergo
            //    lives at <exe_dir>/../../cogito/{cogito.ergo,_build/cogito.ergo,build/cogito.ergo}
            {
                char *exe_dir = ergo_exe_dir();
                if (exe_dir) {
                    const char *exe_rel[] = {
                        "../../cogito/cogito.ergo",
                        "../../cogito/_build/cogito.ergo",
                        "../../cogito/build/cogito.ergo",
                    };
                    for (size_t j = 0; j < sizeof(exe_rel) / sizeof(exe_rel[0]); j++) {
                        p = path_join(exe_dir, exe_rel[j]);
                        if (p && path_is_file(p)) {
                            if (!load_file(p, root_dir, stdlib_dir, arena, visited, hash, err)) {
                                free(p);
                                free(exe_dir);
                                return NULL;
                            }
                            free(p);
                            free(exe_dir);
                            goto found_cogito;
                        }
                        free(p);
                        p = NULL;
                    }
                    free(exe_dir);
                }
            }
            // 3) stdlib fallback (stub that @includes the real cogito.ergo)
            {
                p = path_join(stdlib_dir, "cogito.ergo");
                if (p && path_is_file(p)) {
                    if (!load_file(p, root_dir, stdlib_dir, arena, visited, hash, err)) {
                        free(p);
                        return NULL;
                    }
                    free(p);
                    goto found_cogito;
                }
                free(p);
            }
            set_err(err, abs_path, "Cogito GUI framework not found. Run 'cd cogito && meson setup build && ninja -C build' to build Cogito, or ensure cogito.ergo is in cogito/_build/, cogito/build/, or stdlib");
            return NULL;
        found_cogito:
            continue;
        }
