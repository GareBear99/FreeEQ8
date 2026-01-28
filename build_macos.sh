#!/usr/bin/env bash
set -euo pipefail

# Build script for macOS (Xcode toolchain).
# Requires: cmake, Xcode command line tools, JUCE either as ./JUCE submodule or -DJUCE_DIR=...

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

echo ""
echo "Build finished."
echo "Look in build/ for the plugin artifacts (VST3/AU)."
