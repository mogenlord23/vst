#pragma once
#include "SampleData.h"

namespace mf
{
/** Cheap descriptors used to tag variations in the library and to shape the mascot. */
struct SoundFeatures
{
    float brightness = 0.5f; // 0 dark .. 1 bright   (zero-crossing rate based)
    float punch = 0.5f;      // 0 smooth .. 1 plucky (attack peak vs. body level)
    float motion = 0.5f;     // 0 steady .. 1 moving (envelope / brightness fluctuation)
    juce::StringArray tags;  // e.g. { "BRIGHT", "PLUCKY", "MOVING" }
};

SoundFeatures analyseSound (const SampleData& s);

/** Short, deterministic two-word name for a variation, e.g. "VELVET SPARK". */
juce::String variationName (juce::uint64 seed);
} // namespace mf
