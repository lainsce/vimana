// Native Linux SVG rendering using librsvg + Cairo (full SVG spec support
// including filters, gradients, masks, etc. that nanosvg/SDL3_image drops).
// Returns an RGBA pixel buffer that the caller owns (must free()).

#include <librsvg/rsvg.h>
#include <cairo.h>
#include <stdlib.h>
#include <string.h>

// Render an SVG file at the given pixel dimensions into a newly-allocated
// RGBA8 buffer.  Returns NULL on failure.  Caller must free() the result.
// *out_w and *out_h receive the actual rendered dimensions.
unsigned char* cogito_svg_render_native(const char* path, int target_w, int target_h,
                                        int* out_w, int* out_h) {
    if (!path || !path[0] || target_w <= 0 || target_h <= 0) return NULL;

    GFile* file = g_file_new_for_path(path);
    if (!file) return NULL;

    GError* error = NULL;
    RsvgHandle* handle = rsvg_handle_new_from_gfile_sync(file, RSVG_HANDLE_FLAGS_NONE, NULL, &error);
    g_object_unref(file);
    if (!handle) {
        if (error) g_error_free(error);
        return NULL;
    }

    int w = target_w;
    int h = target_h;

    // Create a Cairo ARGB32 image surface at the target dimensions
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        g_object_unref(handle);
        return NULL;
    }

    cairo_t* cr = cairo_create(surface);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        g_object_unref(handle);
        return NULL;
    }

    // Render the SVG into the viewport
    RsvgRectangle viewport = { .x = 0, .y = 0, .width = (double)w, .height = (double)h };
    rsvg_handle_render_document(handle, cr, &viewport, NULL);

    cairo_destroy(cr);
    cairo_surface_flush(surface);

    // Convert from Cairo premultiplied ARGB32 (native endian) to straight RGBA8
    const unsigned char* src = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    if (!src) {
        cairo_surface_destroy(surface);
        g_object_unref(handle);
        return NULL;
    }

    size_t len = (size_t)w * (size_t)h * 4;
    unsigned char* pixels = (unsigned char*)malloc(len);
    if (!pixels) {
        cairo_surface_destroy(surface);
        g_object_unref(handle);
        return NULL;
    }

    for (int y = 0; y < h; y++) {
        const unsigned char* row = src + y * stride;
        unsigned char* dst = pixels + y * w * 4;
        for (int x = 0; x < w; x++) {
            // Cairo ARGB32 is stored as uint32 in native endian.
            // On little-endian: bytes are [B, G, R, A], premultiplied.
            unsigned char b_pm = row[x * 4 + 0];
            unsigned char g_pm = row[x * 4 + 1];
            unsigned char r_pm = row[x * 4 + 2];
            unsigned char a     = row[x * 4 + 3];

            // Un-premultiply alpha
            if (a == 0) {
                dst[x * 4 + 0] = 0;
                dst[x * 4 + 1] = 0;
                dst[x * 4 + 2] = 0;
                dst[x * 4 + 3] = 0;
            } else if (a == 255) {
                dst[x * 4 + 0] = r_pm;
                dst[x * 4 + 1] = g_pm;
                dst[x * 4 + 2] = b_pm;
                dst[x * 4 + 3] = 255;
            } else {
                dst[x * 4 + 0] = (unsigned char)((r_pm * 255 + a / 2) / a);
                dst[x * 4 + 1] = (unsigned char)((g_pm * 255 + a / 2) / a);
                dst[x * 4 + 2] = (unsigned char)((b_pm * 255 + a / 2) / a);
                dst[x * 4 + 3] = a;
            }
        }
    }

    cairo_surface_destroy(surface);
    g_object_unref(handle);

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    return pixels;
}

// Render SVG from in-memory data (same output contract as above).
unsigned char* cogito_svg_render_native_from_data(const unsigned char* svg_data, size_t svg_len,
                                                   int target_w, int target_h,
                                                   int* out_w, int* out_h) {
    if (!svg_data || svg_len == 0 || target_w <= 0 || target_h <= 0) return NULL;

    GError* error = NULL;
    RsvgHandle* handle = rsvg_handle_new_from_data(svg_data, (gsize)svg_len, &error);
    if (!handle) {
        if (error) g_error_free(error);
        return NULL;
    }

    int w = target_w;
    int h = target_h;

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        g_object_unref(handle);
        return NULL;
    }

    cairo_t* cr = cairo_create(surface);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        g_object_unref(handle);
        return NULL;
    }

    RsvgRectangle viewport = { .x = 0, .y = 0, .width = (double)w, .height = (double)h };
    rsvg_handle_render_document(handle, cr, &viewport, NULL);

    cairo_destroy(cr);
    cairo_surface_flush(surface);

    const unsigned char* src = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    if (!src) {
        cairo_surface_destroy(surface);
        g_object_unref(handle);
        return NULL;
    }

    size_t len = (size_t)w * (size_t)h * 4;
    unsigned char* pixels = (unsigned char*)malloc(len);
    if (!pixels) {
        cairo_surface_destroy(surface);
        g_object_unref(handle);
        return NULL;
    }

    for (int y = 0; y < h; y++) {
        const unsigned char* row = src + y * stride;
        unsigned char* dst = pixels + y * w * 4;
        for (int x = 0; x < w; x++) {
            unsigned char b_pm = row[x * 4 + 0];
            unsigned char g_pm = row[x * 4 + 1];
            unsigned char r_pm = row[x * 4 + 2];
            unsigned char a     = row[x * 4 + 3];

            if (a == 0) {
                dst[x * 4 + 0] = 0;
                dst[x * 4 + 1] = 0;
                dst[x * 4 + 2] = 0;
                dst[x * 4 + 3] = 0;
            } else if (a == 255) {
                dst[x * 4 + 0] = r_pm;
                dst[x * 4 + 1] = g_pm;
                dst[x * 4 + 2] = b_pm;
                dst[x * 4 + 3] = 255;
            } else {
                dst[x * 4 + 0] = (unsigned char)((r_pm * 255 + a / 2) / a);
                dst[x * 4 + 1] = (unsigned char)((g_pm * 255 + a / 2) / a);
                dst[x * 4 + 2] = (unsigned char)((b_pm * 255 + a / 2) / a);
                dst[x * 4 + 3] = a;
            }
        }
    }

    cairo_surface_destroy(surface);
    g_object_unref(handle);

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    return pixels;
}
