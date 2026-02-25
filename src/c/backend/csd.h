// Cogito Client-Side Decorations (CSD) Interface
// Provides hit-testing for borderless windows with custom title bars

#ifndef COGITO_CSD_H
#define COGITO_CSD_H

#include "backend.h"
#include <SDL3/SDL.h>

// Hit-test region types
typedef enum {
    COGITO_HITTEST_NORMAL = 0,           // Normal window content
    COGITO_HITTEST_DRAGGABLE,            // Draggable title area
    COGITO_HITTEST_RESIZE_TOPLEFT,     // Resize handle - top-left
    COGITO_HITTEST_RESIZE_TOP,           // Resize handle - top
    COGITO_HITTEST_RESIZE_TOPRIGHT,      // Resize handle - top-right
    COGITO_HITTEST_RESIZE_RIGHT,         // Resize handle - right
    COGITO_HITTEST_RESIZE_BOTTOMRIGHT,   // Resize handle - bottom-right
    COGITO_HITTEST_RESIZE_BOTTOM,        // Resize handle - bottom
    COGITO_HITTEST_RESIZE_BOTTOMLEFT,    // Resize handle - bottom-left
    COGITO_HITTEST_RESIZE_LEFT,          // Resize handle - left
    COGITO_HITTEST_BUTTON_CLOSE,         // Close button (clickable, not draggable)
    COGITO_HITTEST_BUTTON_MIN,           // Minimize button
    COGITO_HITTEST_BUTTON_MAX,           // Maximize button
} CogitoHitTestResult;

// CSD region configuration
typedef struct {
    int border_size;          // Size of resize borders (default: 8px)
    int button_size;          // Size of window control buttons
    int button_gap;           // Gap between buttons
    int title_height;         // Height of title bar
    int padding;              // Padding inside title bar
    
    // Button positions (relative to window)
    int close_btn_x;
    int close_btn_y;
    int min_btn_x;
    int min_btn_y;
    int max_btn_x;
    int max_btn_y;
    
    // Title area (draggable region)
    int title_x;
    int title_y;
    int title_w;
    int title_h;
} CogitoCSDConfig;

// CSD state per window
typedef struct {
    bool enabled;                    // CSD enabled for this window
    CogitoCSDConfig config;          // CSD configuration
    bool debug_overlay;                // Show debug overlay
    bool has_appbar;                 // Window has appbar widget
    
    // Cached hit regions for debug overlay
    CogitoRect resize_borders[8];    // 8 resize border regions
    CogitoRect draggable_area;       // Draggable title area
    CogitoRect button_areas[3];      // Close, min, max button areas
} CogitoCSDState;

// Initialize CSD for a window
void cogito_csd_init(CogitoCSDState* csd, bool has_appbar);

// Configure CSD with custom settings
void cogito_csd_configure(CogitoCSDState* csd, const CogitoCSDConfig* config);

// Perform hit test at given coordinates
// Returns hit test result and fills out region info if provided
CogitoHitTestResult cogito_csd_hit_test(CogitoCSDState* csd, int x, int y, int window_w, int window_h);

// Update button positions based on appbar layout
void cogito_csd_update_button_positions(CogitoCSDState* csd, int close_x, int close_y, 
                                        int min_x, int min_y, int max_x, int max_y,
                                        int btn_size);

// Enable/disable debug overlay
void cogito_csd_set_debug_overlay(CogitoCSDState* csd, bool enable);

// Draw debug overlay showing hit regions
void cogito_csd_draw_debug_overlay(CogitoCSDState* csd, CogitoBackend* backend);

// Get hit test result as SDL hit test result for SDL_SetWindowHitTest
// Returns SDL_HitTestResult value (defined in SDL.h)
int cogito_csd_to_sdl_hit_test(CogitoHitTestResult result);

#endif // COGITO_CSD_H
