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

void* vimana_app_autorelease_push(void) {
    return objc_autoreleasePoolPush();
}

void vimana_app_autorelease_pop(void* pool) {
    if (pool) {
        objc_autoreleasePoolPop(pool);
    }
}

// Call before begin_frame to create a pool that catches all Metal objects
// created during the draw phase (SDL_RenderFillRect, SDL_RenderGeometry, etc.)
void vimana_autorelease_push(void) {
    g_frame_pool = objc_autoreleasePoolPush();
}

// Call after present to drain all autoreleased Metal objects from the frame
void vimana_autorelease_pop(void) {
    if (g_frame_pool) {
        objc_autoreleasePoolPop(g_frame_pool);
        g_frame_pool = NULL;
    }
}

// ============================================================================
// Block-scoped wrappers for individual SDL calls
// ============================================================================

bool vimana_poll_event_with_autoreleasepool(SDL_Event* event) {
    @autoreleasepool {
        return SDL_PollEvent(event);
    }
}

bool vimana_wait_event_with_autoreleasepool(SDL_Event* event, int timeout_ms) {
    @autoreleasepool {
        return SDL_WaitEventTimeout(event, timeout_ms);
    }
}

void vimana_render_present_with_autoreleasepool(SDL_Renderer* renderer) {
    @autoreleasepool {
        if (renderer) {
            SDL_RenderPresent(renderer);
        }
    }
}
}
