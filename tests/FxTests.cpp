#include <catch2/catch_test_macros.hpp>
#include "core/FxChain.h"
#include "plugin/PluginProcessor.h"
#include "TestHelpers.h"

using namespace testutil;

TEST_CASE ("FX chain with everything on stays finite, denormal-free and under the ceiling", "[fx]")
{
    mf::FxChain fx;
    fx.prepare (48000.0, 512);
    mf::FxSettings s;
    s.fltOn = s.distOn = s.choOn = s.dlyOn = s.revOn = true;
    s.fltRes = 1.0f;
    s.distDrive = 1.0f;
    s.dlyFeedback = 0.95f;
    s.masterGainDb = 6.0f;
    s.limiterCeilingDb = -1.0f;
    juce::AudioBuffer<float> buf (2, 512);
    mf::Rng r (1);
    const float ceiling = juce::Decibels::decibelsToGain (-1.0f);
    for (int b = 0; b < 400; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float v = b < 20 ? (float) (r.uniform() * 2 - 1) : 0.0f; // burst then silence (tails)
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        s.fltType = b % 3;
        fx.process (buf, 512, s);
        REQUIRE (allFinite (buf));
        REQUIRE (noDenormals (buf));
        REQUIRE (peak (buf) <= ceiling + 1e-5f);
    }
}

TEST_CASE ("Bypassed FX chain is transparent", "[fx]")
{
    mf::FxChain fx;
    fx.prepare (48000.0, 256);
    mf::FxSettings s;
    s.limiterOn = false;
    juce::AudioBuffer<float> buf (2, 256);
    for (int i = 0; i < 256; ++i)
    {
        buf.setSample (0, i, 0.3f * std::sin (i * 0.1f));
        buf.setSample (1, i, -0.2f);
    }
    juce::AudioBuffer<float> ref (buf);
    fx.process (buf, 256, s);
    for (int i = 0; i < 256; ++i)
    {
        REQUIRE (std::abs (buf.getSample (0, i) - ref.getSample (0, i)) < 1e-6f);
        REQUIRE (std::abs (buf.getSample (1, i) - ref.getSample (1, i)) < 1e-6f);
    }
}

TEST_CASE ("Tempo-synced delay time", "[fx]")
{
    mf::FxSettings s;
    s.dlySync = true;
    s.bpm = 120.0;
    s.dlyDiv = 7; // 1/4
    CHECK (std::abs (mf::FxChain::delayTimeMs (s) - 500.0) < 1e-6);
    s.dlyDiv = 4; // 1/8
    CHECK (std::abs (mf::FxChain::delayTimeMs (s) - 250.0) < 1e-6);
    s.dlySync = false;
    s.dlyTimeMs = 123.0f;
    CHECK (std::abs (mf::FxChain::delayTimeMs (s) - 123.0) < 1e-4);
}

TEST_CASE ("Full processor output has no NaN or denormals", "[processor][fx]")
{
    MutaformProcessor proc;
    proc.setPlayConfigDetails (0, 2, 44100.0, 300);
    proc.prepareToPlay (44100.0, 300);
    for (int l = 0; l < mf::kNumLayers; ++l)
        proc.setLayerSample (l, makeSine (110.0 * (l + 1), 44100.0, 20000), "s");
    for (auto id : { mf::pid::fltOn, mf::pid::distOn, mf::pid::choOn, mf::pid::dlyOn, mf::pid::revOn })
        proc.apvts.getParameter (id)->setValueNotifyingHost (1.0f);
    proc.apvts.getParameter (mf::pid::layer (2, "loop"))->setValueNotifyingHost (1.0f);
    proc.apvts.getParameter (mf::pid::layer (3, "reverse"))->setValueNotifyingHost (1.0f);

    juce::AudioBuffer<float> buf (2, 300);
    for (int b = 0; b < 600; ++b)
    {
        juce::MidiBuffer m;
        if (b % 10 == 0) m.addEvent (juce::MidiMessage::noteOn (1, 36 + b % 48, 0.9f), b % 300);
        if (b % 10 == 5) m.addEvent (juce::MidiMessage::noteOff (1, 36 + (b - 5) % 48), 0);
        if (b == 400) m.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        proc.processBlock (buf, m);
        REQUIRE (allFinite (buf));
        REQUIRE (noDenormals (buf));
        REQUIRE (peak (buf) <= 1.0f);
    }
}
