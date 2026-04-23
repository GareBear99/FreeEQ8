#pragma once
#include <cmath>
#include <algorithm>

// ============================================================================
// IntentMode
// ----------------------------------------------------------------------------
// Behavioural biasing for the Smart EQ layer. Selecting an intent does NOT
// set fixed preset bands — it changes *where the detector looks first* so the
// same underlying resonance-detection algorithm ranks different peaks.
//
// Each mode defines a frequency-domain weighting curve returned by
// `intentWeightFor(mode, hz)`:
//
//   None        : flat 1.0 everywhere (no biasing)
//   VocalClean  : boost weight around 200-500 Hz (mud) and 2-5 kHz (harshness)
//   DrumPunch   : boost weight around 200-500 Hz (box) and 6-10 kHz (ring)
//   GuitarSpace : boost weight around 150-400 Hz (mud) and 1-4 kHz (honk)
//   MasterPolish: mild 150-450 Hz + gentle >10 kHz weighting
//
// Weights are multiplicative and typically in the range 0.7..2.0.
// ============================================================================
enum class IntentMode : int
{
    None         = 0,
    VocalClean   = 1,
    DrumPunch    = 2,
    GuitarSpace  = 3,
    MasterPolish = 4,
};

// Smooth Gaussian "bump" in log-frequency space.
inline float intentBump(float hz, float centerHz, float octaves, float peakGain) noexcept
{
    if (hz <= 0.0f || centerHz <= 0.0f) return 0.0f;
    const float logDelta = std::log2(hz / centerHz) / std::max(0.1f, octaves);
    return peakGain * std::exp(-2.0f * logDelta * logDelta);
}

inline float intentWeightFor(IntentMode mode, float hz) noexcept
{
    float w = 1.0f;
    switch (mode)
    {
        case IntentMode::None:
            return 1.0f;
        case IntentMode::VocalClean:
            w += intentBump(hz,  300.0f, 0.6f, 0.6f);   // mud
            w += intentBump(hz, 3200.0f, 0.7f, 0.5f);   // harshness
            break;
        case IntentMode::DrumPunch:
            w += intentBump(hz,  300.0f, 0.6f, 0.5f);   // box
            w += intentBump(hz, 7500.0f, 0.7f, 0.4f);   // ring
            break;
        case IntentMode::GuitarSpace:
            w += intentBump(hz,  250.0f, 0.6f, 0.5f);   // mud
            w += intentBump(hz, 2500.0f, 0.8f, 0.5f);   // honk
            break;
        case IntentMode::MasterPolish:
            w += intentBump(hz,  250.0f, 0.8f, 0.3f);
            w += intentBump(hz, 12000.0f, 0.8f, 0.2f);  // air ring
            break;
    }
    return std::clamp(w, 0.5f, 2.5f);
}

inline const char* intentModeLabel(IntentMode mode) noexcept
{
    switch (mode)
    {
        case IntentMode::VocalClean:   return "Vocal Clean";
        case IntentMode::DrumPunch:    return "Drum Punch";
        case IntentMode::GuitarSpace:  return "Guitar Space";
        case IntentMode::MasterPolish: return "Master Polish";
        case IntentMode::None:         default: return "None";
    }
}
