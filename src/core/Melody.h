#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace mf
{
/** MELODY engine: rule-based melody writer (no AI). Always in key, phrase-structured:
    chord progression -> motif rhythm -> chord-tone / passing-tone pitches -> cadence. */
enum class Scale : int { major = 0, minor, dorian, majorPentatonic, minorPentatonic, harmonicMinor, lydian, phrygian, count };

juce::StringArray scaleNames();
juce::StringArray keyNames();
const std::vector<int>& scaleIntervals (Scale s);

struct MelodySettings
{
    int key = 0;              // 0 = C .. 11 = B
    Scale scale = Scale::minor;
    float mood = 0.4f;        // 0 calm/legato .. 1 energetic/syncopated
    float density = 0.5f;     // 0 sparse .. 1 busy
    int bars = 4;             // 1, 2, 4 or 8
    int octave = 0;           // -2 .. +2
    juce::uint64 seed = 1;
};

struct MelodyNote
{
    int start = 0;            // in 16th steps from pattern start
    int length = 1;           // in 16th steps
    int note = 60;            // MIDI note
    float velocity = 0.8f;
};

struct MelodyPattern
{
    static constexpr int stepsPerBar = 16;
    int bars = 4;
    std::vector<MelodyNote> notes; // sorted by start
    std::vector<int> chordRoots;   // per half-bar: MIDI root of the chord (for display)
    int totalSteps() const noexcept { return bars * stepsPerBar; }
};

MelodyPattern generateMelody (const MelodySettings& s);

/** Snap a MIDI note to the nearest note of key/scale. */
int snapToScale (int note, int key, Scale scale);
} // namespace mf
