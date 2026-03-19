// Objective-C wrapper for autoreleasepool to fix macOS RAM balloon
// Provides both block-scoped wrappers and push/pop API for long-lived pools

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <SDL3/SDL.h>
#import <objc/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Push/Pop autoreleasepool for wrapping the entire render phase
// ============================================================================
// Uses the ObjC runtime's objc_autoreleasePoolPush/Pop which allows
// splitting pool creation and drain across function boundaries.

extern void* objc_autoreleasePoolPush(void);
extern void objc_autoreleasePoolPop(void* pool);

static void* g_frame_pool = NULL;

// Call before begin_frame to create a pool that catches all Metal objects
// created during the draw phase (SDL_RenderFillRect, SDL_RenderGeometry, etc.)
void cogito_autorelease_push(void) {
    g_frame_pool = objc_autoreleasePoolPush();
}

// Call after present to drain all autoreleased Metal objects from the frame
void cogito_autorelease_pop(void) {
    if (g_frame_pool) {
        objc_autoreleasePoolPop(g_frame_pool);
        g_frame_pool = NULL;
    }
}

// ============================================================================
// Block-scoped wrappers for individual SDL calls
// ============================================================================

bool cogito_poll_event_with_autoreleasepool(SDL_Event* event) {
    @autoreleasepool {
        return SDL_PollEvent(event);
    }
}

bool cogito_wait_event_with_autoreleasepool(SDL_Event* event, int timeout_ms) {
    @autoreleasepool {
        return SDL_WaitEventTimeout(event, timeout_ms);
    }
}

void cogito_render_present_with_autoreleasepool(SDL_Renderer* renderer) {
    @autoreleasepool {
        if (renderer) {
            SDL_RenderPresent(renderer);
        }
    }
}

// ============================================================================
// Legacy / unused (kept for compatibility)
// ============================================================================

void cogito_frame_start(void) {
    // No-op: replaced by cogito_autorelease_push/pop
}

// ============================================================================
// macOS Dock icon
// ============================================================================

// Draw the macOS app-icon squircle (continuous-curvature rounded rect).
static NSBezierPath *cogito_macos_squircle_path(NSSize size, NSPoint origin) {
    CGFloat w = size.width, h = size.height;
    CGFloat r = w * 0.25 ; // macOS Tahoe icon corner radius ≈ 25% of size
    return [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(origin.x, origin.y, w, h)
                                           xRadius:r yRadius:r];
}

bool cogito_macos_set_dock_icon(const char *path) {
    if (!path || !path[0]) return false;
    @autoreleasepool {
        NSString *ns_path = [NSString stringWithUTF8String:path];

        // Prefer .icns from the .app bundle — properly rasterised artwork.
        NSString *bundle_path = [[NSBundle mainBundle] bundlePath];
        if ([bundle_path hasSuffix:@".app"]) {
            NSString *res_dir = [bundle_path stringByAppendingPathComponent:@"Contents/Resources"];
            NSString *stem = [[ns_path lastPathComponent] stringByDeletingPathExtension];
            NSString *icns_path = [[res_dir stringByAppendingPathComponent:stem]
                                   stringByAppendingPathExtension:@"icns"];
            if ([[NSFileManager defaultManager] fileExistsAtPath:icns_path]) {
                ns_path = icns_path;
            }
        }

        NSImage *src = [[NSImage alloc] initWithContentsOfFile:ns_path];
        if (!src) return false;

        // Apply squircle mask with proper Dock inset (52/64 of tile size).
        CGFloat canvas = 1024;
        CGFloat icon_sz = canvas * (52.0 / 64.0);  // ≈ 832
        CGFloat pad = (canvas - icon_sz) / 2.0;
        NSSize sz = NSMakeSize(canvas, canvas);
        NSImage *masked = [[NSImage alloc] initWithSize:sz];
        [masked lockFocus];
        NSBezierPath *clip = cogito_macos_squircle_path(
            NSMakeSize(icon_sz, icon_sz), NSMakePoint(pad, pad));
        [clip addClip];
        [src drawInRect:NSMakeRect(pad, pad, icon_sz, icon_sz)
               fromRect:NSZeroRect
              operation:NSCompositingOperationSourceOver
               fraction:1.0];

        // Tahoe-style specular highlight border.
        // A very subtle, nearly uniform thin white inner stroke with gentle
        // top-to-bottom falloff — just enough to separate the icon from its
        // background and give a slight glass-edge feel.
        {
            CGFloat stroke_w = 16.0;  // ≈1px at 64px Dock size
            CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
            CGContextSaveGState(ctx);

            CGRect r = NSMakeRect(pad + stroke_w * 0.5, pad + stroke_w * 0.5,
                                  icon_sz - stroke_w, icon_sz - stroke_w);
            CGFloat cr = (icon_sz - stroke_w) * 0.25;
            CGPathRef rr = CGPathCreateWithRoundedRect(r, cr, cr, NULL);
            CGContextSetLineWidth(ctx, stroke_w);
            CGContextAddPath(ctx, rr);
            CGContextReplacePathWithStrokedPath(ctx);
            CGContextClip(ctx);

            NSGradient *highlight = [[NSGradient alloc]
                initWithColorsAndLocations:
                    [NSColor colorWithWhite:1.0 alpha:0.55], 0.0,
                    [NSColor colorWithWhite:1.0 alpha:0.33], 0.25,
                    [NSColor colorWithWhite:1.0 alpha:0.05], 0.5,
                    [NSColor colorWithWhite:1.0 alpha:0.33], 0.75,
                    [NSColor colorWithWhite:1.0 alpha:0.55], 1.0,
                    nil];
            // angle -45 → top-left to bottom-right diagonal, matching the default macOS icon lighting
            [highlight drawInRect:NSMakeRect(pad, pad, icon_sz, icon_sz) angle:-45];

            CGPathRelease(rr);
            CGContextRestoreGState(ctx);
        }

        [masked unlockFocus];

        [NSApp setApplicationIconImage:masked];
        return true;
    }
}

#ifdef __cplusplus
}
#endif
