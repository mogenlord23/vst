#include "Melody.h"
#include "Evolver.h" // Rng

namespace mf
{
juce::StringArray scaleNames() { return { "MAJOR", "MINOR", "DORIAN", "MAJ PENTA", "MIN PENTA", "HARM MINOR", "LYDIAN", "PHRYGIAN" }; }
juce::StringArray keyNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }

const std::vector<int>& scaleIntervals (Scale s)
{
    static const std::array<std::vector<int>, (size_t) Scale::count> tables {{
        { 0, 2, 4, 5, 7, 9, 11 },  // major
        { 0, 2, 3, 5, 7, 8, 10 },  // natural minor
        { 0, 2, 3, 5, 7, 9, 10 },  // dorian
        { 0, 2, 4, 7, 9 },         // major pentatonic
        { 0, 3, 5, 7, 10 },        // minor pentatonic
        { 0, 2, 3, 5, 7, 8, 11 },  // harmonic minor
        { 0, 2, 4, 6, 7, 9, 11 },  // lydian
        { 0, 1, 3, 5, 7, 8, 10 },  // phrygian
    }};
    return tables[(size_t) juce::jlimit (0, (int) Scale::count - 1, (int) s)];
}

int snapToScale (int note, int key, Scale scale)
{
    const auto& iv = scaleIntervals (scale);
    int best = note, bestD = 99;
    for (int d = -6; d <= 6; ++d)
    {
        const int pc = ((note + d - key) % 12 + 12) % 12;
        if (std::find (iv.begin(), iv.end(), pc) != iv.end() && std::abs (d) < bestD)
        {
            bestD = std::abs (d);
            best = note + d;
        }
    }
    return best;
}

namespace
{
// Scale-degree arithmetic over a 7-note (or 5-note) mode, unbounded octaves.
struct Mode
{
    int key;
    std::vector<int> iv;
    int base; // MIDI note of degree 0 in the middle octave

    int noteOf (int degree) const
    {
        const int n = (int) iv.size();
        const int oct = (int) std::floor ((double) degree / n);
        const int idx = degree - oct * n;
        return base + oct * 12 + iv[(size_t) idx];
    }
    int degreeNear (int midi) const
    {
        int best = 0, bd = 999;
        for (int d = -20; d <= 30; ++d)
        {
            const int dist = std::abs (noteOf (d) - midi);
            if (dist < bd) { bd = dist; best = d; }
        }
        return best;
    }
};

// Pleasant progressions in scale degrees (0-based), heptatonic.
const std::vector<std::vector<int>>& progressions (bool minorish)
{
    static const std::vector<std::vector<int>> major { { 0, 4, 5, 3 }, { 0, 5, 3, 4 }, { 5, 3, 0, 4 }, { 0, 3, 5, 4 }, { 0, 3, 0, 4 }, { 3, 4, 2, 5 } };
    static const std::vector<std::vector<int>> minor { { 0, 5, 2, 6 }, { 0, 3, 5, 4 }, { 0, 6, 5, 6 }, { 0, 5, 3, 4 }, { 5, 6, 0, 0 }, { 0, 3, 6, 2 } };
    return minorish ? minor : major;
}
} // namespace

MelodyPattern generateMelody (const MelodySettings& s)
{
    MelodyPattern pat;
    pat.bars = juce::jlimit (1, 8, s.bars);
    Rng r (s.seed * 0x9E3779B97F4A7C15ull + 0x4d656c6f6479ull);
    const float mood = juce::jlimit (0.0f, 1.0f, s.mood), dens = juce::jlimit (0.0f, 1.0f, s.density);

    Mode m { s.key, scaleIntervals (s.scale), 60 + s.key + 12 * juce::jlimit (-2, 2, s.octave) };
    if (m.base > 66 + 12 * s.octave) m.base -= 12; // keep the tonic around C3..F#3
    const int n = (int) m.iv.size();
    const bool minorish = s.scale == Scale::minor || s.scale == Scale::dorian || s.scale == Scale::minorPentatonic
                       || s.scale == Scale::harmonicMinor || s.scale == Scale::phrygian;

    // ---- chords: one per bar (two per bar for 1-bar patterns)
    const auto& progs = progressions (minorish);
    const auto& prog = progs[(size_t) r.range (0, (int) progs.size() - 1)];
    const int chordSlots = pat.bars == 1 ? 2 : pat.bars;
    std::vector<int> chordDeg;
    for (int i = 0; i < chordSlots; ++i)
    {
        int d = prog[(size_t) (i % 4)];
        if (n == 5) d = juce::jlimit (0, 4, (d * 5 + 3) / 7); // map to pentatonic degrees
        chordDeg.push_back (d);
    }
    for (int i = 0; i < chordSlots; ++i)
        for (int k = 0; k < (pat.bars == 1 ? 1 : 2); ++k)
            pat.chordRoots.push_back (m.noteOf (chordDeg[(size_t) i]) - 12);
    const int stepsPerChord = pat.totalSteps() / chordSlots;
    auto chordAt = [&] (int step) { return chordDeg[(size_t) juce::jlimit (0, chordSlots - 1, step / stepsPerChord)]; };
    auto isChordTone = [&] (int degree, int step)
    {
        const int root = chordAt (step);
        const int rel = ((degree - root) % n + n) % n;
        return n == 7 ? (rel == 0 || rel == 2 || rel == 4) : (rel == 0 || rel == 2 || rel == 3);
    };

    // ---- rhythm motifs (16 steps). Strong beats are favoured; mood adds syncopation.
    auto makeRhythm = [&] (float d)
    {
        std::array<bool, 16> on {};
        for (int st = 0; st < 16; ++st)
        {
            double p;
            if (st % 8 == 0)      p = 0.92;
            else if (st % 4 == 0) p = 0.45 + 0.4 * d;
            else if (st % 2 == 0) p = 0.12 + 0.55 * d + 0.15 * mood;
            else                  p = 0.02 + 0.30 * d * mood + 0.12 * mood;
            on[(size_t) st] = r.chance (p);
        }
        on[0] = true;
        return on;
    };
    const auto motifA = makeRhythm (dens), motifB = makeRhythm (std::min (1.0f, dens + 0.15f));

    // ---- pitches: contour random walk over scale degrees, chord tones on strong beats
    int deg = m.degreeNear (m.noteOf (0) + (r.chance (0.5) ? 0 : 4)); // start on tonic or a 3rd/5th
    const int lo = -2, hi = n + 4;                                    // comfortable range
    std::vector<int> motifDegrees; // first bar's degrees (relative to its chord), reused for repetition
    for (int bar = 0; bar < pat.bars; ++bar)
    {
        const bool last = bar == pat.bars - 1;
        const bool repeatBar = pat.bars >= 2 && (bar % 2 == 0) && bar > 0 && ! last && r.chance (0.7);
        const auto& rhythm = (bar % 2 == 1) ? motifB : motifA;
        std::vector<int> onsets;
        for (int st = 0; st < 16; ++st)
            if (rhythm[(size_t) st]) onsets.push_back (st);
        if (last) // cadence: thin out the end and land on a long tonic
        {
            onsets.erase (std::remove_if (onsets.begin(), onsets.end(), [] (int st) { return st > 8; }), onsets.end());
            if (onsets.empty() || onsets.back() != 8) onsets.push_back (8);
        }
        for (size_t k = 0; k < onsets.size(); ++k)
        {
            const int st = onsets[k];
            const int prevDeg = deg;
            const int step = bar * 16 + st;
            const bool strong = st % 4 == 0;
            if (repeatBar && k < motifDegrees.size())
            {
                // motif repetition, shifted with the chord
                deg = motifDegrees[k] + chordAt (step) - chordAt (0);
            }
            else
            {
                const double leapP = 0.12 + 0.25 * mood;
                int move = r.chance (leapP) ? (r.chance (0.5) ? 2 : 3) + (r.chance (0.3) ? 1 : 0) : 1;
                if (r.chance (0.12)) move = 0;
                int dir = r.chance (0.5) ? 1 : -1;
                if (deg + dir * move > hi || deg + dir * move < lo) dir = -dir;
                deg += dir * move;
                if (strong && ! isChordTone (deg, step))
                {
                    // nudge to nearest chord tone
                    for (int d2 : { 1, -1, 2, -2 })
                        if (isChordTone (deg + d2, step)) { deg += d2; break; }
                }
            }
            deg = juce::jlimit (lo, hi, deg);
            // keep every interval within an octave
            while (deg - prevDeg > n) deg -= n;
            while (prevDeg - deg > n) deg += n;
            if (last && k == onsets.size() - 1)
                deg = std::abs (prevDeg - 0) <= std::abs (prevDeg - n) ? 0 : n; // land on the tonic
            if (bar == 0)
                motifDegrees.push_back (deg);

            MelodyNote nt;
            nt.start = step;
            const int next = k + 1 < onsets.size() ? bar * 16 + onsets[k + 1] : (bar + 1) * 16;
            int len = next - step;
            if (last && k == onsets.size() - 1) len = 16 - st;
            const double stacc = 0.35 + 0.6 * (1.0 - mood);
            nt.length = std::max (1, (int) std::round (len * (len <= 2 ? 1.0 : stacc + (1.0 - stacc) * r.uniform (0.4, 1.0))));
            nt.note = juce::jlimit (24, 108, m.noteOf (deg));
            nt.velocity = (float) juce::jlimit (0.3, 1.0, (strong ? 0.82 : 0.62) + 0.15 * mood + r.uniform (-0.08, 0.08));
            pat.notes.push_back (nt);
        }
    }
    return pat;
}
} // namespace mf
