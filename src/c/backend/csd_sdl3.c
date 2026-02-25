// Cogito Client-Side Decorations (CSD) Implementation for SDL3

#include "csd.h"
#include "backend.h"
#include <SDL3/SDL.h>
#include <string.h>

#ifndef COGITO_CSD_BORDER_SIZE
#define COGITO_CSD_BORDER_SIZE 8
#endif

#ifndef COGITO_CSD_BUTTON_SIZE
#define COGITO_CSD_BUTTON_SIZE 12
#endif

#ifndef COGITO_CSD_BUTTON_GAP
#define COGITO_CSD_BUTTON_GAP 8
#endif

void cogito_csd_init(CogitoCSDState* csd, bool has_appbar) {
    if (!csd) return;
    memset(csd, 0, sizeof(CogitoCSDState));
    
    csd->enabled = has_appbar;  // Only enable CSD if window has appbar
    csd->has_appbar = has_appbar;
    csd->debug_overlay = false;
    
    // Default configuration
    csd->config.border_size = COGITO_CSD_BORDER_SIZE;
    csd->config.button_size = COGITO_CSD_BUTTON_SIZE;
    csd->config.button_gap = COGITO_CSD_BUTTON_GAP;
    csd->config.title_height = 32;
    csd->config.padding = 8;
}

void cogito_csd_configure(CogitoCSDState* csd, const CogitoCSDConfig* config) {
    if (!csd || !config) return;
    csd->config = *config;
}

void cogito_csd_update_button_positions(CogitoCSDState* csd, int close_x, int close_y, 
                                        int min_x, int min_y, int max_x, int max_y,
                                        int btn_size) {
    if (!csd) return;
    
    csd->config.close_btn_x = close_x;
    csd->config.close_btn_y = close_y;
    csd->config.min_btn_x = min_x;
    csd->config.min_btn_y = min_y;
    csd->config.max_btn_x = max_x;
    csd->config.max_btn_y = max_y;
    csd->config.button_size = btn_size;
    
    // Update button areas for hit testing
    int bs = btn_size > 0 ? btn_size : COGITO_CSD_BUTTON_SIZE;
    
    csd->button_areas[0] = (CogitoRect){close_x, close_y, bs, bs};  // Close
    csd->button_areas[1] = (CogitoRect){min_x, min_y, bs, bs};     // Min
    csd->button_areas[2] = (CogitoRect){max_x, max_y, bs, bs};      // Max
    
    // Update title area (draggable region between buttons and left edge)
    int leftmost_btn = close_x;
    if (min_x < leftmost_btn) leftmost_btn = min_x;
    if (max_x < leftmost_btn) leftmost_btn = max_x;
    
    csd->config.title_x = csd->config.border_size;
    csd->config.title_y = csd->config.border_size;
    csd->config.title_w = leftmost_btn - csd->config.border_size - csd->config.padding;
    csd->config.title_h = csd->config.title_height;
    
    csd->draggable_area = (CogitoRect){
        csd->config.title_x, 
        csd->config.title_y, 
        csd->config.title_w, 
        csd->config.title_h
    };
}

CogitoHitTestResult cogito_csd_hit_test(CogitoCSDState* csd, int x, int y, int window_w, int window_h) {
    if (!csd || !csd->enabled) return COGITO_HITTEST_NORMAL;
    if (window_w <= 0 || window_h <= 0) return COGITO_HITTEST_NORMAL;
    
    int bs = csd->config.border_size > 0 ? csd->config.border_size : COGITO_CSD_BORDER_SIZE;
    
    // Check button areas first (highest priority after resize borders)
    for (int i = 0; i < 3; i++) {
        CogitoRect* btn = &csd->button_areas[i];
        if (btn->w > 0 && btn->h > 0 &&
            x >= btn->x && x < btn->x + btn->w &&
            y >= btn->y && y < btn->y + btn->h) {
            switch (i) {
                case 0: return COGITO_HITTEST_BUTTON_CLOSE;
                case 1: return COGITO_HITTEST_BUTTON_MIN;
                case 2: return COGITO_HITTEST_BUTTON_MAX;
            }
        }
    }
    
    // Check resize borders (8px from edges)
    bool in_left = x < bs;
    bool in_right = x >= window_w - bs;
    bool in_top = y < bs;
    bool in_bottom = y >= window_h - bs;
    
    if (in_top && in_left) return COGITO_HITTEST_RESIZE_TOPLEFT;
    if (in_top && in_right) return COGITO_HITTEST_RESIZE_TOPRIGHT;
    if (in_bottom && in_left) return COGITO_HITTEST_RESIZE_BOTTOMLEFT;
    if (in_bottom && in_right) return COGITO_HITTEST_RESIZE_BOTTOMRIGHT;
    if (in_top) return COGITO_HITTEST_RESIZE_TOP;
    if (in_bottom) return COGITO_HITTEST_RESIZE_BOTTOM;
    if (in_left) return COGITO_HITTEST_RESIZE_LEFT;
    if (in_right) return COGITO_HITTEST_RESIZE_RIGHT;
    
    // Check draggable title area
    if (csd->has_appbar && 
        x >= csd->draggable_area.x && 
        x < csd->draggable_area.x + csd->draggable_area.w &&
        y >= csd->draggable_area.y && 
        y < csd->draggable_area.y + csd->draggable_area.h) {
        return COGITO_HITTEST_DRAGGABLE;
    }
    
    return COGITO_HITTEST_NORMAL;
}

int cogito_csd_to_sdl_hit_test(CogitoHitTestResult result) {
    switch (result) {
        case COGITO_HITTEST_NORMAL: return SDL_HITTEST_NORMAL;
        case COGITO_HITTEST_DRAGGABLE: return SDL_HITTEST_DRAGGABLE;
        case COGITO_HITTEST_RESIZE_TOPLEFT: return SDL_HITTEST_RESIZE_TOPLEFT;
        case COGITO_HITTEST_RESIZE_TOP: return SDL_HITTEST_RESIZE_TOP;
        case COGITO_HITTEST_RESIZE_TOPRIGHT: return SDL_HITTEST_RESIZE_TOPRIGHT;
        case COGITO_HITTEST_RESIZE_RIGHT: return SDL_HITTEST_RESIZE_RIGHT;
        case COGITO_HITTEST_RESIZE_BOTTOMRIGHT: return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
        case COGITO_HITTEST_RESIZE_BOTTOM: return SDL_HITTEST_RESIZE_BOTTOM;
        case COGITO_HITTEST_RESIZE_BOTTOMLEFT: return SDL_HITTEST_RESIZE_BOTTOMLEFT;
        case COGITO_HITTEST_RESIZE_LEFT: return SDL_HITTEST_RESIZE_LEFT;
        // Buttons are treated as normal (clickable but not draggable)
        case COGITO_HITTEST_BUTTON_CLOSE:
        case COGITO_HITTEST_BUTTON_MIN:
        case COGITO_HITTEST_BUTTON_MAX:
            return SDL_HITTEST_NORMAL;
        default: return SDL_HITTEST_NORMAL;
    }
}

void cogito_csd_set_debug_overlay(CogitoCSDState* csd, bool enable) {
    if (!csd) return;
    csd->debug_overlay = enable;
}

void cogito_csd_draw_debug_overlay(CogitoCSDState* csd, CogitoBackend* backend) {
    if (!csd || !csd->debug_overlay || !backend) return;
    
    // Blue: Resize borders
    CogitoColor blue = {0, 100, 255, 128};
    
    // Green: Draggable area
    CogitoColor green = {0, 255, 100, 128};
    
    // Red: Button areas
    CogitoColor red = {255, 50, 50, 180};
    
    int bs = csd->config.border_size > 0 ? csd->config.border_size : COGITO_CSD_BORDER_SIZE;
    int w = csd->config.title_x + csd->config.title_w + bs * 2;
    int h = csd->config.title_y + csd->config.title_h + bs * 2;
    
    // Draw resize borders (blue)
    // Top
    backend->draw_rect(0, 0, w, bs, blue);
    // Bottom
    backend->draw_rect(0, h - bs, w, bs, blue);
    // Left
    backend->draw_rect(0, bs, bs, h - bs * 2, blue);
    // Right
    backend->draw_rect(w - bs, bs, bs, h - bs * 2, blue);
    
    // Draw draggable area (green)
    if (csd->draggable_area.w > 0 && csd->draggable_area.h > 0) {
        backend->draw_rect(csd->draggable_area.x, csd->draggable_area.y,
                          csd->draggable_area.w, csd->draggable_area.h, green);
    }
    
    // Draw button areas (red)
    for (int i = 0; i < 3; i++) {
        CogitoRect* btn = &csd->button_areas[i];
        if (btn->w > 0 && btn->h > 0) {
            backend->draw_rect(btn->x, btn->y, btn->w, btn->h, red);
        }
    }
}
