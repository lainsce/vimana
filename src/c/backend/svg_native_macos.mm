// Native macOS SVG rendering using NSImage (full SVG spec support including
// filters, gradients, masks, etc. that nanosvg/SDL3_image drops).
// Returns an RGBA pixel buffer that the caller owns (must free()).

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#ifdef __cplusplus
extern "C" {
#endif

// Render an SVG file at the given pixel dimensions into a newly-allocated
// RGBA8 buffer.  Returns NULL on failure.  Caller must free() the result.
// *out_w and *out_h receive the actual rendered dimensions.
unsigned char* cogito_svg_render_native(const char* path, int target_w, int target_h,
                                        int* out_w, int* out_h) {
    if (!path || !path[0] || target_w <= 0 || target_h <= 0) return NULL;

    @autoreleasepool {
        NSString* nsPath = [NSString stringWithUTF8String:path];
        if (!nsPath) return NULL;

        NSImage* image = [[NSImage alloc] initWithContentsOfFile:nsPath];
        if (!image || NSEqualSizes(image.size, NSZeroSize)) return NULL;

        int w = target_w;
        int h = target_h;

        // Create an RGBA bitmap context — must use premultiplied alpha
        // because macOS SVG rendering produces blank output with
        // NSBitmapFormatAlphaNonpremultiplied.
        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:NULL
                          pixelsWide:w
                          pixelsHigh:h
                       bitsPerSample:8
                     samplesPerPixel:4
                            hasAlpha:YES
                            isPlanar:NO
                      colorSpaceName:NSCalibratedRGBColorSpace
                        bitmapFormat:0  // premultiplied alpha
                         bytesPerRow:w * 4
                        bitsPerPixel:32];
        if (!rep) return NULL;

        [NSGraphicsContext saveGraphicsState];
        NSGraphicsContext* ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
        [NSGraphicsContext setCurrentContext:ctx];

        NSRect drawRect = NSMakeRect(0, 0, w, h);
        [image drawInRect:drawRect
                 fromRect:NSZeroRect
                operation:NSCompositingOperationSourceOver
                 fraction:1.0];

        [NSGraphicsContext restoreGraphicsState];

        unsigned char* src = [rep bitmapData];
        if (!src) return NULL;

        size_t len = (size_t)w * (size_t)h * 4;
        unsigned char* pixels = (unsigned char*)malloc(len);
        if (!pixels) return NULL;

        // Un-premultiply alpha: the bitmap is premultiplied but the rest
        // of the pipeline (SDL texture with SDL_BLENDMODE_BLEND) expects
        // straight alpha.
        for (size_t i = 0; i < (size_t)w * (size_t)h; i++) {
            unsigned char r = src[i*4], g = src[i*4+1], b = src[i*4+2], a = src[i*4+3];
            if (a > 0 && a < 255) {
                pixels[i*4]   = (unsigned char)((r * 255 + a/2) / a);
                pixels[i*4+1] = (unsigned char)((g * 255 + a/2) / a);
                pixels[i*4+2] = (unsigned char)((b * 255 + a/2) / a);
            } else {
                pixels[i*4]   = r;
                pixels[i*4+1] = g;
                pixels[i*4+2] = b;
            }
            pixels[i*4+3] = a;
        }

        if (out_w) *out_w = w;
        if (out_h) *out_h = h;
        return pixels;
    }
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Render SVG from in-memory data (same output contract as above).
unsigned char* cogito_svg_render_native_from_data(const unsigned char* svg_data, size_t svg_len,
                                                   int target_w, int target_h,
                                                   int* out_w, int* out_h) {
    if (!svg_data || svg_len == 0 || target_w <= 0 || target_h <= 0) return NULL;

    @autoreleasepool {
        NSData* nsData = [NSData dataWithBytesNoCopy:(void*)svg_data length:svg_len freeWhenDone:NO];
        if (!nsData) return NULL;

        NSImage* image = [[NSImage alloc] initWithData:nsData];
        if (!image || NSEqualSizes(image.size, NSZeroSize)) return NULL;

        int w = target_w;
        int h = target_h;

        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:NULL
                          pixelsWide:w
                          pixelsHigh:h
                       bitsPerSample:8
                     samplesPerPixel:4
                            hasAlpha:YES
                            isPlanar:NO
                      colorSpaceName:NSCalibratedRGBColorSpace
                        bitmapFormat:0  // premultiplied alpha
                         bytesPerRow:w * 4
                        bitsPerPixel:32];
        if (!rep) return NULL;

        [NSGraphicsContext saveGraphicsState];
        NSGraphicsContext* ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
        [NSGraphicsContext setCurrentContext:ctx];

        NSRect drawRect = NSMakeRect(0, 0, w, h);
        [image drawInRect:drawRect
                 fromRect:NSZeroRect
                operation:NSCompositingOperationSourceOver
                 fraction:1.0];

        [NSGraphicsContext restoreGraphicsState];

        unsigned char* src = [rep bitmapData];
        if (!src) return NULL;

        size_t len = (size_t)w * (size_t)h * 4;
        unsigned char* pixels = (unsigned char*)malloc(len);
        if (!pixels) return NULL;

        // Un-premultiply alpha
        for (size_t i = 0; i < (size_t)w * (size_t)h; i++) {
            unsigned char r = src[i*4], g = src[i*4+1], b = src[i*4+2], a = src[i*4+3];
            if (a > 0 && a < 255) {
                pixels[i*4]   = (unsigned char)((r * 255 + a/2) / a);
                pixels[i*4+1] = (unsigned char)((g * 255 + a/2) / a);
                pixels[i*4+2] = (unsigned char)((b * 255 + a/2) / a);
            } else {
                pixels[i*4]   = r;
                pixels[i*4+1] = g;
                pixels[i*4+2] = b;
            }
            pixels[i*4+3] = a;
        }

        if (out_w) *out_w = w;
        if (out_h) *out_h = h;
        return pixels;
    }
}

#ifdef __cplusplus
}
#endif
