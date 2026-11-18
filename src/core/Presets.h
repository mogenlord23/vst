#pragma once
#include "SampleData.h"
#include "Params.h"
#include <map>
#include <optional>

namespace mf
{
struct PresetLayer
{
    SamplePtr audio;                 // null = empty layer
    std::vector<float> multipoints;
};

struct PresetData
{
    juce::String name;
    std::map<juce::String, float> params;  // parameter ID -> real (denormalised) value
    std::array<PresetLayer, kNumLayers> layers;
};

/** %APPDATA%/Mutaform/Presets on Windows (~/.config/Mutaform/Presets elsewhere). */
juce::File userPresetRoot();

/** Writes <root>/<name>/preset.json and layerN.wav files. Overwrites an existing preset. */
bool savePreset (const juce::File& root, const PresetData& preset, juce::String& error);
std::optional<PresetData> loadPreset (const juce::File& presetDir, juce::String& error);

/** Sub-directories of root that contain a preset.json, sorted by name. */
juce::Array<juce::File> listUserPresets (const juce::File& root);
juce::String safePresetDirName (const juce::String& name);

namespace factory
{
    int count();
    juce::String name (int index);
    /** Builds the factory preset; all sounds are synthesised in code (deterministic). */
    PresetData create (int index, double sampleRate = 48000.0);
} // namespace factory
} // namespace mf
