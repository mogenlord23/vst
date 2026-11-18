#pragma once
#include "Params.h"
#include "SampleSlot.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace mf
{
enum class MultipointMode { off = 0, key = 1, velocity = 2, cycle = 3 };

struct LayerSettings
{
    float start = 0.0f, end = 1.0f;          // normalised positions in the sample
    bool loop = false;
    float loopStart = 0.2f, loopEnd = 0.8f;  // normalised
    float xfadeMs = 15.0f;
    bool reverse = false;
    MultipointMode mpMode = MultipointMode::off;
    bool mute = false, solo = false;
    float gainDb = 0.0f;
    float pan = 0.0f;                        // -1 .. 1
    float pitchSemis = 0.0f;                 // semitones incl. cents
};

struct EngineSettings
{
    std::array<LayerSettings, kNumLayers> layers;
    float attackMs = 1.0f, decayMs = 400.0f, sustain = 1.0f, releaseMs = 250.0f;
    float velSens = 0.8f;
    float glideMs = 0.0f;     // portamento from the previous note
    float vibrato = 0.0f;     // 0..1 depth (max +-0.6 st, 5.6 Hz, delayed onset)
    float pitchBendRange = 2.0f;
};

/** Realtime 16-voice, 5-layer sampler. Lock-free and allocation-free in render(). */
class SamplerEngine
{
public:
    SamplerEngine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // ---- message thread ----
    void setSample (int layer, SamplePtr sample) { slots[(size_t) layer].publish (std::move (sample)); }
    void setMultipoints (int layer, const std::vector<float>& normalisedPoints);
    void collectGarbage();

    /** Message thread: queue a note from the UI (velocity 0 = note off). Lock-free. */
    void queueNote (int note, float velocity);

    // ---- audio thread ----
    void render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const EngineSettings& settings);
    void allNotesOff (bool immediately);

    // ---- any thread (relaxed info for UI) ----
    int getNumActiveVoices() const noexcept { return activeVoices.load (std::memory_order_relaxed); }
    /** Normalised playhead (0..1 in sample) of the most recent voice on that layer, or -1. */
    float getPlayhead (int layer) const noexcept { return playheads[(size_t) layer].load (std::memory_order_relaxed); }

private:
    struct LayerRegion
    {
        const SampleData* sample = nullptr;
        uint32_t generation = 0;
        bool audible = false;
        int len = 0;
        double start = 0, end = 0, length = 0;        // in sample frames
        double vLoopStart = 0, vLoopEnd = 0, xfade = 0; // virtual (playback-direction) coords
        bool loop = false, reverse = false;
        double rateRatio = 1.0;                        // sampleRate / hostRate
        float gainL = 1, gainR = 1;                    // target
        float curL = 1, curR = 1;                      // smoothed
        float pitchSemis = 0;
        MultipointMode mpMode = MultipointMode::off;
    };

    struct LayerPlay
    {
        bool active = false;
        double pos = 0;              // virtual position
        uint32_t generation = 0;
        int fadeIn = 0;
    };

    struct Voice
    {
        bool active = false;
        int note = -1;
        float velocity = 0;
        float velGain = 1;
        bool keyDown = false, sustained = false;
        uint64_t age = 0;
        juce::ADSR adsr;
        std::array<LayerPlay, kNumLayers> layers;
        int stealFade = 0;      // samples of fast fade-out remaining before restart
        int pendingNote = -1;
        float pendingVelocity = 0;
        double glideSemis = 0;  // current pitch offset sliding to 0
        double vibPhase = 0;
        int samplesPlayed = 0;
    };

    void updateRegions (const EngineSettings& s);
    void handleMidi (const juce::MidiMessage& m, const EngineSettings& s);
    void noteOn (int note, float velocity, const EngineSettings& s);
    void startVoice (Voice& v, int note, float velocity, const EngineSettings& s);
    void noteOff (int note);
    void renderVoices (juce::AudioBuffer<float>& out, int start, int num, const EngineSettings& s);
    double startPositionFor (int layer, int note, float velocity);

    struct QueuedNote { int note; float velocity; };
    juce::AbstractFifo noteFifo { 64 };
    std::array<QueuedNote, 64> noteQueue {};

    std::array<SampleSlot, kNumLayers> slots;
    std::array<LayerRegion, kNumLayers> regions;
    std::array<Voice, kNumVoices> voices;

    std::array<std::array<std::atomic<float>, kMaxMultipoints>, kNumLayers> multipoints {};
    std::array<std::atomic<int>, kNumLayers> numMultipoints {};
    std::array<int, kNumLayers> cycleCounter {};

    std::array<std::atomic<float>, kNumLayers> playheads {};
    std::atomic<int> activeVoices { 0 };

    double hostRate = 44100.0;
    uint64_t ageCounter = 0;
    bool sustainPedal = false;
    int lastNote = -1;
    float pitchBend = 0.0f; // -1..1
    juce::ADSR::Parameters adsrParams;
    static constexpr int kStealFadeSamples = 64;
    static constexpr int kSwapFadeSamples = 64;
};
} // namespace mf
