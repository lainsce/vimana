# Cogito Design System Specification

This document defines the design tokens, color system, spacing, typography, and animation constants used throughout the Cogito GUI framework.

## 1. Color System

Cogito uses a color system with dynamic color generation from a primary accent color. The system supports both light and dark themes.

### 1.1 Primary Palette

| Token | Light Theme | Dark Theme | Description |
|-------|-------------|------------|-------------|
| `@primary` | `#72dec2` | `#72dec2` | Primary brand color |
| `@on-primary` | `#00382b` | `#00382b` | Text/icons on primary |
| `@primary-container` | `#c8f7ef` | `#005142` | Container with primary tint |
| `@on-primary-container` | `#00201a` | `#c8f7ef` | Text/icons on primary container |

### 1.2 Secondary Palette

| Token | Light Theme | Dark Theme | Description |
|-------|-------------|------------|-------------|
| `@secondary` | `#4a9e8f` | `#84e8d0` | Secondary brand color |
| `@on-secondary` | `#ffffff` | `#00382b` | Text/icons on secondary |
| `@secondary-container` | `#c8f7ef` | `#005142` | Container with secondary tint |
| `@on-secondary-container` | `#00201a` | `#c8f7ef` | Text/icons on secondary container |

### 1.3 Tertiary Palette

| Token | Light Theme | Dark Theme | Description |
|-------|-------------|------------|-------------|
| `@tertiary` | `#3d6463` | `#a5ceca` | Tertiary/accent color |
| `@on-tertiary` | `#ffffff` | `#00382b` | Text/icons on tertiary |
| `@tertiary-container` | `#c8f7ef` | `#1d4d49` | Container with tertiary tint |
| `@on-tertiary-container` | `#00201a` | `#c8f7ef` | Text/icons on tertiary container |

### 1.4 Error Palette

| Token | Light Theme | Dark Theme | Description |
|-------|-------------|------------|-------------|
| `@error` | `#ba1a1a` | `#ffb4ab` | Error/destructive color |
| `@on-error` | `#ffffff` | `#690005` | Text/icons on error |
| `@error-container` | `#ffdad6` | `#93000a` | Error container background |
| `@on-error-container` | `#410002` | `#ffdad6` | Text/icons on error container |

### 1.5 Neutral Palette

| Token | Light Theme | Dark Theme | Description |
|-------|-------------|------------|-------------|
| `@background` | `#fafdfb` | `#191c1a` | App background |
| `@on-background` | `#191c1a` | `#e1e3e0` | Text/icons on background |
| `@surface` | `#fafdfb` | `#191c1a` | Surface background |
| `@surface-tint` | `#72dec229` | `#72dec2` | Surface tint overlay |
| `@on-surface` | `#191c1a` | `#e1e3e0` | Text/icons on surface |

### 1.6 Surface Container Hierarchy

Surface containers provide elevation-like hierarchy through progressively darker/lighter backgrounds:

| Token | Light Theme | Dark Theme | Usage |
|-------|-------------|------------|-------|
| `@surface-dim` | `#d9dbd9` | `#0f110f` | Dimmed surface (lowest emphasis) |
| `@surface-bright` | `#fafff8` | `#23312e` | Bright surface (highest emphasis) |
| `@surface-container-lowest` | `#ffffff` | `#0d100e` | Highest elevation container |
| `@surface-container-low` | `#f3f7f4` | `#1a1d1b` | Low elevation container |
| `@surface-container` | `#eff5f2` | `#1e211f` | Default container |
| `@surface-container-high` | `#e8ebe9` | `#282c2a` | High elevation container |
| `@surface-container-highest` | `#e2e5e3` | `#323632` | Highest elevation container |

### 1.7 Surface Variant & Outline

| Token | Light Theme | Dark Theme | Description |
|-------|-------------|------------|-------------|
| `@surface-variant` | `#dbe5e1` | `#3f4946` | Variant surface for contrast |
| `@on-surface-variant` | `#3f4946` | `#bec9c4` | Text/icons on surface variant |
| `@outline` | `#6f7975` | `#89938f` | Subtle borders/outlines |
| `@outline-variant` | `#bec9c4` | `#3f4946` | Less prominent outlines |

### 1.8 Inverse Colors

| Token | Light Theme | Dark Theme | Description |
|-------|-------------|------------|-------------|
| `@inverse-surface` | `#2d312f` | `#e1e3e0` | Inverse of surface |
| `@inverse-on-surface` | `#f0f1ef` | `#2d312f` | Text on inverse surface |
| `@inverse-primary` | `#84e8d0` | `#006b56` | Primary on inverse surface |

### 1.9 Shadow & Scrim

| Token | Light Theme | Dark Theme | Description |
|-------|-------------|------------|-------------|
| `@shadow` | `#000000` | `#000000` | Shadow color |
| `@scrim` | `#000000` | `#000000` | Modal scrim/overlay |

---

## 2. Typography

### 2.1 Font Size Scale

| Token | Size (px) | Usage |
|-------|-----------|-------|
| `.big-display` | 56 | Large display headings |
| `.display` | 36 | Display headings |
| `.title` / `.view-title` | 24 | Screen titles |
| `.subtitle` / `.view-subtitle` | 18 | Subtitles, section headers |
| `.heading` | 16 | Section headings |
| `.content-title` | 16 | Content titles (bold) |
| `.content-subtitle` | 14 | Content subtitles |
| `.body` | 14 | Body text, tooltips |
| Default | 14 | Default label/widget text |
| `.caption` | 12 | Captions, hints |

### 2.2 Font Weight

| Value | Usage |
|-------|-------|
| `normal` | Default, body text |
| `bold` | Emphasis, titles |

### 2.3 Font Variants

| Class | Properties | Usage |
|-------|------------|-------|
| `.monospace` | `font-size: 14`, `font-weight: normal` | Code, monospace content |
| `.tabular` | `font-size: 14`, `font-variant-numeric: tnum` | Numbers, tables |

### 2.4 Font Family

| Token | Value | Usage |
|-------|-------|-------|
| Default | System UI | All UI elements |
| `.big-display`, `.display` | `serif` | Display typography |

---

## 3. Spacing

### 3.1 Base Unit

Cogito uses a 4dp base unit for spacing. All spacing values should be multiples of 4.

### 3.2 Common Spacing Tokens

| Token | Value (dp) | Usage |
|-------|------------|-------|
| `xs` | 4 | Minimal gaps, icon padding |
| `sm` | 8 | Compact padding, small gaps |
| `md` | 12 | Default gaps, list item spacing |
| `lg` | 16 | Standard padding, card padding |
| `xl` | 20 | Large padding |
| `2xl` | 24 | Section spacing |
| `3xl` | 32 | Large section spacing |

### 3.3 Widget-Specific Spacing

#### Padding

| Widget | Padding | Notes |
|--------|---------|-------|
| `button` | `0 16` (horizontal) | Text button padding |
| `button.toolbar_format` | `0 8` | Compact toolbar button |
| `button.toolbar_format_active` | `0 8` | Active toolbar button |
| `chip` | `0 16` | Horizontal padding |
| `textfield` | `8 16` | Input field padding |
| `searchfield` | `8 16` | Search field padding |
| `dropdown` | `8 16` | Dropdown padding |
| `datepicker` | `16` | Date picker container |
| `colorpicker` | `16` | Color picker container |
| `treeview` | `8` | Tree view padding |
| `toasts` | `8 16` | Toast container padding |
| `tooltip` | `8` | Tooltip padding |
| `label` | `8` | Default label padding |
| `toolbar` | `8 16` | Toolbar padding |
| `toolbar.floating` | `8` | Floating toolbar padding |
| `carousel_item` | `16` | Carousel item padding |

#### Margins

| Widget | Margin | Notes |
|--------|--------|-------|
| `nav-rail` | `4 0` | Vertical padding |
| `scroller` | `0` | Default (no padding) |
| `scroller.sidebar` | `0` | Sidebar scroller |

### 3.4 Gap

Container spacing for `vstack`, `hstack`, and `grid`:

| Widget | Default Gap | Notes |
|--------|-------------|-------|
| `vstack` | 0 (configurable) | Vertical container |
| `hstack` | 0 (configurable) | Horizontal container |
| `grid` | Configurable via `cogito_grid_set_gap()` |

---

## 4. Sizing

### 4.1 Widget Minimum Sizes

| Widget | Min Width | Min Height | Notes |
|--------|-----------|------------|-------|
| `button.toolbar_format` | 48 | 48 | Toolbar button |
| `button.toolbar_format_active` | 48 | 48 | Active toolbar button |
| `fab` | 56 | 56 | Standard FAB |
| `chip` | - | 32 | Filter chip |
| `textfield` | - | 56 | Text input |
| `searchfield` | 180 | 56 | Search input |
| `textview` | - | 56 | Multi-line input |
| `dropdown` | - | 56 | Dropdown selector |
| `datepicker` | 360 | - | Date picker |
| `colorpicker` | 360 | - | Color picker |
| `slider` | 360 | 48 | Slider control |
| `stepper` | 96 | 32 | Stepper control |
| `tabs` | - | 42 | Tab bar |
| `progress` | 120 | 12 | Progress bar |
| `toast` | - | 34 | Toast notification |
| `treeview` | 360 | - | Tree view |
| `toolbar` | - | 64 | Toolbar |
| `carousel` | 360 | 240 | Carousel container |
| `checkbox` | 24 | 24 | Checkbox |
| `radio` | 24 | 24 | Radio button |
| `switch` | 64 | 28 | Toggle switch |
| `switch knob` | 32 | 24 | Switch knob |
| `image.about-window-icon` | 128 | 128 | About window icon |

### 4.2 Icon Sizes

| Context | Size (dp) |
|---------|-----------|
| Nav rail item icon | 24 |
| Nav rail toggle icon | 24 |
| Default icon | 24 |

---

## 5. Border Radius

### 5.1 Radius Scale

| Token | Value (dp) | Usage |
|-------|------------|-------|
| `xs` | 4 | Small elements, text field |
| `s` | 6 | Tooltips |
| `m` | 8 | Cards, containers |
| `l` | 10 | Tabs, toasts |
| `xl` | 18 | Pickers, dialogs, FAB (extended) |
| `full` | 100 | Pills, circular elements |

### 5.2 Widget Border Radius

| Widget | Radius | Notes |
|--------|--------|-------|
| `button` | Default | Filled button |
| `button.toolbar_format` | 100 | Pill shape |
| `fab` | 100 | Circular |
| `chip` | 100 | Pill shape |
| `textfield` | `4 4 0 0` | Top corners only |
| `searchfield` | 100 | Pill shape |
| `textview` | 8 | All corners |
| `dropdown` | `4 4 0 0` | Top corners only |
| `datepicker` | 18 | Rounded container |
| `colorpicker` | 18 | Rounded container |
| `slider` | 100 | Track radius |
| `stepper` | 8 | All corners |
| `tabs` | 10 | Tab indicator |
| `progress` | 100 | Track radius |
| `toast` | 10 | Rounded |
| `treeview` | 8 | All corners |
| `toolbar.floating` | 100 | Pill shape |
| `carousel` | 26 | Large radius |
| `carousel_item` | 26 | Large radius |
| `tooltip` | 6 | Small radius |
| `checkbox .box` | 4 | Checkbox container |
| `grid` | 8 | All corners |

---

## 6. Borders

### 6.1 Border Width

| Usage | Width |
|-------|-------|
| Standard border | 1px |
| Switch track | 2px |

### 6.2 Border Styles

| Style | Usage |
|-------|-------|
| `solid` | Default border |
| `none` | No border |

### 6.3 Widget Borders

| Widget | Border | Notes |
|--------|--------|-------|
| `button.outlined` | `1 solid @outline` | Outlined button |
| `chip` | `1 solid @outline` | Filter chip |
| `switch track` | `2 solid @outline` | Switch track border |

---

## 7. Elevation & Shadows

### 7.1 Elevation Levels

| Level | Usage |
|-------|-------|
| 0 | No shadow (flat) |
| 1 | Subtle lift (floating toolbar) |
| 3 | Prominent lift (FAB, dialogs) |

### 7.2 Box Shadow Format

```
box-shadow: [ <dx> <dy> <blur> <spread> <color> ]
```

Example:
```
box-shadow: [ 0 1 3 0 alpha(@shadow, 12%) ]
```

### 7.3 Widget Shadows

| Widget | Shadow |
|--------|--------|
| `datepicker` | `0 1 3 0 alpha(@shadow, 12%)` |
| `colorpicker` | `0 1 3 0 alpha(@shadow, 12%)` |
| `label.carousel-item-label` | `0 2 4 0 alpha(@shadow, 14%)` |

---

## 8. Animation

### 8.1 Duration Tokens

| Token | Duration (ms) | Usage |
|-------|---------------|-------|
| `SHORT1` | 50 | Very fast (FAB close) |
| `SHORT2` | 100 | Fast (switch toggle) |
| `SHORT3` | 150 | Quick (checkbox) |
| `SHORT4` | 200 | Standard (button state) |
| `MEDIUM1` | 250 | Slow (dropdown open) |
| `MEDIUM2` | 300 | Slower (dialog) |
| `MEDIUM3` | 350 | Medium (page transition) |
| `MEDIUM4` | 400 | Extended transitions |
| `LONG1` | 450 | Long transitions |
| `LONG2` | 500 | Extended animations |
| `LONG3` | 550 | Complex animations |
| `LONG4` | 600 | Maximum duration |

### 8.2 Active Latch

| Constant | Value | Purpose |
|----------|-------|---------|
| `COGITO_ACTIVE_LATCH_MS` | 90 | Keep pressed visual briefly to avoid click flash |

### 8.3 Easing Functions

| Token | Value | Usage |
|-------|-------|-------|
| `LINEAR` | 0 | Linear interpolation |
| `STANDARD` | 1 | Smooth start and end |
| `IN` | 2 | Accelerate |
| `OUT` | 3 | Decelerate |
| `IN_OUT` | 4 | Accelerate then decelerate |
| `EMPHASIZED` | 5 | Spring with overshoot (in-back) |
| `EMPHASIZED_OUT` | 6 | Spring decelerate (out-back) |

### 8.4 Physics Spring Constants

| Type | Stiffness (k) | Damping (z) | Usage |
|------|---------------|-------------|-------|
| Expressive | 750.0 | 0.65 | Buttons, sliders, FAB (overshoot allowed) |
| Functional | 1400.0 | 0.90 | Toolbars, layout, opacity (no overshoot) |

---

## 9. Component-Specific Constants

### 9.1 Navigation Rail

#### Collapsed Mode

| Constant | Value (dp) | Description |
|----------|------------|-------------|
| `COLLAPSED_WIDTH` | 96 | Rail width when collapsed |
| `COLLAPSED_TOP_OFFSET` | 24 | Top padding |
| `COLLAPSED_SECTION_GAP` | 36 | Gap between sections |
| `COLLAPSED_ITEM_STEP` | 96 | Vertical step between items |
| `COLLAPSED_INDICATOR_WIDTH` | 64 | Pill indicator width |
| `COLLAPSED_INDICATOR_HEIGHT` | 32 | Pill indicator height |
| `COLLAPSED_INDICATOR_NO_LABEL_HEIGHT` | 56 | Icon-only indicator height |
| `COLLAPSED_ICON_Y_OFFSET` | 4 | Icon centering offset |
| `COLLAPSED_ICON_Y_OFFSET_NO_LABEL` | 16 | Icon-only centering offset |
| `COLLAPSED_INDICATOR_LABEL_GAP` | 4 | Gap between indicator and label |

#### Expanded Mode

| Constant | Value (dp) | Description |
|----------|------------|-------------|
| `EXPANDED_MIN_WIDTH` | 220 | Minimum rail width |
| `EXPANDED_MAX_WIDTH` | 360 | Maximum rail width |
| `EXPANDED_TOP_OFFSET` | 24 | Top padding |
| `EXPANDED_SECTION_GAP` | 20 | Gap between sections |
| `EXPANDED_ITEM_STEP` | 72 | Vertical step between items |
| `EXPANDED_INDICATOR_HEIGHT` | 56 | Indicator height |
| `EXPANDED_INDICATOR_MIN_WIDTH` | 112 | Minimum indicator width |
| `EXPANDED_SIDE_PADDING` | 24 | Horizontal padding |
| `EXPANDED_CONTENT_INSET` | 16 | Content inset |
| `EXPANDED_LABEL_GAP` | 12 | Gap between icon and label |

#### Shared

| Constant | Value (dp) | Description |
|----------|------------|-------------|
| `SHARED_TOGGLE_BUTTON_SIZE` | 48 | Toggle button size |
| `SHARED_TOGGLE_ICON_SIZE` | 24 | Toggle icon size |
| `SHARED_ITEM_ICON_SIZE` | 24 | Item icon size |

### 9.2 Bottom Sheet

| Constant | Value | Description |
|----------|-------|-------------|
| `DEFAULT_RATIO` | 0.60 | Default height ratio (60% of screen) |
| `TOP_INSET` | 56 | Top inset from screen edge |
| `HANDLE_TOUCH_H` | 32 | Touch target height for drag handle |

### 9.3 Side Sheet

| Constant | Value | Description |
|----------|-------|-------------|
| `DEFAULT_RATIO` | 0.40 | Default width ratio (40% of screen) |
| `LEFT_INSET` | 0 | Left inset from screen edge |
| `HANDLE_TOUCH_W` | 32 | Touch target width for drag handle |

---

## 10. State Styles

### 10.1 Interactive States

| State | Description |
|-------|-------------|
| `:hover` | Mouse/pointer hovering |
| `:active` | Being pressed/clicked |
| `:focused` | Has keyboard focus |
| `:checked` | Selected/toggled on |
| `:disabled` | Not interactive |
| `:selection` | Text selection highlight |

### 10.2 State Opacity Values

| State | Opacity | Usage |
|-------|---------|-------|
| Hover overlay | 12% | `alpha(@primary, 12%)` |
| Active overlay | 20% | `alpha(@primary, 20%)` |
| Disabled content | 32% | `alpha(@primary, 32%)` |
| Disabled background | 16% | `alpha(@on-background, 16%)` |

---

## 11. Accessibility

### 11.1 Touch Targets

Minimum touch target size: 48x48 dp

### 11.2 Contrast Ratios

- Text on background: Minimum 4.5:1 (WCAG AA)
- Large text: Minimum 3:1 (WCAG AA)

### 11.3 Focus Indicators

Focus indicators use the primary color with appropriate contrast.

---

## 12. Environment Variables

### 12.1 Accessibility

| Variable | Values | Purpose |
|----------|--------|---------|
| `COGITO_CONTRAST` | `high`, `HIGH` | Enable high contrast mode |
| `COGITO_HIGH_CONTRAST` | `1`, `true`, `yes`, `on` | Enable high contrast mode |
| `COGITO_REDUCED_MOTION` | `1`, `true`, `yes`, `on` | Reduce animations |
| `COGITO_MOTION` | `reduce`, `REDUCE`, `minimal` | Reduce animations |

### 12.2 Theme Variants

SUM themes support `@when` conditions for variant styling:

```
@when dark        // Dark mode
@when light       // Light mode
@when high-contrast  // High contrast mode
@when reduced-motion // Reduced motion mode
@when default     // Base condition (always true)
```

---

## 13. Style Properties Reference

### 13.1 Available Properties

| Property | Type | Description |
|----------|------|-------------|
| `background` | color | Background color |
| `color` | color | Text/icon color |
| `border` | `<width> <style> <color>` | Border shorthand |
| `border-color` | color | Border color |
| `border-radius` | `<tl> <tr> <br> <bl>` | Corner radius |
| `border-width` | int | Border width |
| `border-style` | `none`, `solid`, `dashed`, `dotted` | Border style |
| `padding` | `<top> <right> <bottom> <left>` | Inner spacing |
| `margin` | `<top> <right> <bottom> <left>` | Outer spacing |
| `font-size` | int | Font size in pixels |
| `font-weight` | `normal`, `bold` | Font weight |
| `font-family` | string | Font family name |
| `font-variant-numeric` | `tnum` | Numeric variant |
| `min-width` | int | Minimum width |
| `min-height` | int | Minimum height |
| `max-width` | int | Maximum width |
| `max-height` | int | Maximum height |
| `gap` | int | Container gap |
| `opacity` | float | Element opacity |
| `elevation` | int | Shadow elevation level |
| `box-shadow` | `[ dx dy blur spread color ]` | Box shadow |
| `transition` | `<duration> <easing>` | Animation transition |
| `track-height` | int | Slider/progress track height |
| `icon-size` | int | Icon size |
| `icon-color` | color | Icon color |

### 13.2 Color Functions

| Function | Syntax | Description |
|----------|--------|-------------|
| `alpha()` | `alpha(color, percentage)` | Set color opacity |

Example:
```
background: alpha(@shadow, 40%)
```

---

## 14. Theme Files

Default theme files are located in the `cogito/` directory:

- `cogito_default.sum` - Light theme
- `cogito_dark.sum` - Dark theme

Custom themes can be loaded via:
- `cogito_load_sum_file(path)` - Load from file
- `cogito_load_sum_inline(src)` - Load from string
