#include "PluginProcessor.h"
#include "PluginEditor.h"

MutaformProcessor::MutaformProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MUTAFORM", mf::createParameterLayout())
{
    masterGainParam = apvts.getRawParameterValue (mf::pid::masterGain);
}

void MutaformProcessor::prepareToPlay (double, int) {}

bool MutaformProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void MutaformProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
}

juce::AudioProcessorEditor* MutaformProcessor::createEditor()
{
    return new MutaformEditor (*this);
}

void MutaformProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream mos (destData, false);
    state.writeToStream (mos);
}

void MutaformProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid() && tree.hasType (apvts.state.getType()))
        apvts.replaceState (tree);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MutaformProcessor();
}
