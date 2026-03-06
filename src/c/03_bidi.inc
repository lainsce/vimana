// Cogito Bidi Text Pipeline
// Unicode Bidirectional Algorithm via FriBidi + SDL3_ttf direction support

#ifdef COGITO_HAS_FRIBIDI
#include <fribidi.h>
#endif

#define COGITO_BIDI_MAX_RUNS 64

typedef struct CogitoBidiRun {
  int byte_offset;  // offset into original UTF-8 string
  int byte_len;     // byte length of this run
  bool rtl;         // true if this run is right-to-left
} CogitoBidiRun;

// Check if a UTF-8 string is pure ASCII (fast path: no bidi processing needed)
static bool cogito_text_is_ascii(const char* text) {
  for (const unsigned char* p = (const unsigned char*)text; *p; p++) {
    if (*p >= 0x80) return false;
  }
  return true;
}

#ifdef COGITO_HAS_FRIBIDI

// Convert UTF-8 to UTF-32 (FriBidiChar). Returns number of codepoints.
static int cogito_utf8_to_utf32(const char* utf8, FriBidiChar* out, int max_cp) {
  int n = 0;
  const unsigned char* p = (const unsigned char*)utf8;
  while (*p && n < max_cp) {
    uint32_t cp;
    int step;
    if (*p < 0x80) {
      cp = *p; step = 1;
    } else if ((*p & 0xE0) == 0xC0) {
      cp = *p & 0x1F; step = 2;
    } else if ((*p & 0xF0) == 0xE0) {
      cp = *p & 0x0F; step = 3;
    } else if ((*p & 0xF8) == 0xF0) {
      cp = *p & 0x07; step = 4;
    } else {
      p++; continue;
    }
    for (int i = 1; i < step; i++) {
      if ((p[i] & 0xC0) != 0x80) { step = i; break; }
      cp = (cp << 6) | (p[i] & 0x3F);
    }
    out[n++] = (FriBidiChar)cp;
    p += step;
  }
  return n;
}

// Build byte offset map: for each codepoint index, the byte offset in UTF-8
static void cogito_utf8_byte_offsets(const char* utf8, int* offsets, int max_cp) {
  int n = 0;
  const unsigned char* base = (const unsigned char*)utf8;
  const unsigned char* p = base;
  while (*p && n < max_cp) {
    offsets[n++] = (int)(p - base);
    if (*p < 0x80) p += 1;
    else if ((*p & 0xE0) == 0xC0) p += 2;
    else if ((*p & 0xF0) == 0xE0) p += 3;
    else if ((*p & 0xF8) == 0xF0) p += 4;
    else p++;
  }
  // Sentinel: end offset
  if (n < max_cp) offsets[n] = (int)(p - base);
}

// Run the Unicode Bidi Algorithm and split text into directional runs.
// Returns number of runs (0 on error). Runs are in visual order.
static int cogito_bidi_get_runs(const char* text, int text_len,
                                CogitoBidiRun* runs, int max_runs) {
  if (!text || text_len <= 0 || !runs || max_runs <= 0) return 0;

  // Allocate working buffers on the stack for reasonable text lengths
  int cp_count;
  FriBidiChar* logical = NULL;
  FriBidiChar* visual = NULL;
  FriBidiCharType* types = NULL;
  FriBidiLevel* levels = NULL;
  FriBidiStrIndex* l2v = NULL;
  FriBidiStrIndex* v2l = NULL;
  int* byte_offsets = NULL;
  bool heap = false;

  // Max codepoints we handle (4096 chars covers most UI text)
  #define COGITO_BIDI_STACK_MAX 4096
  FriBidiChar logical_stack[COGITO_BIDI_STACK_MAX];
  FriBidiChar visual_stack[COGITO_BIDI_STACK_MAX];
  FriBidiCharType types_stack[COGITO_BIDI_STACK_MAX];
  FriBidiLevel levels_stack[COGITO_BIDI_STACK_MAX];
  FriBidiStrIndex l2v_stack[COGITO_BIDI_STACK_MAX];
  FriBidiStrIndex v2l_stack[COGITO_BIDI_STACK_MAX];
  int byte_offsets_stack[COGITO_BIDI_STACK_MAX + 1];

  // Convert UTF-8 to UTF-32
  cp_count = cogito_utf8_to_utf32(text, logical_stack, COGITO_BIDI_STACK_MAX);
  if (cp_count <= 0) return 0;

  if (cp_count <= COGITO_BIDI_STACK_MAX) {
    logical = logical_stack;
    visual = visual_stack;
    types = types_stack;
    levels = levels_stack;
    l2v = l2v_stack;
    v2l = v2l_stack;
    byte_offsets = byte_offsets_stack;
  } else {
    // Very long text: heap allocate
    size_t sz = (size_t)cp_count;
    logical = (FriBidiChar*)malloc(sz * sizeof(FriBidiChar));
    visual = (FriBidiChar*)malloc(sz * sizeof(FriBidiChar));
    types = (FriBidiCharType*)malloc(sz * sizeof(FriBidiCharType));
    levels = (FriBidiLevel*)malloc(sz * sizeof(FriBidiLevel));
    l2v = (FriBidiStrIndex*)malloc(sz * sizeof(FriBidiStrIndex));
    v2l = (FriBidiStrIndex*)malloc(sz * sizeof(FriBidiStrIndex));
    byte_offsets = (int*)malloc((sz + 1) * sizeof(int));
    if (!logical || !visual || !types || !levels || !l2v || !v2l || !byte_offsets) {
      free(logical); free(visual); free(types);
      free(levels); free(l2v); free(v2l); free(byte_offsets);
      return 0;
    }
    heap = true;
    cp_count = cogito_utf8_to_utf32(text, logical, cp_count);
  }

  // Build byte offset map
  cogito_utf8_byte_offsets(text, byte_offsets, cp_count + 1);

  // Determine base direction from global state
  FriBidiParType base_dir = cogito_is_rtl() ? FRIBIDI_PAR_RTL : FRIBIDI_PAR_LTR;

  // Get bidi types
  fribidi_get_bidi_types(logical, cp_count, types);

  // Get embedding levels
  FriBidiLevel max_level = fribidi_get_par_embedding_levels_ex(
    types, NULL, cp_count, &base_dir, levels);
  if (max_level == 0 && cp_count > 0) {
    // Fallback: all same direction
    if (heap) {
      free(logical); free(visual); free(types);
      free(levels); free(l2v); free(v2l); free(byte_offsets);
    }
    runs[0].byte_offset = 0;
    runs[0].byte_len = text_len;
    runs[0].rtl = cogito_is_rtl();
    return 1;
  }

  // Compute visual reordering map
  for (int i = 0; i < cp_count; i++) {
    l2v[i] = i;
    v2l[i] = i;
  }
  memcpy(visual, logical, (size_t)cp_count * sizeof(FriBidiChar));
  FriBidiLevel reorder_max = fribidi_reorder_line(FRIBIDI_FLAGS_DEFAULT, types, cp_count,
                       0, base_dir, levels, visual, l2v);
  (void)max_level;
  (void)reorder_max;
  // Build v2l from l2v
  for (int i = 0; i < cp_count; i++) {
    if (l2v[i] >= 0 && l2v[i] < cp_count)
      v2l[l2v[i]] = i;
  }

  // Extract runs: group consecutive codepoints (in visual order) with same level parity
  int num_runs = 0;
  int i = 0;
  while (i < cp_count && num_runs < max_runs) {
    int logical_idx = v2l[i];
    bool run_rtl = (levels[logical_idx] & 1) != 0;

    // Find extent of this run in visual order
    int run_start = i;
    while (i < cp_count) {
      int li = v2l[i];
      bool cur_rtl = (levels[li] & 1) != 0;
      if (cur_rtl != run_rtl) break;
      i++;
    }

    // Find min/max logical indices in this visual run for byte offsets
    int min_logical = cp_count, max_logical = -1;
    for (int k = run_start; k < i; k++) {
      int li = v2l[k];
      if (li < min_logical) min_logical = li;
      if (li > max_logical) max_logical = li;
    }

    runs[num_runs].byte_offset = byte_offsets[min_logical];
    int end_byte = (max_logical + 1 < cp_count)
                   ? byte_offsets[max_logical + 1]
                   : text_len;
    runs[num_runs].byte_len = end_byte - byte_offsets[min_logical];
    runs[num_runs].rtl = run_rtl;
    num_runs++;
  }

  if (heap) {
    free(logical); free(visual); free(types);
    free(levels); free(l2v); free(v2l); free(byte_offsets);
  }
  return num_runs;
  #undef COGITO_BIDI_STACK_MAX
}

#else // !COGITO_HAS_FRIBIDI

// Stub: no bidi processing, single run with global direction
static int cogito_bidi_get_runs(const char* text, int text_len,
                                CogitoBidiRun* runs, int max_runs) {
  if (!text || text_len <= 0 || !runs || max_runs <= 0) return 0;
  runs[0].byte_offset = 0;
  runs[0].byte_len = text_len;
  runs[0].rtl = cogito_is_rtl();
  return 1;
}

#endif // COGITO_HAS_FRIBIDI

// Check if text needs bidi processing (has non-ASCII characters and we're in RTL mode,
// or text contains RTL characters even in LTR mode)
static bool cogito_text_needs_bidi(const char* text) {
  if (!text || !text[0]) return false;
  // In LTR mode with pure ASCII, no bidi needed
  if (!cogito_is_rtl() && cogito_text_is_ascii(text)) return false;
  // In RTL mode, always process (even ASCII needs direction set)
  if (cogito_is_rtl()) return true;
  // In LTR mode with non-ASCII: check for RTL codepoints
  // Quick check: if any byte >= 0xD0 (covers Arabic 0x0600+, Hebrew 0x0590+)
  for (const unsigned char* p = (const unsigned char*)text; *p; p++) {
    if (*p >= 0xD0) return true;
  }
  return false;
}
