#pragma once

// ── Product configuration ─────────────────────────────────────────────
// PROEQ8 is set by CMake for the ProEQ8 target. FreeEQ8 builds without it.

#if PROEQ8
    static constexpr int    kNumBands     = 24;
    static constexpr bool   kIsProVersion = true;
    static constexpr const char* kProductName  = "ProEQ8";
    static constexpr const char* kProductTag   = "24-Band Parametric EQ";
#else
    static constexpr int    kNumBands     = 8;
    static constexpr bool   kIsProVersion = false;
    static constexpr const char* kProductName  = "FreeEQ8";
    static constexpr const char* kProductTag   = "8-Band Parametric EQ";
#endif

static constexpr const char* kVersion = "2.2.0";

// GitHub repo for update checking
static constexpr const char* kGitHubOwner = "GareBear99";
static constexpr const char* kGitHubRepo  = "FreeEQ8";

// License activation server URL (Cloudflare Worker)
// Replace with your deployed worker URL before shipping.
static constexpr const char* kActivationServerURL =
    "https://proeq8-license-server.admension.workers.dev";
