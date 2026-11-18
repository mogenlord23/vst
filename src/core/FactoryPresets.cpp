// Factory presets. Every sound here is synthesised from scratch in code (no external samples).
#include "Presets.h"
#include "Evolver.h"
#include <cmath>

namespace mf
{
namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;

std::shared_ptr<SampleData> makeBuffer (const juce::String& name, double sr, double seconds)
{
    auto s = std::make_shared<SampleData>();
    s->name = name;
    s->sampleRate = sr;
    s->audio.setSize (2, (int) (seconds * sr));
    s->audio.clear();
    return s;
}

void finish (SampleData& s, float peakDb = -1.0f)
{
    normalise (s.audio, juce::Decibels::decibelsToGain (peakDb));
    const int fade = std::min (s.length(), (int) (0.004 * s.sampleRate));
    s.audio.applyGainRamp (s.length() - fade, fade, 1.0f, 0.0f);
}

// --- FM bell: 2-operator FM with inharmonic ratio, slight stereo detune
std::shared_ptr<SampleData> fmBell (double sr)
{
    auto s = makeBuffer ("Glass Bell", sr, 2.6);
    const double f = 261.63; // C4 played at root C3 -> one octave up character
    for (int c = 0; c < 2; ++c)
    {
        const double det = c == 0 ? 0.997 : 1.003;
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / sr;
            const double idx = 3.2 * std::exp (-t * 2.8);
            const double mod = std::sin (twoPi * f * 3.5 * det * t) * idx;
            const double env = std::exp (-t * 1.7) * std::min (1.0, t * 400.0);
            const double v = std::sin (twoPi * f * det * t + mod) * env + 0.25 * std::sin (twoPi * f * 2.0 * t) * std::exp (-t * 4.0);
            s->audio.setSample (c, i, (float) v);
        }
    }
    finish (*s);
    return s;
}

// --- band-limited breath noise burst (one-pole filtered noise)
std::shared_ptr<SampleData> breath (double sr, uint64_t seed)
{
    auto s = makeBuffer ("Breath", sr, 1.4);
    Rng r (seed);
    for (int c = 0; c < 2; ++c)
    {
        double lp = 0, slow = 0;
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / sr;
            const double env = std::sin (juce::jmin (1.0, t / 1.4) * juce::MathConstants<double>::pi);
            const double n = r.uniform (-1.0, 1.0);
            lp += (n - lp) * 0.18;
            slow += (lp - slow) * 0.01;
            s->audio.setSample (c, i, (float) ((lp - slow) * env * env));
        }
    }
    finish (*s, -6.0f);
    return s;
}

// --- kick: sine with exponential pitch drop + click
std::shared_ptr<SampleData> kick (double sr, uint64_t seed)
{
    auto s = makeBuffer ("Rust Kick", sr, 0.9);
    Rng r (seed);
    double phase = 0;
    for (int i = 0; i < s->length(); ++i)
    {
        const double t = i / sr;
        const double f = 48.0 + 180.0 * std::exp (-t * 28.0);
        phase += twoPi * f / sr;
        const double body = std::sin (phase) * std::exp (-t * 5.5);
        const double click = (r.uniform (-1.0, 1.0)) * std::exp (-t * 900.0) * 0.6;
        const double v = std::tanh ((body + click) * 1.6);
        s->audio.setSample (0, i, (float) v);
        s->audio.setSample (1, i, (float) v);
    }
    finish (*s);
    return s;
}

// --- sub thump: low sine with fast decay
std::shared_ptr<SampleData> sub (double sr)
{
    auto s = makeBuffer ("Sub Thump", sr, 0.7);
    for (int i = 0; i < s->length(); ++i)
    {
        const double t = i / sr;
        const double v = std::sin (twoPi * 55.0 * t) * std::exp (-t * 6.0) * std::min (1.0, t * 300.0);
        s->audio.setSample (0, i, (float) v);
        s->audio.setSample (1, i, (float) v);
    }
    finish (*s, -3.0f);
    return s;
}

// --- vowel pad: additive saw-like partials shaped by two formant peaks, loopable middle
std::shared_ptr<SampleData> vowelPad (double sr, double f0, double f1, double f2, const juce::String& name)
{
    auto s = makeBuffer (name, sr, 3.0);
    const int partials = (int) std::min (60.0, 9000.0 / f0);
    for (int c = 0; c < 2; ++c)
    {
        const double det = c == 0 ? 1.0 : 1.004;
        for (int k = 1; k <= partials; ++k)
        {
            const double fk = f0 * k * det;
            const double form = std::exp (-std::pow ((fk - f1) / 180.0, 2.0)) + 0.6 * std::exp (-std::pow ((fk - f2) / 260.0, 2.0)) + 0.04;
            const double amp = form / k;
            const double ph = k * 0.7 + c;
            for (int i = 0; i < s->length(); ++i)
                s->audio.addSample (c, i, (float) (amp * std::sin (twoPi * fk * i / sr + ph)));
        }
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / sr;
            const double env = std::min (1.0, t / 0.35) * std::min (1.0, (3.0 - t) / 0.4);
            const double vib = 1.0 + 0.05 * std::sin (twoPi * 5.2 * t);
            s->audio.setSample (c, i, (float) (s->audio.getSample (c, i) * env * vib));
        }
    }
    finish (*s);
    return s;
}

// --- Karplus-Strong pluck
std::shared_ptr<SampleData> pluck (double sr, uint64_t seed)
{
    auto s = makeBuffer ("Seed Pluck", sr, 1.8);
    Rng r (seed);
    const int period = (int) std::round (sr / 261.63);
    for (int c = 0; c < 2; ++c)
    {
        std::vector<float> line ((size_t) period);
        for (auto& v : line)
            v = (float) r.uniform (-1.0, 1.0);
        int idx = 0;
        float prev = 0;
        for (int i = 0; i < s->length(); ++i)
        {
            const float cur = line[(size_t) idx];
            const float avg = 0.4985f * (cur + prev);
            prev = cur;
            line[(size_t) idx] = avg;
            idx = (idx + 1) % period;
            s->audio.setSample (c, i, cur);
        }
    }
    finish (*s);
    return s;
}

// --- metallic hit: inharmonic partials (circular plate modes) with individual decays
std::shared_ptr<SampleData> metal (double sr)
{
    auto s = makeBuffer ("Orbit Metal", sr, 2.2);
    static constexpr double ratios[] = { 1.0, 1.59, 2.14, 2.30, 2.65, 2.92, 3.50, 3.60, 4.06, 4.15 };
    const double f = 180.0;
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 10; ++k)
        {
            const double fk = f * ratios[k] * (c == 0 ? 1.0 : 1.002);
            const double dec = 1.6 + k * 0.9;
            const double a = 1.0 / (1.0 + k * 0.45);
            for (int i = 0; i < s->length(); ++i)
            {
                const double t = i / sr;
                s->audio.addSample (c, i, (float) (a * std::sin (twoPi * fk * t + k) * std::exp (-t * dec) * std::min (1.0, t * 2000.0)));
            }
        }
    finish (*s);
    return s;
}

using P = std::map<juce::String, float>;

void layerParams (P& p, int i, float gainDb, float pan = 0, int semi = 0)
{
    p[pid::layer (i, "gain")] = gainDb;
    p[pid::layer (i, "pan")] = pan;
    p[pid::layer (i, "semi")] = (float) semi;
}
} // namespace

namespace factory
{
int count() { return 5; }

juce::String name (int index)
{
    static const char* names[] = { "Glass Bloom", "Rust Kick", "Tape Choir", "Seed Pluck", "Orbit Metal" };
    return juce::isPositiveAndBelow (index, 5) ? names[index] : "";
}

PresetData create (int index, double sr)
{
    PresetData d;
    d.name = name (index);
    auto& p = d.params;
    switch (index)
    {
        case 0: // Glass Bloom: FM bell + breath, reverb wash
            d.layers[0].audio = fmBell (sr);
            d.layers[1].audio = breath (sr, 11);
            layerParams (p, 0, 0.0f);
            layerParams (p, 1, -14.0f, 0.2f);
            p[pid::attack] = 2.0f; p[pid::decay] = 1600.0f; p[pid::sustain] = 0.4f; p[pid::release] = 900.0f;
            p[pid::revOn] = 1; p[pid::revSize] = 0.82f; p[pid::revMix] = 0.32f; p[pid::revDamp] = 0.35f;
            p[pid::choOn] = 1; p[pid::choMix] = 0.25f; p[pid::choRate] = 0.3f;
            p[pid::chaos] = 35.0f;
            break;

        case 1: // Rust Kick: punchy kick + sub, drive
            d.layers[0].audio = kick (sr, 21);
            d.layers[1].audio = sub (sr);
            layerParams (p, 0, 0.0f);
            layerParams (p, 1, -8.0f);
            p[pid::attack] = 0.0f; p[pid::decay] = 600.0f; p[pid::sustain] = 1.0f; p[pid::release] = 120.0f;
            p[pid::distOn] = 1; p[pid::distDrive] = 0.45f; p[pid::distMix] = 0.7f;
            p[pid::fltOn] = 1; p[pid::fltType] = 0; p[pid::fltCutoff] = 6500.0f; p[pid::fltRes] = 0.25f;
            p[pid::chaos] = 25.0f;
            break;

        case 2: // Tape Choir: two vowel pads, looped, chorus
            d.layers[0].audio = vowelPad (sr, 130.81, 700.0, 1150.0, "Choir Ah");
            d.layers[1].audio = vowelPad (sr, 196.0, 400.0, 2000.0, "Choir Ee");
            layerParams (p, 0, 0.0f, -0.25f);
            layerParams (p, 1, -5.0f, 0.25f, -12);
            for (int i = 0; i < 2; ++i)
            {
                p[pid::layer (i, "loop")] = 1;
                p[pid::layer (i, "loopStart")] = 0.25f;
                p[pid::layer (i, "loopEnd")] = 0.75f;
                p[pid::layer (i, "xfade")] = 120.0f;
            }
            p[pid::attack] = 380.0f; p[pid::decay] = 800.0f; p[pid::sustain] = 0.85f; p[pid::release] = 1400.0f;
            p[pid::choOn] = 1; p[pid::choMix] = 0.5f; p[pid::choDepth] = 0.5f;
            p[pid::revOn] = 1; p[pid::revMix] = 0.28f; p[pid::revSize] = 0.7f;
            p[pid::chaos] = 50.0f;
            break;

        case 3: // Seed Pluck: KS pluck + its evolved variation as layer 2, synced delay
        {
            d.layers[0].audio = pluck (sr, 31);
            auto evolved = evolve (*d.layers[0].audio, 0x5eedull, 0.45f, (1u << (int) EvolveProcess::envelopeFilter) | (1u << (int) EvolveProcess::saturation));
            if (evolved.audio != nullptr)
            {
                evolved.audio->name = "Pluck Mutant";
                d.layers[1].audio = evolved.audio;
            }
            layerParams (p, 0, 0.0f, -0.15f);
            layerParams (p, 1, -7.0f, 0.3f, 12);
            p[pid::attack] = 0.0f; p[pid::decay] = 900.0f; p[pid::sustain] = 0.6f; p[pid::release] = 450.0f;
            p[pid::dlyOn] = 1; p[pid::dlySync] = 1; p[pid::dlyDiv] = 5; p[pid::dlyFeedback] = 0.42f; p[pid::dlyMix] = 0.28f;
            p[pid::revOn] = 1; p[pid::revMix] = 0.15f;
            p[pid::chaos] = 45.0f;
            break;
        }

        case 4: // Orbit Metal: metallic hit with multipoints by key, reversed second layer
        default:
            d.layers[0].audio = metal (sr);
            d.layers[1].audio = metal (sr);
            d.layers[0].multipoints = { 0.0f, 0.08f, 0.2f, 0.35f };
            layerParams (p, 0, 0.0f);
            layerParams (p, 1, -10.0f, 0.4f, -12);
            p[pid::layer (0, "mpMode")] = 1;
            p[pid::layer (1, "reverse")] = 1;
            p[pid::attack] = 0.0f; p[pid::decay] = 1200.0f; p[pid::sustain] = 0.5f; p[pid::release] = 700.0f;
            p[pid::fltOn] = 1; p[pid::fltType] = 2; p[pid::fltCutoff] = 2400.0f; p[pid::fltRes] = 0.45f; p[pid::fltMix] = 0.5f;
            p[pid::revOn] = 1; p[pid::revMix] = 0.3f; p[pid::revSize] = 0.75f;
            p[pid::chaos] = 70.0f;
            break;
    }
    return d;
}
} // namespace factory
} // namespace mf
