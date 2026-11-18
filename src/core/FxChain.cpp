#include "FxChain.h"
#include "Params.h"
#include <cmath>

namespace mf
{
static constexpr double kMaxDelaySeconds = 2.6;

void FxChain::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    maxBlock = std::max (1, maxBlockSize);
    wet.setSize (2, maxBlock, false, true, false);

    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
    filter.prepare (spec);
    chorus.prepare (spec);
    reverb.prepare (spec);

    delayLine.setSize (2, (int) (kMaxDelaySeconds * sr) + 8, false, true, false);

    for (auto* b : { &fltBlend, &distBlend, &choBlend, &dlyBlend, &revBlend })
        b->amount.reset (sr, 0.03);
    cutoff.reset (sr, 0.03);
    drive.reset (sr, 0.03);
    delaySamples.reset (sr, 0.12);
    master.reset (sr, 0.02);
    limRelease = (float) std::exp (-1.0 / (0.08 * sr));
    reset();
}

void FxChain::reset()
{
    filter.reset();
    chorus.reset();
    reverb.reset();
    delayLine.clear();
    delayWrite = 0;
    delayDamp[0] = delayDamp[1] = 0.0f;
    delayWasIdle = true;
    limGain = 1.0f;
    for (auto* b : { &fltBlend, &distBlend, &choBlend, &dlyBlend, &revBlend })
        b->amount.setCurrentAndTargetValue (0.0f);
    cutoff.setCurrentAndTargetValue (8000.0f);
    drive.setCurrentAndTargetValue (1.0f);
    delaySamples.setCurrentAndTargetValue ((float) (0.35 * sr));
    master.setCurrentAndTargetValue (1.0f);
    lastFltType = -1;
}

double FxChain::delayTimeMs (const FxSettings& s)
{
    if (s.dlySync)
    {
        const double bpm = juce::jlimit (20.0, 400.0, s.bpm > 0 ? s.bpm : 120.0);
        const int d = juce::jlimit (0, (int) kDelayDivisionsQuarters.size() - 1, s.dlyDiv);
        return 60000.0 / bpm * (double) kDelayDivisionsQuarters[(size_t) d];
    }
    return (double) s.dlyTimeMs;
}

void FxChain::blendInto (juce::AudioBuffer<float>& io, const juce::AudioBuffer<float>& w, int n, Blend& b)
{
    for (int i = 0; i < n; ++i)
    {
        const float a = b.amount.getNextValue();
        for (int c = 0; c < 2; ++c)
        {
            auto* d = io.getWritePointer (c);
            d[i] += (w.getSample (c, i) - d[i]) * a;
        }
    }
}

void FxChain::process (juce::AudioBuffer<float>& io, int n, const FxSettings& s)
{
    if (n > maxBlock || io.getNumChannels() < 2)
        return;

    // ---------------- filter
    fltBlend.amount.setTargetValue (s.fltOn ? juce::jlimit (0.0f, 1.0f, s.fltMix) : 0.0f);
    if (! fltBlend.idle())
    {
        if (s.fltType != lastFltType)
        {
            using T = juce::dsp::StateVariableTPTFilterType;
            filter.setType (s.fltType == 1 ? T::highpass : (s.fltType == 2 ? T::bandpass : T::lowpass));
            lastFltType = s.fltType;
        }
        filter.setResonance (0.5f + 11.5f * juce::jlimit (0.0f, 1.0f, s.fltRes) * juce::jlimit (0.0f, 1.0f, s.fltRes));
        cutoff.setTargetValue (juce::jlimit (20.0f, (float) (sr * 0.45), s.fltCutoff));
        for (int i = 0; i < n; ++i)
        {
            if ((i & 15) == 0)
                filter.setCutoffFrequency (cutoff.getCurrentValue());
            cutoff.getNextValue();
            for (int c = 0; c < 2; ++c)
                wet.setSample (c, i, filter.processSample (c, io.getSample (c, i)));
        }
        blendInto (io, wet, n, fltBlend);
    }
    else
    {
        cutoff.setCurrentAndTargetValue (juce::jlimit (20.0f, (float) (sr * 0.45), s.fltCutoff));
    }

    // ---------------- distortion
    distBlend.amount.setTargetValue (s.distOn ? juce::jlimit (0.0f, 1.0f, s.distMix) : 0.0f);
    drive.setTargetValue (std::pow (40.0f, juce::jlimit (0.0f, 1.0f, s.distDrive)));
    if (! distBlend.idle())
    {
        for (int i = 0; i < n; ++i)
        {
            const float g = drive.getNextValue();
            const float comp = 1.0f / std::sqrt (g);
            for (int c = 0; c < 2; ++c)
            {
                const float x = io.getSample (c, i) * g;
                // asymmetric soft clipper with DC-balanced bias
                const float y = std::tanh (x + 0.15f) - std::tanh (0.15f);
                wet.setSample (c, i, y * std::max (comp, 0.35f));
            }
        }
        blendInto (io, wet, n, distBlend);
    }
    else
    {
        drive.setCurrentAndTargetValue (drive.getTargetValue());
    }

    // ---------------- chorus
    choBlend.amount.setTargetValue (s.choOn ? juce::jlimit (0.0f, 1.0f, s.choMix) : 0.0f);
    if (! choBlend.idle())
    {
        chorus.setRate (juce::jlimit (0.01f, 20.0f, s.choRate));
        chorus.setDepth (juce::jlimit (0.0f, 1.0f, s.choDepth));
        chorus.setCentreDelay (8.0f);
        chorus.setFeedback (0.15f);
        chorus.setMix (1.0f);
        for (int c = 0; c < 2; ++c)
            wet.copyFrom (c, 0, io, c, 0, n);
        juce::dsp::AudioBlock<float> block (wet.getArrayOfWritePointers(), 2, (size_t) n);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
        blendInto (io, wet, n, choBlend);
    }

    // ---------------- delay (stereo, slight ping-pong cross-feed, damped feedback)
    dlyBlend.amount.setTargetValue (s.dlyOn ? juce::jlimit (0.0f, 1.0f, s.dlyMix) : 0.0f);
    const float targetDelay = (float) juce::jlimit (1.0, kMaxDelaySeconds * sr - 4.0, delayTimeMs (s) * 0.001 * sr);
    if (! dlyBlend.idle())
    {
        if (delayWasIdle)
        {
            delayLine.clear(); // no stale echoes when the delay is switched back on
            delayDamp[0] = delayDamp[1] = 0.0f;
            delayWasIdle = false;
        }
        delaySamples.setTargetValue (targetDelay);
        const int len = delayLine.getNumSamples();
        const float fb = juce::jlimit (0.0f, 0.95f, s.dlyFeedback);
        const float damp = 0.35f;
        for (int i = 0; i < n; ++i)
        {
            const float d = delaySamples.getNextValue();
            float readPos = (float) delayWrite - d;
            while (readPos < 0.0f) readPos += (float) len;
            const int i0 = (int) readPos;
            const int i1 = (i0 + 1) % len;
            const float t = readPos - (float) i0;
            float out[2];
            for (int c = 0; c < 2; ++c)
            {
                const auto* dl = delayLine.getReadPointer (c);
                out[c] = dl[i0] + (dl[i1] - dl[i0]) * t;
                delayDamp[c] += (out[c] - delayDamp[c]) * (1.0f - damp);
            }
            for (int c = 0; c < 2; ++c)
            {
                const float cross = 0.3f * delayDamp[1 - c] + 0.7f * delayDamp[c];
                float v = io.getSample (c, i) + cross * fb;
                if (std::abs (v) < 1.0e-15f) v = 0.0f;
                delayLine.setSample (c, delayWrite, v);
                wet.setSample (c, i, io.getSample (c, i) + out[c]);
            }
            delayWrite = (delayWrite + 1) % len;
        }
        blendInto (io, wet, n, dlyBlend);
    }
    else
    {
        delaySamples.setCurrentAndTargetValue (targetDelay);
        delayWasIdle = true;
    }

    // ---------------- reverb
    revBlend.amount.setTargetValue (s.revOn ? juce::jlimit (0.0f, 1.0f, s.revMix) : 0.0f);
    if (! revBlend.idle())
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize = juce::jlimit (0.0f, 1.0f, s.revSize);
        p.damping = juce::jlimit (0.0f, 1.0f, s.revDamp);
        p.width = juce::jlimit (0.0f, 1.0f, s.revWidth);
        p.wetLevel = 1.0f;
        p.dryLevel = 1.0f;
        reverb.setParameters (p);
        for (int c = 0; c < 2; ++c)
            wet.copyFrom (c, 0, io, c, 0, n);
        juce::dsp::AudioBlock<float> block (wet.getArrayOfWritePointers(), 2, (size_t) n);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (block));
        blendInto (io, wet, n, revBlend);
    }

    // ---------------- master + limiter
    master.setTargetValue (juce::Decibels::decibelsToGain (s.masterGainDb, -100.0f));
    const float ceiling = juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 0.0f, s.limiterCeilingDb));
    float minGain = 1.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = master.getNextValue();
        float l = io.getSample (0, i) * g;
        float r = io.getSample (1, i) * g;
        if (! std::isfinite (l)) l = 0.0f;
        if (! std::isfinite (r)) r = 0.0f;
        if (s.limiterOn)
        {
            const float pk = std::max (std::abs (l), std::abs (r));
            const float need = pk > ceiling ? ceiling / pk : 1.0f;
            limGain = need < limGain ? need : need + (limGain - need) * limRelease;
            l = juce::jlimit (-ceiling, ceiling, l * limGain);
            r = juce::jlimit (-ceiling, ceiling, r * limGain);
            minGain = std::min (minGain, limGain);
        }
        io.setSample (0, i, l);
        io.setSample (1, i, r);
    }
    limiterGainReport.store (minGain, std::memory_order_relaxed);
}
} // namespace mf
