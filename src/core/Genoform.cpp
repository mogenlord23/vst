#include "Genoform.h"
#include "Evolver.h"       // Rng
#include "Interp.h"
#include "SoundFeatures.h" // detectPitch
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace mf
{
const char* geneName (int i)
{
    static const char* names[kGenes] = { "wave", "ratio", "detune", "osc2", "fm", "fm_decay", "noise", "noise_color",
                                         "cutoff", "filter_env", "filter_decay", "resonance", "attack", "decay", "sustain",
                                         "pitch_env", "pitch_decay", "drive", "release", "sub" };
    return juce::isPositiveAndBelow (i, kGenes) ? names[i] : "?";
}

namespace
{
constexpr double kTwoPi = juce::MathConstants<double>::twoPi;
constexpr double kAnalysisRate = 22050.0;
constexpr int kBands = 32, kSlices = 8, kEnvPoints = 24, kFftOrder = 10, kFft = 1 << kFftOrder;

struct Features
{
    std::array<float, kBands * kSlices> bands {};
    std::array<float, kEnvPoints> env {};
};

/** Mono copy resampled to the analysis rate, max 1.5 s. */
std::vector<float> analysisSignal (const SampleData& s)
{
    const double ratio = s.sampleRate / kAnalysisRate;
    const int n = std::min ((int) (s.length() / ratio), (int) (1.5 * kAnalysisRate));
    std::vector<float> out ((size_t) std::max (n, kFft), 0.0f);
    const auto* l = s.audio.getReadPointer (0);
    const auto* r = s.audio.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        const double p = i * ratio;
        out[(size_t) i] = 0.5f * (readHermite (l, s.length(), p) + readHermite (r, s.length(), p));
    }
    return out;
}

Features analyse (const std::vector<float>& x, juce::dsp::FFT& fft)
{
    Features f;
    const int n = (int) x.size();
    std::vector<float> buf ((size_t) kFft * 2);
    static const auto bandEdges = []
    {
        std::array<int, kBands + 1> e {};
        for (int b = 0; b <= kBands; ++b)
        {
            const double hz = 60.0 * std::pow (10000.0 / 60.0, (double) b / kBands);
            e[(size_t) b] = juce::jlimit (1, kFft / 2 - 1, (int) std::round (hz / kAnalysisRate * kFft));
        }
        return e;
    }();
    for (int sl = 0; sl < kSlices; ++sl)
    {
        const int centre = (int) ((sl + 0.5) / kSlices * n);
        const int start = juce::jlimit (0, std::max (0, n - kFft), centre - kFft / 2);
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < kFft && start + i < n; ++i)
            buf[(size_t) i] = x[(size_t) (start + i)] * (float) (0.5 - 0.5 * std::cos (kTwoPi * i / (kFft - 1)));
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        for (int b = 0; b < kBands; ++b)
        {
            double e = 0.0;
            const int a = bandEdges[(size_t) b], z = std::max (a + 1, bandEdges[(size_t) b + 1]);
            for (int k = a; k < z; ++k) e += (double) buf[(size_t) k] * buf[(size_t) k];
            f.bands[(size_t) (sl * kBands + b)] = (float) (10.0 * std::log10 (e / (z - a) + 1e-10));
        }
    }
    for (int p = 0; p < kEnvPoints; ++p)
    {
        const int a = p * n / kEnvPoints, z = std::max (a + 1, (p + 1) * n / kEnvPoints);
        double e = 0.0;
        for (int i = a; i < z; ++i) e += (double) x[(size_t) i] * x[(size_t) i];
        f.env[(size_t) p] = (float) (10.0 * std::log10 (e / (z - a) + 1e-10));
    }
    // normalise loudness so the GA matches shape, not absolute level
    float bmax = -200.0f, emax = -200.0f;
    for (auto v : f.bands) bmax = std::max (bmax, v);
    for (auto v : f.env) emax = std::max (emax, v);
    for (auto& v : f.bands) v = std::max (v - bmax, -70.0f);
    for (auto& v : f.env) v = std::max (v - emax, -60.0f);
    return f;
}

float distance (const Features& a, const Features& b)
{
    double db = 0.0, de = 0.0;
    for (size_t i = 0; i < a.bands.size(); ++i) db += std::abs (a.bands[i] - b.bands[i]);
    for (size_t i = 0; i < a.env.size(); ++i) de += std::abs (a.env[i] - b.env[i]);
    return (float) (db / a.bands.size() / 30.0 + 0.6 * de / a.env.size() / 30.0);
}

inline double sq (double v) { return v * v; }
} // namespace

std::shared_ptr<SampleData> renderGenome (const Genome& g, double sr, double seconds, double freq)
{
    auto out = std::make_shared<SampleData>();
    out->sampleRate = sr;
    out->name = "Genoform";
    const int n = std::max (64, (int) (seconds * sr));
    out->audio.setSize (2, n);
    out->audio.clear();

    static constexpr double ratios[] = { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 7.0 };
    const double wave = g[0] * 3.0;
    const double ratio = ratios[juce::jlimit (0, 7, (int) (g[1] * 8.0))];
    const double detune = (g[2] - 0.5) * 0.03;
    const double osc2 = g[3];
    const double fmIdx = sq (g[4]) * 7.0;
    const double fmDec = 0.01 + sq (g[5]) * 2.0;
    const double noise = sq (g[6]) * 0.8;
    const double noiseLp = 0.02 + g[7] * 0.95;
    const double cutoff = 40.0 * std::pow (2.0, g[8] * 9.0);
    const double fEnv = (g[9] - 0.3) * 7.0; // octaves
    const double fDec = 0.005 + sq (g[10]) * 2.0;
    const double res = g[11] * 0.92;
    const double att = std::pow (g[12], 3.0) * 0.6 + 0.0005;
    const double dec = 0.02 + sq (g[13]) * 3.0;
    const double sus = g[14];
    const double pEnv = (g[15] - 0.5) * 24.0;
    const double pDec = 0.002 + sq (g[16]) * 0.4;
    const double drive = 1.0 + g[17] * 5.0;
    const double rel = 0.02 + sq (g[18]) * 1.2;
    const double sub = g[19] * 0.6;
    const double gateEnd = seconds * 0.7;

    Rng rng (0x5eed);
    double ph1 = 0, ph2 = 0, phs = 0, nlp = 0, ic1 = 0, ic2 = 0, relStart = -1, relLevel = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        // amplitude envelope (ADSR, gate held for 70 % of the length)
        double amp;
        if (t < gateEnd)
        {
            amp = t < att ? t / att : sus + (1.0 - sus) * std::exp (-(t - att) / dec * 3.0);
            relLevel = amp;
        }
        else
        {
            if (relStart < 0) relStart = t;
            amp = relLevel * std::exp (-(t - relStart) / rel * 4.0);
        }
        const double pitch = freq * std::exp2 (pEnv * std::exp (-t / pDec) / 12.0);
        const double fmEnv = std::exp (-t / fmDec);
        ph2 += kTwoPi * pitch * ratio * (1.0 + detune) / sr;
        const double mod = std::sin (ph2);
        ph1 += kTwoPi * pitch / sr;
        phs += kTwoPi * pitch * 0.5 / sr;
        if (ph1 > kTwoPi * 64) { ph1 -= kTwoPi * 64; }
        if (ph2 > kTwoPi * 64) { ph2 -= kTwoPi * 64; }
        const double p = ph1 + fmIdx * fmEnv * mod;
        const double u = std::fmod (p / kTwoPi, 1.0);
        const double sine = std::sin (p), tri = 1.0 - 4.0 * std::abs (u - 0.5), saw = 2.0 * u - 1.0, sqr = u < 0.5 ? 1.0 : -1.0;
        double o1;
        if (wave < 1.0)      o1 = sine + (tri - sine) * wave;
        else if (wave < 2.0) o1 = tri + (saw - tri) * (wave - 1.0);
        else                 o1 = saw + (sqr - saw) * (wave - 2.0);
        double x = o1 * (1.0 - osc2 * 0.5) + mod * osc2 * 0.5 + std::sin (phs) * sub;
        nlp += (rng.uniform (-1.0, 1.0) - nlp) * noiseLp;
        x += nlp * noise * 2.0;
        // filter with its own envelope
        const double fc = juce::jlimit (30.0, sr * 0.45, cutoff * std::exp2 (fEnv * std::exp (-t / fDec)));
        const double gg = std::tan (juce::MathConstants<double>::pi * fc / sr), k = 2.0 - 2.0 * res;
        const double a1 = 1.0 / (1.0 + gg * (gg + k));
        const double v3 = x - ic2, v1 = a1 * ic1 + gg * a1 * v3, v2 = ic2 + gg * v1;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        if (! std::isfinite (ic1) || ! std::isfinite (ic2)) { ic1 = ic2 = 0.0; }
        const double y = std::tanh (v2 * drive) / std::tanh (drive) * amp;
        out->audio.setSample (0, i, (float) y);
        out->audio.setSample (1, i, (float) y);
    }
    normalise (out->audio, juce::Decibels::decibelsToGain (-1.0f));
    const int fade = std::min (n, (int) (0.004 * sr));
    out->audio.applyGainRamp (n - fade, fade, 1.0f, 0.0f);
    return out;
}

float genoformDistance (const SampleData& a, const SampleData& b)
{
    juce::dsp::FFT fft (kFftOrder);
    return distance (analyse (analysisSignal (a), fft), analyse (analysisSignal (b), fft));
}

GenoformResult genoformMatch (const SampleData& target, const GenoformOptions& opt,
                              const std::function<bool (float, float)>& progress)
{
    GenoformResult res;
    juce::dsp::FFT fft (kFftOrder);
    const auto targetSig = analysisSignal (target);
    const auto tf = analyse (targetSig, fft);
    const double seconds = std::min (1.5, std::max (0.15, target.seconds()));
    const auto pi = detectPitch (target);
    const double freq = pi.confidence > 0.5f ? 440.0 * std::exp2 ((pi.midi - 69.0) / 12.0) : 261.63;

    Rng r (opt.seed ^ 0x47656e6f666f726dull);
    const int P = juce::jlimit (8, 256, opt.population), G = juce::jlimit (1, 500, opt.generations);
    std::vector<Genome> pop ((size_t) P);
    std::vector<float> fit ((size_t) P);
    for (auto& g : pop)
        for (auto& v : g) v = (float) r.uniform();
    // a few musically sensible starting points
    if (P > 4)
    {
        pop[0] = { 0.0f, 0.15f, 0.5f, 0.0f, 0.0f, 0.3f, 0.0f, 0.5f, 0.8f, 0.3f, 0.3f, 0.1f, 0.0f, 0.4f, 0.0f, 0.5f, 0.1f, 0.0f, 0.2f, 0.0f }; // pluck-ish sine
        pop[1] = { 0.7f, 0.15f, 0.55f, 0.3f, 0.2f, 0.4f, 0.0f, 0.5f, 0.6f, 0.6f, 0.3f, 0.3f, 0.1f, 0.5f, 0.6f, 0.5f, 0.1f, 0.2f, 0.4f, 0.2f }; // saw pad
        pop[2] = { 0.0f, 0.4f, 0.5f, 0.0f, 0.6f, 0.2f, 0.0f, 0.5f, 0.9f, 0.3f, 0.2f, 0.0f, 0.0f, 0.35f, 0.0f, 0.5f, 0.1f, 0.0f, 0.3f, 0.0f }; // FM bell
        pop[3] = { 0.3f, 0.15f, 0.5f, 0.0f, 0.0f, 0.2f, 0.9f, 0.4f, 0.6f, 0.5f, 0.1f, 0.3f, 0.0f, 0.15f, 0.0f, 0.5f, 0.1f, 0.3f, 0.1f, 0.0f }; // noise hit
    }
    auto evaluate = [&] (const Genome& g)
    {
        auto s = renderGenome (g, kAnalysisRate, seconds, freq);
        std::vector<float> sig ((size_t) std::max (s->length(), kFft), 0.0f);
        for (int i = 0; i < s->length(); ++i) sig[(size_t) i] = s->audio.getSample (0, i);
        return distance (tf, analyse (sig, fft));
    };
    for (int i = 0; i < P; ++i) fit[(size_t) i] = evaluate (pop[(size_t) i]);

    for (int gen = 0; gen < G; ++gen)
    {
        std::vector<int> order ((size_t) P);
        for (int i = 0; i < P; ++i) order[(size_t) i] = i;
        std::sort (order.begin(), order.end(), [&] (int a, int b) { return fit[(size_t) a] < fit[(size_t) b]; });
        const float best = fit[(size_t) order[0]];
        if (progress && ! progress ((float) gen / G, std::exp (-best * 2.5f)))
            return {};

        const int elites = std::max (2, P / 8);
        std::vector<Genome> next;
        std::vector<float> nextFit;
        for (int e = 0; e < elites; ++e)
        {
            next.push_back (pop[(size_t) order[(size_t) e]]);
            nextFit.push_back (fit[(size_t) order[(size_t) e]]);
        }
        auto tournament = [&]
        {
            int best2 = r.range (0, P - 1);
            for (int k = 0; k < 2; ++k)
            {
                const int c = r.range (0, P - 1);
                if (fit[(size_t) c] < fit[(size_t) best2]) best2 = c;
            }
            return best2;
        };
        const double sigma = 0.28 * (1.0 - (double) gen / G) + 0.03;
        while ((int) next.size() < P)
        {
            const auto& a = pop[(size_t) tournament()];
            const auto& b = pop[(size_t) tournament()];
            Genome child;
            for (int k = 0; k < kGenes; ++k)
            {
                float v = r.chance (0.5) ? a[(size_t) k] : b[(size_t) k];
                if (r.chance (0.22))
                {
                    // approximately gaussian (sum of uniforms)
                    const double gauss = (r.uniform() + r.uniform() + r.uniform() - 1.5) * 2.0;
                    v = (float) juce::jlimit (0.0, 1.0, v + gauss * sigma);
                }
                child[(size_t) k] = v;
            }
            next.push_back (child);
            nextFit.push_back (evaluate (child));
        }
        pop.swap (next);
        fit.swap (nextFit);
        res.generations = gen + 1;
    }
    int bi = 0;
    for (int i = 1; i < P; ++i)
        if (fit[(size_t) i] < fit[(size_t) bi]) bi = i;
    res.best = pop[(size_t) bi];
    res.score = std::exp (-fit[(size_t) bi] * 2.5f);
    res.audio = renderGenome (res.best, target.sampleRate, std::min (4.0, std::max (0.3, target.seconds())), freq);
    if (progress) progress (1.0f, res.score);
    return res;
}
} // namespace mf
