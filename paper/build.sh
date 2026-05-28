#!/bin/bash
# Build DAFx26 demo paper PDF
# Requires: pdflatex (MacTeX, TeX Live, or MiKTeX)

set -e

cd "$(dirname "$0")"

echo "=== Building DAFx26_FreeEQ8.pdf ==="

# Check for pdflatex
if ! command -v pdflatex &> /dev/null; then
    echo "ERROR: pdflatex not found."
    echo ""
    echo "Install LaTeX:"
    echo "  macOS:   brew install --cask mactex-no-gui"
    echo "  Ubuntu:  sudo apt install texlive-latex-recommended texlive-fonts-recommended texlive-latex-extra"
    echo "  Windows: Install MiKTeX from https://miktex.org/"
    echo ""
    echo "Alternatively, use Overleaf (https://overleaf.com):"
    echo "  1. Create new project"
    echo "  2. Upload DAFx26_FreeEQ8.tex"
    echo "  3. Click 'Recompile'"
    exit 1
fi

# Clean old build artifacts
rm -f DAFx26_FreeEQ8.aux DAFx26_FreeEQ8.log DAFx26_FreeEQ8.out DAFx26_FreeEQ8.bbl DAFx26_FreeEQ8.blg

# First pass
pdflatex -interaction=nonstopmode DAFx26_FreeEQ8.tex

# Second pass (for references)
pdflatex -interaction=nonstopmode DAFx26_FreeEQ8.tex

# Clean intermediate files
rm -f DAFx26_FreeEQ8.aux DAFx26_FreeEQ8.log DAFx26_FreeEQ8.out

echo ""
echo "=== SUCCESS ==="
echo "Output: paper/DAFx26_FreeEQ8.pdf"
echo ""
echo "Next steps:"
echo "  1. Review the PDF"
echo "  2. Submit to DAFx26 Demo Track: https://dafx26.mit.edu/call-for-demonstrations/"
echo "  3. Deadline: May 22, 2026"
