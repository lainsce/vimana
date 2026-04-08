#!/bin/sh
# vimana_packager.sh — post-build hook for Vimana apps built with `yis run`
#
# Usage (called by yis run):
#   sh vimana_packager.sh --embed-resources <entry_path> <resource_dir>
#   sh vimana_packager.sh <entry_path> <out_path> <app_name>

set -e

generate_vimana_icon_png() {
  out_png="$1"
  app_key="$2"

  if ! command -v python3 >/dev/null 2>&1; then
  return 1
  fi

  python3 - "$out_png" "$app_key" <<'PY'
import struct
import sys
import zlib


def chunk(tag, data):
  return (
    struct.pack(">I", len(data))
    + tag
    + data
    + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
  )


def in_round_rect(x, y, w, h, r):
  if x < 0 or y < 0 or x >= w or y >= h:
    return False
  cx = x
  cy = y
  if x < r:
    cx = r
  elif x >= w - r:
    cx = w - r - 1
  if y < r:
    cy = r
  elif y >= h - r:
    cy = h - r - 1
  dx = x - cx
  dy = y - cy
  return dx * dx + dy * dy <= r * r


def fnv1a_32(text):
  h = 2166136261
  for b in text.encode("utf-8"):
    h ^= b
    h = (h * 16777619) & 0xFFFFFFFF
  return h


def put_px(idx, w, h, x, y, color):
  if 0 <= x < w and 0 <= y < h:
    p = y * w + x
    if idx[p] != 0:
      idx[p] = color


def draw_line(idx, w, h, x0, y0, x1, y1, color, thickness):
  dx = abs(x1 - x0)
  sx = 1 if x0 < x1 else -1
  dy = -abs(y1 - y0)
  sy = 1 if y0 < y1 else -1
  err = dx + dy
  x = x0
  y = y0
  while True:
    r = thickness // 2
    for oy in range(-r, r + 1):
      for ox in range(-r, r + 1):
        put_px(idx, w, h, x + ox, y + oy, color)
    if x == x1 and y == y1:
      break
    e2 = 2 * err
    if e2 >= dy:
      err += dy
      x += sx
    if e2 <= dx:
      err += dx
      y += sy


def draw_motif(idx, width, height, seed):
  mode = seed % 6

  if mode == 0:
    band = 70 + (seed % 56)
    skew = 0.92 + ((seed >> 8) % 16) / 100.0
    for y in range(height):
      x0 = int((y - height / 2) * skew + width / 2)
      for x in range(x0 - band, x0 + band + 1):
        put_px(idx, width, height, x, y, 3)

  elif mode == 1:
    cx = width // 2
    cy = height // 2
    r0 = 190 + (seed % 70)
    step = 80 + ((seed >> 4) % 18)
    th = 24 + ((seed >> 12) % 12)
    for y in range(height):
      dy = y - cy
      for x in range(width):
        p = y * width + x
        if idx[p] == 0:
          continue
        d = int((x - cx) * (x - cx) + dy * dy)
        for k in range(3):
          rr = r0 + k * step
          lo = (rr - th) * (rr - th)
          hi = (rr + th) * (rr + th)
          if lo <= d <= hi:
            idx[p] = 3
            break

  elif mode == 2:
    grid = 88 + (seed % 72)
    bias = (seed >> 7) & 1
    cx = width // 2
    cy = height // 2
    for y in range(height):
      for x in range(width):
        p = y * width + x
        if idx[p] == 0:
          continue
        if abs(x - cx) + abs(y - cy) > 620:
          continue
        if ((x // grid) + (y // grid) + bias) % 2 == 0:
          idx[p] = 3

  elif mode == 3:
    bars = 3 + (seed % 4)
    margin = 220
    gap = (width - margin * 2) // (bars + 1)
    half = 34 + ((seed >> 5) % 20)
    top = 220
    bot = 804
    for i in range(1, bars + 1):
      cx = margin + i * gap
      for y in range(top, bot):
        taper = abs((y - (top + bot) // 2) / float(bot - top))
        hw = int(half * (1.0 - taper * 0.55))
        for x in range(cx - hw, cx + hw + 1):
          put_px(idx, width, height, x, y, 3)

  elif mode == 4:
    edge = 260 + (seed % 120)
    for y in range(height):
      for x in range(width):
        p = y * width + x
        if idx[p] == 0:
          continue
        if x + y < edge:
          idx[p] = 3
        elif (width - 1 - x) + y < edge:
          idx[p] = 3
        elif x + (height - 1 - y) < edge:
          idx[p] = 3
        elif (width - 1 - x) + (height - 1 - y) < edge:
          idx[p] = 3

  else:
    cx = width // 2
    cy = height // 2
    rays = 8 + (seed % 7)
    r = 370
    th = 30 + ((seed >> 10) % 12)
    for i in range(rays):
      ang = (i * 6.283185307179586) / rays
      x1 = int(cx + r * __import__("math").cos(ang))
      y1 = int(cy + r * __import__("math").sin(ang))
      draw_line(idx, width, height, cx, cy, x1, y1, 3, th)


def write_png(path, width, height, radius, app_key):
  # 2-bit palette (4 colors): transparent, light surface, dark ink, accent.
  palettes = [
    ((0xEA, 0xE4, 0xD4), (0x2A, 0x2A, 0x24), (0xB8, 0x7A, 0x1A)),
    ((0xEE, 0xE8, 0xD9), (0x1F, 0x29, 0x38), (0xC9, 0x4C, 0x45)),
    ((0xE6, 0xEC, 0xE0), (0x1E, 0x2F, 0x26), (0xBC, 0x6B, 0x2C)),
    ((0xE8, 0xE7, 0xF0), (0x2B, 0x1F, 0x3A), (0x24, 0x8A, 0x8A)),
    ((0xF1, 0xE8, 0xE2), (0x3B, 0x23, 0x1B), (0x1E, 0x70, 0xB8)),
    ((0xE4, 0xED, 0xF0), (0x1C, 0x24, 0x30), (0xC1, 0x5F, 0x1F)),
    ((0xEC, 0xE5, 0xDB), (0x2D, 0x27, 0x20), (0x7D, 0x4D, 0xB5)),
    ((0xE7, 0xEF, 0xE7), (0x22, 0x2A, 0x22), (0xB0, 0x3C, 0x6E)),
  ]
  seed = fnv1a_32(app_key)
  bg, ink, accent = palettes[seed % len(palettes)]
  palette = bytes([0x00, 0x00, 0x00, bg[0], bg[1], bg[2], ink[0], ink[1], ink[2], accent[0], accent[1], accent[2]])
  # Only palette index 0 is transparent.
  trns = bytes([0x00, 0xFF, 0xFF, 0xFF])

  idx = bytearray(width * height)

  # Rounded-rectangle base.
  for y in range(height):
    row = y * width
    for x in range(width):
      if in_round_rect(x, y, width, height, radius):
        idx[row + x] = 1

  # Framed inner rounded rectangle for a stronger icon silhouette.
  pad = 128 + ((seed >> 5) % 28)
  inner_r = 152 + ((seed >> 11) % 54)
  for y in range(pad, height - pad):
    row = y * width
    for x in range(pad, width - pad):
      if in_round_rect(x - pad, y - pad, width - pad * 2, height - pad * 2, inner_r):
        idx[row + x] = 2

  draw_motif(idx, width, height, seed)

  # Add a deterministic app marker notch strip near the bottom.
  mark = seed ^ 0xA53A9B17
  y0 = 820
  for i in range(10):
    if ((mark >> i) & 1) == 0:
      continue
    x0 = 186 + i * 66
    for y in range(y0, y0 + 34):
      for x in range(x0, x0 + 42):
        put_px(idx, width, height, x, y, 3)

  # Pack 2-bit indexed rows with filter byte 0.
  raw = bytearray()
  for y in range(height):
    raw.append(0)
    base = y * width
    for x in range(0, width, 4):
      a = idx[base + x] & 0x03
      b = idx[base + x + 1] & 0x03
      c = idx[base + x + 2] & 0x03
      d = idx[base + x + 3] & 0x03
      raw.append((a << 6) | (b << 4) | (c << 2) | d)

  ihdr = struct.pack(">IIBBBBB", width, height, 2, 3, 0, 0, 0)
  idat = zlib.compress(bytes(raw), 9)

  with open(path, "wb") as f:
    f.write(b"\x89PNG\r\n\x1a\n")
    f.write(chunk(b"IHDR", ihdr))
    f.write(chunk(b"PLTE", palette))
    f.write(chunk(b"tRNS", trns))
    f.write(chunk(b"IDAT", idat))
    f.write(chunk(b"IEND", b""))


def main():
  out_png = sys.argv[1]
  app_key = (sys.argv[2] if len(sys.argv) > 2 else "VIMANA") or "VIMANA"
  write_png(out_png, 1024, 1024, 250, app_key)


if __name__ == "__main__":
  main()
PY
}

build_icns_from_master() {
  resources_dir="$1"
  master_png="$2"

  if ! command -v iconutil >/dev/null 2>&1; then
  return 1
  fi

  iconset_dir="$resources_dir/AppIcon.iconset"
  rm -rf "$iconset_dir"
  mkdir -p "$iconset_dir"

  # Keep only the 1024 master so icon data remains exactly the generated 2-bit source.
  cp "$master_png" "$iconset_dir/icon_512x512@2x.png"

  iconutil -c icns "$iconset_dir" -o "$resources_dir/AppIcon.icns" >/dev/null
  rm -rf "$iconset_dir"
}

set_bundle_icon_plist_key() {
  plist_path="$1"
  if [ ! -f "$plist_path" ]; then
  return 0
  fi

  if [ -x /usr/libexec/PlistBuddy ]; then
  /usr/libexec/PlistBuddy -c "Set :CFBundleIconFile AppIcon" "$plist_path" >/dev/null 2>&1 || \
    /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string AppIcon" "$plist_path" >/dev/null 2>&1 || true
  fi
}

entry_dir_appicon_png() {
  entry_dir="$1"
  p="$entry_dir/AppIcon.png"
  if [ -f "$p" ]; then
    echo "$p"
    return 0
  fi
  return 1
}

case "$1" in
  --embed-resources)
    # No compile-time resources to embed for Vimana apps.
    exit 0
    ;;
  *)
    ENTRY_PATH="$1"
    OUT_PATH="$2"
    APP_NAME="$3"
    APP_ICON_KEY="$APP_NAME"

    # Derive bundle root from out_path (strip /Contents/MacOS/<exe>)
    case "$OUT_PATH" in
      */Contents/MacOS/*)
        CONTENTS_DIR="${OUT_PATH%%/Contents/MacOS/*}/Contents"
        RESOURCES_DIR="$CONTENTS_DIR/Resources"
        ;;
      *)
        exit 0
        ;;
    esac

    # Copy any app-adjacent asset files (.chr, .png, .toml) to Resources.
    ENTRY_DIR="$(dirname "$ENTRY_PATH")"
    if [ -d "$RESOURCES_DIR" ] && [ -d "$ENTRY_DIR" ]; then
      for f in "$ENTRY_DIR"/*.chr "$ENTRY_DIR"/*.png "$ENTRY_DIR"/*.toml; do
        [ -f "$f" ] && cp "$f" "$RESOURCES_DIR/"
      done
    fi

    # Use the extras app folder name (extras/<App>/...) as the icon key when available.
    REST_PATH=""
    if [ "${ENTRY_PATH#extras/}" != "$ENTRY_PATH" ]; then
      REST_PATH="${ENTRY_PATH#extras/}"
    elif [ "${ENTRY_PATH#*/extras/}" != "$ENTRY_PATH" ]; then
      REST_PATH="${ENTRY_PATH#*/extras/}"
    fi
    if [ -n "$REST_PATH" ]; then
      EXTRA_APP="${REST_PATH%%/*}"
      if [ -n "$EXTRA_APP" ] && [ "$EXTRA_APP" != "$REST_PATH" ]; then
        APP_ICON_KEY="$EXTRA_APP"
      fi
    fi

    USER_ICON_PNG=""
    if [ -d "$ENTRY_DIR" ]; then
      USER_ICON_PNG="$(entry_dir_appicon_png "$ENTRY_DIR" || true)"
    fi

    # macOS app icon generation for Vimana bundles:
    # - 1024x1024 source image
    # - 250 px rounded corners
    # - 2-bit indexed palette
    if [ "$(uname)" = "Darwin" ] && [ -d "$RESOURCES_DIR" ]; then
      MASTER_ICON_PNG="$RESOURCES_DIR/AppIcon-1024.png"
      ICONS_FILE="$RESOURCES_DIR/AppIcon.icns"
      PLIST_PATH="$CONTENTS_DIR/Info.plist"

      # Prefer AppIcon.png next to the entry .yi file; fallback to generated icon when absent.
      if [ -n "$USER_ICON_PNG" ]; then
        cp "$USER_ICON_PNG" "$MASTER_ICON_PNG"
        if build_icns_from_master "$RESOURCES_DIR" "$MASTER_ICON_PNG"; then
          set_bundle_icon_plist_key "$PLIST_PATH"
        fi
      elif [ -f "$ICONS_FILE" ]; then
        set_bundle_icon_plist_key "$PLIST_PATH"
      elif [ -f "$MASTER_ICON_PNG" ]; then
        if build_icns_from_master "$RESOURCES_DIR" "$MASTER_ICON_PNG"; then
          set_bundle_icon_plist_key "$PLIST_PATH"
        fi
      elif generate_vimana_icon_png "$MASTER_ICON_PNG" "$APP_ICON_KEY"; then
        if build_icns_from_master "$RESOURCES_DIR" "$MASTER_ICON_PNG"; then
          set_bundle_icon_plist_key "$PLIST_PATH"
        fi
      fi
    fi

    # ── Embed dylibs for self-contained .app bundle ──────────────────────
    if [ "$(uname)" = "Darwin" ]; then
      FRAMEWORKS_DIR="$CONTENTS_DIR/Frameworks"
      mkdir -p "$FRAMEWORKS_DIR"

      # Find libvimana.dylib path from the linked executable.
      VIMANA_REF="$(otool -L "$OUT_PATH" 2>/dev/null \
        | grep 'libvimana' | head -1 | awk '{print $1}')"

      # Resolve @rpath to the actual file by searching common locations.
      VIMANA_LIB="$VIMANA_REF"
      case "$VIMANA_LIB" in
        @rpath/*)
          VIMANA_LEAF="${VIMANA_LIB#@rpath/}"
          for d in /opt/homebrew/lib /usr/local/lib /usr/lib; do
            if [ -f "$d/$VIMANA_LEAF" ]; then
              VIMANA_LIB="$d/$VIMANA_LEAF"; break
            fi
          done
          ;;
      esac

      if [ -f "$VIMANA_LIB" ]; then
        cp "$VIMANA_LIB" "$FRAMEWORKS_DIR/libvimana.dylib"

        # Find SDL3 dependency of libvimana.
        SDL3_REF="$(otool -L "$VIMANA_LIB" 2>/dev/null \
          | grep 'libSDL3' | head -1 | awk '{print $1}')"
        SDL3_LIB="$SDL3_REF"
        # Resolve @rpath in SDL3 reference too.
        case "$SDL3_LIB" in
          @rpath/*)
            SDL3_LEAF="${SDL3_LIB#@rpath/}"
            for d in /opt/homebrew/lib /usr/local/lib /usr/lib; do
              if [ -f "$d/$SDL3_LEAF" ]; then
                SDL3_LIB="$d/$SDL3_LEAF"; break
              fi
            done
            ;;
        esac
        if [ -f "$SDL3_LIB" ]; then
          cp "$SDL3_LIB" "$FRAMEWORKS_DIR/libSDL3.dylib"
        fi

        # Fix install names on embedded copies.
        install_name_tool -id \
          @executable_path/../Frameworks/libvimana.dylib \
          "$FRAMEWORKS_DIR/libvimana.dylib" 2>/dev/null || true

        if [ -f "$FRAMEWORKS_DIR/libSDL3.dylib" ]; then
          install_name_tool -id \
            @executable_path/../Frameworks/libSDL3.dylib \
            "$FRAMEWORKS_DIR/libSDL3.dylib" 2>/dev/null || true

          # Rewrite libvimana's reference to SDL3.
          if [ -n "$SDL3_REF" ]; then
            install_name_tool -change "$SDL3_REF" \
              @executable_path/../Frameworks/libSDL3.dylib \
              "$FRAMEWORKS_DIR/libvimana.dylib" 2>/dev/null || true
          fi
        fi

        # Ad-hoc codesign embedded dylibs first, then the whole bundle.
        codesign --force --sign - \
          "$FRAMEWORKS_DIR/libvimana.dylib" 2>/dev/null || true
        [ -f "$FRAMEWORKS_DIR/libSDL3.dylib" ] && \
          codesign --force --sign - \
            "$FRAMEWORKS_DIR/libSDL3.dylib" 2>/dev/null || true
      fi
    fi

    # Ad-hoc codesign the full .app bundle (deep) for Gatekeeper.
    APP_BUNDLE="$(cd "$CONTENTS_DIR/.." && pwd)"
    codesign --force --deep --sign - "$APP_BUNDLE" 2>/dev/null || true

    exit 0
    ;;
esac
