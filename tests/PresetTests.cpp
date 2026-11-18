#include <catch2/catch_test_macros.hpp>
#include "core/Presets.h"
#include "plugin/PluginProcessor.h"
#include "TestHelpers.h"

using namespace testutil;

TEST_CASE ("Five synthesized factory presets", "[presets]")
{
    REQUIRE (mf::factory::count() == 5);
    juce::StringArray names;
    for (int i = 0; i < mf::factory::count(); ++i)
    {
        auto p = mf::factory::create (i);
        CHECK (p.name.isNotEmpty());
        names.addIfNotAlreadyThere (p.name);
        int loaded = 0;
        for (auto& L : p.layers)
            if (L.audio != nullptr)
            {
                ++loaded;
                REQUIRE (allFinite (L.audio->audio));
                CHECK (peak (L.audio->audio) > 0.3f);
                CHECK (peak (L.audio->audio) <= 1.0f);
            }
        CHECK (loaded >= 1);
    }
    CHECK (names.size() == 5);
}

TEST_CASE ("Factory presets are deterministic", "[presets]")
{
    auto a = mf::factory::create (3), b = mf::factory::create (3);
    REQUIRE (a.layers[0].audio->length() == b.layers[0].audio->length());
    for (int i = 0; i < a.layers[0].audio->length(); i += 97)
        REQUIRE (a.layers[0].audio->audio.getSample (0, i) == b.layers[0].audio->audio.getSample (0, i));
}

TEST_CASE ("User preset save/load round-trip (JSON + audio)", "[presets]")
{
    auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mutaform_preset_test");
    root.deleteRecursively();

    MutaformProcessor a;
    a.presetRoot = root;
    a.setLayerSample (0, makeSine (330.0, 48000.0, 9000), "sine");
    a.setLayerSample (3, makeRamp (5000), "ramp");
    a.setMultipoints (0, { 0.1f, 0.6f });
    a.apvts.getParameter (mf::pid::layer (0, "semi"))->setValueNotifyingHost (a.apvts.getParameter (mf::pid::layer (0, "semi"))->convertTo0to1 (7.0f));
    a.apvts.getParameter (mf::pid::revOn)->setValueNotifyingHost (1.0f);

    juce::String err;
    REQUIRE (a.saveUserPreset ("My: Test/Preset", err));
    auto dirs = mf::listUserPresets (root);
    REQUIRE (dirs.size() == 1);
    CHECK (dirs[0].getChildFile ("preset.json").existsAsFile());
    CHECK (dirs[0].getChildFile ("layer1.wav").existsAsFile());
    CHECK (dirs[0].getChildFile ("layer4.wav").existsAsFile());
    CHECK_FALSE (dirs[0].getChildFile ("layer2.wav").existsAsFile());

    MutaformProcessor b;
    b.presetRoot = root;
    REQUIRE (b.loadPresetById ("user:" + dirs[0].getFileName(), err));
    CHECK (b.getCurrentPresetName() == "My: Test/Preset");
    CHECK (b.getLayer (0).hasSample());
    CHECK_FALSE (b.getLayer (1).hasSample());
    CHECK (b.getLayer (3).hasSample());
    CHECK (b.getLayer (0).multipoints.size() == 2);
    CHECK (std::abs (b.apvts.getRawParameterValue (mf::pid::layer (0, "semi"))->load() - 7.0f) < 1e-4f);
    CHECK (b.apvts.getRawParameterValue (mf::pid::revOn)->load() > 0.5f);
    // float WAV keeps audio exact
    const auto& sa = *a.getLayer (0).current();
    const auto& sb = *b.getLayer (0).current();
    REQUIRE (sa.length() == sb.length());
    for (int i = 0; i < sa.length(); i += 13)
        REQUIRE (sa.audio.getSample (0, i) == sb.audio.getSample (0, i));
    root.deleteRecursively();
}

TEST_CASE ("Factory preset loads into the processor", "[presets][processor]")
{
    MutaformProcessor p;
    juce::String err;
    for (int i = 0; i < mf::factory::count(); ++i)
    {
        REQUIRE (p.loadPresetById ("factory:" + juce::String (i), err));
        CHECK (p.getCurrentPresetName() == mf::factory::name (i));
        CHECK (p.getLayer (0).hasSample());
    }
    CHECK_FALSE (p.loadPresetById ("factory:99", err));
}

TEST_CASE ("Randomizer stays in range and is reproducible", "[presets][random]")
{
    MutaformProcessor a, b;
    for (auto* p : { &a, &b })
        for (int l = 0; l < mf::kNumLayers; ++l)
            p->setLayerSample (l, makeSine (200.0, 48000.0, 4800), "s");
    a.randomizeParameters (99);
    b.randomizeParameters (99);
    for (auto* prm : a.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm);
        const float v = rp->getValue();
        REQUIRE (v >= 0.0f);
        REQUIRE (v <= 1.0f);
        REQUIRE (v == b.apvts.getParameter (rp->getParameterID())->getValue());
    }
    // master and limiter untouched
    CHECK (a.apvts.getParameter (mf::pid::limiterOn)->getValue() > 0.5f);

    a.setPlayConfigDetails (0, 2, 48000.0, 256);
    a.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buf (2, 256);
    for (int blk = 0; blk < 200; ++blk)
    {
        juce::MidiBuffer m;
        if (blk % 20 == 0) m.addEvent (juce::MidiMessage::noteOn (1, 48 + blk % 24, 1.0f), 0);
        a.processBlock (buf, m);
        REQUIRE (allFinite (buf));
    }
}
