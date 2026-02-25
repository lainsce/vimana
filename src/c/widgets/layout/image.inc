case COGITO_IMAGE:
      // Try to load image from file path during layout so intrinsic sizing
      // can use real dimensions.  Only attempt if the backend is ready
      // (renderer available); otherwise the draw pass will handle it.
      if (n->icon && n->icon->data && n->icon->data[0] && !n->image.loaded) {
        if (cogito_backend && cogito_backend->texture_create) {
          CogitoTexture* tex = NULL;
          int tw = 0, th = 0;
          if (cogito_image_load_file_texture(n->icon->data, n->w, n->h,
                                             &tex, &tw, &th, &n->image.avg_lum)) {
            n->image.texture = tex;
            n->image.w = tw;
            n->image.h = th;
            n->image.loaded = true;
            cogito_layout_image_loaded_this_pass = true;
            // Dimensions are now set; a subsequent layout pass will pick
            // them up via the intrinsic-size case.  We intentionally do NOT
            // call cogito_window_relayout here — doing so re-enters
            // cogito_layout_window recursively, causing O(N) nested full
            // layout passes when N images load during the same pass.
          }
          // Do NOT mark image_loaded on failure here — the draw pass
          // will retry when the renderer is guaranteed to be active.
        }
      }
      break;
