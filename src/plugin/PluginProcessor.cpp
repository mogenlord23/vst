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
    s.glideMs = params.glide->load();
    s.vibrato = params.vibrato->load();
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
    ++L.revision;
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

bool MutaformProcessor::evolveLayer (int layer, juce::uint64 seed, int steps, float chaosOverride)
{
    auto& L = layers[(size_t) layer];
    if (L.busy || L.current() == nullptr)
        return false;

    while (seed == 0)
        seed = (juce::uint64) juce::Random::getSystemRandom().nextInt64();

    JobResult job;
    job.layer = layer;
    job.epoch = L.epoch;
    job.chaos = chaosOverride >= 0.0f ? juce::jlimit (0.0f, 1.0f, chaosOverride) : params.chaos->load() * 0.01f;
    job.mask = currentProcessMask();
    mf::Rng seeder (seed);
    for (int i = 0; i < juce::jlimit (1, 8, steps); ++i)
    {
        juce::uint64 s = i == 0 ? seed : 0;
        while (s == 0)
            s = seeder.next();
        job.seeds.push_back (s);
    }

    L.busy = true;
    L.lastError.clear();
    ++runningJobs;
    sendChangeMessage();

    auto source = L.current();
    workers.addJob ([this, job, source]() mutable
                    {
                        mf::SamplePtr cur = source;
                        for (auto s : job.seeds)
                        {
                            auto res = mf::evolve (*cur, s, job.chaos, job.mask, &cancelJobs);
                            if (res.audio == nullptr) { job.failed = job.evolved.empty(); break; }
                            cur = res.audio;
                            job.features.push_back (mf::analyseSound (*res.audio));
                            job.evolved.push_back (std::move (res));
                        }
                        {
                            const juce::ScopedLock sl (resultLock);
                            finishedJobs.push_back (std::move (job));
                        }
                        --runningJobs;
                    });
    return true;
}

int MutaformProcessor::evolveGroup (juce::uint32 layerMask, int steps)
{
    if (groupPending != 0)
        return 0;
    int started = 0;
    for (int i = 0; i < mf::kNumLayers; ++i)
    {
        const auto& L = layers[(size_t) i];
        if ((layerMask & (1u << i)) == 0 || L.locked || ! L.hasSample() || L.busy)
            continue;
        if (evolveLayer (i, 0, steps))
        {
            groupPending |= 1u << i;
            ++started;
        }
    }
    return started;
}

void MutaformProcessor::finishGroupMember (int layer)
{
    if ((groupPending & (1u << layer)) == 0)
        return;
    groupPending &= ~(1u << layer);
    if (groupPending == 0)
        pushGeneration();
}

void MutaformProcessor::pushGeneration()
{
    Generation g;
    for (int i = 0; i < mf::kNumLayers; ++i)
        g.idx[(size_t) i] = layers[(size_t) i].index;
    if (generationIndex >= 0 && generationIndex < (int) generations.size() && generations[(size_t) generationIndex].idx == g.idx)
        return;
    generations.resize ((size_t) std::max (0, generationIndex + 1));
    generations.push_back (g);
    while (generations.size() > 64)
        generations.erase (generations.begin());
    generationIndex = (int) generations.size() - 1;
}

void MutaformProcessor::resetGenerations()
{
    generations.clear();
    generationIndex = -1;
    groupPending = 0;
    pushGeneration();
}

bool MutaformProcessor::canStepGeneration (int delta) const
{
    const int t = generationIndex + delta;
    return groupPending == 0 && ! anyBusy() && t >= 0 && t < (int) generations.size();
}

bool MutaformProcessor::stepGeneration (int delta)
{
    if (! canStepGeneration (delta))
        return false;
    generationIndex += delta;
    const auto g = generations[(size_t) generationIndex];
    for (int i = 0; i < mf::kNumLayers; ++i)
    {
        const int idx = g.idx[(size_t) i];
        auto& L = layers[(size_t) i];
        if (idx >= 0 && idx < (int) L.history.size() && idx != L.index)
            navigateTo (i, idx);
    }
    sendChangeMessage();
    return true;
}

int MutaformProcessor::getLevel (int layer) const
{
    const auto& L = layers[(size_t) layer];
    int lvl = 0;
    for (int k = 1; k <= L.index && k < (int) L.history.size(); ++k)
        lvl += 1 + juce::roundToInt (L.history[(size_t) k].chaos * 9.0f);
    return lvl;
}

int MutaformProcessor::getLevel() const
{
    int total = 0;
    for (int i = 0; i < mf::kNumLayers; ++i)
        total += getLevel (i);
    return total;
}

void MutaformProcessor::setLocked (int layer, bool locked)
{
    layers[(size_t) layer].locked = locked;
    sendChangeMessage();
}

void MutaformProcessor::toggleFavorite (int layer, int index)
{
    auto& L = layers[(size_t) layer];
    if (juce::isPositiveAndBelow (index, (int) L.history.size()))
        L.history[(size_t) index].favorite = ! L.history[(size_t) index].favorite;
    sendChangeMessage();
}

void MutaformProcessor::rollDice()
{
    DiceUndo u;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            u.params[rp->getParameterID()] = rp->convertFrom0to1 (rp->getValue());
    for (int i = 0; i < mf::kNumLayers; ++i)
        u.idx[(size_t) i] = layers[(size_t) i].index;
    diceUndo = u;

    randomizeParameters();
    evolveGroup (0x1f, juce::Random::getSystemRandom().nextInt (juce::Range<int> (1, 4)));
    sendChangeMessage();
}

bool MutaformProcessor::undoDice()
{
    if (! diceUndo.has_value() || anyBusy())
        return false;
    const auto u = *diceUndo;
    diceUndo.reset();
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (auto it = u.params.find (rp->getParameterID()); it != u.params.end())
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->convertTo0to1 (it->second));
                rp->endChangeGesture();
            }
    for (int i = 0; i < mf::kNumLayers; ++i)
    {
        auto& L = layers[(size_t) i];
        const int idx = u.idx[(size_t) i];
        if (idx >= 0 && idx < (int) L.history.size() && idx != L.index)
            navigateTo (i, idx);
    }
    pushGeneration();
    sendChangeMessage();
    return true;
}

void MutaformProcessor::applyResult (JobResult& r)
{
    auto& L = layers[(size_t) r.layer];
    if (r.epoch != L.epoch)
    {
        finishGroupMember (r.layer);
        return; // layer changed meanwhile
    }
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
            auto& e = L.history[(size_t) r.targetIndex];
            if (! e.hasFeatures)
            {
                e.features = mf::analyseSound (*e.audio);
                e.hasFeatures = true;
            }
            publishLayer (r.layer);
        }
        else
        {
            L.lastError = "Could not re-render variation";
            sendChangeMessage();
        }
        return;
    }

    if (r.evolved.empty())
    {
        L.lastError = "Evolve failed";
        finishGroupMember (r.layer);
        sendChangeMessage();
        return;
    }

    // truncate redo branch, append the new variations
    L.history.resize ((size_t) L.index + 1);
    for (size_t k = 0; k < r.evolved.size(); ++k)
    {
        LayerModel::Entry e;
        e.audio = r.evolved[k].audio;
        e.seed = r.seeds[k];
        e.chaos = r.chaos;
        e.processMask = r.mask;
        e.label = r.evolved[k].steps.joinIntoString (" > ");
        if (k < r.features.size())
        {
            e.features = r.features[k];
            e.hasFeatures = true;
        }
        L.history.push_back (std::move (e));
    }

    // cap history: keep the original (entry 0), drop the oldest variation
    int dropped = 0;
    while ((int) L.history.size() > kMaxHistory)
    {
        L.history.erase (L.history.begin() + 1);
        ++dropped;
        // entries that can no longer be derived (parent gone, no audio) are dropped too;
        // this only happens with histories restored from a saved state
        while (L.history.size() > 1 && L.history[1].audio == nullptr)
        {
            L.history.erase (L.history.begin() + 1);
            ++dropped;
        }
        if (L.history.size() > 1)
            L.history[1].root = true; // its parent is gone; it keeps its own audio
    }
    if (dropped > 0)
        for (auto& g : generations)
        {
            auto& gi = g.idx[(size_t) r.layer];
            gi = gi <= 0 ? gi : std::max (1, gi - dropped);
        }

    L.index = (int) L.history.size() - 1;
    publishLayer (r.layer);
    if (groupPending & (1u << r.layer))
        finishGroupMember (r.layer);
    else
        pushGeneration();
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
    if (e.audio != nullptr)
    {
        e.features = mf::analyseSound (*e.audio);
        e.hasFeatures = true;
    }
    L.history.push_back (std::move (e));
    L.index = 0;
    ++L.epoch;
    L.busy = false;
    L.sourcePath = sourcePath;
    publishLayer (layer);
    resetGenerations();
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
    resetGenerations();
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
mf::PresetData MutaformProcessor::capturePreset (const juce::String& name) const
{
    mf::PresetData d;
    d.name = name;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            d.params[rp->getParameterID()] = rp->convertFrom0to1 (rp->getValue());
    for (int i = 0; i < mf::kNumLayers; ++i)
    {
        d.layers[(size_t) i].audio = layers[(size_t) i].current();
        d.layers[(size_t) i].multipoints = layers[(size_t) i].multipoints;
    }
    return d;
}

void MutaformProcessor::applyPreset (const mf::PresetData& d)
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto it = d.params.find (rp->getParameterID());
            const float norm = it != d.params.end() ? rp->convertTo0to1 (it->second) : rp->getDefaultValue();
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (norm);
            rp->endChangeGesture();
        }
    for (int i = 0; i < mf::kNumLayers; ++i)
    {
        const auto& L = d.layers[(size_t) i];
        if (L.audio != nullptr)
            setLayerSample (i, L.audio, "preset:" + d.name);
        else
            clearLayer (i);
        setMultipoints (i, L.multipoints);
    }
    currentPreset = d.name;
    sendChangeMessage();
}

bool MutaformProcessor::loadPresetById (const juce::String& id, juce::String& error)
{
    if (id.startsWith ("factory:"))
    {
        const int idx = id.fromFirstOccurrenceOf (":", false, false).getIntValue();
        if (! juce::isPositiveAndBelow (idx, mf::factory::count()))
        {
            error = "Unknown factory preset";
            return false;
        }
        applyPreset (mf::factory::create (idx, getSampleRate() > 0 ? getSampleRate() : 48000.0));
        return true;
    }
    if (id.startsWith ("user:"))
    {
        const auto dir = presetRoot.getChildFile (mf::safePresetDirName (id.fromFirstOccurrenceOf (":", false, false)));
        if (auto p = mf::loadPreset (dir, error))
        {
            applyPreset (*p);
            return true;
        }
        return false;
    }
    error = "Unknown preset id";
    return false;
}

bool MutaformProcessor::saveUserPreset (const juce::String& name, juce::String& error)
{
    if (name.trim().isEmpty())
    {
        error = "Empty preset name";
        return false;
    }
    if (! mf::savePreset (presetRoot, capturePreset (name.trim()), error))
        return false;
    currentPreset = name.trim();
    return true;
}

void MutaformProcessor::randomizeParameters (juce::uint64 seed)
{
    while (seed == 0)
        seed = (juce::uint64) juce::Random::getSystemRandom().nextInt64();
    mf::Rng r (seed);

    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* rp = apvts.getParameter (id))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
            rp->endChangeGesture();
        }
    };
    auto logRange = [&r] (double lo, double hi) { return (float) std::exp (r.uniform (std::log (lo), std::log (hi))); };

    for (int i = 0; i < mf::kNumLayers; ++i)
    {
        if (! layers[(size_t) i].hasSample())
            continue;
        set (mf::pid::layer (i, "gain"), i == 0 ? (float) r.uniform (-3.0, 0.0) : (float) r.uniform (-14.0, -2.0));
        set (mf::pid::layer (i, "pan"), i == 0 ? 0.0f : (float) r.uniform (-0.6, 0.6));
        static constexpr int semis[] = { 0, 0, 0, 0, 12, -12, 7, -5, 5, 24 };
        set (mf::pid::layer (i, "semi"), i == 0 ? 0.0f : (float) semis[r.range (0, 9)]);
        set (mf::pid::layer (i, "cents"), (float) r.uniform (-12.0, 12.0));
        set (mf::pid::layer (i, "reverse"), r.chance (0.2) ? 1.0f : 0.0f);
        set (mf::pid::layer (i, "start"), r.chance (0.7) ? 0.0f : (float) r.uniform (0.0, 0.15));
        set (mf::pid::layer (i, "end"), r.chance (0.7) ? 1.0f : (float) r.uniform (0.6, 1.0));
        const bool loop = r.chance (0.25);
        set (mf::pid::layer (i, "loop"), loop ? 1.0f : 0.0f);
        if (loop)
        {
            const float ls = (float) r.uniform (0.1, 0.5);
            set (mf::pid::layer (i, "loopStart"), ls);
            set (mf::pid::layer (i, "loopEnd"), (float) std::min (0.98, ls + r.uniform (0.15, 0.45)));
            set (mf::pid::layer (i, "xfade"), logRange (10.0, 200.0));
        }
    }
    set (mf::pid::attack, r.chance (0.6) ? (float) r.uniform (0.0, 8.0) : logRange (10.0, 600.0));
    set (mf::pid::decay, logRange (120.0, 2500.0));
    set (mf::pid::sustain, (float) r.uniform (0.3, 1.0));
    set (mf::pid::release, logRange (60.0, 2000.0));
    set (mf::pid::chaos, (float) r.uniform (15.0, 85.0));

    set (mf::pid::fltOn, r.chance (0.5) ? 1.0f : 0.0f);
    set (mf::pid::fltType, (float) (r.chance (0.6) ? 0 : r.range (1, 2)));
    set (mf::pid::fltCutoff, logRange (300.0, 14000.0));
    set (mf::pid::fltRes, (float) r.uniform (0.0, 0.6));
    set (mf::pid::fltMix, (float) r.uniform (0.6, 1.0));
    set (mf::pid::distOn, r.chance (0.35) ? 1.0f : 0.0f);
    set (mf::pid::distDrive, (float) r.uniform (0.1, 0.6));
    set (mf::pid::distMix, (float) r.uniform (0.4, 1.0));
    set (mf::pid::choOn, r.chance (0.35) ? 1.0f : 0.0f);
    set (mf::pid::choRate, logRange (0.1, 2.0));
    set (mf::pid::choDepth, (float) r.uniform (0.2, 0.7));
    set (mf::pid::choMix, (float) r.uniform (0.2, 0.6));
    set (mf::pid::dlyOn, r.chance (0.4) ? 1.0f : 0.0f);
    set (mf::pid::dlySync, 1.0f);
    set (mf::pid::dlyDiv, (float) r.range (2, 8));
    set (mf::pid::dlyFeedback, (float) r.uniform (0.15, 0.55));
    set (mf::pid::dlyMix, (float) r.uniform (0.1, 0.35));
    set (mf::pid::revOn, r.chance (0.55) ? 1.0f : 0.0f);
    set (mf::pid::revSize, (float) r.uniform (0.3, 0.9));
    set (mf::pid::revDamp, (float) r.uniform (0.2, 0.7));
    set (mf::pid::revWidth, (float) r.uniform (0.6, 1.0));
    set (mf::pid::revMix, (float) r.uniform (0.1, 0.4));
    currentPreset = "Random " + juce::String::toHexString ((juce::int64) seed).getLastCharacters (6).toUpperCase();
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
    root.setProperty ("preset", currentPreset, nullptr);
    root.appendChild (apvts.copyState(), nullptr);

    juce::ValueTree layersTree (kLayers);
    for (int i = 0; i < mf::kNumLayers; ++i)
    {
        const auto& L = layers[(size_t) i];
        juce::ValueTree lt (kLayer);
        lt.setProperty (kIndex, i, nullptr);
        lt.setProperty (kSource, L.sourcePath, nullptr);
        lt.setProperty (kHistIndex, L.index, nullptr);
        lt.setProperty ("locked", L.locked, nullptr);
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
            et.setProperty ("fav", en.favorite, nullptr);
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
    currentPreset = root.getProperty ("preset").toString();

    auto layersTree = root.getChildWithName (kLayers);
    for (auto& L : layers)
    {
        L.clear();
        L.locked = false;
    }

    for (auto lt : layersTree)
    {
        const int i = (int) lt.getProperty (kIndex, -1);
        if (! juce::isPositiveAndBelow (i, mf::kNumLayers))
            continue;
        auto& L = layers[(size_t) i];
        L.sourcePath = lt.getProperty (kSource).toString();
        L.locked = (bool) lt.getProperty ("locked", false);
        for (auto et : lt)
        {
            LayerModel::Entry en;
            en.seed = (juce::uint64) et.getProperty (kSeed).toString().getHexValue64();
            en.chaos = (float) et.getProperty (kChaos, 0.0f);
            en.processMask = (juce::uint32) (int) et.getProperty (kMask, 0xff);
            en.label = et.getProperty (kLabel).toString();
            en.root = (bool) et.getProperty ("root", false);
            en.favorite = (bool) et.getProperty ("fav", false);
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
    {
        auto& L = layers[(size_t) i];
        for (auto& e : L.history)
            if (e.audio != nullptr && ! e.hasFeatures)
            {
                e.features = mf::analyseSound (*e.audio);
                e.hasFeatures = true;
            }
        publishLayer (i);
    }
    diceUndo.reset();
    resetGenerations();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MutaformProcessor();
}
