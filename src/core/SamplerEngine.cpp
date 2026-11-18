#include "SamplerEngine.h"
#include "Interp.h"
#include <cmath>

namespace mf
{
SamplerEngine::SamplerEngine()
{
    for (auto& p : playheads)
        p.store (-1.0f);
    for (auto& n : numMultipoints)
        n.store (0);
}

void SamplerEngine::prepare (double sampleRate, int)
{
    hostRate = sampleRate > 0 ? sampleRate : 44100.0;
    for (auto& v : voices)
        v.adsr.setSampleRate (hostRate);
    reset();
}

void SamplerEngine::reset()
{
    for (auto& v : voices)
    {
        v.active = false;
        v.adsr.reset();
        v.stealFade = 0;
        v.pendingNote = -1;
        for (auto& l : v.layers)
            l.active = false;
    }
    sustainPedal = false;
    pitchBend = 0.0f;
    lastNote = -1;
    activeVoices.store (0);
}

void SamplerEngine::setMultipoints (int layer, const std::vector<float>& pts)
{
    const int n = std::min ((int) pts.size(), kMaxMultipoints);
    for (int i = 0; i < n; ++i)
        multipoints[(size_t) layer][(size_t) i].store (juce::jlimit (0.0f, 1.0f, pts[(size_t) i]));
    numMultipoints[(size_t) layer].store (n);
}

void SamplerEngine::queueNote (int note, float velocity)
{
    const auto scope = noteFifo.write (1);
    if (scope.blockSize1 > 0)
        noteQueue[(size_t) scope.startIndex1] = { juce::jlimit (0, 127, note), juce::jlimit (0.0f, 1.0f, velocity) };
}

void SamplerEngine::collectGarbage()
{
    for (auto& s : slots)
        s.collectGarbage();
}

void SamplerEngine::allNotesOff (bool immediately)
{
    for (auto& v : voices)
    {
        if (! v.active)
            continue;
        v.keyDown = false;
        v.sustained = false;
        v.pendingNote = -1;
        if (immediately)
        {
            v.active = false;
            v.adsr.reset();
        }
        else
        {
            v.adsr.noteOff();
        }
    }
}

//==============================================================================
void SamplerEngine::updateRegions (const EngineSettings& s)
{
    bool anySolo = false;
    for (auto& l : s.layers)
        anySolo = anySolo || l.solo;

    for (int i = 0; i < kNumLayers; ++i)
    {
        auto& r = regions[(size_t) i];
        const auto& ls = s.layers[(size_t) i];
        r.sample = slots[(size_t) i].acquire();
        r.generation = slots[(size_t) i].getGeneration();
        r.audible = r.sample != nullptr && r.sample->length() > 1 && ! ls.mute && (! anySolo || ls.solo);

        if (r.sample == nullptr)
        {
            r.len = 0;
            continue;
        }

        r.len = r.sample->length();
        const double len = (double) r.len;
        double a = juce::jlimit (0.0, 1.0, (double) ls.start) * len;
        double b = juce::jlimit (0.0, 1.0, (double) ls.end) * len;
        if (b < a)
            std::swap (a, b);
        const double minLen = std::min (len, 32.0);
        if (b - a < minLen)
        {
            b = std::min (len, a + minLen);
            a = b - minLen;
        }
        r.start = a;
        r.end = b;
        r.length = b - a;
        r.reverse = ls.reverse;

        double la = juce::jlimit (a, b, (double) ls.loopStart * len);
        double lb = juce::jlimit (a, b, (double) ls.loopEnd * len);
        if (lb < la)
            std::swap (la, lb);
        r.loop = ls.loop && (lb - la) >= 32.0;
        if (r.reverse)
        {
            r.vLoopStart = b - lb;
            r.vLoopEnd = b - la;
        }
        else
        {
            r.vLoopStart = la - a;
            r.vLoopEnd = lb - a;
        }
        const double loopLen = r.vLoopEnd - r.vLoopStart;
        const double xfSamples = (double) ls.xfadeMs * 0.001 * r.sample->sampleRate;
        r.xfade = std::max (0.0, std::min ({ xfSamples, r.vLoopStart, loopLen * 0.5 }));

        r.rateRatio = r.sample->sampleRate / hostRate;
        r.pitchSemis = ls.pitchSemis;
        r.mpMode = ls.mpMode;

        const float g = juce::Decibels::decibelsToGain (ls.gainDb, -60.0f);
        const float pan = juce::jlimit (-1.0f, 1.0f, ls.pan);
        r.gainL = g * std::min (1.0f, 1.0f - pan);
        r.gainR = g * std::min (1.0f, 1.0f + pan);
        if (! r.audible)
            r.gainL = r.gainR = 0.0f;
    }
}

double SamplerEngine::startPositionFor (int layer, int note, float velocity)
{
    const auto& r = regions[(size_t) layer];
    const int n = numMultipoints[(size_t) layer].load (std::memory_order_relaxed);
    if (r.mpMode == MultipointMode::off || n <= 0)
        return 0.0;

    int idx = 0;
    switch (r.mpMode)
    {
        case MultipointMode::key:      idx = ((note - kRootNote) % n + n) % n; break;
        case MultipointMode::velocity: idx = juce::jlimit (0, n - 1, (int) (velocity * (float) n)); break;
        case MultipointMode::cycle:    idx = cycleCounter[(size_t) layer]++ % n; break;
        case MultipointMode::off:      break;
    }
    const double pt = (double) multipoints[(size_t) layer][(size_t) idx].load (std::memory_order_relaxed) * r.len;
    const double v = r.reverse ? r.end - pt : pt - r.start;
    return juce::jlimit (0.0, std::max (0.0, r.length - 1.0), v);
}

void SamplerEngine::startVoice (Voice& v, int note, float velocity, const EngineSettings& s)
{
    v.active = true;
    v.note = note;
    v.velocity = velocity;
    const float vs = juce::jlimit (0.0f, 1.0f, s.velSens);
    v.velGain = (1.0f - vs) + vs * velocity * velocity;
    v.keyDown = true;
    v.sustained = false;
    v.age = ++ageCounter;
    v.stealFade = 0;
    v.pendingNote = -1;
    v.glideSemis = (s.glideMs > 0.5f && lastNote >= 0) ? (double) (lastNote - note) : 0.0;
    v.vibPhase = 0.0;
    v.samplesPlayed = 0;
    lastNote = note;
    v.adsr.setParameters (adsrParams);
    v.adsr.reset();
    v.adsr.noteOn();
    for (int i = 0; i < kNumLayers; ++i)
    {
        auto& lp = v.layers[(size_t) i];
        const auto& r = regions[(size_t) i];
        lp.active = r.sample != nullptr && r.len > 1;
        lp.pos = lp.active ? startPositionFor (i, note, velocity) : 0.0;
        lp.generation = r.generation;
        lp.fadeIn = 0;
    }
}

void SamplerEngine::noteOn (int note, float velocity, const EngineSettings& s)
{
    // retrigger of the same held note: steal that voice
    Voice* target = nullptr;
    for (auto& v : voices)
        if (! v.active) { target = &v; break; }

    if (target == nullptr)
    {
        // prefer the oldest releasing voice, else the oldest voice
        Voice* oldestReleasing = nullptr;
        Voice* oldest = nullptr;
        for (auto& v : voices)
        {
            if (! v.keyDown && ! v.sustained && (oldestReleasing == nullptr || v.age < oldestReleasing->age))
                oldestReleasing = &v;
            if (oldest == nullptr || v.age < oldest->age)
                oldest = &v;
        }
        target = oldestReleasing != nullptr ? oldestReleasing : oldest;
        // fade out quickly, then start the new note
        target->stealFade = kStealFadeSamples;
        target->pendingNote = note;
        target->pendingVelocity = velocity;
        target->keyDown = true;
        target->age = ++ageCounter;
        return;
    }
    startVoice (*target, note, velocity, s);
}

void SamplerEngine::noteOff (int note)
{
    for (auto& v : voices)
    {
        if (! v.active)
            continue;
        if (v.pendingNote == note)
        {
            v.pendingNote = -1; // released before it started
            v.keyDown = false;
            continue;
        }
        if (v.note == note && v.keyDown && v.pendingNote < 0)
        {
            v.keyDown = false;
            if (sustainPedal)
                v.sustained = true;
            else
                v.adsr.noteOff();
        }
    }
}

void SamplerEngine::handleMidi (const juce::MidiMessage& m, const EngineSettings& s)
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity(), s);
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isSustainPedalOn())
        sustainPedal = true;
    else if (m.isSustainPedalOff())
    {
        sustainPedal = false;
        for (auto& v : voices)
            if (v.active && v.sustained)
            {
                v.sustained = false;
                v.adsr.noteOff();
            }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        allNotesOff (m.isAllSoundOff());
    else if (m.isPitchWheel())
        pitchBend = juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
}

//==============================================================================
void SamplerEngine::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const EngineSettings& s)
{
    out.clear();
    updateRegions (s);

    adsrParams.attack = std::max (0.0005f, s.attackMs * 0.001f);
    adsrParams.decay = std::max (0.001f, s.decayMs * 0.001f);
    adsrParams.sustain = juce::jlimit (0.0f, 1.0f, s.sustain);
    adsrParams.release = std::max (0.002f, s.releaseMs * 0.001f);
    for (auto& v : voices)
        if (v.active)
            v.adsr.setParameters (adsrParams);

    {
        const auto scope = noteFifo.read (noteFifo.getNumReady());
        auto handleQueued = [this, &s] (int start, int size)
        {
            for (int i = 0; i < size; ++i)
            {
                const auto& q = noteQueue[(size_t) (start + i)];
                if (q.velocity > 0.0f) noteOn (q.note, q.velocity, s);
                else                   noteOff (q.note);
            }
        };
        handleQueued (scope.startIndex1, scope.blockSize1);
        handleQueued (scope.startIndex2, scope.blockSize2);
    }

    const int numSamples = out.getNumSamples();
    int pos = 0;
    for (const auto meta : midi)
    {
        const int evPos = juce::jlimit (0, numSamples, meta.samplePosition);
        if (evPos > pos)
        {
            renderVoices (out, pos, evPos - pos, s);
            pos = evPos;
        }
        handleMidi (meta.getMessage(), s);
    }
    if (pos < numSamples)
        renderVoices (out, pos, numSamples - pos, s);

    // smoothed gains reach their targets at block end
    for (auto& r : regions)
    {
        r.curL = r.gainL;
        r.curR = r.gainR;
    }

    // UI info
    int count = 0;
    std::array<uint64_t, kNumLayers> newestAge {};
    std::array<float, kNumLayers> ph {};
    ph.fill (-1.0f);
    for (auto& v : voices)
    {
        if (! v.active)
            continue;
        ++count;
        for (int i = 0; i < kNumLayers; ++i)
        {
            const auto& lp = v.layers[(size_t) i];
            const auto& r = regions[(size_t) i];
            if (lp.active && r.len > 0 && v.age >= newestAge[(size_t) i])
            {
                newestAge[(size_t) i] = v.age;
                const double x = r.reverse ? r.end - lp.pos : r.start + lp.pos;
                ph[(size_t) i] = (float) (x / (double) r.len);
            }
        }
    }
    activeVoices.store (count, std::memory_order_relaxed);
    for (int i = 0; i < kNumLayers; ++i)
        playheads[(size_t) i].store (ph[(size_t) i], std::memory_order_relaxed);
}

void SamplerEngine::renderVoices (juce::AudioBuffer<float>& out, int start, int num, const EngineSettings& s)
{
    auto* outL = out.getWritePointer (0);
    auto* outR = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    const int blockLen = out.getNumSamples();

    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        // per-layer increments, refreshed every 32 samples (glide + vibrato)
        std::array<double, kNumLayers> inc {};
        const double glideDecay = s.glideMs > 0.5f ? std::exp (-32.0 / (s.glideMs * 0.001 * hostRate / 3.0)) : 0.0;
        const double vibInc = 2.0 * juce::MathConstants<double>::pi * 5.6 / hostRate * 32.0;
        auto computeInc = [&]
        {
            const double onset = juce::jlimit (0.0, 1.0, (v.samplesPlayed - 0.15 * hostRate) / (0.35 * hostRate));
            const double vib = (double) s.vibrato * 0.6 * onset * std::sin (v.vibPhase);
            for (int i = 0; i < kNumLayers; ++i)
            {
                const auto& r = regions[(size_t) i];
                const double semis = (double) (v.note - kRootNote) + r.pitchSemis + (double) (pitchBend * s.pitchBendRange)
                                     + v.glideSemis + vib;
                inc[(size_t) i] = juce::jlimit (1.0e-4, 64.0, r.rateRatio * std::exp2 (semis / 12.0));
            }
        };
        computeInc();

        for (int n = 0; n < num; ++n)
        {
            const int idx = start + n;
            float stealGain = 1.0f;
            if (v.stealFade > 0)
            {
                stealGain = (float) v.stealFade / (float) kStealFadeSamples;
                if (--v.stealFade == 0)
                {
                    if (v.pendingNote >= 0)
                    {
                        startVoice (v, v.pendingNote, v.pendingVelocity, s);
                        computeInc();
                    }
                    else
                    {
                        v.active = false;
                        break;
                    }
                }
            }

            if ((++v.samplesPlayed & 31) == 0)
            {
                v.glideSemis = std::abs (v.glideSemis) < 1.0e-3 ? 0.0 : v.glideSemis * glideDecay;
                v.vibPhase += vibInc;
                if (v.vibPhase > 2.0 * juce::MathConstants<double>::pi)
                    v.vibPhase -= 2.0 * juce::MathConstants<double>::pi;
                computeInc();
            }

            const float env = v.adsr.getNextSample() * v.velGain * stealGain;
            float sumL = 0.0f, sumR = 0.0f;
            bool anyLayer = false;

            for (int i = 0; i < kNumLayers; ++i)
            {
                auto& lp = v.layers[(size_t) i];
                if (! lp.active)
                    continue;
                const auto& r = regions[(size_t) i];
                if (r.sample == nullptr || r.len <= 1)
                {
                    lp.active = false;
                    continue;
                }
                if (lp.generation != r.generation)
                {
                    // sample swapped underneath us: keep position, clamp, short fade-in
                    lp.generation = r.generation;
                    lp.pos = juce::jlimit (0.0, std::max (0.0, r.length - 1.0), lp.pos);
                    lp.fadeIn = kSwapFadeSamples;
                }

                if (! r.loop && lp.pos >= r.length)
                {
                    lp.active = false;
                    continue;
                }
                anyLayer = true;

                const auto* dl = r.sample->audio.getReadPointer (0);
                const auto* dr = r.sample->audio.getReadPointer (1);
                auto toSample = [&r] (double p) { return r.reverse ? (r.end - 1.0 - p) : (r.start + p); };

                const double x = toSample (lp.pos);
                float sl = readHermite (dl, r.len, x);
                float sr = readHermite (dr, r.len, x);

                if (r.loop && r.xfade > 0.0 && lp.pos > r.vLoopEnd - r.xfade)
                {
                    const float t = (float) juce::jlimit (0.0, 1.0, (lp.pos - (r.vLoopEnd - r.xfade)) / r.xfade);
                    const double x2 = toSample (lp.pos - (r.vLoopEnd - r.vLoopStart));
                    const float g1 = std::sqrt (1.0f - t), g2 = std::sqrt (t);
                    sl = sl * g1 + readHermite (dl, r.len, x2) * g2;
                    sr = sr * g1 + readHermite (dr, r.len, x2) * g2;
                }

                float fade = 1.0f;
                if (lp.fadeIn > 0)
                {
                    fade = 1.0f - (float) lp.fadeIn / (float) kSwapFadeSamples;
                    --lp.fadeIn;
                }

                // smoothed layer gain across the whole block
                const float a = (float) idx / (float) std::max (1, blockLen);
                const float gl = r.curL + (r.gainL - r.curL) * a;
                const float gr = r.curR + (r.gainR - r.curR) * a;
                sumL += sl * gl * fade;
                sumR += sr * gr * fade;

                lp.pos += inc[(size_t) i];
                if (r.loop && lp.pos >= r.vLoopEnd)
                    lp.pos -= (r.vLoopEnd - r.vLoopStart);
            }

            if (outR != nullptr)
            {
                outL[idx] += sumL * env;
                outR[idx] += sumR * env;
            }
            else
            {
                outL[idx] += 0.5f * (sumL + sumR) * env;
            }

            if ((! anyLayer && v.stealFade == 0) || (! v.adsr.isActive() && v.stealFade == 0))
            {
                v.active = false;
                v.adsr.reset();
                break;
            }
        }
    }
}
} // namespace mf
