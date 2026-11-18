#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "core/Params.h"
#include "core/SamplerEngine.h"

/** Message-thread model of one layer: its sample history (for EVOLVE undo/redo). */
struct LayerModel
{
    struct Entry
    {
        mf::SamplePtr audio;    // may be null after state restore (re-rendered on demand)
        juce::uint64 seed = 0;  // 0 for the original sample
        float chaos = 0.0f;
        juce::uint32 processMask = 0xff;
        juce::String label;
    };

    std::vector<Entry> history;
    int index = -1;
    juce::String sourcePath;
    std::vector<float> multipoints;

    mf::SamplePtr current() const { return index >= 0 ? history[(size_t) index].audio : nullptr; }
    bool hasSample() const { return current() != nullptr; }
    void clear() { history.clear(); index = -1; sourcePath.clear(); multipoints.clear(); }
};

class MutaformProcessor : public juce::AudioProcessor,
                          public juce::ChangeBroadcaster,
                          private juce::Timer
{
public:
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
    double getTailLengthSeconds() const override { return 2.0; }

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

    /** Collect retired buffers etc. Called by the timer; tests may call it directly. */
    void housekeeping();

    juce::AudioProcessorValueTreeState apvts;

private:
    void timerCallback() override { housekeeping(); }
    void publishLayer (int layer);
    mf::EngineSettings readEngineSettings() const;

    mf::ParamRefs params;
    mf::SamplerEngine engine;
    std::array<LayerModel, mf::kNumLayers> layers;

    juce::AudioBuffer<float> mixBuffer;
    juce::SmoothedValue<float> masterGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MutaformProcessor)
};
