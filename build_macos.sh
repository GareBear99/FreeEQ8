#!/usr/bin/env bash
set -euo pipefail

# Build script for macOS (Xcode toolchain).
# Builds both FreeEQ8 (free) and ProEQ8 (commercial) targets.
# Requires: cmake, Xcode command line tools, JUCE either as ./JUCE submodule or -DJUCE_DIR=...

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

mkdir -p build
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.13"

echo "=== Building FreeEQ8 ==="
cmake --build build --config Release --target FreeEQ8_All

echo "=== Building ProEQ8 ==="
cmake --build build --config Release --target ProEQ8_All

echo ""
echo "Build finished."
echo "FreeEQ8: build/FreeEQ8_artefacts/Release/ (VST3/AU)"
echo "ProEQ8:  build/ProEQ8_artefacts/Release/  (VST3/AU)"
