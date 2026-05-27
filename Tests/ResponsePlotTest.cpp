/*
    ResponsePlotTest.cpp — SVF vs RBJ magnitude response comparison.
    Generates CSV data for PAPER.md Figure 1.

    Sweeps a sine through Bell filters at three center frequencies
    (8 kHz, 12 kHz, 16 kHz) and measures the magnitude response at
    each frequency point. Compares:
      1. RBJ biquad @ 44.1 kHz (native)
      2. Simper SVF @ 44.1 kHz (native)
      3. RBJ biquad @ 4× oversampled (176.4 kHz effective)

    Build & run (no JUCE):
        g++ -std=c++17 -O2 Tests/ResponsePlotTest.cpp -o ResponsePlotTest -I.
        ./ResponsePlotTest > Tests/response_data.csv
*/

#include <cmath>
#include <cstdio>
#include <algorithm>
#include "../Source/DSP/Biquad.h"
#include "../Source/DSP/SvfBiquad.h"

static constexpr double kPi2 = 2.0 * 3.14159265358979323846;
static constexpr double SR = 44100.0;
static constexpr double SR_4X = 44100.0 * 4.0;

int main()
{
    // Center frequencies to test
    static const double fcs[] = { 8000.0, 12000.0, 16000.0 };
    static const char* fcLabels[] = { "8000", "12000", "16000" };

    // Test frequencies: logarithmic sweep 20 Hz to 20 kHz (200 points)
    constexpr int N = 200;
    double testFreqs[N];
    for (int i = 0; i < N; ++i)
        testFreqs[i] = 20.0 * std::pow(1000.0, (double)i / (double)(N - 1));

    // Header
    std::printf("freq_hz");
    for (int fc = 0; fc < 3; ++fc)
    {
        std::printf(",rbj_%s_dB,svf_%s_dB,rbj4x_%s_dB", fcLabels[fc], fcLabels[fc], fcLabels[fc]);
    }
    std::printf("\n");

    for (int i = 0; i < N; ++i)
    {
        double f = testFreqs[i];
        // Clamp test freq below Nyquist
        if (f >= SR * 0.49) { f = SR * 0.49; }

        std::printf("%.1f", f);

        for (int fc = 0; fc < 3; ++fc)
        {
            double centerFreq = fcs[fc];

            // 1. RBJ @ native SR
            {
                Biquad bq;
                bq.set(Biquad::Type::Bell, SR, centerFreq, 1.0, 6.0);
                bq.reset();
                int samplesPerCycle = (int)(SR / f);
                int total = samplesPerCycle * 128;
                int start = total / 2;
                double sI = 0, sO = 0;
                for (int s = 0; s < total; ++s) {
                    float x = (float)std::sin(kPi2 * f * s / SR);
                    float y = bq.processL(x);
                    if (s >= start) { sI += x*x; sO += y*y; }
                }
                double mag = (sI > 1e-30) ? 10.0 * std::log10(sO / sI) : -999.0;
                std::printf(",%.4f", mag);
            }

            // 2. SVF @ native SR
            {
                SvfBiquad svf;
                svf.set(SvfBiquad::Type::Bell, SR, centerFreq, 1.0, 6.0);
                int samplesPerCycle = (int)(SR / f);
                int total = samplesPerCycle * 128;
                int start = total / 2;
                double sI = 0, sO = 0;
                for (int s = 0; s < total; ++s) {
                    float x = (float)std::sin(kPi2 * f * s / SR);
                    float y = svf.processL(x);
                    if (s >= start) { sI += x*x; sO += y*y; }
                }
                double mag = (sI > 1e-30) ? 10.0 * std::log10(sO / sI) : -999.0;
                std::printf(",%.4f", mag);
            }

            // 3. RBJ @ 4× oversampled (simulate: run filter at 4×SR, input/output at 1×SR)
            // We approximate by running the filter at SR_4X with the same center freq
            {
                Biquad bq4x;
                bq4x.set(Biquad::Type::Bell, SR_4X, centerFreq, 1.0, 6.0);
                int samplesPerCycle = (int)(SR_4X / f);
                int total = samplesPerCycle * 128;
                int start = total / 2;
                double sI = 0, sO = 0;
                for (int s = 0; s < total; ++s) {
                    float x = (float)std::sin(kPi2 * f * s / SR_4X);
                    float y = bq4x.processL(x);
                    if (s >= start) { sI += x*x; sO += y*y; }
                }
                double mag = (sI > 1e-30) ? 10.0 * std::log10(sO / sI) : -999.0;
                std::printf(",%.4f", mag);
            }
        }
        std::printf("\n");
    }

    // Print summary to stderr
    std::fprintf(stderr, "ResponsePlotTest: %d frequency points × 3 center freqs × 3 topologies = %d measurements\n",
                 N, N * 9);
    std::fprintf(stderr, "Pipe stdout to CSV: ./ResponsePlotTest > Tests/response_data.csv\n");
    return 0;
}
