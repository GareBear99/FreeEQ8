# FreeEQ8 Tester Call

FreeEQ8 is a free open-source EQ plugin built with JUCE.

I am looking for producers, engineers, and plugin developers to test FreeEQ8 across different DAWs and systems before the next cleanup pass.

## What I need tested

- DAW scanning and loading
- VST3 compatibility
- AU compatibility on macOS
- UI scaling and responsiveness
- Basic EQ workflow
- Preset/session recall
- CPU behavior
- Crashes, freezes, or weird host behavior

## Best DAWs to test

- REAPER
- Ableton Live
- Logic Pro
- FL Studio
- Studio One
- Cubase
- Bitwig

## How to help

1. Download the latest release.
2. Load FreeEQ8 in your DAW.
3. Try it on vocals, drums, synths, bass, and master bus.
4. Report whether it works or what breaks.

## Public technical feedback status

FreeEQ8 has been shared for engineering/testing feedback. Current cross-checked status:

- JUCE Forum / JUCE support: relevant, but future asks should be narrow and specific.
- Tracktion/pluginval: relevant validation route — https://github.com/Tracktion/pluginval/issues/168
- Chowdhury-DSP/BYOD: issue closed as not directly repo-relevant; Jatin offered paid expert review by email.
- DISTRHO/DPF: off-target because FreeEQ8 is JUCE, not DPF. Do not repeat unless there is a DPF/LV2/CLAP port.
- iPlug2: off-target because FreeEQ8 is JUCE, not iPlug2. Do not repeat unless there is an iPlug2 port/comparison.

Full tracker: [PUBLIC_FEEDBACK_CHANNELS_2026.md](PUBLIC_FEEDBACK_CHANNELS_2026.md)

## Submit feedback

Use the tester feedback issue form:

https://github.com/GareBear99/FreeEQ8/issues/new?template=tester-feedback.yml

## Repo

https://github.com/GareBear99/FreeEQ8

## Latest release

https://github.com/GareBear99/FreeEQ8/releases

## What kind of feedback helps most?

Good feedback looks like this:

- OS:
- DAW:
- Plugin format:
- FreeEQ8 version:
- What worked:
- What broke:
- Steps to reproduce:
- Screenshot/log if available:

Thank you for helping test a free open-source audio plugin.
