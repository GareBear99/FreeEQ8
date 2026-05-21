# FreeEQ8 submission tracker — 2026 index campaign

This file tracks public submission routes for FreeEQ8 so PRs, directory listings, editorial pitches, product launches, forum posts, and SEO backlinks do not duplicate work.

_Last updated: 2026-05-21_

## Audit-backed positioning snapshot

FreeEQ8 is now strong enough to submit beyond small GitHub lists. The repo currently presents a public JUCE/C++ audio plugin package with:

- VST3 / AU / Standalone targets for FreeEQ8, plus ProEQ8 commercial target scaffolding.
- 8-band parametric EQ positioning for FreeEQ8.
- Linear phase engine, dynamic EQ controls, match EQ, mid/side processing, oversampling, per-band drive/saturation, spectrum FIFO/analyzer, presets, level meter, update checker, and release workflow files.
- macOS / Linux / Windows build packaging scripts or workflows.
- Public release docs, milestone report, tester call, test matrix, outreach templates, screenshots, Ableton screenshots, and featured/submission trackers.
- Standalone regression evidence: `Tests/AuditRegressionTest.cpp` and `Tests/BiquadTest.cpp` compile and pass outside the JUCE build when tested locally from this package.

### Local audit result from this package

```text
g++ -std=c++17 -O2 -pthread Tests/AuditRegressionTest.cpp -o /tmp/freeeq8_audit_test
/tmp/freeeq8_audit_test
# ALL AUDIT REGRESSION TESTS PASSED

g++ -std=c++17 -O2 Tests/BiquadTest.cpp -o /tmp/freeeq8_biquad_test
/tmp/freeeq8_biquad_test
# ALL TESTS PASSED (6 types x 3 sample rates x 2 configs + sanity)
```

### Current indexable SEO phrase set

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
| Confirmed listed | 3 | FreeEQ8 is already visible/listed. |
| Active PRs/issues | 10 | Waiting for maintainer review or response. |
| Direct email submissions sent | 9 | Outreach sent by email; wait for reply before nudging. |
| Immediate manual/account targets | 9 | Highest-value places that need an account, form, or manual submission. |
| Editorial/forum targets | 20+ | Worth pitching/posting after release assets are clean. |
| Indie/dev launch platforms | 12+ | Good for wider SEO/product discovery. |
| Skipped/blocked | 4 | Not a fit, archived, or blocked by rules. |

## Confirmed listed

| Target | Status | Link |
|---|---:|---|
| GareBear99 / awesome-audio-plugins-dev | ✅ Listed | https://github.com/GareBear99/awesome-audio-plugins-dev#equalizers |
| webprofusion / OpenAudio | ✅ Listed | https://github.com/webprofusion/OpenAudio |
| ad-si / awesome-music-production | ✅ Listed | https://github.com/ad-si/awesome-music-production/pull/197 |

## Active PRs / issues

| Target | Type | Status | Link | Follow-up rule |
|---|---|---:|---|---|
| sudara / awesome-juce | PR | ⏳ New PR open | https://github.com/sudara/awesome-juce/pull/64 | Do not comment unless maintainer replies. |
| dreikanter / awesome-vst | PR | ⏳ Open | https://github.com/dreikanter/awesome-vst/pull/18 | Wait. |
| olilarkin / awesome-musicdsp | PR | ⏳ Open | https://github.com/olilarkin/awesome-musicdsp/pull/11 | Wait. |
| noteflakes / awesome-music | PR + issue | ⏳ Open | https://github.com/noteflakes/awesome-music/pull/109 | Issue #101 is linked; wait. |
| notthetup / awesome-webaudio | Issue | ⏳ Weak fit | https://github.com/notthetup/awesome-webaudio/issues/83 | Leave alone unless maintainer replies. |
| twinysam / FreeAudioPluginList | PR | ⏳ Open | https://github.com/twinysam/FreeAudioPluginList/pull/11 | Wait. |
| nodiscc / awesome-linuxaudio | PR | ⏳ Open | https://github.com/nodiscc/awesome-linuxaudio/pull/71 | Wait. |
| landscape82 / awesome-sound-design-resources | PR | ⏳ Open | https://github.com/landscape82/awesome-sound-design-resources/pull/3 | Wait. |
| brandonhimpfen / awesome-audio-engineering | PR | ⏳ Open | https://github.com/brandonhimpfen/awesome-audio-engineering/pull/5 | Wait. |
| detroitsynth / awesome-open-synth | PR | ⏳ Open | https://github.com/detroitsynth/awesome-open-synth/pull/3 | Wait. |

## Direct email submissions sent

These were sent as direct outreach. Do not resend unless there is a real update, release build, packaged installer, demo video, benchmark/test report, or 10–14 day follow-up window has passed.

| Target | Status | Recipient / route | Sent date | Pitch angle | Follow-up rule |
|---|---:|---|---|---|---|
| ProducersBuzz | ⏳ Sent | `submit@producersbuzz.com` | 2026-05-12 | Free plugin/editorial listing | Wait for reply. |
| Tutorials Dojo | ⏳ Sent | `support@tutorialsdojo.com` | 2026-05-16 | Edge audio AI / local-first audio tooling article mention | Wait. |
| Tutorials Dojo guest article | ⏳ Sent | `support@tutorialsdojo.com` | 2026-05-16 | Guest article: edge audio AI and local-first audio tooling | Wait. |
| Bedroom Producers Blog | ⏳ Sent | `tomislav@bedroomproducersblog.com` | 2026-05-16 | Free/open-source plugin coverage | Best editorial fit; send one audited-release follow-up after package/demo update. |
| Rekkerd | ✅ Covered / follow-up allowed | `ronnie@rekkerd.org` | 2026-05-16 | Plugin news / release coverage | Follow up only for major version, ProEQ8, or new demo video. |
| KVR Audio | ⏳ Sent | `contactus@kvraudio.com` | 2026-05-16 | Plugin/news info; future product listing | Do account/product listing next; email alone is not enough. |
| Gearnews | ⏳ Sent | `news@gearnews.com` | 2026-05-16 | Studio/software/plugin news | Wait. |
| Home Music Maker | ⏳ Sent | `info@homemusicmaker.com` | 2026-05-16 | Home producer free plugin/tool coverage | Wait. |
| Producer Spot | ⏳ Sent | `info@producerspot.com` | 2026-05-16 | Free plugin / producer tool coverage | Wait. |

## Immediate manual/account targets — do next

These are the highest-value gaps after the audit. They need manual submission, account setup, or forms.

| Target | Priority | Action | Recommended indexing angle | Required assets before submitting |
|---|---:|---|---|---|
| KVR Audio Product Database | A+ | Create Developer Account; add FreeEQ8 product; submit news item | `FreeEQ8 — free open-source JUCE/C++ parametric EQ plugin, VST3/AU/Standalone` | Logo/screenshot, release URL, platform list, version, short/long descriptions. |
| Audio Plugins for Free | A | Upload / submit plugin | `Free VST/AU EQ plugin for mixing and mastering` | Direct download/release link, screenshot, OS/format list. |
| Plugins4Free | A | Submit/list plugin if route is active | `Free EQ effect plugin for Cubase, FL Studio, Reaper, Ableton and VST/AU hosts` | Stable download link and install notes. |
| AlternativeTo | A | Suggest new application | `Free and open-source alternative to FabFilter Pro-Q, TDR Nova, ZL Equalizer` | Platforms, GPL-3.0 license, tags, description, screenshot. |
| Product Hunt | A- | Schedule launch | `Free open-source EQ plugin for producers and DSP developers` | Hero image, screenshots, launch comment, maker profile, demo GIF/video. |
| Hacker News / Show HN | A- | Submit only when a major release page/demo is clean | `Show HN: FreeEQ8 – a free open-source JUCE/C++ EQ plugin` | GitHub repo, concise technical comment, no vote requests. |
| REAPER Forum | A | Forum post | `Free open-source EQ plugin tested in REAPER; looking for host feedback` | REAPER screenshot, VST3 install notes. |
| KVR Forum thread | A | Developer/product thread after product listing | `FreeEQ8 — open-source EQ with linear phase, dynamic EQ, match EQ, M/S, analyzer` | KVR product page first. |
| LinuxMusicians | B+ | Post after Linux package/build path is clean | `Open-source JUCE EQ with Linux VST3 build path` | Honest Linux status and build instructions. |

## Editorial / press targets

| Target | Priority | Submission type | Recommended angle |
|---|---:|---|---|
| CDM / Create Digital Music | High | News-tip/contact form | Open-source music technology / independent DSP ecosystem. |
| Sonic State | High | Contact/news route | Music technology and software/plugin news. |
| Synth Anatomy | High | Contact/news route | Better for FreeVox8/spectral-vocoder angle, still useful for FreeEQ8 ecosystem. |
| MusicTech | High | Editorial/news route | Free open-source EQ plugin and independent audio-tool ecosystem. |
| MusicRadar / Computer Music | High | Editorial/news route | Free producer plugin / open-source audio software. |
| Ask.Audio | Medium-high | Contact/editorial route | JUCE/C++ tutorial/review angle. |
| Production Expert | Medium-high | Editorial/contact route | Pro-audio utility and plugin review angle. |
| AudioNewsRoom | Medium-high | Contact route | Indie music-tech/plugin coverage. |
| Attack Magazine | Medium | Contact form | Producer utility / free mix tool. |
| AudioTechnology Magazine | Medium | Editorial/contact route | Studio/audio tech coverage. |
| Mixdown Magazine | Medium | Editorial/contact route | Gear/software/plugin news. |
| Pro Sound Web | Medium | News/editorial route | Pro-audio software/tooling angle. |
| Mixonline | Medium | Editorial route | Professional recording/mixing software angle. |
| Tape Op | Medium-low | Editorial/contact route | Recording engineer / open-source tool angle. |

## Reddit / forum targets

Use bold titles for indexing, but avoid posting the exact same text everywhere.

| Community | Priority | Suggested title angle |
|---|---:|---|
| r/FREEVSTS | A | `FreeEQ8 — free open-source parametric EQ plugin (VST3/AU/Standalone)` |
| r/AudioPlugins | A- | `FreeEQ8: open-source JUCE/C++ EQ plugin with linear phase, dynamic EQ, match EQ and analyzer` |
| r/audioengineering | A- | `I built a free open-source EQ plugin and want engineer feedback` |
| r/mixingmastering | B+ | `FreeEQ8 — free EQ plugin for mixing/mastering feedback` |
| r/musicproduction | B+ | `Free FabFilter Pro-Q style EQ alternative: FreeEQ8 open-source VST3/AU` |
| r/WeAreTheMusicMakers | B | `Free open-source EQ plugin for producers — looking for testing feedback` |
| r/edmproduction | B | `FreeEQ8 — free EQ plugin for electronic music producers` |
| r/Reaper | A | `FreeEQ8 in REAPER — free open-source VST3 EQ plugin` |
| r/ableton | B+ | `FreeEQ8 tested in Ableton Live 10+ — free open-source EQ plugin` |
| r/linuxaudio | B+ | `FreeEQ8 Linux build path — open-source JUCE EQ plugin` |
| r/JUCE | A | `FreeEQ8 — public JUCE/C++ EQ plugin codebase for review` |

## Up-and-coming / underused discovery platforms

| Platform | Priority | Type | Recommended FreeEQ8 angle |
|---|---:|---|---|
| Product Hunt | High | Product launch | Free open-source JUCE/C++ EQ plugin for producers and DSP developers. |
| Hacker News / Show HN | High | Dev community post | `Show HN: FreeEQ8 – a free open-source JUCE/C++ EQ plugin` |
| DEV.to | High | Article | `Building FreeEQ8: A Free Open-Source EQ Plugin in JUCE/C++` |
| Hashnode | Medium | Article mirror | Developer-facing JUCE/C++ DSP story. |
| Uneed | Medium | Product directory | Free open-source creative software. |
| Microlaunch | Medium | Indie product launch | Open-source audio plugin launch. |
| Fazier | Medium | Product/startup directory | Indie audio software / open-source tool. |
| DevHunt | Medium | Developer product directory | Public JUCE/DSP codebase for audio developers. |
| Launching Next | Medium | Product/startup directory | Free creative software launch. |
| SaaSHub | Medium | Software alternative listing | FreeEQ8 as a free/open-source EQ alternative. |
| Futurepedia | Medium | AI/tool directory | Use ARC + FreeVox8 + FreeEQ8 local-first audio/AI ecosystem angle. |
| There’s An AI For That | Medium | AI directory | Better for ARC/FreeVox8 than FreeEQ8 alone. |
| Toolify | Medium | AI/tool directory | Local-first audio AI/DSP ecosystem angle. |
| AIxploria | Medium | AI directory | ARC/FreeVox8 audio AI route. |
| OpenTools AI | Medium | AI directory | Local-first creative tooling route. |

## Audio/plugin-specific directory targets

| Target | Priority | Action |
|---|---:|---|
| KVR Audio product database | Highest | Create official developer/product listing. |
| Audio Plugins for Free | Highest | Submit/upload plugin. |
| Plugins4Free | High | Submit/list as free EQ/effect plugin when release assets and installers are ready. |
| Plexwave free plugins | High | Find official submission/contact route; submit under Effects / EQ. |
| Plugin Boutique | Medium-high | Use developer/product submission form when packaged installers are ready. Better for ProEQ8. |
| LibreMusicProduction | High niche | Submit from open-source/Linux audio angle. |
| LinuxMusicians forum | Medium | Post only if Linux build path is clean and honest. |
| JUCE forum | High | Developer-facing release/testing post. |
| Audio Developer Conference community | Medium | Developer credibility route, not direct producer conversion. |
| VSTBuzz / Audio Plugin Deals | Medium-low | Deal/freebie angle; lower fit unless there is a packaged release. |

## Skipped / blocked

| Target | Status | Reason |
|---|---:|---|
| spnw / free-music-plugins | ⚠️ Archived | Repository is read-only. PR creation failed because repo is archived. |
| BillyDM / awesome-audio-dsp | ⛔ Skipped | Contribution/self-promotion/financial-incentive concern. |
| matthewamend / awesome-audio-plugins | ⛔ Skipped | Plugin API/library list, not end-user plugin list. |
| DolbyIO / awesome-audio | ⛔ Skipped | API/DAW/developer-resource focus; no strong individual plugin section. |

## Outreach rules

- Keep the loud SEO phrases in titles, tags, headings, and first paragraph.
- For maintainer-run GitHub lists, keep the submitted line short and factual so PRs do not get rejected as advertising.
- Do not open duplicate PRs while a PR is open.
- Do not post repeated comments after a nudge.
- Wait for maintainer action or at least 10–14 days before a polite follow-up.
- If a maintainer closes a PR with instructions, open a new clean PR only if requested.
- For publications, only follow up when there is a meaningful update: packaged installers, release tag, test report, screenshots, demo video, or coverage page.
- Use the `free FabFilter Pro-Q alternative` language for search indexing, but phrase it as an alternative/comparison, not an official replacement claim.

## One-line canonical submission text

> FreeEQ8 — Free, open-source JUCE-based 8-band parametric EQ plugin for VST3/AU/Standalone music production workflows.

## SEO one-line submission text

> FreeEQ8 is a free FabFilter Pro-Q style open-source EQ plugin for VST3/AU/Standalone workflows, built in JUCE/C++ with linear phase, dynamic EQ, match EQ, M/S processing, per-band drive, oversampling, and a real-time spectrum analyzer.

## Medium canonical submission text

> FreeEQ8 is a free, open-source JUCE-based 8-band parametric EQ plugin with linear phase, dynamic EQ, match EQ, M/S processing, per-band drive, oversampling, band linking, and VST3/AU/Standalone support.

## Product Hunt tagline

> Free open-source JUCE/C++ EQ plugin for producers and DSP developers.

## Show HN title

> Show HN: FreeEQ8 – a free open-source JUCE/C++ EQ plugin

## DEV.to article title

> FreeEQ8: The Free Open-Source EQ Plugin Built to Challenge Paid Mixing Tools

## 2026 public testing-feedback outreach

| Target / community | Status | Link |
|---|---:|---|
| JUCE team | Email sent | `info@juce.com` |
| Tracktion/pluginval | Posted | https://github.com/Tracktion/pluginval/issues/168 |
| Chowdhury-DSP/BYOD | Posted | https://github.com/Chowdhury-DSP/BYOD/issues/383 |
| DISTRHO/DPF | Posted | https://github.com/DISTRHO/DPF/issues/526 |
| iPlug2 community | Posted | https://github.com/orgs/iPlug2/discussions/1354 |

These are technical-feedback routes, not claimed endorsements.


## 2026-05-21 Gmail + Live Listing Validation Update

- **Rekkerd.org is confirmed live coverage**, not just a submission. Article: `FREE: FreeEQ8 parametric EQ effect plugin by Gary Doman` published 2026-05-19. Use this as the primary third-party credibility anchor in future outreach.
- **Rekkerd email evidence:** Ronnie replied “Thanks Gary, I’ll post about it,” then later acknowledged the version typo note with “Woops, will fix that version typo. Thanks!”
- **KVR replied with the correct route:** developer account -> product listing -> submit news item. Email alone is not enough; this is now the next highest-value indexing action.
- **BPB / ProducerSpot / HomeMusicMaker / Gearnews / MusicTech-style targets:** sent, but no confirmed positive reply found in the audited Gmail window.
- **OpenAudio / awesome-music-production / awesome-audio-dsp / awesome-juce / awesome-webaudio:** evidence exists of accepted or completed GitHub/listing activity, but avoid duplicate submissions.
- **Off-target outreach to avoid:** iPlug2 and DISTRHO/DPF unless FreeEQ8 gains real iPlug2/DPF/LV2 relevance.
- Full validation record: `docs/outreach/EMAIL_VALIDATION_AUDIT_2026-05-21.md`.
