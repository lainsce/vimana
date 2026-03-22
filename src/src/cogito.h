// Cogito C API (opaque handles, synchronous callbacks)
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CogitoApp cogito_app;
typedef struct CogitoNode cogito_node;
typedef struct CogitoNode cogito_window;
typedef struct CogitoTexture CogitoTexture;
typedef uint64_t cogito_timer_id;

typedef void (*cogito_node_fn)(cogito_node *node, void *user);
typedef void (*cogito_index_fn)(cogito_node *node, int index, void *user);
typedef void (*cogito_draw_fn)(cogito_node *node, int width, int height,
                               void *user);
typedef void (*cogito_timer_fn)(void *user);
typedef void (*cogito_timer_user_free_fn)(void *user);
typedef enum {
  COGITO_WINDOW_HITTEST_NORMAL = 0,
  COGITO_WINDOW_HITTEST_DRAGGABLE,
  COGITO_WINDOW_HITTEST_RESIZE_TOPLEFT,
  COGITO_WINDOW_HITTEST_RESIZE_TOP,
  COGITO_WINDOW_HITTEST_RESIZE_TOPRIGHT,
  COGITO_WINDOW_HITTEST_RESIZE_RIGHT,
  COGITO_WINDOW_HITTEST_RESIZE_BOTTOMRIGHT,
  COGITO_WINDOW_HITTEST_RESIZE_BOTTOM,
  COGITO_WINDOW_HITTEST_RESIZE_BOTTOMLEFT,
  COGITO_WINDOW_HITTEST_RESIZE_LEFT,
  COGITO_WINDOW_HITTEST_BUTTON_CLOSE,
  COGITO_WINDOW_HITTEST_BUTTON_MIN,
  COGITO_WINDOW_HITTEST_BUTTON_MAX,
} cogito_hit_test_result;
typedef cogito_hit_test_result (*cogito_hit_test_fn)(cogito_window *window,
                                                     int x, int y, void *user);

typedef enum {
  COGITO_NODE_WINDOW = 1,
  COGITO_NODE_APPBAR,
  COGITO_NODE_VSTACK,
  COGITO_NODE_HSTACK,
  COGITO_NODE_ZSTACK,
  COGITO_NODE_REVEALER,
  COGITO_NODE_BIN,
  COGITO_NODE_FIXED,
  COGITO_NODE_SCROLLER,
  COGITO_NODE_GRID,
  COGITO_NODE_LABEL,
  COGITO_NODE_BUTTON,
  COGITO_NODE_ICONBTN,
  COGITO_NODE_CHECKBOX,
  COGITO_NODE_SWITCH,
  COGITO_NODE_TEXTFIELD,
  COGITO_NODE_TEXTVIEW,
  COGITO_NODE_SEARCHFIELD,
  COGITO_NODE_DROPDOWN,
  COGITO_NODE_SLIDER,
  COGITO_NODE_TABS,
  COGITO_NODE_VIEW_SWITCHER,
  COGITO_NODE_PROGRESS,
  COGITO_NODE_DATEPICKER,
  COGITO_NODE_COLORPICKER,
  COGITO_NODE_FONTBUTTON,
  COGITO_NODE_STEPPER,
  COGITO_NODE_BUTTON_GROUP,
  COGITO_NODE_TREEVIEW,
  COGITO_NODE_TOASTS,
  COGITO_NODE_TOAST,
  COGITO_NODE_TOOLBAR,
  COGITO_NODE_CAROUSEL,
  COGITO_NODE_CAROUSEL_ITEM,
  COGITO_NODE_DIALOG,
  COGITO_NODE_DIALOG_SLOT,
  COGITO_NODE_TOOLTIP,
  COGITO_NODE_IMAGE,
  COGITO_NODE_CHIP,
  COGITO_NODE_FAB,
  COGITO_NODE_NAV_RAIL,
  COGITO_NODE_BOTTOM_NAV,
  COGITO_NODE_DIVIDER,
  COGITO_NODE_CARD,
  COGITO_NODE_AVATAR,
  COGITO_NODE_BADGE,
  COGITO_NODE_BANNER,
  COGITO_NODE_BOTTOM_SHEET,
  COGITO_NODE_SIDE_SHEET,
  COGITO_NODE_TIMEPICKER,
  COGITO_NODE_ACTIVE_INDICATOR,
  COGITO_NODE_SWITCHBAR,
  COGITO_NODE_CONTENT_LIST,
  COGITO_NODE_EMPTY_PAGE,
  COGITO_NODE_TIP_VIEW,
  COGITO_NODE_SETTINGS_WINDOW,
  COGITO_NODE_SETTINGS_PAGE,
  COGITO_NODE_SETTINGS_LIST,
  COGITO_NODE_SETTINGS_ROW,
  COGITO_NODE_WELCOME_SCREEN,
  COGITO_NODE_VIEW_DUAL,
  COGITO_NODE_VIEW_CHOOSER,
  COGITO_NODE_ABOUT_WINDOW,
  COGITO_NODE_SPLIT_BUTTON,
  COGITO_NODE_FAB_MENU,
  COGITO_NODE_DRAWING_AREA,
  COGITO_NODE_SHAPE,
  COGITO_NODE_WEBVIEW,
  COGITO_NODE_FLOW,
  COGITO_NODE_VIRTUAL_LIST,
  COGITO_NODE_VIDEO_VIEW,
} cogito_node_kind;

// App / window lifecycle
cogito_app *cogito_app_new(void);
cogito_app *cogito_app_get_active(void);
void cogito_app_free(cogito_app *app);
void cogito_app_run(cogito_app *app, cogito_window *window);

void cogito_app_set_appid(cogito_app *app, const char *rdnn);
void cogito_app_set_app_name(cogito_app *app, const char *name);
void cogito_app_set_accent_color(cogito_app *app, const char *hex,
                                 bool follow_system);
void cogito_app_set_dark_mode(cogito_app *app, bool dark, bool follow_system);
// Loads an image and extracts an Ensor accent, then applies it as app accent.
// Returns the applied #RRGGBB string, or NULL on failure.
const char *cogito_app_set_accent_from_image(cogito_app *app, const char *path,
                                             bool follow_system);
void cogito_app_set_ensor_variant(cogito_app *app, int variant);
void cogito_app_set_contrast(cogito_app *app, double contrast);
// Extract up to 4 accent colors from a pixel buffer (RGB or RGBA).
// out_argb receives up to 4 packed 0xRRGGBB values. Returns count written (1..4).
int cogito_accent_from_pixels(const unsigned char *pixels, int n_bytes,
                               bool alpha, int *out_argb, int out_cap);
void cogito_app_set_icon(cogito_app *app, const char *path);
const char *cogito_app_get_icon(cogito_app *app);
void cogito_app_set_mono_icon(cogito_app *app, const char *path);
void cogito_app_set_baseline_font(cogito_app *app, const char *path);
void cogito_app_set_emphasized_font(cogito_app *app, const char *path);
bool cogito_open_url(const char *url);
bool cogito_app_copy_to_clipboard(cogito_app *app, const char *text);
char *cogito_app_paste_from_clipboard(cogito_app *app);  // returns malloc'd string
bool cogito_clipboard_has(const char *mime_type);
void *cogito_clipboard_get_data(const char *mime_type, size_t *size);
bool cogito_clipboard_set_data(const char *mime_type, const void *data, size_t size);

// Text direction (RTL support)
void cogito_set_direction(int dir); // 0 = LTR, 1 = RTL
int cogito_get_direction(void);
bool cogito_is_rtl(void);

// Input state
bool cogito_is_shift_down(void);
bool cogito_is_mouse_down(void);
int cogito_pointer_x_pos(void);
int cogito_pointer_y_pos(void);

// Printing
bool cogito_print_text(const char *text);
bool cogito_print_image(const unsigned char *rgba_pixels, int width, int height);

// Async task system
typedef void (*cogito_task_fn)(void *user);
typedef void (*cogito_task_done_fn)(void *user);
void cogito_async_run(cogito_task_fn work, cogito_task_done_fn done, void *user);
void cogito_task_run(void *work_fn, void *done_fn);

// i18n
const char *cogito_i18n(const char *key);
void cogito_i18n_load(const char *json_path);
void cogito_set_locale(const char *locale);
const char *cogito_get_locale(void);
void cogito_set_i18n_domain(const char *domain);

// Transitions (value animators)
typedef void (*cogito_transition_update_fn)(double value, void *user);
typedef void (*cogito_transition_done_fn)(void *user);
void cogito_transition(double from, double to, int duration_ms, int ease,
                       cogito_transition_update_fn on_update,
                       cogito_transition_done_fn on_complete, void *user);
void cogito_transition_start(double from, double to, int ms, int ease,
                             void *update_fn, void *complete_fn);

// VirtualList
void *cogito_virtual_list_new(void);
void cogito_virtual_list_set_item_count(void *node, int count);
void cogito_virtual_list_set_item_height(void *node, int height);
void cogito_virtual_list_set_builder(void *node, void *builder_fn);

// GStreamer playback API (single global player instance)
bool cogito_gst_init(void);
void cogito_gst_shutdown(void);
bool cogito_gst_load(const char *path_or_uri);
bool cogito_gst_play(void);
bool cogito_gst_pause(void);
bool cogito_gst_stop(void);
bool cogito_gst_seek_ms(int64_t position_ms);
int64_t cogito_gst_get_position_ms(void);
int64_t cogito_gst_get_duration_ms(void);
bool cogito_gst_set_volume(double value_0_to_1);
double cogito_gst_get_volume(void);
// Returns: 0=none, 1=eos, 2=error, 3=tag (metadata/art update)
int cogito_gst_poll_event(void);
const char *cogito_gst_last_error(void);
// Absolute filesystem path to latest extracted embedded cover art, or "".
const char *cogito_gst_last_cover_path(void);
// Text metadata from the latest GST_MESSAGE_TAG, or "".
const char *cogito_gst_last_tag_title(void);
const char *cogito_gst_last_tag_artist(void);
const char *cogito_gst_last_tag_album(void);
int cogito_gst_last_tag_track_number(void);

// GStreamer video frame API
bool cogito_gst_has_video_frame(void);
int cogito_gst_video_width(void);
int cogito_gst_video_height(void);
CogitoTexture *cogito_gst_update_video_texture(CogitoTexture *existing);

// System notifications
bool cogito_notify(const char *title, const char *body);

// MPRIS / media key integration
bool cogito_mpris_init(const char *app_name);
void cogito_mpris_shutdown(void);
void cogito_mpris_set_metadata(const char *title, const char *artist,
                               const char *album, int64_t length_ms);
void cogito_mpris_set_playback_status(int status); // 0=stopped, 1=playing, 2=paused
typedef void (*cogito_mpris_callback)(int action, void *user);
void cogito_mpris_set_callback(cogito_mpris_callback cb, void *user);
int cogito_mpris_poll(void); // Returns: 0=none, 1=play, 2=pause, 3=next, 4=prev, 5=stop

cogito_timer_id cogito_timer_set_timeout(uint32_t delay_ms,
                                         cogito_timer_fn fn, void *user);
cogito_timer_id cogito_timer_set_interval(uint32_t interval_ms,
                                          cogito_timer_fn fn, void *user);
cogito_timer_id cogito_timer_set_timeout_ex(uint32_t delay_ms,
                                            cogito_timer_fn fn, void *user,
                                            cogito_timer_user_free_fn user_free);
cogito_timer_id cogito_timer_set_interval_ex(uint32_t interval_ms,
                                             cogito_timer_fn fn, void *user,
                                             cogito_timer_user_free_fn user_free);
cogito_timer_id cogito_timer_set_timeout_for(cogito_node *owner,
                                             uint32_t delay_ms,
                                             cogito_timer_fn fn, void *user);
cogito_timer_id cogito_timer_set_interval_for(cogito_node *owner,
                                              uint32_t interval_ms,
                                              cogito_timer_fn fn, void *user);
cogito_timer_id cogito_timer_set_timeout_for_ex(
    cogito_node *owner, uint32_t delay_ms, cogito_timer_fn fn, void *user,
    cogito_timer_user_free_fn user_free);
cogito_timer_id cogito_timer_set_interval_for_ex(
    cogito_node *owner, uint32_t interval_ms, cogito_timer_fn fn, void *user,
    cogito_timer_user_free_fn user_free);
bool cogito_timer_clear(cogito_timer_id timer_id);
void cogito_timer_clear_for(cogito_node *owner);
void cogito_timer_clear_all(void);

cogito_window *cogito_window_new(const char *title, int w, int h);
void cogito_window_free(cogito_window *window);
void cogito_window_set_resizable(cogito_window *window, bool on);
void cogito_window_set_autosize(cogito_window *window, bool on);
void cogito_window_set_size_request(cogito_window *window, int w, int h);
void cogito_window_set_a11y_label(cogito_window *window, const char *label);
void cogito_window_set_builder(cogito_window *window, cogito_node_fn builder,
                               void *user);
void cogito_window_on_resize(cogito_window *window, cogito_node_fn handler,
                             void *user);
typedef void (*cogito_file_drop_fn)(cogito_window *window,
                                     const char **paths, int count,
                                     void *user);
void cogito_window_on_file_drop(cogito_window *window,
                                 cogito_file_drop_fn handler, void *user);
void cogito_window_set_file_drop_fn(cogito_node *node, void *handler_fn);
bool cogito_window_is_compact(cogito_window *window);
void *cogito_window_get_native_handle(cogito_window *window);
bool cogito_window_has_native_handle(cogito_window *window);
void cogito_window_set_hit_test(cogito_window *window,
                                cogito_hit_test_fn callback, void *user);
void cogito_hit_test_cleanup(void);
void cogito_window_set_debug_overlay(cogito_window *window, bool enable);
void cogito_rebuild_active_window(void);
void cogito_window_relayout(cogito_window *window);

// Node creation
cogito_node *cogito_node_new(cogito_node_kind kind);
cogito_node *cogito_grid_new_with_cols(int cols);
cogito_node *cogito_label_new(const char *text);
cogito_node *cogito_button_new(const char *text);
cogito_node *cogito_carousel_new(void);
cogito_node *cogito_carousel_item_new(void);
void cogito_carousel_item_set_text(cogito_node *item, const char *text);
void cogito_carousel_item_set_halign(cogito_node *item, int align);
void cogito_carousel_item_set_valign(cogito_node *item, int align);
int cogito_carousel_get_active_index(cogito_node *node);
void cogito_carousel_set_active_index(cogito_node *node, int index);
cogito_node *cogito_iconbtn_new(const char *text);
void cogito_iconbtn_set_shape(cogito_node *node, int shape);
int cogito_iconbtn_get_shape(cogito_node *node);
void cogito_iconbtn_set_color_style(cogito_node *node, int style);
int cogito_iconbtn_get_color_style(cogito_node *node);
void cogito_iconbtn_set_width(cogito_node *node, int width);
int cogito_iconbtn_get_width(cogito_node *node);
void cogito_iconbtn_set_toggle(cogito_node *node, bool toggle);
bool cogito_iconbtn_get_toggle(cogito_node *node);
void cogito_iconbtn_set_checked(cogito_node *node, bool checked);
bool cogito_iconbtn_get_checked(cogito_node *node);
void cogito_iconbtn_set_size(cogito_node *node, int size);
int cogito_iconbtn_get_size(cogito_node *node);
cogito_node *cogito_checkbox_new(const char *text, const char *group);
cogito_node *cogito_chip_new(const char *text);
cogito_node *cogito_fab_new(const char *icon);
cogito_node *cogito_nav_rail_new(void);
cogito_node *cogito_bottom_nav_new(void);
cogito_node *cogito_switch_new(const char *text);
cogito_node *cogito_textfield_new(const char *text);
cogito_node *cogito_textview_new(const char *text);
cogito_node *cogito_searchfield_new(const char *text);
cogito_node *cogito_dropdown_new(void);
cogito_node *cogito_slider_new(double min, double max, double value);
cogito_node *cogito_slider_range_new(double min, double max, double start,
                                     double end);
cogito_node *cogito_tabs_new(void);
cogito_node *cogito_view_switcher_new(void);
cogito_node *cogito_progress_new(double value);
cogito_node *cogito_divider_new(const char *orientation, bool is_inset);
void cogito_divider_set_wavy(cogito_node *divider, bool on);
bool cogito_divider_get_wavy(cogito_node *divider);
cogito_node *cogito_card_new(const char *title);
void cogito_card_set_variant(cogito_node *card, int variant);
void cogito_card_set_header_image(cogito_node *card, const char *url);
cogito_node *cogito_card_get_overflow_btn(cogito_node *card);
void cogito_card_set_overflow_btn(cogito_node *card, cogito_node *btn);
cogito_node *cogito_avatar_new(const char *text_or_icon);
cogito_node *cogito_badge_new(int count);
cogito_node *cogito_banner_new(const char *text);
void cogito_banner_set_action(cogito_node *banner, const char *text,
                              cogito_node_fn handler, void *user);
void cogito_banner_set_icon(cogito_node *banner, const char *icon);
cogito_node *cogito_bottom_sheet_new(const char *title);
cogito_node *cogito_side_sheet_new(const char *title);
void cogito_side_sheet_set_mode(cogito_node *node, int mode);
cogito_node *cogito_timepicker_new(void);
void cogito_timepicker_on_change(cogito_node *tp, cogito_node_fn handler,
                                 void *user);
int cogito_timepicker_get_hour(cogito_node *tp);
int cogito_timepicker_get_minute(cogito_node *tp);
void cogito_timepicker_set_time(cogito_node *tp, int hour, int minute);
void cogito_avatar_set_image(cogito_node *avatar, const char *path);
void cogito_badge_set_count(cogito_node *badge, int count);
int cogito_badge_get_count(cogito_node *badge);
cogito_node *cogito_datepicker_new(void);
cogito_node *cogito_colorpicker_new(void);
cogito_node *cogito_fontbutton_new(void);
cogito_node *cogito_stepper_new(double min, double max, double value,
                                double step);
cogito_node *cogito_buttongroup_new(void);
cogito_node *cogito_treeview_new(void);
cogito_node *cogito_toasts_new(void);
cogito_node *cogito_toast_new(const char *text);
cogito_node *cogito_toolbar_new(void);
cogito_node *cogito_window_bar_new(void);
void cogito_toolbar_set_vibrant(cogito_node *toolbar, bool vibrant);
bool cogito_toolbar_get_vibrant(cogito_node *toolbar);
void cogito_toolbar_set_vertical(cogito_node *toolbar, bool vertical);
bool cogito_toolbar_get_vertical(cogito_node *toolbar);
cogito_node *cogito_dialog_new(const char *title);
cogito_node *cogito_dialog_slot_new(void);
cogito_node *cogito_popover_new(void);
void cogito_popover_show(cogito_node *anchor, cogito_node *popover);
void cogito_popover_close(cogito_window *window);
cogito_node *cogito_context_menu_new(void);
void cogito_context_menu_add_item(cogito_node *menu, const char *label,
                                  cogito_node_fn fn, void *user);
void cogito_context_menu_add_section(cogito_node *menu, const char *label,
                                     cogito_node_fn fn, void *user);
void cogito_context_menu_set_icon(cogito_node *menu, const char *icon);
void cogito_context_menu_set_shortcut(cogito_node *menu, const char *shortcut);
void cogito_node_set_context_menu(cogito_node *node, cogito_node *menu);
cogito_node *cogito_appbar_new(const char *title, const char *subtitle);
void cogito_appbar_set_title(cogito_node *appbar, const char *title);
void cogito_appbar_set_subtitle(cogito_node *appbar, const char *subtitle);
cogito_node *cogito_image_new(const char *icon);
cogito_node *cogito_webview_new(const char *url);
cogito_node *cogito_drawing_area_new(void);
cogito_node *cogito_shape_new(int preset);

// New widgets
cogito_node *cogito_active_indicator_new(void);
cogito_node *cogito_switchbar_new(const char *text);
bool cogito_switchbar_get_checked(cogito_node *sb);
void cogito_switchbar_set_checked(cogito_node *sb, bool checked);
void cogito_switchbar_on_change(cogito_node *sb, cogito_node_fn fn, void *user);
cogito_node *cogito_content_list_new(void);
cogito_node *cogito_empty_page_new(const char *title);
void cogito_empty_page_set_description(cogito_node *ep, const char *desc);
void cogito_empty_page_set_icon(cogito_node *ep, const char *icon);
void cogito_empty_page_set_action(cogito_node *ep, const char *text,
                                  cogito_node_fn fn, void *user);
cogito_node *cogito_tip_view_new(const char *text);
void cogito_tip_view_set_title(cogito_node *tv, const char *title);
cogito_node *cogito_settings_window_new(const char *title);
cogito_node *cogito_settings_page_new(const char *title);
cogito_node *cogito_settings_list_new(const char *title);
cogito_node *cogito_settings_row_new(const char *label);
cogito_node *cogito_welcome_screen_new(const char *title);
void cogito_welcome_screen_set_description(cogito_node *ws, const char *desc);
void cogito_welcome_screen_set_icon(cogito_node *ws, const char *icon);
void cogito_welcome_screen_set_action(cogito_node *ws, const char *text,
                                      cogito_node_fn fn, void *user);
void cogito_welcome_screen_set_has_seen(cogito_node *ws, const char *has_seen);
cogito_node *cogito_view_dual_new(void);
void cogito_view_dual_set_ratio(cogito_node *vd, double ratio);
void cogito_view_dual_set_show_handle(cogito_node *vd, bool show_handle);
bool cogito_view_dual_get_show_handle(cogito_node *vd);
cogito_node *cogito_view_chooser_new(void);
void cogito_view_chooser_set_items(cogito_node *vc, const char **items,
                                   size_t count);
void cogito_view_chooser_bind(cogito_node *vc, cogito_node *view_switcher);
cogito_node *cogito_about_window_new(const char *app_name, const char *version);
void cogito_about_window_set_icon(cogito_node *aw, const char *icon);
void cogito_about_window_set_description(cogito_node *aw, const char *desc);
void cogito_about_window_set_website(cogito_node *aw, const char *url);
void cogito_about_window_set_issue_url(cogito_node *aw, const char *url);
cogito_node *cogito_menu_button_new(const char *icon);
cogito_node *cogito_split_button_new(const char *text);
void cogito_split_button_add_menu(cogito_node *sb, const char *label,
                                  cogito_node_fn fn, void *user);
void cogito_split_button_add_menu_section(cogito_node *sb, const char *label,
                                          cogito_node_fn fn, void *user);
void cogito_split_button_set_size(cogito_node *sb, int size);
void cogito_split_button_set_variant(cogito_node *sb, int variant);

// Tree / layout
void cogito_node_add(cogito_node *parent, cogito_node *child);
void cogito_node_remove(cogito_node *parent, cogito_node *child);
void cogito_node_reparent(cogito_node *new_parent, cogito_node *child);
void cogito_node_free(cogito_node *node);

void cogito_node_set_margins(cogito_node *node, int top, int right, int bottom,
                             int left);
void cogito_node_set_padding(cogito_node *node, int top, int right, int bottom,
                             int left);
void cogito_node_set_align(cogito_node *node, int align);
void cogito_node_set_halign(cogito_node *node, int align);
void cogito_node_set_valign(cogito_node *node, int align);
void cogito_node_set_hexpand(cogito_node *node, bool expand);
void cogito_node_set_vexpand(cogito_node *node, bool expand);
void cogito_node_set_hexpand_set(cogito_node *node, bool set);
void cogito_node_set_vexpand_set(cogito_node *node, bool set);
void cogito_node_set_min_w(cogito_node *node, int w);
void cogito_node_set_min_h(cogito_node *node, int h);
void cogito_node_set_gap(cogito_node *node, int gap);
void cogito_node_set_homogeneous(cogito_node *node, bool on);
void cogito_node_set_col_homogeneous(cogito_node *node, bool on);
void cogito_node_set_row_homogeneous(cogito_node *node, bool on);
void cogito_node_set_id(cogito_node *node, const char *id);

// Common props
void cogito_node_set_text(cogito_node *node, const char *text);
const char *cogito_node_get_text(cogito_node *node);
void cogito_node_set_subtitle(cogito_node *node, const char *text);
void cogito_node_set_disabled(cogito_node *node, bool on);
void cogito_node_set_opacity(cogito_node *node, float opacity);
void cogito_node_set_visible(cogito_node *node, bool on);
void cogito_node_set_font_size(cogito_node *node, int size);
void cogito_node_set_font_weight(cogito_node *node, int weight);
void cogito_node_set_font_family(cogito_node *node, const char *family);
void cogito_node_set_shadow(cogito_node *node, int level);
void cogito_node_set_border_width(cogito_node *node, int width);
void cogito_node_set_border_radius(cogito_node *node, int radius);
void cogito_node_set_border_color(cogito_node *node, const char *hex);
bool cogito_node_get_visible(cogito_node *node);
int cogito_node_get_x(cogito_node *node);
int cogito_node_get_y(cogito_node *node);
int cogito_node_get_w(cogito_node *node);
int cogito_node_get_h(cogito_node *node);
void cogito_node_set_editable(cogito_node *node, bool on);
bool cogito_node_get_editable(cogito_node *node);
void cogito_node_set_class(cogito_node *node, const char *cls);
void cogito_node_set_a11y_label(cogito_node *node, const char *label);
void cogito_node_set_a11y_role(cogito_node *node, const char *role);
void cogito_node_set_tooltip(cogito_node *node, const char *text);
void cogito_revealer_set_show_children(cogito_node *revealer,
                                       bool show_children);
void cogito_bin_set_accent_color(cogito_node *bin, const char *hex);
void cogito_node_build(cogito_node *node, cogito_node_fn builder, void *user);
void cogito_pointer_capture(cogito_node *node);
void cogito_pointer_release(void);

// Callbacks
void cogito_node_on_click(cogito_node *node, cogito_node_fn fn, void *user);
void cogito_node_on_change(cogito_node *node, cogito_node_fn fn, void *user);
void cogito_node_on_select(cogito_node *node, cogito_index_fn fn, void *user);
void cogito_node_on_activate(cogito_node *node, cogito_index_fn fn, void *user);
void cogito_node_on_check(cogito_node *node, cogito_index_fn fn, void *user);

// Widget-specific helpers
void cogito_dropdown_set_items(cogito_node *dropdown, const char **items,
                               size_t count);
int cogito_dropdown_get_selected(cogito_node *dropdown);
void cogito_dropdown_set_selected(cogito_node *dropdown, int idx);

void cogito_tabs_set_items(cogito_node *tabs, const char **items, size_t count);
void cogito_tabs_set_ids(cogito_node *tabs, const char **ids, size_t count);
int cogito_tabs_get_selected(cogito_node *tabs);
void cogito_tabs_set_selected(cogito_node *tabs, int idx);
void cogito_tabs_bind(cogito_node *tabs, cogito_node *view_switcher);

void cogito_nav_rail_set_items(cogito_node *rail, const char **labels,
                               const char **icons, size_t count);
void cogito_nav_rail_set_badges(cogito_node *rail, const int *badges,
                                size_t count);
void cogito_nav_rail_set_toggle(cogito_node *rail, bool visible);
void cogito_nav_rail_set_divider(cogito_node *rail, bool visible);
void cogito_nav_rail_set_centered(cogito_node *rail, bool centered);
void cogito_nav_rail_set_no_label(cogito_node *rail, bool no_label);
bool cogito_nav_rail_get_no_label(cogito_node *rail);
int cogito_nav_rail_get_selected(cogito_node *rail);
void cogito_nav_rail_set_selected(cogito_node *rail, int idx);
void cogito_nav_rail_on_change(cogito_node *rail, cogito_index_fn fn,
                               void *user);

void cogito_bottom_nav_set_items(cogito_node *nav, const char **labels,
                                 const char **icons, size_t count);
int cogito_bottom_nav_get_selected(cogito_node *nav);
void cogito_bottom_nav_set_selected(cogito_node *nav, int idx);
void cogito_bottom_nav_on_change(cogito_node *nav, cogito_index_fn fn,
                                 void *user);

double cogito_slider_get_value(cogito_node *slider);
void cogito_slider_set_value(cogito_node *slider, double value);
void cogito_slider_set_bounds(cogito_node *slider, double min, double max);
void cogito_slider_set_size(cogito_node *slider, int size);
int cogito_slider_get_size(cogito_node *slider);
void cogito_slider_set_icon(cogito_node *slider, const char *icon);
void cogito_slider_set_centered(cogito_node *slider, bool on);
bool cogito_slider_get_centered(cogito_node *slider);
void cogito_slider_set_end_dots(cogito_node *slider, bool on);
bool cogito_slider_get_end_dots(cogito_node *slider);
void cogito_slider_set_value_bubble(cogito_node *slider, bool on);
bool cogito_slider_get_value_bubble(cogito_node *slider);
void cogito_slider_set_bubble_format(cogito_node *slider, const char *format);
const char *cogito_slider_get_bubble_format(cogito_node *slider);
void cogito_slider_set_bubble_scale(cogito_node *slider, double scale);
double cogito_slider_get_bubble_scale(cogito_node *slider);
void cogito_slider_set_show_bubble(cogito_node *slider, bool on);
bool cogito_slider_get_show_bubble(cogito_node *slider);
void cogito_slider_set_wavy(cogito_node *slider, bool on);
bool cogito_slider_get_wavy(cogito_node *slider);
void cogito_slider_set_step(cogito_node *slider, double step);
double cogito_slider_get_step(cogito_node *slider);
void cogito_slider_set_step_dots(cogito_node *slider, bool on);
bool cogito_slider_get_step_dots(cogito_node *slider);
void cogito_slider_set_range(cogito_node *slider, double start, double end);
void cogito_slider_set_range_start(cogito_node *slider, double start);
void cogito_slider_set_range_end(cogito_node *slider, double end);
double cogito_slider_get_range_start(cogito_node *slider);
double cogito_slider_get_range_end(cogito_node *slider);

bool cogito_checkbox_get_checked(cogito_node *cb);
void cogito_checkbox_set_checked(cogito_node *cb, bool checked);

bool cogito_chip_get_selected(cogito_node *chip);
void cogito_chip_set_selected(cogito_node *chip, bool selected);
void cogito_chip_set_closable(cogito_node *chip, bool closable);

bool cogito_switch_get_checked(cogito_node *sw);
void cogito_switch_set_checked(cogito_node *sw, bool checked);

void cogito_textfield_set_text(cogito_node *tf, const char *text);
const char *cogito_textfield_get_text(cogito_node *tf);
void cogito_textfield_set_hint(cogito_node *tf, const char *hint);
const char *cogito_textfield_get_hint(cogito_node *tf);
void cogito_textfield_set_prefix(cogito_node *tf, const char *prefix);
const char *cogito_textfield_get_prefix(cogito_node *tf);
void cogito_textfield_set_suffix(cogito_node *tf, const char *suffix);
const char *cogito_textfield_get_suffix(cogito_node *tf);
void cogito_textfield_set_regex(cogito_node *tf, const char *pattern);
const char *cogito_textfield_get_regex(cogito_node *tf);
bool cogito_textfield_is_valid(cogito_node *tf);
void cogito_textview_set_text(cogito_node *tv, const char *text);
const char *cogito_textview_get_text(cogito_node *tv);
void cogito_searchfield_set_text(cogito_node *sf, const char *text);
const char *cogito_searchfield_get_text(cogito_node *sf);

void cogito_progress_set_value(cogito_node *prog, double value);
double cogito_progress_get_value(cogito_node *prog);
void cogito_progress_set_indeterminate(cogito_node *prog, bool on);
bool cogito_progress_get_indeterminate(cogito_node *prog);
void cogito_progress_set_thickness(cogito_node *prog, int px);
int cogito_progress_get_thickness(cogito_node *prog);
void cogito_progress_set_wavy(cogito_node *prog, bool on);
bool cogito_progress_get_wavy(cogito_node *prog);
void cogito_progress_set_circular(cogito_node *prog, bool on);
bool cogito_progress_get_circular(cogito_node *prog);

void cogito_stepper_set_value(cogito_node *stepper, double value);
double cogito_stepper_get_value(cogito_node *stepper);
void cogito_stepper_on_change(cogito_node *stepper, cogito_node_fn fn,
                              void *user);
void cogito_buttongroup_on_select(cogito_node *seg, cogito_index_fn fn,
                                void *user);
void cogito_buttongroup_set_size(cogito_node *bg, int size);
int cogito_buttongroup_get_size(cogito_node *bg);
void cogito_buttongroup_set_shape(cogito_node *bg, int shape);
int cogito_buttongroup_get_shape(cogito_node *bg);
void cogito_buttongroup_set_connected(cogito_node *bg, bool connected);
bool cogito_buttongroup_get_connected(cogito_node *bg);
void cogito_buttongroup_set_selected(cogito_node *bg, int idx);
int cogito_buttongroup_get_selected(cogito_node *bg);

// Theming
void cogito_load_sum_file(const char *path);
void cogito_load_sum_inline(const char *src);
void cogito_set_script_dir(const char *dir);
const char *cogito_get_script_dir(void);

// Embedded resources (SVGs baked into binaries as base64)
void cogito_register_embedded_resource(const char *name, const unsigned char *data, size_t len);
bool cogito_debug_style(void);
void cogito_style_dump(cogito_node *node);
void cogito_style_dump_tree(cogito_node *root, int depth);
void cogito_style_dump_button_demo(void);

#ifdef __cplusplus
}
#endif
// Label helpers
void cogito_label_set_text(cogito_node *label, const char *text);
void cogito_label_set_wrap(cogito_node *label, bool on);
void cogito_label_set_max_width_chars(cogito_node *label, int max_chars);
void cogito_label_set_line_chars(cogito_node *label, int line_chars);
void cogito_label_set_ellipsis(cogito_node *label, bool on);
void cogito_label_set_fade(cogito_node *label, bool on);
void cogito_label_set_marquee(cogito_node *label, bool on);
void cogito_label_set_align(cogito_node *label, int align);

// Image helpers
void cogito_image_set_icon(cogito_node *image, const char *icon);
void cogito_image_set_source(cogito_node *image, const char *source);
void cogito_image_set_blur(cogito_node *image, float sigma);
void cogito_image_set_size(cogito_node *image, int w, int h);
void cogito_image_set_radius(cogito_node *image, int radius);
void cogito_image_set_alt_text(cogito_node *image, const char *alt_text);
void cogito_image_set_fit(cogito_node *image, int fit); // 0=cover, 1=contain
void cogito_webview_set_url(cogito_node *webview, const char *url);
const char *cogito_webview_get_url(cogito_node *webview);
void cogito_webview_set_open_external_on_click(cogito_node *webview, bool on);
bool cogito_webview_get_open_external_on_click(cogito_node *webview);
bool cogito_webview_open(cogito_node *webview);
void cogito_drawing_area_on_press(cogito_node *area, cogito_node_fn fn,
                                  void *user);
void cogito_drawing_area_on_drag(cogito_node *area, cogito_node_fn fn,
                                 void *user);
void cogito_drawing_area_on_release(cogito_node *area, cogito_node_fn fn,
                                    void *user);
void cogito_drawing_area_on_draw(cogito_node *area, cogito_draw_fn fn,
                                 void *user);
int cogito_drawing_area_get_x(cogito_node *area);
int cogito_drawing_area_get_y(cogito_node *area);
bool cogito_drawing_area_get_pressed(cogito_node *area);
void cogito_drawing_area_clear(cogito_node *area);
cogito_node *cogito_video_view_new(void);
void cogito_video_view_set_fit(cogito_node *view, int fit);
void cogito_canvas_set_color(cogito_node *area, const char *color);
void cogito_canvas_set_line_width(cogito_node *area, int width);
void cogito_canvas_line(cogito_node *area, int x1, int y1, int x2, int y2);
void cogito_canvas_rect(cogito_node *area, int x, int y, int w, int h);
void cogito_canvas_fill_rect(cogito_node *area, int x, int y, int w, int h);
void cogito_canvas_circle(cogito_node *area, int cx, int cy, int r);
void cogito_canvas_fill_circle(cogito_node *area, int cx, int cy, int r);
void cogito_shape_set_preset(cogito_node *shape, int preset);
int cogito_shape_get_preset(cogito_node *shape);
void cogito_shape_set_size(cogito_node *shape, int size_dp);
void cogito_shape_set_size_h(cogito_node *shape, int size_h_dp);
int cogito_shape_get_size(cogito_node *shape);
void cogito_shape_set_color(cogito_node *shape, const char *color);
void cogito_shape_set_color_style(cogito_node *shape, int style);
int cogito_shape_get_color_style(cogito_node *shape);
void cogito_shape_set_vertex(cogito_node *shape, int index, float x, float y);
float cogito_shape_get_vertex_x(cogito_node *shape, int index);
float cogito_shape_get_vertex_y(cogito_node *shape, int index);
void cogito_shape_set_clip(cogito_node *shape, bool clip);

// Appbar helpers
cogito_node *cogito_appbar_add_button(cogito_node *appbar, const char *icon,
                                      cogito_node_fn fn, void *user);
void cogito_appbar_set_controls(cogito_node *appbar, const char *layout);
void cogito_appbar_pair_scroller(cogito_node *appbar, cogito_node *scroller);

// Dialog helpers
void cogito_dialog_slot_show(cogito_node *slot, cogito_node *dialog);
void cogito_dialog_slot_clear(cogito_node *slot);
void cogito_dialog_close(cogito_node *dialog);
void cogito_dialog_remove(cogito_node *dialog);
void cogito_window_set_dialog(cogito_window *window, cogito_node *dialog);
void cogito_window_clear_dialog(cogito_window *window);
void cogito_window_set_side_sheet(cogito_window *window,
                                  cogito_node *side_sheet);
void cogito_window_clear_side_sheet(cogito_window *window);

// Containers and layout helpers
void cogito_fixed_set_pos(cogito_node *fixed, cogito_node *child, int x, int y);
void cogito_scroller_set_axes(cogito_node *scroller, bool h, bool v);
void cogito_scroller_set_sync(cogito_node *scroller, cogito_node *other);
void cogito_scroller_set_min_content_width(cogito_node *scroller, int w);
void cogito_scroller_set_min_content_height(cogito_node *scroller, int h);
void cogito_scroller_set_scroll_y(cogito_node *scroller, int y);
void cogito_carousel_set_active_index(cogito_node *carousel, int idx);
int cogito_carousel_get_active_index(cogito_node *carousel);
void cogito_grid_set_cols(cogito_node *grid, int cols);
void cogito_grid_add(cogito_node *grid, cogito_node *child, int col, int row,
                     int col_span, int row_span);
void cogito_grid_set_gap(cogito_node *grid, int x, int y);
void cogito_grid_set_span(cogito_node *child, int col_span, int row_span);
void cogito_grid_set_align(cogito_node *child, int halign, int valign);

// Widget event helpers
void cogito_button_set_text(cogito_node *button, const char *text);
void cogito_button_set_size(cogito_node *button, int size);
int cogito_button_get_size(cogito_node *button);
void cogito_button_add_menu(cogito_node *button, const char *label,
                            cogito_node_fn fn, void *user);
void cogito_button_add_menu_section(cogito_node *button, const char *label,
                                    cogito_node_fn fn, void *user);
void cogito_iconbtn_add_menu(cogito_node *button, const char *label,
                             cogito_node_fn fn, void *user);
void cogito_iconbtn_add_menu_section(cogito_node *button, const char *label,
                                     cogito_node_fn fn, void *user);
void cogito_button_set_menu_divider(cogito_node *button, bool divider);
bool cogito_button_get_menu_divider(cogito_node *button);
void cogito_button_set_menu_item_gap(cogito_node *button, int gap);
int cogito_button_get_menu_item_gap(cogito_node *button);
void cogito_button_set_menu_vibrant(cogito_node *button, bool vibrant);
bool cogito_button_get_menu_vibrant(cogito_node *button);
// Menu item property setters (act on the last-added menu item)
void cogito_menu_set_icon(cogito_node *node, const char *icon);
void cogito_menu_set_shortcut(cogito_node *node, const char *shortcut);
void cogito_menu_set_submenu(cogito_node *node, bool submenu);
void cogito_menu_set_toggled(cogito_node *node, bool toggled);
void cogito_iconbtn_set_menu_divider(cogito_node *iconbtn, bool divider);
bool cogito_iconbtn_get_menu_divider(cogito_node *iconbtn);
void cogito_iconbtn_set_menu_item_gap(cogito_node *iconbtn, int gap);
int cogito_iconbtn_get_menu_item_gap(cogito_node *iconbtn);
void cogito_iconbtn_set_menu_vibrant(cogito_node *iconbtn, bool vibrant);
bool cogito_iconbtn_get_menu_vibrant(cogito_node *iconbtn);

void cogito_checkbox_on_change(cogito_node *cb, cogito_node_fn fn, void *user);
void cogito_chip_on_click(cogito_node *chip, cogito_node_fn fn, void *user);
void cogito_chip_on_close(cogito_node *chip, cogito_node_fn fn, void *user);
void cogito_fab_set_extended(cogito_node *fab, bool extended,
                             const char *label);
void cogito_fab_set_size(cogito_node *fab, int size); // 0=S(42), 1=M(56), 2=L(96)
void cogito_fab_set_icon(cogito_node *fab, const char *icon);
void cogito_fab_set_color(cogito_node *fab, int color); // 0=Primary, 1=Secondary, 2=Tertiary, 3=PrimaryContainer, 4=SecondaryContainer, 5=TertiaryContainer
void cogito_fab_menu_set_color(cogito_node *fab, int color); // 0=Primary, 1=Secondary, 2=Tertiary
void cogito_fab_menu_set_size(cogito_node *fab, int size); // 0=S(56), 1=M(80), 2=L(96)
void cogito_fab_on_click(cogito_node *fab, cogito_node_fn fn, void *user);
void cogito_switch_on_change(cogito_node *sw, cogito_node_fn fn, void *user);

void cogito_textfield_on_change(cogito_node *tf, cogito_node_fn fn, void *user);
void cogito_textview_on_change(cogito_node *tv, cogito_node_fn fn, void *user);
void cogito_searchfield_on_change(cogito_node *sf, cogito_node_fn fn,
                                  void *user);

void cogito_dropdown_on_change(cogito_node *dropdown, cogito_node_fn fn,
                               void *user);
void cogito_slider_on_change(cogito_node *slider, cogito_node_fn fn,
                             void *user);
void cogito_tabs_on_change(cogito_node *tabs, cogito_node_fn fn, void *user);
void cogito_datepicker_on_change(cogito_node *datepicker, cogito_node_fn fn,
                                 void *user);
void cogito_colorpicker_on_change(cogito_node *colorpicker, cogito_node_fn fn,
                                  void *user);
void cogito_colorpicker_set_hex(cogito_node *colorpicker, const char *hex);
const char *cogito_colorpicker_get_hex(cogito_node *colorpicker);
void cogito_fontbutton_on_change(cogito_node *fontbutton, cogito_node_fn fn,
                                 void *user);
void cogito_fontbutton_set_font(cogito_node *fontbutton, const char *font_name);
const char *cogito_fontbutton_get_font(cogito_node *fontbutton);

void cogito_content_list_set_selected(cogito_node *list, int idx);
void cogito_content_list_remove_at(cogito_node *list, int idx);
void cogito_content_list_clear(cogito_node *list);
void cogito_content_list_set_visible_dividers(cogito_node *list, bool on);
void cogito_content_list_on_select(cogito_node *list, cogito_index_fn fn, void *user);
void cogito_content_list_on_activate(cogito_node *list, cogito_index_fn fn, void *user);
void cogito_grid_clear(cogito_node *grid);
void cogito_grid_on_select(cogito_node *grid, cogito_index_fn fn, void *user);
void cogito_grid_on_activate(cogito_node *grid, cogito_index_fn fn, void *user);

void cogito_view_switcher_set_active(cogito_node *view_switcher,
                                     const char *id);
const char *cogito_view_switcher_get_active(cogito_node *view_switcher);
void cogito_view_switcher_add_lazy(cogito_node *view_switcher, const char *id,
                                   cogito_node_fn builder);

void cogito_toast_set_text(cogito_node *toast, const char *text);
void cogito_toast_on_click(cogito_node *toast, cogito_node_fn fn, void *user);
void cogito_toast_set_action(cogito_node *toast, const char *action_text,
                             cogito_node_fn fn, void *user);

// Node/window helpers
cogito_window *cogito_node_window(cogito_node *node);
cogito_node *cogito_node_get_parent(cogito_node *node);
cogito_node *cogito_node_parent(cogito_node *node);
size_t cogito_node_get_child_count(cogito_node *node);
cogito_node *cogito_node_get_child(cogito_node *node, size_t index);
