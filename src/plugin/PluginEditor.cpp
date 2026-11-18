#include "PluginEditor.h"
#include "MutaformUI.h"

namespace
{
const char* mimeForPath (const juce::String& path)
{
    if (path.endsWithIgnoreCase (".html")) return "text/html";
    if (path.endsWithIgnoreCase (".css"))  return "text/css";
    if (path.endsWithIgnoreCase (".js"))   return "text/javascript";
    if (path.endsWithIgnoreCase (".svg"))  return "image/svg+xml";
    if (path.endsWithIgnoreCase (".json")) return "application/json";
    return "application/octet-stream";
}
} // namespace

MutaformEditor::MutaformEditor (MutaformProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    browser = std::make_unique<juce::WebBrowserComponent> (makeBrowserOptions());
    addAndMakeVisible (*browser);
    browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable (true, true);
    setResizeLimits (600, 400, 1800, 1200);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (900.0 / 600.0);
    setSize (900, 600);
}

MutaformEditor::~MutaformEditor() = default;

juce::WebBrowserComponent::Options MutaformEditor::makeBrowserOptions()
{
    using Opts = juce::WebBrowserComponent::Options;
    return Opts {}
        .withBackend (Opts::Backend::webview2)
        .withWinWebView2Options (Opts::WinWebView2 {}
                                     .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                                              .getChildFile ("Mutaform_WebView2"))
                                     .withBackgroundColour (juce::Colour (0xff0d0f13))
                                     .withStatusBarDisabled())
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeFunction ("ping", [] (const juce::Array<juce::var>& args, auto complete)
                             {
                                 complete (juce::var ("pong:" + (args.isEmpty() ? juce::String() : args[0].toString())));
                             })
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); });
}

std::optional<juce::WebBrowserComponent::Resource> MutaformEditor::getResource (const juce::String& url) const
{
    auto path = url.upToFirstOccurrenceOf ("?", false, false);
    if (path.isEmpty() || path == "/")
        path = "/index.html";

    const auto fileName = path.fromLastOccurrenceOf ("/", false, false);

    for (int i = 0; i < MutaformUI::namedResourceListSize; ++i)
    {
        const auto* resName = MutaformUI::namedResourceList[i];
        if (fileName == MutaformUI::getNamedResourceOriginalFilename (resName))
        {
            int size = 0;
            const auto* data = MutaformUI::getNamedResource (resName, size);
            std::vector<std::byte> bytes ((size_t) size);
            std::memcpy (bytes.data(), data, (size_t) size);
            return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mimeForPath (fileName)) };
        }
    }
    return std::nullopt;
}

void MutaformEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0f13));
}

void MutaformEditor::resized()
{
    browser->setBounds (getLocalBounds());
}
