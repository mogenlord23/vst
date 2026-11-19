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
    float pitchMidi = 60.0f; // detected fundamental (fractional MIDI note)
    float pitchConfidence = 0.0f;
};

SoundFeatures analyseSound (const SampleData& s);

/** Short, deterministic two-word name for a variation, e.g. "VELVET SPARK". */
juce::String variationName (juce::uint64 seed);
} // namespace mf

namespace mf
{
struct PitchInfo
{
    float midi = 60.0f;      // fractional MIDI note of the fundamental
    float confidence = 0.0f; // 0..1 (YIN periodicity)
};

/** YIN fundamental estimate on the sustained part of the sound. */
PitchInfo detectPitch (const SampleData& s);

/** STRETCHTONE "tonalize": runs a sound through tuned comb resonators so that unpitched
    material (noise, claps, speech) rings at `freqHz`. Offline, allocates. */
std::shared_ptr<SampleData> tonalize (const SampleData& s, double freqHz, float amount = 0.75f);
} // namespace mf
