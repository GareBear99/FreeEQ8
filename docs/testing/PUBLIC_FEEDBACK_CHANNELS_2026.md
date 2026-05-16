# FreeEQ8 Public Testing Feedback Channels — 2026

FreeEQ8 is actively requesting technical feedback from audio DSP developers, plugin framework maintainers, host-validation users, and DAW testers.

This tracker documents public outreach threads and the kind of feedback requested. It is meant to make testing transparent, avoid duplicate outreach, and give contributors a clear place to reply.

_Last updated: 2026-05-16_

## Active public feedback threads

| Target / community | Status | Link | Why it matters |
|---|---:|---|---|
| JUCE team | Email sent | `info@juce.com` | FreeEQ8 is a JUCE/C++ plugin; feedback requested on JUCE plugin architecture, validation expectations, host compatibility, and real-time safety. |
| Tracktion/pluginval | Posted | https://github.com/Tracktion/pluginval/issues/168 | Plugin validation and host-test focused feedback, especially VST3/AU validation behavior and pluginval compatibility expectations. |
| Chowdhury-DSP/BYOD | Posted | https://github.com/Chowdhury-DSP/BYOD/issues/383 | Audio DSP/plugin developer feedback from an open-source effects-plugin codebase/community. |
| DISTRHO/DPF | Posted | https://github.com/DISTRHO/DPF/issues/526 | Cross-platform plugin-framework feedback around VST3/AU/LV2/CLAP-style compatibility, packaging, and RT-safe expectations. |
| iPlug2 community | Posted | https://github.com/orgs/iPlug2/discussions/1354 | Plugin-framework and DSP developer feedback through a GitHub Discussion rather than an issue thread. |

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
