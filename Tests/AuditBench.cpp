/*
    AuditBench.cpp — micro-benchmarks for the Milestone-A changes.

    Reports:
      - SpectrumFIFO triple-buffer push throughput (samples/second, ns/sample).
      - SpectrumFIFO triple-buffer consume latency (ns per processIfReady,
        excluding the FFT, which is the same before and after this change).
      - MatchEQ chunking loop overhead per 2048-sample chunk.

    The numbers are for local comparison only; treat them as lower bounds
    (release-mode clang with -O3 on an otherwise-idle machine).

    Build:
        clang++ -std=c++17 -O3 -DNDEBUG -pthread Tests/AuditBench.cpp -o AuditBench
    Run:
        ./AuditBench
*/

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>
#include <algorithm>

// Mirror of SpectrumFIFO's swap-chain algorithm, minus JUCE.
template <int FFT_SIZE>
struct TripleBuf
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

using clk = std::chrono::high_resolution_clock;
using ns  = std::chrono::nanoseconds;

static double ns_per(std::chrono::nanoseconds t, long long n)
{
    return (double)t.count() / (double)n;
}

static void bench_push_throughput()
{
    constexpr int FFT = 4096;
    constexpr int BLOCK = 512;                 // typical DAW buffer
    constexpr long long ITERS = 200000;        // 200k pushes ≈ 102M samples

    TripleBuf<FFT> pp;
    std::vector<float> block(BLOCK, 0.0f);
    for (int i = 0; i < BLOCK; ++i) block[(size_t)i] = (float)i;

    auto t0 = clk::now();
    for (long long it = 0; it < ITERS; ++it)
        pp.push(block.data(), BLOCK);
    auto t1 = clk::now();
    auto dt = std::chrono::duration_cast<ns>(t1 - t0);

    const long long samples = ITERS * BLOCK;
    std::printf("push (BLOCK=%d):  %.2f ns/sample  (%.1f Msamples/sec)\n",
                BLOCK, ns_per(dt, samples),
                (double)samples / (double)dt.count() * 1000.0);
}

static void bench_consume_latency()
{
    constexpr int FFT = 4096;
    constexpr long long ITERS = 200000;

    TripleBuf<FFT> pp;
    std::array<float, FFT> snap {};
    std::vector<float> block(FFT, 0.0f);

    // Pre-publish once per iteration via a dedicated writer loop that always
    // has fresh data available. We alternate a full-FFT push + a consume so
    // the consumer always has something to do.
    auto t0 = clk::now();
    for (long long it = 0; it < ITERS; ++it)
    {
        pp.push(block.data(), FFT);        // one wrap
        (void)pp.tryConsume(snap);
    }
    auto t1 = clk::now();
    auto dt = std::chrono::duration_cast<ns>(t1 - t0);

    // Attribute half the time to consume (crude but symmetric).
    std::printf("push-FFT + consume:  %.2f ns/iter  (wrap + drain)\n",
                ns_per(dt, ITERS));
}

static void bench_chunking_overhead()
{
    constexpr int kMaxChunk = 2048;
    constexpr long long ITERS = 10000000;

    // Measure the cost of the chunking loop itself (no actual work).
    volatile int sink = 0;
    auto t0 = clk::now();
    for (long long it = 0; it < ITERS; ++it)
    {
        const int numSamples = 4096;
        int offset = 0;
        while (offset < numSamples)
        {
            const int chunk = std::min(kMaxChunk, numSamples - offset);
            sink += chunk;
            offset += chunk;
        }
    }
    auto t1 = clk::now();
    auto dt = std::chrono::duration_cast<ns>(t1 - t0);

    std::printf("chunking loop (n=4096, cap=2048): %.2f ns/call\n",
                ns_per(dt, ITERS));
    (void)sink;
}

int main()
{
    std::printf("── Milestone-A micro-benchmarks ──\n");
    bench_push_throughput();
    bench_consume_latency();
    bench_chunking_overhead();
    return 0;
}
