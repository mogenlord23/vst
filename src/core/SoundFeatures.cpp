#include "SoundFeatures.h"
namespace mf { struct PitchInfo; }
#include <cmath>

namespace mf
{
SoundFeatures analyseSound (const SampleData& s)
{
    SoundFeatures f;
    const int n = s.length();
    if (n < 64)
        return f;
    const auto* l = s.audio.getReadPointer (0);
    const auto* r = s.audio.getReadPointer (1);
    const double sr = s.sampleRate;

    // 10 ms frames: RMS + zero-crossing rate
    const int frame = std::max (32, (int) (0.01 * sr));
    const int frames = std::max (1, n / frame);
    std::vector<double> rms ((size_t) frames, 0.0), zcr ((size_t) frames, 0.0);
    double peakRms = 0.0;
    for (int fi = 0; fi < frames; ++fi)
    {
        double e = 0.0;
        int z = 0;
        float prev = 0.5f * (l[fi * frame] + r[fi * frame]);
        for (int i = fi * frame; i < std::min (n, (fi + 1) * frame); ++i)
        {
            const float x = 0.5f * (l[i] + r[i]);
            e += (double) x * x;
            if ((x > 0.0f) != (prev > 0.0f)) ++z;
            prev = x;
        }
        rms[(size_t) fi] = std::sqrt (e / frame);
        zcr[(size_t) fi] = (double) z / frame;
        peakRms = std::max (peakRms, rms[(size_t) fi]);
    }
    if (peakRms < 1.0e-6)
        return f;

    // energy-weighted zero-crossing rate -> brightness (~ spectral centroid proxy)
    double wz = 0.0, w = 0.0;
    for (int fi = 0; fi < frames; ++fi)
    {
        wz += zcr[(size_t) fi] * rms[(size_t) fi];
        w += rms[(size_t) fi];
    }
    const double zc = w > 0 ? wz / w : 0.0;
    const double centroidHz = zc * sr * 0.5;
    f.brightness = (float) juce::jlimit (0.0, 1.0, (std::log2 (std::max (50.0, centroidHz)) - std::log2 (300.0)) / (std::log2 (5000.0) - std::log2 (300.0)));

    // punch: early peak vs. average level of the rest
    int peakFrame = 0;
    for (int fi = 0; fi < frames; ++fi)
        if (rms[(size_t) fi] >= peakRms) { peakFrame = fi; break; }
    double body = 0.0;
    int bodyCount = 0;
    for (int fi = peakFrame + 5; fi < frames; ++fi) { body += rms[(size_t) fi]; ++bodyCount; }
    body = bodyCount > 0 ? body / bodyCount : peakRms;
    const double earliness = 1.0 - juce::jlimit (0.0, 1.0, peakFrame * 0.01 / 0.25); // peak within 250 ms
    f.punch = (float) juce::jlimit (0.0, 1.0, earliness * juce::jlimit (0.0, 1.0, (peakRms / std::max (1.0e-6, body) - 1.0) / 5.0));

    // motion: frame-to-frame fluctuation of level and brightness on the audible part
    double flux = 0.0;
    int fc = 0;
    for (int fi = 1; fi < frames; ++fi)
    {
        if (rms[(size_t) fi] < peakRms * 0.05) continue;
        flux += std::abs (std::log ((rms[(size_t) fi] + 1e-6) / (rms[(size_t) fi - 1] + 1e-6))) * 0.5
              + std::abs (zcr[(size_t) fi] - zcr[(size_t) fi - 1]) * 20.0;
        ++fc;
    }
    f.motion = (float) juce::jlimit (0.0, 1.0, fc > 0 ? flux / fc / 0.6 : 0.0);

    f.tags.add (f.brightness >= 0.5f ? "BRIGHT" : "DARK");
    f.tags.add (f.punch >= 0.35f ? "PLUCKY" : "SMOOTH");
    f.tags.add (f.motion >= 0.4f ? "MOVING" : "STEADY");
    const auto pi = detectPitch (s);
    f.pitchMidi = pi.midi;
    f.pitchConfidence = pi.confidence;
    if (pi.confidence >= 0.5f) f.tags.add ("TONAL");
    return f;
}

juce::String variationName (juce::uint64 seed)
{
    static const char* adj[] = { "VELVET", "RUSTY", "NEON", "HOLLOW", "TINY", "MOLTEN", "GLASSY", "DUSTY", "LUCKY", "SLEEPY",
                                 "FIZZY", "COSMIC", "WOBBLY", "SILVER", "FERAL", "MISTY", "CRISPY", "LUNAR", "AMBER", "SALTY",
                                 "WARM", "FROSTY", "BRAVE", "SHY", "ODD", "ZESTY", "SOFT", "WILD", "QUIET", "SUNNY" };
    static const char* noun[] = { "SPARK", "PEBBLE", "COMET", "BLOOM", "BEETLE", "ORBIT", "MOTH", "CRATER", "LANTERN", "DUNE",
                                  "GECKO", "NEBULA", "PRISM", "CACTUS", "BUBBLE", "ECHO", "FOSSIL", "GLIMMER", "SPROUT", "TIDE",
                                  "RIPPLE", "STATIC", "MARBLE", "WHISKER", "SPORE", "CINDER", "PULSE", "QUARTZ", "DRIFT", "HUM" };
    auto x = seed * 0x9e3779b97f4a7c15ull;
    x ^= x >> 29;
    return juce::String (adj[x % 30]) + " " + noun[(x >> 17) % 30];
}
} // namespace mf

namespace mf
{
PitchInfo detectPitch (const SampleData& s)
{
    PitchInfo out;
    const double sr = s.sampleRate;
    const int n = s.length();
    if (n < 2048 || sr <= 0)
        return out;

    // downmix
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i)
        x[(size_t) i] = 0.5f * (s.audio.getSample (0, i) + s.audio.getSample (1, i));

    const int W = 1024;
    const int tauMin = std::max (2, (int) (sr / 2000.0)), tauMax = std::min (W - 1, (int) (sr / 50.0));
    std::vector<double> d ((size_t) tauMax + 2);
    std::vector<float> pitches, confs;
    const int startAt = std::min (n - 2 * W - 1, (int) (0.03 * sr));
    const int endAt = std::min (n - 2 * W - 1, (int) (0.6 * sr));
    for (int pos = std::max (0, startAt); pos <= endAt; pos += W / 2)
    {
        double energy = 0.0;
        for (int i = 0; i < W; ++i) energy += (double) x[(size_t) (pos + i)] * x[(size_t) (pos + i)];
        if (energy < 1.0e-4 * W) continue;
        // difference function + cumulative mean normalisation
        double running = 0.0;
        d[0] = 1.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double sum = 0.0;
            for (int i = 0; i < W; ++i)
            {
                const double diff = (double) x[(size_t) (pos + i)] - x[(size_t) (pos + i + tau)];
                sum += diff * diff;
            }
            running += sum;
            d[(size_t) tau] = running > 0 ? sum * tau / running : 1.0;
        }
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (d[(size_t) tau] < 0.15 && d[(size_t) tau] <= d[(size_t) tau + 1])
            {
                best = tau;
                break;
            }
        if (best < 0)
        {
            double mn = 1e9;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (d[(size_t) tau] < mn) { mn = d[(size_t) tau]; best = tau; }
        }
        // parabolic interpolation
        double t = best;
        if (best > tauMin && best < tauMax - 1)
        {
            const double a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
            const double den = a - 2 * b + c;
            if (std::abs (den) > 1e-12) t = best + 0.5 * (a - c) / den;
        }
        const double f = sr / t;
        pitches.push_back ((float) (69.0 + 12.0 * std::log2 (f / 440.0)));
        confs.push_back ((float) juce::jlimit (0.0, 1.0, 1.0 - d[(size_t) best]));
    }
    if (pitches.empty())
        return out;
    // median pitch over confident frames
    std::vector<float> good;
    float confSum = 0.0f;
    for (size_t i = 0; i < pitches.size(); ++i)
    {
        confSum += confs[i];
        if (confs[i] > 0.6f) good.push_back (pitches[i]);
    }
    out.confidence = confSum / (float) pitches.size();
    auto& src = good.empty() ? pitches : good;
    std::nth_element (src.begin(), src.begin() + (long) src.size() / 2, src.end());
    out.midi = src[src.size() / 2];
    if (good.size() < pitches.size() / 2)
        out.confidence = std::min (out.confidence, 0.5f);
    return out;
}

std::shared_ptr<SampleData> tonalize (const SampleData& s, double freqHz, float amount)
{
    auto out = std::make_shared<SampleData> (s);
    const double sr = s.sampleRate;
    const int n = s.length();
    const int tail = (int) (0.35 * sr);
    out->audio.setSize (2, n + tail, true, true, false);
    const float wet = juce::jlimit (0.0f, 1.0f, amount);
    // three tuned feedback combs (fundamental, octave, fifth above octave) with damping
    const double ratios[3] = { 1.0, 2.0, 3.0 };
    const float gains[3] = { 1.0f, 0.5f, 0.3f };
    for (int c = 0; c < 2; ++c)
    {
        std::vector<float> acc ((size_t) (n + tail), 0.0f);
        for (int k = 0; k < 3; ++k)
        {
            const double period = sr / (freqHz * ratios[k]);
            const int len = (int) period + 4;
            std::vector<float> line ((size_t) len, 0.0f);
            int w = 0;
            float lp = 0.0f;
            const float fb = 0.985f - 0.01f * (float) k;
            for (int i = 0; i < n + tail; ++i)
            {
                const float in = i < n ? s.audio.getSample (c, i) : 0.0f;
                // fractional read
                double rp = w - period;
                while (rp < 0) rp += len;
                const int i0 = (int) rp;
                const float fr = (float) (rp - i0);
                const float y = line[(size_t) i0] + (line[(size_t) ((i0 + 1) % len)] - line[(size_t) i0]) * fr;
                lp += (y - lp) * 0.6f;
                line[(size_t) w] = in * 0.12f + lp * fb;
                w = (w + 1) % len;
                acc[(size_t) i] += y * gains[k];
            }
        }
        float pk = 1e-6f;
        for (auto v : acc) pk = std::max (pk, std::abs (v));
        float dryPk = 1e-6f;
        for (int i = 0; i < n; ++i) dryPk = std::max (dryPk, std::abs (s.audio.getSample (c, i)));
        const float g = dryPk / pk;
        for (int i = 0; i < n + tail; ++i)
        {
            const float dry = i < n ? s.audio.getSample (c, i) : 0.0f;
            out->audio.setSample (c, i, dry * (1.0f - wet) + acc[(size_t) i] * g * wet);
        }
    }
    normalise (out->audio, juce::Decibels::decibelsToGain (-1.0f));
    const int fade = std::min (out->length(), (int) (0.05 * sr));
    out->audio.applyGainRamp (out->length() - fade, fade, 1.0f, 0.0f);
    return out;
}
} // namespace mf
