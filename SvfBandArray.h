#pragma once
/*
    SvfBandArray.h — v2.2.4
    ═══════════════════════════════════════════════════════════════

    AVX2 / SSE / ARM Neon vectorised SVF processing for N bands.

    Architecture
    ────────────
    Processes N SVF bands simultaneously by packing their state
    variables into SIMD registers. On AVX2 (256-bit), 8 float32
    lanes process all 8 bands of FreeEQ8 in a single instruction
    pass. On SSE2/Neon (128-bit), 4 lanes process 4 bands per
    instruction.

    Why this matters
    ────────────────
    Scalar SVF (current): 8 bands × 9 muls + 7 adds per sample = 72 FLOPs
    AVX2 SVF (this file):  1 SIMD pass × 8 bands per instruction  = ~72/8 = 9 effective scalar-equivalent FLOPs
    Projected speedup: 2–4× over scalar depending on CPU (FMA3, cache, throughput).

    Performance targets (projected, post-integration):
      Scalar 8-band stereo:  70.4 ns/sample (measured v2.2.2)
      AVX2   8-band stereo:  ~18–25 ns/sample (projected)
      CPU budget used:       ~0.15–0.22% (from 0.62%)

    Usage
    ─────
    SvfBandArray<8> array;
    array.setCoeffs(bandIdx, a1, a2, a3, m0, m1, m2);
    float out = array.processL(inputSample);  // processes all 8 bands, sums
    float outR = array.processR(inputSample);

    Implementation detail
    ─────────────────────
    Uses #ifdef __AVX2__ / __SSE2__ / __ARM_NEON guards so the same
    header works on all platforms. Falls back to scalar loop when no
    SIMD is available. Zero runtime branches on the hot path.

    Integration plan (v2.4.0)
    ─────────────────────────
    1. Replace EQBand's per-band scalar Biquad/SvfBiquad with SvfBandArray<kNumBands>
    2. Set coefficients from maybeUpdateCoeffs() → array.setCoeffs()
    3. Call array.processL/R() from processBlock() inner loop
    4. Benchmark: run FeatureBench SVF/8-band section, verify < 30 ns/sample

    NOTE: This file is the scaffold + portable fallback. The AVX2 hot path
    requires JUCE or the project to compile with -march=native / -mavx2 -mfma.
    See CMakeLists.txt target_compile_options for enabling this per-platform.
*/

#include <array>
#include <cmath>
#include <algorithm>

// Detect SIMD availability
#if defined(__AVX2__)
  #include <immintrin.h>
  #define FREEEQ8_HAS_AVX2 1
#elif defined(__SSE2__)
  #include <emmintrin.h>
  #include <xmmintrin.h>
  #define FREEEQ8_HAS_SSE2 1
#elif defined(__ARM_NEON)
  #include <arm_neon.h>
  #define FREEEQ8_HAS_NEON 1
#endif

template <int N>
class SvfBandArray
{
    static_assert(N > 0 && N <= 32, "SvfBandArray: N must be 1..32");

public:
    // ── Coefficient storage (32-byte aligned for AVX2 load) ──────────
    alignas(32) std::array<float, N> a1 {};
    alignas(32) std::array<float, N> a2 {};
    alignas(32) std::array<float, N> a3 {};
    alignas(32) std::array<float, N> m0 {};
    alignas(32) std::array<float, N> m1 {};
    alignas(32) std::array<float, N> m2 {};

    // ── State storage ─────────────────────────────────────────────────
    alignas(32) std::array<float, N> ic1eqL {};
    alignas(32) std::array<float, N> ic2eqL {};
    alignas(32) std::array<float, N> ic1eqR {};
    alignas(32) std::array<float, N> ic2eqR {};

    // ── Set coefficients for band b (called from UI/param thread) ─────
    void setCoeffs(int b, float a1v, float a2v, float a3v,
                   float m0v, float m1v, float m2v) noexcept
    {
        a1[(size_t)b] = a1v; a2[(size_t)b] = a2v; a3[(size_t)b] = a3v;
        m0[(size_t)b] = m0v; m1[(size_t)b] = m1v; m2[(size_t)b] = m2v;
    }

    // ── Process one sample through all N bands, return summed output ──
    inline float processL(float in) noexcept
    {
#if FREEEQ8_HAS_AVX2
        if constexpr (N == 8)
            return processAVX2(in, ic1eqL, ic2eqL);
#elif FREEEQ8_HAS_SSE2
        if constexpr (N == 4)
            return processSSE2(in, ic1eqL, ic2eqL);
#elif FREEEQ8_HAS_NEON
        if constexpr (N == 4)
            return processNeon(in, ic1eqL, ic2eqL);
#endif
        return processScalar(in, ic1eqL, ic2eqL);
    }

    inline float processR(float in) noexcept
    {
#if FREEEQ8_HAS_AVX2
        if constexpr (N == 8)
            return processAVX2(in, ic1eqR, ic2eqR);
#elif FREEEQ8_HAS_SSE2
        if constexpr (N == 4)
            return processSSE2(in, ic1eqR, ic2eqR);
#elif FREEEQ8_HAS_NEON
        if constexpr (N == 4)
            return processNeon(in, ic1eqR, ic2eqR);
#endif
        return processScalar(in, ic1eqR, ic2eqR);
    }

    void reset() noexcept
    {
        ic1eqL.fill(0.0f); ic2eqL.fill(0.0f);
        ic1eqR.fill(0.0f); ic2eqR.fill(0.0f);
    }

private:
    // ── Scalar fallback — identical to SvfBiquad hot path ────────────
    inline float processScalar(float in,
                                std::array<float, N>& ic1eq,
                                std::array<float, N>& ic2eq) noexcept
    {
        float sum = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float v0 = in;
            const float v3 = v0 - ic2eq[(size_t)i];
            const float t  = a2[(size_t)i] * ic1eq[(size_t)i];
            const float v1 = a1[(size_t)i] * ic1eq[(size_t)i] + a2[(size_t)i] * v3;
            const float v2 = ic2eq[(size_t)i] + t + a3[(size_t)i] * v3;
            ic1eq[(size_t)i] = v1 + v1 - ic1eq[(size_t)i];
            ic2eq[(size_t)i] = v2 + v2 - ic2eq[(size_t)i];
            sum += m0[(size_t)i] * v0 + m1[(size_t)i] * v1 + m2[(size_t)i] * v2;
        }
        return sum;
    }

#ifdef FREEEQ8_HAS_AVX2
    // ── AVX2 path — 8 bands in one 256-bit register pass ─────────────
    // Processes all 8 bands simultaneously using FMA3 instructions.
    // Throughput: ~2-4x scalar depending on CPU pipeline depth.
    inline float processAVX2(float in,
                              std::array<float, N>& ic1eq,
                              std::array<float, N>& ic2eq) noexcept
    {
        // Step 1: broadcast input across all 8 lanes
        __m256 v0 = _mm256_set1_ps(in);

        // Step 2: load state and coefficients from aligned arrays
        __m256 ic1 = _mm256_load_ps(ic1eq.data());
        __m256 ic2 = _mm256_load_ps(ic2eq.data());
        __m256 A1  = _mm256_load_ps(a1.data());
        __m256 A2  = _mm256_load_ps(a2.data());
        __m256 A3  = _mm256_load_ps(a3.data());
        __m256 M0  = _mm256_load_ps(m0.data());
        __m256 M1  = _mm256_load_ps(m1.data());
        __m256 M2  = _mm256_load_ps(m2.data());

        // Step 3: v3 = v0 - ic2eq  (all 8 bands simultaneously)
        __m256 v3 = _mm256_sub_ps(v0, ic2);

        // Step 4: t = a2 * ic1eq  (cached intermediate)
        __m256 t = _mm256_mul_ps(A2, ic1);

        // Step 5: v1 = fma(a2, v3, a1 * ic1eq)  — bandpass state
        __m256 v1 = _mm256_fmadd_ps(A2, v3, _mm256_mul_ps(A1, ic1));

        // Step 6: v2 = ic2eq + t + fma(a3, v3)  — lowpass state
        __m256 v2 = _mm256_add_ps(ic2, t);
        v2 = _mm256_fmadd_ps(A3, v3, v2);

        // Step 7: state update via addition (avoids 2.0f multiply)
        __m256 ic1_new = _mm256_sub_ps(_mm256_add_ps(v1, v1), ic1);
        __m256 ic2_new = _mm256_sub_ps(_mm256_add_ps(v2, v2), ic2);
        _mm256_store_ps(ic1eq.data(), ic1_new);
        _mm256_store_ps(ic2eq.data(), ic2_new);

        // Step 8: output = m0*v0 + m1*v1 + m2*v2  (all 8 bands)
        __m256 out = _mm256_mul_ps(M0, v0);
        out = _mm256_fmadd_ps(M1, v1, out);
        out = _mm256_fmadd_ps(M2, v2, out);

        // Step 9: horizontal sum — collapse 8 band outputs to 1 float
        // _mm256_hadd_ps reduces 8→4→2, then we extract high/low 128-bit halves
        __m256 h1  = _mm256_hadd_ps(out, out);   // [a+b, c+d, a+b, c+d | e+f, g+h, e+f, g+h]
        __m256 h2  = _mm256_hadd_ps(h1,  h1);    // [a+b+c+d, ...]
        __m128 lo  = _mm256_castps256_ps128(h2);
        __m128 hi  = _mm256_extractf128_ps(h2, 1);
        __m128 sum = _mm_add_ps(lo, hi);
        return _mm_cvtss_f32(sum);
    }
#endif

#ifdef FREEEQ8_HAS_SSE2
    // ── SSE2 path — 4 bands in 128-bit register ───────────────────────
    inline float processSSE2(float in,
                              std::array<float, N>& ic1eq,
                              std::array<float, N>& ic2eq) noexcept
    {
        __m128 v0  = _mm_set1_ps(in);
        __m128 ic1 = _mm_load_ps(ic1eq.data());
        __m128 ic2 = _mm_load_ps(ic2eq.data());
        __m128 A1  = _mm_load_ps(a1.data());
        __m128 A2  = _mm_load_ps(a2.data());
        __m128 A3  = _mm_load_ps(a3.data());
        __m128 M0  = _mm_load_ps(m0.data());
        __m128 M1  = _mm_load_ps(m1.data());
        __m128 M2  = _mm_load_ps(m2.data());

        __m128 v3  = _mm_sub_ps(v0, ic2);
        __m128 t   = _mm_mul_ps(A2, ic1);
        __m128 v1  = _mm_add_ps(_mm_mul_ps(A1, ic1), _mm_mul_ps(A2, v3));
        __m128 v2  = _mm_add_ps(_mm_add_ps(ic2, t), _mm_mul_ps(A3, v3));

        _mm_store_ps(ic1eq.data(), _mm_sub_ps(_mm_add_ps(v1, v1), ic1));
        _mm_store_ps(ic2eq.data(), _mm_sub_ps(_mm_add_ps(v2, v2), ic2));

        __m128 out = _mm_add_ps(_mm_mul_ps(M0, v0),
                     _mm_add_ps(_mm_mul_ps(M1, v1), _mm_mul_ps(M2, v2)));

        // Horizontal sum: SSE3 hadd or manual
        __m128 h = _mm_add_ps(out, _mm_movehl_ps(out, out));
        h = _mm_add_ss(h, _mm_shuffle_ps(h, h, 1));
        return _mm_cvtss_f32(h);
    }
#endif

#ifdef FREEEQ8_HAS_NEON
    // ── ARM Neon path — 4 bands in 128-bit register ───────────────────
    inline float processNeon(float in,
                              std::array<float, N>& ic1eq,
                              std::array<float, N>& ic2eq) noexcept
    {
        float32x4_t v0  = vdupq_n_f32(in);
        float32x4_t ic1 = vld1q_f32(ic1eq.data());
        float32x4_t ic2 = vld1q_f32(ic2eq.data());
        float32x4_t A1  = vld1q_f32(a1.data());
        float32x4_t A2  = vld1q_f32(a2.data());
        float32x4_t A3  = vld1q_f32(a3.data());
        float32x4_t M0  = vld1q_f32(m0.data());
        float32x4_t M1  = vld1q_f32(m1.data());
        float32x4_t M2  = vld1q_f32(m2.data());

        float32x4_t v3  = vsubq_f32(v0, ic2);
        float32x4_t t   = vmulq_f32(A2, ic1);
        // FMA on Neon: vmlaq_f32(acc, a, b) = acc + a*b
        float32x4_t v1  = vmlaq_f32(vmulq_f32(A1, ic1), A2, v3);
        float32x4_t v2  = vmlaq_f32(vaddq_f32(ic2, t), A3, v3);

        vst1q_f32(ic1eq.data(), vsubq_f32(vaddq_f32(v1, v1), ic1));
        vst1q_f32(ic2eq.data(), vsubq_f32(vaddq_f32(v2, v2), ic2));

        float32x4_t out = vmlaq_f32(vmlaq_f32(vmulq_f32(M0, v0), M1, v1), M2, v2);

        // Horizontal sum
        float32x2_t p = vadd_f32(vget_high_f32(out), vget_low_f32(out));
        return vget_lane_f32(vpadd_f32(p, p), 0);
    }
#endif
};

// Convenience alias — the 8-band stereo case
using SvfBandArray8 = SvfBandArray<8>;
