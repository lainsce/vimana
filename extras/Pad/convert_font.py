#!/usr/bin/env python3
"""Convert notepad.tal newyork12 font to Vimana set_font_glyph/set_font_width calls."""

import re, sys

tal_path = '/Users/nayu/Developer/Cogito/Uxn/notepad.tal'
with open(tal_path, 'r') as f:
    content = f.read()

# --- Extract font data ---
# Structure: @font [ <widths 224 bytes> &glyphs [ <glyph data> ] ]
font_start = content.find('@font')
if font_start < 0:
    print("ERROR: Could not find @font", file=sys.stderr)
    sys.exit(1)

# Find the bracket after @font
bracket_start = content.find('[', font_start)
glyphs_label = content.find('&glyphs', font_start)

# Width data is between first '[' and '&glyphs'
width_section = content[bracket_start+1:glyphs_label]
# Extract hex bytes
width_bytes = []
for word in re.findall(r'[0-9a-fA-F]{2,}', width_section):
    for i in range(0, len(word), 2):
        width_bytes.append(int(word[i:i+2], 16))

# First 96 widths for ASCII chars 0x20-0x7F
widths = width_bytes[:96]
print(f"-- Extracted {len(width_bytes)} width bytes, using first 96", file=sys.stderr)

# --- Extract glyph data ---
# Find bracket after &glyphs
glyph_bracket = content.find('[', glyphs_label)
# Find matching ']' - search for ] that ends the glyph data
# Stop at @chicago or end of file
glyph_end = content.find('@chicago', glyph_bracket)
if glyph_end < 0:
    glyph_end = len(content)
glyph_section = content[glyph_bracket:glyph_end]

# Find the closing bracket
closing = glyph_section.rfind(']')
if closing >= 0:
    glyph_section = glyph_section[:closing]

# Extract hex bytes (skip labels like &glyphs, $1, etc)
glyph_bytes = []
for word in re.findall(r'[0-9a-fA-F]{2,}', glyph_section):
    for i in range(0, len(word), 2):
        glyph_bytes.append(int(word[i:i+2], 16))

# Each char = 32 bytes (16 plane0 + 16 plane1)
num_chars = min(len(glyph_bytes) // 32, 96)

# --- Convert to 2bpp format ---
def byte_to_2bpp(p0, p1):
    """Convert plane0 byte and plane1 byte to 2bpp uint16."""
    result = 0
    for bit in range(8):
        b0 = (p0 >> (7 - bit)) & 1
        b1 = (p1 >> (7 - bit)) & 1
        color = b1 * 2 + b0
        result |= (color << ((7 - bit) * 2))
    return result

# --- Generate Yi code ---
print("-- Newyork12 font: set_font_glyph calls")
print("-- Generated from notepad.tal")
print()

for ci in range(num_chars):
    char_code = 0x20 + ci
    base = ci * 32
    p0 = glyph_bytes[base:base+16]
    p1 = glyph_bytes[base+16:base+32]
    
    if len(p0) < 16 or len(p1) < 16:
        break
    
    rows = []
    for r in range(16):
        val = byte_to_2bpp(p0[r], p1[r])
        rows.append(f'"{val:04x}"')
    
    row_str = ", ".join(rows)
    
    # Print as Yi code
    if char_code >= 33 and char_code <= 126:
        ch = chr(char_code)
        if ch == '"':
            ch = 'dquote'
        elif ch == '\\':
            ch = 'backslash'
        print(f'    scr.set_font_glyph({char_code}, [{row_str}]) -- {ch}')
    else:
        print(f'    scr.set_font_glyph({char_code}, [{row_str}])')

print()
print("-- Font widths")
for ci in range(min(len(widths), 96)):
    char_code = 0x20 + ci
    w = widths[ci]
    if char_code >= 32 and char_code <= 126:
        print(f'    scr.set_font_width({char_code}, {w})')

print()
print("-- Width table array for model")
print("def CHAR_WIDTHS = [", end="")
for i, w in enumerate(widths[:96]):
    if i > 0:
        print(", ", end="")
    if i % 16 == 0 and i > 0:
        print()
        print("    ", end="")
    print(w, end="")
print("]")

# --- Also extract special icons ---
print()
print("-- Special icons (8-byte arrays)")

# Extract specific icons from notepad.tal
icon_patterns = {
    'ICN_FILL': r'@fill-icn\s*\[\s*((?:[0-9a-fA-F]+\s*)+)\]',
    'ICN_ERROR': r'@error-icn\s*\[\s*((?:[0-9a-fA-F]+\s*)+)\]',
    'ICN_BLINK': r'@blink-icn\s*\[\s*((?:[0-9a-fA-F]+\s*)+)\]',
    'ICN_BOTTOM': r'@bottom-icn\s*\[\s*((?:[0-9a-fA-F]+\s*)+)\]',
}

for name, pattern in icon_patterns.items():
    m = re.search(pattern, content)
    if m:
        hex_data = m.group(1)
        bytes_list = []
        for word in hex_data.split():
            for i in range(0, len(word), 2):
                bytes_list.append(int(word[i:i+2], 16))
        arr_str = ", ".join(str(b) for b in bytes_list[:8])
        print(f'def {name} = [{arr_str}]')

# Extract bullet and marker icons (16 bytes each, 2-plane)
for icon_name, label in [('ICN_BULLET', 'bullet-icn'), ('ICN_MARKER', 'marker-icn')]:
    pattern = rf'@{label}\s*(?:\([^)]*\)\s*)?\[\s*((?:[0-9a-fA-F]+\s*)+)\]'
    m = re.search(pattern, content)
    if m:
        hex_data = m.group(1)
        bytes_list = []
        for word in hex_data.split():
            for i in range(0, len(word), 2):
                bytes_list.append(int(word[i:i+2], 16))
        # These are 16-byte 2-plane 8x8 sprites. Convert to 2bpp for set_font_glyph
        # Top row: [p0_0..p0_7], Bottom row: [p1_0..p1_7] — but these are 8x8 only
        # Store as put_icn data (just plane 0, 8 bytes)
        p0 = bytes_list[:8]
        p1 = bytes_list[8:16] if len(bytes_list) >= 16 else [0]*8
        arr_str = ", ".join(str(b) for b in p0)
        print(f'def {icon_name}_P0 = [{arr_str}]')
        arr_str = ", ".join(str(b) for b in p1)
        print(f'def {icon_name}_P1 = [{arr_str}]')
        # Also as 2bpp for set_font_glyph (8x8, so 8 rows of 2bpp)
        rows = []
        for r in range(8):
            val = byte_to_2bpp(p0[r], p1[r])
            rows.append(f'"{val:04x}"')
        row_str = ", ".join(rows)
        print(f'-- {icon_name} 2bpp: [{row_str}]')

# Mouse icon
m = re.search(r'@mouse-icn\s*\[\s*((?:[0-9a-fA-F]+\s*)+)\]', content)
if m:
    hex_data = m.group(1)
    bytes_list = []
    for word in hex_data.split():
        for i in range(0, len(word), 2):
            bytes_list.append(int(word[i:i+2], 16))
    arr_str = ", ".join(str(b) for b in bytes_list[:8])
    print(f'def ICN_MOUSE = [{arr_str}]')

# Caret icon (16 bytes for 8x16)
m = re.search(r'@caret-icn\s*\[\s*((?:[0-9a-fA-F]+\s*)+)\]', content)
if m:
    hex_data = m.group(1)
    bytes_list = []
    for word in hex_data.split():
        for i in range(0, len(word), 2):
            bytes_list.append(int(word[i:i+2], 16))
    arr_str = ", ".join(str(b) for b in bytes_list[:8])
    print(f'def ICN_CARET_TOP = [{arr_str}]')
    if len(bytes_list) >= 16:
        arr_str = ", ".join(str(b) for b in bytes_list[8:16])
        print(f'def ICN_CARET_BOT = [{arr_str}]')

print()
print("-- Corner widget chr data (3 states, each 4x4 tiles = 16 sprites of 8 bytes)")
# corner/default-chr has complex multi-state data
# For now just note it exists - we'll handle it separately
