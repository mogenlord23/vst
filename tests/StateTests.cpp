#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "plugin/PluginProcessor.h"
#include "TestHelpers.h"

TEST_CASE ("Silence when no notes are played", "[processor]")
{
    MutaformProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < 50; ++block)
    {
        buf.clear();
        proc.processBlock (buf, midi);
        REQUIRE (testutil::peak (buf) == 0.0f);
    }
}

TEST_CASE ("Parameter state round-trips", "[state]")
{
    MutaformProcessor a;
    auto* p = a.apvts.getParameter (mf::pid::masterGain);
    REQUIRE (p != nullptr);
    p->setValueNotifyingHost (0.25f);

    juce::MemoryBlock mb;
    a.getStateInformation (mb);

    MutaformProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    REQUIRE_THAT (b.apvts.getParameter (mf::pid::masterGain)->getValue(),
                  Catch::Matchers::WithinAbs (0.25f, 1e-4));
}

#include "TestHelpers.h"

TEST_CASE ("Full state (samples, multipoints, history) round-trips", "[state]")
{
    MutaformProcessor a;
    a.setLayerSample (0, testutil::makeSine (220.0, 44100.0, 3000), "sine");
    a.setLayerSample (4, testutil::makeRamp (2000), "ramp");
    a.setMultipoints (4, { 0.75f, 0.25f });
    a.apvts.getParameter (mf::pid::layer (4, "reverse"))->setValueNotifyingHost (1.0f);
    REQUIRE (a.evolveLayer (0, 0xabcdef));
    REQUIRE (a.waitUntilIdle());

    juce::MemoryBlock mb;
    a.getStateInformation (mb);

    MutaformProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (b.getLayer (0).history.size() == 2);
    CHECK (b.getLayer (0).index == 1);
    CHECK (b.getLayer (0).history[1].seed == 0xabcdef);
    CHECK_FALSE (b.getLayer (1).hasSample());
    REQUIRE (b.getLayer (4).hasSample());
    CHECK (b.getLayer (4).current()->name == "ramp");
    REQUIRE (b.getLayer (4).multipoints.size() == 2);
    CHECK (b.getLayer (4).multipoints[0] == 0.25f); // stored sorted
    CHECK (b.apvts.getRawParameterValue (mf::pid::layer (4, "reverse"))->load() > 0.5f);

    const auto& x = *a.getLayer (0).current();
    const auto& y = *b.getLayer (0).current();
    REQUIRE (x.length() == y.length());
    for (int i = 0; i < x.length(); ++i)
        REQUIRE (x.audio.getSample (1, i) == y.audio.getSample (1, i));

    // garbage state must not crash or change anything
    const char junk[] = "nope";
    b.setStateInformation (junk, sizeof (junk));
    CHECK (b.getLayer (4).hasSample());
}
