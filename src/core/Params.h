#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace mf
{
inline constexpr int kNumLayers = 5;
inline constexpr int kNumVoices = 16;
inline constexpr int kMaxMultipoints = 8;
inline constexpr int kRootNote = 60; // C3 (DAW convention C3 = 60)

namespace pid
{
    // global
    inline constexpr const char* masterGain = "master_gain";
    inline constexpr const char* limiterOn = "limiter_on";
    inline constexpr const char* limiterCeiling = "limiter_ceiling";
    inline constexpr const char* attack = "amp_attack";
    inline constexpr const char* decay = "amp_decay";
    inline constexpr const char* sustain = "amp_sustain";
    inline constexpr const char* release = "amp_release";
    inline constexpr const char* velSens = "vel_sens";
    inline constexpr const char* glide = "glide";
    inline constexpr const char* vibrato = "vibrato";
    // MELODY engine
    inline constexpr const char* melOn = "mel_on";
    inline constexpr const char* melKey = "mel_key";
    inline constexpr const char* melScale = "mel_scale";
    inline constexpr const char* melMood = "mel_mood";
    inline constexpr const char* melDensity = "mel_density";
    inline constexpr const char* melBars = "mel_bars";
    inline constexpr const char* melOctave = "mel_octave";
    inline constexpr const char* autoTune = "auto_tune";
    inline constexpr const char* tonalize = "tonalize";
    inline constexpr const char* fxDry = "fx_dry";
    inline constexpr const char* chaos = "chaos";
    // evolve process enables
    inline constexpr std::array<const char*, 8> evoProcess { "evo_pitch", "evo_grain", "evo_filter", "evo_sat",
                                                             "evo_stretch", "evo_ring", "evo_revtail", "evo_trans" };
    // fx
    inline constexpr const char* fltOn = "flt_on";
    inline constexpr const char* fltType = "flt_type";
    inline constexpr const char* fltCutoff = "flt_cutoff";
    inline constexpr const char* fltRes = "flt_res";
    inline constexpr const char* fltMix = "flt_mix";
    inline constexpr const char* distOn = "dist_on";
    inline constexpr const char* distDrive = "dist_drive";
    inline constexpr const char* distMix = "dist_mix";
    inline constexpr const char* choOn = "cho_on";
    inline constexpr const char* choRate = "cho_rate";
    inline constexpr const char* choDepth = "cho_depth";
    inline constexpr const char* choMix = "cho_mix";
    inline constexpr const char* dlyOn = "dly_on";
    inline constexpr const char* dlySync = "dly_sync";
    inline constexpr const char* dlyTime = "dly_time";
    inline constexpr const char* dlyDiv = "dly_div";
    inline constexpr const char* dlyFeedback = "dly_fb";
    inline constexpr const char* dlyMix = "dly_mix";
    inline constexpr const char* revOn = "rev_on";
    inline constexpr const char* revSize = "rev_size";
    inline constexpr const char* revDamp = "rev_damp";
    inline constexpr const char* revWidth = "rev_width";
    inline constexpr const char* revMix = "rev_mix";

    // per-layer suffixes
    inline constexpr std::array<const char*, 14> layerSuffixes { "start", "end", "loop", "loopStart", "loopEnd", "xfade",
                                                                 "reverse", "mpMode", "mute", "gain", "pan", "semi", "cents", "solo" };
    juce::String layer (int layerIndex, const char* suffix); // layerIndex 0-based -> "l1_start"
} // namespace pid

/** Note values for tempo-synced delay, in quarter notes. */
inline constexpr std::array<float, 11> kDelayDivisionsQuarters { 0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.5f, 0.75f,
                                                                 2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 4.0f };
juce::StringArray delayDivisionNames();

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Cached raw parameter pointers for lock-free audio-thread access. */
struct ParamRefs
{
    struct Layer
    {
        std::atomic<float>* start {}, *end {}, *loop {}, *loopStart {}, *loopEnd {}, *xfade {}, *reverse {}, *mpMode {},
            *mute {}, *gain {}, *pan {}, *semi {}, *cents {}, *solo {};
    };
    std::array<Layer, kNumLayers> layers;
    std::atomic<float>* masterGain {}, *limiterOn {}, *limiterCeiling {}, *attack {}, *decay {}, *sustain {}, *release {},
        *velSens {}, *chaos {}, *glide {}, *vibrato {};
    std::atomic<float>* melOn {}, *melKey {}, *melScale {}, *melMood {}, *melDensity {}, *melBars {}, *melOctave {},
        *autoTune {}, *tonalize {}, *fxDry {};
    std::array<std::atomic<float>*, 8> evo {};
    std::atomic<float>* fltOn {}, *fltType {}, *fltCutoff {}, *fltRes {}, *fltMix {}, *distOn {}, *distDrive {}, *distMix {},
        *choOn {}, *choRate {}, *choDepth {}, *choMix {}, *dlyOn {}, *dlySync {}, *dlyTime {}, *dlyDiv {}, *dlyFeedback {},
        *dlyMix {}, *revOn {}, *revSize {}, *revDamp {}, *revWidth {}, *revMix {};

    void bind (juce::AudioProcessorValueTreeState& apvts);
};
} // namespace mf
