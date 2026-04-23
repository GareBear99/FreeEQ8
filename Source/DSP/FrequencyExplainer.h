#pragma once

// ============================================================================
// FrequencyExplainer
// ----------------------------------------------------------------------------
// Static mapping from a frequency (Hz) + gain-direction to a short human
// description. Drives the "Explain Mode" hover popup and the Smart EQ layer's
// suggestion labels. Semantic ranges are the standard ones used by mixing
// engineers (sub-bass, low-end, lower-mid mud, body, boxiness, nasal,
// upper-mid honk, presence, harshness, sibilance, air).
//
// Return strings are static string literals (lifetime is forever), safe to
// pass to juce::String or to draw directly.
// ============================================================================
inline const char* frequencyRangeLabel(float hz) noexcept
{
    if (hz <   30.0f) return "sub-bass";
    if (hz <   80.0f) return "sub / rumble";
    if (hz <  150.0f) return "low-end weight";
    if (hz <  250.0f) return "low thump";
    if (hz <  500.0f) return "mud / low-mid";
    if (hz <  800.0f) return "body / boxiness";
    if (hz < 1200.0f) return "lower-mid fullness";
    if (hz < 2000.0f) return "upper-mid nasal";
    if (hz < 3000.0f) return "honk / definition";
    if (hz < 5000.0f) return "presence";
    if (hz < 7000.0f) return "bite / harshness";
    if (hz < 10000.0f) return "sibilance";
    if (hz < 14000.0f) return "brilliance / air";
    return "ultra air";
}

// Returns a short phrase describing what a cut or boost in the given range
// typically does, e.g. "Cutting boxiness (320 Hz)" or "Adding air (12 kHz)".
// Callers are expected to format the freq themselves if they want units.
inline const char* frequencyActionDescription(float hz, bool isCut) noexcept
{
    // Shared semantic zones, different verbs for cut vs boost.
    if (hz <  80.0f)  return isCut ? "Removing sub rumble"        : "Adding sub weight";
    if (hz < 200.0f)  return isCut ? "Trimming low-end buildup"   : "Adding low-end body";
    if (hz < 400.0f)  return isCut ? "Cutting mud"                : "Adding warmth";
    if (hz < 800.0f)  return isCut ? "Reducing boxiness"          : "Adding body";
    if (hz < 1600.0f) return isCut ? "Reducing nasal / honk"      : "Adding midrange clarity";
    if (hz < 3200.0f) return isCut ? "Taming honk / harsh mids"   : "Adding definition";
    if (hz < 6400.0f) return isCut ? "Reducing harshness"         : "Adding presence";
    if (hz < 10000.0f) return isCut ? "Taming sibilance"          : "Adding bite";
    return isCut ? "Softening air ring" : "Adding air";
}
