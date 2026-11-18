#include "Presets.h"

namespace mf
{
juce::File userPresetRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Mutaform")
        .getChildFile ("Presets");
}

juce::String safePresetDirName (const juce::String& name)
{
    auto s = juce::File::createLegalFileName (name.trim()).trim();
    s = s.removeCharacters ("./\\");
    return s.isEmpty() ? juce::String ("Untitled") : s.substring (0, 64);
}

bool savePreset (const juce::File& root, const PresetData& p, juce::String& error)
{
    const auto dir = root.getChildFile (safePresetDirName (p.name));
    if (! dir.createDirectory())
    {
        error = "Cannot create " + dir.getFullPathName();
        return false;
    }

    juce::DynamicObject::Ptr json = new juce::DynamicObject();
    json->setProperty ("format", "mutaform-preset");
    json->setProperty ("version", 1);
    json->setProperty ("name", p.name);

    juce::DynamicObject::Ptr params = new juce::DynamicObject();
    for (const auto& [id, v] : p.params)
        params->setProperty (id, v);
    json->setProperty ("params", juce::var (params.get()));

    juce::Array<juce::var> layers;
    for (int i = 0; i < kNumLayers; ++i)
    {
        const auto& L = p.layers[(size_t) i];
        juce::DynamicObject::Ptr lo = new juce::DynamicObject();
        const auto file = dir.getChildFile ("layer" + juce::String (i + 1) + ".wav");
        if (L.audio != nullptr)
        {
            if (! writeWav (*L.audio, file))
            {
                error = "Cannot write " + file.getFullPathName();
                return false;
            }
            lo->setProperty ("file", file.getFileName());
            lo->setProperty ("name", L.audio->name);
        }
        else
        {
            file.deleteFile();
        }
        juce::Array<juce::var> mp;
        for (auto v : L.multipoints)
            mp.add (v);
        lo->setProperty ("multipoints", mp);
        layers.add (juce::var (lo.get()));
    }
    json->setProperty ("layers", layers);

    if (! dir.getChildFile ("preset.json").replaceWithText (juce::JSON::toString (juce::var (json.get()))))
    {
        error = "Cannot write preset.json";
        return false;
    }
    return true;
}

std::optional<PresetData> loadPreset (const juce::File& dir, juce::String& error)
{
    const auto jf = dir.getChildFile ("preset.json");
    const auto json = juce::JSON::parse (jf.loadFileAsString());
    if (! json.isObject() || json["format"].toString() != "mutaform-preset")
    {
        error = "Not a Mutaform preset: " + dir.getFileName();
        return std::nullopt;
    }

    PresetData p;
    p.name = json["name"].toString();
    if (p.name.isEmpty())
        p.name = dir.getFileName();

    if (auto* obj = json["params"].getDynamicObject())
        for (const auto& prop : obj->getProperties())
            p.params[prop.name.toString()] = (float) (double) prop.value;

    if (auto* arr = json["layers"].getArray())
    {
        for (int i = 0; i < std::min (kNumLayers, arr->size()); ++i)
        {
            const auto& lo = arr->getReference (i);
            auto& L = p.layers[(size_t) i];
            const auto fname = lo["file"].toString();
            if (fname.isNotEmpty())
            {
                juce::String err;
                auto s = loadSampleFromFile (dir.getChildFile (fname), err);
                if (s == nullptr)
                {
                    error = err;
                    return std::nullopt;
                }
                if (lo["name"].toString().isNotEmpty())
                    s->name = lo["name"].toString();
                L.audio = std::move (s);
            }
            if (auto* mp = lo["multipoints"].getArray())
                for (auto& v : *mp)
                    L.multipoints.push_back ((float) (double) v);
        }
    }
    return p;
}

juce::Array<juce::File> listUserPresets (const juce::File& root)
{
    juce::Array<juce::File> out;
    for (const auto& entry : juce::RangedDirectoryIterator (root, false, "*", juce::File::findDirectories))
        if (entry.getFile().getChildFile ("preset.json").existsAsFile())
            out.add (entry.getFile());
    std::sort (out.begin(), out.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    return out;
}
} // namespace mf
