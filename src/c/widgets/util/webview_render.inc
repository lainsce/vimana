// WebView lightweight markup renderer
// Supports both HTML and bracket-tag variants:
//   <h1>..</h1> / [h1]..[/h1]
//   <h2>..</h2> / [h2]..[/h2]
//   <h3>..</h3> / [h3]..[/h3]
//   <p>..</p>   / [p]..[/p]
//   <ul>..</ul> / [ul]..[/ul]
//   <li>..</li> / [li]..[/li]
//   <code>..</code> / [code]..[/code]
//   <blockquote>..</blockquote> / [blockquote]..[/blockquote]
//   <hr>, <hr/>, <hr /> / [hr]
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

static int cogito_wv_match_tag_name(const char* name, int len) {
  if (len == 2 && name[0] == 'h' && name[1] == '1') return COGITO_WV_TAG_H1;
  if (len == 2 && name[0] == 'h' && name[1] == '2') return COGITO_WV_TAG_H2;
  if (len == 2 && name[0] == 'h' && name[1] == '3') return COGITO_WV_TAG_H3;
  if (len == 1 && name[0] == 'p') return COGITO_WV_TAG_P;
  if (len == 2 && name[0] == 'u' && name[1] == 'l') return COGITO_WV_TAG_UL;
  if (len == 2 && name[0] == 'l' && name[1] == 'i') return COGITO_WV_TAG_LI;
  if (len == 2 && name[0] == 'h' && name[1] == 'r') return COGITO_WV_TAG_HR;
  if (len == 4 && memcmp(name, "code", 4) == 0) return COGITO_WV_TAG_CODE;
  if (len == 10 && memcmp(name, "blockquote", 10) == 0) return COGITO_WV_TAG_BLOCKQUOTE;
  return COGITO_WV_TAG_NONE;
}

// Parse a bracket tag with *p pointing just past '['.
// Advances *p past closing ']' on success.
static int cogito_wv_parse_bracket_tag(const char** p, bool* is_closing) {
  const char* s = *p;
  *is_closing = false;
  if (*s == '/') {
    *is_closing = true;
    s++;
  }

  const char* start = s;
  while (*s && *s != ']') s++;
  if (*s != ']') return COGITO_WV_TAG_NONE;

  int tag = cogito_wv_match_tag_name(start, (int)(s - start));
  if (tag == COGITO_WV_TAG_NONE) return COGITO_WV_TAG_NONE;

  *p = s + 1;
  return tag;
}

// Parse an HTML tag with *p pointing just past '<'.
// Advances *p past closing '>' on success.
static int cogito_wv_parse_html_tag(const char** p, bool* is_closing, bool* is_self_closing) {
  const char* s = *p;
  *is_closing = false;
  *is_self_closing = false;

  if (*s == '/') {
    *is_closing = true;
    s++;
  }

  const char* name_start = s;
  while ((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9')) s++;
  if (s == name_start) return COGITO_WV_TAG_NONE;

  int tag = cogito_wv_match_tag_name(name_start, (int)(s - name_start));
  if (tag == COGITO_WV_TAG_NONE) return COGITO_WV_TAG_NONE;

  while (*s && *s != '>') {
    if (*s == '/' && s[1] == '>') {
      *is_self_closing = true;
      s += 2;
      *p = s;
      return tag;
    }
    s++;
  }
  if (*s != '>') return COGITO_WV_TAG_NONE;

  *p = s + 1;
  return tag;
}

// Find the matching closing tag for the given tag type.
// Returns pointer to char after the closing tag, or NULL if not found.
// Sets *content_end to the start of the closing tag.
static const char* cogito_wv_find_close(const char* p, int tag, bool html_mode, const char** content_end) {
  const char* close_tag = NULL;
  int close_len = 0;
  if (html_mode) {
    switch (tag) {
      case COGITO_WV_TAG_H1:         close_tag = "</h1>"; close_len = 5; break;
      case COGITO_WV_TAG_H2:         close_tag = "</h2>"; close_len = 5; break;
      case COGITO_WV_TAG_H3:         close_tag = "</h3>"; close_len = 5; break;
      case COGITO_WV_TAG_P:          close_tag = "</p>";  close_len = 4; break;
      case COGITO_WV_TAG_UL:         close_tag = "</ul>"; close_len = 5; break;
      case COGITO_WV_TAG_LI:         close_tag = "</li>"; close_len = 5; break;
      case COGITO_WV_TAG_CODE:       close_tag = "</code>"; close_len = 7; break;
      case COGITO_WV_TAG_BLOCKQUOTE: close_tag = "</blockquote>"; close_len = 13; break;
      default: return NULL;
    }
  } else {
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

static bool cogito_wv_is_space_char(char c) {
  return c == ' ' || c == '\t';
}

static bool cogito_wv_is_name_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '-' || c == '_';
}

static char cogito_wv_ascii_lower(char c) {
  if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
  return c;
}

static bool cogito_wv_name_eq_ci(const char* s, int len, const char* lit) {
  int i = 0;
  for (; i < len && lit[i]; i++) {
    if (cogito_wv_ascii_lower(s[i]) != cogito_wv_ascii_lower(lit[i])) {
      return false;
    }
  }
  return i == len && lit[i] == '\0';
}

static int cogito_wv_parse_href_from_tag(const char* tag_body, int len, char* out, int out_len) {
  if (!tag_body || len <= 0 || !out || out_len <= 0) return 0;
  out[0] = '\0';

  int i = 0;
  while (i < len) {
    while (i < len && (tag_body[i] == ' ' || tag_body[i] == '\t' || tag_body[i] == '\n' || tag_body[i] == '\r')) i++;
    if (i >= len) break;

    int key_start = i;
    while (i < len && cogito_wv_is_name_char(tag_body[i])) i++;
    int key_len = i - key_start;
    if (key_len <= 0) {
      i++;
      continue;
    }

    while (i < len && (tag_body[i] == ' ' || tag_body[i] == '\t')) i++;
    if (i >= len || tag_body[i] != '=') {
      while (i < len && tag_body[i] != ' ' && tag_body[i] != '\t' && tag_body[i] != '\n' && tag_body[i] != '\r') i++;
      continue;
    }
    i++;
    while (i < len && (tag_body[i] == ' ' || tag_body[i] == '\t')) i++;
    if (i >= len) break;

    int val_start = i;
    int val_end = i;
    if (tag_body[i] == '"' || tag_body[i] == '\'') {
      char q = tag_body[i++];
      val_start = i;
      while (i < len && tag_body[i] != q) i++;
      val_end = i;
      if (i < len && tag_body[i] == q) i++;
    } else {
      while (i < len && tag_body[i] != ' ' && tag_body[i] != '\t' && tag_body[i] != '\n' && tag_body[i] != '\r' && tag_body[i] != '>') i++;
      val_end = i;
    }

    if (cogito_wv_name_eq_ci(tag_body + key_start, key_len, "href")) {
      int n = val_end - val_start;
      if (n >= out_len) n = out_len - 1;
      if (n < 0) n = 0;
      if (n > 0) memcpy(out, tag_body + val_start, (size_t)n);
      out[n] = '\0';
      return n;
    }
  }

  return 0;
}

static int cogito_wv_append_plain_entity(const char** pp, const char* end, char* out, int out_cap, int out_len) {
  const char* p = *pp;
  if (!p || p >= end || *p != '&') return out_len;

  if (end - p >= 5 && memcmp(p, "&amp;", 5) == 0) {
    if (out_len + 1 < out_cap) out[out_len++] = '&';
    *pp = p + 5;
    return out_len;
  }
  if (end - p >= 4 && memcmp(p, "&lt;", 4) == 0) {
    if (out_len + 1 < out_cap) out[out_len++] = '<';
    *pp = p + 4;
    return out_len;
  }
  if (end - p >= 4 && memcmp(p, "&gt;", 4) == 0) {
    if (out_len + 1 < out_cap) out[out_len++] = '>';
    *pp = p + 4;
    return out_len;
  }
  if (end - p >= 6 && memcmp(p, "&quot;", 6) == 0) {
    if (out_len + 1 < out_cap) out[out_len++] = '"';
    *pp = p + 6;
    return out_len;
  }
  if (end - p >= 5 && memcmp(p, "&#39;", 5) == 0) {
    if (out_len + 1 < out_cap) out[out_len++] = '\'';
    *pp = p + 5;
    return out_len;
  }
  if (end - p >= 5 && memcmp(p, "&#91;", 5) == 0) {
    if (out_len + 1 < out_cap) out[out_len++] = '[';
    *pp = p + 5;
    return out_len;
  }
  if (end - p >= 5 && memcmp(p, "&#93;", 5) == 0) {
    if (out_len + 1 < out_cap) out[out_len++] = ']';
    *pp = p + 5;
    return out_len;
  }

  return out_len;
}

static void cogito_wv_emit_token(const CogitoNode* n,
                                 const char* token,
                                 int token_len,
                                 bool is_space,
                                 bool bold,
                                 bool italic,
                                 bool link,
                                 int x,
                                 int y,
                                 int w,
                                 int fs,
                                 int line_h,
                                 CogitoColor base_color,
                                 int* cursor_x,
                                 int* line_idx,
                                 bool draw) {
  if (!token || token_len <= 0 || w <= 0) return;
  int draw_w = cogito_text_width_size_n_node(n, token, token_len, fs);
  if (draw_w < 0) draw_w = 0;

  if (is_space && *cursor_x == 0) {
    return;
  }

  if (!is_space && *cursor_x > 0 && *cursor_x + draw_w > w) {
    *cursor_x = 0;
    (*line_idx)++;
  }

  if (is_space && *cursor_x + draw_w > w) {
    *cursor_x = 0;
    (*line_idx)++;
    return;
  }

  if (draw) {
    char buf[1024];
    int ncopy = token_len;
    if (ncopy > 1000) ncopy = 1000;
    memcpy(buf, token, (size_t)ncopy);
    buf[ncopy] = '\0';

    CogitoColor color = base_color;
    if (link) {
      color = cogito_rgba(41, 98, 255, base_color.a);
    } else if (italic) {
      color.a = (uint8_t)(color.a * 0.92f);
    }

    int dx = x + *cursor_x;
    int dy = y + (*line_idx) * line_h;
    cogito_draw_text_size_node(n, dx, dy, buf, color, fs, bold, italic);
    if (link && !is_space && draw_w > 0) {
      int uy = dy + line_h - 2;
      cogito_draw_rect(dx, uy, draw_w, 1, color, 0);
    }
  }

  *cursor_x += draw_w;
  if (*cursor_x > w) {
    *cursor_x = 0;
    (*line_idx)++;
  }
}

static int cogito_wv_draw_inline(const CogitoNode* n,
                                 const char* src,
                                 int src_len,
                                 int x,
                                 int y,
                                 int w,
                                 CogitoColor text_color,
                                 int fs,
                                 bool base_bold,
                                 bool draw) {
  if (!src || src_len <= 0 || w <= 0) return cogito_text_height_size(fs);

  const char* p = src;
  const char* end = src + src_len;
  bool bold = base_bold;
  bool italic = false;
  bool link = false;
  int cursor_x = 0;
  int line_idx = 0;
  int line_h = cogito_text_height_size(fs);
  if (line_h <= 0) line_h = fs > 0 ? fs : 14;

  while (p < end) {
    if (*p == '\r' || *p == '\n') {
      if (*p == '\r' && p + 1 < end && p[1] == '\n') p++;
      cursor_x = 0;
      line_idx++;
      p++;
      continue;
    }

    if (*p == '<') {
      const char* gt = p + 1;
      while (gt < end && *gt != '>') gt++;
      if (gt < end && *gt == '>') {
        const char* q = p + 1;
        while (q < gt && (*q == ' ' || *q == '\t')) q++;
        bool closing = false;
        if (q < gt && *q == '/') {
          closing = true;
          q++;
          while (q < gt && (*q == ' ' || *q == '\t')) q++;
        }

        const char* name = q;
        while (q < gt && cogito_wv_is_name_char(*q)) q++;
        int name_len = (int)(q - name);

        if (name_len > 0) {
          if (cogito_wv_name_eq_ci(name, name_len, "b")) {
            bold = !closing;
            p = gt + 1;
            continue;
          }
          if (cogito_wv_name_eq_ci(name, name_len, "i")) {
            italic = !closing;
            p = gt + 1;
            continue;
          }
          if (cogito_wv_name_eq_ci(name, name_len, "a")) {
            if (closing) {
              link = false;
            } else {
              char href_buf[2048];
              href_buf[0] = '\0';
              int body_start = (int)((q + 1) - (p + 1));
              int body_len = (int)(gt - (p + 1)) - body_start;
              if (body_start >= 0 && body_len > 0) {
                cogito_wv_parse_href_from_tag(p + 1 + body_start, body_len, href_buf, (int)sizeof(href_buf));
              }
              link = href_buf[0] != '\0';
            }
            p = gt + 1;
            continue;
          }
          if (cogito_wv_name_eq_ci(name, name_len, "br")) {
            cursor_x = 0;
            line_idx++;
            p = gt + 1;
            continue;
          }
          if (cogito_wv_name_eq_ci(name, name_len, "code")) {
            p = gt + 1;
            continue;
          }
        }
      }
    }

    if (cogito_wv_is_space_char(*p)) {
      char tok[64];
      int ncopy = 0;
      while (p < end && cogito_wv_is_space_char(*p)) {
        if (ncopy + 1 < (int)sizeof(tok)) tok[ncopy++] = ' ';
        p++;
      }
      tok[ncopy] = '\0';
      cogito_wv_emit_token(n, tok, ncopy, true, bold, italic, link,
                           x, y, w, fs, line_h, text_color,
                           &cursor_x, &line_idx, draw);
      continue;
    }

    {
      char tok[1024];
      int ncopy = 0;
      while (p < end && !cogito_wv_is_space_char(*p) && *p != '\n' && *p != '\r' && *p != '<') {
        int prev_len = ncopy;
        if (*p == '&') {
          const char* before = p;
          ncopy = cogito_wv_append_plain_entity(&p, end, tok, (int)sizeof(tok), ncopy);
          if (p != before) continue;
        }
        if (ncopy + 1 < (int)sizeof(tok)) tok[ncopy++] = *p;
        p++;
        if (ncopy == prev_len) break;
      }
      tok[ncopy] = '\0';
      if (ncopy > 0) {
        cogito_wv_emit_token(n, tok, ncopy, false, bold, italic, link,
                             x, y, w, fs, line_h, text_color,
                             &cursor_x, &line_idx, draw);
      } else {
        p++;
      }
    }
  }

  return (line_idx + 1) * line_h;
}

static bool cogito_wv_extract_first_href(const char* html, char* out, int out_len) {
  if (!html || !html[0] || !out || out_len <= 1) return false;
  out[0] = '\0';

  const char* p = html;
  while (*p) {
    if (*p == '<') {
      const char* gt = p + 1;
      while (*gt && *gt != '>') gt++;
      if (*gt == '>') {
        const char* q = p + 1;
        while (q < gt && (*q == ' ' || *q == '\t')) q++;
        bool closing = false;
        if (q < gt && *q == '/') {
          closing = true;
          q++;
        }
        const char* name = q;
        while (q < gt && cogito_wv_is_name_char(*q)) q++;
        int name_len = (int)(q - name);
        if (!closing && name_len > 0 && cogito_wv_name_eq_ci(name, name_len, "a")) {
          int body_start = (int)((q + 1) - (p + 1));
          int body_len = (int)(gt - (p + 1)) - body_start;
          if (body_start >= 0 && body_len > 0) {
            int n = cogito_wv_parse_href_from_tag(p + 1 + body_start, body_len, out, out_len);
            if (n > 0) return true;
          }
        }
        p = gt + 1;
        continue;
      }
    }
    p++;
  }

  return false;
}

static bool cogito_wv_is_probably_url(const char* s) {
  if (!s || !s[0]) return false;
  return strncmp(s, "https://", 8) == 0 ||
         strncmp(s, "http://", 7) == 0 ||
         strncmp(s, "mailto:", 7) == 0 ||
         strncmp(s, "file://", 7) == 0;
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

    // Check for opening/closing tag in either [tag] or <tag> form.
    if (*p == '[' || *p == '<') {
      const char* tag_end = p + 1;
      bool is_closing = false;
      bool is_self_closing = false;
      bool tag_is_html = (*p == '<');
      int tag = COGITO_WV_TAG_NONE;

      if (*p == '[') {
        tag = cogito_wv_parse_bracket_tag(&tag_end, &is_closing);
      } else {
        tag = cogito_wv_parse_html_tag(&tag_end, &is_closing, &is_self_closing);
      }

      if (tag != COGITO_WV_TAG_NONE) {
        p = tag_end;

        if (is_closing) {
          if (tag == COGITO_WV_TAG_UL) {
            in_ul = false;
          }
          continue;
        }

        if (tag == COGITO_WV_TAG_UL) {
          in_ul = true;
          if (!first_block) cursor_y += 4;
          continue;
        }

        if (tag == COGITO_WV_TAG_HR || is_self_closing) {
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

        // Find closing tag and extract text content.
        const char* content_end = NULL;
        const char* after_close = cogito_wv_find_close(p, tag, tag_is_html, &content_end);
        int content_len = (int)(content_end - p);

        char buf[4096];
        int buf_max = 4095;
        if (tag == COGITO_WV_TAG_LI) {
          int bullet_len = (int)strlen(bullet_char);
          memcpy(buf, bullet_char, (size_t)bullet_len);
          int remain = buf_max - bullet_len;
          int copy_len = content_len < remain ? content_len : remain;
          if (copy_len > 0) memcpy(buf + bullet_len, p, (size_t)copy_len);
          buf[bullet_len + copy_len] = '\0';
        } else {
          cogito_wv_copy_text(buf, buf_max, p, content_len);
        }

        int fs = cogito_wv_font_size(tag);
        bool bold = cogito_wv_is_bold(tag);

        if (!first_block) {
          cursor_y += cogito_wv_margin_top(tag);
        }

        int draw_x = x;
        int draw_w = max_w;

        if (tag == COGITO_WV_TAG_LI || in_ul) {
          draw_x += bullet_indent;
          draw_w -= bullet_indent;
          if (draw_w < 20) draw_w = 20;
        }

        int bq_indent = 0;
        if (tag == COGITO_WV_TAG_BLOCKQUOTE) {
          bq_indent = 16;
          draw_x += bq_indent;
          draw_w -= bq_indent;
          if (draw_w < 20) draw_w = 20;
        }

        int code_pad = 0;
        if (tag == COGITO_WV_TAG_CODE) {
          code_pad = 8;
        }

        int text_h = 0;
        if (tag == COGITO_WV_TAG_CODE) {
          text_h = cogito_draw_text_wrapped(n, draw_x + code_pad, y + cursor_y + code_pad,
                                            draw_w - code_pad * 2, buf, text_color, fs, false, false, 0, false, 0);
        } else {
          text_h = cogito_wv_draw_inline(n, buf, (int)strlen(buf),
                                         draw_x + code_pad, y + cursor_y + code_pad,
                                         draw_w - code_pad * 2, text_color, fs, bold, false);
        }
        if (text_h <= 0) text_h = cogito_text_height_size(fs);

        int block_h = text_h + code_pad * 2;

        if (draw) {
          if (tag == COGITO_WV_TAG_BLOCKQUOTE) {
            CogitoColor bq_border = text_color;
            bq_border.a = (uint8_t)(text_color.a * 0.30f);
            cogito_draw_rect(x + 4, y + cursor_y, 3, block_h, bq_border, 0);

            CogitoColor bq_text = text_color;
            bq_text.a = (uint8_t)(text_color.a * 0.75f);
            cogito_wv_draw_inline(n, buf, (int)strlen(buf),
                                  draw_x + code_pad, y + cursor_y + code_pad,
                                  draw_w - code_pad * 2, bq_text, fs, bold, true);
          } else if (tag == COGITO_WV_TAG_CODE) {
            CogitoColor code_bg = text_color;
            code_bg.a = (uint8_t)(text_color.a * 0.08f);
            cogito_draw_rect(draw_x, y + cursor_y, draw_w, block_h, code_bg, 6);

            CogitoColor code_text = text_color;
            code_text.a = (uint8_t)(text_color.a * 0.85f);
            cogito_draw_text_wrapped(n, draw_x + code_pad, y + cursor_y + code_pad,
                                     draw_w - code_pad * 2, buf, code_text, fs, false, false, 0, true, 0);
          } else {
            cogito_wv_draw_inline(n, buf, (int)strlen(buf),
                                  draw_x, y + cursor_y,
                                  draw_w, text_color, fs, bold, true);
          }
        }

        cursor_y += block_h;
        cursor_y += cogito_wv_margin_bottom(tag);
        first_block = false;
        if (after_close) p = after_close;
        continue;
      }
    }

    {
      // Plain text outside tags - render as paragraph
      const char* line_start = p;
      while (*p && *p != '\n') {
        if (*p == '[' || *p == '<') {
          const char* probe_end = p + 1;
          bool probe_closing = false;
          bool probe_self = false;
          int probe_tag = (*p == '[')
            ? cogito_wv_parse_bracket_tag(&probe_end, &probe_closing)
            : cogito_wv_parse_html_tag(&probe_end, &probe_closing, &probe_self);
          if (probe_tag != COGITO_WV_TAG_NONE) {
            break;
          }
        }
        p++;
      }
      int len = (int)(p - line_start);
      if (len > 0) {
        char buf[4096];
        cogito_wv_copy_text(buf, 4095, line_start, len);

        if (!first_block) cursor_y += 4;

        int fs = 14;
        int text_h = cogito_wv_draw_inline(n, buf, len, x, y + cursor_y, max_w, text_color, fs, false, draw);
        if (text_h <= 0) text_h = cogito_text_height_size(fs);
        cursor_y += text_h + 6;
        first_block = false;
      }
      if (len == 0 && *p && *p != '\n') {
        // Unrecognized markup marker; treat it as a literal character.
        p++;
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
