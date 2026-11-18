#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/SamplerEngine.h"
#include "TestHelpers.h"

using namespace testutil;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
constexpr double kSR = 48000.0;
constexpr int kBlock = 256;

mf::EngineSettings defaultSettings()
{
    mf::EngineSettings s;
    s.attackMs = 0.0f;
    s.decayMs = 1.0f;
    s.sustain = 1.0f;
    s.releaseMs = 5.0f;
    s.velSens = 0.0f;
    for (auto& l : s.layers)
        l.gainDb = 0.0f;
    return s;
}

/** Renders `total` samples of the left channel, sending `first` MIDI on the first block. */
std::vector<float> renderLeft (mf::SamplerEngine& e, const mf::EngineSettings& s, int total, juce::MidiBuffer first)
{
    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, kBlock);
    juce::MidiBuffer empty;
    for (int done = 0; done < total; done += kBlock)
    {
        e.render (buf, done == 0 ? first : empty, s);
        for (int i = 0; i < kBlock; ++i)
            out.push_back (buf.getSample (0, i));
    }
    return out;
}
} // namespace

TEST_CASE ("Sample loading from file and memory", "[sample]")
{
    auto tmp = juce::File::createTempFile (".wav");
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (tmp.createOutputStream().release());
        REQUIRE (os != nullptr);
        auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions {}.withSampleRate (44100.0).withNumChannels (1).withBitsPerSample (16));
        REQUIRE (w != nullptr);
        juce::AudioBuffer<float> mono (1, 4410);
        for (int i = 0; i < 4410; ++i)
            mono.setSample (0, i, 0.5f * std::sin (i * 0.05f));
        REQUIRE (w->writeFromAudioSampleBuffer (mono, 0, 4410));
    }

    juce::String err;
    auto s = mf::loadSampleFromFile (tmp, err);
    REQUIRE (s != nullptr);
    CHECK (s->audio.getNumChannels() == 2);
    CHECK (s->length() == 4410);
    CHECK (s->sampleRate == 44100.0);
    CHECK_THAT (s->audio.getSample (1, 100), WithinAbs (s->audio.getSample (0, 100), 1e-6)); // mono duplicated
    CHECK_THAT (s->audio.getSample (0, 100), WithinAbs (0.5f * std::sin (5.0f), 1e-3));

    juce::MemoryBlock mb;
    REQUIRE (tmp.loadFileAsData (mb));
    auto m = mf::loadSampleFromMemory (mb.getData(), mb.getSize(), "dropped.wav", err);
    REQUIRE (m != nullptr);
    CHECK (m->name == "dropped");
    CHECK (m->length() == 4410);

    const char junk[] = "definitely not audio data at all..........";
    CHECK (mf::loadSampleFromMemory (junk, sizeof (junk), "junk.wav", err) == nullptr);
    tmp.deleteFile();
}

TEST_CASE ("Sample encode/decode is lossless", "[sample]")
{
    auto s = makeSine (123.0, 44100.0, 5000);
    auto block = mf::encodeSample (*s);
    auto d = mf::decodeSample (block);
    REQUIRE (d != nullptr);
    REQUIRE (d->length() == s->length());
    CHECK (d->sampleRate == s->sampleRate);
    for (int i = 0; i < s->length(); ++i)
        REQUIRE (d->audio.getSample (0, i) == s->audio.getSample (0, i));
}

TEST_CASE ("Engine is silent without notes", "[engine]")
{
    mf::SamplerEngine e;
    e.prepare (kSR, kBlock);
    e.setSample (0, makeSine (440.0, kSR, 48000));
    auto out = renderLeft (e, defaultSettings(), kBlock * 40, {});
    for (auto v : out)
        REQUIRE (v == 0.0f);
}

TEST_CASE ("Pitch follows the keyboard (resampling)", "[engine]")
{
    const auto check = [] (double sampleSR, int note, double expected)
    {
        mf::SamplerEngine e;
        e.prepare (kSR, kBlock);
        e.setSample (0, makeSine (440.0, sampleSR, (int) sampleSR * 2));
        auto out = renderLeft (e, defaultSettings(), (int) kSR / 2, noteOn (note));
        const double f = estimateFrequency (out.data(), 2000, (int) out.size(), kSR);
        CHECK_THAT (f, WithinRel (expected, 0.003));
    };
    check (kSR, mf::kRootNote, 440.0);                                // root C3 = original pitch
    check (kSR, mf::kRootNote + 12, 880.0);                           // octave up
    check (kSR, mf::kRootNote - 12, 220.0);                           // octave down
    check (kSR, mf::kRootNote + 7, 440.0 * std::pow (2.0, 7.0 / 12)); // fifth
    check (44100.0, mf::kRootNote, 440.0);                            // sample-rate conversion
}

TEST_CASE ("Layer semitone and cent offsets", "[engine]")
{
    mf::SamplerEngine e;
    e.prepare (kSR, kBlock);
    e.setSample (0, makeSine (440.0, kSR, (int) kSR * 2));
    auto s = defaultSettings();
    s.layers[0].pitchSemis = -12.0f + 0.5f; // -11.5 semitones
    auto out = renderLeft (e, s, (int) kSR / 2, noteOn (mf::kRootNote));
    CHECK_THAT (estimateFrequency (out.data(), 2000, (int) out.size(), kSR), WithinRel (440.0 * std::pow (2.0, -11.5 / 12), 0.003));
}

TEST_CASE ("Interpolated output is smooth for a sine", "[engine]")
{
    mf::SamplerEngine e;
    e.prepare (kSR, kBlock);
    e.setSample (0, makeSine (200.0, kSR, (int) kSR));
    auto out = renderLeft (e, defaultSettings(), 20000, noteOn (mf::kRootNote + 5)); // non-integer ratio
    // compare against ideal resampled sine
    const double f = 200.0 * std::pow (2.0, 5.0 / 12.0);
    double maxErr = 0.0;
    for (int i = 1000; i < 20000; ++i)
        maxErr = std::max (maxErr, std::abs ((double) out[(size_t) i] - 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * f * i / kSR)));
    CHECK (maxErr < 1.0e-3);
}

TEST_CASE ("Reverse plays the region backwards", "[engine]")
{
    mf::SamplerEngine e;
    e.prepare (kSR, kBlock);
    e.setSample (0, makeRamp (4000));
    auto s = defaultSettings();

    auto fwd = renderLeft (e, s, 3072, noteOn (mf::kRootNote));
    CHECK (fwd[500] < fwd[2500]);
    CHECK_THAT (fwd[1000], WithinAbs (0.1f + 0.8f * 1000.0f / 3999.0f, 1e-3));

    e.reset();
    s.layers[0].reverse = true;
    auto rev = renderLeft (e, s, 3072, noteOn (mf::kRootNote));
    CHECK (rev[500] > rev[2500]);
    CHECK_THAT (rev[1000], WithinAbs (0.1f + 0.8f * (3999.0f - 1000.0f) / 3999.0f, 1e-3));
}

TEST_CASE ("Start/end trim the region", "[engine]")
{
    mf::SamplerEngine e;
    e.prepare (kSR, kBlock);
    e.setSample (0, makeRamp (4000));
    auto s = defaultSettings();
    s.layers[0].start = 0.5f;
    s.layers[0].end = 0.75f;
    auto out = renderLeft (e, s, 4096, noteOn (mf::kRootNote));
    CHECK_THAT (out[60], WithinAbs (0.1f + 0.8f * 2060.0f / 3999.0f, 1e-3));
    CHECK (out[1100] == 0.0f); // region of 1000 samples finished
}

TEST_CASE ("Loop points sustain the sound", "[engine]")
{
    mf::SamplerEngine e;
    e.prepare (kSR, kBlock);
    e.setSample (0, makeRamp (2000));
    auto s = defaultSettings();

    auto oneShot = renderLeft (e, s, 8192, noteOn (mf::kRootNote));
    CHECK (oneShot[5000] == 0.0f);

    e.reset();
    s.layers[0].loop = true;
    s.layers[0].loopStart = 0.25f;
    s.layers[0].loopEnd = 0.75f;
    s.layers[0].xfadeMs = 0.0f;
    auto looped = renderLeft (e, s, 8192, noteOn (mf::kRootNote));
    // after the first pass the signal stays within the loop's ramp values
    const float lo = 0.1f + 0.8f * 500.0f / 1999.0f, hi = 0.1f + 0.8f * 1500.0f / 1999.0f;
    for (int i = 2000; i < 8192; ++i)
    {
        REQUIRE (looped[(size_t) i] >= lo - 0.01f);
        REQUIRE (looped[(size_t) i] <= hi + 0.01f);
    }
    // the loop wraps: value repeats with period = loop length (1000 samples)
    CHECK_THAT (looped[3000], WithinAbs (looped[4000], 1e-4));

    // crossfade: no big discontinuity at the wrap point
    e.reset();
    s.layers[0].xfadeMs = 5.0f;
    auto xf = renderLeft (e, s, 8192, noteOn (mf::kRootNote));
    float maxJump = 0.0f;
    for (int i = 2001; i < 8192; ++i)
        maxJump = std::max (maxJump, std::abs (xf[(size_t) i] - xf[(size_t) i - 1]));
    CHECK (maxJump < 0.05f);
}

TEST_CASE ("Multipoint start positions by key", "[engine]")
{
    mf::SamplerEngine e;
    e.prepare (kSR, kBlock);
    e.setSample (0, makeRamp (4000));
    e.setMultipoints (0, { 0.0f, 0.5f });
    auto s = defaultSettings();
    s.layers[0].mpMode = mf::MultipointMode::key;
    s.layers[0].pitchSemis = -1.0f; // keep rate exactly 1.0 for note root+1
    auto a = renderLeft (e, s, 512, noteOn (mf::kRootNote + 1)); // index 1 -> starts at 0.5
    CHECK_THAT (a[60], WithinAbs (0.1f + 0.8f * (2000.0f + 60.0f) / 3999.0f, 2e-3));
}

TEST_CASE ("Velocity scales the level", "[engine]")
{
    mf::SamplerEngine e;
    e.prepare (kSR, kBlock);
    e.setSample (0, makeRamp (4000));
    auto s = defaultSettings();
    s.velSens = 1.0f;
    auto loud = renderLeft (e, s, 512, noteOn (mf::kRootNote, 1.0f));
    e.reset();
    auto soft = renderLeft (e, s, 512, noteOn (mf::kRootNote, 0.5f));
    CHECK_THAT (soft[300] / loud[300], WithinAbs (0.25f, 0.01f));
}

TEST_CASE ("16-voice polyphony with stealing stays finite", "[engine]")
{
    mf::SamplerEngine e;
    e.prepare (kSR, kBlock);
    for (int l = 0; l < mf::kNumLayers; ++l)
        e.setSample (l, makeSine (100.0 + 50 * l, kSR, (int) kSR));
    auto s = defaultSettings();
    juce::AudioBuffer<float> buf (2, kBlock);
    for (int b = 0; b < 200; ++b)
    {
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (1, 40 + (b * 7) % 40, 0.8f), b % kBlock);
        if (b > 5)
            m.addEvent (juce::MidiMessage::noteOff (1, 40 + ((b - 5) * 7) % 40), 10);
        e.render (buf, m, s);
        REQUIRE (allFinite (buf));
        REQUIRE (e.getNumActiveVoices() <= mf::kNumVoices);
    }
}
