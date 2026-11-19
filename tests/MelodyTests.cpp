#include <catch2/catch_test_macros.hpp>
#include "core/Melody.h"
#include "core/Genoform.h"
#include "core/SoundFeatures.h"
#include "core/Evolver.h"
#include "TestHelpers.h"

using namespace testutil;

TEST_CASE ("Melodies are in key, deterministic and phrase-shaped", "[melody]")
{
    for (int scale = 0; scale < (int) mf::Scale::count; ++scale)
        for (int key : { 0, 5, 9 })
            for (juce::uint64 seed = 1; seed < 12; ++seed)
            {
                mf::MelodySettings s;
                s.key = key; s.scale = (mf::Scale) scale; s.seed = seed; s.bars = 4;
                s.density = (float) (seed % 4) / 3.0f; s.mood = (float) (seed % 3) / 2.0f;
                const auto p = mf::generateMelody (s);
                REQUIRE (! p.notes.empty());
                const auto& iv = mf::scaleIntervals (s.scale);
                int prevEnd = 0;
                for (auto& n : p.notes)
                {
                    const int pc = ((n.note - key) % 12 + 12) % 12;
                    REQUIRE (std::find (iv.begin(), iv.end(), pc) != iv.end()); // always in key
                    REQUIRE (n.start >= prevEnd - 16);
                    REQUIRE (n.length >= 1);
                    REQUIRE (n.start + n.length <= p.totalSteps());
                    REQUIRE (n.velocity > 0.0f);
                    REQUIRE (n.velocity <= 1.0f);
                    prevEnd = n.start;
                }
                // ends on the tonic, and no wild leaps (> octave)
                CHECK (((p.notes.back().note - key) % 12 + 12) % 12 == 0);
                for (size_t i = 1; i < p.notes.size(); ++i)
                    CHECK (std::abs (p.notes[i].note - p.notes[i - 1].note) <= 12);
                // same seed -> same melody
                const auto q = mf::generateMelody (s);
                REQUIRE (q.notes.size() == p.notes.size());
                for (size_t i = 0; i < p.notes.size(); ++i)
                    REQUIRE ((q.notes[i].note == p.notes[i].note && q.notes[i].start == p.notes[i].start));
            }
}

TEST_CASE ("Density controls the number of notes", "[melody]")
{
    int sparse = 0, busy = 0;
    for (juce::uint64 seed = 1; seed < 30; ++seed)
    {
        mf::MelodySettings s; s.seed = seed;
        s.density = 0.0f; sparse += (int) mf::generateMelody (s).notes.size();
        s.density = 1.0f; busy += (int) mf::generateMelody (s).notes.size();
    }
    CHECK (busy > sparse * 1.4);
}

TEST_CASE ("YIN pitch detection", "[pitch]")
{
    auto a = makeSine (440.0, 48000.0, 48000);
    auto pa = mf::detectPitch (*a);
    CHECK (std::abs (pa.midi - 69.0f) < 0.1f);
    CHECK (pa.confidence > 0.8f);
    auto low = makeSine (110.0, 44100.0, 44100);
    CHECK (std::abs (mf::detectPitch (*low).midi - 45.0f) < 0.15f);

    // white noise: low confidence
    auto nz = makeSine (100.0, 48000.0, 48000);
    mf::Rng r (3);
    for (int i = 0; i < nz->length(); ++i) for (int c = 0; c < 2; ++c) nz->audio.setSample (c, i, (float) r.uniform (-0.5, 0.5));
    CHECK (mf::detectPitch (*nz).confidence < 0.6f);

    // tonalize makes noise pitched at the requested frequency
    auto tonal = mf::tonalize (*nz, 261.63, 1.0f);
    auto pt = mf::detectPitch (*tonal);
    CHECK (pt.confidence > 0.6f);
    CHECK (std::abs (pt.midi - 60.0f) < 0.5f);
    CHECK (allFinite (tonal->audio));
}

TEST_CASE ("GENOFORM evolution improves the match and is deterministic", "[genoform]")
{
    // target: a synth sound produced by a known genome
    mf::Genome g {};
    for (int i = 0; i < mf::kGenes; ++i) g[(size_t) i] = 0.5f;
    g[0] = 0.9f; g[8] = 0.45f; g[13] = 0.3f; g[14] = 0.0f; g[4] = 0.4f;
    auto target = mf::renderGenome (g, 44100.0, 0.8, 220.0);
    REQUIRE (allFinite (target->audio));

    mf::GenoformOptions o; o.population = 20; o.generations = 1; o.seed = 7;
    const auto early = mf::genoformMatch (*target, o);
    o.generations = 25;
    std::vector<float> trace;
    const auto late = mf::genoformMatch (*target, o, [&] (float, float best) { trace.push_back (best); return true; });
    REQUIRE (late.audio != nullptr);
    CHECK (late.score >= early.score);
    CHECK (late.score > 0.5f);
    for (size_t i = 1; i < trace.size(); ++i)
        CHECK (trace[i] >= trace[i - 1] - 1e-6f); // elitism: best never gets worse
    CHECK (allFinite (late.audio->audio));
    const auto again = mf::genoformMatch (*target, o);
    CHECK (again.best == late.best);

    // cancellation
    const auto cancelled = mf::genoformMatch (*target, o, [] (float p, float) { return p < 0.2f; });
    CHECK (cancelled.audio == nullptr);
}

#include "core/Sequencer.h"
#include "plugin/PluginProcessor.h"

TEST_CASE ("Sequencer plays the pattern in time and releases every note", "[melody][sequencer]")
{
    mf::MelodySequencer seq;
    seq.prepare (48000.0);
    mf::MelodySettings ms; ms.seed = 5; ms.bars = 2; ms.density = 0.7f;
    const auto pat = mf::generateMelody (ms);
    seq.setPattern (mf::SeqPattern::fromMelody (pat));
    std::array<mf::NoteEvent, 128> ev {};
    int ons = 0, offs = 0, held = 0;
    const int block = 512;
    const double bpm = 120.0;
    double ppq = 0.0;
    const int blocks = (int) std::ceil (2 * 4 * 0.5 * 48000.0 / block); // exactly two bars at 120 bpm
    int firstOnSample = -1;
    for (int b = 0; b < blocks; ++b)
    {
        const int n = seq.process (block, true, true, ppq, bpm, ev.data(), (int) ev.size());
        for (int i = 0; i < n; ++i)
        {
            REQUIRE (ev[(size_t) i].samplePos >= 0);
            REQUIRE (ev[(size_t) i].samplePos < block);
            if (i > 0) REQUIRE (ev[(size_t) i].samplePos >= ev[(size_t) i - 1].samplePos);
            if (ev[(size_t) i].velocity > 0) { if (b * block + ev[(size_t) i].samplePos < 192000 - 4) ++ons; ++held; if (firstOnSample < 0) firstOnSample = b * block + ev[(size_t) i].samplePos; }
            else { ++offs; --held; }
        }
        ppq += block * bpm / 60.0 / 48000.0;
    }
    CHECK (ons == (int) pat.notes.size());
    CHECK (firstOnSample == 0);
    // disabling releases everything that is still sounding
    const int n = seq.process (block, false, true, ppq, bpm, ev.data(), (int) ev.size());
    for (int i = 0; i < n; ++i) { CHECK (ev[(size_t) i].velocity == 0.0f); --held; }
    CHECK (held == 0);
}

TEST_CASE ("Processor MELODY mode produces audio without MIDI input", "[melody][processor]")
{
    MutaformProcessor p;
    p.setPlayConfigDetails (0, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);
    p.setLayerSample (0, testutil::makeSine (440.0, 48000.0, 24000), "a");
    // auto-tune detected A (69) -> plays in key
    p.housekeeping();
    p.apvts.getParameter (mf::pid::melOn)->setValueNotifyingHost (1.0f);
    p.newMelody (42);
    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer midi;
    float pk = 0.0f;
    for (int b = 0; b < 400; ++b)
    {
        p.processBlock (buf, midi);
        REQUIRE (testutil::allFinite (buf));
        pk = std::max (pk, testutil::peak (buf));
    }
    CHECK (pk > 0.05f);
    CHECK (p.getSequencer().getCurrentStep() >= 0);
}

TEST_CASE ("Noise is tonalized on load and GENOFORM adds a matched layer variation", "[genoform][processor]")
{
    MutaformProcessor p;
    auto nz = testutil::makeSine (100.0, 48000.0, 24000);
    mf::Rng r (9);
    for (int i = 0; i < nz->length(); ++i)
        for (int c = 0; c < 2; ++c)
            nz->audio.setSample (c, i, (float) r.uniform (-0.5, 0.5) * std::exp (-i / 6000.0f));
    p.setLayerSample (0, nz, "noise");
    const auto& L = p.getLayer (0);
    CHECK (L.history[0].label.contains ("tonalized"));
    CHECK (L.history[0].features.pitchConfidence >= 0.5f);

    REQUIRE (p.startGenoform (0, 1234));
    CHECK_FALSE (p.startGenoform (0)); // one at a time
    REQUIRE (p.waitUntilIdle (60000));
    REQUIRE (L.history.size() == 2);
    CHECK (L.history[1].root);
    CHECK (L.history[1].label.startsWith ("GENOFORM"));
    CHECK (testutil::allFinite (L.history[1].audio->audio));
}
