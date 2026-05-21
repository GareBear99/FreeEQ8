# FreeEQ8 Cross-Checked Submission Status — 2026-05-21

This audit cross-checks FreeEQ8 outreach/listing status against the current repo docs, Gmail evidence, and publicly reachable web/GitHub pages where available. It is intentionally conservative: a target is only marked **verified live listing** when the public page itself currently shows FreeEQ8.

## Status key

| Status | Meaning |
|---|---|
| ✅ Verified live listing / coverage | Public page currently shows FreeEQ8 or the third-party article is live. Safe to cite as a public win. |
| ✅ Merged PR + visible | PR is merged and current public README/list page shows FreeEQ8. Safe to cite as listed. |
| ⚠️ Completed/closed evidence only | Gmail/GitHub notification says closed/completed, but current public README/list page did not show FreeEQ8 during this audit or could not be verified. Do **not** market as live-listed until rechecked. |
| ⏳ Open / pending | PR, issue, email, or form route is still awaiting action/review. |
| 🟡 Routing reply / manual action needed | The organization replied with the correct route, but the listing itself is not complete. |
| ❌ Off-target / do not repeat | Outreach reached a real maintainer/community, but the target is not a fit for the current JUCE/VST3/AU FreeEQ8 project. |

## Verified public wins — safe to cite

| Target | Status | Evidence | Safe public wording |
|---|---|---|---|
| Rekkerd.org | ✅ Verified live coverage | Live article: `FREE: FreeEQ8 parametric EQ effect plugin by Gary Doman`, published 2026-05-19. Gmail confirms Ronnie accepted the submission and later acknowledged the version typo note. | “Featured by Rekkerd.org.” |
| webprofusion / OpenAudio | ✅ Verified live listing | Public OpenAudio page currently shows `FreeEQ8` in Audio Plugins with description: professional-grade, free and open-source 8-band parametric EQ for macOS, Linux, and Windows. Issue #207 is closed. | “Listed on OpenAudio.” |
| ad-si / awesome-music-production | ✅ Merged PR + visible | PR #197 merged into master on 2026-03-16; current public README shows `FreeEQ8` under Apps as a free, open-source 8-band parametric EQ plugin. | “Listed in awesome-music-production.” |
| GareBear99 / awesome-audio-plugins-dev | ✅ Self-maintained listing | Repo badge/link points to the Equalizers section of the user-maintained audio plugin dev list. This is useful SEO, but it is not third-party validation. | “Listed in the GareBear99 audio plugin dev list.” |

## Accepted/closed evidence — do not overclaim as live listing without recheck

| Target | Current conservative status | Evidence found | Action |
|---|---|---|---|
| BillyDM / awesome-audio-dsp | ⚠️ Closed/completed evidence only | Gmail notification says Issue #14 was closed as completed after maintainer discussion. Current public README check did **not** find `FreeEQ8`. | Keep as “completed issue / not currently verified live.” Do not claim public listing until the README/list page shows it. |
| sudara / awesome-juce | ⚠️ Mixed history; not verified live | Gmail shows earlier Issue #62/#63 closed as completed, but current public README check did **not** find `FreeEQ8`. Later PR #61 was closed with instruction to open a new shorter PR. | Keep current route as pending/resubmission, not listed. |
| notthetup / awesome-webaudio | ⚠️ Conflicting evidence; treat as unresolved | Gmail notification says Issue #83 closed as completed, but the public issue page checked during audit showed it open and the README check did not find `FreeEQ8`. Also weak fit because FreeEQ8 is JUCE/VST3/AU, not Web Audio. | Do not claim listed. Leave alone unless maintainer explicitly requests PR again. |
| brandonhimpfen / awesome-audio-engineering | ⚠️ Closed PR, not verified merged/listed | Gmail notification says PR #5 closed and maintainer said FreeEQ8 was technically relevant, but closure alone is not a verified listing. | Mark closed/not listed unless public README confirms it. |

## Open / pending PRs and issues to wait on

| Target | Status | Link / route | Follow-up rule |
|---|---|---|---|
| dreikanter / awesome-vst | ⏳ PR open/pending | `https://github.com/dreikanter/awesome-vst/pull/18` | Wait. |
| olilarkin / awesome-musicdsp | ⏳ PR open/pending | `https://github.com/olilarkin/awesome-musicdsp/pull/11` | Wait. |
| noteflakes / awesome-music | ⏳ PR + issue open/pending | `https://github.com/noteflakes/awesome-music/pull/109`; Issue #101 linked | Wait. |
| twinysam / FreeAudioPluginList | ⏳ PR open/pending | `https://github.com/twinysam/FreeAudioPluginList/pull/11` | Wait. |
| nodiscc / awesome-linuxaudio | ⏳ PR open/pending | `https://github.com/nodiscc/awesome-linuxaudio/pull/71` | Wait; make Linux build notes honest. |
| landscape82 / awesome-sound-design-resources | ⏳ PR open/pending | `https://github.com/landscape82/awesome-sound-design-resources/pull/3` | Wait. |
| detroitsynth / awesome-open-synth | ⏳ PR open/pending | `https://github.com/detroitsynth/awesome-open-synth/pull/3` | Weak fit; wait, do not push. |

## Email outreach — sent, no confirmed coverage found in this audit

| Target | Status | Evidence | Next move |
|---|---|---|---|
| Bedroom Producers Blog | ⏳ Email sent | Sent to `tomislav@bedroomproducersblog.com` on 2026-03-26, 2026-05-05, and 2026-05-16. No positive reply found in audited Gmail results. | Follow up once with Rekkerd link + demo assets only. |
| ProducerSpot | ⏳ Email sent | Sent to `info@producerspot.com` on 2026-05-16. | Wait / one follow-up after asset polish. |
| ProducersBuzz | ⏳ Email sent | Sent to `submit@producersbuzz.com` on 2026-05-12. | Wait / one follow-up after release update. |
| Home Music Maker | ⏳ Email sent | Sent to `info@homemusicmaker.com` on 2026-05-16. | Wait. |
| Gearnews | ⏳ Email sent | Sent to `news@gearnews.com` on 2026-05-16. | Wait. |
| MusicTech / NME / Guitar.com | ⏳ Email sent | Sent to editorial addresses on 2026-05-05. | Low priority; pitch only with bigger story. |
| Sound On Sound | ⏳ Email sent | Sent review request on 2026-05-05. | Wait; follow only with a stable installer/demo. |
| Tutorials Dojo | ⏳ Email sent | Sent edge audio AI/editorial suggestion on 2026-05-16. | Better fit for ARC/edge AI article than FreeEQ8 alone. |

## Manual/account routes — still not completed

| Target | Status | What evidence says | Required action |
|---|---|---|---|
| KVR Audio Product Database | 🟡 Routing reply / manual action needed | KVR replied that plug-in, soundware, and app developers manage product listings and submit news directly through the developer portal. | Create/use KVR developer account, add FreeEQ8 product listing, then submit news item. |
| Audio Plugins for Free | ⏳ Not verified submitted/listed | No confirmed listing found in this audit. | Submit manually if route is available. |
| Plugins4Free | ⏳ Not verified submitted/listed | No confirmed listing found in this audit. | Submit manually if route is available. |
| AlternativeTo | ⏳ Not verified submitted/listed | No confirmed listing found in this audit. | Create application/profile suggestion. |
| Product Hunt | ⏳ Not launched | No confirmed launch found in this audit. | Launch only after hero image/demo/release links are ready. |
| Hacker News / Show HN | ⏳ Not launched | No confirmed Show HN found in this audit. | Post as technical build story, not promo. |
| REAPER Forum / r/Reaper | ⏳ Not verified posted/listed | No confirmed public thread found in this audit. | Strong next target because user has REAPER testing evidence. |

## Technical outreach — validated interpretation

| Target | Status | Evidence | Correct interpretation |
|---|---|---|---|
| JUCE Forum / JUCE support | 🟡 Relevant but needs narrow asks | JUCE support restored account after automated hold and said the forum is the best place, while warning that broad review requests ask a lot of people’s time. Earlier forum replies asked build/DSP questions. | Use narrow technical posts: one build question, one pluginval question, one UI/performance question. |
| Tracktion/pluginval | ⏳ Relevant technical route | Public issue route exists in docs. | Keep as validation-focused only. |
| Chowdhury-DSP/BYOD | 🟡 Paid expert review offered | Jatin replied by email, closed the public issue as not directly relevant, and offered consulting at $85/hour. | Optional paid review, not free community validation. |
| iPlug2 | ❌ Off-target / do not repeat | Maintainer replied “Doesn't seem iPlug2 relevant” and discussion was closed. | Do not continue unless FreeEQ8 has a real iPlug2 port/comparison. |
| DISTRHO/DPF | ❌ Off-target / do not repeat | Maintainer objected that FreeEQ8 is JUCE while DPF is a different framework; discussion closed. | Do not continue unless FreeEQ8 gains a DPF/LV2/CLAP port or specific DPF relevance. |

## Correct public credibility statement

> FreeEQ8 has confirmed third-party coverage from Rekkerd.org and verified public listings on OpenAudio and awesome-music-production. Several other GitHub/list submissions are pending or have closed/completed issue evidence, but they should not be marketed as live listings unless the current public list page visibly contains FreeEQ8.

## Next three actions

1. Finish the KVR developer product listing and news submission.
2. Use Rekkerd + OpenAudio + awesome-music-production as the safe credibility stack in all new pitches.
3. Follow up with BPB/producer blogs only after adding a clean demo clip, stable release link, and direct install notes.
