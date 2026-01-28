# FreeEQ8 - Professional 8-Band Parametric EQ Plugin

[![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Windows-lightgrey.svg)]()
[![JUCE](https://img.shields.io/badge/JUCE-7.0.12-orange.svg)](https://juce.com/)

FreeEQ8 is a professional-grade, free, cross-platform 8-band parametric EQ plugin inspired by Ableton's EQ Eight. Built with JUCE for VST3 and AU formats.

## ✨ Features

### Core EQ Functionality
- **8 Independent Bands** with full parametric control
- **5 Filter Types** per band:
  - Bell (Peaking)
  - Low Shelf
  - High Shelf
  - High Pass
  - Low Pass
- **RBJ Biquad Filters** for pristine audio quality
- **Parameter Smoothing** (20ms) to prevent clicks and zipper noise
- **Per-Band Enable/Disable** for A/B comparison

### Advanced Features
- **Output Gain Control** (-24dB to +24dB)
- **Global Scale Parameter** (0.1x to 2x) - scales all band gains simultaneously
- **Adaptive Q Mode** - automatically adjusts Q based on gain (UI ready)
- **State Save/Restore** - all settings persist in your DAW project

### DSP Specifications
- Stereo processing
- Sample rates: 44.1kHz to 192kHz+
- Internal double-precision calculations
- Low CPU usage
- Zero latency

## 🚀 Quick Start

### macOS
```bash
git clone --recursive https://github.com/TheRustySpoon/FreeEQ8.git
cd FreeEQ8
./build_macos.sh
```

### Windows
```powershell
git clone --recursive https://github.com/TheRustySpoon/FreeEQ8.git
cd FreeEQ8
.\build_windows.ps1
```

Plugins will be automatically installed to your system plugin directories.

## 📋 Build Instructions

### Prerequisites

#### macOS
- Xcode Command Line Tools: `xcode-select --install`
- CMake 3.15+: `brew install cmake`

#### Windows
- Visual Studio 2019+ with C++ build tools
- CMake 3.15+

### Detailed Build Steps

#### 1. Clone with JUCE Submodule

```bash
git clone --recursive https://github.com/TheRustySpoon/FreeEQ8.git
cd FreeEQ8
```

If you already cloned without `--recursive`:
```bash
git submodule update --init --recursive
cd JUCE && git checkout 7.0.12 && cd ..
```

#### 2. Build

**macOS:**
```bash
chmod +x build_macos.sh
./build_macos.sh
```

**Windows:**
```powershell
.\build_windows.ps1
```

#### 3. Plugin Installation

**macOS (automatic):**
- VST3: `~/Library/Audio/Plug-Ins/VST3/FreeEQ8.vst3`
- AU: `~/Library/Audio/Plug-Ins/Components/FreeEQ8.component`

**Windows (manual):**
- Copy `build\FreeEQ8_artefacts\Release\VST3\FreeEQ8.vst3` to:
  - `C:\Program Files\Common Files\VST3\`

#### 4. Rescan in Your DAW
- **Ableton Live**: Preferences → Plug-ins → Rescan
- **Logic Pro**: Automatic detection
- **FL Studio**: Options → Manage plugins → Find plugins

## 🎛️ Usage Guide

### Parameter Ranges
| Parameter | Range | Scale | Description |
|-----------|-------|-------|-------------|
| Frequency | 20 Hz - 20 kHz | Logarithmic | Center/cutoff frequency |
| Q | 0.1 - 24 | Logarithmic | Bandwidth (0.1=wide, 24=narrow) |
| Gain | -24 dB to +24 dB | Linear | Boost/cut amount |
| Output | -24 dB to +24 dB | Linear | Master output level |
| Scale | 0.1x - 2x | Linear | Global gain multiplier |

### Common EQ Techniques

#### Surgical EQ (Problem Frequency Removal)
```
Band: Bell filter
Q: 6-12 (narrow)
Gain: -6 to -12 dB
```

#### Musical EQ (Broad Tonal Shaping)
```
Band: Bell/Shelf filter
Q: 0.5-2 (wide)
Gain: ±3 to ±6 dB
```

#### High-Pass Filtering
```
Band: HighPass filter
Freq: 20-120 Hz (depends on source)
Q: 0.7 (standard)
```

### Example Settings

**Kick Drum:**
- Band 1: Bell @ 60Hz, Q=1.5, +4dB (sub thump)
- Band 2: Bell @ 200Hz, Q=3, -3dB (cardboard removal)
- Band 3: Bell @ 3kHz, Q=2, +2dB (beater click)

**Acoustic Guitar:**
- Band 1: HighPass @ 80Hz (rumble removal)
- Band 2: Bell @ 200Hz, Q=1.5, -2dB (boominess)
- Band 3: Bell @ 3kHz, Q=1, +3dB (presence)
- Band 4: HighShelf @ 8kHz, +2dB (air)

**Vocals:**
- Band 1: HighPass @ 80Hz (rumble)
- Band 2: Bell @ 250Hz, Q=2, -3dB (muddiness)
- Band 3: Bell @ 1kHz, Q=1, +2dB (body)
- Band 4: Bell @ 5kHz, Q=2, +3dB (clarity)

## 🔧 Technical Details

### Architecture
```
┌─────────────────────────────────────┐
│     FreeEQ8 Audio Processor         │
├─────────────────────────────────────┤
│  Input Buffer (Stereo)              │
│          ↓                          │
│  ┌─────────────────────────────┐   │
│  │   Band 1 (Biquad Filter)    │   │
│  │   Band 2 (Biquad Filter)    │   │
│  │   Band 3 (Biquad Filter)    │   │
│  │   Band 4 (Biquad Filter)    │   │
│  │   Band 5 (Biquad Filter)    │   │
│  │   Band 6 (Biquad Filter)    │   │
│  │   Band 7 (Biquad Filter)    │   │
│  │   Band 8 (Biquad Filter)    │   │
│  └─────────────────────────────┘   │
│          ↓                          │
│  Output Gain & Scaling              │
│          ↓                          │
│  Output Buffer (Stereo)             │
└─────────────────────────────────────┘
```

### DSP Implementation
- **Filter Structure**: Direct Form I biquad
- **Coefficient Calculation**: RBJ Audio EQ Cookbook
- **Smoothing**: Linear interpolation over 20ms
- **Update Rate**: Coefficients refreshed every 16 samples during smoothing
- **Precision**: Double-precision (64-bit) internal calculations

### Project Structure
```
FreeEQ8/
├── Source/
│   ├── PluginProcessor.h          # Main audio processor
│   ├── PluginProcessor.cpp        # Audio processing logic
│   ├── PluginEditor.h             # UI header
│   ├── PluginEditor.cpp           # UI implementation
│   └── DSP/
│       ├── Biquad.h               # Biquad filter implementation
│       └── EQBand.h               # EQ band with smoothing
├── JUCE/                          # JUCE framework (submodule)
├── build/                         # Build output (ignored)
├── CMakeLists.txt                 # CMake configuration
├── build_macos.sh                 # macOS build script
├── build_windows.ps1              # Windows build script
├── .gitignore                     # Git ignore rules
└── README.md                      # This file
```

## 🛣️ Roadmap

### v0.4.0 (Next Release)
- [ ] Real-time spectrum analyzer
- [ ] Interactive frequency response curve display
- [ ] Draggable band nodes on curve
- [ ] Adaptive Q implementation (currently UI-only)
- [ ] Band solo/audition mode
- [ ] Preset management system

### v0.5.0 (Future)
- [ ] Multiple filter slopes (12/24/48 dB/oct)
- [ ] Mid/Side processing mode
- [ ] L/R independent processing
- [ ] Oversampling options (2x, 4x, 8x)
- [ ] Output metering with peak hold
- [ ] Resizable UI

### v1.0.0 (Long-term)
- [ ] Linear phase mode
- [ ] Dynamic EQ capabilities
- [ ] Band linking
- [ ] Per-band saturation/drive
- [ ] Undo/Redo system
- [ ] Match EQ functionality

## 🤝 Contributing

Contributions are welcome! Here's how you can help:

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/amazing-feature`
3. **Commit your changes**: `git commit -m 'Add amazing feature'`
4. **Push to the branch**: `git push origin feature/amazing-feature`
5. **Open a Pull Request**

### Development Guidelines
- Follow existing code style
- Add comments for complex DSP algorithms
- Test on both macOS and Windows if possible
- Update documentation for new features

### Areas for Contribution
- 🎨 UI/UX improvements
- 📊 Spectrum analyzer implementation
- 🔊 Additional filter types
- 🐛 Bug fixes and optimizations
- 📚 Documentation improvements
- 🧪 Unit tests

## 📝 Changelog

### v0.3.0 (2026-01-28)
- ✅ Added output gain control (-24dB to +24dB)
- ✅ Added global scale parameter (0.1x to 2x)
- ✅ Added adaptive Q toggle (UI only, DSP pending)
- ✅ Enhanced UI layout with global controls
- ✅ Fixed JUCE 7.0.12 compatibility issues
- ✅ Fixed VST3 build on macOS with Xcode 12
- ✅ Improved parameter smoothing
- ✅ Updated build scripts for reliability

### v0.2.0
- 8-band parametric EQ
- RBJ biquad filters (Bell, Shelf, HP, LP)
- Parameter smoothing (20ms)
- State save/restore via APVTS
- CMake build system for VST3/AU

### v0.1.0
- Initial prototype

## 📄 License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

**Note:** JUCE has its own licensing requirements. For commercial use, you may need a JUCE license. See [JUCE Licensing](https://juce.com/discover/licensing) for details.

## ⚠️ Legal Notice

FreeEQ8 is an **original implementation** of a parametric EQ plugin. It is:
- **NOT** affiliated with, endorsed by, or derived from Ableton AG
- **NOT** a clone of Ableton's EQ Eight
- An independent, open-source project
- Built using public-domain DSP algorithms (RBJ Audio EQ Cookbook)

## 🐛 Known Issues

- Adaptive Q toggle is non-functional (implementation pending)
- No visual frequency response display
- UI is not resizable
- No preset management yet
- No spectrum analyzer yet

Report issues at: https://github.com/TheRustySpoon/FreeEQ8/issues

## 💡 Tips & Tricks

### Performance Optimization
- Disable unused bands to reduce CPU load
- Use wider Q values (lower numbers) for smoother processing
- Enable adaptive Q for automatic gain-dependent Q adjustment

### Mixing Workflow
1. Start with subtractive EQ (cut problem frequencies)
2. Use narrow Q to identify resonances
3. Use wide Q for musical boosts
4. Check your EQ in mono to avoid phase issues

### Sound Design
- Stack multiple bell filters at the same frequency with different Q values
- Automate the scale parameter for dramatic filter sweeps
- Use extreme Q values (>10) for creative resonances

## 🙏 Acknowledgments

- **JUCE Framework** - Cross-platform audio plugin framework
- **Robert Bristow-Johnson** - RBJ Audio EQ Cookbook
- **Audio Plugin Development Community** - For knowledge sharing
- **Ableton** - For inspiration (not affiliation)

## 📧 Support

- **Issues**: [GitHub Issues](https://github.com/TheRustySpoon/FreeEQ8/issues)
- **Discussions**: [GitHub Discussions](https://github.com/TheRustySpoon/FreeEQ8/discussions)
- **Email**: therustyspoon@users.noreply.github.com

## 🌟 Star History

If you find FreeEQ8 useful, please star the repository to show your support!

---

**Built with ❤️ by TheRustySpoon**

*"Great sound shouldn't cost anything"*
