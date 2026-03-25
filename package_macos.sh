#!/usr/bin/env bash
set -euo pipefail

# Package FreeEQ8 + ProEQ8 VST3/AU into a macOS .dmg installer.
# Usage: ./package_macos.sh [build_dir]
#   build_dir defaults to ./build

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-$ROOT/build}"
FREE_ARTEFACTS="$BUILD_DIR/FreeEQ8_artefacts/Release"
PRO_ARTEFACTS="$BUILD_DIR/ProEQ8_artefacts/Release"
VERSION=$(grep 'project(FreeEQ8' "$ROOT/CMakeLists.txt" | sed 's/.*VERSION \([0-9.]*\).*/\1/')
DMG_NAME="FreeEQ8-v${VERSION}-macOS"
STAGING="$BUILD_DIR/dmg_staging"

echo "=== Packaging FreeEQ8 + ProEQ8 v${VERSION} for macOS ==="

# Verify artefacts exist
FREE_VST3="$FREE_ARTEFACTS/VST3/FreeEQ8.vst3"
FREE_AU="$FREE_ARTEFACTS/AU/FreeEQ8.component"
PRO_VST3="$PRO_ARTEFACTS/VST3/ProEQ8.vst3"
PRO_AU="$PRO_ARTEFACTS/AU/ProEQ8.component"

if [ ! -d "$FREE_VST3" ]; then
    echo "ERROR: FreeEQ8 VST3 not found at $FREE_VST3"
    echo "Run ./build_macos.sh first."
    exit 1
fi

# Clean staging area
rm -rf "$STAGING"
mkdir -p "$STAGING/FreeEQ8"
mkdir -p "$STAGING/ProEQ8"

# Copy FreeEQ8 plugins
cp -R "$FREE_VST3" "$STAGING/FreeEQ8/"
if [ -d "$FREE_AU" ]; then
    cp -R "$FREE_AU" "$STAGING/FreeEQ8/"
fi

# Copy FreeEQ8 Standalone app (if built)
FREE_APP="$FREE_ARTEFACTS/Standalone/FreeEQ8.app"
if [ -d "$FREE_APP" ]; then
    cp -R "$FREE_APP" "$STAGING/FreeEQ8/"
    echo "  + FreeEQ8 Standalone app included"
fi

# Copy ProEQ8 plugins (if built)
if [ -d "$PRO_VST3" ]; then
    cp -R "$PRO_VST3" "$STAGING/ProEQ8/"
    if [ -d "$PRO_AU" ]; then
        cp -R "$PRO_AU" "$STAGING/ProEQ8/"
    fi
fi

# Create a README for the DMG
cat > "$STAGING/README.txt" << EOF
FreeEQ8 v${VERSION} + ProEQ8 v${VERSION}
============================================

This DMG contains two plugins:

  FreeEQ8/  —  Free 8-band parametric EQ (GPL-3.0)
  ProEQ8/   —  Commercial 24-band parametric EQ (requires license)

INSTALLATION
------------
VST3:
  Drag the .vst3 bundles to:
    ~/Library/Audio/Plug-Ins/VST3/

AU (Audio Unit):
  Drag the .component bundles to:
    ~/Library/Audio/Plug-Ins/Components/

Then rescan plugins in your DAW:
  - Ableton Live: Preferences → Plug-ins → Rescan
  - Logic Pro: Automatic detection
  - FL Studio: Options → Manage plugins → Find plugins

FREEEQ8 FEATURES
----------------
- 8-band parametric EQ (Bell, Shelf, HP, LP, Bandpass)
- Linear phase mode (2048-sample FIR convolution)
- Dynamic EQ per band (threshold/ratio/attack/release)
- Match EQ (capture & correct)
- Real-time spectrum analyzer (4096-pt FFT)
- Mid/Side processing with per-band routing
- Oversampling up to 8x
- Per-band saturation/drive (tanh)
- 30 factory presets
- Band linking (groups A/B)

PROEQ8 ADDITIONAL FEATURES
--------------------------
- 24 fully independent bands
- 4 saturation modes (Tanh/Tube/Tape/Transistor)
- A/B comparison with snapshot toggle
- Auto-gain bypass (RMS-matched)
- Piano roll overlay (C1-C8)
- Collision detection warnings

https://github.com/GareBear99/FreeEQ8
License: GPL-3.0 (FreeEQ8), Commercial (ProEQ8)
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
