#include "Sequencer.h"
#include <cmath>

namespace mf
{
SeqPattern* SeqPattern::fromMelody (const MelodyPattern& p)
{
    auto* s = new SeqPattern();
    s->totalSteps = std::max (1, p.totalSteps());
    s->count = (int) std::min (p.notes.size(), s->notes.size());
    for (int i = 0; i < s->count; ++i)
    {
        const auto& n = p.notes[(size_t) i];
        s->notes[(size_t) i] = { n.start, n.length, n.note, n.velocity };
    }
    return s;
}

void MelodySequencer::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    internalBeat = 0.0;
    expectedBeat = -1.0;
    for (auto& a : active) a.note = -1;
}

void MelodySequencer::allOff (NoteEvent* out, int& n, int maxOut, int pos)
{
    for (auto& a : active)
        if (a.note >= 0)
        {
            if (n < maxOut) out[n++] = { pos, a.note, 0.0f };
            a.note = -1;
        }
}

int MelodySequencer::process (int numSamples, bool enabled, bool hostPlaying, double hostPpq, double bpm, NoteEvent* out, int maxOut)
{
    int n = 0;
    const SeqPattern* pat = box.acquire();
    if (pat != nullptr)
        patternSteps.store (pat->totalSteps, std::memory_order_relaxed);
    bpm = juce::jlimit (20.0, 400.0, bpm > 0 ? bpm : 120.0);
    const double bps = bpm / 60.0 / sr; // beats per sample

    if (! enabled || pat == nullptr || pat->count == 0)
    {
        allOff (out, n, maxOut, 0);
        wasEnabled = false;
        currentStep.store (-1, std::memory_order_relaxed);
        currentNote.store (-1, std::memory_order_relaxed);
        return n;
    }

    double beatStart;
    if (hostPlaying)
        beatStart = hostPpq;
    else
    {
        if (! wasEnabled) internalBeat = 0.0;
        beatStart = internalBeat;
    }
    // transport jump / restart: silence held notes
    if (wasEnabled && expectedBeat >= 0.0 && std::abs (beatStart - expectedBeat) > 0.05)
        allOff (out, n, maxOut, 0);
    wasEnabled = true;

    const double beatEnd = beatStart + numSamples * bps;
    auto posOf = [&] (double beat) { return juce::jlimit (0, numSamples - 1, (int) std::floor ((beat - beatStart) / bps)); };

    // note offs
    for (auto& a : active)
        if (a.note >= 0 && a.endBeat < beatEnd)
        {
            if (n < maxOut) out[n++] = { posOf (std::max (beatStart, a.endBeat)), a.note, 0.0f };
            a.note = -1;
        }

    // note ons (window may cross the loop boundary)
    const double loopBeats = pat->totalSteps * 0.25;
    const double k0 = std::floor (beatStart / loopBeats);
    for (int k = 0; k < 2; ++k)
    {
        const double base = (k0 + k) * loopBeats;
        for (int i = 0; i < pat->count; ++i)
        {
            const auto& nt = pat->notes[(size_t) i];
            const double sb = base + nt.start * 0.25;
            if (sb < beatStart || sb >= beatEnd)
                continue;
            const int pos = posOf (sb);
            for (auto& a : active) // retrigger of a held note
                if (a.note == nt.note)
                {
                    if (n < maxOut) out[n++] = { pos, a.note, 0.0f };
                    a.note = -1;
                }
            if (n < maxOut) out[n++] = { pos, nt.note, nt.velocity };
            for (auto& a : active)
                if (a.note < 0)
                {
                    a.note = nt.note;
                    a.endBeat = sb + nt.length * 0.25 * 0.96;
                    break;
                }
            currentNote.store (nt.note, std::memory_order_relaxed);
        }
    }

    // stable sort by position (insertion sort, n is small)
    for (int i = 1; i < n; ++i)
    {
        const auto e = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].samplePos > e.samplePos) { out[j + 1] = out[j]; --j; }
        out[j + 1] = e;
    }

    currentStep.store ((int) std::fmod (std::floor (beatStart * 4.0), (double) pat->totalSteps), std::memory_order_relaxed);
    expectedBeat = beatEnd;
    if (! hostPlaying)
        internalBeat = beatEnd;
    return n;
}
} // namespace mf
