#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

/** WebView-based editor. All visuals live in ui/ (embedded); this class only bridges
    parameters (relays + attachments), native functions and model-change events. */
class MutaformEditor : public juce::AudioProcessorEditor,
                       public juce::FileDragAndDropTarget,
                       private juce::ChangeListener
{
public:
    explicit MutaformEditor (MutaformProcessor&);
    ~MutaformEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    using NativeCompletion = juce::WebBrowserComponent::NativeFunctionCompletion;
    using Args = juce::Array<juce::var>;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url) const;
    juce::WebBrowserComponent::Options makeBrowserOptions();
    void addNative (juce::WebBrowserComponent::Options& o, const char* name,
                    std::function<juce::var (const Args&)> fn);

    juce::var modelToVar() const;
    juce::var peaksFor (int layer, int bins) const;
    void browseForSample (int layer);
    void exportLayer (int layer);
    void pickCharacterFolder();
    void emit (const juce::String& event, const juce::var& payload);

    MutaformProcessor& processor;

    std::vector<std::unique_ptr<juce::WebSliderRelay>> sliderRelays;
    std::vector<std::unique_ptr<juce::WebToggleButtonRelay>> toggleRelays;
    std::vector<std::unique_ptr<juce::WebComboBoxRelay>> comboRelays;
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::WebToggleButtonParameterAttachment>> toggleAttachments;
    std::vector<std::unique_ptr<juce::WebComboBoxParameterAttachment>> comboAttachments;

    std::unique_ptr<juce::WebBrowserComponent> browser;
    std::unique_ptr<juce::FileChooser> chooser;
    int selectedLayer = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MutaformEditor)
};
