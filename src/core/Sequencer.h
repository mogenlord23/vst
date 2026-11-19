#pragma once
#include "LockFreeBox.h"
#include "Melody.h"
#include "SamplerEngine.h"

namespace mf
{
/** Audio-thread friendly copy of a MelodyPattern (fixed capacity, no heap use when read). */
struct SeqPattern
{
    struct Note { int start = 0, length = 1, note = 60; float velocity = 0.8f; };
    int totalSteps = 64;
    int count = 0;
    std::array<Note, 512> notes {};

    static SeqPattern* fromMelody (const MelodyPattern& p);
};

/** Plays a SeqPattern in sync with the host transport (PPQ) or with an internal clock when the
    host is stopped. Lock-free, allocation-free in process(). 16th-note steps. */
class MelodySequencer
{
public:
    void prepare (double sampleRate);
    void setPattern (SeqPattern* p) { box.publish (p); }   // message thread, takes ownership
    void collectGarbage() { box.collectGarbage(); }

    /** Audio thread. Writes note on/off events (sorted by samplePos) and returns their count. */
    int process (int numSamples, bool enabled, bool hostPlaying, double hostPpq, double bpm, NoteEvent* out, int maxOut);

    /** UI info (any thread). */
    int getCurrentStep() const noexcept { return currentStep.load (std::memory_order_relaxed); }
    int getCurrentNote() const noexcept { return currentNote.load (std::memory_order_relaxed); }
    int getPatternSteps() const noexcept { return patternSteps.load (std::memory_order_relaxed); }

private:
    struct Active { int note = -1; double endBeat = 0; };
    void allOff (NoteEvent* out, int& n, int maxOut, int pos);

    LockFreeBox<SeqPattern> box;
    double sr = 44100.0;
    double internalBeat = 0.0;
    double expectedBeat = -1.0;
    bool wasEnabled = false;
    std::array<Active, 32> active {};
    std::atomic<int> currentStep { -1 }, currentNote { -1 }, patternSteps { 64 };
};
} // namespace mf
