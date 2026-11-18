#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class MutaformEditor : public juce::AudioProcessorEditor
{
public:
    explicit MutaformEditor (MutaformProcessor&);
    ~MutaformEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url) const;
    juce::WebBrowserComponent::Options makeBrowserOptions();

    MutaformProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MutaformEditor)
};
