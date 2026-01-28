# Build script for Windows (Visual Studio generator via CMake)
    # Requires: CMake, Visual Studio Build Tools, JUCE as ./JUCE submodule OR set JUCE_DIR env var
    $ErrorActionPreference = "Stop"
    $root = Split-Path -Parent $MyInvocation.MyCommand.Path
    Set-Location $root

    if (!(Test-Path "build")) { New-Item -ItemType Directory -Path "build" | Out-Null }

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    Write-Host "`nBuild finished. Look in build/ for VST3 output."
