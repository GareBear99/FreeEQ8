/*
    BiquadTest.cpp — standalone test for Biquad coefficient generation.
    Verifies RBJ Audio EQ Cookbook coefficients across multiple filter types
    and sample rates.

    Build & run:
        cmake -S . -B build -DFREEEQ8_BUILD_TESTS=ON
        cmake --build build --target FreeEQ8_Tests
        ./build/FreeEQ8_Tests
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <array>
#include <algorithm>

// Include the struct under test (standalone, no JUCE dependency)
#include "../Source/DSP/Biquad.h"

// ── Helpers ──────────────────────────────────────────────────────────

static constexpr double kTol = 1e-10;

static bool near(double a, double b) { return std::abs(a - b) < kTol; }

static int failures = 0;

#define CHECK(cond, msg) \
    do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } } while (0)

// ── Reference RBJ coefficient generators (must exactly mirror Biquad::set) ──
struct NormCoeffs { double b0, b1, b2, a1, a2; };

static NormCoeffs ref_bell(double sr, double freq, double Q, double gain)
{
    freq = std::clamp(freq, 10.0, sr * 0.45);
    Q    = std::clamp(Q, 0.1, 24.0);
    double A     = std::pow(10.0, gain / 40.0);
    double w0    = 2.0 * kPi * (freq / sr);
    double cosw0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * Q);
    double b0_ = 1.0 + alpha * A;
    double b1_ = -2.0 * cosw0;
    double b2_ = 1.0 - alpha * A;
    double a0_ = 1.0 + alpha / A;
    double a1_ = -2.0 * cosw0;
    double a2_ = 1.0 - alpha / A;
    return { b0_/a0_, b1_/a0_, b2_/a0_, a1_/a0_, a2_/a0_ };
}

static NormCoeffs ref_lowpass(double sr, double freq, double Q, double /*gain*/)
{
    freq = std::clamp(freq, 10.0, sr * 0.45);
    Q    = std::clamp(Q, 0.1, 24.0);
    double w0    = 2.0 * kPi * (freq / sr);
    double cosw0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * Q);
    double b0_ = (1.0 - cosw0) * 0.5;
    double b1_ = 1.0 - cosw0;
    double b2_ = (1.0 - cosw0) * 0.5;
    double a0_ = 1.0 + alpha;
    return { b0_/a0_, b1_/a0_, b2_/a0_, (-2.0*cosw0)/a0_, (1.0 - alpha)/a0_ };
}

static NormCoeffs ref_highpass(double sr, double freq, double Q, double /*gain*/)
{
    freq = std::clamp(freq, 10.0, sr * 0.45);
    Q    = std::clamp(Q, 0.1, 24.0);
    double w0    = 2.0 * kPi * (freq / sr);
    double cosw0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * Q);
    double b0_ = (1.0 + cosw0) * 0.5;
    double b1_ = -(1.0 + cosw0);
    double b2_ = (1.0 + cosw0) * 0.5;
    double a0_ = 1.0 + alpha;
    return { b0_/a0_, b1_/a0_, b2_/a0_, (-2.0*cosw0)/a0_, (1.0 - alpha)/a0_ };
}

static NormCoeffs ref_lowshelf(double sr, double freq, double Q, double gain)
{
    freq = std::clamp(freq, 10.0, sr * 0.45);
    Q    = std::clamp(Q, 0.1, 24.0);
    double A     = std::pow(10.0, gain / 40.0);
    double w0    = 2.0 * kPi * (freq / sr);
    double cosw0 = std::cos(w0);
    double sinw0 = std::sin(w0);
    double S     = std::clamp(Q / 2.0, 0.1, 4.0);
    double alphaS = sinw0/2.0 * std::sqrt((A + 1.0/A) * (1.0/S - 1.0) + 2.0);
    double b0_ =    A*((A+1) - (A-1)*cosw0 + 2*std::sqrt(A)*alphaS);
    double b1_ =  2*A*((A-1) - (A+1)*cosw0);
    double b2_ =    A*((A+1) - (A-1)*cosw0 - 2*std::sqrt(A)*alphaS);
    double a0_ =        (A+1) + (A-1)*cosw0 + 2*std::sqrt(A)*alphaS;
    double a1_ =   -2*((A-1) + (A+1)*cosw0);
    double a2_ =        (A+1) + (A-1)*cosw0 - 2*std::sqrt(A)*alphaS;
    return { b0_/a0_, b1_/a0_, b2_/a0_, a1_/a0_, a2_/a0_ };
}

static NormCoeffs ref_highshelf(double sr, double freq, double Q, double gain)
{
    freq = std::clamp(freq, 10.0, sr * 0.45);
    Q    = std::clamp(Q, 0.1, 24.0);
    double A     = std::pow(10.0, gain / 40.0);
    double w0    = 2.0 * kPi * (freq / sr);
    double cosw0 = std::cos(w0);
    double sinw0 = std::sin(w0);
    double S     = std::clamp(Q / 2.0, 0.1, 4.0);
    double alphaS = sinw0/2.0 * std::sqrt((A + 1.0/A) * (1.0/S - 1.0) + 2.0);
    double b0_ =    A*((A+1) + (A-1)*cosw0 + 2*std::sqrt(A)*alphaS);
    double b1_ = -2*A*((A-1) + (A+1)*cosw0);
    double b2_ =    A*((A+1) + (A-1)*cosw0 - 2*std::sqrt(A)*alphaS);
    double a0_ =        (A+1) - (A-1)*cosw0 + 2*std::sqrt(A)*alphaS;
    double a1_ =    2*((A-1) - (A+1)*cosw0);
    double a2_ =        (A+1) - (A-1)*cosw0 - 2*std::sqrt(A)*alphaS;
    return { b0_/a0_, b1_/a0_, b2_/a0_, a1_/a0_, a2_/a0_ };
}

static NormCoeffs ref_bandpass(double sr, double freq, double Q, double /*gain*/)
{
    freq = std::clamp(freq, 10.0, sr * 0.45);
    Q    = std::clamp(Q, 0.1, 24.0);
    double w0    = 2.0 * kPi * (freq / sr);
    double cosw0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * Q);
    double a0_ = 1.0 + alpha;
    return { alpha/a0_, 0.0, -alpha/a0_, (-2.0*cosw0)/a0_, (1.0 - alpha)/a0_ };
}

// ── Unified test runner ──

using RefFn = NormCoeffs(*)(double, double, double, double);

static void testType(Biquad::Type type, const char* name, RefFn ref,
                     double sr, double freq, double Q, double gain)
{
    Biquad bq;
    bq.set(type, sr, freq, Q, gain);
    NormCoeffs exp = ref(sr, freq, Q, gain);

    char buf[256];
    auto check = [&](double actual, double expected, const char* coeff) {
        std::snprintf(buf, sizeof(buf), "%s @ %.0fHz/%.0fHz Q=%.2f gain=%.1f — %s (got %.12g, exp %.12g)",
                      name, freq, sr, Q, gain, coeff, actual, expected);
        CHECK(near(actual, expected), buf);
    };
    check(bq.b0, exp.b0, "b0");
    check(bq.b1, exp.b1, "b1");
    check(bq.b2, exp.b2, "b2");
    check(bq.a1, exp.a1, "a1");
    check(bq.a2, exp.a2, "a2");
}

// ── Main ──

int main()
{
    static const double srs[] = { 44100.0, 48000.0, 96000.0 };

    for (double sr : srs)
    {
        testType(Biquad::Type::Bell,      "Bell",      ref_bell,      sr, 1000.0,  1.0,  6.0);
        testType(Biquad::Type::Bell,      "Bell",      ref_bell,      sr,  200.0,  0.5, -3.0);
        testType(Biquad::Type::LowShelf,  "LowShelf",  ref_lowshelf,  sr,  100.0,  0.7,  4.0);
        testType(Biquad::Type::LowShelf,  "LowShelf",  ref_lowshelf,  sr,  250.0,  1.5, -6.0);
        testType(Biquad::Type::HighShelf, "HighShelf",  ref_highshelf, sr, 8000.0,  0.7,  3.0);
        testType(Biquad::Type::HighShelf, "HighShelf",  ref_highshelf, sr, 4000.0,  1.0, -5.0);
        testType(Biquad::Type::HighPass,  "HighPass",   ref_highpass,  sr,   80.0,  0.7,  0.0);
        testType(Biquad::Type::HighPass,  "HighPass",   ref_highpass,  sr,  300.0,  1.4,  0.0);
        testType(Biquad::Type::LowPass,   "LowPass",    ref_lowpass,  sr,10000.0,  0.7,  0.0);
        testType(Biquad::Type::LowPass,   "LowPass",    ref_lowpass,  sr, 5000.0,  2.0,  0.0);
        testType(Biquad::Type::Bandpass,  "Bandpass",   ref_bandpass,  sr, 1000.0,  2.0,  0.0);
        testType(Biquad::Type::Bandpass,  "Bandpass",   ref_bandpass,  sr,  500.0,  0.5,  0.0);
    }

    // ── Sanity: process a sample, verify no NaN ──
    {
        Biquad bq;
        bq.set(Biquad::Type::Bell, 44100.0, 1000.0, 1.0, 6.0);
        float out = bq.processL(1.0f);
        CHECK(!std::isnan(out), "processL returned NaN");
        CHECK(!std::isinf(out), "processL returned Inf");
    }

    // ── Sanity: reset clears delay state ──
    {
        Biquad bq;
        bq.set(Biquad::Type::Bell, 44100.0, 1000.0, 1.0, 6.0);
        bq.processL(1.0f);
        bq.processR(0.5f);
        bq.reset();
        CHECK(bq.z1L == 0.0 && bq.z2L == 0.0 && bq.z1R == 0.0 && bq.z2R == 0.0,
              "reset() clears all delay state");
    }

    if (failures == 0)
        std::printf("ALL TESTS PASSED (6 types x 3 sample rates x 2 configs + sanity)\n");
    else
        std::printf("%d TEST(S) FAILED\n", failures);

    return failures == 0 ? 0 : 1;
}
