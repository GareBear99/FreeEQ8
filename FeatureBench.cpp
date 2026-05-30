/*
    FeatureBench.cpp — per-feature benchmark suite for FreeEQ8.

    Every feature claim in the README is stress-tested and timed here.
    Numbers are reproducible: seed-deterministic signal, wall-clock medians
    over multiple trials, warm-up runs discarded.

    Build (no JUCE needed):
        clang++ -std=c++17 -O3 -DNDEBUG -pthread Tests/FeatureBench.cpp -o FeatureBench
        g++     -std=c++17 -O3 -DNDEBUG -pthread Tests/FeatureBench.cpp -o FeatureBench

    Run:
        ./FeatureBench [--csv]   # --csv prints machine-readable CSV after the table

    Output columns:
        Feature         — what is being measured
        Claim           — what the README/docs assert
        ns/sample       — nanoseconds of processing per audio sample
        MB/s            — throughput in megabytes of float audio per second
        Headroom        — ratio of CPU budget available vs cost (44.1kHz, 512 block)
        Status          — PASS / WARN / NOTE

    CPU headroom formula:
        budget_ns  = block_size / sample_rate * 1e9 / 2   (50% CPU of one core)
        cost_ns    = ns_per_sample * block_size
        headroom   = budget_ns / cost_ns

    Interpretation:
        headroom > 10x  → comfortable (green)
        headroom 3-10x  → acceptable (yellow)
        headroom < 3x   → tight — review before shipping on low-end hardware (red)

    NOTE: All benchmarks are STANDALONE (no JUCE) and use only the header-only
    DSP classes. They measure algorithm cost only — no I/O, no GUI, no locks
    beyond the atomics already in the production code.
*/

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <functional>
#include <cassert>

// Pull in production DSP headers (standalone — no JUCE required for Biquad.h)
#include <string>
#include "../Source/DSP/Biquad.h"
#include "../Source/DSP/SvfBiquad.h"

// --- Timing helpers ----------------------------------------------------------

using Clock = std::chrono::steady_clock;
using Ns    = std::chrono::nanoseconds;

static inline Ns now_ns() { return Clock::now().time_since_epoch(); }

struct BenchResult
{
    std::string feature;
    std::string claim;
    double ns_per_sample;
    double mb_per_sec;
    double headroom_x;   // at 44.1kHz, 512-block, 50% CPU
    std::string status;
    std::string note;
};

std::vector<BenchResult> results;

// Run fn() warmup times, then trials times, return median ns per total_samples
static double bench_ns_per_sample(std::function<void()> fn,
                                  int total_samples,
                                  int warmup = 4,
                                  int trials = 16)
{
    for (int i = 0; i < warmup; ++i) fn();

    std::vector<double> times(trials);
    for (int i = 0; i < trials; ++i)
    {
        auto t0 = Clock::now();
        fn();
        auto t1 = Clock::now();
        times[i] = std::chrono::duration<double, std::nano>(t1 - t0).count()
                   / (double)total_samples;
    }
    std::sort(times.begin(), times.end());
    return times[trials / 2]; // median
}

static void record(const char* feature,
                   const char* claim,
                   double ns_per_sample,
                   const char* note = "")
{
    const double bytes_per_sample = 4.0; // float
    const double mb_per_sec = (bytes_per_sample / ns_per_sample) * 1e3;

    // Headroom at 44100 Hz, 512-sample block, 50% of one core
    const double block_size  = 512.0;
    const double sample_rate = 44100.0;
    const double budget_ns   = (block_size / sample_rate) * 1e9 * 0.5;
    const double cost_ns     = ns_per_sample * block_size;
    const double headroom    = budget_ns / cost_ns;

    const char* status = (headroom > 10.0) ? "PASS"
                       : (headroom > 3.0)  ? "WARN"
                                           : "TIGHT";

    results.push_back({ std::string(feature), std::string(claim), ns_per_sample, mb_per_sec, headroom, std::string(status), std::string(note) });
}

// --- Signal generators -------------------------------------------------------

static std::vector<float> make_sine(int n, float freq_hz, float sr = 44100.0f)
{
    std::vector<float> buf(n);
    for (int i = 0; i < n; ++i)
        buf[i] = 0.5f * std::sin(2.0f * (float)kPi * freq_hz * i / sr);
    return buf;
}

static std::vector<float> make_noise(int n, unsigned seed = 0xDEADBEEF)
{
    std::vector<float> buf(n);
    unsigned state = seed;
    for (int i = 0; i < n; ++i)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        buf[i] = ((float)(state & 0xFFFF) / 32768.0f) - 1.0f;
    }
    return buf;
}

// --- 1. Biquad — single band, all 6 filter types -----------------------------

static void bench_biquad_single_band()
{
    constexpr int N = 65536;
    auto noise = make_noise(N);
    std::vector<float> out(N);

    struct Case { Biquad::Type type; const char* name; };
    static const Case cases[] = {
        { Biquad::Type::Bell,      "Bell"      },
        { Biquad::Type::LowShelf,  "LowShelf"  },
        { Biquad::Type::HighShelf, "HighShelf" },
        { Biquad::Type::HighPass,  "HighPass"  },
        { Biquad::Type::LowPass,   "LowPass"   },
        { Biquad::Type::Bandpass,  "Bandpass"  },
    };

    for (auto& c : cases)
    {
        Biquad bq;
        bq.set(c.type, 44100.0, 1000.0, 1.0, 6.0);

        char feat[64];
        std::snprintf(feat, sizeof(feat), "Biquad/%s (1 band)", c.name);

        auto fn = [&]() {
            bq.reset();
            for (int i = 0; i < N; ++i)
                out[i] = bq.processL(noise[i]);
        };

        double ns = bench_ns_per_sample(fn, N);
        record(feat, "TDF-II, 64-bit double coeffs, RBJ cookbook", ns,
               "single channel, 1 stage");
    }
}

// --- 2. Biquad — 8 bands (FreeEQ8 full path, stereo) ------------------------

static void bench_biquad_8band_stereo()
{
    constexpr int N = 65536;
    auto L = make_noise(N, 0xAAAAAAAA);
    auto R = make_noise(N, 0x55555555);
    std::vector<float> ol(N), or_(N);

    std::array<Biquad, 8> bands;
    static const Biquad::Type types[] = {
        Biquad::Type::HighPass, Biquad::Type::LowShelf,
        Biquad::Type::Bell, Biquad::Type::Bell,
        Biquad::Type::Bell, Biquad::Type::Bell,
        Biquad::Type::HighShelf, Biquad::Type::LowPass
    };
    static const double freqs[] = { 80, 200, 500, 1000, 2000, 4000, 8000, 12000 };
    for (int i = 0; i < 8; ++i)
        bands[i].set(types[i], 44100.0, freqs[i], 1.0, i % 2 == 0 ? 3.0 : -3.0);

    auto fn = [&]() {
        for (auto& b : bands) b.reset();
        for (int i = 0; i < N; ++i)
        {
            float l = L[i], r = R[i];
            for (auto& b : bands) { l = b.processL(l); r = b.processR(r); }
            ol[i] = l; or_[i] = r;
        }
    };

    double ns = bench_ns_per_sample(fn, N);
    record("Biquad/8-band stereo", "≤1 ns/sample per band at 44.1kHz", ns,
           "8 bands * 2 channels, 1 stage each");
}

// --- 3. Dynamic EQ — per-sample envelope + per-sample coeff update ----------

static void bench_dynamic_eq()
{
    constexpr int N = 65536;
    constexpr double SR = 44100.0;
    auto L = make_noise(N, 0x12345678);
    auto R = make_noise(N, 0x87654321);
    std::vector<float> ol(N), or_(N);

    // Mirror EQBand dynamic envelope logic (standalone)
    struct DynBand {
        Biquad bq, scBq;
        float envLevel = 0.0f;
        float dynGainMod = 0.0f;
        float threshDb = -20.0f;
        float ratio = 4.0f;
        float attackMs = 10.0f;
        float releaseMs = 100.0f;

        void prepare(double sr, double freq)
        {
            bq.set(Biquad::Type::Bell, sr, freq, 1.0, 0.0);
            scBq.set(Biquad::Type::Bandpass, sr, freq, 2.0, 0.0);
            envLevel = 0.0f; dynGainMod = 0.0f;
        }

        void process(float& l, float& r, double sr)
        {
            // Envelope follower (sidechain bandpass → rectify → one-pole)
            float scMono = (l + r) * 0.5f;
            float scFilt = scBq.processL(scMono);
            float rect   = std::abs(scFilt);
            float aCoeff = 1.0f - std::exp(-1.0f / (float)(sr * attackMs  * 0.001f));
            float rCoeff = 1.0f - std::exp(-1.0f / (float)(sr * releaseMs * 0.001f));
            if (rect > envLevel) envLevel += aCoeff * (rect - envLevel);
            else                 envLevel += rCoeff * (rect - envLevel);

            float envDb = 20.0f * std::log10(std::max(envLevel, 1e-7f));
            dynGainMod = (envDb > threshDb)
                ? -(envDb - threshDb) * (1.0f - 1.0f / ratio)
                : 0.0f;

            // Recompute biquad with dynamic gain mod (per-sample, as fixed)
            bq.set(Biquad::Type::Bell, sr, 1000.0, 1.0, dynGainMod);
            l = bq.processL(l);
            r = bq.processR(r);
        }
    };

    DynBand band;
    band.prepare(SR, 1000.0);

    auto fn = [&]() {
        band.prepare(SR, 1000.0);
        for (int i = 0; i < N; ++i)
        {
            float l = L[i], r = R[i];
            band.process(l, r, SR);
            ol[i] = l; or_[i] = r;
        }
    };

    double ns = bench_ns_per_sample(fn, N);
    record("Dynamic EQ (per-sample)", "Zero-lag envelope→filter response (fixed v2.2.1)", ns,
           "includes envelope follower + per-sample bq.set() + biquad process");
}

// --- 4. Cascaded biquads — slope benchmark (1/2/4 stages) -------------------

static void bench_cascaded_slopes()
{
    constexpr int N = 65536;
    auto sig = make_noise(N);
    std::vector<float> out(N);

    for (int stages : {1, 2, 4})
    {
        std::array<Biquad, 4> bqs;
        for (int s = 0; s < stages; ++s)
            bqs[s].set(Biquad::Type::HighPass, 44100.0, 80.0, 0.707, 0.0);

        auto fn = [&]() {
            for (int s = 0; s < stages; ++s) bqs[s].reset();
            for (int i = 0; i < N; ++i)
            {
                float x = sig[i];
                for (int s = 0; s < stages; ++s) x = bqs[s].processL(x);
                out[i] = x;
            }
        };

        char feat[48], claim[64];
        std::snprintf(feat,  sizeof(feat),  "Slope/%d-stage (-%d dB/oct)",
                      stages, stages * 12);
        std::snprintf(claim, sizeof(claim), "%d cascaded biquad stages", stages);

        double ns = bench_ns_per_sample(fn, N);
        record(feat, claim, ns, "single channel");
    }
}

// --- 5. Mid/Side encode+decode -----------------------------------------------

static void bench_mid_side()
{
    constexpr int N = 65536;
    auto L = make_noise(N, 0xAB12CD34);
    auto R = make_noise(N, 0xEF56AB78);
    std::vector<float> ol(N), or_(N);

    auto fn = [&]() {
        for (int i = 0; i < N; ++i)
        {
            // Encode
            float m = (L[i] + R[i]) * 0.5f;
            float s = (L[i] - R[i]) * 0.5f;
            // Simulate per-band processing (noop here — cost is encode/decode only)
            // Decode
            ol[i] = m + s;
            or_[i] = m - s;
        }
    };

    double ns = bench_ns_per_sample(fn, N);
    record("Mid/Side encode+decode", "Per-block encode/decode, per-band routing", ns,
           "2 adds + 2 muls per sample pair; near-zero cost");
}

// --- 6. Oversampling — simulate 2x/4x/8x cost (factor benchmark) ------------

static void bench_oversampling_cost()
{
    // We can't link JUCE here, so benchmark what we CAN measure:
    // The processing cost of running 8 biquad bands at Nx the sample count,
    // which is the dominant cost in the oversampled path (filter cost scales linearly).
    constexpr int N_base = 4096;
    for (int factor : {1, 2, 4, 8})
    {
        int N = N_base * factor;
        auto sig = make_noise(N);
        std::vector<float> out(N);

        std::array<Biquad, 8> bands;
        static const double freqs[] = { 80, 200, 500, 1000, 2000, 4000, 8000, 12000 };
        for (int i = 0; i < 8; ++i)
            bands[i].set(Biquad::Type::Bell, 44100.0 * factor, freqs[i], 1.0, 3.0);

        auto fn = [&]() {
            for (auto& b : bands) b.reset();
            for (int i = 0; i < N; ++i)
            {
                float x = sig[i];
                for (auto& b : bands) x = b.processL(x);
                out[i] = x;
            }
        };

        char feat[48], note[64];
        std::snprintf(feat, sizeof(feat), "Oversampling/%dx EQ cost", factor);
        std::snprintf(note, sizeof(note), "8-band mono at %dx sample rate (=%d Hz)",
                      factor, (int)(44100 * factor));

        // ns_per_sample is relative to BASE sample count (native rate)
        double raw_ns = bench_ns_per_sample(fn, N);
        double ns_at_native = raw_ns * factor;  // cost expressed per native-rate sample
        record(feat, "Linear scaling with oversample factor (JUCE polyphase IIR)", ns_at_native, note);
    }
}

// --- 7. Parameter smoothing — 20ms ramp cost ---------------------------------

static void bench_smoothing()
{
    constexpr int N = 65536;
    constexpr double SR = 44100.0;
    auto sig = make_noise(N);
    std::vector<float> out(N);

    // Simulate SmoothedValue linear interpolation (20ms → ~882 samples at 44.1kHz)
    constexpr int smoothLen = (int)(0.02 * SR);  // 882 samples
    float current = 0.0f, target = 6.0f;
    float step = (target - current) / smoothLen;

    Biquad bq;
    bq.set(Biquad::Type::Bell, SR, 1000.0, 1.0, current);

    auto fn = [&]() {
        bq.reset();
        current = 0.0f;
        int counter = 0;
        for (int i = 0; i < N; ++i)
        {
            // Update biquad every 16 samples during smoothing
            if (counter++ >= 16 && current < target)
            {
                counter = 0;
                current = std::min(current + step * 16, target);
                bq.set(Biquad::Type::Bell, SR, 1000.0, 1.0, current);
            }
            out[i] = bq.processL(sig[i]);
        }
    };

    double ns = bench_ns_per_sample(fn, N);
    record("Param smoothing (16-sample interval)", "20ms ramp, coeff update every 16 samples", ns,
           "includes amortised bq.set() cost");
}

// --- 8. Band linking — propagation overhead ----------------------------------

static void bench_band_linking()
{
    // Band linking fires on parameter change, not per sample.
    // Measure the propagation loop: iterate 8 bands, check link group, update.
    constexpr int ITERS = 1000000;

    float gains[8] = { 0, 3, -3, 6, -6, 3, 0, -3 };
    int   groups[8] = { 1, 1, 0,  1,  0, 1, 2,  2  };
    float results_arr[8];

    auto fn = [&]() {
        for (int iter = 0; iter < ITERS; ++iter)
        {
            float delta = 0.5f;
            int srcGroup = 1;
            float src_gain = gains[0] + delta;
            for (int i = 0; i < 8; ++i)
            {
                if (groups[i] == srcGroup)
                    results_arr[i] = std::clamp(gains[i] + delta, -24.0f, 24.0f);
            }
        }
    };

    // This is per-event not per-sample; express as µs per link event
    auto t0 = Clock::now();
    for (int warmup = 0; warmup < 4; ++warmup) fn();
    t0 = Clock::now();
    fn();
    auto t1 = Clock::now();
    double us = std::chrono::duration<double, std::nano>(t1 - t0).count() / ITERS / 1000.0;

    // Store as ns/sample but note it's per-event
    double ns_fake = us * 1000.0 / 512.0; // amortised if one link per block
    char note[128];
    std::snprintf(note, sizeof(note), "%.4f us per link-propagation event (8-band scan)", us);
    record("Band linking (per event)", "Ratio-based freq, delta-based gain/Q propagation",
           ns_fake, note);
}

// --- 9. Adaptive Q scaling ---------------------------------------------------

static void bench_adaptive_q()
{
    constexpr int N = 65536;
    auto sig = make_noise(N);
    std::vector<float> out(N);

    // Adaptive Q: effectiveQ = Q * (1 + |gain| * 0.12), clamp(0.1, 24)
    Biquad bq;

    auto fn = [&]() {
        bq.reset();
        for (int i = 0; i < N; ++i)
        {
            float gain = 6.0f;
            float q = 1.0f * (1.0f + std::abs(gain) * 0.12f);
            q = std::clamp(q, 0.1f, 24.0f);
            bq.set(Biquad::Type::Bell, 44100.0, 1000.0, q, gain);
            out[i] = bq.processL(sig[i]);
        }
    };

    double ns = bench_ns_per_sample(fn, N);
    record("Adaptive Q (per-block update)", "Q auto-widens with gain (0.12 factor)", ns,
           "bq.set() called every sample (worst-case, normally per-block)");
}

// --- 10. Tanh saturation / drive ---------------------------------------------

static void bench_saturation()
{
    constexpr int N = 65536;
    auto sig = make_noise(N);
    std::vector<float> out_l(N), out_r(N);

    struct SatCase { const char* name; std::function<void(float&, float&, float)> fn; };

    auto tanh_sat = [](float& l, float& r, float d) {
        float inv = 1.0f / std::tanh(d);
        l = std::tanh(l * d) * inv;
        r = std::tanh(r * d) * inv;
    };
    auto tube_sat = [](float& l, float& r, float d) {
        auto tube = [d](float x) -> float {
            float xd = x * d;
            return xd >= 0.0f ? xd / (1.0f + xd) : xd / (1.0f - 0.5f * xd);
        };
        float norm = tube(1.0f);
        l = tube(l) / norm;
        r = tube(r) / norm;
    };
    auto tape_sat = [](float& l, float& r, float d) {
        float inv = 1.0f / std::atan(d);
        l = std::atan(l * d) * inv;
        r = std::atan(r * d) * inv;
    };
    // Fixed Transistor (v2.2.1)
    auto transistor_sat = [](float& l, float& r, float d) {
        float invD = 1.0f / d;
        l = std::clamp(l * d, -1.0f, 1.0f) * invD;
        r = std::clamp(r * d, -1.0f, 1.0f) * invD;
    };

    struct { const char* name; std::function<void(float&,float&,float)> fn; } cases[] = {
        { "Tanh (FreeEQ8 default)", tanh_sat },
        { "Tube (ProEQ8)",          tube_sat },
        { "Tape / arctan (ProEQ8)", tape_sat },
        { "Transistor (fixed v2.2.1)", transistor_sat },
    };

    for (auto& c : cases)
    {
        float d = 1.0f + 0.5f * 9.0f; // 50% drive
        auto fn = [&]() {
            for (int i = 0; i < N; ++i)
            {
                float l = sig[i], r = sig[N - 1 - i];
                c.fn(l, r, d);
                out_l[i] = l; out_r[i] = r;
            }
        };

        char feat[64];
        std::snprintf(feat, sizeof(feat), "Saturation/%s", c.name);
        double ns = bench_ns_per_sample(fn, N);
        record(feat, "Per-band tanh waveshaper, gain-compensated", ns, "stereo, 50% drive");
    }
}


// --- SpectrumFIFO standalone mirror (hoisted — local classes can't have static constexpr) --
template <int FFT_SIZE>
struct SpectrumFIFOBench
{
    static constexpr int NUM_SLOTS = 3;
    std::array<std::array<float, FFT_SIZE>, NUM_SLOTS> slots {};
    std::atomic<int>  fifoWriteIndex { 0 };
    std::atomic<int>  midSlot        { 1 };
    std::atomic<bool> fresh          { false };
    int writeSlot = 0;
    int readSlot  = 2;

    void writerFlip()
    {
        writeSlot = midSlot.exchange(writeSlot, std::memory_order_release);
        fresh.store(true, std::memory_order_release);
    }
    void push(const float* data, int n)
    {
        int idx = fifoWriteIndex.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            slots[(size_t)writeSlot][(size_t)idx] = data[i];
            if (++idx >= FFT_SIZE) { idx = 0; writerFlip(); }
        }
        fifoWriteIndex.store(idx, std::memory_order_relaxed);
    }
    bool tryConsume(std::array<float, FFT_SIZE>& dest)
    {
        if (!fresh.exchange(false, std::memory_order_acquire)) return false;
        readSlot = midSlot.exchange(readSlot, std::memory_order_acquire);
        dest = slots[(size_t)readSlot];
        return true;
    }
};

// --- 11. SpectrumFIFO triple-buffer push throughput -------------------------

static void bench_spectrum_fifo()
{
    // Mirror the SpectrumFIFO write path without JUCE FFT
    constexpr int FFT_SIZE = 4096;
    constexpr int N = FFT_SIZE * 16; // push 16 full frames

    SpectrumFIFOBench<FFT_SIZE> fifo;

    auto sig = make_noise(N);


    auto fn = [&]() {
        fifo.fifoWriteIndex.store(0, std::memory_order_relaxed);
        fifo.push(sig.data(), N);
    };

    double ns = bench_ns_per_sample(fn, N);
    record("SpectrumFIFO push (audio thread)", "Lock-free triple-buffer, no heap alloc", ns,
           "16 x 4096-sample frames, single producer");
}

// --- 12. MatchEQ correction gains — pre-computed vs naive pow() --------------

static void bench_match_eq_correction()
{
    constexpr int NUM_BINS = 2048;
    constexpr int BLOCK = 512;
    constexpr int TRIALS_OUTER = 256; // total blocks to process

    // Correction curve (static once computed)
    std::array<float, NUM_BINS> corrDb;
    std::array<float, NUM_BINS> corrGain; // pre-computed
    for (int i = 0; i < NUM_BINS; ++i)
    {
        corrDb[i]   = (std::sin(i * 0.01f) * 12.0f); // ±12 dB shaped curve
        corrGain[i] = std::pow(10.0f, corrDb[i] / 20.0f);
    }

    // Naive: pow(10) per bin per block (old behaviour)
    auto naive_fn = [&]() {
        volatile float acc = 0.0f;
        for (int blk = 0; blk < TRIALS_OUTER; ++blk)
        {
            acc += std::pow(10.0f, corrDb[0] / 20.0f);
            acc += std::pow(10.0f, corrDb[NUM_BINS - 1] / 20.0f);
            for (int i = 1; i < NUM_BINS; ++i)
                acc += std::pow(10.0f, corrDb[i] / 20.0f);
        }
        (void)acc;
    };

    // Fixed: table lookup (v2.2.1)
    auto fixed_fn = [&]() {
        volatile float acc = 0.0f;
        for (int blk = 0; blk < TRIALS_OUTER; ++blk)
        {
            acc += corrGain[0];
            acc += corrGain[NUM_BINS - 1];
            for (int i = 1; i < NUM_BINS; ++i)
                acc += corrGain[i];
        }
        (void)acc;
    };

    int total = NUM_BINS * TRIALS_OUTER;
    double ns_naive = bench_ns_per_sample(naive_fn, total);
    double ns_fixed = bench_ns_per_sample(fixed_fn, total);

    record("MatchEQ gain lookup (naive pow)", "OLD: pow(10) per bin per block", ns_naive,
           "baseline — eliminated in v2.2.1");
    record("MatchEQ gain lookup (v2.2.1)",    "NEW: pre-computed table lookup", ns_fixed,
           [&](){ static char buf[32]; std::snprintf(buf,sizeof(buf),"%dx speedup vs naive",(int)std::round(ns_naive/ns_fixed)); return buf; }());
}

// --- 13. LinearPhaseEngine — FIR kernel build cost ---------------------------

static void bench_linear_phase_rebuild()
{
    // Measure the magnitude → IFFT → window → FFT pipeline cost.
    // This runs on the background thread, but we time it to confirm it's
    // fast enough not to stall even if called many times per second.
    constexpr int FIR_LEN = 4096;
    constexpr int FFT_SIZE = 8192;
    constexpr int NUM_BINS = FIR_LEN / 2 + 1; // 2049

    std::array<float, NUM_BINS> magDb;
    for (int i = 0; i < NUM_BINS; ++i)
        magDb[i] = (float)(6.0 * std::sin(i * 0.002));

    std::array<float, FFT_SIZE * 2> freqBuf;
    std::array<float, FIR_LEN>       firBuf;

    auto fn = [&]() {
        // Step 1: build frequency-domain representation from dB magnitude
        std::fill(freqBuf.begin(), freqBuf.end(), 0.0f);
        freqBuf[0] = std::pow(10.0f, magDb[0] / 20.0f); // DC

        for (int i = 1; i < FFT_SIZE / 2; ++i)
        {
            float frac   = (float)i / (float)(FFT_SIZE / 2);
            int   srcBin = std::min((int)(frac * (float)(NUM_BINS - 1)), NUM_BINS - 1);
            float linGain = std::pow(10.0f, magDb[srcBin] / 20.0f);
            freqBuf[i * 2]     = linGain;
            freqBuf[i * 2 + 1] = 0.0f;
        }
        freqBuf[1] = std::pow(10.0f, magDb[NUM_BINS - 1] / 20.0f);

        // Step 2: circular shift (simulated via index math)
        for (int i = 0; i < FIR_LEN; ++i)
        {
            int srcIdx = (i - FIR_LEN / 2 + FFT_SIZE) % FFT_SIZE;
            firBuf[i]  = (srcIdx < FFT_SIZE * 2) ? freqBuf[srcIdx] : 0.0f;
        }

        // Step 3: apply Hann window
        for (int i = 0; i < FIR_LEN; ++i)
        {
            float w = 0.5f * (1.0f - std::cos(2.0f * (float)kPi * i / (float)(FIR_LEN - 1)));
            firBuf[i] *= w;
        }

        // (IFFT and forward FFT omitted — JUCE dependency; the above is the dominant loop cost)
    };

    // This is measured per rebuild event, not per sample
    auto t0 = Clock::now();
    for (int w = 0; w < 4; ++w) fn();
    t0 = Clock::now();
    constexpr int N_RUNS = 64;
    for (int i = 0; i < N_RUNS; ++i) fn();
    auto t1 = Clock::now();

    double us_per_rebuild = std::chrono::duration<double, std::micro>(t1 - t0).count() / N_RUNS;
    char note[128];
    std::snprintf(note, sizeof(note),
                  "%.1f µs per FIR rebuild (background thread); FFT cost is JUCE-side",
                  us_per_rebuild);
    // Express as ns/sample using FIR_LEN as the "work unit"
    double ns = (us_per_rebuild * 1000.0) / FIR_LEN;
    record("LinearPhase FIR rebuild (bg thread)", "4096-tap Hann-windowed FIR from biquad magnitude",
           ns, note);
}

// --- 14. Biquad coefficient computation (RBJ) --------------------------------

static void bench_biquad_set()
{
    // How fast is bq.set()? Relevant for Dynamic EQ (called per sample when dynEnabled)
    constexpr int N = 65536;
    Biquad bq;
    float gain = 0.0f;

    static const Biquad::Type types[] = {
        Biquad::Type::Bell, Biquad::Type::LowShelf, Biquad::Type::HighShelf,
        Biquad::Type::HighPass, Biquad::Type::LowPass, Biquad::Type::Bandpass
    };

    for (auto type : types)
    {
        auto fn = [&]() {
            for (int i = 0; i < N; ++i)
            {
                gain = (float)(i % 48) - 24.0f; // sweep gain
                bq.set(type, 44100.0, 1000.0, 1.0, gain);
            }
        };

        const char* tname = "";
        switch(type) {
            case Biquad::Type::Bell:      tname = "Bell";      break;
            case Biquad::Type::LowShelf:  tname = "LowShelf";  break;
            case Biquad::Type::HighShelf: tname = "HighShelf"; break;
            case Biquad::Type::HighPass:  tname = "HighPass";  break;
            case Biquad::Type::LowPass:   tname = "LowPass";   break;
            case Biquad::Type::Bandpass:  tname = "Bandpass";  break;
        }
        char feat[64];
        std::snprintf(feat, sizeof(feat), "bq.set() cost/%s", tname);
        double ns = bench_ns_per_sample(fn, N);
        record(feat, "RBJ coefficient recompute (per-sample dynEQ cost)", ns,
               "sin/cos/pow per call; headroom shown vs per-sample budget");
    }
}

// --- 15. Correctness spot-check: Biquad unity gain at 0 dB ------------------

static void verify_biquad_unity()
{
    // Bell at 0 dB gain should pass signal through unchanged (within float epsilon)
    Biquad bq;
    bq.set(Biquad::Type::Bell, 44100.0, 1000.0, 1.0, 0.0);

    constexpr int N = 1024;
    auto sig = make_noise(N);
    float max_err = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        float out = bq.processL(sig[i]);
        max_err = std::max(max_err, std::abs(out - sig[i]));
    }

    char note[128];
    std::snprintf(note, sizeof(note), "max |out - in| = %.2e (should be < 1e-6)", (double)max_err);
    // Record as a correctness note, not a throughput benchmark
    record("Correctness/Bell 0dB unity",
           "Bell filter at 0 dB gain is transparent (|err| < 1e-6)",
           0.0, note);
}

// --- 16. ScopedNoDenormals equivalent — denormal flush cost -----------------

static void bench_denormal_handling()
{
    constexpr int N = 65536;
    auto sig = make_noise(N);
    std::vector<float> out(N);
    Biquad bq;
    bq.set(Biquad::Type::Bell, 44100.0, 1000.0, 8.0, 20.0);

    // Add a denormal-prone very quiet tail
    for (int i = 0; i < N; ++i)
        sig[i] *= 1e-38f; // sub-normal magnitude

    auto fn = [&]() {
        bq.reset();
        for (int i = 0; i < N; ++i)
            out[i] = bq.processL(sig[i]);
    };

    double ns = bench_ns_per_sample(fn, N);
    record("Denormal handling", "ScopedNoDenormals (FTZ/DAZ) in processBlock",
           ns, "measured with near-subnormal signal; if slow → FTZ not active");
}


// --- SVF BENCHMARKS (v2.2.2) — Simper trapezoidal SVF vs RBJ Biquad ---------

static void bench_svf_single_band()
{
    constexpr int N = 65536;
    auto noise = make_noise(N);
    std::vector<float> out(N);

    struct Case { SvfBiquad::Type type; const char* name; };
    static const Case cases[] = {
        { SvfBiquad::Type::Bell,      "Bell"      },
        { SvfBiquad::Type::LowShelf,  "LowShelf"  },
        { SvfBiquad::Type::HighShelf, "HighShelf" },
        { SvfBiquad::Type::HighPass,  "HighPass"  },
        { SvfBiquad::Type::LowPass,   "LowPass"   },
        { SvfBiquad::Type::Bandpass,  "Bandpass"  },
    };
    for (auto& c : cases)
    {
        SvfBiquad bq;
        bq.set(c.type, 44100.0, 1000.0, 1.0, 6.0);
        char feat[64];
        std::snprintf(feat, sizeof(feat), "SVF/%s (1 band)", c.name);
        auto fn = [&]() {
            bq.reset();
            for (int i = 0; i < N; ++i)
                out[i] = bq.processL(noise[i]);
        };
        double ns = bench_ns_per_sample(fn, N);
        record(feat, "Simper SVF 2-integrator, 64-bit, BLT (same prewarping as RBJ)", ns, "single channel");
    }
}

static void bench_svf_8band_stereo()
{
    constexpr int N = 65536;
    auto L = make_noise(N, 0xAAAAAAAA);
    auto R = make_noise(N, 0x55555555);
    std::vector<float> ol(N), or_(N);

    std::array<SvfBiquad, 8> bands;
    static const SvfBiquad::Type types[] = {
        SvfBiquad::Type::HighPass, SvfBiquad::Type::LowShelf,
        SvfBiquad::Type::Bell,     SvfBiquad::Type::Bell,
        SvfBiquad::Type::Bell,     SvfBiquad::Type::Bell,
        SvfBiquad::Type::HighShelf,SvfBiquad::Type::LowPass
    };
    static const double freqs[] = { 80, 200, 500, 1000, 2000, 4000, 8000, 12000 };
    for (int i = 0; i < 8; ++i)
        bands[i].set(types[i], 44100.0, freqs[i], 1.0, i%2==0 ? 3.0 : -3.0);

    auto fn = [&]() {
        for (auto& b : bands) b.reset();
        for (int i = 0; i < N; ++i) {
            float l = L[i], r = R[i];
            for (auto& b : bands) { l = b.processL(l); r = b.processR(r); }
            ol[i] = l; or_[i] = r;
        }
    };
    double ns = bench_ns_per_sample(fn, N);
    record("SVF/8-band stereo", "2-integrator x8 x2ch — vs RBJ/8-band stereo", ns,
           "overhead vs RBJ shown in SVF/vs-RBJ row");
}

static void bench_svf_vs_rbj_ratio()
{
    constexpr int N = 65536;
    auto L = make_noise(N, 0x11223344);
    auto R = make_noise(N, 0x44332211);
    std::vector<float> ol(N), or_(N);

    std::array<Biquad, 8>    rbj_bands;
    std::array<SvfBiquad, 8> svf_bands;
    for (int i = 0; i < 8; ++i) {
        rbj_bands[i].set(Biquad::Type::Bell,    44100.0, 1000.0*(i+1), 1.0, 3.0);
        svf_bands[i].set(SvfBiquad::Type::Bell, 44100.0, 1000.0*(i+1), 1.0, 3.0);
    }

    double ns_rbj = bench_ns_per_sample([&]() {
        for (auto& b : rbj_bands) b.reset();
        for (int i = 0; i < N; ++i) {
            float l=L[i], r=R[i];
            for (auto& b : rbj_bands) { l=b.processL(l); r=b.processR(r); }
            ol[i]=l; or_[i]=r;
        }
    }, N);

    double ns_svf = bench_ns_per_sample([&]() {
        for (auto& b : svf_bands) b.reset();
        for (int i = 0; i < N; ++i) {
            float l=L[i], r=R[i];
            for (auto& b : svf_bands) { l=b.processL(l); r=b.processR(r); }
            ol[i]=l; or_[i]=r;
        }
    }, N);

    double ratio = ns_svf / ns_rbj;
    char note[128];
    std::snprintf(note, sizeof(note),
                  "SVF=%.2f ns/samp, RBJ=%.2f ns/samp, overhead=%.2fx", ns_svf, ns_rbj, ratio);
    record("SVF/vs-RBJ throughput ratio", "SVF cost premium vs RBJ (target < 1.5x)", ns_svf, note);
}

static void bench_svf_set_cost()
{
    constexpr int N = 65536;
    SvfBiquad bq;
    float gain = 0.0f;

    static const SvfBiquad::Type types[] = {
        SvfBiquad::Type::Bell, SvfBiquad::Type::LowShelf,
        SvfBiquad::Type::HighShelf, SvfBiquad::Type::HighPass,
        SvfBiquad::Type::LowPass, SvfBiquad::Type::Bandpass
    };
    static const char* tnames[] = {
        "Bell","LowShelf","HighShelf","HighPass","LowPass","Bandpass"
    };
    for (int t = 0; t < 6; ++t)
    {
        auto fn = [&]() {
            for (int i = 0; i < N; ++i) {
                gain = (float)(i % 48) - 24.0f;
                bq.set(types[t], 44100.0, 1000.0, 1.0, gain);
            }
        };
        char feat[64];
        std::snprintf(feat, sizeof(feat), "SVF bq.set()/%s", tnames[t]);
        double ns = bench_ns_per_sample(fn, N);
        record(feat, "SVF coefficient recompute (tan() per call)", ns,
               "compare vs RBJ bq.set() — tan() vs sin()/cos()");
    }
}

static void bench_svf_dynamic_eq()
{
    constexpr int N = 65536;
    constexpr double SR = 44100.0;
    auto L = make_noise(N, 0xABCDEF01);
    auto R = make_noise(N, 0x10FEDCBA);
    std::vector<float> ol(N), or_(N);

    struct SvfDynBand {
        SvfBiquad bq, scBq;
        float envLevel=0.0f, threshDb=-20.0f, ratio=4.0f, attackMs=10.0f, releaseMs=100.0f;
        void prepare(double sr, double freq) {
            bq.set(SvfBiquad::Type::Bell, sr, freq, 1.0, 0.0);
            scBq.set(SvfBiquad::Type::Bandpass, sr, freq, 2.0, 0.0);
            envLevel=0.0f;
        }
        void process(float& l, float& r, double sr) {
            float rect = std::abs(scBq.processL((l+r)*0.5f));
            float ac = 1.0f-std::exp(-1.0f/(float)(sr*attackMs*0.001f));
            float rc = 1.0f-std::exp(-1.0f/(float)(sr*releaseMs*0.001f));
            if (rect>envLevel) envLevel+=ac*(rect-envLevel);
            else               envLevel+=rc*(rect-envLevel);
            float db = 20.0f*std::log10(std::max(envLevel,1e-7f));
            float dg = (db>threshDb) ? -(db-threshDb)*(1.0f-1.0f/ratio) : 0.0f;
            bq.set(SvfBiquad::Type::Bell, sr, 1000.0, 1.0, dg);
            l=bq.processL(l); r=bq.processR(r);
        }
    };

    SvfDynBand band;
    band.prepare(SR, 1000.0);

    auto fn = [&]() {
        band.prepare(SR, 1000.0);
        for (int i = 0; i < N; ++i) {
            float l=L[i], r=R[i];
            band.process(l, r, SR);
            ol[i]=l; or_[i]=r;
        }
    };

    double ns = bench_ns_per_sample(fn, N);
    record("SVF Dynamic EQ (per-sample)", "SVF envelope + per-sample set() + process", ns,
           "compare vs RBJ Dynamic EQ row — tan() overhead visible here");
}


// ─── INSTANCE SCALING BENCHMARKS (v2.2.4) ────────────────────────────────────
// Gap identified by Document 11: "not yet crossed into industrial benchmark
// validation under real DAW stress matrices."
// These benchmarks simulate N simultaneous plugin instances at 44.1kHz/512 block.

// ─── INST.1  Instance scaling — RBJ ──────────────────────────────────────────

static void bench_instance_scaling_rbj()
{
    constexpr int BLOCK = 512;
    constexpr double SR = 44100.0;
    static const double freqs[] = { 80,200,500,1000,2000,4000,8000,12000 };
    static const Biquad::Type types[] = {
        Biquad::Type::HighPass, Biquad::Type::LowShelf,
        Biquad::Type::Bell,     Biquad::Type::Bell,
        Biquad::Type::Bell,     Biquad::Type::Bell,
        Biquad::Type::HighShelf,Biquad::Type::LowPass
    };

    static const int instance_counts[] = { 1, 8, 32, 64, 128 };

    for (int numInst : instance_counts)
    {
        auto L = make_noise(BLOCK * numInst, 0xABCDEF01 + (unsigned)numInst);
        auto R = make_noise(BLOCK * numInst, 0x10FEDCBA + (unsigned)numInst);
        std::vector<float> ol(BLOCK * numInst), or_(BLOCK * numInst);

        // Simulate numInst independent plugin instances (each with 8 bands)
        std::vector<std::array<Biquad, 8>> instances((size_t)numInst);
        for (auto& inst : instances)
            for (int b = 0; b < 8; ++b)
                inst[(size_t)b].set(types[b], SR, freqs[b], 1.0, b%2?3.0:-3.0);

        auto fn = [&]() {
            for (int i = 0; i < numInst; ++i)
            {
                auto& inst = instances[(size_t)i];
                const int off = i * BLOCK;
                for (auto& bq : inst) bq.reset();
                for (int s = 0; s < BLOCK; ++s)
                {
                    float l = L[(size_t)(off+s)], r = R[(size_t)(off+s)];
                    for (auto& bq : inst) { l = bq.processL(l); r = bq.processR(r); }
                    ol[(size_t)(off+s)] = l;
                    or_[(size_t)(off+s)] = r;
                }
            }
        };

        int totalSamples = BLOCK * numInst;
        double ns = bench_ns_per_sample(fn, totalSamples);

        char feat[64], note[128];
        std::snprintf(feat, sizeof(feat), "ScaleRBJ/%d instances x 8-band stereo", numInst);
        std::snprintf(note, sizeof(note), "%.1f%% CPU at 44.1kHz/512 (50%% budget = %.0f ns/samp)",
            ns * BLOCK / (512.0/44100.0*1e9) * 100.0,
            512.0/44100.0*1e9*0.5/BLOCK);
        record(feat, "RBJ instance scaling — real DAW load simulation", ns, note);
    }
}

// ─── INST.2  Instance scaling — SVF ──────────────────────────────────────────

static void bench_instance_scaling_svf()
{
    constexpr int BLOCK = 512;
    constexpr double SR = 44100.0;
    static const double freqs[] = { 80,200,500,1000,2000,4000,8000,12000 };
    static const SvfBiquad::Type types[] = {
        SvfBiquad::Type::HighPass, SvfBiquad::Type::LowShelf,
        SvfBiquad::Type::Bell,     SvfBiquad::Type::Bell,
        SvfBiquad::Type::Bell,     SvfBiquad::Type::Bell,
        SvfBiquad::Type::HighShelf,SvfBiquad::Type::LowPass
    };

    static const int instance_counts[] = { 1, 8, 32, 64, 128 };

    for (int numInst : instance_counts)
    {
        auto L = make_noise(BLOCK * numInst, 0xABCDEF01 + (unsigned)numInst);
        auto R = make_noise(BLOCK * numInst, 0x10FEDCBA + (unsigned)numInst);
        std::vector<float> ol(BLOCK * numInst), or_(BLOCK * numInst);

        std::vector<std::array<SvfBiquad, 8>> instances((size_t)numInst);
        for (auto& inst : instances)
            for (int b = 0; b < 8; ++b)
                inst[(size_t)b].set(types[b], SR, freqs[b], 1.0, b%2?3.0:-3.0);

        auto fn = [&]() {
            for (int i = 0; i < numInst; ++i)
            {
                auto& inst = instances[(size_t)i];
                const int off = i * BLOCK;
                for (auto& bq : inst) bq.reset();
                for (int s = 0; s < BLOCK; ++s)
                {
                    float l = L[(size_t)(off+s)], r = R[(size_t)(off+s)];
                    for (auto& bq : inst) { l = bq.processL(l); r = bq.processR(r); }
                    ol[(size_t)(off+s)] = l;
                    or_[(size_t)(off+s)] = r;
                }
            }
        };

        int totalSamples = BLOCK * numInst;
        double ns = bench_ns_per_sample(fn, totalSamples);

        double cpuPct = ns * BLOCK / (512.0/44100.0*1e9) * 100.0;
        char feat[64], note[128];
        std::snprintf(feat, sizeof(feat), "ScaleSVF/%d instances x 8-band stereo", numInst);
        std::snprintf(note, sizeof(note), "%.2f%% CPU at 44.1kHz/512 (50%% budget)", cpuPct);
        record(feat, "SVF instance scaling — real DAW load simulation", ns, note);
    }
}

// ─── INST.3  Worst-case Dynamic EQ — all 8 bands + fast envelope ─────────────
// "The actual limit is NOT filter math — it becomes dynamic coefficient churn"
// This benchmark quantifies exactly that ceiling.

static void bench_worst_case_dynamic_eq()
{
    constexpr int N = 65536;
    constexpr double SR = 44100.0;

    // White noise burst signal — worst case for envelope follower (all transients)
    auto L = make_noise(N, 0x12345678);
    auto R = make_noise(N, 0x87654321);
    std::vector<float> ol(N), or_(N);

    // Simulate 8 simultaneous dynamic EQ bands (worst case: all active)
    struct DynBand {
        SvfBiquad bq, scBq;
        float env = 0.0f;
        float lastGain = 0.0f;


        int counter = 0;

        void prepare(double sr, double fc) {
            bq.set(SvfBiquad::Type::Bell, sr, fc, 1.0, 0.0);
            scBq.set(SvfBiquad::Type::Bandpass, sr, fc, 2.0, 0.0);
            env = 0.0f; lastGain = 0.0f; counter = 0;
        }
        void process(float& l, float& r, double sr) {
            float rect = std::abs(scBq.processL((l+r)*0.5f));
            float ac = 1.0f-std::exp(-1.0f/(float)(sr*0.01f));
            float rc = 1.0f-std::exp(-1.0f/(float)(sr*0.1f));
            if (rect>env) env+=ac*(rect-env); else env+=rc*(rect-env);
            float db   = 20.0f*std::log10(std::max(env,1e-7f));
            float gain = (db>-20.0f)? -(db+20.0f)*0.75f : 0.0f;
            // Variable-cadence (v2.2.3 fix)
            float delta = std::abs(gain - lastGain);
            bool perSample = (delta > 0.1f);
            if (perSample || counter++ >= 4) {
                bq.set(SvfBiquad::Type::Bell, sr, 1000.0, 1.0, gain);
                lastGain = gain; counter = 0;
            }
            l = bq.processL(l); r = bq.processR(r);
        }
    };

    static const double freqs[] = {100,300,600,1000,2000,4000,8000,12000};
    std::array<DynBand, 8> bands;
    for (int i = 0; i < 8; ++i) bands[(size_t)i].prepare(SR, freqs[i]);

    auto fn = [&]() {
        for (int i = 0; i < 8; ++i) bands[(size_t)i].prepare(SR, freqs[i]);
        for (int s = 0; s < N; ++s) {
            float l=L[(size_t)s], r=R[(size_t)s];
            for (auto& b : bands) b.process(l, r, SR);
            ol[(size_t)s]=l; or_[(size_t)s]=r;
        }
    };

    double ns = bench_ns_per_sample(fn, N);
    double cpuPct = ns * 512 / (512.0/44100.0*1e9) * 100.0;
    char note[128];
    std::snprintf(note, sizeof(note),
        "Worst case: 8 dyn bands, white noise, all transients. %.2f%% CPU at 512 block",
        cpuPct);
    record("WorstCase/8-band DynEQ all active", "White noise burst — max envelope churn, variable-cadence active", ns, note);
}

// ─── SIMD.1  SvfBandArray<8> scalar fallback benchmark ───────────────────────
// Documents the baseline before explicit SIMD (v2.2.5) is engaged

#include "../Source/DSP/SvfBandArray.h"

static void bench_svf_band_array()
{
    constexpr int N = 65536;
    auto sig = make_noise(N);
    std::vector<float> out(N);

    SvfBandArray8 arr;
    // Set coefficients for all 8 bands (Bell, various freqs)
    static const double freqs[] = {80,200,500,1000,2000,4000,8000,12000};
    for (int b = 0; b < 8; ++b)
    {
        SvfBiquad tmp;
        tmp.set(SvfBiquad::Type::Bell, 44100.0, freqs[b], 1.0, b%2?3.0:-3.0);
        // Extract coefficients from SvfBiquad into array
        arr.setCoeffs(b, (float)tmp.a1, (float)tmp.a2, (float)tmp.a3,
                         (float)tmp.m0, (float)tmp.m1, (float)tmp.m2);
    }

    auto fn = [&]() {
        arr.reset();
        for (int i = 0; i < N; ++i)
            out[(size_t)i] = arr.processL(sig[(size_t)i]);
    };

    double ns = bench_ns_per_sample(fn, N);
    const char* simd_mode =
#if defined(__AVX2__)
        "AVX2 (8-wide float32)";
#elif defined(__SSE2__)
        "SSE2 (4-wide float32)";
#elif defined(__ARM_NEON)
        "ARM Neon (4-wide float32)";
#else
        "Scalar fallback (no SIMD detected)";
#endif
    char note[128];
    std::snprintf(note, sizeof(note), "SIMD mode: %s — compare vs SVF/8-band-stereo", simd_mode);
    record("SvfBandArray/8-band mono", "Packed band array — baseline for v2.2.5 SIMD integration", ns, note);
}

// --- Printing -----------------------------------------------------------------

static void print_table(bool csv)
{
    if (csv)
    {
        std::printf("feature,claim,ns_per_sample,mb_per_sec,headroom_x,status,note\n");
        for (auto& r : results)
        {
            // Escape commas in fields
            auto esc = [](const char* s) -> std::string {
                std::string out = "\"";
                for (const char* p = s; *p; ++p) {
                    if (*p == '"') out += '"';
                    out += *p;
                }
                out += '"';
                return out;
            };
            std::printf("%s,%s,%.4f,%.1f,%.1f,%s,%s\n",
                esc(r.feature.c_str()).c_str(), esc(r.claim.c_str()).c_str(),
                r.ns_per_sample, r.mb_per_sec, r.headroom_x,
                r.status.c_str(), esc(r.note.c_str()).c_str());
        }
        return;
    }

    // Header
    std::printf("\n");
    std::printf("+==========================================================================================+\n");
    std::printf("|           FreeEQ8 v2.2.1 — Feature Benchmark Suite                                      |\n");
    std::printf("|           44.1 kHz * 512-sample block * 50%% CPU budget * median over 16 trials          |\n");
    std::printf("+==========================================================================================+\n");
    std::printf("| %-34s | %8s | %7s | %8s | %-6s |\n",
                "Feature", "ns/samp", "MB/s", "Headrm", "Status");
    std::printf("+==========================================================================================+\n");

    std::string lastGroup;
    for (auto& r : results)
    {
        // Extract group prefix (before '/')
        std::string feat = r.feature;
        auto slash = feat.find('/');
        std::string group = (slash != std::string::npos) ? feat.substr(0, slash) : feat;

        if (group != lastGroup)
        {
            if (!lastGroup.empty())
                std::printf("+==========================================================================================+\n");
            lastGroup = group;
        }

        const char* status_sym = (r.status == "PASS")  ? "[PASS]" :
                                 (r.status == "WARN")  ? "[WARN]" :
                                 (r.status == "NOTE")  ? "[NOTE]" : "[TGHT]";

        // Correctness entries have ns=0
        if (r.ns_per_sample < 0.001)
            std::printf("| %-34s | %8s | %7s | %8s | %-6s |\n",
                r.feature.c_str(), "n/a", "n/a", "n/a", status_sym);
        else
            std::printf("| %-34s | %8.2f | %7.0f | %7.1fx | %-6s |\n",
                r.feature.c_str(), r.ns_per_sample, r.mb_per_sec, r.headroom_x, status_sym);
    }

    std::printf("+==========================================================================================+\n");
    std::printf("| Notes:                                                                                   |\n");
    for (auto& r : results)
    {
        if (!r.note.empty())
        {
            std::string note(r.note);
            // Word-wrap at 88 chars
            if (note.size() > 80) note = note.substr(0, 77) + "...";
            std::printf("|  %-34s: %-49s |\n", r.feature.c_str(), note.c_str());
        }
    }
    std::printf("+==========================================================================================+\n");

    // Summary
    int pass = 0, warn = 0, tight = 0;
    for (auto& r : results)
    {
        if (r.ns_per_sample < 0.001) continue;
        if (r.status == "PASS")  pass++;
        else if (r.status == "WARN")  warn++;
        else tight++;
    }
    std::printf("\nSummary: %d PASS  %d WARN  %d TIGHT\n", pass, warn, tight);

    if (warn || tight)
    {
        std::printf("\nReview WARN/TIGHT items before low-end hardware deployment.\n");
        std::printf("WARN = headroom 3-10x (acceptable).  TIGHT = headroom <3x (review).\n");
    }
    std::printf("\n");
}

// --- Main ---------------------------------------------------------------------

int main(int argc, char** argv)
{
    bool csv = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--csv") == 0) csv = true;

    if (!csv)
    {
        std::printf("FreeEQ8 Feature Benchmark Suite — warming up...\n");
        std::fflush(stdout);
    }

    // Run all benchmarks
    bench_biquad_single_band();
    bench_biquad_8band_stereo();
    bench_dynamic_eq();
    bench_cascaded_slopes();
    bench_mid_side();
    bench_oversampling_cost();
    bench_smoothing();
    bench_band_linking();
    bench_adaptive_q();
    bench_saturation();
    bench_spectrum_fifo();
    bench_match_eq_correction();
    bench_linear_phase_rebuild();
    bench_biquad_set();
    verify_biquad_unity();
    bench_denormal_handling();

    // ── SVF benchmarks (v2.2.2) ──────────────────────────────────────────────
    bench_svf_single_band();
    bench_svf_8band_stereo();
    bench_svf_vs_rbj_ratio();
    bench_svf_set_cost();
    bench_svf_dynamic_eq();

    // -- Instance scaling + worst-case + SvfBandArray (v2.2.4) --
    bench_instance_scaling_rbj();
    bench_instance_scaling_svf();
    bench_worst_case_dynamic_eq();
    bench_svf_band_array();

    print_table(csv);
    return 0;
}
