// text_spans.inc — Rich text span management for CogitoNode
//
// Spans are non-overlapping, sorted by start byte offset.
// Each span stores style overrides with _set flags; unset properties
// fall through to node-level defaults.

// ── Span array helpers ──────────────────────────────────────────

static void cogito_spans_ensure_cap(CogitoNode* n, int need) {
  if (n->span_cap >= need) return;
  int new_cap = n->span_cap ? n->span_cap * 2 : 8;
  if (new_cap < need) new_cap = need;
  CogitoTextSpan* p = (CogitoTextSpan*)realloc(n->spans, (size_t)new_cap * sizeof(CogitoTextSpan));
  if (!p) return;
  n->spans = p;
  n->span_cap = new_cap;
}

static void cogito_spans_insert_at(CogitoNode* n, int idx, CogitoTextSpan s) {
  cogito_spans_ensure_cap(n, n->span_count + 1);
  if (idx < n->span_count)
    memmove(&n->spans[idx + 1], &n->spans[idx],
            (size_t)(n->span_count - idx) * sizeof(CogitoTextSpan));
  n->spans[idx] = s;
  n->span_count++;
}

static void cogito_spans_remove_at(CogitoNode* n, int idx) {
  if (idx < 0 || idx >= n->span_count) return;
  if (idx < n->span_count - 1)
    memmove(&n->spans[idx], &n->spans[idx + 1],
            (size_t)(n->span_count - idx - 1) * sizeof(CogitoTextSpan));
  n->span_count--;
}

// ── Shift operations (called on text insert / delete) ───────────

static void cogito_spans_shift_insert(CogitoNode* n, int pos, int len) {
  if (!n || len <= 0) return;
  for (int i = 0; i < n->span_count; i++) {
    CogitoTextSpan* s = &n->spans[i];
    if (s->start >= pos) {
      s->start += len;
      s->end += len;
    } else if (s->end > pos) {
      // Insertion inside span — expand it
      s->end += len;
    }
  }
}

static void cogito_spans_shift_delete(CogitoNode* n, int del_start, int del_end) {
  if (!n || del_start >= del_end) return;
  int del_len = del_end - del_start;
  int write = 0;
  for (int i = 0; i < n->span_count; i++) {
    CogitoTextSpan* s = &n->spans[i];
    if (s->end <= del_start) {
      // Entirely before deleted range — keep as-is
      n->spans[write++] = *s;
    } else if (s->start >= del_end) {
      // Entirely after deleted range — shift backward
      s->start -= del_len;
      s->end -= del_len;
      n->spans[write++] = *s;
    } else if (s->start >= del_start && s->end <= del_end) {
      // Entirely inside deleted range — remove (skip)
    } else if (s->start < del_start && s->end > del_end) {
      // Deleted range is inside span — shrink
      s->end -= del_len;
      n->spans[write++] = *s;
    } else if (s->start < del_start) {
      // Overlaps start of deletion — truncate end
      s->end = del_start;
      if (s->end > s->start) n->spans[write++] = *s;
    } else {
      // Overlaps end of deletion — truncate start
      s->start = del_start;
      s->end -= del_len;
      if (s->end > s->start) n->spans[write++] = *s;
    }
  }
  n->span_count = write;
  // Remove any degenerate zero-length spans left over
  for (int i = n->span_count - 1; i >= 0; i--) {
    if (n->spans[i].start >= n->spans[i].end)
      cogito_spans_remove_at(n, i);
  }
}

// ── Clear all spans ─────────────────────────────────────────────

static void cogito_spans_clear(CogitoNode* n) {
  if (!n) return;
  if (n->spans) { free(n->spans); n->spans = NULL; }
  n->span_count = 0;
  n->span_cap = 0;
}

// ── Clone span array (for undo snapshots) ───────────────────────

static CogitoTextSpan* cogito_spans_clone(const CogitoTextSpan* src, int count) {
  if (!src || count <= 0) return NULL;
  CogitoTextSpan* dst = (CogitoTextSpan*)malloc((size_t)count * sizeof(CogitoTextSpan));
  if (dst) memcpy(dst, src, (size_t)count * sizeof(CogitoTextSpan));
  return dst;
}

// ── Get effective style at a byte position ──────────────────────
// Returns a span with _set flags indicating which properties are
// overridden at that position. If pos is not inside any span,
// all _set flags are false.

static CogitoTextSpan cogito_spans_at(const CogitoNode* n, int pos) {
  CogitoTextSpan result;
  memset(&result, 0, sizeof(result));
  if (!n) return result;
  for (int i = 0; i < n->span_count; i++) {
    const CogitoTextSpan* s = &n->spans[i];
    if (pos >= s->start && pos < s->end) {
      return *s;
    }
    if (s->start > pos) break; // spans are sorted
  }
  return result;
}

// ── Make a blank span with only position set ────────────────────

static CogitoTextSpan cogito_span_blank(int start, int end) {
  CogitoTextSpan s;
  memset(&s, 0, sizeof(s));
  s.start = start;
  s.end = end;
  return s;
}

// ── Merge a style property into a span ──────────────────────────

enum {
  COGITO_SPAN_BOLD = 0,
  COGITO_SPAN_ITALIC,
  COGITO_SPAN_UNDERLINE,
  COGITO_SPAN_STRIKETHROUGH,
  COGITO_SPAN_COLOR,
  COGITO_SPAN_SIZE,
  COGITO_SPAN_FAMILY,
};

// Apply a style override to the range [start, end) in the node's spans.
// This splits/creates spans as needed so that the range has the given property set.
static void cogito_spans_apply_bold(CogitoNode* n, int start, int end, bool val);
static void cogito_spans_apply_italic(CogitoNode* n, int start, int end, bool val);
static void cogito_spans_apply_underline(CogitoNode* n, int start, int end, bool val);
static void cogito_spans_apply_strikethrough(CogitoNode* n, int start, int end, bool val);
static void cogito_spans_apply_color(CogitoNode* n, int start, int end, CogitoColor val);
static void cogito_spans_apply_size(CogitoNode* n, int start, int end, int val);
static void cogito_spans_apply_family(CogitoNode* n, int start, int end, const char* val);

// Internal: ensure span coverage for [start, end) by splitting existing spans
// and creating new blank spans for uncovered regions. After this, every byte
// in [start, end) belongs to exactly one span.
static void cogito_spans_ensure_coverage(CogitoNode* n, int start, int end) {
  // 1. Split any span that partially overlaps start
  for (int i = 0; i < n->span_count; i++) {
    CogitoTextSpan* s = &n->spans[i];
    if (s->start < start && s->end > start && s->end <= end) {
      // Split: keep [s->start, start), [start, s->end) is new span
      CogitoTextSpan right = *s;
      right.start = start;
      s->end = start;
      cogito_spans_insert_at(n, i + 1, right);
      break;
    }
    if (s->start < start && s->end > end) {
      // Range is entirely within this span — split into 3
      CogitoTextSpan mid = *s;
      mid.start = start;
      mid.end = end;
      CogitoTextSpan right = *s;
      right.start = end;
      s->end = start;
      cogito_spans_insert_at(n, i + 1, mid);
      cogito_spans_insert_at(n, i + 2, right);
      return; // done — the range is fully covered by the mid span
    }
  }
  // 2. Split any span that partially overlaps end
  for (int i = 0; i < n->span_count; i++) {
    CogitoTextSpan* s = &n->spans[i];
    if (s->start >= start && s->start < end && s->end > end) {
      CogitoTextSpan right = *s;
      right.start = end;
      s->end = end;
      cogito_spans_insert_at(n, i + 1, right);
      break;
    }
  }
  // 3. Fill gaps: for any uncovered region in [start, end), insert blank spans
  int pos = start;
  for (int i = 0; i <= n->span_count; i++) {
    int s_start = (i < n->span_count) ? n->spans[i].start : end;
    if (s_start > end) s_start = end;
    if (s_start > pos) {
      // Gap [pos, s_start) needs a blank span
      cogito_spans_insert_at(n, i, cogito_span_blank(pos, s_start));
      i++; // skip the just-inserted span
    }
    if (i < n->span_count && n->spans[i].end > pos)
      pos = n->spans[i].end;
    if (pos >= end) break;
  }
}

#define COGITO_SPANS_APPLY_IMPL(NAME, FIELD, TYPE) \
static void cogito_spans_apply_##NAME(CogitoNode* n, int start, int end, TYPE val) { \
  if (!n || start >= end) return; \
  cogito_spans_ensure_coverage(n, start, end); \
  for (int i = 0; i < n->span_count; i++) { \
    CogitoTextSpan* s = &n->spans[i]; \
    if (s->start >= start && s->end <= end) { \
      s->FIELD##_set = true; \
      s->FIELD = val; \
    } \
  } \
}

COGITO_SPANS_APPLY_IMPL(bold, bold, bool)
COGITO_SPANS_APPLY_IMPL(italic, italic, bool)
COGITO_SPANS_APPLY_IMPL(underline, underline, bool)
COGITO_SPANS_APPLY_IMPL(strikethrough, strikethrough, bool)
COGITO_SPANS_APPLY_IMPL(color, color, CogitoColor)
COGITO_SPANS_APPLY_IMPL(size, size, int)

#undef COGITO_SPANS_APPLY_IMPL

static void cogito_spans_apply_family(CogitoNode* n, int start, int end, const char* val) {
  if (!n || start >= end || !val) return;
  cogito_spans_ensure_coverage(n, start, end);
  for (int i = 0; i < n->span_count; i++) {
    CogitoTextSpan* s = &n->spans[i];
    if (s->start >= start && s->end <= end) {
      s->family_set = true;
      strncpy(s->family, val, 63);
      s->family[63] = '\0';
    }
  }
}
