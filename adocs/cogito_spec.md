# Cogito GUI Framework Specification

This document defines the framework-level behavior of Cogito.
It focuses on the public programming model and runtime behavior, not build system or backend implementation details.

## 1. Scope

In scope:

- Application, window, and node model.
- Widget set and composition semantics.
- Event and callback behavior.
- Styling via SUM.
- Window integration features exposed by the public API.
- Debug and inspection features relevant to app developers.

Out of scope:

- Meson/dependency setup instructions.
- Renderer/backend internals.
- Platform-specific private implementation hooks.

## 2. Core Abstractions

Cogito exposes three primary opaque handles:

- `cogito_app`: application lifetime and global app metadata.
- `cogito_window`: top-level window/root node.
- `cogito_node`: widget or container in the UI tree.

The framework is declarative by tree construction: app code builds node trees and registers callbacks.

## 3. Lifecycle

### 3.1 App Lifecycle

Public entry points:

- `cogito_app_new()`
- `cogito_app_free()`
- `cogito_app_run(cogito_app* app, cogito_window* window)`

App metadata:

- `cogito_app_set_appid(...)`
- `cogito_app_set_app_name(...)`
- `cogito_app_set_accent_color(...)`
- `cogito_app_set_icon(...)`
- `cogito_app_get_icon(...)`
- `cogito_open_url(...)`
- Timer scheduling APIs:
  `cogito_timer_set_timeout(...)`,
  `cogito_timer_set_interval(...)`,
  `cogito_timer_clear(...)`,
  `cogito_timer_clear_all(...)`

### 3.2 Window Lifecycle

Public entry points:

- `cogito_window_new(title, w, h)`
- `cogito_window_free(...)`
- `cogito_window_set_resizable(...)`
- `cogito_window_set_autosize(...)`
- `cogito_window_set_builder(...)`
- `cogito_window_set_a11y_label(...)`
- `cogito_rebuild_active_window()`

Current behavior: application runtime exits when the last open window closes.

## 4. Node Tree Model

### 4.1 Ownership and Structure

- Nodes form a rooted tree under a window.
- `cogito_node_add(parent, child)` appends child nodes.
- `cogito_node_remove(parent, child)` detaches child nodes.
- `cogito_node_free(node)` releases detached nodes.
- `cogito_node_new(kind)` creates a node of the specified kind.

Tree/query helpers:

- `cogito_node_get_parent(...)`
- `cogito_node_parent(...)` (alias)
- `cogito_node_get_child_count(...)`
- `cogito_node_get_child(...)`
- `cogito_node_window(...)`

### 4.2 Node Creation

Widget factory functions:

- `cogito_node_new(kind)` - generic node creation.
- `cogito_grid_new_with_cols(cols)` - grid with column count.
- `cogito_label_new(text)` - label with text.
- `cogito_button_new(text)` - button with text.
- `cogito_iconbtn_new(text)` - icon button.
- `cogito_checkbox_new(text, group)` - checkbox with optional group.
- `cogito_chip_new(text)` - chip/filter chip.
- `cogito_fab_new(icon)` - floating action button.
- `cogito_switch_new(text)` - toggle switch.
- `cogito_textfield_new(text)` - single-line text input.
- `cogito_textview_new(text)` - multi-line text input.
- `cogito_searchfield_new(text)` - search input field.
- `cogito_dropdown_new()` - dropdown selector.
- `cogito_slider_new(min, max, value)` - slider control.
- `cogito_slider_range_new(min, max, start, end)` - range slider.
- `cogito_tabs_new()` - tab bar.
- `cogito_view_switcher_new()` - view container for tabs.
- `cogito_progress_new(value)` - progress indicator.
- `cogito_divider_new(orientation, is_inset)` - divider line.
- `cogito_card_new(title)` - card container.
- `cogito_avatar_new(text_or_icon)` - avatar display.
- `cogito_badge_new(count)` - notification badge.
- `cogito_banner_new(text)` - banner message.
- `cogito_bottom_sheet_new(title)` - bottom sheet.
- `cogito_side_sheet_new(title)` - side sheet.
- `cogito_timepicker_new()` - time picker.
- `cogito_datepicker_new()` - date picker.
- `cogito_colorpicker_new()` - color picker.
- `cogito_stepper_new(min, max, value, step)` - stepper control.
- `cogito_buttongroup_new()` - segmented button group.
- `cogito_treeview_new()` - tree view.
- `cogito_toasts_new()` - toast container.
- `cogito_toast_new(text)` - toast notification.
- `cogito_toolbar_new()` - toolbar.
- `cogito_dialog_new(title)` - dialog.
- `cogito_dialog_slot_new()` - dialog slot.
- `cogito_appbar_new(title, subtitle)` - app bar.
- `cogito_image_new(icon)` - image/icon display.
- `cogito_nav_rail_new()` - navigation rail.
- `cogito_bottom_nav_new()` - bottom navigation.
- `cogito_carousel_new()` - carousel container.
- `cogito_carousel_item_new()` - carousel item.
- `cogito_active_indicator_new()` - loading indicator.
- `cogito_switchbar_new(text)` - switch bar.
- `cogito_content_list_new()` - content list.
- `cogito_empty_page_new(title)` - empty state page.
- `cogito_tip_view_new(text)` - tip view.
- `cogito_settings_window_new(title)` - settings window.
- `cogito_settings_page_new(title)` - settings page.
- `cogito_settings_list_new(title)` - settings list.
- `cogito_settings_row_new(label)` - settings row.
- `cogito_welcome_screen_new(title)` - welcome screen.
- `cogito_view_dual_new()` - dual view container.
- `cogito_view_chooser_new()` - view chooser.
- `cogito_about_window_new(app_name, version)` - about window.
- `cogito_split_button_new(text)` - split button.
- `cogito_menu_button_new(icon)` - menu button.

### 4.3 Widget Kinds

Current public kinds include:

- Window/container: `window`, `vstack`, `hstack`, `zstack`, `fixed`, `scroller`, `list`, `grid`.
- Text/content: `label`, `image`, `tooltip`.
- Button/container: `buttongroup`
- Actions/inputs: `button`, `iconbtn`, `fab`, `checkbox`, `switch`, `chip`, `textfield`, `textview`, `searchfield`, `dropdown`, `slider`, `stepper`.
- Navigation/surfaces: `tabs`, `view_switcher`, `appbar`, `toolbar`, `nav_rail`, `bottom_nav`, `dialog`, `dialog_slot`, `toast`, `toasts`, `treeview`, `progress`, `datepicker`, `colorpicker`.
- Display/surfaces: `divider`, `card`, `avatar`, `badge`, `banner`, `bottom_sheet`, `side_sheet`, `carousel`, `carousel_item`.
- Time/pickers: `timepicker`.
- Indicators: `active_indicator`, `switchbar`.
- Content containers: `content_list`, `empty_page`, `tip_view`.
- Settings: `settings_window`, `settings_page`, `settings_list`, `settings_row`.
- Welcome/view: `welcome_screen`, `view_dual`, `view_chooser`, `about_window`.
- Composite: `split_button`, `fab_menu`.

Exact C enum values are defined in `cogito/src/cogito.h`.

## 5. Layout and Common Properties

Common node properties:

- Spacing: margins and padding.
- Alignment: horizontal/vertical and combined alignment.
- Expansion: horizontal/vertical expand flags.
- Gap for container spacing.
- Identity: node ID and class.
- Content/state: text, editable, disabled, tooltip, accessibility label/role.

Key APIs:

- `cogito_node_set_margins(...)`
- `cogito_node_set_padding(...)`
- `cogito_node_set_align(...)`
- `cogito_node_set_halign(...)`
- `cogito_node_set_valign(...)`
- `cogito_node_set_hexpand(...)`
- `cogito_node_set_vexpand(...)`
- `cogito_node_set_gap(...)`
- `cogito_node_set_id(...)`
- `cogito_node_set_class(...)`
- `cogito_node_set_text(...)`
- `cogito_node_get_text(...)`
- `cogito_node_set_disabled(...)`
- `cogito_node_set_editable(...)`
- `cogito_node_get_editable(...)`
- `cogito_node_set_a11y_label(...)`
- `cogito_node_set_a11y_role(...)`
- `cogito_node_set_tooltip(...)`
- `cogito_node_build(...)`

## 6. Interaction Model

### 6.1 Callback Types

- Node callbacks: `cogito_node_fn(node, user)`.
- Indexed callbacks: `cogito_index_fn(node, index, user)`.

Common registration points:

- `cogito_node_on_click(...)`
- `cogito_node_on_change(...)`
- `cogito_node_on_select(...)`
- `cogito_node_on_activate(...)`

Widget-specific callback registration:

- `cogito_checkbox_on_change(...)`
- `cogito_switch_on_change(...)`
- `cogito_textfield_on_change(...)`
- `cogito_textview_on_change(...)`
- `cogito_searchfield_on_change(...)`
- `cogito_dropdown_on_change(...)`
- `cogito_slider_on_change(...)`
- `cogito_tabs_on_change(...)`
- `cogito_datepicker_on_change(...)`
- `cogito_colorpicker_on_change(...)`
- `cogito_timepicker_on_change(...)`
- `cogito_stepper_on_change(...)`
- `cogito_buttongroup_on_select(...)`
- `cogito_nav_rail_on_change(...)`
- `cogito_bottom_nav_on_change(...)`
- `cogito_fab_on_click(...)`
- `cogito_chip_on_click(...)`
- `cogito_chip_on_close(...)`
- `cogito_toast_on_click(...)`
- `cogito_list_on_select(...)`
- `cogito_list_on_activate(...)`
- `cogito_grid_on_select(...)`
- `cogito_grid_on_activate(...)`
- `cogito_switchbar_on_change(...)`

### 6.2 Pointer Capture

- `cogito_pointer_capture(node)` captures pointer interaction to a node.
- `cogito_pointer_release()` releases capture.

## 7. Widget-Specific APIs

Cogito provides helper APIs for richer widgets, for example:

- Item models and selection: dropdown/tabs/nav rail/bottom nav.
- Value getters/setters: slider/progress/stepper.
- State getters/setters: checkbox/switch/chip.
- Text setters/getters: textfield/textview/searchfield.
- Dialog management: dialog slot and window dialog helpers.
- Appbar controls: add buttons and control layout.
- Banner actions: `cogito_banner_set_action()`, `cogito_banner_set_icon()`.
- Avatar: `cogito_avatar_set_image()`.
- Badge: `cogito_badge_set_count()`, `cogito_badge_get_count()`.
- Timepicker: `cogito_timepicker_get_hour()`, `cogito_timepicker_get_minute()`, `cogito_timepicker_set_time()`.
- Switchbar: `cogito_switchbar_get_checked()`, `cogito_switchbar_set_checked()`.
- Empty page: `cogito_empty_page_set_description()`, `cogito_empty_page_set_icon()`, `cogito_empty_page_set_action()`.
- Welcome screen: `cogito_welcome_screen_set_description()`, `cogito_welcome_screen_set_icon()`, `cogito_welcome_screen_set_action()`.
- About window: `cogito_about_window_set_icon()`, `cogito_about_window_set_description()`, `cogito_about_window_set_website()`, `cogito_about_window_set_issue_url()`.
- Split button: `cogito_split_button_add_menu()`, `cogito_split_button_add_menu_section()`.
- View dual: `cogito_view_dual_set_ratio()`.
- View chooser: `cogito_view_chooser_set_items()`, `cogito_view_chooser_bind()`.
- View switcher: `cogito_view_switcher_set_active()`, `cogito_view_switcher_add_lazy()`.
- Side sheet: `cogito_side_sheet_set_mode()`.
- Toast: `cogito_toast_set_text()`, `cogito_toast_on_click()`, `cogito_toast_set_action()`.
- Toolbar: `cogito_toolbar_set_vibrant()`, `cogito_toolbar_get_vibrant()`.
- Button menu: `cogito_button_add_menu()`, `cogito_button_add_menu_section()`, `cogito_button_set_menu_divider()`, `cogito_button_set_menu_vibrant()`.
- Icon button menu: `cogito_iconbtn_add_menu()`, `cogito_iconbtn_add_menu_section()`.
- Carousel: `cogito_carousel_get_active_index()`, `cogito_carousel_set_active_index()`, `cogito_carousel_item_set_text()`.
- Icon button: `cogito_iconbtn_set_shape()`, `cogito_iconbtn_set_color_style()`, `cogito_iconbtn_set_toggle()`, `cogito_iconbtn_set_checked()`.
- FAB: `cogito_fab_set_extended()`, `cogito_fab_set_size()`, `cogito_fab_set_icon()`, `cogito_fab_set_color()`.
- Progress: `cogito_progress_set_indeterminate()`, `cogito_progress_set_thickness()`, `cogito_progress_set_wavy()`, `cogito_progress_set_circular()`.
- Slider: `cogito_slider_set_size()`, `cogito_slider_set_icon()`, `cogito_slider_set_centered()`, `cogito_slider_set_range()`.
- Button group: `cogito_buttongroup_set_size()`, `cogito_buttongroup_set_shape()`, `cogito_buttongroup_set_connected()`.
- Label: `cogito_label_set_wrap()`, `cogito_label_set_ellipsis()`, `cogito_label_set_align()`.
- Image: `cogito_image_set_icon()`, `cogito_image_set_source()`, `cogito_image_set_size()`, `cogito_image_set_radius()`.
- Grid: `cogito_grid_set_gap()`, `cogito_grid_set_span()`, `cogito_grid_set_align()`.
- Fixed: `cogito_fixed_set_pos()`.
- Scroller: `cogito_scroller_set_axes()`.

The authoritative API surface is `cogito/src/cogito.h`.

## 8. Styling and Theming (SUM)

### 8.1 Theme Source

SUM is the styling language used by Cogito.
Themes can be loaded from file or source text:

- `cogito_load_sum_file(path)`
- `cogito_load_sum_inline(src)`
- `cogito_set_script_dir(dir)`
- `cogito_get_script_dir()`

### 8.2 Cascade Model (Framework-Level)

Computed style resolution combines:

- Framework defaults.
- Per-kind styles.
- Class/custom-class styles.
- State styles (`hover`, `active`, `checked`, `disabled`, `selection`).
- Direct inline values.

### 8.3 Style Debugging

Debug APIs:

- `cogito_debug_style()`
- `cogito_style_dump(node)`
- `cogito_style_dump_tree(root, depth)`
- `cogito_style_dump_button_demo()`

Environment:

- `COGITO_DEBUG_STYLE=1` enables style debug output.

## 9. Window Integration

### 9.1 Native Handle Access

Public API:

- `cogito_window_get_native_handle(window)`
- `cogito_window_has_native_handle(window)`

This is for integrations that require platform-native window access.

### 9.2 Hit Testing and Client-Side Decorations

Cogito exposes custom hit-test integration for borderless/custom-decorated windows:

- `cogito_window_set_hit_test(window, callback, user)`
- `cogito_window_set_debug_overlay(window, enable)`
- `cogito_hit_test_cleanup()`

Hit-test callback returns `cogito_hit_test_result` values including:

- normal content.
- draggable region.
- directional resize regions.
- close/min/max button regions.

Debug environment:

- `COGITO_DEBUG_CSD=1` enables CSD overlay diagnostics.

### 9.3 Dialog and Sheet Management

Window-level dialog and sheet management:

- `cogito_window_set_dialog(window, dialog)`
- `cogito_window_clear_dialog(window)`
- `cogito_window_set_side_sheet(window, side_sheet)`
- `cogito_window_clear_side_sheet(window)`

Dialog operations:

- `cogito_dialog_slot_show(slot, dialog)`
- `cogito_dialog_slot_clear(slot)`
- `cogito_dialog_close(dialog)`
- `cogito_dialog_remove(dialog)`

## 10. Inspector and Debug Flags

Cogito includes a runtime inspector for widget hierarchy/style debugging.
Current runtime controls:

- `COGITO_INSPECTOR=1` to start with inspector enabled.
- `Ctrl+Shift+I` to toggle inspector during runtime.

Native handle debug logging:

- `COGITO_DEBUG_NATIVE=1`

## 11. Multi-Window Behavior

Cogito runtime supports multiple windows with window-ID-based event routing.

Current behavior:

- Events are routed to the originating window.
- Keyboard focus is tracked per focused window.
- Window close is handled per window; application exits when all windows are closed.

Implementation note:

- Current backend default window registry limit is `COGITO_MAX_WINDOWS` (currently 8).

## 12. Compatibility Notes

- Public C API is declared in `cogito/src/cogito.h`.
- Ergo bindings are expected to preserve this model at the language level.
- Backend and renderer details are intentionally non-normative in this spec.
