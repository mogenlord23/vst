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
