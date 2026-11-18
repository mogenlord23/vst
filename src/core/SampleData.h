#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <vector>

namespace mf
{
/** Immutable (once published) stereo sample. Always 2 channels. */
struct SampleData
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    juce::String name;

    int length() const noexcept { return audio.getNumSamples(); }
    double seconds() const noexcept { return sampleRate > 0 ? length() / sampleRate : 0.0; }
};

using SamplePtr = std::shared_ptr<const SampleData>;

inline constexpr double kMaxSampleSeconds = 30.0;

/** Registers WAV, AIFF, FLAC, OGG and MP3 readers. */
juce::AudioFormatManager& audioFormats();

/** Converts an arbitrary reader into a stereo SampleData (mono duplicated, >2ch truncated). */
std::shared_ptr<SampleData> readSample (juce::AudioFormatReader& reader, const juce::String& name, juce::String& error);
std::shared_ptr<SampleData> loadSampleFromFile (const juce::File& file, juce::String& error);
std::shared_ptr<SampleData> loadSampleFromMemory (const void* data, size_t size, const juce::String& name, juce::String& error);

/** Lossless compact binary encoding (raw float32, gzip) used in plugin state. */
juce::MemoryBlock encodeSample (const SampleData& s);
std::shared_ptr<SampleData> decodeSample (const juce::MemoryBlock& block);

/** Writes 32-bit float WAV. */
bool writeWav (const SampleData& s, const juce::File& file);

/** Mono min/max peaks: result[2*i] = min, result[2*i+1] = max for each of numBins bins. */
std::vector<float> computePeaks (const SampleData& s, int numBins);

/** Removes DC and normalises the peak to `targetPeak` (linear). Returns gain applied. */
float normalise (juce::AudioBuffer<float>& audio, float targetPeak);
} // namespace mf
