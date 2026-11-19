#include "Params.h"

namespace mf
{
juce::String pid::layer (int layerIndex, const char* suffix)
{
    return "l" + juce::String (layerIndex + 1) + "_" + suffix;
}

juce::StringArray delayDivisionNames()
{
    return { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1" };
}

namespace
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using API = juce::AudioParameterInt;
using Range = juce::NormalisableRange<float>;

Range timeRange (float lo, float hi, float centre)
{
    Range r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

std::unique_ptr<APF> fl (const juce::String& id, const juce::String& name, Range r, float def, const juce::String& label = {})
{
    return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, r, def, juce::AudioParameterFloatAttributes().withLabel (label));
}
std::unique_ptr<APB> bl (const juce::String& id, const juce::String& name, bool def)
{
    return std::make_unique<APB> (juce::ParameterID { id, 1 }, name, def);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kNumLayers; ++i)
    {
        const auto n = "L" + juce::String (i + 1) + " ";
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("layer" + juce::String (i + 1), "Layer " + juce::String (i + 1), "|");
        g->addChild (fl (pid::layer (i, "start"), n + "Start", Range (0.0f, 1.0f), 0.0f));
        g->addChild (fl (pid::layer (i, "end"), n + "End", Range (0.0f, 1.0f), 1.0f));
        g->addChild (bl (pid::layer (i, "loop"), n + "Loop", false));
        g->addChild (fl (pid::layer (i, "loopStart"), n + "Loop Start", Range (0.0f, 1.0f), 0.2f));
        g->addChild (fl (pid::layer (i, "loopEnd"), n + "Loop End", Range (0.0f, 1.0f), 0.8f));
        g->addChild (fl (pid::layer (i, "xfade"), n + "Loop Crossfade", timeRange (0.0f, 500.0f, 40.0f), 15.0f, "ms"));
        g->addChild (bl (pid::layer (i, "reverse"), n + "Reverse", false));
        g->addChild (std::make_unique<APC> (juce::ParameterID { pid::layer (i, "mpMode"), 1 }, n + "Multipoint",
                                            juce::StringArray { "Off", "Key", "Velocity", "Cycle" }, 0));
        g->addChild (bl (pid::layer (i, "mute"), n + "Mute", false));
        g->addChild (fl (pid::layer (i, "gain"), n + "Level", Range (-48.0f, 12.0f, 0.1f), i == 0 ? 0.0f : -6.0f, "dB"));
        g->addChild (fl (pid::layer (i, "pan"), n + "Pan", Range (-1.0f, 1.0f), 0.0f));
        g->addChild (std::make_unique<API> (juce::ParameterID { pid::layer (i, "semi"), 1 }, n + "Semitones", -24, 24, 0));
        g->addChild (fl (pid::layer (i, "cents"), n + "Cents", Range (-100.0f, 100.0f, 1.0f), 0.0f, "ct"));
        g->addChild (bl (pid::layer (i, "solo"), n + "Solo", false));
        layout.add (std::move (g));
    }

    layout.add (fl (pid::attack, "Attack", timeRange (0.0f, 5000.0f, 200.0f), 1.0f, "ms"));
    layout.add (fl (pid::decay, "Decay", timeRange (1.0f, 5000.0f, 400.0f), 400.0f, "ms"));
    layout.add (fl (pid::sustain, "Sustain", Range (0.0f, 1.0f), 1.0f));
    layout.add (fl (pid::release, "Release", timeRange (1.0f, 8000.0f, 400.0f), 250.0f, "ms"));
    layout.add (fl (pid::velSens, "Velocity", Range (0.0f, 1.0f), 0.8f));
    layout.add (fl (pid::glide, "Glide", timeRange (0.0f, 2000.0f, 200.0f), 0.0f, "ms"));
    layout.add (fl (pid::vibrato, "Vibrato", Range (0.0f, 1.0f), 0.0f));

    layout.add (bl (pid::melOn, "Melody", false));
    layout.add (std::make_unique<APC> (juce::ParameterID { pid::melKey, 1 }, "Melody Key",
                                       juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9));
    layout.add (std::make_unique<APC> (juce::ParameterID { pid::melScale, 1 }, "Melody Scale",
                                       juce::StringArray { "Major", "Minor", "Dorian", "Maj Penta", "Min Penta", "Harm Minor", "Lydian", "Phrygian" }, 1));
    layout.add (fl (pid::melMood, "Melody Mood", Range (0.0f, 1.0f), 0.4f));
    layout.add (fl (pid::melDensity, "Melody Density", Range (0.0f, 1.0f), 0.5f));
    layout.add (std::make_unique<APC> (juce::ParameterID { pid::melBars, 1 }, "Melody Bars", juce::StringArray { "1", "2", "4", "8" }, 2));
    layout.add (std::make_unique<API> (juce::ParameterID { pid::melOctave, 1 }, "Melody Octave", -2, 2, 0));
    layout.add (bl (pid::autoTune, "Auto Tune", true));
    layout.add (bl (pid::tonalize, "Tonalize Noise", true));
    layout.add (fl (pid::fxDry, "FX Dry", Range (0.0f, 1.0f), 1.0f));

    layout.add (fl (pid::chaos, "Chaos", Range (0.0f, 100.0f, 0.1f), 40.0f, "%"));
    const std::array<const char*, 8> evoNames { "Pitch Shift", "Grain Shuffle", "Envelope Filter", "Saturation",
                                                "Time Stretch", "Ring Mod", "Reverse Tail", "Transient Shaper" };
    for (size_t i = 0; i < pid::evoProcess.size(); ++i)
        layout.add (bl (pid::evoProcess[i], juce::String ("Evolve ") + evoNames[i], true));

    layout.add (bl (pid::fltOn, "Filter On", false));
    layout.add (std::make_unique<APC> (juce::ParameterID { pid::fltType, 1 }, "Filter Type", juce::StringArray { "LP", "HP", "BP" }, 0));
    layout.add (fl (pid::fltCutoff, "Filter Cutoff", timeRange (20.0f, 20000.0f, 1000.0f), 8000.0f, "Hz"));
    layout.add (fl (pid::fltRes, "Filter Resonance", Range (0.0f, 1.0f), 0.2f));
    layout.add (fl (pid::fltMix, "Filter Mix", Range (0.0f, 1.0f), 1.0f));

    layout.add (bl (pid::distOn, "Distortion On", false));
    layout.add (fl (pid::distDrive, "Distortion Drive", Range (0.0f, 1.0f), 0.3f));
    layout.add (fl (pid::distMix, "Distortion Mix", Range (0.0f, 1.0f), 1.0f));

    layout.add (bl (pid::choOn, "Chorus On", false));
    layout.add (fl (pid::choRate, "Chorus Rate", timeRange (0.05f, 5.0f, 0.8f), 0.6f, "Hz"));
    layout.add (fl (pid::choDepth, "Chorus Depth", Range (0.0f, 1.0f), 0.35f));
    layout.add (fl (pid::choMix, "Chorus Mix", Range (0.0f, 1.0f), 0.5f));

    layout.add (bl (pid::dlyOn, "Delay On", false));
    layout.add (bl (pid::dlySync, "Delay Sync", true));
    layout.add (fl (pid::dlyTime, "Delay Time", timeRange (1.0f, 2000.0f, 300.0f), 350.0f, "ms"));
    layout.add (std::make_unique<APC> (juce::ParameterID { pid::dlyDiv, 1 }, "Delay Division", delayDivisionNames(), 4));
    layout.add (fl (pid::dlyFeedback, "Delay Feedback", Range (0.0f, 0.95f), 0.35f));
    layout.add (fl (pid::dlyMix, "Delay Mix", Range (0.0f, 1.0f), 0.25f));

    layout.add (bl (pid::revOn, "Reverb On", false));
    layout.add (fl (pid::revSize, "Reverb Size", Range (0.0f, 1.0f), 0.6f));
    layout.add (fl (pid::revDamp, "Reverb Damping", Range (0.0f, 1.0f), 0.4f));
    layout.add (fl (pid::revWidth, "Reverb Width", Range (0.0f, 1.0f), 1.0f));
    layout.add (fl (pid::revMix, "Reverb Mix", Range (0.0f, 1.0f), 0.25f));

    layout.add (fl (pid::masterGain, "Master", Range (-48.0f, 6.0f, 0.1f), -3.0f, "dB"));
    layout.add (bl (pid::limiterOn, "Limiter", true));
    layout.add (fl (pid::limiterCeiling, "Limiter Ceiling", Range (-12.0f, 0.0f, 0.1f), -0.3f, "dB"));
    return layout;
}

void ParamRefs::bind (juce::AudioProcessorValueTreeState& apvts)
{
    auto get = [&apvts] (const juce::String& id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };
    for (int i = 0; i < kNumLayers; ++i)
    {
        auto& l = layers[(size_t) i];
        l.start = get (pid::layer (i, "start"));
        l.end = get (pid::layer (i, "end"));
        l.loop = get (pid::layer (i, "loop"));
        l.loopStart = get (pid::layer (i, "loopStart"));
        l.loopEnd = get (pid::layer (i, "loopEnd"));
        l.xfade = get (pid::layer (i, "xfade"));
        l.reverse = get (pid::layer (i, "reverse"));
        l.mpMode = get (pid::layer (i, "mpMode"));
        l.mute = get (pid::layer (i, "mute"));
        l.gain = get (pid::layer (i, "gain"));
        l.pan = get (pid::layer (i, "pan"));
        l.semi = get (pid::layer (i, "semi"));
        l.cents = get (pid::layer (i, "cents"));
        l.solo = get (pid::layer (i, "solo"));
    }
    masterGain = get (pid::masterGain);
    limiterOn = get (pid::limiterOn);
    limiterCeiling = get (pid::limiterCeiling);
    attack = get (pid::attack);
    decay = get (pid::decay);
    sustain = get (pid::sustain);
    release = get (pid::release);
    velSens = get (pid::velSens);
    glide = get (pid::glide);
    vibrato = get (pid::vibrato);
    melOn = get (pid::melOn); melKey = get (pid::melKey); melScale = get (pid::melScale); melMood = get (pid::melMood);
    melDensity = get (pid::melDensity); melBars = get (pid::melBars); melOctave = get (pid::melOctave);
    autoTune = get (pid::autoTune); tonalize = get (pid::tonalize); fxDry = get (pid::fxDry);
    chaos = get (pid::chaos);
    for (size_t i = 0; i < evo.size(); ++i)
        evo[i] = get (pid::evoProcess[i]);
    fltOn = get (pid::fltOn); fltType = get (pid::fltType); fltCutoff = get (pid::fltCutoff); fltRes = get (pid::fltRes); fltMix = get (pid::fltMix);
    distOn = get (pid::distOn); distDrive = get (pid::distDrive); distMix = get (pid::distMix);
    choOn = get (pid::choOn); choRate = get (pid::choRate); choDepth = get (pid::choDepth); choMix = get (pid::choMix);
    dlyOn = get (pid::dlyOn); dlySync = get (pid::dlySync); dlyTime = get (pid::dlyTime); dlyDiv = get (pid::dlyDiv);
    dlyFeedback = get (pid::dlyFeedback); dlyMix = get (pid::dlyMix);
    revOn = get (pid::revOn); revSize = get (pid::revSize); revDamp = get (pid::revDamp); revWidth = get (pid::revWidth); revMix = get (pid::revMix);
}
} // namespace mf
