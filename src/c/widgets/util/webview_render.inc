// WebView lightweight HTML/BBCode content renderer
// Supports: [h1]..[/h1], [h2]..[/h2], [h3]..[/h3], [p]..[/p],
//           [ul]..[/ul], [li]..[/li], [hr], [code]..[/code],
//           [blockquote]..[/blockquote], [b]..[/b], [i]..[/i]
//
// Designed for rendering Markdown-converted HTML content in the Cogito GUI.

enum {
  COGITO_WV_TAG_NONE = 0,
  COGITO_WV_TAG_H1,
  COGITO_WV_TAG_H2,
  COGITO_WV_TAG_H3,
  COGITO_WV_TAG_P,
  COGITO_WV_TAG_UL,
  COGITO_WV_TAG_LI,
  COGITO_WV_TAG_HR,
  COGITO_WV_TAG_CODE,
  COGITO_WV_TAG_BLOCKQUOTE,
};

typedef struct {
  int tag;
  const char* text;   // pointer into source
  int text_len;
} CogitoWvBlock;

// Parse a single opening tag at *p (pointing just past '[').
// Returns the tag type and advances *p past the ']'.
// Returns COGITO_WV_TAG_NONE if not recognised.
static int cogito_wv_parse_tag(const char** p) {
  const char* s = *p;
  if (s[0] == 'h' && s[1] == '1' && s[2] == ']') { *p = s + 3; return COGITO_WV_TAG_H1; }
  if (s[0] == 'h' && s[1] == '2' && s[2] == ']') { *p = s + 3; return COGITO_WV_TAG_H2; }
  if (s[0] == 'h' && s[1] == '3' && s[2] == ']') { *p = s + 3; return COGITO_WV_TAG_H3; }
  if (s[0] == 'p' && s[1] == ']') { *p = s + 2; return COGITO_WV_TAG_P; }
  if (s[0] == 'u' && s[1] == 'l' && s[2] == ']') { *p = s + 3; return COGITO_WV_TAG_UL; }
  if (s[0] == 'l' && s[1] == 'i' && s[2] == ']') { *p = s + 3; return COGITO_WV_TAG_LI; }
  if (s[0] == 'h' && s[1] == 'r' && s[2] == ']') { *p = s + 3; return COGITO_WV_TAG_HR; }
  if (s[0] == 'c' && s[1] == 'o' && s[2] == 'd' && s[3] == 'e' && s[4] == ']') { *p = s + 5; return COGITO_WV_TAG_CODE; }
  if (s[0] == 'b' && s[1] == 'l' && s[2] == 'o' && s[3] == 'c' && s[4] == 'k' &&
      s[5] == 'q' && s[6] == 'u' && s[7] == 'o' && s[8] == 't' && s[9] == 'e' && s[10] == ']') { *p = s + 11; return COGITO_WV_TAG_BLOCKQUOTE; }
  return COGITO_WV_TAG_NONE;
}

// Find the matching closing tag for the given tag type.
// Returns pointer to char after the closing tag, or NULL if not found.
// Sets *content_end to the start of the closing tag.
static const char* cogito_wv_find_close(const char* p, int tag, const char** content_end) {
  const char* close_tag = NULL;
  int close_len = 0;
  switch (tag) {
    case COGITO_WV_TAG_H1:         close_tag = "[/h1]"; close_len = 5; break;
    case COGITO_WV_TAG_H2:         close_tag = "[/h2]"; close_len = 5; break;
    case COGITO_WV_TAG_H3:         close_tag = "[/h3]"; close_len = 5; break;
    case COGITO_WV_TAG_P:          close_tag = "[/p]";  close_len = 4; break;
    case COGITO_WV_TAG_UL:         close_tag = "[/ul]"; close_len = 5; break;
    case COGITO_WV_TAG_LI:         close_tag = "[/li]"; close_len = 5; break;
    case COGITO_WV_TAG_CODE:       close_tag = "[/code]"; close_len = 7; break;
    case COGITO_WV_TAG_BLOCKQUOTE: close_tag = "[/blockquote]"; close_len = 14; break;
    default: return NULL;
  }
  const char* found = strstr(p, close_tag);
  if (!found) { *content_end = p + strlen(p); return *content_end; }
  *content_end = found;
  return found + close_len;
}

// Font sizes for each block type
static int cogito_wv_font_size(int tag) {
  switch (tag) {
    case COGITO_WV_TAG_H1: return 28;
    case COGITO_WV_TAG_H2: return 22;
    case COGITO_WV_TAG_H3: return 18;
    case COGITO_WV_TAG_CODE: return 13;
    default: return 14;
  }
}

static bool cogito_wv_is_bold(int tag) {
  return tag == COGITO_WV_TAG_H1 || tag == COGITO_WV_TAG_H2 || tag == COGITO_WV_TAG_H3;
}

// Spacing before a block element
static int cogito_wv_margin_top(int tag) {
  switch (tag) {
    case COGITO_WV_TAG_H1: return 16;
    case COGITO_WV_TAG_H2: return 14;
    case COGITO_WV_TAG_H3: return 12;
    case COGITO_WV_TAG_P:  return 4;
    case COGITO_WV_TAG_LI: return 2;
    case COGITO_WV_TAG_HR: return 8;
    case COGITO_WV_TAG_CODE: return 8;
    case COGITO_WV_TAG_BLOCKQUOTE: return 8;
    default: return 0;
  }
}

// Spacing after a block element
static int cogito_wv_margin_bottom(int tag) {
  switch (tag) {
    case COGITO_WV_TAG_H1: return 8;
    case COGITO_WV_TAG_H2: return 6;
    case COGITO_WV_TAG_H3: return 4;
    case COGITO_WV_TAG_P:  return 6;
    case COGITO_WV_TAG_LI: return 2;
    case COGITO_WV_TAG_HR: return 8;
    case COGITO_WV_TAG_CODE: return 8;
    case COGITO_WV_TAG_BLOCKQUOTE: return 8;
    default: return 0;
  }
}

// Copy up to max_len chars from src (length src_len) into dst, null-terminate.
static void cogito_wv_copy_text(char* dst, int max_len, const char* src, int src_len) {
  int len = src_len < max_len ? src_len : max_len;
  if (len > 0) memcpy(dst, src, (size_t)len);
  dst[len] = '\0';
}

// Render or measure the content of a WebView node.
// If draw is true, draws the content; otherwise just computes total height.
// Returns total content height in pixels.
static int cogito_webview_render(const CogitoNode* n, const char* html,
                                 int x, int y, int max_w,
                                 CogitoColor text_color, bool draw) {
  if (!html || !html[0] || max_w <= 0) return 0;

  int cursor_y = 0;
  bool first_block = true;
  bool in_ul = false;
  const char* p = html;

  // Bullet indent and size
  const int bullet_indent = 20;
  const char* bullet_char = "\xe2\x80\xa2 "; // "• "

  while (*p) {
    // Skip whitespace between tags
    while (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t') p++;
    if (!*p) break;

    // Check for opening tag
    if (*p == '[') {
      const char* tag_start = p + 1;

      // Check for closing tags (consumed by open-tag handlers)
      if (tag_start[0] == '/') {
        // Skip closing tags - they're handled by find_close
        const char* close_end = strchr(p, ']');
        if (close_end) {
          // Check if this is [/ul]
          if (tag_start[1] == 'u' && tag_start[2] == 'l') {
            in_ul = false;
          }
          p = close_end + 1;
          continue;
        }
        p++;
        continue;
      }

      int tag = cogito_wv_parse_tag(&tag_start);
      if (tag == COGITO_WV_TAG_NONE) {
        // Not a known tag, skip this character
        p++;
        continue;
      }
      p = tag_start; // advance past opening tag

      // Handle [ul] - just set list mode
      if (tag == COGITO_WV_TAG_UL) {
        in_ul = true;
        if (!first_block) cursor_y += 4;
        continue;
      }

      // Handle [hr] - no closing tag needed
      if (tag == COGITO_WV_TAG_HR) {
        if (!first_block) cursor_y += cogito_wv_margin_top(COGITO_WV_TAG_HR);
        if (draw) {
          int hr_y = y + cursor_y + 4;
          CogitoColor hr_color = text_color;
          hr_color.a = (uint8_t)(text_color.a * 0.25f);
          cogito_draw_rect(x, hr_y, max_w, 1, hr_color, 0);
        }
        cursor_y += 9; // 4 above + 1 line + 4 below
        cursor_y += cogito_wv_margin_bottom(COGITO_WV_TAG_HR);
        first_block = false;
        continue;
      }

      // Find closing tag and extract text content
      const char* content_end = NULL;
      const char* after_close = cogito_wv_find_close(p, tag, &content_end);

      int content_len = (int)(content_end - p);

      // Prepare text buffer
      char buf[4096];
      int buf_max = 4095;
      if (tag == COGITO_WV_TAG_LI) {
        // Prepend bullet character for list items
        int bullet_len = (int)strlen(bullet_char);
        memcpy(buf, bullet_char, (size_t)bullet_len);
        int remain = buf_max - bullet_len;
        int copy_len = content_len < remain ? content_len : remain;
        if (copy_len > 0) memcpy(buf + bullet_len, p, (size_t)copy_len);
        buf[bullet_len + copy_len] = '\0';
      } else {
        cogito_wv_copy_text(buf, buf_max, p, content_len);
      }

      // Compute font size and style
      int fs = cogito_wv_font_size(tag);
      bool bold = cogito_wv_is_bold(tag);

      // Margin top (collapse for first block)
      if (!first_block) {
        cursor_y += cogito_wv_margin_top(tag);
      }

      // Compute draw position
      int draw_x = x;
      int draw_w = max_w;

      // Indent for list items
      if (tag == COGITO_WV_TAG_LI || in_ul) {
        draw_x += bullet_indent;
        draw_w -= bullet_indent;
        if (draw_w < 20) draw_w = 20;
      }

      // Indent for blockquotes
      int bq_indent = 0;
      if (tag == COGITO_WV_TAG_BLOCKQUOTE) {
        bq_indent = 16;
        draw_x += bq_indent;
        draw_w -= bq_indent;
        if (draw_w < 20) draw_w = 20;
      }

      // Code block background
      int code_pad = 0;
      if (tag == COGITO_WV_TAG_CODE) {
        code_pad = 8;
      }

      // Measure text height (word-wrapped)
      int text_h = cogito_draw_text_wrapped(n, draw_x + code_pad, y + cursor_y + code_pad,
                                            draw_w - code_pad * 2, buf, text_color, fs, bold, false, 0, false);
      if (text_h <= 0) text_h = cogito_text_height_size(fs);

      int block_h = text_h + code_pad * 2;

      if (draw) {
        // Draw blockquote left border
        if (tag == COGITO_WV_TAG_BLOCKQUOTE) {
          CogitoColor bq_border = text_color;
          bq_border.a = (uint8_t)(text_color.a * 0.30f);
          cogito_draw_rect(x + 4, y + cursor_y, 3, block_h, bq_border, 0);

          // Dim the text color for blockquotes
          CogitoColor bq_text = text_color;
          bq_text.a = (uint8_t)(text_color.a * 0.75f);
          cogito_draw_text_wrapped(n, draw_x + code_pad, y + cursor_y + code_pad,
                                   draw_w - code_pad * 2, buf, bq_text, fs, false, false, 0, true);
        }
        // Draw code block background
        else if (tag == COGITO_WV_TAG_CODE) {
          CogitoColor code_bg = text_color;
          code_bg.a = (uint8_t)(text_color.a * 0.08f);
          cogito_draw_rect(draw_x, y + cursor_y, draw_w, block_h, code_bg, 6);

          CogitoColor code_text = text_color;
          code_text.a = (uint8_t)(text_color.a * 0.85f);
          cogito_draw_text_wrapped(n, draw_x + code_pad, y + cursor_y + code_pad,
                                   draw_w - code_pad * 2, buf, code_text, fs, false, false, 0, true);
        }
        // Draw normal text
        else {
          cogito_draw_text_wrapped(n, draw_x, y + cursor_y,
                                   draw_w, buf, text_color, fs, bold, false, 0, true);
        }
      }

      cursor_y += block_h;
      cursor_y += cogito_wv_margin_bottom(tag);
      first_block = false;

      // Advance past the closing tag
      if (after_close) p = after_close;
    } else {
      // Plain text outside tags - render as paragraph
      const char* line_start = p;
      while (*p && *p != '[' && *p != '\n') p++;
      int len = (int)(p - line_start);
      if (len > 0) {
        char buf[4096];
        cogito_wv_copy_text(buf, 4095, line_start, len);

        if (!first_block) cursor_y += 4;

        int fs = 14;
        int text_h = cogito_draw_text_wrapped(n, x, y + cursor_y, max_w, buf, text_color, fs, false, false, 0, draw);
        if (text_h <= 0) text_h = cogito_text_height_size(fs);
        cursor_y += text_h + 6;
        first_block = false;
      }
      if (*p == '\n') p++;
    }
  }
  return cursor_y;
}

// Convenience wrappers
static int cogito_webview_measure_content(const CogitoNode* n, const char* html, int max_w, CogitoColor text_color) {
  return cogito_webview_render(n, html, 0, 0, max_w, text_color, false);
}

static void cogito_webview_draw_content(const CogitoNode* n, const char* html,
                                        int x, int y, int max_w,
                                        CogitoColor text_color) {
  cogito_webview_render(n, html, x, y, max_w, text_color, true);
}
