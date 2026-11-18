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
}

void MutaformProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    mixBuffer.setSize (2, std::max (1, samplesPerBlock), false, true, false);
    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.masterGain->load()));
}

void MutaformProcessor::releaseResources()
{
    engine.reset();
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

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (params.masterGain->load(), -100.0f));
    masterGain.applyGain (mix, n);

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
}

void MutaformProcessor::publishLayer (int layer)
{
    auto& L = layers[(size_t) layer];
    engine.setSample (layer, L.current());
    engine.setMultipoints (layer, L.multipoints);
    sendChangeMessage();
}

void MutaformProcessor::setLayerSample (int layer, mf::SamplePtr sample, const juce::String& sourcePath)
{
    auto& L = layers[(size_t) layer];
    L.history.clear();
    L.history.push_back ({ std::move (sample), 0, 0.0f, 0xff, "Original" });
    L.index = 0;
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
            if ((e == 0 || e == L.index) && en.audio != nullptr)
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
