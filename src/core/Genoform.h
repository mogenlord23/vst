#pragma once
#include "SampleData.h"
#include <array>
#include <atomic>
#include <functional>

namespace mf
{
/** GENOFORM — MUTAFORM's resynthesis technology.
    A compact subtractive/FM synth voice is described by a 20-gene genome (all 0..1).
    A genetic algorithm (tournament selection, uniform crossover, annealed gaussian mutation,
    elitism) evolves genomes so the rendered sound matches a target sample's spectral
    envelope (32 log bands x 8 time slices) and loudness contour. Deterministic per seed. */
inline constexpr int kGenes = 20;
using Genome = std::array<float, kGenes>;

const char* geneName (int index);

/** Renders the genome as a one-shot at `freqHz`. */
std::shared_ptr<SampleData> renderGenome (const Genome& g, double sampleRate, double seconds, double freqHz);

struct GenoformResult
{
    Genome best {};
    float score = 0.0f;              // 0..1 similarity (1 = identical features)
    std::shared_ptr<SampleData> audio; // rendered at the source sample rate
    int generations = 0;
};

struct GenoformOptions
{
    int population = 32;
    int generations = 40;
    juce::uint64 seed = 1;
};

/** Runs the evolution. `progress` (optional) receives 0..1; return false from it to cancel. */
GenoformResult genoformMatch (const SampleData& target, const GenoformOptions& opt,
                              const std::function<bool (float progress, float bestScore)>& progress = {});

/** Feature distance used as fitness (exposed for tests). Lower = more similar. */
float genoformDistance (const SampleData& a, const SampleData& b);
} // namespace mf
