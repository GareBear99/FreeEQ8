# DAFx26 Demo Paper: FreeEQ8/ProEQ8

## Paper Title
**Real-Time State-Space Parameterization in Digital Equalization: A Production-Grade SVF Implementation**

## Submission Target
- **Conference**: DAFx26 (29th International Conference on Digital Audio Effects)
- **Track**: Demonstrations
- **Deadline**: May 22, 2026
- **Venue**: MIT, Cambridge, MA (September 1-4, 2026)
- **Submission Portal**: https://dafx26.mit.edu/call-for-demonstrations/

## Files
- `DAFx26_FreeEQ8.tex` — LaTeX source (4-page demo paper)
- `build.sh` — Build script for local compilation

## Building the PDF

### Option 1: Overleaf (Recommended)
1. Go to https://overleaf.com
2. Create a new blank project
3. Upload `DAFx26_FreeEQ8.tex`
4. Click "Recompile"
5. Download the PDF

### Option 2: Local Compilation (macOS)
```bash
# Install BasicTeX (smaller) or MacTeX (full)
brew install --cask basictex

# Update PATH (may require new terminal)
export PATH="/Library/TeX/texbin:$PATH"

# Install required packages
sudo tlmgr update --self
sudo tlmgr install booktabs listings hyperref

# Build
./build.sh
```

### Option 3: Local Compilation (Ubuntu/Debian)
```bash
sudo apt install texlive-latex-recommended texlive-fonts-recommended texlive-latex-extra
./build.sh
```

## Demonstration Plan
The live demo at DAFx26 will showcase:
1. Side-by-side RBJ vs SVF frequency response at 16 kHz
2. Real-time spectrum analyzer with lock-free triple-buffer rendering
3. Dynamic EQ envelope tracking on drum transients
4. 24-band surgical EQ workflow in ProEQ8

## Supporting Materials
- Source code: https://github.com/GareBear99/FreeEQ8
- Full technical paper: `PAPER.md` in repository root
- Benchmark data: `Tests/response_data.csv`
- CI validation: `pluginval` at strictness-level-10

## Published Outreach

- **dev.to announcement**: [FreeEQ8 Architecture (note: original cramping claims were incorrect — see PAPER.md)](https://dev.to/tizwildin/we-eliminated-eq-frequency-cramping-without-oversampling-heres-how-dafx26-paper-4f7l)
- **GitHub Pages PDF**: https://garebear99.github.io/FreeEQ8/pdf/DAFx26_FreeEQ8.pdf
- **Landing page**: https://garebear99.github.io/FreeEQ8/

## Author
Gary Doman (GareBear99 / TizWildin)
