#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace testutil
{
inline bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i)))
                return false;
    return true;
}

inline bool noDenormals (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (c, i);
            if (v != 0.0f && std::fpclassify (v) == FP_SUBNORMAL)
                return false;
        }
    return true;
}

inline float peak (const juce::AudioBuffer<float>& b)
{
    float p = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c)
        p = std::max (p, b.getMagnitude (c, 0, b.getNumSamples()));
    return p;
}
} // namespace testutil

#include "core/SampleData.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace testutil
{
inline std::shared_ptr<mf::SampleData> makeSine (double freq, double sr, int len, float amp = 0.5f)
{
    auto s = std::make_shared<mf::SampleData>();
    s->sampleRate = sr;
    s->name = "sine";
    s->audio.setSize (2, len);
    for (int i = 0; i < len; ++i)
    {
        const float v = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr);
        s->audio.setSample (0, i, v);
        s->audio.setSample (1, i, v);
    }
    return s;
}

inline std::shared_ptr<mf::SampleData> makeRamp (int len, double sr = 48000.0)
{
    auto s = std::make_shared<mf::SampleData>();
    s->sampleRate = sr;
    s->name = "ramp";
    s->audio.setSize (2, len);
    for (int i = 0; i < len; ++i)
    {
        const float v = 0.1f + 0.8f * (float) i / (float) (len - 1);
        s->audio.setSample (0, i, v);
        s->audio.setSample (1, i, v);
    }
    return s;
}

/** Estimates frequency from positive-going zero crossings over [from, to). */
inline double estimateFrequency (const float* d, int from, int to, double sr)
{
    int first = -1, last = -1, count = 0;
    for (int i = from + 1; i < to; ++i)
        if (d[i - 1] <= 0.0f && d[i] > 0.0f)
        {
            if (first < 0) first = i;
            last = i;
            ++count;
        }
    if (count < 2)
        return 0.0;
    return (count - 1) * sr / (double) (last - first);
}

inline juce::MidiBuffer noteOn (int note, float vel = 1.0f, int pos = 0)
{
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::noteOn (1, note, vel), pos);
    return m;
}
} // namespace testutil
