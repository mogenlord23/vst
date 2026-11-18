#include <catch2/catch_test_macros.hpp>
#include "core/Evolver.h"
#include "plugin/PluginProcessor.h"
#include "TestHelpers.h"

using namespace testutil;

namespace
{
std::shared_ptr<mf::SampleData> makePluck()
{
    // decaying harmonic tone with a noisy attack
    auto s = std::make_shared<mf::SampleData>();
    s->sampleRate = 48000.0;
    const int len = 24000;
    s->audio.setSize (2, len);
    mf::Rng r (7);
    for (int i = 0; i < len; ++i)
    {
        const double t = i / 48000.0;
        const double env = std::exp (-t * 6.0);
        double v = 0.5 * std::sin (2 * juce::MathConstants<double>::pi * 220 * t) + 0.25 * std::sin (2 * juce::MathConstants<double>::pi * 440 * t);
        if (i < 600) v += (r.uniform() - 0.5) * (1.0 - i / 600.0);
        s->audio.setSample (0, i, (float) (v * env));
        s->audio.setSample (1, i, (float) (v * env * 0.9));
    }
    return s;
}

bool identical (const mf::SampleData& a, const mf::SampleData& b)
{
    if (a.length() != b.length())
        return false;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < a.length(); ++i)
            if (a.audio.getSample (c, i) != b.audio.getSample (c, i))
                return false;
    return true;
}
} // namespace

TEST_CASE ("Rng is deterministic", "[evolve]")
{
    mf::Rng a (42), b (42), c (43);
    bool differs = false;
    for (int i = 0; i < 1000; ++i)
    {
        const auto x = a.next();
        REQUIRE (x == b.next());
        differs = differs || x != c.next();
    }
    CHECK (differs);
}

TEST_CASE ("EVOLVE is deterministic for the same seed", "[evolve]")
{
    auto src = makePluck();
    for (float chaos : { 0.0f, 0.4f, 1.0f })
        for (juce::uint64 seed : { 1ull, 0xdeadbeefull, 123456789ull })
        {
            auto a = mf::evolve (*src, seed, chaos);
            auto b = mf::evolve (*src, seed, chaos);
            REQUIRE (a.audio != nullptr);
            REQUIRE (b.audio != nullptr);
            REQUIRE (identical (*a.audio, *b.audio));
            REQUIRE (a.steps == b.steps);
        }
}

TEST_CASE ("EVOLVE output differs across seeds and is clean", "[evolve]")
{
    auto src = makePluck();
    auto a = mf::evolve (*src, 11, 0.6f);
    auto b = mf::evolve (*src, 12, 0.6f);
    REQUIRE (a.audio != nullptr);
    REQUIRE (b.audio != nullptr);
    CHECK_FALSE (identical (*a.audio, *b.audio));
    for (auto* r : { &a, &b })
    {
        CHECK (allFinite (r->audio->audio));
        const float pk = peak (r->audio->audio);
        CHECK (pk <= 0.9f);
        CHECK (pk > 0.5f);
        CHECK (r->audio->seconds() <= mf::kMaxEvolveSeconds + 0.1);
    }
}

TEST_CASE ("CHAOS controls the number of processes", "[evolve]")
{
    auto src = makePluck();
    for (juce::uint64 seed = 1; seed < 30; ++seed)
    {
        CHECK (mf::evolve (*src, seed, 0.0f).steps.size() == 2);
        CHECK (mf::evolve (*src, seed, 1.0f).steps.size() == 5);
        const auto mid = mf::evolve (*src, seed, 0.5f).steps.size();
        CHECK (mid >= 3);
        CHECK (mid <= 4);
    }
}

TEST_CASE ("Every process renders finite audio", "[evolve]")
{
    auto src = makePluck();
    for (int p = 0; p < (int) mf::EvolveProcess::count; ++p)
        for (juce::uint64 seed = 1; seed < 6; ++seed)
        {
            auto r = mf::evolve (*src, seed, 1.0f, 1u << p);
            REQUIRE (r.audio != nullptr);
            REQUIRE (allFinite (r.audio->audio));
            REQUIRE (noDenormals (r.audio->audio));
        }
}

TEST_CASE ("Processor EVOLVE runs off-thread with undo/redo history", "[evolve][processor]")
{
    MutaformProcessor proc;
    proc.setLayerSample (0, makePluck(), "pluck");
    REQUIRE_FALSE (proc.canUndo (0));

    std::vector<juce::uint64> seeds;
    for (int i = 0; i < 25; ++i)
    {
        REQUIRE (proc.evolveLayer (0));
        REQUIRE (proc.isBusy (0));
        REQUIRE (proc.waitUntilIdle());
        seeds.push_back (proc.getLayer (0).history.back().seed);
    }
    const auto& L = proc.getLayer (0);
    CHECK ((int) L.history.size() == 26);
    CHECK (L.index == 25);

    // at least 20 undo steps
    for (int i = 0; i < 20; ++i)
    {
        REQUIRE (proc.undo (0));
        proc.waitUntilIdle();
    }
    CHECK (L.index == 5);
    REQUIRE (proc.redo (0));
    proc.waitUntilIdle();
    CHECK (L.index == 6);

    // the variation at index 6 can be reproduced from its seed
    auto chainSrc = proc.getLayer (0).history[5].audio;
    REQUIRE (chainSrc != nullptr);
    const auto& e6 = L.history[6];
    auto again = mf::evolve (*chainSrc, e6.seed, e6.chaos, e6.processMask);
    REQUIRE (identical (*again.audio, *L.history[6].audio));

    // evolving after undo truncates the redo branch
    REQUIRE (proc.evolveLayer (0, 777));
    proc.waitUntilIdle();
    CHECK (L.index == 7);
    CHECK ((int) L.history.size() == 8);
    CHECK (L.history.back().seed == 777);
}

TEST_CASE ("History is capped at 32 entries keeping the original", "[evolve][processor]")
{
    MutaformProcessor proc;
    auto src = makePluck();
    src->audio.setSize (2, 6000, true); // shorter = faster
    proc.setLayerSample (0, src, "short");
    for (int i = 0; i < MutaformProcessor::kMaxHistory + 4; ++i)
    {
        REQUIRE (proc.evolveLayer (0, (juce::uint64) (1000 + i)));
        REQUIRE (proc.waitUntilIdle());
    }
    const auto& L = proc.getLayer (0);
    CHECK ((int) L.history.size() == MutaformProcessor::kMaxHistory);
    CHECK (L.history[0].label == "Original");
    CHECK (L.history[1].root);
    CHECK (L.history.back().seed == 1000 + MutaformProcessor::kMaxHistory + 3);
    for (auto& e : L.history)
        CHECK (e.audio != nullptr);
}

TEST_CASE ("Restored state re-renders missing variations identically", "[evolve][state]")
{
    MutaformProcessor a;
    auto src = makePluck();
    src->audio.setSize (2, 8000, true);
    a.setLayerSample (0, src, "pluck");
    for (int i = 0; i < 4; ++i)
    {
        REQUIRE (a.evolveLayer (0, (juce::uint64) (50 + i)));
        REQUIRE (a.waitUntilIdle());
    }
    REQUIRE (a.undo (0));
    a.waitUntilIdle(); // index 3 of 0..4
    juce::MemoryBlock mb;
    a.getStateInformation (mb);

    MutaformProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    const auto& LB = b.getLayer (0);
    REQUIRE (LB.history.size() == 5);
    CHECK (LB.index == 3);
    REQUIRE (LB.current() != nullptr);
    CHECK (identical (*LB.current(), *a.getLayer (0).current()));
    CHECK (LB.history[1].audio == nullptr); // stored as recipe only
    REQUIRE (b.redo (0));
    REQUIRE (b.waitUntilIdle());
    REQUIRE (b.jumpToHistory (0, 1));
    REQUIRE (b.waitUntilIdle());
    CHECK (LB.index == 1);
    CHECK (identical (*LB.current(), *a.getLayer (0).history[1].audio));
}
