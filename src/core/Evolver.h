#pragma once
#include "SampleData.h"
#include <atomic>
#include <cstdint>

namespace mf
{
/** Small deterministic PRNG (SplitMix64 seeding + xoshiro256**), identical on all platforms. */
class Rng
{
public:
    explicit Rng (uint64_t seed);
    uint64_t next() noexcept;
    double uniform() noexcept;                       // [0,1)
    double uniform (double lo, double hi) noexcept { return lo + (hi - lo) * uniform(); }
    int range (int lo, int hiInclusive) noexcept;    // [lo, hi]
    bool chance (double p) noexcept { return uniform() < p; }

private:
    uint64_t s[4] {};
};

enum class EvolveProcess : int
{
    pitchShift = 0, grainShuffle, envelopeFilter, saturation, timeStretch, ringMod, reverseTail, transientShaper, count
};

const char* processName (EvolveProcess p);

struct EvolveResult
{
    std::shared_ptr<SampleData> audio; // null when cancelled / failed
    juce::StringArray steps;           // human-readable description of the chain
};

/** Offline variation renderer. Pure function of (source, seed, chaos, mask): same inputs give
    bit-identical output. Never call on the audio thread (allocates, may take ~100 ms).
    @param chaos        0..1, controls number (2..5) and strength of processes
    @param processMask  bit i enables EvolveProcess i (0 = all enabled)
*/
EvolveResult evolve (const SampleData& source, uint64_t seed, float chaos, uint32_t processMask = 0xff,
                     const std::atomic<bool>* cancel = nullptr);

inline constexpr double kMaxEvolveSeconds = 20.0;
} // namespace mf
