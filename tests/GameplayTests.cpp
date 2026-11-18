#include <catch2/catch_test_macros.hpp>
#include "plugin/PluginProcessor.h"
#include "core/SoundFeatures.h"
#include "core/SamplerEngine.h"
#include "TestHelpers.h"

using namespace testutil;

namespace
{
void setParam (MutaformProcessor& p, const juce::String& id, float value)
{
    auto* rp = p.apvts.getParameter (id);
    rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}
std::shared_ptr<mf::SampleData> shortTone() { return makeSine (330.0, 48000.0, 6000); }
} // namespace

TEST_CASE ("Group EVOLVE advances the level and level navigation steps back/forward", "[gameplay]")
{
    MutaformProcessor p;
    p.setLayerSample (0, shortTone(), "a");
    p.setLayerSample (2, shortTone(), "b");
    CHECK (p.getLevel() == 0);
    CHECK_FALSE (p.canStepGeneration (-1));

    REQUIRE (p.evolveGroup() == 2);
    REQUIRE (p.waitUntilIdle());
    const int l1 = p.getLevel();
    CHECK (l1 > 0);
    REQUIRE (p.evolveGroup (0x1f, 3) == 2); // three mutations per layer in one go
    REQUIRE (p.waitUntilIdle());
    CHECK (p.getLayer (0).index == 4);
    CHECK (p.getLevel() > l1);

    REQUIRE (p.canStepGeneration (-1));
    REQUIRE (p.stepGeneration (-1));
    REQUIRE (p.waitUntilIdle());
    CHECK (p.getLayer (0).index == 1);
    CHECK (p.getLevel() == l1);
    REQUIRE (p.stepGeneration (1));
    REQUIRE (p.waitUntilIdle());
    CHECK (p.getLayer (2).index == 4);
}

TEST_CASE ("Locked layers are skipped by group EVOLVE and the dice", "[gameplay]")
{
    MutaformProcessor p;
    p.setLayerSample (0, shortTone(), "a");
    p.setLayerSample (1, shortTone(), "b");
    p.setLocked (1, true);
    REQUIRE (p.evolveGroup() == 1);
    REQUIRE (p.waitUntilIdle());
    CHECK (p.getLayer (0).index == 1);
    CHECK (p.getLayer (1).index == 0);

    p.rollDice();
    REQUIRE (p.waitUntilIdle());
    CHECK (p.getLayer (1).index == 0);
    CHECK (p.getLayer (0).index >= 2);
    REQUIRE (p.canUndoDice());
    REQUIRE (p.undoDice());
    REQUIRE (p.waitUntilIdle());
    CHECK (p.getLayer (0).index == 1);
    CHECK_FALSE (p.canUndoDice());
}

TEST_CASE ("Favourites and locks survive state save/restore", "[gameplay][state]")
{
    MutaformProcessor a;
    a.setLayerSample (0, shortTone(), "a");
    REQUIRE (a.evolveLayer (0, 42));
    REQUIRE (a.waitUntilIdle());
    a.toggleFavorite (0, 1);
    a.setLocked (0, true);
    juce::MemoryBlock mb;
    a.getStateInformation (mb);
    MutaformProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (b.getLayer (0).locked);
    CHECK (b.getLayer (0).history[1].favorite);
    CHECK (b.getLayer (0).history[1].hasFeatures);
}

TEST_CASE ("Sound features tag dark/bright and plucky/smooth sounds", "[gameplay][features]")
{
    auto low = makeSine (110.0, 48000.0, 48000);
    auto high = makeSine (4000.0, 48000.0, 48000);
    CHECK (mf::analyseSound (*low).tags.contains ("DARK"));
    CHECK (mf::analyseSound (*high).tags.contains ("BRIGHT"));

    auto pluck = makeSine (440.0, 48000.0, 48000);
    for (int i = 0; i < pluck->length(); ++i)
        for (int c = 0; c < 2; ++c)
            pluck->audio.setSample (c, i, pluck->audio.getSample (c, i) * std::exp (-i / 2000.0f));
    CHECK (mf::analyseSound (*pluck).tags.contains ("PLUCKY"));
    CHECK (mf::analyseSound (*low).tags.contains ("SMOOTH"));
    CHECK (mf::variationName (1) == mf::variationName (1));
    CHECK (mf::variationName (1) != mf::variationName (2));
}

TEST_CASE ("Glide slides pitch from the previous note; vibrato modulates", "[engine][gameplay]")
{
    mf::SamplerEngine e;
    e.prepare (48000.0, 256);
    e.setSample (0, makeSine (440.0, 48000.0, 96000));
    mf::EngineSettings s;
    s.attackMs = 0; s.releaseMs = 1; s.velSens = 0;
    s.glideMs = 300.0f;
    juce::AudioBuffer<float> buf (2, 256);
    std::vector<float> out;
    auto run = [&] (juce::MidiBuffer m, int blocks)
    {
        for (int b = 0; b < blocks; ++b)
        {
            e.render (buf, b == 0 ? m : juce::MidiBuffer(), s);
            for (int i = 0; i < 256; ++i)
                out.push_back (buf.getSample (0, i));
        }
    };
    run (noteOn (60), 10);
    juce::MidiBuffer m2;
    m2.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    m2.addEvent (juce::MidiMessage::noteOn (1, 72, 1.0f), 0);
    out.clear();
    run (m2, 200);
    const double early = estimateFrequency (out.data(), 200, 2600, 48000.0);   // still near the old pitch
    const double late = estimateFrequency (out.data(), 30000, 50000, 48000.0); // arrived at the new pitch
    CHECK (early < 700.0);
    CHECK (std::abs (late - 880.0) < 10.0);

    // vibrato: pitch varies over time
    mf::SamplerEngine v;
    v.prepare (48000.0, 256);
    v.setSample (0, makeSine (440.0, 48000.0, 96000));
    s.glideMs = 0.0f;
    s.vibrato = 1.0f;
    std::vector<float> vo;
    for (int b = 0; b < 300; ++b)
    {
        v.render (buf, b == 0 ? noteOn (60) : juce::MidiBuffer(), s);
        for (int i = 0; i < 256; ++i)
            vo.push_back (buf.getSample (0, i));
    }
    double fmin = 1e9, fmax = 0;
    for (int start = 30000; start + 2400 < (int) vo.size(); start += 1200)
    {
        const double f = estimateFrequency (vo.data(), start, start + 2400, 48000.0);
        fmin = std::min (fmin, f);
        fmax = std::max (fmax, f);
    }
    CHECK (fmax - fmin > 8.0);
    CHECK (allFinite (buf));
}
