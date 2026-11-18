#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::Identifier kStateRoot ("MUTAFORM_STATE");
const juce::Identifier kLayers ("LAYERS");
const juce::Identifier kLayer ("LAYER");
const juce::Identifier kEntry ("ENTRY");
const juce::Identifier kVersion ("version");
const juce::Identifier kIndex ("index");
const juce::Identifier kSource ("source");
const juce::Identifier kHistIndex ("historyIndex");
const juce::Identifier kMultipoints ("multipoints");
const juce::Identifier kSeed ("seed");
const juce::Identifier kChaos ("chaos");
const juce::Identifier kMask ("mask");
const juce::Identifier kLabel ("label");
const juce::Identifier kAudio ("audio");
} // namespace

MutaformProcessor::MutaformProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", mf::createParameterLayout())
{
    params.bind (apvts);
    startTimerHz (20);
}

MutaformProcessor::~MutaformProcessor()
{
    stopTimer();
    cancelJobs = true;
    workers.removeAllJobs (true, 10000);
}

void MutaformProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    fx.prepare (sampleRate, std::max (1, samplesPerBlock));
    mixBuffer.setSize (2, std::max (1, samplesPerBlock), false, true, false);
}

void MutaformProcessor::releaseResources()
{
    engine.reset();
    fx.reset();
}

bool MutaformProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

mf::EngineSettings MutaformProcessor::readEngineSettings() const
{
    mf::EngineSettings s;
    for (int i = 0; i < mf::kNumLayers; ++i)
    {
        const auto& p = params.layers[(size_t) i];
        auto& l = s.layers[(size_t) i];
        l.start = p.start->load();
        l.end = p.end->load();
        l.loop = p.loop->load() > 0.5f;
        l.loopStart = p.loopStart->load();
        l.loopEnd = p.loopEnd->load();
        l.xfadeMs = p.xfade->load();
        l.reverse = p.reverse->load() > 0.5f;
        l.mpMode = (mf::MultipointMode) juce::jlimit (0, 3, (int) p.mpMode->load());
        l.mute = p.mute->load() > 0.5f;
        l.solo = p.solo->load() > 0.5f;
        l.gainDb = p.gain->load();
        l.pan = p.pan->load();
        l.pitchSemis = p.semi->load() + p.cents->load() * 0.01f;
    }
    s.attackMs = params.attack->load();
    s.decayMs = params.decay->load();
    s.sustain = params.sustain->load();
    s.releaseMs = params.release->load();
    s.velSens = params.velSens->load();
    return s;
}

mf::FxSettings MutaformProcessor::readFxSettings() const
{
    mf::FxSettings f;
    const auto on = [] (std::atomic<float>* p) { return p->load() > 0.5f; };
    f.fltOn = on (params.fltOn); f.fltType = (int) params.fltType->load(); f.fltCutoff = params.fltCutoff->load();
    f.fltRes = params.fltRes->load(); f.fltMix = params.fltMix->load();
    f.distOn = on (params.distOn); f.distDrive = params.distDrive->load(); f.distMix = params.distMix->load();
    f.choOn = on (params.choOn); f.choRate = params.choRate->load(); f.choDepth = params.choDepth->load(); f.choMix = params.choMix->load();
    f.dlyOn = on (params.dlyOn); f.dlySync = on (params.dlySync); f.dlyTimeMs = params.dlyTime->load();
    f.dlyDiv = (int) params.dlyDiv->load(); f.dlyFeedback = params.dlyFeedback->load(); f.dlyMix = params.dlyMix->load();
    f.revOn = on (params.revOn); f.revSize = params.revSize->load(); f.revDamp = params.revDamp->load();
    f.revWidth = params.revWidth->load(); f.revMix = params.revMix->load();
    f.masterGainDb = params.masterGain->load();
    f.limiterOn = on (params.limiterOn); f.limiterCeilingDb = params.limiterCeiling->load();
    return f;
}

void MutaformProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    if (mixBuffer.getNumSamples() < n)
        return; // host exceeded the announced block size; never allocate here

    juce::AudioBuffer<float> mix (mixBuffer.getArrayOfWritePointers(), 2, n);
    engine.render (mix, midi, readEngineSettings());

    auto fxs = readFxSettings();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                fxs.bpm = *bpm;
    fx.process (mix, n, fxs);

    if (buffer.getNumChannels() >= 2)
    {
        buffer.copyFrom (0, 0, mix, 0, 0, n);
        buffer.copyFrom (1, 0, mix, 1, 0, n);
    }
    else if (buffer.getNumChannels() == 1)
    {
        buffer.copyFromWithRamp (0, 0, mix.getReadPointer (0), n, 0.5f, 0.5f);
        buffer.addFrom (0, 0, mix, 1, 0, n, 0.5f);
    }
}

//==============================================================================
void MutaformProcessor::housekeeping()
{
    engine.collectGarbage();

    std::vector<JobResult> done;
    {
        const juce::ScopedLock sl (resultLock);
        done.swap (finishedJobs);
    }
    for (auto& r : done)
        applyResult (r);
}

bool MutaformProcessor::waitUntilIdle (int timeoutMs)
{
    const auto start = juce::Time::getMillisecondCounter();
    while (anyBusy())
    {
        housekeeping();
        if ((int) (juce::Time::getMillisecondCounter() - start) > timeoutMs)
            return false;
        juce::Thread::sleep (2);
    }
    housekeeping();
    return true;
}

bool MutaformProcessor::anyBusy() const
{
    for (auto& L : layers)
        if (L.busy)
            return true;
    return false;
}

void MutaformProcessor::publishLayer (int layer)
{
    auto& L = layers[(size_t) layer];
    engine.setSample (layer, L.current());
    engine.setMultipoints (layer, L.multipoints);
    sendChangeMessage();
}

juce::uint32 MutaformProcessor::currentProcessMask() const
{
    juce::uint32 mask = 0;
    for (size_t i = 0; i < params.evo.size(); ++i)
        if (params.evo[i]->load() > 0.5f)
            mask |= 1u << i;
    return mask == 0 ? 0xffu : mask;
}

bool MutaformProcessor::evolveLayer (int layer, juce::uint64 seed)
{
    auto& L = layers[(size_t) layer];
    if (L.busy || L.current() == nullptr)
        return false;

    while (seed == 0)
        seed = (juce::uint64) juce::Random::getSystemRandom().nextInt64();

    JobResult job;
    job.layer = layer;
    job.epoch = L.epoch;
    job.seed = seed;
    job.chaos = params.chaos->load() * 0.01f;
    job.mask = currentProcessMask();

    L.busy = true;
    L.lastError.clear();
    ++runningJobs;
    sendChangeMessage();

    auto source = L.current();
    workers.addJob ([this, job, source]() mutable
                    {
                        job.evolved = mf::evolve (*source, job.seed, job.chaos, job.mask, &cancelJobs);
                        {
                            const juce::ScopedLock sl (resultLock);
                            finishedJobs.push_back (std::move (job));
                        }
                        --runningJobs;
                    });
    return true;
}

void MutaformProcessor::applyResult (JobResult& r)
{
    auto& L = layers[(size_t) r.layer];
    if (r.epoch != L.epoch)
        return; // layer changed meanwhile
    L.busy = false;

    if (r.targetIndex >= 0)
    {
        // chain re-render: fill entries ending at targetIndex
        const int n = (int) r.chainAudio.size();
        for (int k = 0; k < n; ++k)
        {
            const int idx = r.targetIndex - n + 1 + k;
            if (juce::isPositiveAndBelow (idx, (int) L.history.size()))
                L.history[(size_t) idx].audio = r.chainAudio[(size_t) k];
        }
        if (juce::isPositiveAndBelow (r.targetIndex, (int) L.history.size()) && L.history[(size_t) r.targetIndex].audio != nullptr)
        {
            L.index = r.targetIndex;
            publishLayer (r.layer);
        }
        else
        {
            L.lastError = "Could not re-render variation";
            sendChangeMessage();
        }
        return;
    }

    if (r.evolved.audio == nullptr)
    {
        L.lastError = "Evolve failed";
        sendChangeMessage();
        return;
    }

    // truncate redo branch, append the new variation
    L.history.resize ((size_t) L.index + 1);
    LayerModel::Entry e;
    e.audio = r.evolved.audio;
    e.seed = r.seed;
    e.chaos = r.chaos;
    e.processMask = r.mask;
    e.label = r.evolved.steps.joinIntoString (" > ");
    L.history.push_back (std::move (e));

    // cap history: keep the original (entry 0), drop the oldest variation
    while ((int) L.history.size() > kMaxHistory)
    {
        L.history.erase (L.history.begin() + 1);
        // entries that can no longer be derived (parent gone, no audio) are dropped too;
        // this only happens with histories restored from a saved state
        while (L.history.size() > 1 && L.history[1].audio == nullptr)
            L.history.erase (L.history.begin() + 1);
        if (L.history.size() > 1)
            L.history[1].root = true; // its parent is gone; it keeps its own audio
    }
    L.index = (int) L.history.size() - 1;
    publishLayer (r.layer);
}

void MutaformProcessor::navigateTo (int layer, int index)
{
    auto& L = layers[(size_t) layer];
    if (L.history[(size_t) index].audio != nullptr)
    {
        L.index = index;
        publishLayer (layer);
        return;
    }

    // find the nearest materialised ancestor and re-render the chain in the background
    int base = index - 1;
    while (base > 0 && L.history[(size_t) base].audio == nullptr)
        --base;
    if (L.history[(size_t) base].audio == nullptr)
        return;

    JobResult job;
    job.layer = layer;
    job.epoch = ++L.epoch;
    job.targetIndex = index;

    struct Step { juce::uint64 seed; float chaos; juce::uint32 mask; };
    std::vector<Step> steps;
    for (int i = base + 1; i <= index; ++i)
    {
        const auto& e = L.history[(size_t) i];
        steps.push_back ({ e.seed, e.chaos, e.processMask });
    }

    L.busy = true;
    ++runningJobs;
    sendChangeMessage();

    auto source = L.history[(size_t) base].audio;
    workers.addJob ([this, job, source, steps]() mutable
                    {
                        mf::SamplePtr cur = source;
                        for (auto& st : steps)
                        {
                            auto res = mf::evolve (*cur, st.seed, st.chaos, st.mask, &cancelJobs);
                            if (res.audio == nullptr) { job.chainAudio.clear(); break; }
                            cur = res.audio;
                            job.chainAudio.push_back (cur);
                        }
                        {
                            const juce::ScopedLock sl (resultLock);
                            finishedJobs.push_back (std::move (job));
                        }
                        --runningJobs;
                    });
}

bool MutaformProcessor::canUndo (int layer) const
{
    const auto& L = layers[(size_t) layer];
    return ! L.busy && L.index > 0;
}

bool MutaformProcessor::canRedo (int layer) const
{
    const auto& L = layers[(size_t) layer];
    return ! L.busy && L.index >= 0 && L.index + 1 < (int) L.history.size();
}

bool MutaformProcessor::undo (int layer)
{
    if (! canUndo (layer))
        return false;
    navigateTo (layer, layers[(size_t) layer].index - 1);
    return true;
}

bool MutaformProcessor::redo (int layer)
{
    if (! canRedo (layer))
        return false;
    navigateTo (layer, layers[(size_t) layer].index + 1);
    return true;
}

bool MutaformProcessor::jumpToHistory (int layer, int index)
{
    auto& L = layers[(size_t) layer];
    if (L.busy || ! juce::isPositiveAndBelow (index, (int) L.history.size()) || index == L.index)
        return false;
    navigateTo (layer, index);
    return true;
}

void MutaformProcessor::setLayerSample (int layer, mf::SamplePtr sample, const juce::String& sourcePath)
{
    auto& L = layers[(size_t) layer];
    L.history.clear();
    LayerModel::Entry e;
    e.audio = std::move (sample);
    e.root = true;
    e.label = "Original";
    L.history.push_back (std::move (e));
    L.index = 0;
    ++L.epoch;
    L.busy = false;
    L.sourcePath = sourcePath;
    publishLayer (layer);
}

bool MutaformProcessor::loadSampleFile (int layer, const juce::File& file, juce::String& error)
{
    auto s = mf::loadSampleFromFile (file, error);
    if (s == nullptr)
        return false;
    setLayerSample (layer, std::move (s), file.getFullPathName());
    return true;
}

bool MutaformProcessor::loadSampleMemory (int layer, const void* data, size_t size, const juce::String& name, juce::String& error)
{
    auto s = mf::loadSampleFromMemory (data, size, name, error);
    if (s == nullptr)
        return false;
    setLayerSample (layer, std::move (s), name);
    return true;
}

void MutaformProcessor::clearLayer (int layer)
{
    layers[(size_t) layer].clear();
    publishLayer (layer);
}

void MutaformProcessor::setMultipoints (int layer, std::vector<float> points)
{
    std::sort (points.begin(), points.end());
    if (points.size() > (size_t) mf::kMaxMultipoints)
        points.resize ((size_t) mf::kMaxMultipoints);
    layers[(size_t) layer].multipoints = std::move (points);
    engine.setMultipoints (layer, layers[(size_t) layer].multipoints);
    sendChangeMessage();
}

//==============================================================================
juce::AudioProcessorEditor* MutaformProcessor::createEditor()
{
    return new MutaformEditor (*this);
}

void MutaformProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root (kStateRoot);
    root.setProperty (kVersion, 1, nullptr);
    root.appendChild (apvts.copyState(), nullptr);

    juce::ValueTree layersTree (kLayers);
    for (int i = 0; i < mf::kNumLayers; ++i)
    {
        const auto& L = layers[(size_t) i];
        juce::ValueTree lt (kLayer);
        lt.setProperty (kIndex, i, nullptr);
        lt.setProperty (kSource, L.sourcePath, nullptr);
        lt.setProperty (kHistIndex, L.index, nullptr);
        juce::StringArray mp;
        for (auto p : L.multipoints)
            mp.add (juce::String (p, 6));
        lt.setProperty (kMultipoints, mp.joinIntoString (","), nullptr);

        for (int e = 0; e < (int) L.history.size(); ++e)
        {
            const auto& en = L.history[(size_t) e];
            juce::ValueTree et (kEntry);
            et.setProperty (kSeed, juce::String::toHexString ((juce::int64) en.seed), nullptr);
            et.setProperty (kChaos, en.chaos, nullptr);
            et.setProperty (kMask, (int) en.processMask, nullptr);
            et.setProperty (kLabel, en.label, nullptr);
            // audio is stored for the original and the current variation; the rest is
            // re-rendered deterministically from (seed, chaos, mask) when navigated to
            et.setProperty ("root", en.root, nullptr);
            if ((e == 0 || e == L.index || en.root) && en.audio != nullptr)
                et.setProperty (kAudio, mf::encodeSample (*en.audio), nullptr);
            lt.appendChild (et, nullptr);
        }
        layersTree.appendChild (lt, nullptr);
    }
    root.appendChild (layersTree, nullptr);

    juce::MemoryOutputStream mos (destData, false);
    root.writeToStream (mos);
}

void MutaformProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto root = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (! root.isValid())
        return;

    // accept both the full state and a bare APVTS tree
    auto paramTree = root.hasType (kStateRoot) ? root.getChildWithName (apvts.state.getType()) : root;
    if (paramTree.isValid() && paramTree.hasType (apvts.state.getType()))
        apvts.replaceState (paramTree);

    if (! root.hasType (kStateRoot))
        return;

    auto layersTree = root.getChildWithName (kLayers);
    for (int i = 0; i < mf::kNumLayers; ++i)
        layers[(size_t) i].clear();

    for (auto lt : layersTree)
    {
        const int i = (int) lt.getProperty (kIndex, -1);
        if (! juce::isPositiveAndBelow (i, mf::kNumLayers))
            continue;
        auto& L = layers[(size_t) i];
        L.sourcePath = lt.getProperty (kSource).toString();
        for (auto et : lt)
        {
            LayerModel::Entry en;
            en.seed = (juce::uint64) et.getProperty (kSeed).toString().getHexValue64();
            en.chaos = (float) et.getProperty (kChaos, 0.0f);
            en.processMask = (juce::uint32) (int) et.getProperty (kMask, 0xff);
            en.label = et.getProperty (kLabel).toString();
            en.root = (bool) et.getProperty ("root", false);
            if (auto* mb = et.getProperty (kAudio).getBinaryData())
                en.audio = mf::decodeSample (*mb);
            L.history.push_back (std::move (en));
        }
        L.index = juce::jlimit (-1, (int) L.history.size() - 1, (int) lt.getProperty (kHistIndex, 0));
        if (L.history.empty() || L.history[0].audio == nullptr)
        {
            L.clear();
        }
        else if (L.current() == nullptr)
        {
            L.index = 0; // current variation missing: fall back to original
        }
        if (! L.history.empty())
            L.history[0].root = true;
        juce::StringArray mp;
        mp.addTokens (lt.getProperty (kMultipoints).toString(), ",", {});
        for (auto& t : mp)
            if (t.isNotEmpty())
                L.multipoints.push_back (t.getFloatValue());
    }

    for (int i = 0; i < mf::kNumLayers; ++i)
        publishLayer (i);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MutaformProcessor();
}
