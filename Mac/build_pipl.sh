#!/bin/bash
set -euo pipefail

if [ $# -lt 3 ]; then
  echo "Usage: $0 <plugin_dir> <plugin_path> <sdk_root>" >&2
  exit 1
fi

PLUGIN_DIR="$1"
PLUGIN_PATH="$2"
SDK_ROOT="$3"

PIPL_SOURCE="$PLUGIN_DIR/MultiSlicerPiPL.r"
PIPL_OUTPUT="$PLUGIN_PATH/Contents/Resources/MultiSlicer.rsrc"

if [ ! -f "$PIPL_SOURCE" ]; then
  echo "PiPL source not found: $PIPL_SOURCE" >&2
  exit 1
fi

mkdir -p "$(dirname "$PIPL_OUTPUT")"

xcrun Rez -useDF \
  -d AE_OS_MAC \
  -d __MACH__ \
  -i "$SDK_ROOT/Headers" \
  -i "$SDK_ROOT/Headers/SP" \
  -i "$SDK_ROOT/Resources" \
  -o "$PIPL_OUTPUT" \
  "$PIPL_SOURCE"

echo "PiPL resource generated: $PIPL_OUTPUT"
