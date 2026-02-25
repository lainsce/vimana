// ---- Cogito GUI (SDL3) ----

#include "backend.h"
#include <SDL3/SDL.h> // Required for SDL_GetPerformanceCounter (spring dt) and other SDL types
#if defined(__APPLE__)
#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

// Versioning: numeric for build/config, user-visible string for titles and UI
#define COGITO_VERSION "1.0"
#define COGITO_USER_VERSION "1.0 \"B-17 Bomber\""

static bool sum_system_is_dark(void);
static bool sum_system_accent(CogitoColor* out);
static int sum_system_ensor_scheme(void);
static double sum_system_font_weight(void);
static double sum_system_roundness(void);
static double sum_system_contrast(void);
static int sum_system_spacing(void);

static bool cogito_system_is_dark(void) {
  return sum_system_is_dark();
}

static bool cogito_system_accent(CogitoColor* out) {
  if (sum_system_accent(out)) return true;
  if (out) {
    *out = cogito_color(114,
    222,
    194,
    255);
  }
  return false;
}

static int cogito_system_ensor_scheme(void) {
  return sum_system_ensor_scheme();
}

static double cogito_system_font_weight(void) {
  return sum_system_font_weight();
}

static double cogito_system_roundness(void) {
  return sum_system_roundness();
}

static double cogito_system_contrast(void) {
  return sum_system_contrast();
}

static int cogito_system_spacing(void) {
  return sum_system_spacing();
}

static void cogito_apply_theme(bool dark, CogitoColor accent);

// Animation Duration Constants (milliseconds)
#define COGITO_ANIM_DURATION_SHORT1 50 // Very fast (fab close, etc.)
#define COGITO_ANIM_DURATION_SHORT2 100 // Fast (switch toggle)
#define COGITO_ANIM_DURATION_SHORT3 150 // Quick (checkbox)
#define COGITO_ANIM_DURATION_SHORT4 200 // Standard (button state)
#define COGITO_ANIM_DURATION_MEDIUM1 250 // Slow (dropdown open)
#define COGITO_ANIM_DURATION_MEDIUM2 300 // Slower (dialog)
#define COGITO_ANIM_DURATION_MEDIUM3 350 // Medium (page transition)
#define COGITO_ANIM_DURATION_MEDIUM4 400
#define COGITO_ANIM_DURATION_LONG1 450
#define COGITO_ANIM_DURATION_LONG2 500
#define COGITO_ANIM_DURATION_LONG3 550
#define COGITO_ANIM_DURATION_LONG4 600
#define COGITO_ACTIVE_LATCH_MS 100 // Keep pressed visual briefly to avoid click flash
#define COGITO_BOTTOM_SHEET_DEFAULT_RATIO 0.60f
#define COGITO_BOTTOM_SHEET_TOP_INSET 56
#define COGITO_BOTTOM_SHEET_HANDLE_TOUCH_H 32
#define COGITO_SIDE_SHEET_DEFAULT_RATIO 0.40f
#define COGITO_SIDE_SHEET_LEFT_INSET 0
#define COGITO_SIDE_SHEET_HANDLE_TOUCH_W 32
// Nav Rail Constants (dp)
// Shared between collapsed and expanded modes
#define COGITO_NAV_RAIL_SHARED_TOGGLE_BUTTON_SIZE 48
#define COGITO_NAV_RAIL_SHARED_TOGGLE_ICON_SIZE 24
#define COGITO_NAV_RAIL_SHARED_ITEM_ICON_SIZE 24
// Collapsed mode
#define COGITO_NAV_RAIL_COLLAPSED_WIDTH 96
#define COGITO_NAV_RAIL_COLLAPSED_TOP_OFFSET 24
#define COGITO_NAV_RAIL_COLLAPSED_SECTION_GAP 36
#define COGITO_NAV_RAIL_COLLAPSED_ITEM_STEP 96
#define COGITO_NAV_RAIL_COLLAPSED_INDICATOR_WIDTH 64 // Cogito Expressive: 64dp pill indicator
#ifndef COGITO_SHAPE_VERTEX_COUNT
#define COGITO_SHAPE_VERTEX_COUNT 128 // Max vertices for custom shapes (e.g. from icon paths)
#endif
#ifndef COGITO_SHAPE_SVG_POLY_MAX
#define COGITO_SHAPE_SVG_POLY_MAX 1024 // Max vertices for SVG polygon shapes (e.g. from icon paths)
#endif
#ifndef COGITO_DRAWING_AREA_MAX_POINTS
#define COGITO_DRAWING_AREA_MAX_POINTS 8192
#endif
#define COGITO_NAV_RAIL_COLLAPSED_INDICATOR_HEIGHT 32 // Cogito Expressive: 32dp indicator height
#define COGITO_NAV_RAIL_COLLAPSED_INDICATOR_NO_LABEL_HEIGHT 56 // Icon-only mode: taller indicator to center icon
#define COGITO_NAV_RAIL_COLLAPSED_ICON_Y_OFFSET 4 // Center icon in 32dp indicator
#define COGITO_NAV_RAIL_COLLAPSED_ICON_Y_OFFSET_NO_LABEL 16 // Center icon in 56dp indicator
#define COGITO_NAV_RAIL_COLLAPSED_INDICATOR_LABEL_GAP 4
// Expanded mode
#define COGITO_NAV_RAIL_EXPANDED_MIN_WIDTH 220
#define COGITO_NAV_RAIL_EXPANDED_MAX_WIDTH 360
#define COGITO_NAV_RAIL_EXPANDED_TOP_OFFSET 24
#define COGITO_NAV_RAIL_EXPANDED_SECTION_GAP 20
#define COGITO_NAV_RAIL_EXPANDED_ITEM_STEP 72
#define COGITO_NAV_RAIL_EXPANDED_INDICATOR_HEIGHT 56
#define COGITO_NAV_RAIL_EXPANDED_INDICATOR_MIN_WIDTH 112
#define COGITO_NAV_RAIL_EXPANDED_SIDE_PADDING 24
#define COGITO_NAV_RAIL_EXPANDED_CONTENT_INSET 16
#define COGITO_NAV_RAIL_EXPANDED_LABEL_GAP 12
// Animation Easing Functions (legacy, kept for compatibility)
#define COGITO_EASING_LINEAR 0
#define COGITO_EASING_STANDARD 1 // Standard easing (smooth start and end)
#define COGITO_EASING_IN 2 // Accelerate
#define COGITO_EASING_OUT 3 // Decelerate
#define COGITO_EASING_IN_OUT 4 // Accelerate then decelerate
#define COGITO_EASING_EMPHASIZED 5 // Spring with overshoot (in-back)
#define COGITO_EASING_EMPHASIZED_OUT 6 // Spring decelerate (out-back)

// Physics Spring Tokens — stiffness (k) and damping ratio (z)
// Expressive: overshoot allowed, feels alive (buttons, sliders, FAB)
#define COGITO_SPRING_K_EXPRESSIVE   750.0
#define COGITO_SPRING_Z_EXPRESSIVE   0.5
// Functional: critically/over-damped, no overshoot (toolbars, layout, opacity)
#define COGITO_SPRING_K_FUNCTIONAL   1400.0
#define COGITO_SPRING_Z_FUNCTIONAL   1.0
// Second-Order Spring state (one instance per animated scalar/channel)
typedef struct {
  double x; // current value
  double y; // current acceleration
  double v; // current velocity  (preserved on target change for continuity)
  bool   inited; // has been initialized
  double target; // target value
} CogitoSpring;

// Advance a spring by dt seconds toward target.
// Uses semi-implicit Euler integration (stable for large dt).
// k = stiffness (angular frequency squared), z = damping ratio.
static inline void cogito_spring_tick(CogitoSpring* s, double target, double k, double z, double dt) {
  if (!s) return;
  if (!s->inited) {
    s->x = target;
    s->v = 0.0;
    s->inited = true;
    return;
  }
  if (dt <= 0.0) return;
  // Clamp dt to avoid instability on long frame gaps (e.g. first frame after resize)
  if (dt > 0.05) dt = 0.05;
  double w = sqrt(k); // natural frequency
  double d = z * 2.0 * w; // damping coefficient
  // Semi-implicit Euler: update velocity first, then position
  double a = -k * (s->x - target) - d * s->v;
  s->v += a * dt;
  s->x += s->v * dt;
}
// Returns true if the spring is still meaningfully moving (needs another frame)
static inline bool cogito_spring_active(const CogitoSpring* s, double target) {
  if (!s || !s->inited) return false;
  double dx = s->x - target;
  return (dx * dx > 1e-6 || s->v * s->v > 1e-6);
}
// Snap spring to target with zero velocity (use for first-frame init)
static inline void cogito_spring_snap(CogitoSpring* s, double target) {
  if (!s) return;
  s->x = target;
  s->v = 0.0;
  s->inited = true;
}
// Per-frame dt computed from SDL_GetPerformanceCounter() — shared globally
static double cogito_spring_dt = 0.0;
static uint64_t cogito_spring_last_ticks = 0;

static void cogito_spring_dt_update(void) {
  uint64_t now = SDL_GetPerformanceCounter();
  if (cogito_spring_last_ticks == 0) {
    cogito_spring_dt = 0.0;
  } else {
    uint64_t freq = SDL_GetPerformanceFrequency();
    cogito_spring_dt = (double)(now - cogito_spring_last_ticks) / (double)(freq > 0 ? freq : 1);
  }
  cogito_spring_last_ticks = now;
}
// Color functions now use backend.h implementations
#define cogito_luma cogito_color_luma
#define cogito_mix cogito_color_mix
#define cogito_alpha cogito_color_alpha
#define cogito_on_color cogito_color_on_color

typedef struct CogitoApp {
  ErgoObj base;
  void* native;
  ErgoStr* app_id;
  ErgoStr* app_name;
  bool accent_override;
  bool dark_override;
  bool dark_mode;
  CogitoColor accent_color;
  bool accent_set;
  bool system_dark;
  CogitoColor system_accent;
  bool system_accent_set;
  double system_theme_next_check;
  ErgoStr* app_icon; // path to app icon (png/svg/jpg/bmp)
} CogitoApp;

typedef enum {
  COGITO_SIDE_SHEET_MODAL,
  COGITO_SIDE_SHEET_INSET,
  COGITO_SIDE_SHEET_SIDEBAR
} CogitoSideSheetMode;

typedef enum {
  COGITO_APPBAR,
  COGITO_BOTTOM_NAV,
  COGITO_BUTTON,
  COGITO_CAROUSEL,
  COGITO_CAROUSEL_ITEM,
  COGITO_CHECKBOX,
  COGITO_CHIP,
  COGITO_COLORPICKER,
  COGITO_DATEPICKER,
  COGITO_DIALOG,
  COGITO_DIALOG_SLOT,
  COGITO_DROPDOWN,
  COGITO_FAB,
  COGITO_FAB_MENU,
  COGITO_FIXED,
  COGITO_GRID,
  COGITO_HSTACK,
  COGITO_ICONBTN,
  COGITO_IMAGE,
  COGITO_LABEL,
  COGITO_LIST,
  COGITO_NAV_RAIL,
  COGITO_POPOVER,
  COGITO_PROGRESS,
  COGITO_SCROLLER,
  COGITO_SEARCHFIELD,
  COGITO_BUTTON_GROUP,
  COGITO_SLIDER,
  COGITO_STEPPER,
  COGITO_SWITCH,
  COGITO_TABS,
  COGITO_TEXTFIELD,
  COGITO_TEXTVIEW,
  COGITO_TOAST,
  COGITO_TOASTS,
  COGITO_TOOLBAR,
  COGITO_TOOLTIP,
  COGITO_TREEVIEW,
  COGITO_VIEWSWITCHER,
  COGITO_VSTACK,
  COGITO_WINDOW,
  COGITO_ZSTACK,
  COGITO_DIVIDER,
  COGITO_CARD,
  COGITO_AVATAR,
  COGITO_BADGE,
  COGITO_BANNER,
  COGITO_BOTTOM_SHEET,
  COGITO_TIMEPICKER,
  COGITO_ACTIVE_INDICATOR,
  COGITO_SWITCHBAR,
  COGITO_CONTENT_LIST,
  COGITO_EMPTY_PAGE,
  COGITO_TIP_VIEW,
  COGITO_SETTINGS_WINDOW,
  COGITO_SETTINGS_PAGE,
  COGITO_SETTINGS_LIST,
  COGITO_SETTINGS_ROW,
  COGITO_WELCOME_SCREEN,
  COGITO_VIEW_DUAL,
  COGITO_VIEW_CHOOSER,
  COGITO_ABOUT_WINDOW,
  COGITO_SPLIT_BUTTON,
  COGITO_DRAWING_AREA,
  COGITO_SHAPE,
  COGITO_MENU_SECTION,
  COGITO_SIDE_SHEET,
  COGITO_KIND_COUNT
} CogitoKind;

typedef enum {
  COGITO_BORDER_NONE = 0,
  COGITO_BORDER_SOLID = 1,
  COGITO_BORDER_DASHED = 2,
  COGITO_BORDER_DOTTED = 3
} CogitoBorderStyle;

typedef struct CogitoBoxShadow {
  float dx;
  float dy;
  float blur;
  float spread;
  CogitoColor color;
  bool inset;
} CogitoBoxShadow;

typedef struct CogitoStyle {
  bool has_bg;
  bool has_text;
  bool has_border;
  bool has_border_width;
  bool has_border_style;
  bool has_radius;
  bool has_radius_tl;
  bool has_radius_tr;
  bool has_radius_br;
  bool has_radius_bl;
  bool has_font_size;
  bool has_padding;
  bool has_padding_left;
  bool has_padding_top;
  bool has_padding_right;
  bool has_padding_bottom;
  bool has_margin;
  bool has_margin_left;
  bool has_margin_top;
  bool has_margin_right;
  bool has_margin_bottom;
  bool has_selection;
  bool has_track;
  bool has_track_on;
  bool has_knob;
  bool has_check;
  bool has_font_weight;
  bool has_letter_spacing;
  bool has_font_tabular;
  bool has_font_mono;
  bool has_font_family;
  bool has_box_shadow;
  bool has_min_w;
  bool has_min_h;
  bool has_max_w;
  bool has_max_h;
  bool has_icon_size;
  bool has_icon_color;
  bool has_transition_ms;
  bool has_transition_ease;
  bool has_appbar_btn_size;
  bool has_appbar_btn_gap;
  bool has_appbar_btn_pad_top;
  bool has_appbar_btn_pad_right;
  bool has_appbar_btn_close_color;
  bool has_appbar_btn_min_color;
  bool has_appbar_btn_max_color;
  bool has_appbar_btn_border_color;
  bool has_appbar_btn_border_width;
  bool has_shadow;
  bool has_opacity;
  bool has_track_height;
  bool has_divider;
  bool has_item_gap;
  CogitoColor bg;
  CogitoColor text;
  CogitoColor border;
  CogitoColor selection;
  CogitoColor track;
  CogitoColor track_on;
  CogitoColor knob;
  CogitoColor check;
  CogitoColor appbar_btn_close_color;
  CogitoColor appbar_btn_min_color;
  CogitoColor appbar_btn_max_color;
  CogitoColor appbar_btn_border_color;
  int border_width;
  CogitoBorderStyle border_style;
  int radius;
  int radius_tl;
  int radius_tr;
  int radius_br;
  int radius_bl;
  int font_size;
  int font_weight;
  float letter_spacing;
  bool font_tabular;
  bool font_mono;
  char font_family[
    64
  ];
  CogitoBoxShadow box_shadow;
  int min_w;
  int min_h;
  int max_w;
  int max_h;
  int icon_size;
  CogitoColor icon_color;
  int transition_ms;
  int transition_ease;
  int appbar_btn_size;
  int appbar_btn_gap;
  int appbar_btn_pad_top;
  int appbar_btn_pad_right;
  int appbar_btn_border_width;
  bool divider;
  int item_gap;
  int shadow_level;
  int padding_left;
  int padding_top;
  int padding_right;
  int padding_bottom;
  int margin_left;
  int margin_top;
  int margin_right;
  int margin_bottom;
  float opacity;
  int track_height;
} CogitoStyle;

#define COGITO_MAX_CUSTOM_CLASSES 50000 // 50,000 custom classes + 12 label classes
#define COGITO_LABEL_CLASS_COUNT 12

typedef struct CogitoCustomClassStyle {
  char name[
    64
  ];
  int kind;
  CogitoStyle style;
  CogitoStyle hover;
  CogitoStyle active;
  CogitoStyle focused;
  CogitoStyle checked;
  CogitoStyle disabled;
} CogitoCustomClassStyle;

typedef struct CogitoTheme {
  CogitoStyle base;
  CogitoStyle per_kind[COGITO_KIND_COUNT
  ];
  CogitoStyle per_kind_hover[COGITO_KIND_COUNT
  ];
  CogitoStyle per_kind_active[COGITO_KIND_COUNT
  ];
  CogitoStyle per_kind_focused[COGITO_KIND_COUNT
  ];
  CogitoStyle per_kind_checked[COGITO_KIND_COUNT
  ];
  CogitoStyle per_kind_disabled[COGITO_KIND_COUNT
  ];
  CogitoStyle label_class[COGITO_LABEL_CLASS_COUNT
  ];
  CogitoStyle label_class_hover[COGITO_LABEL_CLASS_COUNT
  ];
  CogitoStyle label_class_active[COGITO_LABEL_CLASS_COUNT
  ];
  CogitoStyle label_class_focused[COGITO_LABEL_CLASS_COUNT
  ];
  CogitoStyle label_class_checked[COGITO_LABEL_CLASS_COUNT
  ];
  CogitoStyle label_class_disabled[COGITO_LABEL_CLASS_COUNT
  ];
  CogitoStyle class_mono;
  CogitoStyle class_mono_hover;
  CogitoStyle class_mono_active;
  CogitoStyle class_mono_focused;
  CogitoStyle class_mono_checked;
  CogitoStyle class_mono_disabled;
  CogitoStyle class_tabular;
  CogitoStyle class_tabular_hover;
  CogitoStyle class_tabular_active;
  CogitoStyle class_tabular_focused;
  CogitoStyle class_tabular_checked;
  CogitoStyle class_tabular_disabled;
  CogitoStyle menu;
  CogitoStyle menu_item;
  CogitoStyle appbar_child_button;
  CogitoStyle appbar_child_button_hover;
  CogitoStyle appbar_child_button_active;
  CogitoStyle appbar_child_iconbtn;
  CogitoStyle appbar_child_iconbtn_hover;
  CogitoStyle appbar_child_iconbtn_active;
  CogitoStyle appbar_win_btn_all;
  CogitoStyle appbar_win_btn_hover_all;
  CogitoStyle appbar_win_btn_active_all;
  CogitoStyle appbar_win_btn[
    3
  ];
  CogitoStyle appbar_win_btn_hover[
    3
  ];
  CogitoStyle appbar_win_btn_active[
    3
  ];
  CogitoStyle checkbox_check;
  CogitoStyle checkbox_check_hover;
  CogitoStyle checkbox_check_active;
  CogitoStyle checkbox_check_checked;
  CogitoStyle radio_check;
  CogitoStyle radio_check_hover;
  CogitoStyle radio_check_active;
  CogitoStyle radio_check_checked;
  CogitoStyle checkbox_box;
  CogitoStyle checkbox_box_hover;
  CogitoStyle checkbox_box_active;
  CogitoStyle checkbox_box_checked;
  CogitoStyle radio_box;
  CogitoStyle radio_box_hover;
  CogitoStyle radio_box_active;
  CogitoStyle radio_box_checked;
  CogitoStyle switch_track;
  CogitoStyle switch_track_hover;
  CogitoStyle switch_track_active;
  CogitoStyle switch_track_checked;
  CogitoStyle switch_knob;
  CogitoStyle switch_knob_hover;
  CogitoStyle switch_knob_active;
  CogitoStyle switch_knob_checked;
  CogitoStyle content_list_item_first;
  CogitoStyle content_list_item_last;
  CogitoStyle content_list_item_middle;
  CogitoStyle content_list_item_single;
  CogitoStyle content_list_label;
  CogitoStyle button_outlined;
  CogitoStyle button_outlined_hover;
  CogitoStyle button_outlined_active;
  CogitoStyle button_outlined_focused;
  CogitoStyle button_outlined_disabled;
  CogitoStyle button_text;
  CogitoStyle button_text_hover;
  CogitoStyle button_text_active;
  CogitoStyle button_text_focused;
  CogitoStyle button_text_disabled;
  // Iconbtn color styles (0=filled, 1=tonal, 2=outlined, 3=iconic)
  CogitoStyle iconbtn_filled;
  CogitoStyle iconbtn_filled_hover;
  CogitoStyle iconbtn_filled_active;
  CogitoStyle iconbtn_tonal;
  CogitoStyle iconbtn_tonal_hover;
  CogitoStyle iconbtn_tonal_active;
  CogitoStyle iconbtn_outlined;
  CogitoStyle iconbtn_outlined_hover;
  CogitoStyle iconbtn_outlined_active;
  CogitoStyle iconbtn_iconic;
  CogitoStyle iconbtn_iconic_hover;
  CogitoStyle iconbtn_iconic_active;
  CogitoCustomClassStyle custom_classes[COGITO_MAX_CUSTOM_CLASSES
  ];
  int custom_class_count;
} CogitoTheme;

typedef struct CogitoFontCacheEntry {
  int size;
  CogitoFont* font;
  bool ready;
} CogitoFontCacheEntry;

typedef struct CogitoNode {
  ErgoObj base;
  CogitoKind kind;
  struct CogitoNode* parent;
  struct CogitoNode** children;
  size_t len;
  size_t cap;

  // Content
  ErgoStr* text;
  ErgoStr* subtitle;
  ErgoStr* hint;
  ErgoStr* icon;
  ErgoStr* tooltip;
  ErgoStr* action_text;
  ErgoStr* view_id;
  ErgoStr* view_active_id;
  ErgoStr* class_name;
  ErgoStr* a11y_label;
  ErgoStr* a11y_role;

  // Lazy view builders (ViewSwitcher)
  ErgoStr** view_builder_ids;
  ErgoFn** view_builders;
  size_t view_builder_len;
  size_t view_builder_cap;

  // Collections (menus, tabs)
  ErgoStr** menu_labels;
  ErgoFn** menu_handlers;
  size_t menu_len;
  size_t menu_cap;
  // Menu sections for intelligent dividers
  bool* menu_section_after;
  // Menu item properties (parallel arrays, same length as menu_labels)
  ErgoStr** menu_icons;      // Leading icon name per item (NULL = none)
  ErgoStr** menu_shortcuts;  // Trailing shortcut text per item (NULL = none)
  bool* menu_submenu;        // Whether item opens a submenu (trailing arrow)
  bool* menu_toggled;        // Whether item is in toggled state
  ErgoStr** tab_labels;
  ErgoStr** tab_icons;
  ErgoStr** tab_ids;
  size_t tab_len;
  size_t tab_cap;

  // Callbacks
  ErgoFn* on_click;
  ErgoFn* on_change;
  ErgoFn* on_select;
  ErgoFn* on_activate;
  ErgoFn* builder;
  ErgoFn* on_action;
  ErgoFn* on_draw;

  // State
  ErgoVal group;
  bool checked;
  int selected;
  bool tree_collapsed;
  bool should_close;
  bool disabled;
  bool editable;
  bool resizable;
  bool fixed_pos_set;
  bool text_wrap;
  bool text_ellipsis;
  int text_align;
  bool font_mono;
  bool font_tabular;

  // Geometry
  int x, y, w, h;
  int margin_left, margin_top, margin_right, margin_bottom;
  int padding_left, padding_top, padding_right, padding_bottom;
  int align;
  bool hexpand;
  bool vexpand;
  int gap;
  bool auto_size;
  int fixed_x, fixed_y;
  int grid_span_x, grid_span_y;
  int grid_align_x, grid_align_y;
  int action_button_x, action_button_y, action_button_w, action_button_h;

  // Dialog / Popover / SideSheet
  struct CogitoNode* dialog;
  struct CogitoNode* view_switcher;
  bool dialog_open;
  struct CogitoNode* popover;
  struct CogitoNode* popover_anchor;
  bool popover_open;
  struct CogitoNode* side_sheet_node;
  bool side_sheet_open;

  // Style flags
  bool margin_set;
  bool padding_set;
  bool bg_set;
  bool text_color_set;
  bool border_color_set;
  bool border_width_set;
  bool radius_set;
  bool selection_set;
  bool track_set;
  bool track_on_set;
  bool knob_set;
  bool check_set;
  bool font_weight_set;
  bool letter_spacing_set;
  bool min_w_set;
  bool min_h_set;
  bool max_w_set;
  bool max_h_set;
  bool shadow_set;
  bool font_size_set;
  bool border_style_set;
  bool radius_tl_set, radius_tr_set, radius_br_set, radius_bl_set;
  bool font_family_set;
  bool box_shadow_set;
  bool track_height_set;
  bool opacity_set;

  // Style values
  CogitoColor bg;
  CogitoColor text_color;
  CogitoColor border_color;
  CogitoColor selection_color;
  CogitoColor track_color;
  CogitoColor track_on_color;
  CogitoColor knob_color;
  CogitoColor check_color;
  int border_width;
  int border_radius;
  int font_size;
  int font_weight;
  float letter_spacing;
  int min_w, min_h;
  int max_w, max_h;
  int shadow_level;
  CogitoBorderStyle border_style;
  int radius_tl, radius_tr, radius_br, radius_bl;
  char font_family[
    64
  ];
  CogitoBoxShadow box_shadow;
  int track_height;
  float opacity;
  bool has_opacity;


  // Animation (legacy time fields — kept for anim_ms/ease compat)
  bool anim_inited;
  double anim_start;
  int anim_ms;
  int anim_ease;
  // Legacy color animation fields (still used for backward compat paths)
  CogitoColor anim_bg;
  CogitoColor anim_bg_from;
  CogitoColor anim_bg_target;
  CogitoColor anim_text;
  CogitoColor anim_text_from;
  CogitoColor anim_text_target;
  CogitoColor anim_border;
  CogitoColor anim_border_from;
  CogitoColor anim_border_target;
  CogitoColor anim_selection;
  CogitoColor anim_selection_from;
  CogitoColor anim_selection_target;
  double anim_bg_start;
  double anim_text_start;
  double anim_border_start;
  double anim_selection_start;
  int anim_x, anim_x_from, anim_x_target;
  double anim_x_start;
  int anim_y, anim_y_from, anim_y_target;
  double anim_y_start;
  int anim_w, anim_w_from, anim_w_target;
  double anim_w_start;
  int anim_h, anim_h_from, anim_h_target;
  double anim_h_start;
  bool anim_layout_active;
  double active_latch_until;
  float opacity_from;
  float opacity_target;
  double opacity_start;
  // TextField floating-label animation channel (separate from shared layout springs)
  float tf_label_anim_current;
  float tf_label_anim_from;
  float tf_label_anim_target;
  double tf_label_anim_start;
  bool tf_label_anim_inited;
  // --- Physics Spring Animators (Second-Order Spring Solver) ---
  // One spring per color channel (R,G,B,A) × 4 color properties = 16 springs
  CogitoSpring spring_bg_r,  spring_bg_g,  spring_bg_b,  spring_bg_a;
  CogitoSpring spring_txt_r, spring_txt_g, spring_txt_b, spring_txt_a;
  CogitoSpring spring_bdr_r, spring_bdr_g, spring_bdr_b, spring_bdr_a;
  CogitoSpring spring_sel_r, spring_sel_g, spring_sel_b, spring_sel_a;
  // Scalar springs: layout position/size and opacity
  CogitoSpring spring_x; // layout x
  CogitoSpring spring_y; // layout y
  CogitoSpring spring_w; // layout w
  CogitoSpring spring_h; // layout h
  CogitoSpring spring_opacity; // opacity
  // Font-weight spring for expressive typography (wght axis)
  CogitoSpring spring_font_weight;
  int spring_font_weight_target_int; // last integer target (to detect changes)

  // Ripple animation (mouse-position-aware, for Primary-colored interactables)
  double ripple_start; // Animation start time
  float ripple_progress; // 0.0 to 1.0, updated each frame
  int ripple_x; // Click position relative to widget
  int ripple_y;
  float ripple_max_radius; // Pre-calculated max radius
  bool ripple_active; // Whether ripple is currently animating

  // Window-only theme transition: radial reveal from interaction origin.
  bool window_theme_reveal_active;
  double window_theme_reveal_start;
  double window_theme_reveal_duration;
  int window_theme_reveal_origin_x;
  int window_theme_reveal_origin_y;
  float window_theme_reveal_radius;
  float window_theme_reveal_max_radius;
  CogitoColor window_theme_reveal_old_bg;
  CogitoColor window_theme_reveal_new_bg;

  // Kind-specific data (anonymous union — only one member active per node)
  union {
    struct {
      int scroll_x, scroll_y;
      int content_w, content_h;
      bool horz, vert;
      bool bar_hovered;
      uint8_t bar_alpha;
      double bar_hover_until;
    } scroller;

    struct {
      double min, max, value;
      double value2;
      bool dragging;
      uint8_t mode;
      uint8_t active_handle;
      uint8_t size;
    } slider;

    struct {
      double min, max, value, step;
      bool hit_minus, hit_plus;
    } stepper;

    struct {
      int year, month, day;
      int view_year, view_month;
      int list_start;
      bool popover_panel;
      uint8_t mode;
      struct CogitoNode* owner;
    } datepicker;

    struct {
      double h, c, t;
      bool hex_editing;
      bool popover_panel;
      struct CogitoNode* owner;
    } colorpicker;

    struct {
      int hour, minute;
      bool ampm;
      bool initialized;
      bool popover_panel;
      bool dial_visible;
      uint8_t mode; // 0: hour dial, 1: minute dial
      struct CogitoNode* owner;
    } timepicker;

    struct {
      int btn_close_x, btn_min_x, btn_max_x;
      int btn_y, btn_size;
      int title_y, subtitle_y;
      char controls[
        16
      ];
    } appbar;

    struct {
      int cols;
      int gap_x, gap_y;
    } grid;

    struct {
      bool dragging;
      int scroll_x;
      int drag_start_x, drag_start_scroll;
      int active_index, item_count;
      float velocity;
      double scroll_xf;
      double anim_start, anim_from, anim_to;
      bool animating;
      double drag_last_time, drag_last_xf;
    } carousel;

    struct {
      CogitoTexture* texture;
      int w, h;
      bool loaded;
      float avg_lum;
    } image;

    struct {
      int caret;
      int sel_start, sel_end;
      bool selecting;
    } text_input;

    struct {
      ErgoStr** labels;
      ErgoStr** icons;
      int* badges; // 0: none, -1: dot, >0: numeric
      size_t len, cap;
      int selected;
      bool expanded;
      bool show_toggle;
      bool no_label; // If true, use icon-only mode with larger indicator
    } nav_rail;

    struct {
      ErgoStr** labels;
      ErgoStr** icons;
      size_t len, cap;
      int selected;
    } bottom_nav;

    struct { int count;
    } badge;
    struct {
      double value;
      bool indeterminate;
      bool circular;
      uint8_t shape;
    } progress;
    struct {
      bool outlined, text_style;
      uint8_t size;
      int icon_shape;
      int icon_color_style;
      int icon_width;
      bool icon_toggle;
      bool menu_divider;
      int menu_item_gap;
      bool menu_vibrant;
    } button;
    struct { bool selected, has_close;
    } chip;
     struct {
       bool dragging;
       bool expanded;
       int drag_start_y;
       int drag_start_h;
       int desired_h;
    } bottom_sheet;
     struct {
       bool dragging;
       bool expanded;
       int drag_start_x;
       int drag_start_w;
       int desired_w;
       uint8_t mode;
    } side_sheet;
    struct { bool extended; uint8_t size; // 0=S(42), 1=M(56, default), 2=L(96)
    } fab;
    struct { bool vertical, inset;
    } divider;
    struct { int cls;
    } label;
    struct { double angle; double last_time;
    } indicator;
    struct { float ratio; bool dragging; int drag_offset;
    } view_dual;
    struct { int arrow_x; uint8_t size; // 0=XS..4=XL, same as button
      bool outlined, text_style;
    } split_btn;
    struct {
      bool pressed;
      bool dragging;
      int pointer_x;
      int pointer_y;
      int press_x;
      int press_y;
      int point_count;
      int points_x[COGITO_DRAWING_AREA_MAX_POINTS];
      int points_y[COGITO_DRAWING_AREA_MAX_POINTS];
      CogitoColor canvas_color;
      int canvas_line_width;
    } drawing_area;
    struct {
      int preset;
      int size_dp;
      int color_style; // 0=custom, 1=primary, 2=secondary, 3=tertiary
      bool custom_color_set;
      CogitoColor custom_color;
      float vertices[COGITO_SHAPE_VERTEX_COUNT * 2];
      int svg_poly_count;
      float svg_poly[COGITO_SHAPE_SVG_POLY_MAX * 2];
    } shape;
    struct { bool has_divider_before; bool has_divider_after;
    } menu_section;
    struct { bool vibrant; bool vertical;
    } toolbar;
    // ButtonGroup: size (0-4 for XS-XL), shape (0=round,1=square), connected (0=standard,1=connected)
    // pressed_child: index of child being pressed (-1 = none), for width expansion animation
    struct { int size; int shape; int connected; int pressed_child;
    } buttongroup;
    struct {
      uint8_t variant;                  // 0=elevated(default), 1=filled, 2=outlined
      struct CogitoNode* header_image;  // optional top image (NOT in children[]; own ref)
      struct CogitoNode* overflow_btn;  // optional overflow iconbtn (in children[])
      struct CogitoNode** actions;      // action button nodes (in children[]; array freed separately)
      size_t action_len, action_cap;
    } card;
  };
} CogitoNode;

typedef struct CogitoState {
  ErgoObj base;
  ErgoVal value;
} CogitoState;

typedef struct CogitoMenu {
  bool open;
  CogitoNode* owner;
  int x;
  int y;
  int w;
  int h;
  int item_h;
  int pad_x;
  int pad_y;
  int pad_top;
  int pad_bottom;
} CogitoMenu;

static CogitoTheme cogito_theme;
static bool cogito_theme_initialized = false;
static bool cogito_backend_ready = false;
static CogitoMenu cogito_menu = {
  0
};
static int cogito_menu_kbd_index = -1;
static CogitoNode* cogito_fab_menu_open_node = NULL;
static int cogito_fab_menu_kbd_index = -1;
static CogitoFont* cogito_font = NULL;
static bool cogito_font_ready = false;
static CogitoFont* cogito_font_serif = NULL;
static bool cogito_font_serif_ready = false;
static CogitoFont* cogito_font_mono = NULL;
static bool cogito_font_mono_ready = false;
static bool cogito_font_serif_from_env = false;
static bool cogito_font_mono_from_env = false;
static const char* cogito_font_serif_path_active = NULL;
static const char* cogito_font_mono_path_active = NULL;
static int cogito_font_serif_loaded_size = 0;
static int cogito_font_mono_loaded_size = 0;
static CogitoFont* cogito_font_bold = NULL;
static bool cogito_font_bold_ready = false;
static bool cogito_font_bold_variable = false;
static CogitoFontCacheEntry cogito_font_cache[
  8
];
static CogitoFontCacheEntry cogito_font_serif_cache[
  8
];
static CogitoFontCacheEntry cogito_font_bold_cache[
  8
];
static CogitoFontCacheEntry cogito_font_tabular_cache[
  8
];
static int cogito_font_size_value = 16;
static int cogito_font_loaded_size = 0;
static int cogito_font_bold_loaded_size = 0;
static bool cogito_font_from_env = false;
static bool cogito_font_bold_from_env = false;
static bool cogito_sum_default_loaded = false;
static char* cogito_user_sum = NULL;
static int cogito_style_version = 0;
static CogitoNode* cogito_focused = NULL;
static bool cogito_focus_visible = false;
static CogitoNode* cogito_focus_before_overlay = NULL;
static bool cogito_focus_before_overlay_visible = false;
static double cogito_frame_now = 0.0;
static bool cogito_frame_now_valid = false;

static inline double cogito_now(void) {
  if (cogito_frame_now_valid) return cogito_frame_now;
  if (cogito_backend && cogito_backend->get_time) return cogito_backend->get_time();
  return 0.0;
}

static inline double cogito_refresh_now(void) {
  if (cogito_backend && cogito_backend->get_time) cogito_frame_now = cogito_backend->get_time();
  else cogito_frame_now = 0.0;
  cogito_frame_now_valid = true;
  return cogito_frame_now;
}
// Draw-context: forced text color for selected children (propagates down render tree)
static CogitoColor cogito_selection_text = {
  0
};
static bool cogito_has_selection_text = false;
static bool cogito_dialog_slot_force_draw = false;
static bool cogito_toasts_force_draw = false;
static bool cogito_fab_menu_force_draw = false;
static CogitoNode* cogito_slider_active = NULL;
static CogitoNode* cogito_carousel_active = NULL;
static CogitoNode* cogito_bottom_sheet_active = NULL;
static CogitoNode* cogito_side_sheet_active = NULL;
static int cogito_carousel_animating_count = 0; // Track number of animating carousels globally
static double cogito_anim_end_time = 0.0; // Time when all animations will complete (time-based for smooth animation)
static CogitoNode* cogito_colorpicker_active = NULL;
static int cogito_colorpicker_channel = -1;
static CogitoNode* cogito_timepicker_active = NULL;
static CogitoNode* cogito_pointer_capture = NULL;
static CogitoNode* cogito_canvas_active = NULL;
static CogitoNode* cogito_tooltip_hovered = NULL;
static CogitoApp* cogito_active_app = NULL;
static CogitoNode* cogito_active_window = NULL;
static CogitoWindow* cogito_active_backend_window = NULL;
static bool cogito_theme_dark = false;
static int    cogito_theme_ensor_variant = 0;  // 0=default,1=vibrant,2=muted,3=mono,4=salad,5=content
static double cogito_theme_contrast = 0.0;    // -1.0 to 1.0 (from system or user)
static double cogito_theme_font_weight = 0.0; // 0=unset, 100-900
static double cogito_theme_roundness = 0.0;   // 0=unset, multiplier
static int    cogito_theme_spacing = 0;        // 0=unset, 1=compact, 2=normal, 3=comfortable
static bool cogito_prefers_reduced_motion = false;
static bool cogito_prefers_high_contrast = false;
static bool cogito_theme_window_reveal_pending = false;
// If true, theme recompute is deferred until a window exists.
static bool cogito_theme_refresh_deferred = false;
// Set by cogito_app_update_theme() to let run() avoid duplicate style passes.
static bool cogito_theme_applied_last_update = false;
static int cogito_theme_action_origin_x = 0;
static int cogito_theme_action_origin_y = 0;
static bool cogito_theme_action_origin_valid = false;
static double cogito_theme_action_origin_time = 0.0;
// Set during layout when an image acquires real intrinsic dimensions.
static bool cogito_layout_image_loaded_this_pass = false;
// Optional startup profiling helpers.
static bool cogito_draw_count_enabled = false;
static size_t cogito_draw_count_nodes = 0;
static CogitoColor cogito_theme_accent = {
  0,
  0,
  0,
  0
};

static bool cogito_overlay_contains_node(const CogitoNode* root, const CogitoNode* needle) {
  if (!root || !needle) return false;
  if (root == needle) return true;
  for (size_t i = 0; i < root->len; i++) {
    if (cogito_overlay_contains_node(root->children[i], needle)) return true;
  }
  return false;
}

static void cogito_overlay_focus_snapshot(void) {
  if (!cogito_focused || cogito_focused->disabled) return;
  cogito_focus_before_overlay = cogito_focused;
  cogito_focus_before_overlay_visible = cogito_focus_visible;
}

static void cogito_overlay_focus_restore(CogitoNode* root) {
  if (!root) return;
  if (cogito_focus_before_overlay &&
      !cogito_focus_before_overlay->disabled &&
      cogito_overlay_contains_node(root, cogito_focus_before_overlay)) {
    cogito_focused = cogito_focus_before_overlay;
    cogito_focus_visible = cogito_focus_before_overlay_visible;
  } else {
    cogito_focus_visible = false;
  }
  cogito_focus_before_overlay = NULL;
  cogito_focus_before_overlay_visible = false;
}
enum {
  COGITO_LABEL_CLASS_NONE = 0,
  COGITO_LABEL_CLASS_TITLE,
  COGITO_LABEL_CLASS_SUBTITLE,
  COGITO_LABEL_CLASS_BODY,
  COGITO_LABEL_CLASS_BIG_DISPLAY,
  COGITO_LABEL_CLASS_DISPLAY,
  COGITO_LABEL_CLASS_VIEW_TITLE,
  COGITO_LABEL_CLASS_VIEW_SUBTITLE,
  COGITO_LABEL_CLASS_HEADING,
  COGITO_LABEL_CLASS_CONTENT_TITLE,
  COGITO_LABEL_CLASS_CONTENT_SUBTITLE,
  COGITO_LABEL_CLASS_CAPTION
};
enum {
  COGITO_STYLE_CLASS_NONE = 0,
  COGITO_STYLE_CLASS_MONO,
  COGITO_STYLE_CLASS_TABULAR,
  COGITO_STYLE_CLASS_OUTLINED,
  COGITO_STYLE_CLASS_TEXT,
  COGITO_STYLE_CLASS_FILLED,
  COGITO_STYLE_CLASS_TONAL,
  COGITO_STYLE_CLASS_ICONIC
};
enum {
  COGITO_PROGRESS_SHAPE_FLAT = 0,
  COGITO_PROGRESS_SHAPE_WAVY
};
enum {
  COGITO_SHAPE_CIRCLE = 0,
  COGITO_SHAPE_SQUARE,
  COGITO_SHAPE_SLANTED,
  COGITO_SHAPE_ARCH,
  COGITO_SHAPE_SEMICIRCLE,
  COGITO_SHAPE_OVAL,
  COGITO_SHAPE_PILL,
  COGITO_SHAPE_TRIANGLE,
  COGITO_SHAPE_ARROW,
  COGITO_SHAPE_FAN,
  COGITO_SHAPE_DIAMOND,
  COGITO_SHAPE_CLAMSHELL,
  COGITO_SHAPE_PENTAGON,
  COGITO_SHAPE_GEM,
  COGITO_SHAPE_VERY_SUNNY,
  COGITO_SHAPE_SUNNY,
  COGITO_SHAPE_COOKIE_4,
  COGITO_SHAPE_COOKIE_6,
  COGITO_SHAPE_COOKIE_7,
  COGITO_SHAPE_COOKIE_9,
  COGITO_SHAPE_COOKIE_12,
  COGITO_SHAPE_CLOVER_4,
  COGITO_SHAPE_CLOVER_8,
  COGITO_SHAPE_BURST,
  COGITO_SHAPE_SOFT_BURST,
  COGITO_SHAPE_BOOM,
  COGITO_SHAPE_SOFT_BOOM,
  COGITO_SHAPE_FLOWER,
  COGITO_SHAPE_PUFFY,
  COGITO_SHAPE_PUFFY_DIAMOND,
  COGITO_SHAPE_GHOSTISH,
  COGITO_SHAPE_PIXEL_CIRCLE,
  COGITO_SHAPE_PIXEL_TRIANGLE,
  COGITO_SHAPE_BUN,
  COGITO_SHAPE_HEART,
  COGITO_SHAPE_COUNT
};
enum {
  COGITO_SLIDER_MODE_NORMAL = 0,
  COGITO_SLIDER_MODE_CENTERED,
  COGITO_SLIDER_MODE_RANGE
};
enum {
  COGITO_SLIDER_SIZE_XS = 0,
  COGITO_SLIDER_SIZE_S,
  COGITO_SLIDER_SIZE_M,
  COGITO_SLIDER_SIZE_L,
  COGITO_SLIDER_SIZE_XL
};
enum {
  COGITO_BUTTON_SIZE_XS = 0,
  COGITO_BUTTON_SIZE_S,
  COGITO_BUTTON_SIZE_M,
  COGITO_BUTTON_SIZE_L,
  COGITO_BUTTON_SIZE_XL
};

static uint8_t cogito_button_clamp_size_int(int size) {
  if (size < COGITO_BUTTON_SIZE_XS) return (uint8_t)COGITO_BUTTON_SIZE_XS;
  if (size > COGITO_BUTTON_SIZE_XL) return (uint8_t)COGITO_BUTTON_SIZE_XL;
  return (uint8_t)size;
}

static uint8_t cogito_iconbtn_clamp_size_int(int size) {
  if (size < COGITO_BUTTON_SIZE_XS) return (uint8_t)COGITO_BUTTON_SIZE_XS;
  if (size > COGITO_BUTTON_SIZE_XL) return (uint8_t)COGITO_BUTTON_SIZE_XL;
  return (uint8_t)size;
}

// Get pressed/active corner radius for buttons based on size
// XS: 8dp, S: 8dp, M: 12dp, L: 16dp, XL: 16dp
static int cogito_button_pressed_radius(int size) {
  uint8_t clamped = cogito_button_clamp_size_int(size);
  switch (clamped) {
    case COGITO_BUTTON_SIZE_XS: return 8;
    case COGITO_BUTTON_SIZE_S:  return 8;
    case COGITO_BUTTON_SIZE_M:  return 12;
    case COGITO_BUTTON_SIZE_L:  return 16;
    case COGITO_BUTTON_SIZE_XL: return 16;
    default: return 12;
  }
}

static void cogito_button_size_metrics(int size, int kind, int* out_h, int* out_pad_x, int* out_icon, int* out_gap) {
  // Size tiers = (XS, S, M, L, XL):
  // Heights:  XS=32, S=40, M=56, L=96, XL=136
  // Icons:    XS=20, S=24, M=24, L=32, XL=40
  // Padding:  XS=12, S=16, M=24, L=48, XL=64 (only for buttons, not iconbtn)
  // Gap:      XS=4,  S=8,  M=8,  L=12, XL=16
  uint8_t clamped = cogito_button_clamp_size_int(size);
  int h = 0, pad_x = 0, icon = 0, gap = 0;
  if (clamped == COGITO_BUTTON_SIZE_XS) {
    h = 32;
    pad_x = 12;
    icon = 20;
    gap = 4;
  } else if (clamped == COGITO_BUTTON_SIZE_S) {
    h = 40;
    pad_x = 16;
    icon = 24;
    gap = 8;
  } else if (clamped == COGITO_BUTTON_SIZE_M) {
    h = 56;
    pad_x = 24;
    icon = 24;
    gap = 8;
  } else if (clamped == COGITO_BUTTON_SIZE_L) {
    h = 96;
    pad_x = 48;
    icon = 32;
    gap = 12;
  } else if (clamped == COGITO_BUTTON_SIZE_XL) {
    h = 136;
    pad_x = 64;
    icon = 40;
    gap = 16;
  } else {
    h = 56;
    pad_x = 24;
    icon = 24;
    gap = 8;
  }
  if (out_h) *out_h = h;
  if (out_pad_x) {
    if (kind == COGITO_BUTTON) {
      *out_pad_x = pad_x;
    } else {
      // For non-button kinds (like iconbtn), set padding to 0
      *out_pad_x = 0;
    }
  }
  if (out_icon) *out_icon = icon;
  if (out_gap) {
    if (kind == COGITO_BUTTON) {
      *out_gap = gap;
    } else {
      // For non-button kinds (like iconbtn), set gap to 0
      *out_gap = 0;
    }
  }
}

static bool cogito_drag_active = false;
static CogitoVec2 cogito_drag_start_mouse = {
  0
};
static CogitoVec2 cogito_drag_start_win = {
  0
};
static bool cogito_drag_pending = false;
static CogitoVec2 cogito_drag_pending_mouse = {
  0
};
static double cogito_drag_last_move = 0.0;
static bool cogito_inspector_enabled = false;
static bool cogito_inspector_open = false;
static CogitoNode* cogito_inspector_selected = NULL;
static int cogito_inspector_view_x = 0;
static int cogito_inspector_view_y = 0;
static int cogito_inspector_view_w = 0;
static int cogito_inspector_view_h = 0;
static int cogito_main_pointer_x = 0;
static int cogito_main_pointer_y = 0;
static bool cogito_main_pointer_valid = false;
static bool cogito_main_pointer_routed = false;
static bool cogito_main_pointer_down = false;
static bool cogito_main_pointer_pressed = false;

static inline bool cogito_pointer_query(int* out_x, int* out_y,
                                        bool* out_down, bool* out_pressed) {
  if (out_x) *out_x = 0;
  if (out_y) *out_y = 0;
  if (out_down) *out_down = false;
  if (out_pressed) *out_pressed = false;
  if (!cogito_backend) return false;

  int mx = 0;
  int my = 0;
  bool down = false;
  bool pressed = false;

  if (cogito_main_pointer_valid) {
    if (!cogito_main_pointer_routed) return false;
    mx = cogito_main_pointer_x;
    my = cogito_main_pointer_y;
    down = cogito_main_pointer_down;
    pressed = cogito_main_pointer_pressed;
  } else if (cogito_active_backend_window && cogito_backend->get_mouse_position_in_window) {
    cogito_backend->get_mouse_position_in_window(cogito_active_backend_window, &mx, &my);
    down = cogito_backend->is_mouse_button_down && cogito_backend->is_mouse_button_down(0);
    pressed = cogito_backend->is_mouse_button_pressed && cogito_backend->is_mouse_button_pressed(0);
  } else if (cogito_backend->get_mouse_position) {
    cogito_backend->get_mouse_position(&mx, &my);
    down = cogito_backend->is_mouse_button_down && cogito_backend->is_mouse_button_down(0);
    pressed = cogito_backend->is_mouse_button_pressed && cogito_backend->is_mouse_button_pressed(0);
  } else {
    return false;
  }

  if (out_x) *out_x = mx;
  if (out_y) *out_y = my;
  if (out_down) *out_down = down;
  if (out_pressed) *out_pressed = pressed;
  return true;
}

static inline void cogito_theme_mark_action_origin(int x, int y) {
  cogito_theme_action_origin_x = x;
  cogito_theme_action_origin_y = y;
  cogito_theme_action_origin_valid = true;
  cogito_theme_action_origin_time = cogito_now();
}

static inline bool cogito_theme_take_action_origin(int* out_x, int* out_y) {
  if (!cogito_theme_action_origin_valid) return false;
  double now = cogito_now();
  if (now - cogito_theme_action_origin_time > 0.8) {
    cogito_theme_action_origin_valid = false;
    return false;
  }
  if (out_x) *out_x = cogito_theme_action_origin_x;
  if (out_y) *out_y = cogito_theme_action_origin_y;
  cogito_theme_action_origin_valid = false;
  return true;
}

static inline bool cogito_theme_has_reveal_target(void) {
  return cogito_active_window &&
         cogito_active_window->kind == COGITO_WINDOW &&
         cogito_active_window->w > 0 &&
         cogito_active_window->h > 0;
}

static inline void cogito_theme_request_window_reveal(void) {
  // Ignore theme reveal requests during startup/setup while no drawable
  // window exists. This avoids stale pending ripples on first paint.
  if (!cogito_theme_has_reveal_target()) {
    cogito_theme_window_reveal_pending = false;
    cogito_theme_action_origin_valid = false;
    return;
  }
  cogito_theme_window_reveal_pending = true;
}

static float cogito_window_theme_reveal_max_radius(const CogitoNode* win, int ox, int oy) {
  if (!win || win->w <= 0 || win->h <= 0) return 0.0f;
  int x0 = win->x;
  int y0 = win->y;
  int x1 = win->x + win->w;
  int y1 = win->y + win->h;
  float d0 = hypotf((float)(ox - x0), (float)(oy - y0));
  float d1 = hypotf((float)(ox - x1), (float)(oy - y0));
  float d2 = hypotf((float)(ox - x0), (float)(oy - y1));
  float d3 = hypotf((float)(ox - x1), (float)(oy - y1));
  float max_d = d0;
  if (d1 > max_d) max_d = d1;
  if (d2 > max_d) max_d = d2;
  if (d3 > max_d) max_d = d3;
  return max_d;
}

static void cogito_window_theme_reveal_start(CogitoNode* win, int origin_x, int origin_y, CogitoColor old_bg, CogitoColor new_bg) {
  if (!win || win->kind != COGITO_WINDOW || win->w <= 0 || win->h <= 0) return;
  if (cogito_prefers_reduced_motion) {
    win->window_theme_reveal_active = false;
    return;
  }
  if (origin_x < win->x) origin_x = win->x;
  if (origin_x > win->x + win->w) origin_x = win->x + win->w;
  if (origin_y < win->y) origin_y = win->y;
  if (origin_y > win->y + win->h) origin_y = win->y + win->h;

  win->window_theme_reveal_active = true;
  win->window_theme_reveal_start = cogito_now();
  win->window_theme_reveal_duration = 0.52;
  win->window_theme_reveal_origin_x = origin_x;
  win->window_theme_reveal_origin_y = origin_y;
  win->window_theme_reveal_radius = 0.0f;
  win->window_theme_reveal_max_radius = cogito_window_theme_reveal_max_radius(win, origin_x, origin_y);
  win->window_theme_reveal_old_bg = old_bg;
  win->window_theme_reveal_new_bg = new_bg;

  double end_time = win->window_theme_reveal_start + win->window_theme_reveal_duration;
  if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
}
#define COGITO_INSPECTOR_MAX_NODES 2048
static CogitoNode* cogito_inspector_nodes[COGITO_INSPECTOR_MAX_NODES
];
static int cogito_inspector_depths[COGITO_INSPECTOR_MAX_NODES
];
static int cogito_inspector_node_count = 0;
static CogitoNode* cogito_inspector_collapsed_nodes[COGITO_INSPECTOR_MAX_NODES
];
static bool cogito_inspector_collapsed_values[COGITO_INSPECTOR_MAX_NODES
];
static int cogito_inspector_collapsed_count = 0;
static int cogito_inspector_tree_scroll_y = 0;
#define COGITO_INSPECTOR_STATUS_H 24
/* Scrollbar thumb drag (one at a time) */
static CogitoNode* cogito_scrollbar_drag_scroller = NULL;
static bool cogito_scrollbar_drag_vert = false;
static int cogito_scrollbar_drag_start_mouse = 0;
static int cogito_scrollbar_drag_start_scroll = 0;
static int cogito_scrollbar_drag_max = 0; /* max scroll (max_y or max_x) */
static int cogito_scrollbar_drag_travel = 1; /* track length minus thumb (pixels) */

static CogitoWindow* cogito_backend_window_for_node(CogitoNode* node) {
  if (!node || !cogito_active_window || !cogito_active_backend_window) return NULL;
  CogitoNode* root = node;
  while (root->parent) {
    root = root->parent;
  }
  if (root == cogito_active_window) return cogito_active_backend_window;
  return NULL;
}

#if defined(__APPLE__)
typedef unsigned long CogitoNSUInteger;
typedef long CogitoNSInteger;

static id cogito_nsstring(const char* s) {
  id nsstring = (id)objc_getClass("NSString");
  SEL sel = sel_registerName("stringWithUTF8String:");
  return ((id (*)(id, SEL, const char*))objc_msgSend)(nsstring, sel, s ? s : "");
}

static void cogito_configure_macos_window(bool has_appbar) {
  if (!has_appbar) return;
  // Get native window handle from backend
  if (!cogito_backend || !cogito_active_backend_window) return;
  id nsw =
#if defined(__OBJC__)
      (__bridge id)cogito_backend->window_get_native_handle(cogito_active_backend_window);
#else
      (id)cogito_backend->window_get_native_handle(cogito_active_backend_window);
#endif
  if (!nsw) return;
  SEL sel_set_title = sel_registerName("setTitle:");
  ((void (*)(id, SEL, id))objc_msgSend)(nsw, sel_set_title, cogito_nsstring(""));
  SEL sel_set_title_visibility = sel_registerName("setTitleVisibility:");
  ((void (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_set_title_visibility,
  1UL);
  SEL sel_set_transparent = sel_registerName("setTitlebarAppearsTransparent:");
  ((void (*)(id, SEL, BOOL))objc_msgSend)(nsw, sel_set_transparent, (BOOL)1);
  SEL sel_set_movable = sel_registerName("setMovableByWindowBackground:");
  // Dragging should start only from Cogito appbar hit-testing, not full-window background.
  ((void (*)(id, SEL, BOOL))objc_msgSend)(nsw, sel_set_movable, (BOOL)0);
  SEL sel_style_mask = sel_registerName("styleMask");
  CogitoNSUInteger mask = ((CogitoNSUInteger (*)(id, SEL))objc_msgSend)(nsw, sel_style_mask);
  mask |= (1UL << 15);
  SEL sel_set_style_mask = sel_registerName("setStyleMask:");
  ((void (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_set_style_mask, mask);
  SEL sel_standard_button = sel_registerName("standardWindowButton:");
  id close = ((id (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_standard_button,
  0UL);
  id min = ((id (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_standard_button,
  1UL);
  id zoom = ((id (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_standard_button,
  2UL);
  SEL sel_set_hidden = sel_registerName("setHidden:");
  if (close) ((void (*)(id, SEL, BOOL))objc_msgSend)(close, sel_set_hidden, (BOOL)1);
  if (min) ((void (*)(id, SEL, BOOL))objc_msgSend)(min, sel_set_hidden, (BOOL)1);
  if (zoom) ((void (*)(id, SEL, BOOL))objc_msgSend)(zoom, sel_set_hidden, (BOOL)1);
}

static void cogito_macos_drag_window(void) {
  if (!cogito_backend || !cogito_active_backend_window) return;
  id nsw =
#if defined(__OBJC__)
      (__bridge id)cogito_backend->window_get_native_handle(cogito_active_backend_window);
#else
      (id)cogito_backend->window_get_native_handle(cogito_active_backend_window);
#endif
  if (!nsw) return;
  Class nsapp = (Class)objc_getClass("NSApplication");
  if (!nsapp) return;
  SEL sel_shared = sel_registerName("sharedApplication");
  id app = ((id (*)(id, SEL))objc_msgSend)((id)nsapp, sel_shared);
  if (!app) return;
  SEL sel_event = sel_registerName("currentEvent");
  id event = ((id (*)(id, SEL))objc_msgSend)(app, sel_event);
  if (!event) return;
  SEL sel_drag = sel_registerName("performWindowDragWithEvent:");
  ((void (*)(id, SEL, id))objc_msgSend)(nsw, sel_drag, event);
}

static void cogito_macos_set_window_a11y_label(const char* label) {
  if (!cogito_backend || !cogito_active_backend_window) return;
  id nsw =
#if defined(__OBJC__)
      (__bridge id)cogito_backend->window_get_native_handle(cogito_active_backend_window);
#else
      (id)cogito_backend->window_get_native_handle(cogito_active_backend_window);
#endif
  if (!nsw) return;
  SEL sel = sel_registerName("setAccessibilityLabel:");
  if (!sel) return;
  ((void (*)(id, SEL, id))objc_msgSend)(nsw, sel, cogito_nsstring(label ? label : ""));
}

static void cogito_macos_set_window_shadow(CogitoWindow* backend_window, bool enable) {
  if (!cogito_backend || !backend_window) return;
  id nsw =
#if defined(__OBJC__)
      (__bridge id)cogito_backend->window_get_native_handle(backend_window);
#else
      (id)cogito_backend->window_get_native_handle(backend_window);
#endif
  if (!nsw) return;
  SEL sel = sel_registerName("setHasShadow:");
  if (!sel) return;
  ((void (*)(id, SEL, BOOL))objc_msgSend)(nsw, sel, (BOOL)(enable ? 1 : 0));
}

static void cogito_macos_set_process_name(const char* name) {
  const char* process_name = (name && name[
    0
  ]) ? name : COGITO_USER_VERSION;
  setprogname(process_name);
  Class nsprocess = (Class)objc_getClass("NSProcessInfo");
  if (!nsprocess) return;
  SEL sel_process_info = sel_registerName("processInfo");
  id process_info = ((id (*)(id, SEL))objc_msgSend)((id)nsprocess, sel_process_info);
  if (!process_info) return;
  SEL sel_set_name = sel_registerName("setProcessName:");
  if (!sel_set_name) return;
  ((void (*)(id, SEL, id))objc_msgSend)(process_info, sel_set_name, cogito_nsstring(process_name));
}

static bool cogito_macos_apply_app_name(const char* name) {
  const char* app_name = (name && name[
    0
  ]) ? name : COGITO_USER_VERSION;
  cogito_macos_set_process_name(app_name);

  Class nsapp = (Class)objc_getClass("NSApplication");
  if (!nsapp) return false;
  SEL sel_shared = sel_registerName("sharedApplication");
  id app = ((id (*)(id, SEL))objc_msgSend)((id)nsapp, sel_shared);
  if (!app) return false;

  SEL sel_main_menu = sel_registerName("mainMenu");
  id main_menu = ((id (*)(id, SEL))objc_msgSend)(app, sel_main_menu);
  if (!main_menu) return false;

  SEL sel_item_at_index = sel_registerName("itemAtIndex:");
  id app_item = ((id (*)(id, SEL, CogitoNSInteger))objc_msgSend)(main_menu, sel_item_at_index, (CogitoNSInteger)0);
  if (!app_item) return false;

  SEL sel_set_title = sel_registerName("setTitle:");
  ((void (*)(id, SEL, id))objc_msgSend)(app_item, sel_set_title, cogito_nsstring(app_name));
  return true;
}

#endif

// Use cogito_color from backend.h instead
#define cogito_rgba cogito_color

static bool cogito_font_family_eq(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

static int cogito_font_size(void);
static int cogito_font_weight_normal(void);
static bool cogito_load_font_file(CogitoFont** out, const char* path, int size, int weight);

static CogitoFont* cogito_font_for_node(const CogitoNode* n) {
  if (n && n->font_mono) {
    if (!cogito_font_mono_ready &&
        cogito_font_mono_from_env &&
        cogito_font_mono_path_active &&
        cogito_font_mono_path_active[0]) {
      if (cogito_load_font_file(&cogito_font_mono, cogito_font_mono_path_active,
                                cogito_font_size(), cogito_font_weight_normal())) {
        cogito_font_mono_ready = true;
        cogito_font_mono_loaded_size = cogito_font_size();
      }
    }
    if (cogito_font_mono_ready) return cogito_font_mono;
  }
  if (n && n->font_family_set && n->font_family[
    0
  ]) {
    if (cogito_font_family_eq(n->font_family, "monospace") ||
        cogito_font_family_eq(n->font_family, "mono")) {
      if (!cogito_font_mono_ready &&
          cogito_font_mono_from_env &&
          cogito_font_mono_path_active &&
          cogito_font_mono_path_active[0]) {
        if (cogito_load_font_file(&cogito_font_mono, cogito_font_mono_path_active,
                                  cogito_font_size(), cogito_font_weight_normal())) {
          cogito_font_mono_ready = true;
          cogito_font_mono_loaded_size = cogito_font_size();
        }
      }
      if (cogito_font_mono_ready) return cogito_font_mono;
    }
    if (cogito_font_family_eq(n->font_family, "serif")) {
      if (!cogito_font_serif_ready &&
          cogito_font_serif_from_env &&
          cogito_font_serif_path_active &&
          cogito_font_serif_path_active[0]) {
        if (cogito_load_font_file(&cogito_font_serif, cogito_font_serif_path_active,
                                  cogito_font_size(), cogito_font_weight_normal())) {
          cogito_font_serif_ready = true;
          cogito_font_serif_loaded_size = cogito_font_size();
        }
      }
      if (cogito_font_serif_ready) return cogito_font_serif;
    }
  }
  return cogito_font;
}
// Use backend.h implementations
#define cogito_color_eq(a, b) ((a).r == (b).r && (a).g == (b).g && (a).b == (b).b && (a).a == (b).a)

// Legacy easing (kept for any remaining non-spring paths)
#define cogito_ease cogito_backend_ease

static double cogito_backend_ease(double t, int ease) {
  if (t <= 0.0) return 0.0;
  if (t >= 1.0) return 1.0;
  switch (ease) {
    case 2: return t * t; // ease-in
    case 3: { double u = 1.0 - t; return 1.0 - u * u;
    } // ease-out
    case 4: { // ease-in-out
      if (t < 0.5) return 2.0 * t * t;
      double u = 1.0 - t;
      return 1.0 - 2.0 * u * u;
    }
    case 1: // ease
      return t * t * (3.0 - 2.0 * t);
    case 5: { // spring (ease-in-back)
      double s = 1.70158;
      return t * t * ((s + 1) * t - s);
    }
    case 6: { // spring-out (ease-out-back)
      double u = t - 1;
      double s = 1.70158;
      return u * u * ((s + 1) * u + s) + 1;
    }
    default: return t; // linear
  }
}

static double cogito_cubic_bezier_axis(double p1, double p2, double u) {
  double omt = 1.0 - u;
  return 3.0 * omt * omt * u * p1 +
         3.0 * omt * u * u * p2 +
         u * u * u;
}

static double cogito_cubic_bezier_axis_derivative(double p1, double p2, double u) {
  double omt = 1.0 - u;
  return 3.0 * omt * omt * p1 +
         6.0 * omt * u * (p2 - p1) +
         3.0 * u * u * (1.0 - p2);
}

// CSS cubic-bezier solver: given progress t on x-axis, returns y-axis progress.
static double cogito_ease_cubic_bezier(double t,
                                       double x1, double y1,
                                       double x2, double y2) {
  if (t <= 0.0) return 0.0;
  if (t >= 1.0) return 1.0;

  // Newton-Raphson on x(u) = t.
  double u = t;
  for (int i = 0; i < 6; i++) {
    double x = cogito_cubic_bezier_axis(x1, x2, u) - t;
    double dx = cogito_cubic_bezier_axis_derivative(x1, x2, u);
    if (fabs(dx) < 1e-6) break;
    u -= x / dx;
    if (u < 0.0) { u = 0.0; break; }
    if (u > 1.0) { u = 1.0; break; }
  }

  // Refine with bisection for stability.
  double lo = 0.0;
  double hi = 1.0;
  for (int i = 0; i < 10; i++) {
    double x = cogito_cubic_bezier_axis(x1, x2, u);
    if (x < t) lo = u;
    else hi = u;
    u = 0.5 * (lo + hi);
  }

  return cogito_cubic_bezier_axis(y1, y2, u);
}
// ---- Physics-driven spring color animation ----
// Advances 4 RGBA spring channels toward a target color using the Expressive profile.
// Returns the resulting interpolated color. Continuity is guaranteed because
// each channel's velocity is preserved even when the target changes mid-flight.
static CogitoColor cogito_spring_color_tick(
    CogitoSpring* sr, CogitoSpring* sg, CogitoSpring* sb, CogitoSpring* sa,
    CogitoColor target, double k, double z, double dt) {
  cogito_spring_tick(sr, (double)target.r, k, z, dt);
  cogito_spring_tick(sg, (double)target.g, k, z, dt);
  cogito_spring_tick(sb, (double)target.b, k, z, dt);
  cogito_spring_tick(sa, (double)target.a, k, z, dt);
  // Clamp channel values to [0,255]
  int r = (int)(sr->x + 0.5); if (r < 0) r = 0; if (r > 255) r = 255;
  int g = (int)(sg->x + 0.5); if (g < 0) g = 0; if (g > 255) g = 255;
  int b = (int)(sb->x + 0.5); if (b < 0) b = 0; if (b > 255) b = 255;
  int a = (int)(sa->x + 0.5); if (a < 0) a = 0; if (a > 255) a = 255;
  return (CogitoColor){(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a
  };
}
// Returns true if any of the four RGBA spring channels are still moving
static bool cogito_spring_color_active(
    const CogitoSpring* sr, const CogitoSpring* sg,
    const CogitoSpring* sb, const CogitoSpring* sa,
    CogitoColor target) {
  return cogito_spring_active(sr, (double)target.r) ||
         cogito_spring_active(sg, (double)target.g) ||
         cogito_spring_active(sb, (double)target.b) ||
         cogito_spring_active(sa, (double)target.a);
}
// Legacy time-based lerp animation (kept for backward compatibility with any widget-specific paths)
// These are no longer called by cogito_anim_apply (which uses the spring solver instead).
__attribute__((unused))
static CogitoColor cogito_anim_color(CogitoNode* n, CogitoColor target, CogitoColor* cur, CogitoColor* from, CogitoColor* tgt, double* start, double now) {
  if (!n) return target;
  if (!n->anim_inited) {
    *cur = target;
    *from = target;
    *tgt = target;
    if (start) *start = now;
    n->anim_inited = true;
    return target;
  }
  bool changed = !(tgt->r == target.r && tgt->g == target.g && tgt->b == target.b && tgt->a == target.a);
  if (changed) {
    bool first_init = (tgt->r == 0 && tgt->g == 0 && tgt->b == 0 && tgt->a == 0 &&
                       cur->r == 0 && cur->g == 0 && cur->b == 0 && cur->a == 0);
    if (first_init) {
      // Channel not yet initialized — snap to target without animation
      *cur = target;
      *from = target;
      *tgt = target;
      if (start) *start = now;
      return target;
    }
    *from = *cur;
    *tgt = target;
    if (start) *start = now;
    if (n->anim_ms > 0) {
      double end_time = now + ((double)n->anim_ms / 1000.0);
      if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    }
  }
  if (n->anim_ms <= 0) {
    *cur = target;
    if (start) *start = now;
    return target;
  }
  if (!changed && cur->r == target.r && cur->g == target.g && cur->b == target.b && cur->a == target.a) {
    return *cur;
  }
  double t0 = start ? *start : n->anim_start;
  double t = (now - t0) * 1000.0 / (double)n->anim_ms;
  if (t >= 1.0) {
    *cur = target;
    *from = target; // Ensure from matches target to avoid residual interpolation
    *tgt = target;
    return target;
  }
  double e = cogito_ease(t, n->anim_ease);
  *cur = cogito_color_lerp(*from, *tgt, e);
  return *cur;
}

static int cogito_anim_int(CogitoNode* n, int target, int* cur, int* from, int* tgt, double* start, double now) {
  if (!n) return target;
  if (cogito_prefers_reduced_motion) {
    *cur = target;
    *from = target;
    *tgt = target;
    if (start) *start = now;
    n->anim_inited = true;
    return target;
  }
  if (!n->anim_inited) {
    *cur = target;
    *from = target;
    *tgt = target;
    if (start) *start = now;
    n->anim_inited = true;
    return target;
  }
  bool changed = (*tgt != target);
  if (changed) {
    *from = *cur;
    *tgt = target;
    if (start) *start = now;
    if (n->anim_ms > 0) {
      double end_time = now + ((double)n->anim_ms / 1000.0);
      if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    }
  }
  if (n->anim_ms <= 0) {
    *cur = target;
    if (start) *start = now;
    return target;
  }
  if (!changed && *cur == target) {
    return *cur;
  }
  double t0 = start ? *start : n->anim_start;
  double t = (now - t0) * 1000.0 / (double)n->anim_ms;
  if (t >= 1.0) {
    *cur = target;
    *from = target; // Ensure from matches target to avoid residual interpolation
    *tgt = target;
    return target;
  }
  double e = cogito_ease(t, n->anim_ease);
  *cur = (int)(*from + e * (*tgt - *from));
  return *cur;
}
// Use backend.h implementation
#define cogito_apply_opacity cogito_color_apply_opacity

__attribute__((unused))
static float cogito_anim_float(CogitoNode* n, float target, float* cur, float* from, float* tgt, double* start, double now) {
  if (!n) return target;
  if (cogito_prefers_reduced_motion) {
    *cur = target;
    *from = target;
    *tgt = target;
    if (start) *start = now;
    n->anim_inited = true;
    return target;
  }
  if (!n->anim_inited) {
    *cur = target;
    *from = target;
    *tgt = target;
    if (start) *start = now;
    n->anim_inited = true;
    return target;
  }
  bool changed = (*tgt != target);
  if (changed) {
    *from = *cur;
    *tgt = target;
    if (start) *start = now;
    if (n->anim_ms > 0) {
      double end_time = now + ((double)n->anim_ms / 1000.0);
      if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    }
  }
  if (n->anim_ms <= 0) {
    *cur = target;
    if (start) *start = now;
    return target;
  }
  if (!changed && *cur == target) {
    return *cur;
  }
  double t0 = start ? *start : n->anim_start;
  double t = (now - t0) * 1000.0 / (double)n->anim_ms;
  if (t >= 1.0) {
    *cur = target;
    *from = target; // Ensure from matches target to avoid residual interpolation
    *tgt = target;
    return target;
  }
  double e = cogito_ease(t, n->anim_ease);
  *cur = *from + (float)e * (*tgt - *from);
  return *cur;
}

static void cogito_anim_apply(CogitoNode* n, const CogitoStyle* s, CogitoColor* bg, CogitoColor* text, CogitoColor* border, CogitoColor* selection) {
  if (!n || !s) return;
  if (s->has_transition_ms) n->anim_ms = s->transition_ms;
  if (s->has_transition_ease) n->anim_ease = s->transition_ease;
  double now = cogito_now();
  double dt = cogito_spring_dt; // frame-rate-independent delta-time

  if (cogito_prefers_reduced_motion) {
    float opacity = n->opacity_set ? n->opacity : 1.0f;
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;
    if (bg) *bg = cogito_apply_opacity(*bg, opacity);
    if (text) *text = cogito_apply_opacity(*text, opacity);
    if (border) *border = cogito_apply_opacity(*border, opacity);
    if (selection) *selection = cogito_apply_opacity(*selection, opacity);
    n->anim_inited = true;
    return;
  }

  // Choose spring profile: expressive for UI elements, functional for opacity
  double k_color = COGITO_SPRING_K_EXPRESSIVE;
  double z_color = COGITO_SPRING_Z_EXPRESSIVE;
  double k_op    = COGITO_SPRING_K_FUNCTIONAL;
  double z_op    = COGITO_SPRING_Z_FUNCTIONAL;

  // Animate opacity via functional spring (no overshoot on opacity)
  float opacity = n->opacity;
  if (n->opacity_set) {
    cogito_spring_tick(&n->spring_opacity, (double)n->opacity, k_op, z_op, dt);
    float sp_op = (float)n->spring_opacity.x;
    if (sp_op < 0.0f) sp_op = 0.0f;
    if (sp_op > 1.0f) sp_op = 1.0f;
    opacity = sp_op;
    // Keep legacy float anim in sync for external readers
    n->opacity_from = n->opacity;
    n->opacity_target = n->opacity;
    if (cogito_spring_active(&n->spring_opacity, (double)n->opacity)) {
      double end_time = now + 0.6;
      if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    }
  }
  // Animate colors via expressive spring (overshoot = liveliness)
  if (bg) {
    *bg = cogito_spring_color_tick(
        &n->spring_bg_r, &n->spring_bg_g, &n->spring_bg_b, &n->spring_bg_a,
        *bg, k_color, z_color, dt);
    *bg = cogito_apply_opacity(*bg, opacity);
    if (cogito_spring_color_active(&n->spring_bg_r, &n->spring_bg_g,
                                    &n->spring_bg_b, &n->spring_bg_a, *bg)) {
      double end_time = now + 0.6;
      if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    }
  }
  if (text) {
    *text = cogito_spring_color_tick(
        &n->spring_txt_r, &n->spring_txt_g, &n->spring_txt_b, &n->spring_txt_a,
        *text, k_color, z_color, dt);
    *text = cogito_apply_opacity(*text, opacity);
    if (cogito_spring_color_active(&n->spring_txt_r, &n->spring_txt_g,
                                    &n->spring_txt_b, &n->spring_txt_a, *text)) {
      double end_time = now + 0.6;
      if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    }
  }
  if (border) {
    *border = cogito_spring_color_tick(
        &n->spring_bdr_r, &n->spring_bdr_g, &n->spring_bdr_b, &n->spring_bdr_a,
        *border, k_color, z_color, dt);
    *border = cogito_apply_opacity(*border, opacity);
    if (cogito_spring_color_active(&n->spring_bdr_r, &n->spring_bdr_g,
                                    &n->spring_bdr_b, &n->spring_bdr_a, *border)) {
      double end_time = now + 0.6;
      if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    }
  }
  if (selection) {
    *selection = cogito_spring_color_tick(
        &n->spring_sel_r, &n->spring_sel_g, &n->spring_sel_b, &n->spring_sel_a,
        *selection, k_color, z_color, dt);
    *selection = cogito_apply_opacity(*selection, opacity);
    if (cogito_spring_color_active(&n->spring_sel_r, &n->spring_sel_g,
                                    &n->spring_sel_b, &n->spring_sel_a, *selection)) {
      double end_time = now + 0.6;
      if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    }
  }
  (void)now; // used via cogito_anim_end_time
  // ---- State-Driven Typography: font weight spring (wght axis) ----
  // Hover/Focus/Selection states trigger a weight-axis shift instead of color-only.
  // Targets: Normal=400, Hover=500, Focus/Select=600
  // Only applies when the backend supports font_set_variation.
  if (cogito_backend && cogito_backend->font_set_variation) {
    // Determine target weight from node state
    int wght_target = 400;
    // The node's font_weight field encodes hover/focus/selected weight from theme
    if (n->font_weight_set && n->font_weight > 400) {
      wght_target = n->font_weight;
    }
    cogito_spring_tick(&n->spring_font_weight, (double)wght_target,
                       COGITO_SPRING_K_FUNCTIONAL, COGITO_SPRING_Z_FUNCTIONAL, cogito_spring_dt);
    // Only apply to the active font if there is text to render
    if (n->text && n->text->data && n->text->data[
      0
    ]) {
      CogitoFont* active_font = cogito_font;
      if (active_font && cogito_backend->font_set_variation) {
        // 'wght' tag packed big-endian: 0x77676874
        cogito_backend->font_set_variation(active_font,
        0x77676874u,
                                           (float)n->spring_font_weight.x);
      }
    }
    if (cogito_spring_active(&n->spring_font_weight, (double)wght_target)) {
      double end_time = now + 0.6;
      if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
    }
  }
}
// Ripple animation constants (optimized for low CPU)
#define COGITO_RIPPLE_DURATION_MS 350
#define COGITO_RIPPLE_FPS 30

// Check if a node should have ripple effect based on its class name
// Returns true if the node has Primary, Secondary, or Tertiary background,
// or if it's a button-like element with Surface variant
static bool cogito_node_should_ripple(const CogitoNode* n) {
  if (!n) return false;
  
  // Check class name for color variants
  if (n->class_name && n->class_name->data && n->class_name->data[0]) {
    const char* cls = n->class_name->data;
    // Primary, Secondary, Tertiary backgrounds should have ripple
    if (strstr(cls, "primary") != NULL) return true;
    if (strstr(cls, "secondary") != NULL) return true;
    if (strstr(cls, "tertiary") != NULL) return true;
    // Surface variant on button-like elements should have ripple
    if (strstr(cls, "surface") != NULL) {
      if (n->kind == COGITO_BUTTON || n->kind == COGITO_ICONBTN ||
          n->kind == COGITO_FAB || n->kind == COGITO_FAB_MENU ||
          n->kind == COGITO_CHIP || n->kind == COGITO_SPLIT_BUTTON) {
        return true;
      }
    }
  }
  
  // Check if node is in a ButtonGroup (selected items use Secondary color)
  if (n->parent && n->parent->kind == COGITO_BUTTON_GROUP) {
    return true;
  }
  
  return false;
}

static void cogito_theme_get_state_layer_palette(
  CogitoColor* out_primary,
  CogitoColor* out_secondary,
  CogitoColor* out_secondary_container,
  CogitoColor* out_tertiary,
  CogitoColor* out_surface,
  CogitoColor* out_on_surface,
  CogitoColor* out_on_primary_container,
  CogitoColor* out_on_secondary_container,
  CogitoColor* out_on_tertiary_container,
  CogitoColor* out_on_surface_variant
);

static CogitoColor cogito_node_state_layer_seed(const CogitoNode* n) {
  CogitoColor primary = cogito_theme.iconbtn_filled.bg;
  CogitoColor secondary = cogito_theme.iconbtn_tonal.bg;
  CogitoColor tertiary = primary;
  CogitoColor surface = cogito_theme.per_kind[COGITO_WINDOW].has_bg
    ? cogito_theme.per_kind[COGITO_WINDOW].bg
    : cogito_rgba(128, 128, 128, 255);
  CogitoColor on_surface = cogito_theme.per_kind[COGITO_WINDOW].has_text
    ? cogito_theme.per_kind[COGITO_WINDOW].text
    : cogito_rgba(255, 255, 255, 255);
  CogitoColor on_primary_container = on_surface;
  CogitoColor on_secondary_container = on_surface;
  CogitoColor on_tertiary_container = on_surface;
  CogitoColor on_surface_variant = on_surface;
  cogito_theme_get_state_layer_palette(&primary, &secondary, NULL, &tertiary, &surface, &on_surface,
                                       &on_primary_container, &on_secondary_container,
                                       &on_tertiary_container, &on_surface_variant);
  const char* cls = (n && n->class_name && n->class_name->data) ? n->class_name->data : NULL;
  bool primary_container = cls && (strstr(cls, "primary-container") || strstr(cls, "primary_container"));
  bool secondary_container = cls && (strstr(cls, "secondary-container") || strstr(cls, "secondary_container"));
  bool tertiary_container = cls && (strstr(cls, "tertiary-container") || strstr(cls, "tertiary_container"));
  bool surface_variant = cls && (strstr(cls, "surface-variant") || strstr(cls, "surface_variant"));
  bool iconic_class = cls && strstr(cls, "iconic") != NULL;
  bool iconic_iconbtn = n && n->kind == COGITO_ICONBTN &&
                        (n->button.icon_color_style == 3 || iconic_class);
  if (n && n->kind == COGITO_ICONBTN && n->parent && n->parent->kind == COGITO_APPBAR) {
    if (n->button.icon_color_style == 3 || iconic_class) return primary;
  }
  if (primary_container) return on_primary_container;
  if (secondary_container) return on_secondary_container;
  if (tertiary_container) return on_tertiary_container;
  if (surface_variant) return on_surface_variant;
  if (cls && strstr(cls, "primary") != NULL) return primary;
  if (cls && strstr(cls, "secondary") != NULL) return secondary;
  if (cls && strstr(cls, "tertiary") != NULL) return tertiary;
  if (cls && strstr(cls, "surface") != NULL) return on_surface;
  if (iconic_iconbtn) return on_surface;
  return on_surface;
}

// Start a ripple animation at the given position (mouse-relative to widget)
static void cogito_ripple_start(CogitoNode* n, int mx, int my) {
  if (!n) return;
  if (cogito_prefers_reduced_motion) return;
  n->ripple_active = true;
  n->ripple_start = cogito_now();
  n->ripple_x = mx - n->x;
  n->ripple_y = my - n->y;
  // Pre-calculate max radius to cover entire widget from any click point
  int dx1 = n->ripple_x;
  int dy1 = n->ripple_y;
  int dx2 = n->w - n->ripple_x;
  int dy2 = n->h - n->ripple_y;
  float r1 = sqrtf((float)(dx1*dx1 + dy1*dy1));
  float r2 = sqrtf((float)(dx1*dx1 + dy2*dy2));
  float r3 = sqrtf((float)(dx2*dx2 + dy1*dy1));
  float r4 = sqrtf((float)(dx2*dx2 + dy2*dy2));
  float max_r = r1;
  if (r2 > max_r) max_r = r2;
  if (r3 > max_r) max_r = r3;
  if (r4 > max_r) max_r = r4;
  n->ripple_max_radius = max_r;
  n->ripple_progress = 0.0f;
  // Request animation frame
  double end_time = n->ripple_start + (COGITO_RIPPLE_DURATION_MS / 1000.0);
  if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
}
// Update ripple progress; returns true if still active
static bool cogito_ripple_update(CogitoNode* n) {
  if (!n || !n->ripple_active) return false;
  if (cogito_prefers_reduced_motion) {
    n->ripple_active = false;
    n->ripple_progress = 1.0f;
    return false;
  }
  double now = cogito_now();
  double elapsed_ms = (now - n->ripple_start) * 1000.0;
  if (elapsed_ms >= COGITO_RIPPLE_DURATION_MS) {
    n->ripple_active = false;
    n->ripple_progress = 1.0f;
    return false;
  }
  // Use ease-out curve for natural deceleration
  float t = (float)(elapsed_ms / COGITO_RIPPLE_DURATION_MS);
  n->ripple_progress = 1.0f - (1.0f - t) * (1.0f - t);
  // Request continued animation
  double end_time = n->ripple_start + (COGITO_RIPPLE_DURATION_MS / 1000.0);
  if (end_time > cogito_anim_end_time) cogito_anim_end_time = end_time;
  return true;
}
// Get current ripple radius and alpha for drawing
static void cogito_ripple_get(CogitoNode* n, float* out_radius, float* out_alpha) {
  if (!n || !n->ripple_active) {
    if (out_radius) *out_radius = 0;
    if (out_alpha) *out_alpha = 0;
    return;
  }
  float p = n->ripple_progress;
  if (out_radius) *out_radius = p * n->ripple_max_radius;
  // Alpha fades out quadratically in second half for smooth exit
  if (p < 0.5f) {
    if (out_alpha) *out_alpha = 0.25f; // Full opacity in first half
  } else {
    float fade = 1.0f - (p - 0.5f) * 2.0f;
    if (out_alpha) *out_alpha = 0.25f * fade * fade;
  }
}

static void cogito_style_clear(CogitoStyle* s) {
  memset(s,
  0, sizeof(*s));
}

static bool cogito_debug_enabled(void) {
  const char* v = getenv("COGITO_DEBUG");
  return v && v[
    0
  ] && v[
    0
  ] != '0';
}

static void cogito_draw_rect(int x, int y, int w, int h, CogitoColor bg, int radius);
static bool cogito_hit_rect(int x, int y, int rx, int ry, int rw, int rh);
static CogitoNode* cogito_node_window(CogitoNode* n);
static bool cogito_file_exists(const char* path);
static int cogito_font_weight_normal(void);
static int cogito_font_weight_bold(void);
static bool cogito_load_font_file_ex(CogitoFont** out, const char* path, int size, int weight, bool* out_weight_applied);
static bool cogito_load_font_file(CogitoFont** out, const char* path, int size, int weight);
static bool cogito_load_font_file_tabular(CogitoFont** out, const char* path, int size, int weight);
static CogitoFont* cogito_font_for_size(CogitoFont* f, int size);
static CogitoFont* cogito_font_tabular_for(CogitoFont* f, int size);
static void cogito_font_cache_clear(void);

void cogito_nav_rail_set_no_label(CogitoNode* n, bool val) {
  if (n) n->nav_rail.no_label = val;
}

bool cogito_nav_rail_get_no_label(CogitoNode* n) {
  return n ? n->nav_rail.no_label : false;
}
