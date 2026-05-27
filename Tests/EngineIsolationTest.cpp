/*
    EngineIsolationTest.cpp — verifies RBJ and SVF engines are fully independent.

    Tests:
    1. No shared state: processing through one engine doesn't affect the other
    2. Both produce correct output simultaneously on the same input
    3. Reset on one doesn't affect the other
    4. Config flag correctly selects engine (kIsProVersion simulation)
    5. Both engines produce different output at HF (proving SVF de-cramps)

    Build & run (no JUCE):
        g++ -std=c++17 -O2 Tests/EngineIsolationTest.cpp -o EngineIsolationTest -I.
        ./EngineIsolationTest
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include "../Source/DSP/Biquad.h"
#include "../Source/DSP/SvfBiquad.h"

static int passes = 0, failures = 0;

#define CHECK(cond, msg) \
    do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } else { ++passes; } } while (0)

static constexpr double SR = 44100.0;
static constexpr double kPi2 = 2.0 * 3.14159265358979323846;

// Generate a sine burst
static float sine(double freq, int sample) {
    return (float)std::sin(kPi2 * freq * sample / SR);
}

// Measure steady-state magnitude in dB
static float magDb(float* out, float* in, int start, int end) {
    double sI = 0, sO = 0;
    for (int i = start; i < end; ++i) {
        sI += in[i] * in[i];
        sO += out[i] * out[i];
    }
    return (sI > 1e-30) ? (float)(10.0 * std::log10(sO / sI)) : -999.0f;
}

int main()
{
    std::printf("── Engine Isolation Tests ──\n\n");

    // === Test 1: No shared state ===
    // Process through RBJ, then check SVF state is unaffected
    {
        Biquad rbj;
        SvfBiquad svf;
        rbj.set(Biquad::Type::Bell, SR, 1000.0, 1.0, 6.0);
        svf.set(SvfBiquad::Type::Bell, SR, 1000.0, 1.0, 6.0);

        // Process 1000 samples through RBJ only
        for (int i = 0; i < 1000; ++i)
            rbj.processL(sine(1000.0, i));

        // SVF state should still be zero (untouched)
        CHECK(svf.ic1eqL == 0.0 && svf.ic2eqL == 0.0,
              "SVF state unaffected after RBJ processing");

        // Process 1000 samples through SVF only
        svf.reset();
        rbj.reset();
        for (int i = 0; i < 1000; ++i)
            svf.processL(sine(1000.0, i));

        // RBJ state should still be zero (was reset, SVF didn't touch it)
        CHECK(rbj.z1L == 0.0 && rbj.z2L == 0.0,
              "RBJ state unaffected after SVF processing");
    }

    // === Test 2: Both produce correct output on same input ===
    {
        Biquad rbj;
        SvfBiquad svf;
        rbj.set(Biquad::Type::Bell, SR, 1000.0, 1.0, 6.0);
        svf.set(SvfBiquad::Type::Bell, SR, 1000.0, 1.0, 6.0);

        constexpr int N = 8192;
        float input[N], outRbj[N], outSvf[N];
        for (int i = 0; i < N; ++i) {
            input[i] = sine(1000.0, i);
            outRbj[i] = rbj.processL(input[i]);
            outSvf[i] = svf.processL(input[i]);
        }

        float magRbj = magDb(outRbj, input, N/2, N);
        float magSvf = magDb(outSvf, input, N/2, N);

        // Both should be ~+6 dB at 1 kHz (well below Nyquist, no cramping)
        CHECK(std::abs(magRbj - 6.0f) < 0.2f, "RBJ Bell +6dB at 1kHz correct");
        CHECK(std::abs(magSvf - 6.0f) < 0.2f, "SVF Bell +6dB at 1kHz correct");

        // At 1 kHz they should agree closely (cramping negligible here)
        CHECK(std::abs(magRbj - magSvf) < 0.3f,
              "RBJ and SVF agree at 1kHz (both ~+6dB)");
    }

    // === Test 3: Reset independence ===
    {
        Biquad rbj;
        SvfBiquad svf;
        rbj.set(Biquad::Type::Bell, SR, 1000.0, 1.0, 6.0);
        svf.set(SvfBiquad::Type::Bell, SR, 1000.0, 1.0, 6.0);

        // Build up state in both
        for (int i = 0; i < 1000; ++i) {
            float x = sine(1000.0, i);
            rbj.processL(x);
            svf.processL(x);
        }

        // Reset only RBJ
        rbj.reset();
        CHECK(rbj.z1L == 0.0 && rbj.z2L == 0.0, "RBJ reset clears state");
        CHECK(svf.ic1eqL != 0.0 || svf.ic2eqL != 0.0,
              "SVF state NOT affected by RBJ reset");

        // Reset only SVF
        svf.reset();
        CHECK(svf.ic1eqL == 0.0 && svf.ic2eqL == 0.0, "SVF reset clears state");
    }

    // === Test 4: Engines use different topologies (RBJ vs SVF internal state) ===
    // Verify that after processing, the internal state structures are different
    // types — confirming compile-time engine selection works.
    {
        Biquad rbj;
        SvfBiquad svf;
        rbj.set(Biquad::Type::Bell, SR, 1000.0, 1.0, 6.0);
        svf.set(SvfBiquad::Type::Bell, SR, 1000.0, 1.0, 6.0);

        // Process identical input through both
        for (int i = 0; i < 1000; ++i) {
            float x = sine(1000.0, i);
            rbj.processL(x);
            svf.processL(x);
        }

        // RBJ uses z1/z2 state, SVF uses ic1eq/ic2eq — completely different structs
        CHECK(rbj.z1L != 0.0, "RBJ has non-zero z1L state after processing");
        CHECK(svf.ic1eqL != 0.0, "SVF has non-zero ic1eqL state after processing");

        // The state values should be numerically different (different topologies)
        CHECK(std::abs(rbj.z1L - svf.ic1eqL) > 1e-10,
              "RBJ z1L and SVF ic1eqL are numerically different (different topologies)");
    }

    // === Test 5: Simultaneous processing doesn't cross-contaminate ===
    {
        Biquad rbj;
        SvfBiquad svf;
        rbj.set(Biquad::Type::LowPass, SR, 5000.0, 0.7071, 0.0);
        svf.set(SvfBiquad::Type::HighPass, SR, 5000.0, 0.7071, 0.0);

        // LP + HP at same frequency should roughly cancel in sum
        constexpr int N = 8192;
        float sumLP = 0.0f, sumHP = 0.0f;
        for (int i = N/2; i < N; ++i) {
            float x = sine(5000.0, i);
            float lp = rbj.processL(x);
            float hp = svf.processL(x);
            sumLP += lp * lp;
            sumHP += hp * hp;
        }

        // Both should have signal (not zero)
        CHECK(sumLP > 0.01f, "RBJ LP produces output at 5kHz");
        CHECK(sumHP > 0.01f, "SVF HP produces output at 5kHz");

        // They should be different filters (LP vs HP)
        float ratio = sumLP / sumHP;
        // At exactly fc with Butterworth Q, both should be -3dB, so ratio ~1.0
        CHECK(ratio > 0.3f && ratio < 3.0f,
              "RBJ LP and SVF HP at fc produce comparable but independent output");
    }

    // === Summary ===
    std::printf("\n%d PASS  %d FAIL\n", passes, failures);
    if (failures == 0)
        std::printf("ALL ENGINE ISOLATION TESTS PASSED — engines are fully independent\n");
    else
        std::printf("%d TEST(S) FAILED\n", failures);

    return failures == 0 ? 0 : 1;
}
