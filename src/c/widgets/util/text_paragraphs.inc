// text_paragraphs.inc — Per-paragraph block property management for CogitoNode

// Count paragraphs in text (number of \n chars + 1, or 0 if no text)
static int cogito_para_count_from_text(const char* text, int text_len) {
  if (!text || text_len == 0) return 1;  // empty text = 1 paragraph
  int count = 1;
  for (int i = 0; i < text_len; i++) {
    if (text[i] == '\n') count++;
  }
  return count;
}

// Get paragraph index for a byte offset
static int cogito_para_index_from_byte(const CogitoNode* n, int byte_offset) {
  if (!n || !n->text) return 0;
  const char* t = n->text->data;
  int len = (int)n->text->len;
  if (byte_offset > len) byte_offset = len;
  int idx = 0;
  for (int i = 0; i < byte_offset; i++) {
    if (t[i] == '\n') idx++;
  }
  return idx;
}

// Ensure para_props array has at least `needed` capacity
static void cogito_para_props_ensure(CogitoNode* n, int needed) {
  if (!n) return;
  if (n->para_props_cap >= needed) return;
  int new_cap = needed < 8 ? 8 : needed * 2;
  CogitoParagraphProps* p = (CogitoParagraphProps*)realloc(n->para_props, sizeof(CogitoParagraphProps) * new_cap);
  if (!p) return;
  // Zero-init new entries
  for (int i = n->para_props_cap; i < new_cap; i++) {
    p[i].text_align = 0;
    p[i].line_height = 0.0f;
    p[i].line_height_set = false;
  }
  n->para_props = p;
  n->para_props_cap = new_cap;
}

// Recount paragraphs and resize array to match.
// Preserves existing entries, zero-inits new ones, trims excess.
static void cogito_para_props_recount(CogitoNode* n) {
  if (!n) return;
  const char* t = n->text ? n->text->data : NULL;
  int tlen = n->text ? (int)n->text->len : 0;
  int pc = cogito_para_count_from_text(t, tlen);
  if (pc == n->para_props_count) return;
  cogito_para_props_ensure(n, pc);
  n->para_props_count = pc;
}

// Insert a new paragraph at `para_idx` inheriting props from `src_idx`
static void cogito_para_props_insert(CogitoNode* n, int para_idx, int src_idx) {
  if (!n || !n->para_props) return;
  int old_count = n->para_props_count;
  cogito_para_props_ensure(n, old_count + 1);
  // Shift entries after para_idx to the right
  for (int i = old_count; i > para_idx; i--) {
    n->para_props[i] = n->para_props[i - 1];
  }
  // Inherit from source paragraph
  if (src_idx >= 0 && src_idx < old_count) {
    n->para_props[para_idx] = n->para_props[src_idx < para_idx ? src_idx : src_idx + 1];
  } else {
    n->para_props[para_idx].text_align = 0;
    n->para_props[para_idx].line_height = 0.0f;
    n->para_props[para_idx].line_height_set = false;
  }
  n->para_props_count = old_count + 1;
}

// Remove paragraph at `para_idx`, shift remaining entries left
static void cogito_para_props_remove(CogitoNode* n, int para_idx) {
  if (!n || !n->para_props || para_idx < 0 || para_idx >= n->para_props_count) return;
  for (int i = para_idx; i < n->para_props_count - 1; i++) {
    n->para_props[i] = n->para_props[i + 1];
  }
  n->para_props_count--;
}

// Clone a paragraph props array (returns heap copy, caller owns)
static CogitoParagraphProps* cogito_para_props_clone(const CogitoParagraphProps* src, int count) {
  if (!src || count <= 0) return NULL;
  CogitoParagraphProps* dst = (CogitoParagraphProps*)malloc(sizeof(CogitoParagraphProps) * count);
  if (dst) memcpy(dst, src, sizeof(CogitoParagraphProps) * count);
  return dst;
}

// Resolve alignment for a byte offset — returns per-paragraph value if available, else node default
static int cogito_get_para_align(const CogitoNode* n, int byte_offset) {
  if (n->para_props && n->para_props_count > 0) {
    int idx = cogito_para_index_from_byte(n, byte_offset);
    if (idx < n->para_props_count) return n->para_props[idx].text_align;
  }
  return n->text_align;
}

// Resolve line height for a byte offset — returns per-paragraph value if set, else node default
static float cogito_get_para_line_height(const CogitoNode* n, int byte_offset, bool* out_set) {
  if (n->para_props && n->para_props_count > 0) {
    int idx = cogito_para_index_from_byte(n, byte_offset);
    if (idx < n->para_props_count && n->para_props[idx].line_height_set) {
      if (out_set) *out_set = true;
      return n->para_props[idx].line_height;
    }
  }
  if (out_set) *out_set = n->line_height_set;
  return n->line_height;
}
