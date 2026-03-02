typedef struct {
  double n;
  double aw;
  double nbb;
  double ncb;
  double c;
  double nc;
  double z;
  double fl;
  double rgb_d[3];
} CogitoCam16Viewing;

static bool cogito_cam16_inited = false;
static CogitoCam16Viewing cogito_cam16 = {0};
static const double cogito_pi = 3.14159265358979323846;
static bool cogito_srgb_lut_inited = false;
static double cogito_srgb_to_linear_lut[256];
static double cogito_srgb_to_linear(double c);

static void cogito_srgb_lut_init(void) {
  if (cogito_srgb_lut_inited) return;
  for (int i = 0; i < 256; i++) {
    double c = (double)i / 255.0;
    cogito_srgb_to_linear_lut[i] = cogito_srgb_to_linear(c);
  }
  cogito_srgb_lut_inited = true;
}

static double cogito_lstar_from_y(double y) {
  double y_n = y / 100.0;
  if (y_n <= 216.0 / 24389.0) return y_n * 903.3;
  return 116.0 * cbrt(y_n) - 16.0;
}

static double cogito_y_from_lstar(double lstar) {
  if (lstar <= 8.0) return (lstar / 903.3) * 100.0;
  double fy = (lstar + 16.0) / 116.0;
  return (fy * fy * fy) * 100.0;
}

static double cogito_srgb_to_linear(double c) {
  if (c <= 0.04045) return c / 12.92;
  return pow((c + 0.055) / 1.055, 2.4);
}

static double cogito_linear_to_srgb(double c) {
  if (c <= 0.0031308) return 12.92 * c;
  return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

static void cogito_xyz_from_srgb(CogitoColor c, double* x, double* y, double* z) {
  cogito_srgb_lut_init();
  double r = cogito_srgb_to_linear_lut[(int)c.r];
  double g = cogito_srgb_to_linear_lut[(int)c.g];
  double b = cogito_srgb_to_linear_lut[(int)c.b];
  *x = 100.0 * (0.41233895 * r + 0.35762064 * g + 0.18051042 * b);
  *y = 100.0 * (0.2126 * r + 0.7152 * g + 0.0722 * b);
  *z = 100.0 * (0.01932141 * r + 0.11916382 * g + 0.95034478 * b);
}

static bool cogito_srgb_from_xyz(double x, double y, double z, CogitoColor* out) {
  double xr = x / 100.0;
  double yr = y / 100.0;
  double zr = z / 100.0;
  double r = 3.241377479 * xr + -1.537665240 * yr + -0.498853668 * zr;
  double g = -0.969145251 * xr + 1.875885345 * yr + 0.041565856 * zr;
  double b = 0.055620936 * xr + -0.203955245 * yr + 1.057179911 * zr;
  r = cogito_linear_to_srgb(r);
  g = cogito_linear_to_srgb(g);
  b = cogito_linear_to_srgb(b);
  if (r < 0.0 || r > 1.0 || g < 0.0 || g > 1.0 || b < 0.0 || b > 1.0) return false;
  out->r = (unsigned char)(r * 255.0 + 0.5);
  out->g = (unsigned char)(g * 255.0 + 0.5);
  out->b = (unsigned char)(b * 255.0 + 0.5);
  out->a = 255;
  return true;
}

static void cogito_cam16_init(void) {
  if (cogito_cam16_inited) return;
  cogito_cam16_inited = true;
  double la = 64.0;
  double yb = 20.0;
  double yw = 100.0;
  double f = 1.0;
  double c = 0.69;
  double nc = 1.0;
  double n = yb / yw;
  double z = 1.48 + sqrt(n);
  double nbb = 0.725 / pow(n, 0.2);
  double ncb = nbb;
  double k = 1.0 / (5.0 * la + 1.0);
  double k4 = k * k * k * k;
  double fl = 0.2 * k4 * 5.0 * la + 0.1 * pow(1.0 - k4, 2.0) * cbrt(5.0 * la);
  double d = f * (1.0 - (1.0 / 3.6) * exp((-la - 42.0) / 92.0));
  if (d > 1.0) d = 1.0;
  if (d < 0.0) d = 0.0;
  double xw = 95.047;
  double zw = 108.883;
  double r_w = 0.401288 * xw + 0.650173 * yw + -0.051461 * zw;
  double g_w = -0.250268 * xw + 1.204414 * yw + 0.045854 * zw;
  double b_w = -0.002079 * xw + 0.048952 * yw + 0.953127 * zw;
  double dr = d * yw / r_w + 1.0 - d;
  double dg = d * yw / g_w + 1.0 - d;
  double db = d * yw / b_w + 1.0 - d;
  double r_wc = dr * r_w;
  double g_wc = dg * g_w;
  double b_wc = db * b_w;
  double r_f = pow(fl * fabs(r_wc) / 100.0, 0.42);
  double g_f = pow(fl * fabs(g_wc) / 100.0, 0.42);
  double b_f = pow(fl * fabs(b_wc) / 100.0, 0.42);
  double r_a = (r_wc >= 0 ? 1.0 : -1.0) * (400.0 * r_f / (r_f + 27.13)) + 0.1;
  double g_a = (g_wc >= 0 ? 1.0 : -1.0) * (400.0 * g_f / (g_f + 27.13)) + 0.1;
  double b_a = (b_wc >= 0 ? 1.0 : -1.0) * (400.0 * b_f / (b_f + 27.13)) + 0.1;
  double aw = (2.0 * r_a + g_a + 0.05 * b_a - 0.305) * nbb;
  cogito_cam16.n = n;
  cogito_cam16.aw = aw;
  cogito_cam16.nbb = nbb;
  cogito_cam16.ncb = ncb;
  cogito_cam16.c = c;
  cogito_cam16.nc = nc;
  cogito_cam16.z = z;
  cogito_cam16.fl = fl;
  cogito_cam16.rgb_d[0] = dr;
  cogito_cam16.rgb_d[1] = dg;
  cogito_cam16.rgb_d[2] = db;
}

typedef struct {
  double h;
  double c;
} CogitoCam16;

static CogitoCam16 cogito_cam16_from_xyz(double x, double y, double z) {
  cogito_cam16_init();
  const CogitoCam16Viewing* vc = &cogito_cam16;
  double r = 0.401288 * x + 0.650173 * y + -0.051461 * z;
  double g = -0.250268 * x + 1.204414 * y + 0.045854 * z;
  double b = -0.002079 * x + 0.048952 * y + 0.953127 * z;
  r *= vc->rgb_d[0];
  g *= vc->rgb_d[1];
  b *= vc->rgb_d[2];
  double r_f = pow(vc->fl * fabs(r) / 100.0, 0.42);
  double g_f = pow(vc->fl * fabs(g) / 100.0, 0.42);
  double b_f = pow(vc->fl * fabs(b) / 100.0, 0.42);
  double r_a = (r >= 0 ? 1.0 : -1.0) * (400.0 * r_f / (r_f + 27.13)) + 0.1;
  double g_a = (g >= 0 ? 1.0 : -1.0) * (400.0 * g_f / (g_f + 27.13)) + 0.1;
  double b_a = (b >= 0 ? 1.0 : -1.0) * (400.0 * b_f / (b_f + 27.13)) + 0.1;
  double a = (11.0 * r_a - 12.0 * g_a + b_a) / 11.0;
  double bb = (r_a + g_a - 2.0 * b_a) / 9.0;
  double u = (20.0 * r_a + 20.0 * g_a + 21.0 * b_a) / 20.0;
  double h = atan2(bb, a) * (180.0 / cogito_pi);
  if (h < 0.0) h += 360.0;
  double h_rad = h * (cogito_pi / 180.0);
  double e_h = 0.25 * (cos(h_rad + 2.0) + 3.8);
  double A = (2.0 * r_a + g_a + 0.05 * b_a - 0.305) * vc->nbb;
  double J = 100.0 * pow(A / vc->aw, vc->c * vc->z);
  double t = (50000.0 / 13.0) * vc->nc * vc->ncb * e_h * sqrt(a * a + bb * bb) / (u + 0.305);
  double alpha = pow(t, 0.9) * pow(1.64 - pow(0.29, vc->n), 0.73);
  double C = alpha * sqrt(J / 100.0);
  CogitoCam16 cam = {h, C};
  return cam;
}

static void cogito_cam16_to_xyz(double J, double C, double h, double* x, double* y, double* z) {
  cogito_cam16_init();
  const CogitoCam16Viewing* vc = &cogito_cam16;
  double h_rad = h * (cogito_pi / 180.0);
  double e_h = 0.25 * (cos(h_rad + 2.0) + 3.8);
  double A = vc->aw * pow(J / 100.0, 1.0 / (vc->c * vc->z));
  double t = (C <= 0.0 || J <= 0.0) ? 0.0
    : pow(C / (sqrt(J / 100.0) * pow(1.64 - pow(0.29, vc->n), 0.73)), 1.0 / 0.9);
  double p1 = (50000.0 / 13.0) * vc->nc * vc->ncb * e_h / (t > 0.0 ? t : 1e-6);
  double p2 = A / vc->nbb + 0.305;
  double sin_h = sin(h_rad);
  double cos_h = cos(h_rad);
  double gamma = (p2 * 23.0) / (23.0 * p1 + 11.0 * cos_h + 108.0 * sin_h);
  double a = gamma * cos_h;
  double bb = gamma * sin_h;
  double r_a = (460.0 * p2 + 451.0 * a + 288.0 * bb) / 1403.0;
  double g_a = (460.0 * p2 - 891.0 * a - 261.0 * bb) / 1403.0;
  double b_a = (460.0 * p2 - 220.0 * a - 6300.0 * bb) / 1403.0;
  double r_c = (r_a - 0.1);
  double g_c = (g_a - 0.1);
  double b_c = (b_a - 0.1);
  double r_c_abs = fabs(r_c);
  double g_c_abs = fabs(g_c);
  double b_c_abs = fabs(b_c);
  if (r_c_abs >= 399.999) r_c_abs = 399.999;
  if (g_c_abs >= 399.999) g_c_abs = 399.999;
  if (b_c_abs >= 399.999) b_c_abs = 399.999;
  double r = (r_c >= 0 ? 1.0 : -1.0) * 100.0 / vc->fl * pow((27.13 * r_c_abs) / (400.0 - r_c_abs), 1.0 / 0.42);
  double g = (g_c >= 0 ? 1.0 : -1.0) * 100.0 / vc->fl * pow((27.13 * g_c_abs) / (400.0 - g_c_abs), 1.0 / 0.42);
  double b = (b_c >= 0 ? 1.0 : -1.0) * 100.0 / vc->fl * pow((27.13 * b_c_abs) / (400.0 - b_c_abs), 1.0 / 0.42);
  r /= vc->rgb_d[0];
  g /= vc->rgb_d[1];
  b /= vc->rgb_d[2];
  *x = 1.86206786 * r + -1.01125463 * g + 0.14918677 * b;
  *y = 0.38752654 * r + 0.62144744 * g + -0.00897398 * b;
  *z = -0.01584150 * r + -0.03412294 * g + 1.04996444 * b;
}

static bool cogito_find_hct_color(double h, double c, double t, CogitoColor* out) {
  double low = 0.0;
  double high = 100.0;
  CogitoColor best = {0, 0, 0, 255};
  bool best_ok = false;
  double best_delta = 1e9;
  const double j_eps = 0.02;
  const double tone_eps = 0.02;
  for (int i = 0; i < 14; i++) {
    double mid = (low + high) * 0.5;
    double x = 0.0, y = 0.0, z = 0.0;
    cogito_cam16_to_xyz(mid, c, h, &x, &y, &z);
    double lstar = cogito_lstar_from_y(y);
    CogitoColor rgb = {0, 0, 0, 255};
    bool in_gamut = cogito_srgb_from_xyz(x, y, z, &rgb);
    if (in_gamut) {
      double delta = fabs(lstar - t);
      if (delta < best_delta) {
        best = rgb;
        best_delta = delta;
        best_ok = true;
      }
      if (delta <= tone_eps) break;
    }
    if (lstar < t) low = mid;
    else high = mid;
    if ((high - low) <= j_eps) break;
  }
  if (!best_ok) return false;
  *out = best;
  return true;
}

static CogitoColor cogito_hct_to_rgb(double h, double c, double t) {
  if (t <= 0.0) return (CogitoColor){0, 0, 0, 255};
  if (t >= 100.0) return (CogitoColor){255, 255, 255, 255};
  if (c < 1e-4) {
    double y = cogito_y_from_lstar(t);
    CogitoColor gray = {0, 0, 0, 255};
    double x = y * 0.95047;
    double z = y * 1.08883;
    cogito_srgb_from_xyz(x, y, z, &gray);
    return gray;
  }
  double max_c = c;
  double min_c = 0.0;
  CogitoColor best = {0, 0, 0, 255};
  bool found = false;
  const double c_eps = 0.02;
  for (int i = 0; i < 16; i++) {
    double mid = (min_c + max_c) * 0.5;
    CogitoColor rgb = {0, 0, 0, 255};
    if (cogito_find_hct_color(h, mid, t, &rgb)) {
      best = rgb;
      found = true;
      min_c = mid;
    } else {
      max_c = mid;
    }
    if ((max_c - min_c) <= c_eps) break;
  }
  if (!found) {
    double y = cogito_y_from_lstar(t);
    CogitoColor gray = {0, 0, 0, 255};
    double x = y * 0.95047;
    double z = y * 1.08883;
    cogito_srgb_from_xyz(x, y, z, &gray);
    return gray;
  }
  return best;
}

static void cogito_rgb_to_hct(CogitoColor c, double* out_h, double* out_c, double* out_t) {
  double x = 0.0, y = 0.0, z = 0.0;
  cogito_xyz_from_srgb(c, &x, &y, &z);
  CogitoCam16 cam = cogito_cam16_from_xyz(x, y, z);
  double t = cogito_lstar_from_y(y);
  if (out_h) *out_h = cam.h;
  if (out_c) *out_c = cam.c;
  if (out_t) *out_t = t;
}

static void cogito_color_to_hex(CogitoColor c, char out[8]) {
  snprintf(out, 8, "#%02X%02X%02X", c.r, c.g, c.b);
}

static bool cogito_hex_to_color(const char* s, CogitoColor* out) {
  if (!s || !out) return false;
  if (s[0] == '#') s++;
  if (strlen(s) < 6) return false;
  unsigned int r = 0, g = 0, b = 0;
  if (sscanf(s, "%02x%02x%02x", &r, &g, &b) != 3) return false;
  *out = (CogitoColor){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
  return true;
}
