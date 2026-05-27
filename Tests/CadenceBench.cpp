/*
    CadenceBench.cpp — Variable-cadence coefficient update profiler.
    Measures actual bq.set() call reduction for PAPER.md §4.1.

    Simulates 8-band dynamic EQ processing on three signal types:
      1. Sustained sine (stable envelope → maximum batching)
      2. White noise (all transients → minimal batching)
      3. Transient burst (silence → spike → silence → spike)

    Counts coefficient updates with variable-cadence ON vs always-per-sample.

    Build & run (no JUCE):
        g++ -std=c++17 -O2 Tests/CadenceBench.cpp -o CadenceBench -I.
        ./CadenceBench
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <array>
#include "../Source/DSP/SvfBiquad.h"

static constexpr double SR = 44100.0;
static constexpr int DURATION_SAMPLES = 44100; // 1 second
static constexpr double kPi2 = 2.0 * 3.14159265358979323846;

struct DynBandSim
{
    SvfBiquad bq, scBq;
    float env = 0.0f;
    float lastGainMod = 0.0f;
    int counter = 0;
    int setCalls = 0;

    void prepare(double fc)
    {
        bq.set(SvfBiquad::Type::Bell, SR, fc, 1.0, 0.0);
        scBq.set(SvfBiquad::Type::Bandpass, SR, fc, 2.0, 0.0);
        env = 0.0f; lastGainMod = 0.0f; counter = 0; setCalls = 0;
    }

    void process(float l, float r, bool useCadence)
    {
        float rect = std::abs(scBq.processL((l + r) * 0.5f));
        float ac = 1.0f - std::exp(-1.0f / (float)(SR * 0.01));
        float rc = 1.0f - std::exp(-1.0f / (float)(SR * 0.1));
        if (rect > env) env += ac * (rect - env);
        else            env += rc * (rect - env);

        float db = 20.0f * std::log10(std::max(env, 1e-7f));
        float gain = (db > -20.0f) ? -(db + 20.0f) * 0.75f : 0.0f;

        if (useCadence)
        {
            float delta = std::abs(gain - lastGainMod);
            if (delta > 0.1f || counter++ >= 4)
            {
                bq.set(SvfBiquad::Type::Bell, SR, 1000.0, 1.0, gain);
                lastGainMod = gain;
                counter = 0;
                ++setCalls;
            }
        }
        else
        {
            // Always per-sample
            bq.set(SvfBiquad::Type::Bell, SR, 1000.0, 1.0, gain);
            lastGainMod = gain;
            ++setCalls;
        }

        bq.processL(l);
        bq.processR(r);
    }
};

// Signal generators
static float genSine(int s) { return (float)std::sin(kPi2 * 440.0 * s / SR); }

static uint32_t xorshift(uint32_t& state)
{
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}

static float genNoise(int s)
{
    static uint32_t state = 0x12345678;
    return (float)((int32_t)xorshift(state)) / (float)INT32_MAX;
}

static float genTransient(int s)
{
    // 50ms burst every 250ms (20% duty cycle)
    int cyclePos = s % (int)(SR * 0.25);
    int burstLen = (int)(SR * 0.05);
    if (cyclePos < burstLen)
        return (float)std::sin(kPi2 * 1000.0 * s / SR) * 0.9f;
    return 0.0f;
}

static void runTest(const char* name, float (*gen)(int))
{
    static const double freqs[] = {100, 300, 600, 1000, 2000, 4000, 8000, 12000};

    // Run with cadence ON
    std::array<DynBandSim, 8> bandsOn;
    for (int b = 0; b < 8; ++b) bandsOn[(size_t)b].prepare(freqs[b]);
    for (int s = 0; s < DURATION_SAMPLES; ++s)
    {
        float sig = gen(s);
        for (auto& b : bandsOn) b.process(sig, sig, true);
    }
    int totalOn = 0;
    for (auto& b : bandsOn) totalOn += b.setCalls;

    // Run with cadence OFF (always per-sample)
    std::array<DynBandSim, 8> bandsOff;
    for (int b = 0; b < 8; ++b) bandsOff[(size_t)b].prepare(freqs[b]);
    for (int s = 0; s < DURATION_SAMPLES; ++s)
    {
        float sig = gen(s);
        for (auto& b : bandsOff) b.process(sig, sig, false);
    }
    int totalOff = 0;
    for (auto& b : bandsOff) totalOff += b.setCalls;

    double savingsPct = 100.0 * (1.0 - (double)totalOn / (double)totalOff);
    std::printf("%-20s %10d %10d %10.1f%%\n", name, totalOff, totalOn, savingsPct);
}

int main()
{
    std::printf("%-20s %10s %10s %10s\n", "Signal", "PerSample", "Cadence", "Savings");
    std::printf("%-20s %10s %10s %10s\n", "------", "---------", "-------", "-------");
    runTest("Sustained sine", genSine);
    runTest("White noise", genNoise);
    runTest("Transient burst", genTransient);
    return 0;
}
