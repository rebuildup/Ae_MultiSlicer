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

echo "=== PiPL Resource Generation ==="
echo "  PLUGIN_DIR: $PLUGIN_DIR"
echo "  PLUGIN_PATH: $PLUGIN_PATH"
echo "  SDK_ROOT: $SDK_ROOT"
echo "  PIPL_SOURCE: $PIPL_SOURCE"
echo "  PIPL_OUTPUT: $PIPL_OUTPUT"

if [ ! -f "$PIPL_SOURCE" ]; then
  echo "Error: PiPL source not found: $PIPL_SOURCE" >&2
  exit 1
fi

if [ ! -d "$SDK_ROOT/Headers" ]; then
  echo "Error: SDK Headers not found at: $SDK_ROOT/Headers" >&2
  exit 1
fi

mkdir -p "$(dirname "$PIPL_OUTPUT")"

# Remove existing resource file to ensure fresh generation
rm -f "$PIPL_OUTPUT"

echo "Running Rez compiler..."
xcrun Rez -useDF \
  -d AE_OS_MAC \
  -d __MACH__ \
  -i "$SDK_ROOT/Headers" \
  -i "$SDK_ROOT/Headers/SP" \
  -i "$SDK_ROOT/Resources" \
  -o "$PIPL_OUTPUT" \
  "$PIPL_SOURCE"

if [ ! -f "$PIPL_OUTPUT" ]; then
  echo "Error: Failed to generate PiPL resource" >&2
  exit 1
fi

echo "PiPL resource generated successfully: $PIPL_OUTPUT"
echo "  Size: $(ls -la "$PIPL_OUTPUT" | awk '{print $5}') bytes"

# Verify the resource contains expected content
if command -v strings &> /dev/null; then
  echo "Verifying PiPL content..."
  if strings "$PIPL_OUTPUT" | grep -q "EffectMain"; then
    echo "  ✓ Found 'EffectMain' entry point in PiPL"
  else
    echo "  ⚠ Warning: 'EffectMain' not found in PiPL resource"
  fi
fi
