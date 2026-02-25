# Contributing to FreeEQ8

Thank you for your interest in contributing to FreeEQ8! This document provides guidelines for contributing to the project.

## Code of Conduct

- Be respectful and inclusive
- Provide constructive feedback
- Focus on what is best for the community
- Show empathy towards others

## How to Contribute

### Reporting Bugs

Before creating a bug report, please check existing issues to avoid duplicates.

**When reporting a bug, include:**
- FreeEQ8 version
- Operating system and version
- DAW and version
- Steps to reproduce the bug
- Expected vs actual behavior
- Screenshots/audio examples if applicable

### Suggesting Features

We welcome feature suggestions! Please:
- Check if the feature has already been suggested
- Provide a clear description of the feature
- Explain the use case and benefits
- Consider implementation complexity

### Pull Requests

1. **Fork the repository**
   ```bash
   git clone https://github.com/GareBear99/FreeEQ8.git
   cd FreeEQ8
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes**
   - Follow the existing code style
   - Add comments for complex code
   - Test thoroughly on your platform

3. **Commit your changes**
   ```bash
   git add .
   git commit -m "Add feature: brief description"
   ```
   
   Commit message format:
   - `Add feature: description` for new features
   - `Fix: description` for bug fixes
   - `Update: description` for updates
   - `Refactor: description` for code refactoring
   - `Docs: description` for documentation changes

4. **Push to your fork**
   ```bash
   git push origin feature/your-feature-name
   ```

5. **Create a Pull Request**
   - Provide a clear title and description
   - Reference any related issues
   - Include screenshots/demos if applicable

## Development Setup

### Prerequisites
- JUCE 7.0.12 (included as submodule)
- CMake 3.15+
- C++17 compiler (Xcode/GCC/MSVC)

### Building for Development

```bash
# Clone with submodules
git clone --recursive https://github.com/GareBear99/FreeEQ8.git
cd FreeEQ8

# macOS
./build_macos.sh

# Windows
.\build_windows.ps1
```

## Code Style Guidelines

### C++ Style
- Use 4 spaces for indentation (no tabs)
- Opening braces on the same line for functions/classes
- Use descriptive variable names
- Prefer `const` where applicable
- Use modern C++17 features

### Example:
```cpp
void MyClass::processAudio(float* buffer, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = buffer[i];
        // Process sample...
        buffer[i] = sample;
    }
}
```

### Comments
- Use `//` for single-line comments
- Use `/* */` for multi-line comments
- Document complex DSP algorithms
- Explain "why" not "what"

### File Organization
```
Source/
├── PluginProcessor.h      # Audio processor interface
├── PluginProcessor.cpp    # Audio processing implementation
├── PluginEditor.h         # UI interface
├── PluginEditor.cpp       # UI implementation
└── DSP/
    ├── Biquad.h           # Filter implementations
    └── EQBand.h           # Band logic
```

## Testing

Before submitting:
1. Build on your platform without warnings
2. Test in at least one DAW
3. Verify parameter automation works
4. Test state save/restore
5. Check for audio glitches/clicks
6. Test extreme parameter values

### Manual Testing Checklist
- [ ] Plugin loads in DAW
- [ ] All parameters respond correctly
- [ ] No audio artifacts (clicks, pops)
- [ ] CPU usage is reasonable
- [ ] State saves/restores properly
- [ ] UI updates reflect parameter changes
- [ ] No crashes or hangs

## Priority Areas for Contribution

### High Priority
- 🎨 Spectrum analyzer implementation
- 📊 Visual frequency response curve
- 🎛️ Draggable band nodes
- 🔊 Adaptive Q algorithm
- 💾 Preset management

### Medium Priority
- 🎚️ Multiple filter slopes
- 🔀 Mid/Side processing
- ⚡ Oversampling
- 📏 Output metering
- 🖼️ Resizable UI

### Low Priority (Nice to Have)
- 🧪 Unit tests
- 📱 Standalone app version
- 🎨 Custom look and feel
- 🌐 Internationalization
- ♿ Accessibility improvements

## Documentation

When adding features:
- Update README.md if user-facing
- Add inline comments for complex code
- Update CHANGELOG.md
- Consider adding usage examples

## Questions?

- Open a [Discussion](https://github.com/GareBear99/FreeEQ8/discussions)
- Check existing [Issues](https://github.com/GareBear99/FreeEQ8/issues)
- Email: GareBear99@users.noreply.github.com

## License

By contributing, you agree that your contributions will be licensed under the GPL-3.0 License.

---

Thank you for contributing to FreeEQ8! 🎵
