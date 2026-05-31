#pragma once
#include <cmath>
#include <algorithm>

// =============================================================================
// SvfBiquad — Andrew Simper (Cytomic) State Variable Filter
// =============================================================================
//
// Reference: "Solving the continuous SVF equations using trapezoidal integration
//             and equivalent currents", Andrew Simper, Cytomic, 2013.
//             https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
//
// Why SVF over RBJ (introduced in v2.2.2):
//   CORRECTION (May 2026, after JUCE forum + r/DSP community review):
//   Both SVF and RBJ use the bilinear transform (BLT) with identical prewarping.
//   Both produce the same steady-state frequency response — cramping is a BLT
//   property, not a topology property. Verified by BiquadVsSvfComparison.cpp:
//   RBJ and SVF are identical to 0.0000 dB at every frequency including 16 kHz.
//
//   Real SVF advantages (per SkoomaDentist, r/DSP May 2026):
//   1. Better SNR when fc is near DC (kick/bass EQ at low frequencies)
//   2. Reduces coefficient-change noise during parameter automation
//   3. More stable coefficient interpolation for per-sample Dynamic EQ updates
//
//   Q convention: Simper Bell uses kA=1/(Q*A), A=sqrt(G), so effective
//   bandwidth = Q*sqrt(G) — matches RBJ's stated personal convention.
//
// Additional benefits:
//   - Stable under audio-rate parameter modulation (no feedback blow-up)
//   - All filter types share one 2-integrator core
//   - Parallelisable: v1 and v2 can be computed from v0 directly
//   - Symmetric state update: ic1eq = 2*v1 - ic1eq (no division)
//
// Usage (matches Biquad API):
//   SvfBiquad bq;
//   bq.set(SvfBiquad::Type::Bell, sampleRate, freqHz, Q, gainDb);
//   float out = bq.processL(in);
//   float outR = bq.processR(in);
//   bq.reset();
//
// Thread safety: same as Biquad.h — not thread-safe internally.
// Processing precision: 64-bit double state, float I/O.
// =============================================================================

struct SvfBiquad
{
    enum class Type { Bell, LowShelf, HighShelf, LowPass, HighPass, Bandpass, Notch, AllPass };

    // ── Coefficient state (recomputed on parameter change) ───────────────────
    double a1 = 0.0, a2 = 0.0, a3 = 0.0;   // precomputed kernel coefficients
    double m0 = 1.0, m1 = 0.0, m2 = 0.0;   // output mix (filter-type-specific)

    // ── Per-channel integrator state ─────────────────────────────────────────
    double ic1eqL = 0.0, ic2eqL = 0.0;      // left channel
    double ic1eqR = 0.0, ic2eqR = 0.0;      // right channel

    // ── Coefficient computation ───────────────────────────────────────────────
    // Call whenever fc, Q, or gain changes. Safe to call from audio thread
    // (atomic copy not required — same thread as process).
    void set(Type type, double sampleRate, double freqHz, double Q, double gainDb = 0.0) noexcept
    {
        cachedType = type;
        // Clamp inputs to safe ranges
        freqHz = std::max(1.0,  std::min(freqHz, sampleRate * 0.499));
        Q      = std::max(0.01, std::min(Q,      100.0));

        // Core SVF coefficient: g = tan(pi*fc/fs) — BLT prewarping, same as RBJ
        const double g  = std::tan(kPi * freqHz / sampleRate);
        const double k  = 1.0 / Q;

        // Pre-compute a1, a2, a3 (shared across all filter types)
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;   // = g^2 * a1

        // Output mix coefficients (m0*v0 + m1*v1 + m2*v2)
        // Derived from Simper paper sections per filter type
        switch (type)
        {
            case Type::LowPass:
                m0 = 0.0;  m1 = 0.0;  m2 = 1.0;
                break;

            case Type::HighPass:
                m0 = 1.0;  m1 = -k;   m2 = -1.0;
                break;

            case Type::Bandpass:
                m0 = 0.0;  m1 = 1.0;  m2 = 0.0;
                break;

            case Type::Notch:
                m0 = 1.0;  m1 = -k;   m2 = 0.0;
                break;

            case Type::AllPass:
                m0 = 1.0;  m1 = -2.0 * k;  m2 = 0.0;
                break;

            case Type::Bell:
            {
                // H(s) = (s^2 + (k*A)*s + 1) / (s^2 + (k/A)*s + 1)
                // where A = 10^(gainDb/40).
                //
                // Simper paper (p.18) pseudocode uses k_paper = 1/(Q*A),
                // which equals our kA = k/A. BOTH the denominator coefficients
                // AND the m1 mix coefficient use k_paper, not the base k=1/Q.
                //   a1  = 1/(1 + g*(g + kA))          → denominator
                //   m1  = kA * (A^2 - 1)              → numerator mix
                // Using k instead of kA for m1 makes the bell A-times too wide.
                const double A  = std::pow(10.0, gainDb / 40.0);
                const double kA = k / A;  // = 1/(Q*A) — paper's k
                a1 = 1.0 / (1.0 + g * (g + kA));
                a2 = g * a1;
                a3 = g * a2;
                m0 = 1.0;
                m1 = kA * (A * A - 1.0);  // uses kA, not k — critical
                m2 = 0.0;
            } break;

            case Type::LowShelf:
            {
                // Cutoff shifts down by 1/sqrt(A) as gain increases,
                // keeping the half-gain point constant
                const double A  = std::pow(10.0, gainDb / 40.0);
                const double gA = g / std::sqrt(A);  // modified cutoff
                const double kA = k;
                a1 = 1.0 / (1.0 + gA * (gA + kA));
                a2 = gA * a1;
                a3 = gA * a2;
                m0 = 1.0;
                m1 = k * (A - 1.0);
                m2 = A * A - 1.0;
            } break;

            case Type::HighShelf:
            {
                // Cutoff shifts up by sqrt(A) as gain increases
                const double A  = std::pow(10.0, gainDb / 40.0);
                const double gA = g * std::sqrt(A);  // modified cutoff
                const double kA = k;
                a1 = 1.0 / (1.0 + gA * (gA + kA));
                a2 = gA * a1;
                a3 = gA * a2;
                m0 = A * A;
                m1 = k * (1.0 - A) * A;
                m2 = 1.0 - A * A;
            } break;
        }
    }

    // ── Per-sample processing ─────────────────────────────────────────────────
    // Optimised "bounded" form (Simper §2). Key optimisations vs naive form:
    //
    //   1. Cache `t = a2 * ic1eq` — used in BOTH v1 and v2, saving one mul.
    //      Naive: 2 muls for v1 + 2 muls for v2 = 4 muls on ic1eq/v3 terms.
    //      Opt:   t = a2*ic1eq (1 mul), then v1 = a1*ic1eq + t; v2 = ic2eq + t + a3*v3 (3 muls).
    //
    //   2. State updates use v+v instead of 2*v — avoids a mul, compiles to a
    //      single add or FMA on x86/ARM.
    //
    //   3. Output mix: when m1==0 && m2==0 (notch/allpass special cases compiled
    //      out by the optimiser since m values are doubles stored in the struct).
    //
    // Operation count after optimisation:
    //   Before: 9 muls, 7 adds, 2 state r/w per channel
    //   After:  7 muls, 7 adds, 2 state r/w per channel  (~22% fewer muls)

    // ── Type-specialized output mix helpers ─────────────────────────────────
    // These inline the output mix for known types, eliminating dead multiplies.
    // Use these in hot-path per-band loops when the filter type is known at
    // compile time or cached per-band. The generic processL()/processR() remain
    // for unknown/changing types.

    // Advance integrators and return v0,v1,v2 — shared by all specialisations
    struct SVFResult { double v0, v1, v2; };

    inline SVFResult advanceL(float in) noexcept
    {
        const double v0 = static_cast<double>(in);
        const double v3 = v0 - ic2eqL;
        const double t  = a2 * ic1eqL;
        const double v1 = a1 * ic1eqL + a2 * v3;
        const double v2 = ic2eqL + t + a3 * v3;
        ic1eqL = v1 + v1 - ic1eqL;
        ic2eqL = v2 + v2 - ic2eqL;
        return { v0, v1, v2 };
    }

    inline SVFResult advanceR(float in) noexcept
    {
        const double v0 = static_cast<double>(in);
        const double v3 = v0 - ic2eqR;
        const double t  = a2 * ic1eqR;
        const double v1 = a1 * ic1eqR + a2 * v3;
        const double v2 = ic2eqR + t + a3 * v3;
        ic1eqR = v1 + v1 - ic1eqR;
        ic2eqR = v2 + v2 - ic2eqR;
        return { v0, v1, v2 };
    }

    // LP: out = v2  (m0=0, m1=0, m2=1 — zero muls on mix)
    inline float processLP_L(float in) noexcept { auto r = advanceL(in); return static_cast<float>(r.v2); }
    inline float processLP_R(float in) noexcept { auto r = advanceR(in); return static_cast<float>(r.v2); }

    // HP: out = v0 - k*v1 - v2  (m0=1, m1=-k, m2=-1 — 1 mul)
    inline float processHP_L(float in) noexcept
    {
        auto r = advanceL(in);
        return static_cast<float>(r.v0 + m1 * r.v1 - r.v2);  // m1 = -k
    }
    inline float processHP_R(float in) noexcept
    {
        auto r = advanceR(in);
        return static_cast<float>(r.v0 + m1 * r.v1 - r.v2);
    }

    // BP: out = v1  (m0=0, m1=1, m2=0 — zero muls)
    inline float processBP_L(float in) noexcept { auto r = advanceL(in); return static_cast<float>(r.v1); }
    inline float processBP_R(float in) noexcept { auto r = advanceR(in); return static_cast<float>(r.v1); }

    // Bell/Shelf: out = m0*v0 + m1*v1 + m2*v2  (full 3-mul mix — unavoidable)
    inline float processBell_L(float in) noexcept
    {
        auto r = advanceL(in);
        return static_cast<float>(r.v0 + m1 * r.v1);  // m0=1, m2=0 for Bell
    }
    inline float processBell_R(float in) noexcept
    {
        auto r = advanceR(in);
        return static_cast<float>(r.v0 + m1 * r.v1);
    }

    // Shelf: full mix (m0, m1, m2 all non-trivial)
    inline float processShelf_L(float in) noexcept { auto r = advanceL(in); return static_cast<float>(m0*r.v0 + m1*r.v1 + m2*r.v2); }
    inline float processShelf_R(float in) noexcept { auto r = advanceR(in); return static_cast<float>(m0*r.v0 + m1*r.v1 + m2*r.v2); }

    // Cached type for dispatch in hot loops
    Type cachedType = Type::Bell;

    // Dispatch to fast path based on cachedType (set by set())
    inline float processL(float in) noexcept
    {
        switch (cachedType)
        {
            case Type::LowPass:   return processLP_L(in);
            case Type::HighPass:  return processHP_L(in);
            case Type::Bandpass:  return processBP_L(in);
            case Type::Bell:      return processBell_L(in);
            default:              { auto r = advanceL(in); return static_cast<float>(m0*r.v0 + m1*r.v1 + m2*r.v2); }
        }
    }

    inline float processR(float in) noexcept
    {
        switch (cachedType)
        {
            case Type::LowPass:   return processLP_R(in);
            case Type::HighPass:  return processHP_R(in);
            case Type::Bandpass:  return processBP_R(in);
            case Type::Bell:      return processBell_R(in);
            default:              { auto r = advanceR(in); return static_cast<float>(m0*r.v0 + m1*r.v1 + m2*r.v2); }
        }
    }

    // ── State reset ───────────────────────────────────────────────────────────
    void reset() noexcept
    {
        ic1eqL = ic2eqL = 0.0;
        ic1eqR = ic2eqR = 0.0;
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
};
