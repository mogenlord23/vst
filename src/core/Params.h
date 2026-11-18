#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace mf
{
namespace pid
{
    inline constexpr const char* masterGain = "master_gain";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace mf
