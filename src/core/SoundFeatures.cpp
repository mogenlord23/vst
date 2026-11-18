#include "SoundFeatures.h"
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
