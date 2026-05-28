/*
    PluginBenchTemplate.cpp — External Plugin Integration Template
    
    This file demonstrates how to extend the CompetitiveBench.cpp framework
    to include external VST3/AU plugins for comparison benchmarking.
    
    NOTE: This is a TEMPLATE file - it will NOT compile as-is because it
    requires VST3/AU SDK headers and a plugin hosting framework (such as
    JUCE's AudioPluginHost, Steinberg's VST3 SDK, or Apple's AudioUnit API).
    
    Supported external plugins for comparison:
        - FabFilter Pro-Q 3 (industry standard, proprietary de-cramping)
        - Soothe2 (resonance suppression, ML-based)
        - TDR Nova (dynamic EQ, free/paid)
        - iZotope Neutron (AI-driven EQ)
        - DMG Audio EQuilibrium (precision EQ)
    
    Integration approaches:
        1. VST3 SDK: Direct plugin loading via Steinberg SDK
        2. JUCE AudioPluginHost: Leverage JUCE's plugin scanning/hosting
        3. Command-line DAW: Render through Reaper/Ardour with CLI
    
    Co-Authored-By: Oz <oz-agent@warp.dev>
*/

#include <cstdio>
#include <vector>
#include <string>
#include <memory>

// Pull in the benchmark interface from CompetitiveBench.cpp
// In practice, this would be extracted to a shared header
struct BenchmarkableEQ {
    virtual ~BenchmarkableEQ() = default;
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void setBand(int idx, double fc, double q, double gainDb) = 0;
    virtual void process(float* L, float* R, int numSamples) = 0;
    virtual int getLatency() const = 0;
    virtual const char* getName() const = 0;
    virtual size_t getMemoryBytes() const = 0;
    virtual void reset() = 0;
    virtual int getNumBands() const = 0;
};

// =============================================================================
// VST3 Plugin Wrapper — STUB IMPLEMENTATION
// =============================================================================
// To enable: 
//   1. Download Steinberg VST3 SDK from https://steinbergmedia.github.io/vst3_doc/
//   2. Add include paths to VST SDK headers
//   3. Implement the IPluginFactory loading mechanism
//   4. Map BenchmarkableEQ interface to IEditController parameter changes

#if 0  // Enable when VST3 SDK is available

#include "pluginterfaces/vst/vsttypes.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/common/memorystream.h"

class VST3BenchEQ : public BenchmarkableEQ {
public:
    VST3BenchEQ(const char* pluginPath, int numBands) 
        : m_pluginPath(pluginPath), m_numBands(numBands) {}
    
    void prepare(double sampleRate, int maxBlockSize) override {
        // 1. Load plugin from m_pluginPath using module loading
        // 2. Query IPluginFactory for audio processor component
        // 3. Call processor->setupProcessing() with ProcessSetup
        // 4. Activate the component
        //
        // Pseudocode:
        //   m_module = LoadModule(m_pluginPath);
        //   auto factory = m_module->getFactory();
        //   factory->createInstance(processorCID, &m_processor);
        //   ProcessSetup setup { .sampleRate = sampleRate, ... };
        //   m_processor->setupProcessing(setup);
        //   m_processor->setActive(true);
    }
    
    void setBand(int idx, double fc, double q, double gainDb) override {
        // Map band parameters to plugin's normalized parameter IDs
        // This requires plugin-specific knowledge of parameter layout
        //
        // FabFilter Pro-Q 3 parameter mapping (approximate):
        //   Band N Frequency: paramId = N * 5 + 0
        //   Band N Gain:      paramId = N * 5 + 1  
        //   Band N Q:         paramId = N * 5 + 2
        //   Band N Type:      paramId = N * 5 + 3
        //   Band N Enabled:   paramId = N * 5 + 4
        //
        // m_controller->setParamNormalized(bandFreqId, freqToNorm(fc));
        // m_controller->setParamNormalized(bandGainId, gainToNorm(gainDb));
        // m_controller->setParamNormalized(bandQId, qToNorm(q));
    }
    
    void process(float* L, float* R, int numSamples) override {
        // Set up ProcessData with input/output bus configuration
        // Call m_processor->process(data)
        //
        // ProcessData data;
        // data.numSamples = numSamples;
        // data.inputs = &inputBus;
        // data.outputs = &outputBus;
        // m_processor->process(data);
    }
    
    int getLatency() const override {
        // Query latency from processor
        // return m_processor->getLatencySamples();
        return 0;
    }
    
    const char* getName() const override { 
        return m_pluginPath.c_str(); 
    }
    
    size_t getMemoryBytes() const override {
        // VST3 plugins don't expose memory usage directly
        // Return 0 or estimate from process context
        return 0;
    }
    
    void reset() override {
        // m_processor->setActive(false);
        // m_processor->setActive(true);
    }
    
    int getNumBands() const override { return m_numBands; }

private:
    std::string m_pluginPath;
    int m_numBands;
    // Steinberg::Vst::IAudioProcessor* m_processor = nullptr;
    // Steinberg::Vst::IEditController* m_controller = nullptr;
};

#endif // VST3 SDK stub

// =============================================================================
// Audio Unit (macOS) Plugin Wrapper — STUB IMPLEMENTATION  
// =============================================================================
// To enable on macOS:
//   1. Link against AudioToolbox.framework
//   2. Use AudioComponentFindNext/AudioComponentInstanceNew
//   3. Map BenchmarkableEQ interface to AU parameter tree

#if 0  // Enable on macOS with AU support

#include <AudioToolbox/AudioToolbox.h>

class AUBenchEQ : public BenchmarkableEQ {
public:
    AUBenchEQ(const char* auName, int numBands) 
        : m_auName(auName), m_numBands(numBands) {}
    
    void prepare(double sampleRate, int maxBlockSize) override {
        // 1. Create AudioComponentDescription for EQ type
        // 2. Find component with AudioComponentFindNext
        // 3. Instantiate with AudioComponentInstanceNew
        // 4. Initialize and set stream format
        //
        // AudioComponentDescription desc = {
        //     .componentType = kAudioUnitType_Effect,
        //     .componentSubType = 'equl', // or specific plugin subtype
        //     .componentManufacturer = 'FabF', // FabFilter, etc.
        // };
        // AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
        // AudioComponentInstanceNew(comp, &m_audioUnit);
        // AudioUnitInitialize(m_audioUnit);
    }
    
    void setBand(int idx, double fc, double q, double gainDb) override {
        // Use AudioUnitSetParameter with plugin-specific parameter IDs
        // AudioUnitSetParameter(m_audioUnit, freqParamId, scope, element, value, 0);
    }
    
    void process(float* L, float* R, int numSamples) override {
        // Render via AudioUnitRender or manual buffer push
    }
    
    int getLatency() const override {
        // Query kAudioUnitProperty_Latency
        return 0;
    }
    
    const char* getName() const override { return m_auName.c_str(); }
    size_t getMemoryBytes() const override { return 0; }
    void reset() override { /* AudioUnitReset(m_audioUnit, ...) */ }
    int getNumBands() const override { return m_numBands; }

private:
    std::string m_auName;
    int m_numBands;
    // AudioUnit m_audioUnit = nullptr;
};

#endif // AU stub

// =============================================================================
// JUCE-Based Plugin Wrapper — RECOMMENDED APPROACH
// =============================================================================
// The most practical approach for cross-platform plugin hosting is using
// JUCE's AudioPluginFormatManager and PluginDescription system.
//
// To enable:
//   1. Build with JUCE modules (juce_audio_processors, juce_audio_plugin_client)
//   2. Use AudioPluginFormatManager to scan for plugins
//   3. Create AudioPluginInstance via format->createPluginInstance()
//   4. Map BenchmarkableEQ to AudioProcessorParameter API

#if 0  // Enable with JUCE

#include <juce_audio_processors/juce_audio_processors.h>

class JUCEPluginBenchEQ : public BenchmarkableEQ {
public:
    JUCEPluginBenchEQ(const juce::String& pluginPath, int numBands)
        : m_numBands(numBands)
    {
        // Scan and instantiate plugin
        juce::AudioPluginFormatManager formatManager;
        formatManager.addDefaultFormats();
        
        // Load from known path or by scanning
        juce::PluginDescription desc;
        juce::KnownPluginList knownPlugins;
        // ... populate desc from scanning ...
        
        juce::String error;
        m_plugin = formatManager.createPluginInstance(
            desc, 44100.0, 512, error);
    }
    
    void prepare(double sampleRate, int maxBlockSize) override {
        if (m_plugin) {
            m_plugin->prepareToPlay(sampleRate, maxBlockSize);
        }
    }
    
    void setBand(int idx, double fc, double q, double gainDb) override {
        // Navigate parameter tree to find band parameters
        // This is plugin-specific - would need parameter name mapping
        if (m_plugin) {
            auto& params = m_plugin->getParameters();
            // Find and set frequency/gain/Q parameters for band idx
        }
    }
    
    void process(float* L, float* R, int numSamples) override {
        if (m_plugin) {
            juce::AudioBuffer<float> buffer(2, numSamples);
            buffer.copyFrom(0, 0, L, numSamples);
            buffer.copyFrom(1, 0, R, numSamples);
            
            juce::MidiBuffer midi;
            m_plugin->processBlock(buffer, midi);
            
            buffer.copyTo(L, 0, numSamples);
            buffer.copyTo(R, 1, numSamples);
        }
    }
    
    int getLatency() const override {
        return m_plugin ? m_plugin->getLatencySamples() : 0;
    }
    
    const char* getName() const override {
        return m_plugin ? m_plugin->getName().toRawUTF8() : "Unknown Plugin";
    }
    
    size_t getMemoryBytes() const override { return 0; }
    
    void reset() override {
        if (m_plugin) m_plugin->reset();
    }
    
    int getNumBands() const override { return m_numBands; }

private:
    std::unique_ptr<juce::AudioPluginInstance> m_plugin;
    int m_numBands;
};

#endif // JUCE stub

// =============================================================================
// Offline Rendering Approach — DAW CLI
// =============================================================================
// For plugins that can't be hosted directly, use DAW command-line rendering:
//
// 1. Create a DAW project file with the plugin on a track
// 2. Import test audio file (sine sweep, noise)
// 3. Export/bounce via CLI:
//    - Reaper: reaper -renderproject project.rpp
//    - Ardour: ardour --no-splash -o output.wav session
// 4. Measure the rendered output file
//
// This approach is slower but works for any plugin that can be used in a DAW.

struct OfflineDAWBenchmark {
    static void generateTestProject(const char* pluginName, 
                                     const char* testAudioPath,
                                     const char* projectPath) {
        // Generate minimal Reaper project file with plugin inserted
        printf("// TODO: Generate %s project file for plugin '%s'\n",
               projectPath, pluginName);
        printf("// Test audio: %s\n", testAudioPath);
    }
    
    static void renderAndMeasure(const char* projectPath,
                                  const char* outputPath) {
        // Execute DAW CLI render
        printf("// TODO: Execute render command for %s -> %s\n",
               projectPath, outputPath);
        
        // After render completes, load output and measure:
        // - Latency: cross-correlate input vs output
        // - HF cramping: measure gain at test frequencies
        // - Note: CPU time not measurable with this approach
    }
};

// =============================================================================
// Plugin-Specific Parameter Mappings
// =============================================================================
// Each plugin has different parameter layouts. This section documents
// known mappings for popular EQ plugins.

namespace PluginMappings {

// FabFilter Pro-Q 3 — 24 bands, each with ~5 parameters
// Parameter IDs are sequential per band
struct FabFilterProQ3 {
    static constexpr int PARAMS_PER_BAND = 5;
    
    // Band N parameter IDs (N = 0..23)
    static int freqParamId(int band) { return band * PARAMS_PER_BAND + 0; }
    static int gainParamId(int band) { return band * PARAMS_PER_BAND + 1; }
    static int qParamId(int band)    { return band * PARAMS_PER_BAND + 2; }
    static int typeParamId(int band) { return band * PARAMS_PER_BAND + 3; }
    static int enableParamId(int band) { return band * PARAMS_PER_BAND + 4; }
    
    // Frequency normalization (20Hz - 20kHz log scale)
    static double freqToNorm(double hz) {
        return (std::log(hz / 20.0) / std::log(20000.0 / 20.0));
    }
    
    // Gain normalization (-30 to +30 dB)
    static double gainToNorm(double db) {
        return (db + 30.0) / 60.0;
    }
    
    // Q normalization (0.1 to 30, log scale)
    static double qToNorm(double q) {
        return std::log(q / 0.1) / std::log(30.0 / 0.1);
    }
};

// TDR Nova — 4 bands, similar structure
struct TDRNova {
    static constexpr int MAX_BANDS = 4;
    // Parameter mapping varies by band - consult plugin documentation
};

// Soothe2 — Resonance suppression, different paradigm
// Not a traditional band-based EQ - would need different test methodology
struct Soothe2 {
    // Main parameters: depth, sharpness, selectivity
    // Comparison would measure resonance reduction rather than frequency response
};

}

// =============================================================================
// Manual Measurement Documentation
// =============================================================================
// Some measurements require manual runs with commercial plugins.
// This section documents the procedure for consistent results.

namespace ManualMeasurement {

constexpr const char* PROCEDURE = R"(
MANUAL BENCHMARK PROCEDURE FOR COMMERCIAL PLUGINS
=================================================

1. TEST SIGNAL PREPARATION
   - Generate 1-minute 44.1kHz stereo WAV files:
     a) White noise (for CPU measurement)
     b) Sine sweep 20Hz-20kHz (for frequency response)
     c) 16kHz sine tone (for HF cramping)

2. CPU MEASUREMENT
   - Load plugin in DAW (Logic/Reaper/Ableton)
   - Configure 8-band Bell EQ curve (matching CompetitiveBench settings)
   - Play white noise through plugin
   - Record CPU meter reading (DAW's built-in meter)
   - Note: Convert DAW % to ns/sample using:
     ns_per_sample = (cpu_pct / 100) * (block_size / sample_rate) * 1e9

3. LATENCY MEASUREMENT
   - Most DAWs report plugin latency directly
   - Or: render impulse through plugin, measure delay in output

4. HF CRAMPING MEASUREMENT  
   - Set single Bell band: fc=16kHz, Q=1.0, gain=+6dB
   - Process 16kHz sine tone
   - Measure output level vs input level
   - Error = measured_gain_dB - 6.0

5. DATA RECORDING
   - Record results in COMPETITIVE_ANALYSIS.md table
   - Include: plugin version, DAW, OS, CPU model
   - Multiple runs recommended for CPU stability
)";

}

// =============================================================================
// Main — Template Demonstration
// =============================================================================

int main() {
    printf("PluginBenchTemplate.cpp — External Plugin Integration Template\n");
    printf("===============================================================\n\n");
    
    printf("This file demonstrates how to extend CompetitiveBench.cpp\n");
    printf("to include external VST3/AU plugins for comparison.\n\n");
    
    printf("To enable plugin hosting, uncomment the appropriate section:\n");
    printf("  - VST3BenchEQ:        Requires Steinberg VST3 SDK\n");
    printf("  - AUBenchEQ:          Requires macOS AudioToolbox\n");
    printf("  - JUCEPluginBenchEQ:  Requires JUCE framework (recommended)\n\n");
    
    printf("For plugins that can't be hosted directly, use the\n");
    printf("OfflineDAWBenchmark approach with DAW CLI rendering.\n\n");
    
    printf("Manual measurement procedure:\n");
    printf("%s\n", ManualMeasurement::PROCEDURE);
    
    return 0;
}
