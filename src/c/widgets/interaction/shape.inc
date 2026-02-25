static float cogito_shape_clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

static float cogito_shape_round4(float v) {
  return roundf(v * 10000.0f) / 10000.0f;
}

static float cogito_shape_inset01_4(float v) {
  float c = cogito_shape_clamp01(v);
  if (c < 0.0001f) c = 0.0001f;
  if (c > 0.9999f) c = 0.9999f;
  return cogito_shape_round4(c);
}

#include "shape_paths.inc"

static void cogito_svg_skip_ws(const char** ps) {
  const char* s = ps ? *ps : NULL;
  if (!s) return;
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == ',') s++;
  *ps = s;
}

static bool cogito_svg_parse_f32(const char** ps, float* out) {
  if (!ps || !*ps || !out) return false;
  cogito_svg_skip_ws(ps);
  char* end = NULL;
  float v = strtof(*ps, &end);
  if (end == *ps) return false;
  *out = cogito_shape_round4(v);
  *ps = end;
  return true;
}

static void cogito_shape_store_svg_poly(CogitoNode* n, const float* in_x, const float* in_y, int in_count) {
  if (!n) return;
  n->shape.svg_poly_count = 0;
  if (!in_x || !in_y || in_count < 3) return;

  float min_x = in_x[0], max_x = in_x[0], min_y = in_y[0], max_y = in_y[0];
  for (int i = 1; i < in_count; i++) {
    if (in_x[i] < min_x) min_x = in_x[i];
    if (in_x[i] > max_x) max_x = in_x[i];
    if (in_y[i] < min_y) min_y = in_y[i];
    if (in_y[i] > max_y) max_y = in_y[i];
  }
  float w = max_x - min_x;
  float h = max_y - min_y;
  float scale = (w > h) ? w : h;
  if (scale < 1e-6f) scale = 1.0f;
  float ccx = 0.5f * (min_x + max_x);
  float ccy = 0.5f * (min_y + max_y);

  int out_n = in_count;
  if (out_n > COGITO_SHAPE_SVG_POLY_MAX) out_n = COGITO_SHAPE_SVG_POLY_MAX;

  if (in_count <= COGITO_SHAPE_SVG_POLY_MAX) {
    for (int i = 0; i < in_count; i++) {
      float nx = 0.5f + (in_x[i] - ccx) / scale;
      float ny = 0.5f + (in_y[i] - ccy) / scale;
      n->shape.svg_poly[i * 2 + 0] = cogito_shape_inset01_4(nx);
      n->shape.svg_poly[i * 2 + 1] = cogito_shape_inset01_4(ny);
    }
    n->shape.svg_poly_count = in_count;
    return;
  }

  float seg[4096];
  float total = 0.0f;
  for (int i = 0; i + 1 < in_count; i++) {
    float dx = in_x[i + 1] - in_x[i];
    float dy = in_y[i + 1] - in_y[i];
    float d = sqrtf(dx * dx + dy * dy);
    if (d < 1e-6f) d = 1e-6f;
    seg[i] = d;
    total += d;
  }
  if (total < 1e-6f) return;

  int si = 0;
  float acc = 0.0f;
  for (int k = 0; k < out_n; k++) {
    float target = ((float)k / (float)out_n) * total;
    while (si + 1 < in_count - 1 && acc + seg[si] < target) {
      acc += seg[si];
      si++;
    }
    float local = (target - acc) / seg[si];
    if (local < 0.0f) local = 0.0f;
    if (local > 1.0f) local = 1.0f;
    float sx = in_x[si] + (in_x[si + 1] - in_x[si]) * local;
    float sy = in_y[si] + (in_y[si + 1] - in_y[si]) * local;
    float nx = 0.5f + (sx - ccx) / scale;
    float ny = 0.5f + (sy - ccy) / scale;
    n->shape.svg_poly[k * 2 + 0] = cogito_shape_inset01_4(nx);
    n->shape.svg_poly[k * 2 + 1] = cogito_shape_inset01_4(ny);
  }
  n->shape.svg_poly_count = out_n;
}

static bool cogito_shape_set_from_svg_path(CogitoNode* n, const char* path) {
  if (!n || !path) return false;
  float px[4096];
  float py[4096];
  int count = 0;
  float cx = 0.0f, cy = 0.0f;
  float sx = 0.0f, sy = 0.0f;
  char cmd = 0;
  const char* p = path;

  #define COGITO_SVG_PUSH(X, Y) do { \
    if (count < (int)(sizeof(px) / sizeof(px[0]))) { \
      px[count] = cogito_shape_round4((X)); \
      py[count] = cogito_shape_round4((Y)); \
      count++; \
    } \
  } while (0)

  while (*p) {
    cogito_svg_skip_ws(&p);
    if (!*p) break;
    if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
      cmd = *p;
      p++;
      if (cmd == 'Z' || cmd == 'z') {
        COGITO_SVG_PUSH(sx, sy);
      }
      continue;
    }
    if (!cmd) return false;

    if (cmd == 'M' || cmd == 'm') {
      float x, y;
      if (!cogito_svg_parse_f32(&p, &x) || !cogito_svg_parse_f32(&p, &y)) return false;
      cx = (cmd == 'm') ? (cx + x) : x;
      cy = (cmd == 'm') ? (cy + y) : y;
      sx = cx;
      sy = cy;
      COGITO_SVG_PUSH(cx, cy);
      cmd = (cmd == 'm') ? 'l' : 'L';
      continue;
    }
    if (cmd == 'L' || cmd == 'l') {
      float x, y;
      if (!cogito_svg_parse_f32(&p, &x) || !cogito_svg_parse_f32(&p, &y)) return false;
      cx = (cmd == 'l') ? (cx + x) : x;
      cy = (cmd == 'l') ? (cy + y) : y;
      COGITO_SVG_PUSH(cx, cy);
      continue;
    }
    if (cmd == 'H' || cmd == 'h') {
      float x;
      if (!cogito_svg_parse_f32(&p, &x)) return false;
      cx = (cmd == 'h') ? (cx + x) : x;
      COGITO_SVG_PUSH(cx, cy);
      continue;
    }
    if (cmd == 'V' || cmd == 'v') {
      float y;
      if (!cogito_svg_parse_f32(&p, &y)) return false;
      cy = (cmd == 'v') ? (cy + y) : y;
      COGITO_SVG_PUSH(cx, cy);
      continue;
    }
    if (cmd == 'C' || cmd == 'c') {
      float x1, y1, x2, y2, x3, y3;
      if (!cogito_svg_parse_f32(&p, &x1) || !cogito_svg_parse_f32(&p, &y1) ||
          !cogito_svg_parse_f32(&p, &x2) || !cogito_svg_parse_f32(&p, &y2) ||
          !cogito_svg_parse_f32(&p, &x3) || !cogito_svg_parse_f32(&p, &y3)) {
        return false;
      }
      float p0x = cx, p0y = cy;
      float p1x = (cmd == 'c') ? (cx + x1) : x1;
      float p1y = (cmd == 'c') ? (cy + y1) : y1;
      float p2x = (cmd == 'c') ? (cx + x2) : x2;
      float p2y = (cmd == 'c') ? (cy + y2) : y2;
      float p3x = (cmd == 'c') ? (cx + x3) : x3;
      float p3y = (cmd == 'c') ? (cy + y3) : y3;
      const int samples = 48;
      for (int i = 1; i <= samples; i++) {
        float t = (float)i / (float)samples;
        float u = 1.0f - t;
        float bx = (u*u*u)*p0x + 3.0f*(u*u)*t*p1x + 3.0f*u*(t*t)*p2x + (t*t*t)*p3x;
        float by = (u*u*u)*p0y + 3.0f*(u*u)*t*p1y + 3.0f*u*(t*t)*p2y + (t*t*t)*p3y;
        COGITO_SVG_PUSH(bx, by);
      }
      cx = p3x;
      cy = p3y;
      continue;
    }
    return false;
  }

  #undef COGITO_SVG_PUSH

  if (count < 3) return false;
  if (fabsf(px[0] - px[count - 1]) > 1e-4f || fabsf(py[0] - py[count - 1]) > 1e-4f) {
    if (count < (int)(sizeof(px) / sizeof(px[0]))) {
      px[count] = px[0];
      py[count] = py[0];
      count++;
    }
  }

  cogito_shape_store_svg_poly(n, px, py, count);

  float seg[4096];
  float total = 0.0f;
  for (int i = 0; i + 1 < count; i++) {
    float dx = px[i + 1] - px[i];
    float dy = py[i + 1] - py[i];
    float d = sqrtf(dx * dx + dy * dy);
    if (d < 1e-6f) d = 1e-6f;
    seg[i] = d;
    total += d;
  }
  if (total < 1e-6f) return false;

  float sxp[COGITO_SHAPE_VERTEX_COUNT];
  float syp[COGITO_SHAPE_VERTEX_COUNT];
  int si = 0;
  float acc = 0.0f;
  for (int k = 0; k < COGITO_SHAPE_VERTEX_COUNT; k++) {
    float target = ((float)k / (float)COGITO_SHAPE_VERTEX_COUNT) * total;
    while (si + 1 < count - 1 && acc + seg[si] < target) {
      acc += seg[si];
      si++;
    }
    float local = (target - acc) / seg[si];
    if (local < 0.0f) local = 0.0f;
    if (local > 1.0f) local = 1.0f;
    sxp[k] = cogito_shape_round4(px[si] + (px[si + 1] - px[si]) * local);
    syp[k] = cogito_shape_round4(py[si] + (py[si + 1] - py[si]) * local);
  }

  float min_x = sxp[0], max_x = sxp[0], min_y = syp[0], max_y = syp[0];
  for (int i = 1; i < COGITO_SHAPE_VERTEX_COUNT; i++) {
    if (sxp[i] < min_x) min_x = sxp[i];
    if (sxp[i] > max_x) max_x = sxp[i];
    if (syp[i] < min_y) min_y = syp[i];
    if (syp[i] > max_y) max_y = syp[i];
  }
  float w = max_x - min_x;
  float h = max_y - min_y;
  float scale = (w > h) ? w : h;
  if (scale < 1e-6f) scale = 1.0f;
  float ccx = 0.5f * (min_x + max_x);
  float ccy = 0.5f * (min_y + max_y);
  for (int i = 0; i < COGITO_SHAPE_VERTEX_COUNT; i++) {
    float nx = 0.5f + (sxp[i] - ccx) / scale;
    float ny = 0.5f + (syp[i] - ccy) / scale;
    n->shape.vertices[i * 2 + 0] = cogito_shape_inset01_4(nx);
    n->shape.vertices[i * 2 + 1] = cogito_shape_inset01_4(ny);
  }
  return true;
}

static void cogito_shape_apply_preset_node(CogitoNode* n, int preset) {
  if (!n) return;
  if (preset < 0) preset = 0;
  if (preset >= COGITO_SHAPE_COUNT) preset = COGITO_SHAPE_CIRCLE;
  n->shape.preset = preset;
  n->shape.svg_poly_count = 0;

  const char* svg_path = cogito_shape_svg_path_for_preset(preset);
  if (svg_path && cogito_shape_set_from_svg_path(n, svg_path)) return;

  const char* circle_path = cogito_shape_svg_path_for_preset(COGITO_SHAPE_CIRCLE);
  if (circle_path && cogito_shape_set_from_svg_path(n, circle_path)) {
    n->shape.preset = COGITO_SHAPE_CIRCLE;
    return;
  }

  const float base = -1.57079632679f;
  const float step = (2.0f * 3.14159265359f) / (float)COGITO_SHAPE_VERTEX_COUNT;
  for (int i = 0; i < COGITO_SHAPE_VERTEX_COUNT; i++) {
    float a = base + ((float)i * step);
    n->shape.vertices[i * 2 + 0] = cogito_shape_inset01_4(0.5f + cosf(a) * 0.5f);
    n->shape.vertices[i * 2 + 1] = cogito_shape_inset01_4(0.5f + sinf(a) * 0.5f);
  }
}

static CogitoNode* cogito_shape_new_obj(int preset) {
  CogitoNode* n = cogito_node_new(COGITO_SHAPE);
  n->shape.size_dp = 96;
  n->shape.color_style = 0;
  n->shape.custom_color_set = false;
  n->shape.custom_color = cogito_color(24, 24, 33, 255);
  cogito_shape_apply_preset_node(n, preset);
  return n;
}

static ErgoVal cogito_shape_new(ErgoVal presetv) {
  int preset = (int)ergo_as_int(presetv);
  return EV_OBJ(cogito_shape_new_obj(preset));
}

static void cogito_shape_set_preset(ErgoVal shapev, ErgoVal presetv) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_set_preset expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  int preset = (int)ergo_as_int(presetv);
  cogito_shape_apply_preset_node(n, preset);
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_shape_get_preset(ErgoVal shapev) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_get_preset expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  return EV_INT(n->shape.preset);
}

static void cogito_shape_set_size(ErgoVal shapev, ErgoVal sizev) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_set_size expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  int size_dp = (int)ergo_as_int(sizev);
  if (size_dp < 1) size_dp = 1;
  if (n->shape.size_dp == size_dp) return;
  n->shape.size_dp = size_dp;
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_shape_get_size(ErgoVal shapev) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_get_size expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  int size_dp = (n->shape.size_dp > 0) ? n->shape.size_dp : 96;
  return EV_INT(size_dp);
}

static void cogito_shape_set_color_style(ErgoVal shapev, ErgoVal stylev) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_set_color_style expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  int style = (int)ergo_as_int(stylev);
  if (style < 0) style = 0;
  if (style > 3) style = 3;
  n->shape.color_style = style;
  if (style > 0) {
    n->shape.custom_color_set = false;
    n->bg_set = false;
  }
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_shape_get_color_style(ErgoVal shapev) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_get_color_style expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  int style = n->shape.color_style;
  if (style < 0) style = 0;
  if (style > 3) style = 3;
  return EV_INT(style);
}

static void cogito_shape_set_color(ErgoVal shapev, ErgoVal colorv) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_set_color expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;

  if (colorv.tag == EVT_INT || colorv.tag == EVT_FLOAT) {
    int style = (int)ergo_as_int(colorv);
    if (style < 0) style = 0;
    if (style > 3) style = 3;
    n->shape.color_style = style;
    if (style > 0) {
      n->shape.custom_color_set = false;
      n->bg_set = false;
    }
    cogito_window_relayout(cogito_node_window(n));
    return;
  }

  ErgoStr* s = stdr_to_string(colorv);
  const char* text = (s && s->data) ? s->data : "";
  if (strcmp(text, "primary") == 0 || strcmp(text, "Primary") == 0) {
    n->shape.color_style = 1;
    n->shape.custom_color_set = false;
    n->bg_set = false;
  } else if (strcmp(text, "secondary") == 0 || strcmp(text, "Secondary") == 0) {
    n->shape.color_style = 2;
    n->shape.custom_color_set = false;
    n->bg_set = false;
  } else if (strcmp(text, "tertiary") == 0 || strcmp(text, "Tertiary") == 0) {
    n->shape.color_style = 3;
    n->shape.custom_color_set = false;
    n->bg_set = false;
  } else {
    const char* p = text;
    CogitoColor c = cogito_color(24, 24, 33, 255);
    if (cogito_css_parse_color(&p, &c)) {
      n->shape.color_style = 0;
      n->shape.custom_color = c;
      n->shape.custom_color_set = true;
      n->bg = c;
      n->bg_set = true;
    }
  }
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_shape_set_vertex(ErgoVal shapev, ErgoVal idxv, ErgoVal xv, ErgoVal yv) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_set_vertex expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  int idx = (int)ergo_as_int(idxv);
  if (idx < 0 || idx >= COGITO_SHAPE_VERTEX_COUNT) return;
  float x = (float)ergo_as_float(xv);
  float y = (float)ergo_as_float(yv);
  n->shape.vertices[idx * 2 + 0] = cogito_shape_clamp01(x);
  n->shape.vertices[idx * 2 + 1] = cogito_shape_clamp01(y);
  n->shape.svg_poly_count = 0;
  // Vertex edits change paint only; geometry/layout bounds remain unchanged.
  cogito_queue_draw(cogito_node_window(n));
}

static ErgoVal cogito_shape_get_vertex_x(ErgoVal shapev, ErgoVal idxv) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_get_vertex_x expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  int idx = (int)ergo_as_int(idxv);
  if (idx < 0 || idx >= COGITO_SHAPE_VERTEX_COUNT) return EV_FLOAT(0.0);
  return EV_FLOAT(n->shape.vertices[idx * 2 + 0]);
}

static ErgoVal cogito_shape_get_vertex_y(ErgoVal shapev, ErgoVal idxv) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_get_vertex_y expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  int idx = (int)ergo_as_int(idxv);
  if (idx < 0 || idx >= COGITO_SHAPE_VERTEX_COUNT) return EV_FLOAT(0.0);
  return EV_FLOAT(n->shape.vertices[idx * 2 + 1]);
}
