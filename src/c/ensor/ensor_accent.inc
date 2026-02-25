// Ensor accent extraction from pixel data.
// Ported from Ensor/Ensor.vala

#define COGITO_ENSOR_MAX_SAMPLES        64
#define COGITO_ENSOR_MAX_QUANTIZE_COLS  32

// Extract up to 4 accent colors from a pixel buffer.
//   pixels    : raw pixel bytes (RGB or RGBA order)
//   n_bytes   : total number of bytes in the pixel buffer
//   alpha     : true if pixels are RGBA (4 bytes/pixel), false for RGB (3 bytes/pixel)
//   out       : array of at least 4 CogitoColor to receive the results
// Returns: number of colors written to out (1..4).
static int cogito_ensor_accent_from_pixels(const unsigned char* pixels,
                                            int n_bytes,
                                            bool alpha,
                                            CogitoColor out[4]) {
  int factor = alpha ? 4 : 3;
  if (!pixels || n_bytes < factor) {
    out[0] = cogito_hct_to_rgb(311.12, 57.36, 40.0); // tau purple fallback
    return 1;
  }

  int total_pixels = n_bytes / factor;
  int skip = total_pixels / COGITO_ENSOR_MAX_SAMPLES;
  if (skip < 1) skip = 1;
  int expected = (total_pixels + skip - 1) / skip;

  // Collect filtered pixels
  int* filtered = (int*)malloc(expected * sizeof(int));
  if (!filtered) {
    out[0] = cogito_hct_to_rgb(311.12, 57.36, 40.0);
    return 1;
  }
  int fcount = 0;

  for (int i = 0; i < total_pixels && fcount < expected; i += skip) {
    int offset = i * factor;
    if (offset + factor > n_bytes) break;
    unsigned char r = pixels[offset];
    unsigned char g = pixels[offset + 1];
    unsigned char b = pixels[offset + 2];

    if (alpha) {
      unsigned char a = pixels[offset + 3];
      if (a < 250) continue; // skip transparent
    }
    if (r < 10 && g < 10 && b < 10) continue;   // skip very dark
    if (r > 245 && g > 245 && b > 245) continue; // skip very light

    filtered[fcount++] = cogito_ensor_pack_argb(r, g, b);
  }

  if (fcount == 0) {
    free(filtered);
    out[0] = cogito_hct_to_rgb(311.12, 57.36, 40.0);
    return 1;
  }

  // Quantize with Celebi
  CogitoEnsorQMap result;
  cogito_ensor_qmap_clear(&result);
  cogito_ensor_quantizer_celebi(filtered, fcount, COGITO_ENSOR_MAX_QUANTIZE_COLS, &result);
  free(filtered);

  // Score to get top colors
  int scored[4];
  int n = cogito_ensor_score(&result, 4, scored);

  // Convert ARGB ints to CogitoColor
  for (int i = 0; i < n; i++) {
    out[i].r = (unsigned char)cogito_ensor_argb_r(scored[i]);
    out[i].g = (unsigned char)cogito_ensor_argb_g(scored[i]);
    out[i].b = (unsigned char)cogito_ensor_argb_b(scored[i]);
    out[i].a = 255;
  }
  return n;
}
