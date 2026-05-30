/*
    SvfTest.cpp — standalone correctness tests for SvfBiquad.
    Verifies the Simper SVF topology against known mathematical properties.

    Build & run (no JUCE required):
        g++ -std=c++17 -O2 Tests/SvfTest.cpp -o SvfTest -ISource
        ./SvfTest
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include "../Source/DSP/SvfBiquad.h"

static int failures = 0;
static int passes   = 0;

#define CHECK(cond, msg) \
    do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } else { ++passes; } } while (0)

// Process N samples and return the last output (left channel)
static float runSamples(SvfBiquad& bq, float input, int n)
{
    float out = 0.0f;
    for (int i = 0; i < n; ++i)
        out = bq.processL(input);
    return out;
}

// Measure magnitude at a frequency by running a sine through the filter
static float measureMagnitudeDb(SvfBiquad& bq, double sr, double freqHz, int numCycles = 64)
{
    bq.reset();
    const int samplesPerCycle = (int)(sr / freqHz);
    const int totalSamples = samplesPerCycle * numCycles;
    const int measureStart = totalSamples / 2; // skip transient

    double sumSqIn = 0.0, sumSqOut = 0.0;
    for (int i = 0; i < totalSamples; ++i)
    {
        const float x = (float)std::sin(2.0 * 3.14159265358979323846 * freqHz * (double)i / sr);
        const float y = bq.processL(x);
        if (i >= measureStart)
        {
            sumSqIn  += (double)x * (double)x;
            sumSqOut += (double)y * (double)y;
        }
    }
    if (sumSqIn < 1e-20) return -999.0f;
    return (float)(10.0 * std::log10(sumSqOut / sumSqIn));
}

// ── Test 1: Bell at 0 dB gain should be unity ──────────────────────────
static void test_bell_unity()
{
    SvfBiquad bq;
    bq.set(SvfBiquad::Type::Bell, 44100.0, 1000.0, 1.0, 0.0);

    float maxErr = 0.0f;
    for (int i = 0; i < 4096; ++i)
    {
        float x = (float)std::sin(2.0 * 3.14159265358979323846 * 1000.0 * i / 44100.0);
        float y = bq.processL(x);
        // Skip first 100 samples (transient)
        if (i > 100)
            maxErr = std::max(maxErr, std::abs(y - x));
    }
    CHECK(maxErr < 1e-5f, "Bell 0dB unity: max |out-in| should be < 1e-5");
}

// ── Test 2: Bell peak gain accuracy ────────────────────────────────────
static void test_bell_peak_gain()
{
    SvfBiquad bq;
    bq.set(SvfBiquad::Type::Bell, 44100.0, 1000.0, 1.0, 6.0);
    float magDb = measureMagnitudeDb(bq, 44100.0, 1000.0);
    CHECK(std::abs(magDb - 6.0f) < 0.15f, "Bell +6dB at fc: magnitude should be ~6 dB");

    bq.set(SvfBiquad::Type::Bell, 44100.0, 1000.0, 1.0, -6.0);
    magDb = measureMagnitudeDb(bq, 44100.0, 1000.0);
    CHECK(std::abs(magDb - (-6.0f)) < 0.15f, "Bell -6dB at fc: magnitude should be ~-6 dB");
}

// ── Test 3: LP -3 dB at cutoff frequency ───────────────────────────────
static void test_lp_minus3db()
{
    SvfBiquad bq;
    bq.set(SvfBiquad::Type::LowPass, 44100.0, 1000.0, 0.7071, 0.0);
    float magDb = measureMagnitudeDb(bq, 44100.0, 1000.0);
    // Butterworth Q=0.7071 → exactly -3 dB at fc
    CHECK(std::abs(magDb - (-3.0f)) < 0.3f, "LP Q=0.707 at fc: should be ~-3 dB");
}

// ── Test 4: HP -3 dB at cutoff frequency ───────────────────────────────
static void test_hp_minus3db()
{
    SvfBiquad bq;
    bq.set(SvfBiquad::Type::HighPass, 44100.0, 1000.0, 0.7071, 0.0);
    float magDb = measureMagnitudeDb(bq, 44100.0, 1000.0);
    CHECK(std::abs(magDb - (-3.0f)) < 0.3f, "HP Q=0.707 at fc: should be ~-3 dB");
}

// ── Test 5: SVF stability under audio-rate sweep ───────────────────────
static void test_stability_under_sweep()
{
    SvfBiquad bq;
    bool stable = true;
    for (int i = 0; i < 44100; ++i)
    {
        // Sweep frequency from 20 Hz to 20 kHz over 1 second
        double freq = 20.0 + (20000.0 - 20.0) * ((double)i / 44100.0);
        bq.set(SvfBiquad::Type::Bell, 44100.0, freq, 4.0, 12.0);
        float x = (float)std::sin(2.0 * 3.14159265358979323846 * 1000.0 * i / 44100.0);
        float y = bq.processL(x);
        if (std::isnan(y) || std::isinf(y) || std::abs(y) > 100.0f)
        {
            stable = false;
            break;
        }
    }
    CHECK(stable, "SVF stable under aggressive audio-rate freq sweep");
}

// ── Test 6: State reset clears integrators ─────────────────────────────
static void test_state_reset()
{
    SvfBiquad bq;
    bq.set(SvfBiquad::Type::Bell, 44100.0, 1000.0, 1.0, 6.0);
    // Process some signal to build up state
    for (int i = 0; i < 1000; ++i)
        bq.processL(1.0f);
    bq.reset();
    CHECK(bq.ic1eqL == 0.0 && bq.ic2eqL == 0.0 &&
          bq.ic1eqR == 0.0 && bq.ic2eqR == 0.0,
          "reset() clears all integrator state");
}

// ── Test 7: Bell peak gain is Q-independent ────────────────────────────
static void test_peak_gain_q_independent()
{
    // At the center frequency, Bell gain should be exactly gainDb
    // regardless of Q value
    SvfBiquad bq1, bq2;
    bq1.set(SvfBiquad::Type::Bell, 44100.0, 2000.0, 0.5, 9.0);
    bq2.set(SvfBiquad::Type::Bell, 44100.0, 2000.0, 8.0, 9.0);
    float mag1 = measureMagnitudeDb(bq1, 44100.0, 2000.0);
    float mag2 = measureMagnitudeDb(bq2, 44100.0, 2000.0);
    CHECK(std::abs(mag1 - 9.0f) < 0.2f, "Bell +9dB Q=0.5: peak gain ~9 dB at fc");
    CHECK(std::abs(mag2 - 9.0f) < 0.2f, "Bell +9dB Q=8.0: peak gain ~9 dB at fc");
    CHECK(std::abs(mag1 - mag2) < 0.3f, "Bell peak gain is Q-independent at fc");
}

// ── Test 8: High-frequency gain accuracy (BLT guarantees gain=gainDb at fc) ─
static void test_hf_gain_accuracy()
{
    // At 16 kHz / 44.1 kHz, RBJ would give ~199% Q error.
    // SVF should maintain accurate gain at fc.
    SvfBiquad bq;
    bq.set(SvfBiquad::Type::Bell, 44100.0, 16000.0, 1.0, 6.0);
    float magDb = measureMagnitudeDb(bq, 44100.0, 16000.0, 128);
    CHECK(std::abs(magDb - 6.0f) < 0.5f,
          "Bell +6dB at 16kHz: BLT guarantees exact gain at fc for both RBJ and SVF");
}

// ── Main ───────────────────────────────────────────────────────────────
int main()
{
    std::printf("── SvfBiquad Correctness Tests ──\n");

    test_bell_unity();
    test_bell_peak_gain();
    test_lp_minus3db();
    test_hp_minus3db();
    test_stability_under_sweep();
    test_state_reset();
    test_peak_gain_q_independent();
    test_hf_gain_accuracy();

    std::printf("\n%d PASS  %d FAIL\n", passes, failures);
    if (failures == 0)
        std::printf("ALL SVF TESTS PASSED\n");
    else
        std::printf("%d TEST(S) FAILED\n", failures);

    return failures == 0 ? 0 : 1;
}
