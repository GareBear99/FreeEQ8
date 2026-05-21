# FreeEQ8 submission tracker — 2026 index campaign

_Last cross-checked: 2026-05-21_

This file tracks public submission routes for FreeEQ8 so PRs, directory listings, editorial pitches, product launches, forum posts, and SEO backlinks do not duplicate work.

This tracker is now based on the cross-check file at [`docs/outreach/CROSSCHECKED_SUBMISSION_STATUS_2026-05-21.md`](outreach/CROSSCHECKED_SUBMISSION_STATUS_2026-05-21.md). Use that file as the source of truth when deciding whether something is actually listed, merely submitted, pending, or off-target.

## Audit-backed positioning snapshot

FreeEQ8 currently presents a public JUCE/C++ audio plugin package with:

- VST3 / AU / Standalone targets for FreeEQ8, plus ProEQ8 commercial target scaffolding.
- 8-band parametric EQ positioning for FreeEQ8.
- Linear phase engine, dynamic EQ controls, match EQ, mid/side processing, oversampling, per-band drive/saturation, spectrum FIFO/analyzer, presets, level meter, update checker, and release workflow files.
- macOS / Linux / Windows build packaging scripts or workflows.
- Public release docs, milestone report, tester call, test matrix, outreach templates, screenshots, Ableton screenshots, and featured/submission trackers.
- Standalone regression evidence: `Tests/AuditRegressionTest.cpp` and `Tests/BiquadTest.cpp` compile and pass outside the JUCE build when tested locally from this package.

## Current indexable SEO phrase set

Use these phrases across titles, tags, release copy, directory descriptions, and article headings:

- FreeEQ8
- free EQ plugin
- free parametric EQ plugin
- free VST3 EQ
- free AU EQ plugin
- open-source EQ plugin
- JUCE EQ plugin
- C++ audio plugin
- free FabFilter Pro-Q alternative
- free TDR Nova alternative
- dynamic EQ plugin
- linear phase EQ plugin
- match EQ plugin
- mid/side EQ plugin
- spectrum analyzer EQ
- free mixing plugin
- free mastering EQ
- audio DSP open source
- TizWildin Plugin Ecosystem
- Gary Doman FreeEQ8
- GareBear99 FreeEQ8

## Current status snapshot

| Status | Count | Meaning |
|---|---:|---|
| Verified third-party public wins | 3 | Rekkerd coverage, OpenAudio listing, awesome-music-production listing. |
| Self-maintained listing | 1 | Useful SEO, not third-party validation. |
| Completed/closed evidence needing public-page recheck | 4 | Some GitHub/email threads closed as completed or relevant, but current public page did not prove live listing. |
| Active/open PRs and issues | 8+ | Waiting for maintainer review or response. |
| Direct email submissions sent | 8+ | Outreach sent; no confirmed coverage reply found except Rekkerd and KVR routing. |
| Manual/account targets | 6+ | Highest-value places needing direct account/form action. |
| Off-target technical outreach | 2 | iPlug2 and DISTRHO/DPF are not current fits for a JUCE plugin. |

## Verified public wins — safe to cite

| Target | Status | Evidence |
|---|---:|---|
| Rekkerd.org | ✅ Featured | Live article: `FREE: FreeEQ8 parametric EQ effect plugin by Gary Doman`, published 2026-05-19. |
| webprofusion / OpenAudio | ✅ Listed | Current public OpenAudio page shows `FreeEQ8` in Audio Plugins; Issue #207 is closed. |
| ad-si / awesome-music-production | ✅ Listed | PR #197 merged 2026-03-16; current public README shows `FreeEQ8` under Apps. |
| GareBear99 / awesome-audio-plugins-dev | ✅ Self-maintained listing | Useful index route; not third-party validation. |

## Completed/closed evidence — do not overclaim as live listing yet

| Target | Conservative status | Evidence / reason |
|---|---:|---|
| BillyDM / awesome-audio-dsp | ⚠️ Completed issue evidence only | Gmail says Issue #14 closed as completed, but public README check did not show `FreeEQ8`. |
| sudara / awesome-juce | ⚠️ Mixed history / not verified live | Earlier issues closed as completed, later PR #61 required shorter resubmission, and public README check did not show `FreeEQ8`. |
| notthetup / awesome-webaudio | ⚠️ Conflicting evidence / weak fit | Gmail said Issue #83 closed completed, but public issue page checked open and README did not show `FreeEQ8`. |
| brandonhimpfen / awesome-audio-engineering | ⚠️ Closed PR, not verified live | Gmail says PR #5 closed; closure alone is not proof of listing. |

## Active PRs / issues

| Target | Type | Status | Link | Follow-up rule |
|---|---|---:|---|---|
| sudara / awesome-juce | PR/resubmission route | ⏳ Pending | https://github.com/sudara/awesome-juce/pull/64 | Wait; do not claim listed until README shows FreeEQ8. |
| dreikanter / awesome-vst | PR | ⏳ Open | https://github.com/dreikanter/awesome-vst/pull/18 | Wait. |
| olilarkin / awesome-musicdsp | PR | ⏳ Open | https://github.com/olilarkin/awesome-musicdsp/pull/11 | Wait. |
| noteflakes / awesome-music | PR + issue | ⏳ Open | https://github.com/noteflakes/awesome-music/pull/109 | Wait. |
| twinysam / FreeAudioPluginList | PR | ⏳ Open | https://github.com/twinysam/FreeAudioPluginList/pull/11 | Wait. |
| nodiscc / awesome-linuxaudio | PR | ⏳ Open | https://github.com/nodiscc/awesome-linuxaudio/pull/71 | Wait; keep Linux build status honest. |
| landscape82 / awesome-sound-design-resources | PR | ⏳ Open | https://github.com/landscape82/awesome-sound-design-resources/pull/3 | Wait. |
| detroitsynth / awesome-open-synth | PR | ⏳ Open / weak fit | https://github.com/detroitsynth/awesome-open-synth/pull/3 | Wait; do not push. |

## Direct email submissions sent

These were sent as direct outreach. Do not resend unless there is a real update, release build, packaged installer, demo video, benchmark/test report, or 10–14 day follow-up window has passed.

| Target | Status | Recipient / route | Sent date | Follow-up rule |
|---|---:|---|---|---|
| Rekkerd | ✅ Covered | `ronnie@rekkerd.org` | 2026-05-16 | Follow up only for major version, ProEQ8, or new demo video. |
| KVR Audio | 🟡 Routing reply | `contactus@kvraudio.com` | 2026-03-25 and 2026-05-16 | Create developer account and product listing; email alone is not enough. |
| Bedroom Producers Blog | ⏳ Sent | `tomislav@bedroomproducersblog.com` | 2026-03-26, 2026-05-05, 2026-05-16 | Send one refined follow-up with Rekkerd/OpenAudio/awesome-music-production proof + demo assets. |
| ProducersBuzz | ⏳ Sent | `submit@producersbuzz.com` | 2026-05-12 | Wait / follow once after release polish. |
| ProducerSpot | ⏳ Sent | `info@producerspot.com` | 2026-05-16 | Wait. |
| Home Music Maker | ⏳ Sent | `info@homemusicmaker.com` | 2026-05-16 | Wait. |
| Gearnews | ⏳ Sent | `news@gearnews.com` | 2026-05-16 | Wait. |
| MusicTech / NME / Guitar.com | ⏳ Sent | editorial addresses | 2026-05-05 | Low priority unless story becomes broader. |
| Sound On Sound | ⏳ Sent | SOS editorial contacts | 2026-05-05 | Follow only with stable installer/demo. |
| Tutorials Dojo | ⏳ Sent | `support@tutorialsdojo.com` | 2026-05-16 | Better for edge/local audio AI article angle. |

## Immediate manual/account targets — do next

| Target | Priority | Action | Recommended indexing angle | Required assets before submitting |
|---|---:|---|---|---|
| KVR Audio Product Database | A+ | Create Developer Account; add FreeEQ8 product; submit news item | `FreeEQ8 — free open-source JUCE/C++ parametric EQ plugin, VST3/AU/Standalone` | Logo/screenshot, release URL, platform list, version, short/long descriptions. |
| Audio Plugins for Free | A | Upload / submit plugin | `Free VST/AU EQ plugin for mixing and mastering` | Direct download/release link, screenshot, OS/format list. |
| Plugins4Free | A | Submit/list plugin if route is active | `Free EQ effect plugin for Cubase, FL Studio, REAPER, Ableton and VST/AU hosts` | Stable download link and install notes. |
| AlternativeTo | A | Suggest new application | `Free and open-source alternative to FabFilter Pro-Q, TDR Nova, ZL Equalizer` | Platforms, GPL-3.0 license, tags, description, screenshot. |
| Product Hunt | A- | Schedule launch | `Free open-source EQ plugin for producers and DSP developers` | Hero image, screenshots, launch comment, maker profile, demo GIF/video. |
| Hacker News / Show HN | A- | Submit only when a major release page/demo is clean | `Show HN: FreeEQ8 – a free open-source JUCE/C++ EQ plugin` | GitHub repo, concise technical comment, no vote requests. |
| REAPER Forum / r/Reaper | A | Forum post | `Free open-source EQ plugin tested in REAPER; looking for host feedback` | REAPER screenshot, VST3 install notes. |
| KVR Forum thread | A | Developer/product thread after product listing | `FreeEQ8 — open-source EQ with linear phase, dynamic EQ, match EQ, M/S, analyzer` | KVR product page first. |
| LinuxMusicians | B+ | Post after Linux package/build path is clean | `Open-source JUCE EQ with Linux VST3 build path` | Honest Linux status and build instructions. |

## Technical outreach status

| Target | Status | Correct interpretation |
|---|---:|---|
| JUCE Forum/support | 🟡 Relevant but narrow asks only | Forum is relevant, but future asks should be specific and not broad “audit my repo” requests. |
| Tracktion/pluginval | ⏳ Relevant validation route | Keep focused on plugin validation and host compatibility. |
| Chowdhury-DSP/BYOD | 🟡 Paid expert review offered | Jatin offered review at $85/hour; useful optional expert feedback, not listing/coverage. |
| DISTRHO/DPF | ❌ Off-target | FreeEQ8 is JUCE, not DPF; avoid unless there is a DPF/LV2 port. |
| iPlug2 | ❌ Off-target | Maintainer said it does not seem iPlug2-relevant; avoid unless there is an iPlug2 port/comparison. |

## Best current truth statement for future submissions

> FreeEQ8 is a free, open-source JUCE/C++ 8-band parametric EQ plugin with VST3/AU/Standalone packaging, advanced EQ features, public source code, confirmed third-party coverage from Rekkerd.org, and verified public listings on OpenAudio and awesome-music-production. KVR product-database indexing is the next major step.

## Next actions

1. Create / complete KVR Developer Account product listing.
2. Add Rekkerd/OpenAudio/awesome-music-production proof to follow-up pitches.
3. Follow up with Bedroom Producers Blog using Rekkerd coverage as credibility proof.
4. Submit to Audio Plugins for Free, Plugins4Free, AlternativeTo, and Product Hunt only after release/demo assets are clean.
5. Keep framework-specific outreach targeted only to frameworks FreeEQ8 actually uses.
