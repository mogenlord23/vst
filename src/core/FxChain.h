#pragma once
#include <juce_dsp/juce_dsp.h>

namespace mf
{
struct FxSettings
{
    bool fltOn = false; int fltType = 0; float fltCutoff = 8000.0f, fltRes = 0.2f, fltMix = 1.0f;
    bool distOn = false; float distDrive = 0.3f, distMix = 1.0f;
    bool choOn = false; float choRate = 0.6f, choDepth = 0.35f, choMix = 0.5f;
    bool dlyOn = false, dlySync = true; float dlyTimeMs = 350.0f; int dlyDiv = 4; float dlyFeedback = 0.35f, dlyMix = 0.25f;
    bool revOn = false; float revSize = 0.6f, revDamp = 0.4f, revWidth = 1.0f, revMix = 0.25f;
    float masterGainDb = 0.0f;
    bool limiterOn = true; float limiterCeilingDb = -0.3f;
    double bpm = 120.0;
};

/** Stereo effect chain: filter -> distortion -> chorus -> delay -> reverb -> master -> limiter.
    Allocation-free in process(); all buffers are sized in prepare(). Bypass and mix changes
    are smoothed (no clicks). */
class FxChain
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& stereo, int numSamples, const FxSettings& s);

    static double delayTimeMs (const FxSettings& s);
    /** Linear gain reduction applied by the limiter in the last block (1 = none). */
    float getLimiterGain() const noexcept { return limiterGainReport.load (std::memory_order_relaxed); }

private:
    struct Blend
    {
        juce::SmoothedValue<float> amount;
        bool idle() const { return ! amount.isSmoothing() && amount.getTargetValue() <= 0.0f; }
    };

    void blendInto (juce::AudioBuffer<float>& io, const juce::AudioBuffer<float>& wet, int n, Blend& b);

    double sr = 44100.0;
    int maxBlock = 512;
    juce::AudioBuffer<float> wet;

    // filter
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoff;
    Blend fltBlend;
    int lastFltType = -1;

    // distortion
    juce::SmoothedValue<float> drive;
    Blend distBlend;

    // chorus
    juce::dsp::Chorus<float> chorus;
    Blend choBlend;

    // delay
    juce::AudioBuffer<float> delayLine;
    int delayWrite = 0;
    juce::SmoothedValue<float> delaySamples;
    float delayDamp[2] {};
    Blend dlyBlend;

    // reverb
    juce::dsp::Reverb reverb;
    Blend revBlend;

    // master
    juce::SmoothedValue<float> master;
    float limGain = 1.0f;
    float limRelease = 0.999f;
    std::atomic<float> limiterGainReport { 1.0f };
};
} // namespace mf
