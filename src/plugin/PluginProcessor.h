#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "core/Params.h"
#include "core/SamplerEngine.h"
#include "core/Evolver.h"
#include "core/FxChain.h"
#include "core/Presets.h"
#include "core/SoundFeatures.h"
#include <optional>

/** Message-thread model of one layer: its sample history (for EVOLVE undo/redo). */
struct LayerModel
{
    struct Entry
    {
        mf::SamplePtr audio;    // may be null (released or not restored); re-rendered on demand
        juce::uint64 seed = 0;  // 0 for the original sample
        float chaos = 0.0f;
        juce::uint32 processMask = 0xff;
        bool root = false;      // audio cannot be re-derived from the previous entry
        bool favorite = false;
        bool hasFeatures = false;
        mf::SoundFeatures features;
        juce::String label;
    };

    std::vector<Entry> history;
    int index = -1;
    juce::String sourcePath;
    std::vector<float> multipoints;
    int revision = 0;       // bumped whenever the audible sample changes (UI refresh)
    int epoch = 0;          // bumped whenever the layer is replaced/navigated; stale jobs are dropped
    bool busy = false;      // an EVOLVE / re-render job is running
    bool locked = false;    // excluded from group EVOLVE and the dice
    juce::String lastError;

    mf::SamplePtr current() const { return index >= 0 ? history[(size_t) index].audio : nullptr; }
    bool hasSample() const { return index >= 0; }
    void clear()
    {
        history.clear();
        index = -1;
        sourcePath.clear();
        multipoints.clear();
        ++epoch;
        busy = false;
    }
};

class MutaformProcessor : public juce::AudioProcessor,
                          public juce::ChangeBroadcaster,
                          private juce::Timer
{
public:
    static constexpr int kMaxHistory = 32;

    MutaformProcessor();
    ~MutaformProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- model API (message thread) ----
    bool loadSampleFile (int layer, const juce::File& file, juce::String& error);
    bool loadSampleMemory (int layer, const void* data, size_t size, const juce::String& name, juce::String& error);
    void setLayerSample (int layer, mf::SamplePtr sample, const juce::String& sourcePath);
    void clearLayer (int layer);
    void setMultipoints (int layer, std::vector<float> points);
    const LayerModel& getLayer (int layer) const { return layers[(size_t) layer]; }
    mf::SamplerEngine& getEngine() { return engine; }

    /** Starts an EVOLVE job for the layer. seed == 0 picks a random seed. Returns false if busy/empty. */
    bool evolveLayer (int layer, juce::uint64 seed = 0, int steps = 1, float chaosOverride = -1.0f);
    /** EVOLVE every loaded, unlocked layer (layerMask bit i = layer i). Returns number started. */
    int evolveGroup (juce::uint32 layerMask = 0x1f, int steps = 1);
    /** Dice: randomise parameters and evolve all unlocked layers 1-3 steps. Undoable once. */
    void rollDice();
    bool undoDice();
    bool canUndoDice() const { return diceUndo.has_value(); }
    void setLocked (int layer, bool locked);
    void toggleFavorite (int layer, int index);
    /** Global "level" navigation over group EVOLVE snapshots. */
    bool stepGeneration (int delta);
    bool canStepGeneration (int delta) const;
    int getLevel() const;
    int getLevel (int layer) const;
    bool undo (int layer);
    bool redo (int layer);
    bool jumpToHistory (int layer, int index);
    bool canUndo (int layer) const;
    bool canRedo (int layer) const;
    bool isBusy (int layer) const { return layers[(size_t) layer].busy; }
    bool anyBusy() const;

    /** Collects retired buffers and applies finished jobs. Called by the timer; tests may call it. */
    void housekeeping();
    /** Test helper: blocks (pumping housekeeping) until no job is running. */
    bool waitUntilIdle (int timeoutMs = 20000);

    mf::FxChain& getFx() { return fx; }

    // ---- presets / randomizer (message thread) ----
    mf::PresetData capturePreset (const juce::String& name) const;
    void applyPreset (const mf::PresetData& preset);
    /** id = "factory:N" or "user:<folder name>" */
    bool loadPresetById (const juce::String& id, juce::String& error);
    bool saveUserPreset (const juce::String& name, juce::String& error);
    juce::String getCurrentPresetName() const { return currentPreset; }
    juce::String uiSkin { "pink" };   // UI colour scheme, stored in the plugin state
    juce::File presetRoot = mf::userPresetRoot();
    /** Random but musical parameter set; leaves samples, master and limiter untouched. */
    void randomizeParameters (juce::uint64 seed = 0);

    juce::AudioProcessorValueTreeState apvts;

private:
    struct JobResult
    {
        int layer = 0;
        int epoch = 0;
        int targetIndex = -1;                   // >= 0: re-render of an existing entry (chain)
        std::vector<mf::SamplePtr> chainAudio;  // re-render results for indices (targetIndex - n + 1 .. targetIndex)
        std::vector<mf::EvolveResult> evolved;  // new variations (one per step)
        std::vector<mf::SoundFeatures> features;
        std::vector<juce::uint64> seeds;
        float chaos = 0;
        juce::uint32 mask = 0xff;
        bool failed = false;
    };
    struct Generation { std::array<int, mf::kNumLayers> idx {}; };
    struct DiceUndo { std::map<juce::String, float> params; std::array<int, mf::kNumLayers> idx {}; };

    void pushGeneration();
    void resetGenerations();
    void finishGroupMember (int layer);

    void timerCallback() override { housekeeping(); }
    void publishLayer (int layer);
    void applyResult (JobResult& r);
    void navigateTo (int layer, int index);
    juce::uint32 currentProcessMask() const;
    mf::EngineSettings readEngineSettings() const;
    mf::FxSettings readFxSettings() const;

    mf::ParamRefs params;
    mf::SamplerEngine engine;
    mf::FxChain fx;
    std::array<LayerModel, mf::kNumLayers> layers;

    juce::ThreadPool workers { juce::ThreadPoolOptions {}.withThreadName ("Mutaform evolve").withNumberOfThreads (1) };
    std::atomic<bool> cancelJobs { false };
    juce::CriticalSection resultLock;          // never taken on the audio thread
    std::vector<JobResult> finishedJobs;
    std::atomic<int> runningJobs { 0 };

    juce::AudioBuffer<float> mixBuffer;
    juce::String currentPreset;
    std::vector<Generation> generations;
    int generationIndex = -1;
    juce::uint32 groupPending = 0;
    std::optional<DiceUndo> diceUndo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MutaformProcessor)
};
