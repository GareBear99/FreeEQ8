/*
    ABXListeningTest.cpp — ABX listening test infrastructure for variable-cadence
    Dynamic EQ perceptual transparency validation.
    
    For PAPER.md §5 — Perceptual Validation / Listening Study
    
    Synthesizes 4 test stimuli (10s each):
      1. Sustained sine (440Hz) — worst case for cadence detection
      2. Drum loop (synthesized impulse train with decay)
      3. Vocal simulation (filtered noise with formants)
      4. Full mix (sum of above)
    
    Processes each through:
      - Path A: per-sample coefficient updates (always update)
      - Path B: variable-cadence (0.1dB threshold, 4-sample batch)
    
    Generates randomized ABX trials and records responses to CSV.
    
    Build & run (no JUCE):
        g++ -std=c++17 -O2 Tests/ABXListeningTest.cpp -o ABXListeningTest -I.
        ./ABXListeningTest
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <array>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <random>
#include <algorithm>
#include "../Source/DSP/SvfBiquad.h"

// ===========================================================================
// Constants
// ===========================================================================
static constexpr double SR = 44100.0;
static constexpr int DURATION_SAMPLES = static_cast<int>(SR * 10.0);  // 10 seconds
static constexpr double kPi2 = 2.0 * 3.14159265358979323846;
static constexpr int NUM_BANDS = 8;
static constexpr int DEFAULT_TRIALS_PER_STIMULUS = 10;

// Cadence thresholds matching CadenceBench.cpp
static constexpr float CADENCE_THRESHOLD_DB = 0.1f;
static constexpr int CADENCE_MAX_BATCH = 4;

// ===========================================================================
// PRNG (Xorshift for reproducible noise)
// ===========================================================================
static uint32_t xorshift_state = 0x12345678;

static uint32_t xorshift()
{
    xorshift_state ^= xorshift_state << 13;
    xorshift_state ^= xorshift_state >> 17;
    xorshift_state ^= xorshift_state << 5;
    return xorshift_state;
}

static float randFloat()
{
    return static_cast<float>(static_cast<int32_t>(xorshift())) / static_cast<float>(INT32_MAX);
}

// ===========================================================================
// Dynamic Band Simulator (from CadenceBench.cpp)
// ===========================================================================
struct DynBandSim
{
    SvfBiquad bq, scBq;
    float env = 0.0f;
    float lastGainMod = 0.0f;
    int counter = 0;
    double centerFreq = 1000.0;

    void prepare(double fc)
    {
        centerFreq = fc;
        bq.set(SvfBiquad::Type::Bell, SR, fc, 1.0, 0.0);
        scBq.set(SvfBiquad::Type::Bandpass, SR, fc, 2.0, 0.0);
        env = 0.0f;
        lastGainMod = 0.0f;
        counter = 0;
    }

    void reset()
    {
        bq.reset();
        scBq.reset();
        env = 0.0f;
        lastGainMod = 0.0f;
        counter = 0;
    }

    float process(float in, bool useCadence)
    {
        // Sidechain envelope follower
        float rect = std::abs(scBq.processL(in));
        float ac = 1.0f - std::exp(-1.0f / static_cast<float>(SR * 0.01));
        float rc = 1.0f - std::exp(-1.0f / static_cast<float>(SR * 0.1));
        if (rect > env) env += ac * (rect - env);
        else            env += rc * (rect - env);

        // Dynamic gain calculation
        float db = 20.0f * std::log10(std::max(env, 1e-7f));
        float gain = (db > -20.0f) ? -(db + 20.0f) * 0.75f : 0.0f;

        if (useCadence)
        {
            float delta = std::abs(gain - lastGainMod);
            if (delta > CADENCE_THRESHOLD_DB || counter++ >= CADENCE_MAX_BATCH)
            {
                bq.set(SvfBiquad::Type::Bell, SR, centerFreq, 1.0, gain);
                lastGainMod = gain;
                counter = 0;
            }
        }
        else
        {
            // Always per-sample
            bq.set(SvfBiquad::Type::Bell, SR, centerFreq, 1.0, gain);
            lastGainMod = gain;
        }

        return bq.processL(in);
    }
};

// ===========================================================================
// 8-Band Dynamic EQ Processor
// ===========================================================================
struct DynamicEQ
{
    static constexpr double FREQS[NUM_BANDS] = {100, 300, 600, 1000, 2000, 4000, 8000, 12000};
    std::array<DynBandSim, NUM_BANDS> bands;
    bool useCadence;

    DynamicEQ(bool cadence) : useCadence(cadence)
    {
        for (int b = 0; b < NUM_BANDS; ++b)
            bands[static_cast<size_t>(b)].prepare(FREQS[b]);
    }

    void reset()
    {
        for (auto& b : bands) b.reset();
    }

    float process(float in)
    {
        float out = in;
        for (auto& b : bands)
            out = b.process(out, useCadence);
        return out;
    }
};

// ===========================================================================
// Stimulus Generators
// ===========================================================================
enum class StimulusType { SustainedSine, DrumLoop, VocalSim, FullMix };

static const char* stimulusName(StimulusType t)
{
    switch (t)
    {
        case StimulusType::SustainedSine: return "sustained_sine";
        case StimulusType::DrumLoop:      return "drum_loop";
        case StimulusType::VocalSim:      return "vocal_sim";
        case StimulusType::FullMix:       return "full_mix";
    }
    return "unknown";
}

// Sustained sine (440Hz) — worst case for cadence detection
static float genSustainedSine(int s)
{
    return 0.5f * static_cast<float>(std::sin(kPi2 * 440.0 * s / SR));
}

// Drum loop — exponential decay impulses at ~2Hz
// 10ms exponential decay impulse at 100Hz fundamental + harmonics
static float genDrumLoop(int s)
{
    // Trigger every 500ms (2 Hz)
    int period = static_cast<int>(SR * 0.5);
    int cyclePos = s % period;
    
    if (cyclePos < static_cast<int>(SR * 0.1)) // 100ms decay window
    {
        float t = static_cast<float>(cyclePos) / static_cast<float>(SR);
        float decay = std::exp(-40.0f * t); // ~10ms time constant
        
        // 100Hz fundamental + harmonics (like a kick drum)
        float phase = kPi2 * 100.0 * s / SR;
        float fundamental = std::sin(phase);
        float h2 = 0.5f * std::sin(2.0 * phase);
        float h3 = 0.25f * std::sin(3.0 * phase);
        
        // Add click transient at onset
        float click = (cyclePos < static_cast<int>(SR * 0.005)) ? 0.8f * randFloat() : 0.0f;
        
        return 0.7f * decay * (fundamental + h2 + h3 + click);
    }
    return 0.0f;
}

// Vocal simulation — filtered noise with formants at 500Hz, 1.5kHz, 2.5kHz
static struct VocalSynth
{
    SvfBiquad formant1, formant2, formant3, hpf;
    float env = 0.0f;
    
    VocalSynth()
    {
        formant1.set(SvfBiquad::Type::Bandpass, SR, 500.0, 3.0, 0.0);
        formant2.set(SvfBiquad::Type::Bandpass, SR, 1500.0, 4.0, 0.0);
        formant3.set(SvfBiquad::Type::Bandpass, SR, 2500.0, 5.0, 0.0);
        hpf.set(SvfBiquad::Type::HighPass, SR, 80.0, 0.7, 0.0);
    }
    
    float process(int s)
    {
        // Amplitude envelope — syllable-like modulation (~3Hz)
        float modFreq = 3.0;
        float envMod = 0.5f + 0.5f * static_cast<float>(std::sin(kPi2 * modFreq * s / SR));
        env = 0.9f * env + 0.1f * envMod;
        
        // Glottal pulse source (noise + periodic component)
        float glottalFreq = 120.0; // F0 for average voice
        float pulse = static_cast<float>(std::sin(kPi2 * glottalFreq * s / SR));
        float aspiration = 0.3f * randFloat();
        float source = 0.7f * pulse + aspiration;
        
        // Formant filtering
        float f1 = formant1.processL(source);
        float f2 = formant2.processL(source);
        float f3 = formant3.processL(source);
        
        // Mix formants and apply envelope
        float vocal = hpf.processL((f1 + 0.6f * f2 + 0.4f * f3) * env);
        
        return 0.5f * std::tanh(vocal * 2.0f); // Soft limiting
    }
} vocalSynth;

static float genVocalSim(int s)
{
    return vocalSynth.process(s);
}

// Full mix — sum of all stimuli
static float genFullMix(int s)
{
    float sine = 0.3f * genSustainedSine(s);
    float drum = 0.4f * genDrumLoop(s);
    float vocal = 0.5f * genVocalSim(s);
    return std::tanh(sine + drum + vocal); // Soft limiting
}

static float generateSample(StimulusType type, int s)
{
    switch (type)
    {
        case StimulusType::SustainedSine: return genSustainedSine(s);
        case StimulusType::DrumLoop:      return genDrumLoop(s);
        case StimulusType::VocalSim:      return genVocalSim(s);
        case StimulusType::FullMix:       return genFullMix(s);
    }
    return 0.0f;
}

// ===========================================================================
// Audio Buffer Processing
// ===========================================================================
struct AudioBuffer
{
    std::vector<float> samples;
    
    AudioBuffer(int numSamples = 0) : samples(static_cast<size_t>(numSamples), 0.0f) {}
    
    void resize(int numSamples) { samples.resize(static_cast<size_t>(numSamples)); }
    float& operator[](size_t i) { return samples[i]; }
    float operator[](size_t i) const { return samples[i]; }
    size_t size() const { return samples.size(); }
};

// Generate stimulus and process through Dynamic EQ
static AudioBuffer processStimulus(StimulusType type, bool useCadence)
{
    // Reset PRNG for reproducibility across paths
    xorshift_state = 0x12345678 + static_cast<uint32_t>(type);
    
    AudioBuffer buffer(DURATION_SAMPLES);
    DynamicEQ eq(useCadence);
    
    for (int s = 0; s < DURATION_SAMPLES; ++s)
    {
        float in = generateSample(type, s);
        buffer[static_cast<size_t>(s)] = eq.process(in);
    }
    
    return buffer;
}

// ===========================================================================
// Raw PCM File I/O (16-bit signed, mono)
// ===========================================================================
static bool writeRawPCM(const AudioBuffer& buffer, const std::string& filename)
{
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;
    
    for (size_t i = 0; i < buffer.size(); ++i)
    {
        // Clamp and convert to 16-bit signed integer
        float sample = std::max(-1.0f, std::min(1.0f, buffer[i]));
        int16_t pcm = static_cast<int16_t>(sample * 32767.0f);
        file.write(reinterpret_cast<const char*>(&pcm), sizeof(pcm));
    }
    
    file.close();
    return true;
}

// ===========================================================================
// ABX Trial Structure
// ===========================================================================
struct ABXTrial
{
    StimulusType stimulus;
    int xIsA;  // 1 if X==A, 0 if X==B
    int userResponse;  // 1=A, 2=B, 0=unanswered
    bool correct;
    double responseTimeMs;
    std::string timestamp;
};

// ===========================================================================
// ABX Test Runner
// ===========================================================================
class ABXTestRunner
{
public:
    ABXTestRunner(int trialsPerStimulus = DEFAULT_TRIALS_PER_STIMULUS)
        : trialsPerStim(trialsPerStimulus), rng(static_cast<unsigned>(std::time(nullptr)))
    {
    }
    
    void run()
    {
        printHeader();
        generateTrials();
        runTrials();
        saveResults();
        printSummary();
    }
    
private:
    int trialsPerStim;
    std::mt19937 rng;
    std::vector<ABXTrial> trials;
    std::string resultsFilename;
    
    void printHeader()
    {
        std::printf("\n");
        std::printf("========================================================\n");
        std::printf("  ABX Listening Test — Variable-Cadence Dynamic EQ\n");
        std::printf("========================================================\n");
        std::printf("\n");
        std::printf("This test evaluates perceptual transparency of variable-cadence\n");
        std::printf("coefficient updates in an 8-band dynamic EQ.\n");
        std::printf("\n");
        std::printf("  Path A: Per-sample coefficient updates (reference)\n");
        std::printf("  Path B: Variable-cadence (0.1dB threshold, 4-sample batch)\n");
        std::printf("\n");
        std::printf("Instructions:\n");
        std::printf("  1. You will hear A, then B, then X (which is either A or B)\n");
        std::printf("  2. Press [1] if X sounds like A\n");
        std::printf("  3. Press [2] if X sounds like B\n");
        std::printf("  4. Press [q] to quit at any time\n");
        std::printf("\n");
        std::printf("Stimuli: sustained_sine, drum_loop, vocal_sim, full_mix\n");
        std::printf("Trials per stimulus: %d\n", trialsPerStim);
        std::printf("Total trials: %d\n", trialsPerStim * 4);
        std::printf("\n");
        std::printf("Press [Enter] to begin...\n");
        std::getchar();
    }
    
    void generateTrials()
    {
        // Create balanced trials for each stimulus
        for (int st = 0; st < 4; ++st)
        {
            StimulusType type = static_cast<StimulusType>(st);
            for (int t = 0; t < trialsPerStim; ++t)
            {
                ABXTrial trial;
                trial.stimulus = type;
                trial.xIsA = (t < trialsPerStim / 2) ? 1 : 0;  // Balanced A/B
                trial.userResponse = 0;
                trial.correct = false;
                trial.responseTimeMs = 0.0;
                trial.timestamp = "";
                trials.push_back(trial);
            }
        }
        
        // Randomize trial order
        std::shuffle(trials.begin(), trials.end(), rng);
    }
    
    void runTrials()
    {
        std::printf("\n--- Starting Trials ---\n\n");
        
        // Pre-generate all audio buffers
        std::printf("Generating audio buffers...\n");
        
        struct StimulusBuffers
        {
            AudioBuffer pathA;  // Per-sample updates
            AudioBuffer pathB;  // Variable-cadence
        };
        
        std::array<StimulusBuffers, 4> buffers;
        for (int st = 0; st < 4; ++st)
        {
            StimulusType type = static_cast<StimulusType>(st);
            std::printf("  Processing %s...\n", stimulusName(type));
            buffers[static_cast<size_t>(st)].pathA = processStimulus(type, false);  // No cadence = Path A
            buffers[static_cast<size_t>(st)].pathB = processStimulus(type, true);   // Cadence = Path B
        }
        
        std::printf("\nAudio generation complete.\n");
        std::printf("Writing PCM files for playback...\n\n");
        
        // Run each trial
        int trialNum = 0;
        for (auto& trial : trials)
        {
            ++trialNum;
            int stIdx = static_cast<int>(trial.stimulus);
            
            std::printf("Trial %d/%d — Stimulus: %s\n", 
                        trialNum, static_cast<int>(trials.size()), 
                        stimulusName(trial.stimulus));
            
            // Write A, B, X to temp files
            std::string fileA = "/tmp/abx_A.raw";
            std::string fileB = "/tmp/abx_B.raw";
            std::string fileX = "/tmp/abx_X.raw";
            
            writeRawPCM(buffers[static_cast<size_t>(stIdx)].pathA, fileA);
            writeRawPCM(buffers[static_cast<size_t>(stIdx)].pathB, fileB);
            
            if (trial.xIsA)
                writeRawPCM(buffers[static_cast<size_t>(stIdx)].pathA, fileX);
            else
                writeRawPCM(buffers[static_cast<size_t>(stIdx)].pathB, fileX);
            
            // Playback instructions
            std::printf("  Audio files written:\n");
            std::printf("    A: %s\n", fileA.c_str());
            std::printf("    B: %s\n", fileB.c_str());
            std::printf("    X: %s\n", fileX.c_str());
            std::printf("\n");
            std::printf("  To play (macOS): afplay -f LEI16 -r 44100 -c 1 <file>\n");
            std::printf("  To play (Linux): aplay -f S16_LE -r 44100 -c 1 <file>\n");
            std::printf("\n");
            std::printf("  Listen to A, then B, then X.\n");
            std::printf("  Which is X? [1]=A, [2]=B, [q]=quit: ");
            std::fflush(stdout);
            
            // Get response with timing
            auto startTime = std::chrono::high_resolution_clock::now();
            
            // Get timestamp
            auto now = std::chrono::system_clock::now();
            auto nowTime = std::chrono::system_clock::to_time_t(now);
            char timestamp[64];
            std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&nowTime));
            trial.timestamp = timestamp;
            
            char response = '\0';
            while (response != '1' && response != '2' && response != 'q' && response != 'Q')
            {
                response = static_cast<char>(std::getchar());
                // Clear any remaining newlines
                if (response == '\n') continue;
            }
            
            auto endTime = std::chrono::high_resolution_clock::now();
            trial.responseTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
            
            if (response == 'q' || response == 'Q')
            {
                std::printf("\n\nTest aborted by user.\n");
                break;
            }
            
            trial.userResponse = (response == '1') ? 1 : 2;
            trial.correct = (trial.userResponse == 1 && trial.xIsA == 1) ||
                           (trial.userResponse == 2 && trial.xIsA == 0);
            
            std::printf("  Response: %d (%s)\n\n", 
                        trial.userResponse,
                        trial.correct ? "CORRECT" : "incorrect");
            
            // Clear input buffer
            while (std::getchar() != '\n');
        }
    }
    
    void saveResults()
    {
        // Generate filename with timestamp
        auto now = std::chrono::system_clock::now();
        auto nowTime = std::chrono::system_clock::to_time_t(now);
        char timestamp[32];
        std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", std::localtime(&nowTime));
        resultsFilename = std::string("abx_results_") + timestamp + ".csv";
        
        std::ofstream csv(resultsFilename);
        if (!csv.is_open())
        {
            std::fprintf(stderr, "Error: Could not write results to %s\n", resultsFilename.c_str());
            return;
        }
        
        // CSV header
        csv << "trial,stimulus,x_is_a,user_response,correct,response_time_ms,timestamp\n";
        
        int trialNum = 0;
        for (const auto& trial : trials)
        {
            if (trial.userResponse == 0) continue;  // Skip unanswered trials
            ++trialNum;
            csv << trialNum << ","
                << stimulusName(trial.stimulus) << ","
                << trial.xIsA << ","
                << trial.userResponse << ","
                << (trial.correct ? 1 : 0) << ","
                << trial.responseTimeMs << ","
                << trial.timestamp << "\n";
        }
        
        csv.close();
        std::printf("Results saved to: %s\n", resultsFilename.c_str());
    }
    
    void printSummary()
    {
        std::printf("\n");
        std::printf("========================================================\n");
        std::printf("  Test Summary\n");
        std::printf("========================================================\n");
        
        int total = 0, correct = 0;
        std::array<int, 4> stimTotal = {0, 0, 0, 0};
        std::array<int, 4> stimCorrect = {0, 0, 0, 0};
        
        for (const auto& trial : trials)
        {
            if (trial.userResponse == 0) continue;
            int st = static_cast<int>(trial.stimulus);
            ++total;
            ++stimTotal[static_cast<size_t>(st)];
            if (trial.correct)
            {
                ++correct;
                ++stimCorrect[static_cast<size_t>(st)];
            }
        }
        
        std::printf("\nOverall: %d/%d correct (%.1f%%)\n", 
                    correct, total, 
                    total > 0 ? 100.0 * correct / total : 0.0);
        
        std::printf("\nBy stimulus:\n");
        for (int st = 0; st < 4; ++st)
        {
            StimulusType type = static_cast<StimulusType>(st);
            int t = stimTotal[static_cast<size_t>(st)];
            int c = stimCorrect[static_cast<size_t>(st)];
            std::printf("  %-15s: %d/%d (%.1f%%)\n", 
                        stimulusName(type), c, t,
                        t > 0 ? 100.0 * c / t : 0.0);
        }
        
        // Quick significance check
        if (total >= 10)
        {
            // Binomial test approximation
            double hitRate = static_cast<double>(correct) / static_cast<double>(total);
            double se = std::sqrt(0.5 * 0.5 / total);  // SE under null hypothesis
            double z = (hitRate - 0.5) / se;
            
            std::printf("\nQuick stats:\n");
            std::printf("  Hit rate: %.1f%%\n", hitRate * 100.0);
            std::printf("  Z-score vs chance (50%%): %.2f\n", z);
            
            if (std::abs(z) < 1.96)
                std::printf("  Result: NOT significantly different from chance (p > 0.05)\n");
            else
                std::printf("  Result: SIGNIFICANTLY different from chance (p < 0.05)\n");
            
            std::printf("\nFor full analysis, run: python3 ABXAnalysis.py %s\n", resultsFilename.c_str());
        }
        
        std::printf("\n");
    }
};

// ===========================================================================
// Main
// ===========================================================================
int main(int argc, char* argv[])
{
    int trialsPerStim = DEFAULT_TRIALS_PER_STIMULUS;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "-n") == 0 && i + 1 < argc)
        {
            trialsPerStim = std::atoi(argv[++i]);
            if (trialsPerStim < 2) trialsPerStim = 2;
        }
        else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0)
        {
            std::printf("ABXListeningTest — Variable-Cadence Dynamic EQ ABX Test\n\n");
            std::printf("Usage: %s [-n trials_per_stimulus]\n\n", argv[0]);
            std::printf("Options:\n");
            std::printf("  -n N   Number of trials per stimulus type (default: %d)\n", DEFAULT_TRIALS_PER_STIMULUS);
            std::printf("         Total trials = N × 4 (for 4 stimulus types)\n");
            std::printf("  -h     Show this help message\n\n");
            std::printf("Output:\n");
            std::printf("  abx_results_YYYYMMDD_HHMMSS.csv  — Trial responses\n\n");
            std::printf("Audio playback:\n");
            std::printf("  macOS: afplay -f LEI16 -r 44100 -c 1 /tmp/abx_X.raw\n");
            std::printf("  Linux: aplay -f S16_LE -r 44100 -c 1 /tmp/abx_X.raw\n");
            return 0;
        }
    }
    
    ABXTestRunner runner(trialsPerStim);
    runner.run();
    
    return 0;
}
