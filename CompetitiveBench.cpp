/*
    CompetitiveBench.cpp — Plugin-Agnostic Competitive EQ Benchmark Framework
    
    A standardized benchmark harness for comparing EQ implementations across
    different topologies, enabling fair comparisons for academic validation.
    
    Build (standalone, no JUCE):
        g++ -std=c++17 -O2 -I. Tests/CompetitiveBench.cpp -o CompetitiveBench
        
    Run:
        ./CompetitiveBench             # Human-readable table
        ./CompetitiveBench --csv       # Machine-readable CSV output
    
    Metrics measured:
        - CPU time (ns/sample) at 1, 8, 24 bands
        - Latency (samples)
        - HF cramping: magnitude error at fc=16kHz, Q=1, +6dB Bell
        - Memory footprint (sizeof)
    
    Reference: PAPER.md §2.3 — SVF achieves exact gain at fc while RBJ shows
    NOTE: original -5.27 dB claim was fabricated by LLM. BLT guarantees exact gain at fc for both topologies.
    
    Co-Authored-By: Oz <oz-agent@warp.dev>
*/

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <memory>
#include <string>
#include <functional>

// Pull in production DSP headers
#include "../Source/DSP/Biquad.h"
#include "../Source/DSP/SvfBiquad.h"

// =============================================================================
// BenchmarkableEQ Interface
// =============================================================================
// Abstract interface for any EQ implementation to be benchmarked.
// Implementations must be stateless between prepare() calls.

struct BenchmarkableEQ {
    virtual ~BenchmarkableEQ() = default;
    
    // Prepare for processing at given sample rate with max block size
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    
    // Configure a single band (idx 0-based)
    // gainDb ignored for HP/LP/BP filter types
    virtual void setBand(int idx, double fc, double q, double gainDb) = 0;
    
    // Process stereo audio in-place
    virtual void process(float* L, float* R, int numSamples) = 0;
    
    // Return latency in samples (0 for IIR, >0 for linear phase)
    virtual int getLatency() const = 0;
    
    // Human-readable name for reporting
    virtual const char* getName() const = 0;
    
    // Memory footprint for state + coefficients
    virtual size_t getMemoryBytes() const = 0;
    
    // Reset all filter state (prepare must have been called first)
    virtual void reset() = 0;
    
    // Number of bands currently configured
    virtual int getNumBands() const = 0;
};

// =============================================================================
// RBJ Biquad Implementation
// =============================================================================

class RBJBenchEQ : public BenchmarkableEQ {
public:
    explicit RBJBenchEQ(int numBands) : m_numBands(numBands), m_bands(numBands) {}
    
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        m_sampleRate = sampleRate;
        for (auto& b : m_bands) b.reset();
    }
    
    void setBand(int idx, double fc, double q, double gainDb) override {
        if (idx >= 0 && idx < m_numBands) {
            m_bands[(size_t)idx].set(Biquad::Type::Bell, m_sampleRate, fc, q, gainDb);
        }
    }
    
    void process(float* L, float* R, int numSamples) override {
        for (int i = 0; i < numSamples; ++i) {
            float l = L[i], r = R[i];
            for (auto& b : m_bands) {
                l = b.processL(l);
                r = b.processR(r);
            }
            L[i] = l;
            R[i] = r;
        }
    }
    
    int getLatency() const override { return 0; }
    const char* getName() const override { return "RBJ TDF-II Biquad"; }
    size_t getMemoryBytes() const override { return sizeof(*this); }
    void reset() override { for (auto& b : m_bands) b.reset(); }
    int getNumBands() const override { return m_numBands; }

private:
    int m_numBands;
    double m_sampleRate = 44100.0;
    std::vector<Biquad> m_bands;
};

// =============================================================================
// Simper SVF Implementation
// =============================================================================

class SVFBenchEQ : public BenchmarkableEQ {
public:
    explicit SVFBenchEQ(int numBands) : m_numBands(numBands), m_bands(numBands) {}
    
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        m_sampleRate = sampleRate;
        for (auto& b : m_bands) b.reset();
    }
    
    void setBand(int idx, double fc, double q, double gainDb) override {
        if (idx >= 0 && idx < m_numBands) {
            m_bands[(size_t)idx].set(SvfBiquad::Type::Bell, m_sampleRate, fc, q, gainDb);
        }
    }
    
    void process(float* L, float* R, int numSamples) override {
        for (int i = 0; i < numSamples; ++i) {
            float l = L[i], r = R[i];
            for (auto& b : m_bands) {
                l = b.processL(l);
                r = b.processR(r);
            }
            L[i] = l;
            R[i] = r;
        }
    }
    
    int getLatency() const override { return 0; }
    const char* getName() const override { return "Simper SVF (Cytomic)"; }
    size_t getMemoryBytes() const override { return sizeof(*this); }
    void reset() override { for (auto& b : m_bands) b.reset(); }
    int getNumBands() const override { return m_numBands; }

private:
    int m_numBands;
    double m_sampleRate = 44100.0;
    std::vector<SvfBiquad> m_bands;
};

// =============================================================================
// Benchmark Infrastructure
// =============================================================================

using Clock = std::chrono::steady_clock;

struct BenchmarkResult {
    std::string name;
    int numBands;
    double ns_per_sample;
    double mb_per_sec;
    double headroom_x;         // At 44.1kHz/512/50% CPU
    int latency_samples;
    size_t memory_bytes;
    double hf_cramp_error_db;  // Error vs +6dB target at 16kHz
    std::string notes;
};

std::vector<BenchmarkResult> g_results;

// Generate deterministic noise
static std::vector<float> make_noise(int n, unsigned seed = 0xDEADBEEF) {
    std::vector<float> buf(n);
    unsigned state = seed;
    for (int i = 0; i < n; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        buf[i] = ((float)(state & 0xFFFF) / 32768.0f) - 1.0f;
    }
    return buf;
}

// Generate sine wave
static std::vector<float> make_sine(int n, float freq_hz, float sr = 44100.0f) {
    std::vector<float> buf(n);
    const float twoPi = 2.0f * 3.14159265358979323846f;
    for (int i = 0; i < n; ++i) {
        buf[i] = 0.5f * std::sin(twoPi * freq_hz * (float)i / sr);
    }
    return buf;
}

// Calculate RMS of buffer
static double rms(const float* data, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; ++i) sum += (double)data[i] * (double)data[i];
    return std::sqrt(sum / (double)n);
}

// Run benchmark with warmup and median timing
static double bench_ns_per_sample(std::function<void()> fn, int total_samples,
                                   int warmup = 4, int trials = 16) {
    for (int i = 0; i < warmup; ++i) fn();
    
    std::vector<double> times(trials);
    for (int i = 0; i < trials; ++i) {
        auto t0 = Clock::now();
        fn();
        auto t1 = Clock::now();
        times[i] = std::chrono::duration<double, std::nano>(t1 - t0).count()
                   / (double)total_samples;
    }
    std::sort(times.begin(), times.end());
    return times[trials / 2]; // median
}

// =============================================================================
// HF Cramping Measurement
// =============================================================================
// Per PAPER.md §2.3: Bell +6dB, Q=1.0 at fc=16kHz, 44.1kHz sample rate
// Both RBJ and SVF produce +6.00 dB at fc. The +0.73 dB was the Nyquist reading mislabeled as fc.

static double measure_hf_cramping(BenchmarkableEQ& eq, double sr = 44100.0) {
    const double fc = 16000.0;
    const double q = 1.0;
    const double targetGainDb = 6.0;
    
    // Prepare EQ with single band Bell at 16kHz
    eq.prepare(sr, 1024);
    eq.setBand(0, fc, q, targetGainDb);
    
    // Generate test sine at fc
    constexpr int N = 8192;
    auto input = make_sine(N, (float)fc, (float)sr);
    auto output = input; // Copy for in-place
    std::vector<float> dummy(N, 0.0f);
    
    // Reset and process
    eq.reset();
    
    // Skip first 1024 samples (filter transient)
    eq.process(output.data(), dummy.data(), 1024);
    
    // Measure RMS on steady-state portion
    eq.reset();
    eq.process(output.data(), dummy.data(), N);
    
    double inRMS = rms(input.data() + 1024, N - 1024);
    double outRMS = rms(output.data() + 1024, N - 1024);
    
    double measuredGainDb = 20.0 * std::log10(outRMS / inRMS);
    double errorDb = measuredGainDb - targetGainDb;
    
    return errorDb;
}

// =============================================================================
// CPU Time Benchmark
// =============================================================================

static void benchmark_cpu_time(BenchmarkableEQ& eq, int numBands) {
    constexpr int N = 65536;
    constexpr double SR = 44100.0;
    
    auto L = make_noise(N, 0xAAAAAAAA);
    auto R = make_noise(N, 0x55555555);
    std::vector<float> outL(N), outR(N);
    
    eq.prepare(SR, N);
    
    // Configure bands at typical frequencies
    static const double freqs[] = { 80, 200, 400, 700, 1000, 1500, 2500, 4000,
                                     6000, 8000, 10000, 12000, 14000, 16000,
                                     100, 300, 600, 1200, 2000, 3500, 5000, 7000,
                                     9000, 11000 };
    for (int i = 0; i < numBands && i < 24; ++i) {
        double fc = freqs[i % 24];
        double q = 1.0 + (i % 3) * 0.5; // Q varies 1.0, 1.5, 2.0
        double gain = (i % 2 == 0) ? 3.0 : -3.0;
        eq.setBand(i, fc, q, gain);
    }
    
    auto fn = [&]() {
        std::copy(L.begin(), L.end(), outL.begin());
        std::copy(R.begin(), R.end(), outR.begin());
        eq.reset();
        eq.process(outL.data(), outR.data(), N);
    };
    
    double ns = bench_ns_per_sample(fn, N);
    
    // Calculate metrics
    const double bytes_per_sample = 4.0;
    double mb_per_sec = (bytes_per_sample / ns) * 1e3;
    
    const double block_size = 512.0;
    const double sample_rate = 44100.0;
    const double budget_ns = (block_size / sample_rate) * 1e9 * 0.5;
    const double cost_ns = ns * block_size;
    double headroom = budget_ns / cost_ns;
    
    // Measure HF cramping with single-band version
    std::unique_ptr<BenchmarkableEQ> single;
    if (std::string(eq.getName()).find("RBJ") != std::string::npos) {
        single = std::make_unique<RBJBenchEQ>(1);
    } else {
        single = std::make_unique<SVFBenchEQ>(1);
    }
    double hf_error = measure_hf_cramping(*single);
    
    char note[128];
    std::snprintf(note, sizeof(note), "%d-band stereo, 44.1kHz, 64-bit state",
                  numBands);
    
    g_results.push_back({
        eq.getName(),
        numBands,
        ns,
        mb_per_sec,
        headroom,
        eq.getLatency(),
        eq.getMemoryBytes(),
        hf_error,
        std::string(note)
    });
}

// =============================================================================
// Comprehensive Benchmark Suite
// =============================================================================

static void run_all_benchmarks() {
    // Test at 1, 8, 24 bands as specified
    static const int band_counts[] = { 1, 8, 24 };
    
    for (int numBands : band_counts) {
        // RBJ implementation
        {
            RBJBenchEQ rbj(numBands);
            benchmark_cpu_time(rbj, numBands);
        }
        
        // SVF implementation
        {
            SVFBenchEQ svf(numBands);
            benchmark_cpu_time(svf, numBands);
        }
    }
}

// =============================================================================
// Sweep Test for HF Cramping Characterization
// =============================================================================

static void run_hf_cramping_sweep() {
    printf("\n");
    printf("+===========================================================================+\n");
    printf("|  HF Cramping Characterization Sweep (Bell +6dB, Q=1.0, 44.1kHz)          |\n");
    printf("+===========================================================================+\n");
    printf("| Frequency |     RBJ     |     SVF     |   Ideal  |  RBJ Error | SVF Error |\n");
    printf("+-----------+-------------+-------------+----------+------------+-----------+\n");
    
    static const double test_freqs[] = { 1000, 4000, 8000, 12000, 16000, 18000, 20000 };
    constexpr double SR = 44100.0;
    constexpr double targetGainDb = 6.0;
    constexpr int N = 16384;
    
    for (double fc : test_freqs) {
        if (fc >= SR * 0.5) continue; // Skip above Nyquist
        
        // Generate sine at test frequency
        auto input = make_sine(N, (float)fc, (float)SR);
        auto outL_rbj = input;
        auto outL_svf = input;
        std::vector<float> dummy(N, 0.0f);
        
        // RBJ measurement
        Biquad rbj;
        rbj.set(Biquad::Type::Bell, SR, fc, 1.0, targetGainDb);
        for (int i = 0; i < N; ++i) outL_rbj[i] = rbj.processL(input[i]);
        
        // SVF measurement
        SvfBiquad svf;
        svf.set(SvfBiquad::Type::Bell, SR, fc, 1.0, targetGainDb);
        for (int i = 0; i < N; ++i) outL_svf[i] = svf.processL(input[i]);
        
        // Measure steady-state RMS (skip transient)
        int start = 2048;
        double inRMS = rms(input.data() + start, N - start);
        double rbjRMS = rms(outL_rbj.data() + start, N - start);
        double svfRMS = rms(outL_svf.data() + start, N - start);
        
        double rbjGain = 20.0 * std::log10(rbjRMS / inRMS);
        double svfGain = 20.0 * std::log10(svfRMS / inRMS);
        double rbjError = rbjGain - targetGainDb;
        double svfError = svfGain - targetGainDb;
        
        printf("| %7.0f Hz | %+6.2f dB   | %+6.2f dB   | %+5.1f dB | %+6.2f dB  | %+5.2f dB  |\n",
               fc, rbjGain, svfGain, targetGainDb, rbjError, svfError);
    }
    
    printf("+===========================================================================+\n");
    printf("| Note: -5.27 dB figure was incorrect; RBJ and SVF both exact at fc.         |\n");
    printf("+===========================================================================+\n");
}

// =============================================================================
// Output Formatting
// =============================================================================

static void print_table(bool csv) {
    if (csv) {
        printf("name,num_bands,ns_per_sample,mb_per_sec,headroom_x,latency_samples,"
               "memory_bytes,hf_cramp_error_db,notes\n");
        for (const auto& r : g_results) {
            printf("\"%s\",%d,%.4f,%.1f,%.1f,%d,%zu,%.2f,\"%s\"\n",
                   r.name.c_str(), r.numBands, r.ns_per_sample, r.mb_per_sec,
                   r.headroom_x, r.latency_samples, r.memory_bytes,
                   r.hf_cramp_error_db, r.notes.c_str());
        }
        return;
    }
    
    // Human-readable output
    printf("\n");
    printf("+================================================================================================+\n");
    printf("|  FreeEQ8 Competitive Benchmark Suite — EQ Implementation Comparison                           |\n");
    printf("|  44.1 kHz / 512-sample block / 50%% CPU budget / median of 16 trials                           |\n");
    printf("+================================================================================================+\n");
    printf("| %-25s | Bands | %8s | %7s | %8s | %6s | %10s |\n",
           "Implementation", "ns/samp", "MB/s", "Headroom", "Latency", "HF Error");
    printf("+---------------------------+-------+----------+---------+----------+--------+------------+\n");
    
    std::string lastName;
    for (const auto& r : g_results) {
        if (r.name != lastName) {
            if (!lastName.empty()) {
                printf("+---------------------------+-------+----------+---------+----------+--------+------------+\n");
            }
            lastName = r.name;
        }
        
        const char* hf_status = (std::abs(r.hf_cramp_error_db) < 0.5) ? "exact" :
                               (std::abs(r.hf_cramp_error_db) < 2.0) ? "mild" : "cramped";
        
        printf("| %-25s | %5d | %8.2f | %7.0f | %7.1fx | %4d s | %+5.2f dB %s |\n",
               r.name.c_str(), r.numBands, r.ns_per_sample, r.mb_per_sec,
               r.headroom_x, r.latency_samples, r.hf_cramp_error_db, hf_status);
    }
    
    printf("+================================================================================================+\n");
    
    // Summary comparison
    printf("\n");
    printf("+================================================================================================+\n");
    printf("|  Summary: SVF vs RBJ Comparison                                                               |\n");
    printf("+================================================================================================+\n");
    
    // Find 8-band results for comparison
    double rbj_8band_ns = 0, svf_8band_ns = 0;
    double rbj_hf_err = 0, svf_hf_err = 0;
    for (const auto& r : g_results) {
        if (r.numBands == 8) {
            if (r.name.find("RBJ") != std::string::npos) {
                rbj_8band_ns = r.ns_per_sample;
                rbj_hf_err = r.hf_cramp_error_db;
            } else if (r.name.find("SVF") != std::string::npos) {
                svf_8band_ns = r.ns_per_sample;
                svf_hf_err = r.hf_cramp_error_db;
            }
        }
    }
    
    if (rbj_8band_ns > 0 && svf_8band_ns > 0) {
        double overhead = svf_8band_ns / rbj_8band_ns;
        printf("| 8-band stereo CPU overhead:  SVF is %.2fx slower than RBJ                                    |\n", overhead);
        printf("| 16kHz HF cramping:           RBJ = %+.2f dB error, SVF = %+.2f dB error                       |\n",
               rbj_hf_err, svf_hf_err);
        printf("| Recommendation:              SVF for HF accuracy, RBJ for minimal CPU                        |\n");
    }
    
    printf("+================================================================================================+\n");
    printf("\n");
    printf("Memory footprint:\n");
    for (const auto& r : g_results) {
        if (r.numBands == 8) {
            printf("  %s (%d bands): %zu bytes\n", r.name.c_str(), r.numBands, r.memory_bytes);
        }
    }
    printf("\n");
}

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char** argv) {
    bool csv = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--csv") == 0) csv = true;
    }
    
    if (!csv) {
        printf("FreeEQ8 Competitive Benchmark Suite — measuring EQ implementations...\n");
        std::fflush(stdout);
    }
    
    run_all_benchmarks();
    print_table(csv);
    
    if (!csv) {
        run_hf_cramping_sweep();
    }
    
    return 0;
}
