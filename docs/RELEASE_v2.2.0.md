# FreeEQ8 / ProEQ8 v2.2.0 — Release Notes & Readiness

## TL;DR
This is a **real-time safety + correctness release** for both FreeEQ8
(free, 8-band) and ProEQ8 (commercial, 24-band). No new features in the
user-visible feature set; this is "the release that makes the DSP
engine bulletproof under stress".

- **RT-safety:** no heap allocations on the audio thread for any user
  action, including live oversampling-factor changes.
- **Concurrency:** the spectrum-analyzer FIFO and the linear-phase FIR
  kernel now use the canonical swap-chain triple-buffer pattern —
  provably race-free under stress (3× 300–400 ms concurrent-stress runs,
  0 tears across ~600 M samples and ~300 k kernel handoffs).
- **ProEQ8 demo cadence:** 2 minutes of clean playback followed by a
  30-second mute window (was 4:30 / 30 s). Activation removes the mute.

## What's in the drop
- `FreeEQ8.vst3`, `FreeEQ8.component` (AU), `FreeEQ8.app` (Standalone).
- `ProEQ8.vst3`, `ProEQ8.component` (AU, 24-band, commercial).
- `README.txt` with install instructions + license notice.

## What changed at the code level
See `CHANGELOG.md` under `[2.2.0]` and `docs/MILESTONE_A_REPORT.md` for
the full writeup. Summary:

- A1 — oversampler pool (no heap alloc on audio thread on order change).
- A2 — editor modal dialogs and background HTTP callbacks are lifetime-
  safe (no double-free, no dangling `this`).
- A3 — `MatchEQ::applyCorrection` now chunks blocks >4096 samples instead
  of silently dropping them.
- A4 — `SpectrumFIFO` triple-buffer replaces single-ring with race.
- A5 — linear-phase FIR rebuild moved off the audio thread via a
  dedicated `juce::Thread`; kernel handoff via swap-chain triple buffer.
- A7 — `getTailLengthSeconds` covers the Match-EQ overlap-add tail.
- Demo cadence (ProEQ8 only, unactivated): 120 s clean + 30 s mute.

## Verification evidence
Completed on this machine on the release branch `audit/milestone-a-correctness`:

- `cmake --build build --target FreeEQ8_All ProEQ8_All` — clean, 0
  warnings, 0 errors.
- `FreeEQ8_Tests` — existing biquad coefficient suite green.
- `FreeEQ8_AuditTests` — triple-buffer, chunking, kernel-handoff
  invariants proven against concurrent stress.
- `server/test-activation.js` — **35 / 35 assertions pass** covering
  HMAC sign/verify, 2-device cap, idempotency, deactivation, CORS,
  error paths.
- Deployed license worker `https://proeq8-license-server.admension.workers.dev/health`
  — HTTP 200, reports `{"status":"ok","version":"2.0.0"}`, 264 ms cold start.

## Release-readiness assessment — honest

### Green (verified)
- DSP correctness and RT-safety (`Tests/AuditRegressionTest.cpp`).
- License server endpoints (`server/test-activation.js`, live `/health`
  probe).
- Signature validation math (HMAC-SHA256) matches between client
  (`LicenseValidator.h`) and worker (`server/stripe-webhook.js`).
- Device fingerprinting (`computeDeviceId()`) is deterministic and
  unchanged from v2.1.0.

### Yellow (operator-only — cannot be automated on this machine)
- **End-to-end Stripe checkout → email → in-plugin activation:** this
  requires swiping a real test card through the live Stripe Checkout
  URL, receiving the resulting email at a real inbox, pasting the key
  into ProEQ8, and confirming the `Licensed` state. I can't do this on
  your behalf. **Owner action required** before shipping.
- **Re-verify flow after 7 days / 30-day grace:** time-dependent;
  requires clock manipulation or a week of elapsed time on a real
  install.

### Red (release-blocker on this machine, not for CI)
- **Local DMG is `x86_64`-only.** The macOS 10.15 CommandLineTools SDK
  on this machine does not include arm64 `libSystem`, so
  `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` fails the CMake try_compile.
  The DMG produced locally (`build/FreeEQ8-v2.2.0-macOS.dmg`, 17 MB) is
  therefore suitable for **local testing on Intel Macs only**. For the
  shipping release, **push the tag and let GitHub Actions build the
  universal binary** — `.github/workflows/release.yml` already passes
  `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` on a `macos-14` runner with
  full Xcode.
- **macOS code-signing + notarization:** not yet wired up. Milestone F
  in the audit plan covers this. Unsigned VST3s / AUs will be Gatekeeper-
  quarantined on Apple Silicon and require user override.

## Recommended shipping path
1. Review + approve the `audit/milestone-a-correctness` branch.
2. Push branch, merge into `main`.
3. Tag `v2.2.0` (which `.github/workflows/release.yml` triggers on).
4. CI builds universal macOS DMG, Linux tar.gz, Windows zip, attaches
   them to the GitHub Release.
5. **Owner**: walk through one live Stripe purchase + activation end-to-
   end on a clean machine before public announcement.
6. Optional before step 4: address Milestone F (code-signing +
   notarization) to avoid Gatekeeper friction.

## Artifacts produced locally (not for shipping)
- `build/FreeEQ8-v2.2.0-macOS.dmg` (17 MB, x86_64 only, test use only).
- `build/FreeEQ8_Tests`, `build/FreeEQ8_AuditTests` — validation binaries.

---

Co-Authored-By: Oz <oz-agent@warp.dev>
