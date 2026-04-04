#!/bin/sh
# vimana_packager.sh — post-build hook for Vimana apps built with `yis run`
#
# Usage (called by yis run):
#   sh vimana_packager.sh --embed-resources <entry_path> <resource_dir>
#   sh vimana_packager.sh <entry_path> <out_path> <app_name>

set -e

case "$1" in
  --embed-resources)
    # No compile-time resources to embed for Vimana apps.
    exit 0
    ;;
  *)
    ENTRY_PATH="$1"
    OUT_PATH="$2"
    APP_NAME="$3"

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

    exit 0
    ;;
esac
