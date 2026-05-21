# FreeEQ8 Public Testing Feedback Channels — 2026

_Last cross-checked: 2026-05-21_

FreeEQ8 is actively requesting technical feedback from audio DSP developers, plugin validation users, DAW testers, and JUCE/plugin developers.

This tracker documents public outreach threads and the kind of feedback requested. It also marks which routes were off-target so they are not repeated.

## Current public feedback routes

| Target / community | Status | Link / route | Why it matters | Follow-up rule |
|---|---:|---|---|---|
| JUCE Forum / JUCE support | 🟡 Relevant, but use narrow asks | `info@juce.com` / JUCE forum | FreeEQ8 is a JUCE/C++ plugin; JUCE support restored account and said the forum is the right place. | Ask one specific question at a time: build, pluginval, UI performance, or RT safety. |
| Tracktion/pluginval | ⏳ Relevant validation route | https://github.com/Tracktion/pluginval/issues/168 | Plugin validation and host-test focused feedback, especially VST3/AU validation behavior and pluginval compatibility expectations. | Keep technical and validation-specific. |
| Chowdhury-DSP/BYOD | 🟡 Paid expert feedback offered | https://github.com/Chowdhury-DSP/BYOD/issues/383 | Jatin replied by email and offered technical review/consulting at $85/hour after closing the issue as not directly repo-relevant. | Optional paid review only; do not treat as public validation/listing. |

## Off-target outreach — do not repeat without real port/relevance

| Target / community | Status | Link | Why not repeat |
|---|---:|---|---|
| DISTRHO/DPF | ❌ Off-target | https://github.com/DISTRHO/DPF/issues/526 | Maintainer objected that FreeEQ8 is JUCE while DPF is a different framework. Repeat only if there is a DPF/LV2/CLAP port or specific DPF relevance. |
| iPlug2 community | ❌ Off-target | https://github.com/orgs/iPlug2/discussions/1354 | Maintainer said it does not seem iPlug2-relevant. Repeat only if there is an iPlug2 port/comparison. |

## Feedback requested

FreeEQ8 is specifically looking for engineering feedback on:

- VST3/AU behavior across DAWs
- DAW scanning and plugin loading
- real-time safety concerns
- analyzer/UI performance
- dynamic EQ behavior
- linear-phase and oversampling implementation concerns
- session recall and preset handling
- packaging, validation, and host compatibility expectations

## Preferred feedback format

Please include:

- OS and version
- DAW and version
- Plugin format tested: VST3, AU, Standalone
- FreeEQ8 version or commit
- What worked
- What broke
- Steps to reproduce
- Screenshot/log if available

## Official feedback form

Use the FreeEQ8 tester-feedback issue template:

https://github.com/GareBear99/FreeEQ8/issues/new?template=tester-feedback.yml

## Outreach rule

Do not spam maintainers. Public outreach should be limited to relevant DSP/plugin-framework/testing communities and should be framed as a technical feedback request, not promotion.
