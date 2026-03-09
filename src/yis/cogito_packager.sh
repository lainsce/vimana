#!/bin/sh

set -u

entry_path=${1:-}
out_path=${2:-}
app_name=${3:-}

if [ -z "$entry_path" ] || [ -z "$out_path" ] || [ -z "$app_name" ]; then
  exit 0
fi

dirname_path() {
  case "$1" in
    */*) dirname "$1" ;;
    *) printf '.' ;;
  esac
}

stem_name() {
  base=$(basename "$1")
  case "$base" in
    *.*) printf '%s' "${base%.*}" ;;
    *) printf '%s' "$base" ;;
  esac
}

abs_path() {
  case "$1" in
    /*) printf '%s' "$1" ;;
    *) printf '%s/%s' "$(pwd)" "$1" ;;
  esac
}

find_icon_svg() {
  src_dir=$(dirname_path "$entry_path")
  cand="$src_dir/$app_name.svg"
  if [ -f "$cand" ]; then
    printf '%s' "$cand"
    return 0
  fi

  stem=$(stem_name "$entry_path")
  cand="$src_dir/$stem.svg"
  if [ -f "$cand" ]; then
    printf '%s' "$cand"
    return 0
  fi

  cand="$src_dir/icon.svg"
  if [ -f "$cand" ]; then
    printf '%s' "$cand"
    return 0
  fi

  return 1
}

insert_macos_icon_key() {
  plist=$1
  icon_file=$2
  if [ ! -f "$plist" ]; then
    return 1
  fi
  if grep -q 'CFBundleIconFile' "$plist" 2>/dev/null; then
    return 0
  fi
  tmp="$plist.tmp"
  awk -v icon="$icon_file" '
    /<\/dict>/ && !done {
      print "  <key>CFBundleIconFile</key><string>" icon "</string>"
      done = 1
    }
    { print }
  ' "$plist" > "$tmp" && mv "$tmp" "$plist"
}

make_macos_icns() {
  svg_src=$1
  resources_dir=$2
  icon_name=$3

  mkdir -p "$resources_dir" || return 1

  svg_dst="$resources_dir/$icon_name.svg"
  png_qt="$resources_dir/$icon_name.svg.png"
  png_dst="$resources_dir/$icon_name.png"
  iconset_dir="$resources_dir/$icon_name.iconset"
  icns_path="$resources_dir/$icon_name.icns"

  cp -f "$svg_src" "$svg_dst" || return 1
  rm -f "$png_qt" "$png_dst" "$icns_path"
  rm -rf "$iconset_dir"

  qlmanage -t -s 1024 -o "$resources_dir" "$svg_dst" >/dev/null 2>&1 || true
  if [ -f "$png_qt" ]; then
    cp -f "$png_qt" "$png_dst" || return 1
  fi

  if [ ! -f "$png_dst" ] && command -v rsvg-convert >/dev/null 2>&1; then
    rsvg-convert -w 1024 -h 1024 "$svg_dst" -o "$png_dst" >/dev/null 2>&1 || true
  fi
  if [ ! -f "$png_dst" ] && command -v inkscape >/dev/null 2>&1; then
    inkscape "$svg_dst" -w 1024 -h 1024 -o "$png_dst" >/dev/null 2>&1 || true
  fi
  if [ ! -f "$png_dst" ] && command -v magick >/dev/null 2>&1; then
    magick "$svg_dst" -background none -resize 1024x1024 "$png_dst" >/dev/null 2>&1 || true
  fi
  if [ ! -f "$png_dst" ]; then
    return 1
  fi

  mkdir -p "$iconset_dir" || return 1
  for spec in 16:1 16:2 32:1 32:2 128:1 128:2 256:1 256:2 512:1 512:2; do
    points=${spec%:*}
    scale=${spec#*:}
    px=$((points * scale))
    if [ "$scale" -eq 2 ]; then
      out_png="$iconset_dir/icon_${points}x${points}@2x.png"
    else
      out_png="$iconset_dir/icon_${points}x${points}.png"
    fi
    sips -s format png -z "$px" "$px" "$png_dst" --out "$out_png" >/dev/null 2>&1 || return 1
  done

  iconutil -c icns "$iconset_dir" -o "$icns_path" >/dev/null 2>&1 || return 1
  rm -rf "$iconset_dir"
  rm -f "$png_qt" "$png_dst"
  [ -f "$icns_path" ]
}

package_macos() {
  case "$out_path" in
    */Contents/MacOS/*) ;;
    *) return 0 ;;
  esac

  bundle_root=${out_path%/Contents/MacOS/*}
  resources_dir="$bundle_root/Contents/Resources"
  plist="$bundle_root/Contents/Info.plist"

  icon_svg=$(find_icon_svg) || return 0
  if make_macos_icns "$icon_svg" "$resources_dir" "$app_name"; then
    insert_macos_icon_key "$plist" "$app_name.icns" || true
  else
    printf '%s\n' "warning: Cogito packager failed to generate macOS icon from $icon_svg" >&2
  fi
}

package_linux() {
  icon_svg=$(find_icon_svg) || return 0
  abs_out=$(abs_path "$out_path")
  out_dir=$(dirname_path "$abs_out")
  icon_dst="$out_dir/$app_name.svg"
  desktop_path="$out_dir/$app_name.desktop"
  display_name=$(basename "$(dirname_path "$entry_path")")
  if [ -z "$display_name" ] || [ "$display_name" = "." ]; then
    display_name=$app_name
  fi

  cp -f "$icon_svg" "$icon_dst" || return 1
  cat > "$desktop_path" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=$display_name
Exec=$abs_out
Icon=$(abs_path "$icon_dst")
Path=$out_dir
Terminal=false
Categories=AudioVideo;Audio;Player;
StartupNotify=true
EOF
  chmod +x "$desktop_path" >/dev/null 2>&1 || true
}

case "$(uname -s 2>/dev/null || printf unknown)" in
  Darwin) package_macos ;;
  Linux) package_linux ;;
  *) ;;
esac

exit 0