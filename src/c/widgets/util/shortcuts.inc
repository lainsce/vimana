// widgets/util/shortcuts.inc — Global keyboard shortcut registry
//
// Provides cogito_shortcut_register / cogito_shortcut_unregister / cogito_shortcut_dispatch.
// Shortcuts are stored in a flat static array (COGITO_MAX_SHORTCUTS slots).
// Each entry has a scancode, modifier bitmask, and Yis callback closure.

int cogito_shortcut_register(int scancode, int modifiers, YisVal callback) {
  // Find an empty slot or append
  int slot = -1;
  for (int i = 0; i < cogito_shortcut_count; i++) {
    if (!cogito_shortcuts[i].active) { slot = i; break; }
  }
  if (slot < 0) {
    if (cogito_shortcut_count >= COGITO_MAX_SHORTCUTS) return -1;
    slot = cogito_shortcut_count++;
  }
  cogito_shortcuts[slot].scancode = scancode;
  cogito_shortcuts[slot].modifiers = modifiers;
  cogito_shortcuts[slot].callback = callback;
  cogito_shortcuts[slot].active = true;
  yis_retain_val(callback);
  return slot;
}

bool cogito_shortcut_unregister(int id) {
  if (id < 0 || id >= cogito_shortcut_count) return false;
  if (!cogito_shortcuts[id].active) return false;
  yis_release_val(cogito_shortcuts[id].callback);
  cogito_shortcuts[id].callback = YV_NULLV;
  cogito_shortcuts[id].active = false;
  return true;
}

void cogito_shortcut_unregister_all(void) {
  for (int i = 0; i < cogito_shortcut_count; i++) {
    if (cogito_shortcuts[i].active) {
      yis_release_val(cogito_shortcuts[i].callback);
      cogito_shortcuts[i].callback = YV_NULLV;
      cogito_shortcuts[i].active = false;
    }
  }
  cogito_shortcut_count = 0;
}

// Dispatch: returns true if a shortcut matched and was invoked.
static bool cogito_shortcut_dispatch(void) {
  if (cogito_shortcut_count == 0) return false;

  bool cmd  = cogito_backend->is_key_down(SDL_SCANCODE_LGUI) ||
              cogito_backend->is_key_down(SDL_SCANCODE_RGUI) ||
              cogito_backend->is_key_down(SDL_SCANCODE_LCTRL) ||
              cogito_backend->is_key_down(SDL_SCANCODE_RCTRL);
  bool shift = cogito_backend->is_key_down(SDL_SCANCODE_LSHIFT) ||
               cogito_backend->is_key_down(SDL_SCANCODE_RSHIFT);
  bool alt   = cogito_backend->is_key_down(SDL_SCANCODE_LALT) ||
               cogito_backend->is_key_down(SDL_SCANCODE_RALT);

  int current_mods = 0;
  if (cmd)   current_mods |= COGITO_SHORTCUT_MOD_CMD;
  if (shift) current_mods |= COGITO_SHORTCUT_MOD_SHIFT;
  if (alt)   current_mods |= COGITO_SHORTCUT_MOD_ALT;

  for (int i = 0; i < cogito_shortcut_count; i++) {
    CogitoShortcut *sc = &cogito_shortcuts[i];
    if (!sc->active) continue;
    if (sc->modifiers != current_mods) continue;
    if (!cogito_backend->is_key_pressed(sc->scancode)) continue;

    // Matched — invoke callback
    if (sc->callback.tag == EVT_OBJ || sc->callback.tag == EVT_FN) {
      YisVal args[1] = { YV_NULLV };
      yis_call(sc->callback, 0, args);
    }
    return true;
  }
  return false;
}
