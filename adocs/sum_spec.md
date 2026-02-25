# Sum (Style Unified Model) Styling Language — Working Spec

This document defines the current SUM behavior used by Ergo/Cogito in this repository.
It supersedes the old "minimal v1" draft for implementation work.

## 0. Status

- This is a practical implementation spec for current tooling.
- It describes what the runtime parser accepts today.
- Compiler work should target this profile first.

## 1. Source Format

- Encoding: UTF-8 text.
- Extension: `.sum`.
- Comments: `;` starts a comment and runs to end of line.
- Empty lines are allowed.
- Files are indentation-based.

### 1.1 Preprocessor Directives

SUM supports lightweight directives before rule parsing:

- Theme token:
  - `@name: value`
  - Example: `@primary: #6750A4`
- Diagnostics mode:
  - `@diagnostics: off|warn|strict`
- File composition:
  - `@bring: "path/to/base.sum"` (quotes optional for paths without spaces)
  - Paths resolve relative to the file containing the directive.
  - Imported content is inlined at the `@bring` line, so later declarations still override earlier ones.
- Variant condition block:
  - `@when <condition>`
  - Condition examples: `dark`, `light`, `high-contrast`, `reduced-motion`
  - Negation: `!dark` or `not dark`

### 1.2 Indentation Rules (Current Runtime)

- Selector lines are unindented (column 0).
- Declaration lines are indented.
- In practice, use **2 spaces** for declaration indentation.
- Tabs should be avoided for now.
- `@when` block contents are normally indented one level under `@when`.

## 2. Rule Structure

A SUM file is a sequence of rule groups.

Each rule group is:

```txt
selector-line+
  declaration-line+
```

Notes:

- Multiple selector lines can appear back-to-back before declarations.
- A selector line can also contain comma-separated selectors.
- A declaration line is `property: value`.

## 3. Selectors

SUM selectors are intentionally limited.

### 3.1 Simple Selectors

- Universal: `*`
- Type: `button`
- Class: `.fancy`
- Type + class: `button.fancy`

### 3.2 State Selectors

States use `:state` suffixes:

- `:hover`
- `:active`
- `:checked`
- `:disabled`
- `:selection`

Examples:

- `button:hover`
- `.outlined:active`
- `textfield:selection`

### 3.3 Descendant Selectors (Single Hop)

Two-token selectors are supported as `parent child`.

Examples:

- `appbar iconbtn`
- `checkbox .check`
- `switch .track`

Current parser behavior supports one parent token + one child token.

### 3.3.1 Element Position Selectors

Special selectors target children based on their position within a parent container using the `=` suffix. This is primarily used for styling items in list-like containers (e.g., `content-list`).

Syntax: `widget=position`

Supported positions:
- `first` - First child in the container
- `last` - Last child in the container
- `middle` - Neither first nor last (intermediate children)
- `single` - Only child (first and last simultaneously)

Examples:

```sum
; First item in content-list has full top radius
content-list=first
  border-radius: 12 12 8 8
  background: @surface

; Last item in content-list has full bottom radius
content-list=last
  border-radius: 8 8 12 12
  background: @surface

; Middle items have reduced radius
content-list=middle
  border-radius: 8
  background: @surface

; Single item gets full radius
content-list=single
  border-radius: 12
  background: @surface
```

Element position selectors enable precise styling of container children without requiring manual class assignment in the application code.

### 3.4 Selector Lists

Comma-separated selectors are supported:

```sum
textfield:selection, .textfield:selection
  background: #6750A4
```

### 3.5 Supported Type Names (Current)

Current runtime recognizes these type selectors:

- `appbar`
- `bottom-nav`
- `button`
- `checkbox`
- `chip`
- `colorpicker`
- `datepicker`
- `dialog`
- `dialogslot`
- `divider`
- `dropdown`
- `fab`
- `fixed`
- `grid`
- `hstack`
- `iconbtn`
- `image`
- `label`
- `list`
- `menu`
- `nav-rail`
- `popover`
- `progress`
- `scroller`
- `searchfield`
- `buttongroup`
- `slider`
- `stepper`
- `switch`
- `tabs`
- `textfield`
- `textview`
- `toast`
- `toasts`
- `toolbar`
- `tooltip`
- `treeview`
- `viewswitcher`

Aliases currently accepted include:

- `bottom_nav`
- `dialog-slot`
- `dialog_slot`

Unknown selectors are ignored.

## 4. Cascade Semantics (Current)

- Rules are applied in source order.
- For the same target/style bucket/property, later assignments overwrite earlier ones.
- Unknown properties are ignored.
- Invalid values for a known property are ignored for that declaration only.

## 5. Value Types

### 5.1 Numbers and Units

- Integer and float numbers are supported.
- `sp` suffix is accepted and treated as a styling length unit.
- `px` suffix is accepted by the numeric parser.
- Runtime stores numeric style values as numbers; unit conversion is currently 1:1 in practice.

Examples:

- `14`
- `14sp`
- `14px`
- `0.75`

### 5.2 Colors

Supported color forms:

- `#RGB`
- `#RGBA`
- `#RRGGBB`
- `#RRGGBBAA`
- `rgb(r, g, b)`
- `rgba(r, g, b, a)`
- named colors: `transparent`, `white`, `black`
- token refs: `@primary`
- function: `alpha(color, amount)`
- function: `mix(colorA, colorB, t)`

`alpha()`:

- `amount` accepts `0..1`, `0..255`, or `%`.
- Produces a hex color with adjusted alpha.

`mix()`:

- `t` accepts `0..1` or `%`.
- `0` => first color, `1` => second color.

### 5.3 Strings and Idents

- Double-quoted strings are supported (for example font family in `font`).
- Bare identifiers are supported where relevant.

### 5.4 Durations

For transition-related properties:

- Bare number means milliseconds.
- `ms` suffix means milliseconds.
- `s` suffix means seconds.

Examples:

- `transition-duration: 180`
- `transition-duration: 180ms`
- `transition-duration: 0.18s`

## 6. Properties

### 6.1 Core Visual

- `background`, `background-color`
- `color`, `text-color`
- `opacity`
- `border`
- `border-color`
- `border-width`
- `border-radius`, `radius`
- `box-shadow`

#### Border Radius Shorthand

The `border-radius` property accepts 1-4 values following CSS conventions:

- `border-radius: 12` — All corners 12px
- `border-radius: 12 8` — Top/bottom 12px, left/right 8px
- `border-radius: 12 8 6` — Top 12px, left/right 8px, bottom 6px
- `border-radius: 12 8 6 4` — Top-left 12px, top-right 8px, bottom-right 6px, bottom-left 4px

Order: **top-left**, **top-right**, **bottom-right**, **bottom-left
- `elevation`

### 6.2 Typography

- `font`
- `font-family`
- `font-size`
- `font-weight`
- `font-variant-numeric`
- `letter-spacing`

### 6.3 Spacing and Size

- `padding`
- `padding-left`, `padding-top`, `padding-right`, `padding-bottom`
- `margin`
- `margin-left`, `margin-top`, `margin-right`, `margin-bottom`
- `min-width`, `min-height`
- `max-width`, `max-height`

### 6.4 Interaction/State Colors

- `selection-color`
- `selection-background`
- `highlight-color`
- `transition`
- `transition-duration`
- `transition-easing`
- `transition-timing-function`

### 6.5 Control-Specific

- `icon-size`
- `icon-color`, `icon-tint`
- `track-height`
- `track-color`, `track`
- `knob-color`, `knob`
- `knob-width`, `knob-w`
- `knob-height`, `knob-h`
- `check-color`, `check`

### 6.6 Menu/Appbar-Specific

- `item-padding`, `menu-item-padding`
- `item-height`, `menu-item-height`
- `appbar-btn-size`
- `appbar-btn-gap`
- `appbar-btn-top`
- `appbar-btn-right`
- `appbar-btn-close-color`
- `appbar-btn-min-color`
- `appbar-btn-max-color`
- `appbar-btn-border-color`
- `appbar-btn-border-width`

## 7. Shorthand Behavior

### 7.1 `padding`, `margin`, `radius`

1 to 4 values are supported with CSS-style mapping:

- 1 value: all sides/corners.
- 2 values: vertical/horizontal or paired corners.
- 3 values: top, horizontal, bottom.
- 4 values: top, right, bottom, left.

### 7.2 `border`

Syntax:

```txt
border: <width> <style> <color>
```

- Width is numeric.
- Style: `none | solid | dashed | dotted`.
- Color uses standard color parsing.

### 7.3 `font`

Current shorthand form:

```txt
font: <family> <size> <weight>
```

- Family: quoted string or identifier.
- Size: numeric.
- Weight: numeric or `normal`/`bold`.

### 7.4 `box-shadow`

Current parser supports a single shadow value (optional `[...]` wrapper):

```txt
box-shadow: [ <dx> <dy> [<blur>] [<spread>] <color> [inset] ]
```

## 8. Error Handling

- Unknown selectors are ignored.
- Unknown properties are ignored.
- Invalid declarations do not abort parsing of other declarations.
- Blank/comment lines are skipped.

### 8.1 Diagnostics Mode

Diagnostics can be configured with:

```sum
@diagnostics: warn
```

Modes:

- `off`: suppress SUM diagnostics.
- `warn`: emit warnings and continue (default).
- `strict`: emit errors and abort SUM parse on strict failures.

Message format:

- `warn: line <n>: <message>`
- `error: line <n>: <message>`

## 9. Example

```sum
@diagnostics: warn
@primary: #6750A4

; Base
*
  color: #222
  font: "Inter" 14sp 400

window
  background: #fafafa

button.outlined
  background: alpha(@primary, 12%)
  color: @primary
  border: 1sp solid mix(@primary, #ffffff, 20%)
  border-radius: 20sp
  transition: 180 standard

button.outlined:hover
  background: #6750A41f

button.outlined:active
  background: #6750A433

textfield:selection, .textfield:selection
  background: @primary

@when dark
  window
    background: #121212
```
