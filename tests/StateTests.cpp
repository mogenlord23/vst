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
