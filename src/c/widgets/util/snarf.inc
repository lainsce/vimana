// Snarf clipboard system
// Stores clipboard contents in ~/.snarf as Base64 to prevent clipboard sniping.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// ---- Base64 encode/decode ----

static const char cogito_b64_table[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char* cogito_b64_encode(const char* src, size_t len) {
  size_t olen = 4 * ((len + 2) / 3);
  char* out = (char*)malloc(olen + 1);
  if (!out) return NULL;
  size_t i = 0, j = 0;
  while (i < len) {
    uint32_t a = (uint32_t)(unsigned char)src[i++];
    uint32_t b = (i < len) ? (uint32_t)(unsigned char)src[i++] : 0;
    uint32_t c = (i < len) ? (uint32_t)(unsigned char)src[i++] : 0;
    uint32_t triple = (a << 16) | (b << 8) | c;
    out[j++] = cogito_b64_table[(triple >> 18) & 0x3F];
    out[j++] = cogito_b64_table[(triple >> 12) & 0x3F];
    out[j++] = (i > len + 1) ? '=' : cogito_b64_table[(triple >> 6) & 0x3F];
    out[j++] = (i > len)     ? '=' : cogito_b64_table[triple & 0x3F];
  }
  out[j] = '\0';
  return out;
}

static const signed char cogito_b64_decode_table[128] = {
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
  52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
  -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
  15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
  -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
  41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
};

static char* cogito_b64_decode(const char* src, size_t* out_len) {
  size_t slen = strlen(src);
  size_t alloc = (slen / 4) * 3 + 4;
  char* out = (char*)malloc(alloc);
  if (!out) return NULL;
  size_t olen = 0;
  unsigned int buf = 0;
  int bits = 0;
  for (const char* p = src; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
    if (c >= 128 || cogito_b64_decode_table[c] < 0) continue;
    buf = (buf << 6) | (unsigned int)cogito_b64_decode_table[c];
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out[olen++] = (char)(unsigned char)(buf >> bits);
    }
  }
  out[olen] = '\0';
  if (out_len) *out_len = olen;
  return out;
}

// ---- Snarf file I/O ----

static const char* cogito_snarf_path(void) {
  static char path[1024];
  static bool resolved = false;
  if (!resolved) {
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(path, sizeof(path), "%s/.snarf", home);
    resolved = true;
  }
  return path;
}

static void cogito_snarf_write(const char* text) {
  if (!text) return;
  size_t len = strlen(text);
  char* encoded = cogito_b64_encode(text, len);
  if (!encoded) return;
  FILE* f = fopen(cogito_snarf_path(), "w");
  if (f) {
    fputs(encoded, f);
    fclose(f);
  }
  free(encoded);
}

// Returns malloc'd string or NULL. Caller must free.
static char* cogito_snarf_read(void) {
  FILE* f = fopen(cogito_snarf_path(), "r");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long flen = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (flen <= 0) { fclose(f); return NULL; }
  char* encoded = (char*)malloc((size_t)flen + 1);
  if (!encoded) { fclose(f); return NULL; }
  size_t got = fread(encoded, 1, (size_t)flen, f);
  fclose(f);
  encoded[got] = '\0';
  size_t decoded_len = 0;
  char* decoded = cogito_b64_decode(encoded, &decoded_len);
  free(encoded);
  return decoded;
}

// ---- Text input clipboard operations ----

// Get the currently selected text from a text input node. Returns malloc'd string.
static char* cogito_text_input_get_selected(CogitoNode* n) {
  if (!n || !n->text || !n->text->data) return NULL;
  int s, e;
  if (n->kind == COGITO_COLORPICKER) {
    s = n->kd->colorpicker.sel_start;
    e = n->kd->colorpicker.sel_end;
  } else {
    s = n->kd->text_input.sel_start;
    e = n->kd->text_input.sel_end;
  }
  if (s == e) return NULL;
  if (s > e) { int t = s; s = e; e = t; }
  int len = (int)n->text->len;
  if (s < 0) s = 0;
  if (e > len) e = len;
  if (s >= e) return NULL;
  size_t sel_len = (size_t)(e - s);
  char* buf = (char*)malloc(sel_len + 1);
  if (!buf) return NULL;
  memcpy(buf, n->text->data + s, sel_len);
  buf[sel_len] = '\0';
  return buf;
}

static void cogito_text_input_copy(CogitoNode* n) {
  char* sel = cogito_text_input_get_selected(n);
  if (!sel) return;
  cogito_snarf_write(sel);
  free(sel);
}

static void cogito_text_input_cut(CogitoNode* n) {
  if (!n) return;
  cogito_text_input_copy(n);
  int* sel_start;
  int* sel_end;
  int* caret;
  if (n->kind == COGITO_COLORPICKER) {
    sel_start = &n->kd->colorpicker.sel_start;
    sel_end = &n->kd->colorpicker.sel_end;
    caret = &n->kd->colorpicker.caret;
  } else {
    sel_start = &n->kd->text_input.sel_start;
    sel_end = &n->kd->text_input.sel_end;
    caret = &n->kd->text_input.caret;
  }
  if (*sel_start == *sel_end) return;
  size_t a = (size_t)*sel_start;
  size_t b = (size_t)*sel_end;
  cogito_text_delete_range(n, a, b);
  int new_pos = (int)((a < b) ? a : b);
  *caret = new_pos;
  *sel_start = new_pos;
  *sel_end = new_pos;
  cogito_invoke_change(n);
}

static void cogito_text_input_paste(CogitoNode* n) {
  if (!n) return;
  char* text = cogito_snarf_read();
  if (!text) return;
  // For single-line inputs, strip newlines
  if (n->kind == COGITO_TEXTFIELD || n->kind == COGITO_SEARCHFIELD) {
    for (char* p = text; *p; p++) {
      if (*p == '\n' || *p == '\r') *p = ' ';
    }
  }
  int* sel_start;
  int* sel_end;
  int* caret;
  if (n->kind == COGITO_COLORPICKER) {
    sel_start = &n->kd->colorpicker.sel_start;
    sel_end = &n->kd->colorpicker.sel_end;
    caret = &n->kd->colorpicker.caret;
  } else {
    sel_start = &n->kd->text_input.sel_start;
    sel_end = &n->kd->text_input.sel_end;
    caret = &n->kd->text_input.caret;
  }
  // Delete existing selection
  if (*sel_start != *sel_end) {
    size_t a = (size_t)*sel_start;
    size_t b = (size_t)*sel_end;
    cogito_text_delete_range(n, a, b);
    int new_pos = (int)((a < b) ? a : b);
    *caret = new_pos;
    *sel_start = new_pos;
    *sel_end = new_pos;
  }
  // Insert pasted text at caret
  size_t base_len = n->text ? n->text->len : 0;
  size_t paste_len = strlen(text);
  size_t insert_at = (size_t)*caret;
  if (insert_at > base_len) insert_at = base_len;
  size_t out_len = base_len + paste_len;
  char* buf = (char*)malloc(out_len + 1);
  if (!buf) { free(text); return; }
  if (insert_at > 0 && n->text) memcpy(buf, n->text->data, insert_at);
  memcpy(buf + insert_at, text, paste_len);
  if (base_len > insert_at && n->text) memcpy(buf + insert_at + paste_len, n->text->data + insert_at, base_len - insert_at);
  buf[out_len] = '\0';
  cogito_text_set_from_buf(n, buf, out_len);
  free(buf);
  free(text);
  *caret = (int)(insert_at + paste_len);
  *sel_start = *caret;
  *sel_end = *caret;
  cogito_invoke_change(n);
}

// Select all text in a text input node
static void cogito_text_input_select_all(CogitoNode* n) {
  if (!n || !n->text) return;
  int len = (int)n->text->len;
  if (n->kind == COGITO_COLORPICKER) {
    n->kd->colorpicker.sel_start = 0;
    n->kd->colorpicker.sel_end = len;
    n->kd->colorpicker.caret = len;
  } else {
    n->kd->text_input.sel_start = 0;
    n->kd->text_input.sel_end = len;
    n->kd->text_input.caret = len;
  }
}

// ---- Context menu for text inputs ----

// Synthetic node used as the menu owner for text input context menus.
// Its position is set to the click location so the menu anchors there.
static CogitoNode cogito_ctx_menu_node;
static CogitoNode* cogito_ctx_menu_target = NULL;  // the text input being operated on
static bool cogito_ctx_menu_inited = false;

static void cogito_ctx_menu_init(void) {
  if (cogito_ctx_menu_inited) return;
  memset(&cogito_ctx_menu_node, 0, sizeof(cogito_ctx_menu_node));
  // Static labels — no refcount needed
  static YisStr lbl_undo  = { INT32_MAX, 4, "Undo" };
  static YisStr lbl_redo  = { INT32_MAX, 4, "Redo" };
  static YisStr lbl_cut   = { INT32_MAX, 3, "Cut" };
  static YisStr lbl_copy  = { INT32_MAX, 4, "Copy" };
  static YisStr lbl_paste = { INT32_MAX, 5, "Paste" };
  static YisStr lbl_all   = { INT32_MAX, 10, "Select All" };
  static YisStr sc_z  = { INT32_MAX, 4, "\xE2\x8C\x98" "Z" };  // ⌘Z
  static YisStr sc_sz = { INT32_MAX, 7, "\xE2\x87\xA7\xE2\x8C\x98" "Z" }; // ⇧⌘Z
  static YisStr sc_x = { INT32_MAX, 4, "\xE2\x8C\x98" "X" };  // ⌘X
  static YisStr sc_c = { INT32_MAX, 4, "\xE2\x8C\x98" "C" };  // ⌘C
  static YisStr sc_v = { INT32_MAX, 4, "\xE2\x8C\x98" "V" };  // ⌘V
  static YisStr sc_a = { INT32_MAX, 4, "\xE2\x8C\x98" "A" };  // ⌘A
  static YisStr* labels[6]    = { &lbl_undo, &lbl_redo, &lbl_cut, &lbl_copy, &lbl_paste, &lbl_all };
  static YisFn*  handlers[6]  = { NULL, NULL, NULL, NULL, NULL, NULL };
  static bool    sections[6]  = { false, true, false, false, true, false };
  static YisStr* icons[6]     = { NULL, NULL, NULL, NULL, NULL, NULL };
  static YisStr* shortcuts[6] = { &sc_z, &sc_sz, &sc_x, &sc_c, &sc_v, &sc_a };
  static bool    submenu[6]   = { false, false, false, false, false, false };
  static bool    toggled[6]   = { false, false, false, false, false, false };

  cogito_ctx_menu_node.menu_labels = labels;
  cogito_ctx_menu_node.menu_handlers = handlers;
  cogito_ctx_menu_node.menu_section_after = sections;
  cogito_ctx_menu_node.menu_icons = icons;
  cogito_ctx_menu_node.menu_shortcuts = shortcuts;
  cogito_ctx_menu_node.menu_submenu = submenu;
  cogito_ctx_menu_node.menu_toggled = toggled;
  cogito_ctx_menu_node.menu_len = 6;
  cogito_ctx_menu_node.menu_cap = 6;
  cogito_ctx_menu_inited = true;
}

static bool cogito_is_ctx_menu_owner(const CogitoNode* n) {
  return n == &cogito_ctx_menu_node;
}

// Open the text input context menu at (mx, my) for the given text input node.
static void cogito_ctx_menu_open(CogitoNode* target, int mx, int my) {
  if (!target) return;
  cogito_ctx_menu_init();
  cogito_ctx_menu_target = target;
  // Position the synthetic node at the click location (zero size so menu opens below)
  cogito_ctx_menu_node.x = mx;
  cogito_ctx_menu_node.y = my;
  cogito_ctx_menu_node.w = 0;
  cogito_ctx_menu_node.h = 0;
  // Attach the window so menu positioning can query win_w / win_h
  cogito_ctx_menu_node.parent = cogito_node_window(target);
  cogito_menu_open_for(&cogito_ctx_menu_node);
}

// Handle a click on the context menu. Returns true if handled.
static bool cogito_ctx_menu_invoke(size_t idx) {
  CogitoNode* t = cogito_ctx_menu_target;
  if (!t) return false;
  switch (idx) {
    case 0: cogito_undo_perform(t); break;
    case 1: cogito_redo_perform(t); break;
    case 2: cogito_undo_push(t); cogito_text_input_cut(t); break;
    case 3: cogito_text_input_copy(t); break;
    case 4: cogito_undo_push(t); cogito_text_input_paste(t); break;
    case 5: cogito_text_input_select_all(t); break;
    default: return false;
  }
  cogito_ctx_menu_target = NULL;
  CogitoNode* win = cogito_node_window(t);
  if (win) cogito_window_relayout(win);
  return true;
}
