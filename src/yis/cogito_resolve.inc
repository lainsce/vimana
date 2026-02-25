        if (str_eq_c(imp->name, "cogito")) {
            // Try multiple locations for cogito.yi
            // 1) cwd-relative paths
            const char *cogito_paths[] = {
                "cogito/cogito.yi",
                "cogito/_build/cogito.yi",
                "cogito/build/cogito.yi",
                "../cogito/cogito.yi",
                "../cogito/_build/cogito.yi",
                "../cogito/build/cogito.yi",
                "../../cogito/cogito.yi",
                "../../cogito/_build/cogito.yi",
                "../../cogito/build/cogito.yi",
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
            //    Binary is at <project>/yis/build/yis, so cogito.yi
            //    lives at <exe_dir>/../../cogito/{cogito.yi,_build/cogito.yi,build/cogito.yi}
            {
                char *exe_dir = yis_exe_dir();
                if (exe_dir) {
                    const char *exe_rel[] = {
                        "../../cogito/cogito.yi",
                        "../../cogito/_build/cogito.yi",
                        "../../cogito/build/cogito.yi",
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
            // 3) stdlib fallback (stub that @includes the real cogito.yi)
            {
                p = path_join(stdlib_dir, "cogito.yi");
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
            set_err(err, abs_path, "Cogito GUI framework not found. Run 'cd cogito && meson setup build && ninja -C build' to build Cogito, or ensure cogito.yi is in cogito/_build/, cogito/build/, or stdlib");
            return NULL;
        found_cogito:
            continue;
        }
