# Manual PDF Generation & Upload

The GitHub Actions workflow is currently blocked by a billing issue. Follow these steps to manually generate and host the PDF.

## Step 1: Generate PDF via Overleaf

1. Go to https://overleaf.com and sign in (or create free account)
2. Click "New Project" → "Blank Project"
3. Name it "DAFx26_FreeEQ8"
4. Delete the default `main.tex` content
5. Copy the entire contents of `paper/DAFx26_FreeEQ8.tex` into the editor
6. Click the green "Recompile" button
7. Download the PDF using the download icon next to the preview

## Step 2: Upload PDF to Repository

### Option A: Via GitHub Web UI
1. Go to https://github.com/GareBear99/FreeEQ8
2. Navigate to the `paper/` directory
3. Click "Add file" → "Upload files"
4. Drag and drop `DAFx26_FreeEQ8.pdf`
5. Commit with message: "docs: add compiled DAFx26 demo paper PDF"

### Option B: Via Command Line
```bash
# After downloading PDF to ~/Downloads/
cp ~/Downloads/DAFx26_FreeEQ8.pdf paper/
git add paper/DAFx26_FreeEQ8.pdf
git commit -m "docs: add compiled DAFx26 demo paper PDF"
git push
```

## Step 3: Enable GitHub Pages (Manual)

1. Go to https://github.com/GareBear99/FreeEQ8/settings/pages
2. Under "Source", select "Deploy from a branch"
3. Select branch: `main`, folder: `/docs`
4. Click "Save"

Then create a simple `docs/index.html` that links to the PDF:
```bash
mkdir -p docs/pdf
cp paper/DAFx26_FreeEQ8.pdf docs/pdf/
git add docs/
git commit -m "docs: setup GitHub Pages with PDF"
git push
```

## Step 4: Access the PDF

After Pages deploys (1-2 minutes), the PDF will be available at:
```
https://garebear99.github.io/FreeEQ8/pdf/DAFx26_FreeEQ8.pdf
```

## Step 5: Submit to DAFx26

1. Go to https://dafx26.mit.edu/call-for-demonstrations/
2. Follow the EasyChair submission link
3. Upload `DAFx26_FreeEQ8.pdf`
4. Fill in author details and demo description
5. **Deadline: May 22, 2026**

## Alternative: arXiv Submission

For permanent archival with DOI:
1. Go to https://arxiv.org/submit
2. Select category: cs.SD (Sound) or eess.AS (Audio and Speech)
3. Upload the PDF
4. Add metadata (title, abstract, authors)
5. Submit for moderation
