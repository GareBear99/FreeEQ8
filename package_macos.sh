#!/usr/bin/env bash
set -euo pipefail

# Package FreeEQ8 VST3 + AU into a macOS .dmg installer.
# Usage: ./package_macos.sh [build_dir]
#   build_dir defaults to ./build

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-$ROOT/build}"
ARTEFACTS="$BUILD_DIR/FreeEQ8_artefacts/Release"
VERSION=$(grep 'project(FreeEQ8' "$ROOT/CMakeLists.txt" | sed 's/.*VERSION \([0-9.]*\).*/\1/')
DMG_NAME="FreeEQ8-v${VERSION}-macOS"
STAGING="$BUILD_DIR/dmg_staging"

echo "=== Packaging FreeEQ8 v${VERSION} for macOS ==="

# Verify artefacts exist
VST3="$ARTEFACTS/VST3/FreeEQ8.vst3"
AU="$ARTEFACTS/AU/FreeEQ8.component"

if [ ! -d "$VST3" ]; then
    echo "ERROR: VST3 not found at $VST3"
    echo "Run ./build_macos.sh first."
    exit 1
fi

# Clean staging area
rm -rf "$STAGING"
mkdir -p "$STAGING"

# Copy plugins
cp -R "$VST3" "$STAGING/"
if [ -d "$AU" ]; then
    cp -R "$AU" "$STAGING/"
fi

# Create a README for the DMG
cat > "$STAGING/README.txt" << 'EOF'
FreeEQ8 — Professional 8-Band Parametric EQ
============================================

INSTALLATION
------------
VST3:
  Drag FreeEQ8.vst3 to:
    ~/Library/Audio/Plug-Ins/VST3/

AU (Audio Unit):
  Drag FreeEQ8.component to:
    ~/Library/Audio/Plug-Ins/Components/

Then rescan plugins in your DAW:
  - Ableton Live: Preferences → Plug-ins → Rescan
  - Logic Pro: Automatic detection
  - FL Studio: Options → Manage plugins → Find plugins

FEATURES
--------
- 8-band parametric EQ (Bell, Shelf, HP, LP)
- Linear phase mode
- Dynamic EQ per band
- Match EQ (capture & correct)
- Real-time spectrum analyzer
- Mid/Side processing
- Oversampling up to 8x
- Per-band saturation/drive
- Preset management

https://github.com/GareBear99/FreeEQ8
License: GPL-3.0
EOF

# Remove any existing DMG
rm -f "$BUILD_DIR/${DMG_NAME}.dmg"

# Create DMG
echo "Creating DMG..."
hdiutil create \
    -volname "FreeEQ8 v${VERSION}" \
    -srcfolder "$STAGING" \
    -ov \
    -format UDZO \
    "$BUILD_DIR/${DMG_NAME}.dmg"

# Clean up staging
rm -rf "$STAGING"

echo ""
echo "✅ DMG created: $BUILD_DIR/${DMG_NAME}.dmg"
echo "   Size: $(du -h "$BUILD_DIR/${DMG_NAME}.dmg" | cut -f1)"
