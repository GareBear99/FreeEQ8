#!/usr/bin/env bash
set -euo pipefail

# Build script for Linux.
# Requires: cmake, g++ (or clang++), and several JUCE dependencies.
# This script will attempt to install missing packages via apt if available.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

# ── Install dependencies (Debian/Ubuntu) ──────────────────────────────────────
if command -v apt-get &>/dev/null; then
    echo "Checking JUCE build dependencies (may prompt for sudo)..."
    sudo apt-get update -qq
    sudo apt-get install -y --no-install-recommends \
        build-essential cmake pkg-config \
        libasound2-dev libjack-jackd2-dev \
        libfreetype6-dev libx11-dev libxcomposite-dev \
        libxcursor-dev libxext-dev libxfixes-dev \
        libxinerama-dev libxrandr-dev libxrender-dev \
        libwebkit2gtk-4.0-dev libcurl4-openssl-dev \
        ladspa-sdk
fi

mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j"$(nproc)"

echo ""
echo "Build finished."
echo "VST3 plugin: build/FreeEQ8_artefacts/Release/VST3/FreeEQ8.vst3"
echo ""
echo "To install system-wide:"
echo "  cp -r build/FreeEQ8_artefacts/Release/VST3/FreeEQ8.vst3 ~/.vst3/"
