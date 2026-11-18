#include "Evolver.h"
#include "Interp.h"
#include <cmath>

namespace mf
{
//==============================================================================
static uint64_t splitmix (uint64_t& x) noexcept
{
    uint64_t z = (x += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

Rng::Rng (uint64_t seed)
{
    for (auto& v : s)
        v = splitmix (seed);
}

static inline uint64_t rotl (uint64_t x, int k) noexcept { return (x << k) | (x >> (64 - k)); }

uint64_t Rng::next() noexcept
{
    const uint64_t result = rotl (s[1] * 5, 7) * 9;
    const uint64_t t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl (s[3], 45);
    return result;
}

double Rng::uniform() noexcept { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }

int Rng::range (int lo, int hi) noexcept
{
    if (hi <= lo)
        return lo;
    return lo + (int) (next() % (uint64_t) (hi - lo + 1));
}

const char* processName (EvolveProcess p)
{
    switch (p)
    {
        case EvolveProcess::pitchShift:      return "Pitch shift";
        case EvolveProcess::grainShuffle:    return "Grain shuffle";
        case EvolveProcess::envelopeFilter:  return "Envelope filter";
        case EvolveProcess::saturation:      return "Saturation";
        case EvolveProcess::timeStretch:     return "Time stretch";
        case EvolveProcess::ringMod:         return "Ring mod";
        case EvolveProcess::reverseTail:     return "Reverse tail";
        case EvolveProcess::transientShaper: return "Transient shaper";
        case EvolveProcess::count:           break;
    }
    return "?";
}

//==============================================================================
namespace
{
using Buffer = juce::AudioBuffer<float>;
constexpr double kPi = juce::MathConstants<double>::pi;

int maxLen (double sr) { return (int) (kMaxEvolveSeconds * sr); }

float hann (int i, int n) { return (float) (0.5 - 0.5 * std::cos (2.0 * kPi * (i + 0.5) / n)); }

/** Simple TPT state-variable filter (offline, double precision). */
struct Svf
{
    double ic1 = 0, ic2 = 0;
    double process (double x, double g, double k, int mode)
    {
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + g * a1 * v3;
        const double v2 = ic2 + g * v1;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        if (mode == 0) return v2;                 // LP
        if (mode == 1) return v1;                 // BP
        return x - k * v1 - v2;                   // HP
    }
};

// ---- 1. pitch shift (tape-style resample) -------------------------------------------------
juce::String pitchShift (Buffer& b, double sr, Rng& rng, double s)
{
    static constexpr int intervals[] = { 1, 2, 3, 5, 7, 12, 19, 24 };
    const int maxSemi = 2 + (int) std::round (22.0 * s);
    int candidates = 0;
    for (int iv : intervals)
        if (iv <= maxSemi) ++candidates;
    int semi = intervals[rng.range (0, std::max (0, candidates - 1))];
    if (rng.chance (0.55)) semi = -semi;
    const double cents = rng.uniform (-30.0, 30.0) * s;
    const double ratio = std::exp2 ((semi + cents / 100.0) / 12.0);

    const int inLen = b.getNumSamples();
    const int outLen = juce::jlimit (64, maxLen (sr), (int) (inLen / ratio));
    Buffer out (2, outLen);
    for (int c = 0; c < 2; ++c)
    {
        const auto* src = b.getReadPointer (c);
        auto* dst = out.getWritePointer (c);
        for (int i = 0; i < outLen; ++i)
        {
            const double p = i * ratio;
            dst[i] = p < inLen ? readHermite (src, inLen, p) : 0.0f;
        }
    }
    b = std::move (out);
    return juce::String ("Pitch ") + (semi > 0 ? "+" : "") + juce::String (semi) + " st";
}

// ---- 2. granular shuffle -------------------------------------------------------------------
juce::String grainShuffle (Buffer& b, double sr, Rng& rng, double s)
{
    const int len = b.getNumSamples();
    const int grain = juce::jlimit (64, std::max (64, len / 2), (int) (sr * rng.uniform (0.015, 0.03 + 0.12 * (1.0 - s * 0.5))));
    const int hop = grain / 2;
    const int numGrains = std::max (1, len / hop);
    const int spread = 1 + (int) std::round (s * 10.0);
    const double revProb = 0.1 + 0.4 * s;

    Buffer out (2, len);
    out.clear();
    for (int g = 0; g < numGrains; ++g)
    {
        const int srcGrain = juce::jlimit (0, numGrains - 1, g + rng.range (-spread, spread));
        const bool rev = rng.chance (revProb);
        const int srcStart = srcGrain * hop;
        const int dstStart = g * hop;
        for (int i = 0; i < grain; ++i)
        {
            const int d = dstStart + i;
            if (d >= len) break;
            const int si = srcStart + (rev ? grain - 1 - i : i);
            if (si < 0 || si >= len) continue;
            const float w = hann (i, grain);
            out.addSample (0, d, b.getSample (0, si) * w);
            out.addSample (1, d, b.getSample (1, si) * w);
        }
    }
    // keep the original attack so the result still "speaks"
    const int keep = std::min (len, (int) (sr * 0.01));
    for (int i = 0; i < keep; ++i)
    {
        const float t = (float) i / (float) keep;
        for (int c = 0; c < 2; ++c)
            out.setSample (c, i, b.getSample (c, i) * (1.0f - t) + out.getSample (c, i) * t);
    }
    b = std::move (out);
    return "Grains " + juce::String (juce::roundToInt (grain * 1000.0 / sr)) + " ms";
}

// ---- 3. envelope filter --------------------------------------------------------------------
juce::String envelopeFilter (Buffer& b, double sr, Rng& rng, double s)
{
    const int mode = rng.chance (0.6) ? 0 : (rng.chance (0.5) ? 1 : 2);
    const bool down = rng.chance (0.65);
    const double fHi = rng.uniform (4000.0, 16000.0);
    const double fLo = rng.uniform (80.0, 300.0 + 1200.0 * (1.0 - s));
    const double tau = rng.uniform (0.03, 0.6) * sr;
    const double res = 0.1 + 0.85 * s * rng.uniform (0.4, 1.0);
    const double k = 2.0 - 1.9 * res; // damping
    const double nyq = sr * 0.49;

    std::array<Svf, 2> f;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double e = std::exp (-i / tau);
        const double fc = std::min (nyq, down ? fLo * std::pow (fHi / fLo, e) : fHi * std::pow (fLo / fHi, e));
        const double g = std::tan (kPi * fc / sr);
        for (int c = 0; c < 2; ++c)
        {
            double y = f[(size_t) c].process (b.getSample (c, i), g, k, mode);
            if (! std::isfinite (y)) { f[(size_t) c] = {}; y = 0.0; }
            b.setSample (c, i, (float) y);
        }
    }
    static const char* names[] = { "LP", "BP", "HP" };
    return juce::String ("Filter ") + names[mode] + (down ? " sweep down" : " sweep up");
}

// ---- 4. saturation / waveshaping -----------------------------------------------------------
juce::String saturation (Buffer& b, double sr, Rng& rng, double s)
{
    const int shape = rng.range (0, 3);
    const float drive = (float) (1.0 + 18.0 * s * rng.uniform (0.4, 1.0));
    const float mix = (float) (0.5 + 0.5 * s);
    const int bits = 4 + (int) std::round ((1.0 - s) * 8.0);
    const int hold = 1 + (int) std::round (s * rng.uniform (0.0, 7.0));
    const float levels = (float) (1 << bits);
    juce::ignoreUnused (sr);

    float inPeak = 0.0f, outPeak = 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        auto* d = b.getWritePointer (c);
        float held = 0.0f;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float x = d[i];
            inPeak = std::max (inPeak, std::abs (x));
            float y = 0.0f;
            switch (shape)
            {
                case 0: y = std::tanh (x * drive); break;
                case 1: y = (float) std::sin (x * drive * 1.3); break; // foldback
                case 2: { const float v = x * drive + 0.3f; y = v > 0 ? std::tanh (v) : std::tanh (v * 0.4f) ; y -= std::tanh (0.3f); break; }
                default:
                    if (i % hold == 0) held = std::round (std::tanh (x * drive * 0.5f) * levels) / levels;
                    y = held;
                    break;
            }
            d[i] = x * (1.0f - mix) + y * mix;
            outPeak = std::max (outPeak, std::abs (d[i]));
        }
    }
    if (outPeak > 1.0e-6f && inPeak > 1.0e-6f)
        b.applyGain (inPeak / outPeak);
    static const char* names[] = { "Tanh", "Fold", "Asym", "Crush" };
    return juce::String ("Saturate ") + names[shape];
}

// ---- 5. time stretch (overlap-add) ---------------------------------------------------------
juce::String timeStretch (Buffer& b, double sr, Rng& rng, double s)
{
    const double factor = rng.chance (0.5) ? rng.uniform (1.0 + 0.2 * s, 1.0 + 1.6 * s)
                                           : 1.0 / rng.uniform (1.0 + 0.2 * s, 1.0 + 1.0 * s);
    const int inLen = b.getNumSamples();
    const int outLen = juce::jlimit (64, maxLen (sr), (int) (inLen * factor));
    const int grain = juce::jlimit (32, 1 << 14, (int) (sr * rng.uniform (0.04, 0.09)));
    const int hopOut = grain / 4;
    const double hopIn = hopOut / factor;

    Buffer out (2, outLen);
    out.clear();
    std::vector<float> norm ((size_t) outLen, 0.0f);
    int k = 0;
    for (int dst = 0; dst < outLen; dst += hopOut, ++k)
    {
        const double jitter = rng.uniform (-0.25, 0.25) * hopOut * s;
        const int src = (int) std::floor (k * hopIn + jitter);
        for (int i = 0; i < grain; ++i)
        {
            const int d = dst + i;
            if (d >= outLen) break;
            const int si = juce::jlimit (0, inLen - 1, src + i);
            const float w = hann (i, grain);
            out.addSample (0, d, b.getSample (0, si) * w);
            out.addSample (1, d, b.getSample (1, si) * w);
            norm[(size_t) d] += w;
        }
    }
    for (int i = 0; i < outLen; ++i)
    {
        const float n = norm[(size_t) i] > 1.0e-3f ? 1.0f / norm[(size_t) i] : 0.0f;
        out.setSample (0, i, out.getSample (0, i) * n);
        out.setSample (1, i, out.getSample (1, i) * n);
    }
    b = std::move (out);
    return "Stretch x" + juce::String (factor, 2);
}

// ---- 6. ring modulation --------------------------------------------------------------------
juce::String ringMod (Buffer& b, double sr, Rng& rng, double s)
{
    const double f0 = std::exp (rng.uniform (std::log (30.0), std::log (30.0 + 1800.0 * s + 50.0)));
    const double f1 = rng.chance (0.4) ? f0 * rng.uniform (0.25, 3.0) : f0;
    const float mix = (float) (0.3 + 0.65 * s);
    double phase = 0.0;
    const int n = b.getNumSamples();
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / std::max (1, n - 1);
        const double f = f0 * std::pow (f1 / f0, t);
        phase += 2.0 * kPi * f / sr;
        if (phase > 2.0 * kPi) phase -= 2.0 * kPi;
        const float m = (float) std::sin (phase);
        for (int c = 0; c < 2; ++c)
        {
            const float x = b.getSample (c, i);
            b.setSample (c, i, x * (1.0f - mix) + x * m * mix);
        }
    }
    return "Ring " + juce::String (juce::roundToInt (f0)) + " Hz";
}

// ---- 7. reverse reverb tail (swell) ----------------------------------------------------------
juce::String reverseTail (Buffer& b, double sr, Rng& rng, double s)
{
    const int len = b.getNumSamples();
    const int tail = std::min (maxLen (sr) - len, (int) (sr * rng.uniform (0.3, 0.6 + 1.8 * s)));
    if (tail < 64)
        return "Reverse tail (skipped)";

    // reversed input, padded with silence for the reverb decay
    Buffer rev (2, len + tail);
    rev.clear();
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < len; ++i)
            rev.setSample (c, i, b.getSample (c, len - 1 - i));

    juce::Reverb verb;
    juce::Reverb::Parameters p;
    p.roomSize = (float) (0.65 + 0.34 * s);
    p.damping = (float) rng.uniform (0.1, 0.7);
    p.wetLevel = 1.0f;
    p.dryLevel = 0.0f;
    p.width = 1.0f;
    verb.setParameters (p);
    verb.setSampleRate (sr);
    verb.processStereo (rev.getWritePointer (0), rev.getWritePointer (1), rev.getNumSamples());

    // reverse the wet signal back -> swell that ends where the dry sound starts
    const float swellGain = (float) rng.uniform (0.5, 1.0);
    Buffer out (2, len + tail);
    out.clear();
    float wetPeak = 0.0f, dryPeak = 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        wetPeak = std::max (wetPeak, rev.getMagnitude (c, 0, rev.getNumSamples()));
        dryPeak = std::max (dryPeak, b.getMagnitude (c, 0, len));
    }
    const float wetNorm = wetPeak > 1.0e-6f ? swellGain * dryPeak / wetPeak : 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        for (int i = 0; i < len + tail; ++i)
            out.setSample (c, i, rev.getSample (c, len + tail - 1 - i) * wetNorm);
        out.addFrom (c, tail, b, c, 0, len);
    }
    b = std::move (out);
    return "Reverse tail " + juce::String (tail / sr, 2) + " s";
}

// ---- 8. transient shaper ---------------------------------------------------------------------
juce::String transientShaper (Buffer& b, double sr, Rng& rng, double s)
{
    const double attackAmt = rng.uniform (-1.0, 1.0) * (0.3 + 0.7 * s);
    const double sustainAmt = rng.uniform (-1.0, 1.0) * (0.3 + 0.7 * s);
    auto coef = [sr] (double ms) { return std::exp (-1.0 / (0.001 * ms * sr)); };
    const double fastA = coef (0.5), fastR = coef (30.0), slowA = coef (25.0), slowR = coef (250.0);
    double ef = 0.0, es = 0.0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double x = std::max (std::abs (b.getSample (0, i)), std::abs (b.getSample (1, i)));
        ef = x > ef ? fastA * ef + (1 - fastA) * x : fastR * ef + (1 - fastR) * x;
        es = x > es ? slowA * es + (1 - slowA) * x : slowR * es + (1 - slowR) * x;
        const double ratio = (ef + 1e-6) / (es + 1e-6);   // > 1 on transients
        const double tr = std::log2 (juce::jlimit (0.25, 8.0, ratio));
        double gain = std::exp2 (attackAmt * tr * 1.5) * std::exp2 (sustainAmt * (tr < 0.2 ? 1.0 : 0.0) * 0.8);
        gain = juce::jlimit (0.1, 8.0, gain);
        for (int c = 0; c < 2; ++c)
            b.setSample (c, i, (float) (b.getSample (c, i) * gain));
    }
    return juce::String ("Transient ") + (attackAmt >= 0 ? "punch" : "soften");
}

void finalise (Buffer& b, double sr)
{
    normalise (b, juce::Decibels::decibelsToGain (-1.0f));
    // trim trailing silence (keep 20 ms)
    const float thr = juce::Decibels::decibelsToGain (-72.0f);
    int last = b.getNumSamples() - 1;
    while (last > 64 && std::abs (b.getSample (0, last)) < thr && std::abs (b.getSample (1, last)) < thr)
        --last;
    const int newLen = std::min (b.getNumSamples(), last + 1 + (int) (0.02 * sr));
    b.setSize (2, std::max (64, newLen), true, true, false);
    // short fade-out to avoid clicks at the end
    const int fade = std::min (b.getNumSamples(), (int) (0.005 * sr));
    b.applyGainRamp (b.getNumSamples() - fade, fade, 1.0f, 0.0f);
}
} // namespace

//==============================================================================
EvolveResult evolve (const SampleData& source, uint64_t seed, float chaosIn, uint32_t processMask, const std::atomic<bool>* cancel)
{
    EvolveResult result;
    if (source.length() < 2)
        return result;

    const double chaos = juce::jlimit (0.0, 1.0, (double) chaosIn);
    const double sr = source.sampleRate;
    Rng rng (seed ^ 0x4d757461666f726dull); // "Mutaform"

    std::vector<int> enabled;
    for (int i = 0; i < (int) EvolveProcess::count; ++i)
        if (processMask == 0 || (processMask & (1u << i)) != 0)
            enabled.push_back (i);
    if (enabled.empty())
        for (int i = 0; i < (int) EvolveProcess::count; ++i)
            enabled.push_back (i);

    const int count = juce::jlimit (2, 5, 2 + (int) std::floor (chaos * 3.0 + rng.uniform (0.0, 0.999)));

    // choose a chain: distinct processes while possible, then repeats
    std::vector<int> pool = enabled, chain;
    for (int i = 0; i < count; ++i)
    {
        if (pool.empty())
            pool = enabled;
        const int pick = rng.range (0, (int) pool.size() - 1);
        chain.push_back (pool[(size_t) pick]);
        pool.erase (pool.begin() + pick);
    }

    Buffer b (source.audio);
    for (int id : chain)
    {
        if (cancel != nullptr && cancel->load())
            return {};
        const double strength = juce::jlimit (0.05, 1.0, 0.08 + chaos * rng.uniform (0.55, 1.0));
        juce::String desc;
        switch ((EvolveProcess) id)
        {
            case EvolveProcess::pitchShift:      desc = pitchShift (b, sr, rng, strength); break;
            case EvolveProcess::grainShuffle:    desc = grainShuffle (b, sr, rng, strength); break;
            case EvolveProcess::envelopeFilter:  desc = envelopeFilter (b, sr, rng, strength); break;
            case EvolveProcess::saturation:      desc = saturation (b, sr, rng, strength); break;
            case EvolveProcess::timeStretch:     desc = timeStretch (b, sr, rng, strength); break;
            case EvolveProcess::ringMod:         desc = ringMod (b, sr, rng, strength); break;
            case EvolveProcess::reverseTail:     desc = reverseTail (b, sr, rng, strength); break;
            case EvolveProcess::transientShaper: desc = transientShaper (b, sr, rng, strength); break;
            case EvolveProcess::count:           break;
        }
        result.steps.add (desc);
    }

    finalise (b, sr);

    auto out = std::make_shared<SampleData>();
    out->sampleRate = sr;
    out->name = source.name;
    out->audio = std::move (b);
    result.audio = std::move (out);
    return result;
}
} // namespace mf
