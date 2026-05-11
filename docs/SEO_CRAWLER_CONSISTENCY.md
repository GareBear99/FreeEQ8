# FreeEQ8 SEO & Crawler Consistency Map

This document exists to keep public-facing metadata consistent across GitHub, search crawlers, package indexes, curated audio-plugin lists, and release pages.

## Canonical identity

| Field | Canonical value |
|---|---|
| Project name | FreeEQ8 |
| Repository | `GareBear99/FreeEQ8` |
| Maintainer | Gary Doman / GareBear99 / TizWildinEntertainment |
| Product category | Free parametric EQ audio plugin |
| Core formats | VST3, AU on macOS, Standalone target |
| Source license | GPL-3.0 |
| Current stable source release | v2.2.0 |
| Development lane | v2.3.0-dev Smart EQ Layer |
| Optional commercial build | ProEQ8 |

## Recommended GitHub repository description

FreeEQ8 is a free GPL-3.0 8-band parametric EQ plugin for macOS, Windows, and Linux. Built with JUCE for VST3, AU, and standalone targets; includes linear-phase mode, dynamic EQ controls, match EQ, M/S processing, oversampling, per-band drive, band linking, and a real-time spectrum analyzer.

## Recommended GitHub topics

`audio-plugin`, `vst3`, `au-plugin`, `juce`, `equalizer`, `parametric-eq`, `dynamic-eq`, `linear-phase`, `match-eq`, `music-production`, `dsp`, `cpp`, `cmake`, `open-source`, `free-plugin`

## Crawler-safe product summary

FreeEQ8 is a free, open-source 8-band parametric EQ plugin built with JUCE. It focuses on a practical mixing/mastering workflow with dynamic EQ controls, linear-phase mode, match EQ, per-band drive, M/S processing, oversampling, band linking, and a live spectrum analyzer.

## Claims to avoid

Avoid wording that can look contradictory, unverifiable, or spam-like to crawlers and list maintainers:

- “best EQ”
- “only free EQ”
- “most advanced EQ”
- “FabFilter killer”
- “Pro-Q replacement”
- “competes directly with every paid EQ”
- “guaranteed professional results”

Use evidence-based wording instead:

- “free GPL-3.0 8-band parametric EQ”
- “JUCE-based VST3/AU/standalone target”
- “includes linear-phase mode, dynamic EQ controls, match EQ, per-band drive, M/S, oversampling, and analyzer”
- “optional ProEQ8 commercial build from the same repository”

## Version consistency rules

- `CMakeLists.txt` project version, `Source/Config.h`, README stable release, and `CHANGELOG.md` should match for the plugin source release.
- The license server can have a separate API version, but it must be named clearly as the server/API version.
- Current plugin source version: `2.2.0`.
- Current license server API version: `2.0.0`.

## ProEQ8 wording rule

ProEQ8 should be described as an optional commercial build/activation path from the same repository. Do not let crawler-facing text imply that FreeEQ8 itself is paid, closed-source, or unavailable.

## Cross-linking rule

Keep the main README focused on FreeEQ8 first. Ecosystem links, sample packs, music links, and other projects are useful, but should remain lower in the README so crawlers identify the repo as an audio EQ plugin before they see the broader ecosystem.
