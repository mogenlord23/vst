#include "PluginEditor.h"
#include "MutaformUI.h"

namespace
{
const char* mimeForPath (const juce::String& path)
{
    if (path.endsWithIgnoreCase (".html")) return "text/html; charset=utf-8";
    if (path.endsWithIgnoreCase (".css"))  return "text/css; charset=utf-8";
    if (path.endsWithIgnoreCase (".js"))   return "text/javascript; charset=utf-8";
    if (path.endsWithIgnoreCase (".svg"))  return "image/svg+xml";
    if (path.endsWithIgnoreCase (".json")) return "application/json";
    if (path.endsWithIgnoreCase (".ttf"))  return "font/ttf";
    return "application/octet-stream";
}

int layerArg (const juce::Array<juce::var>& a, int i = 0)
{
    return a.size() > i ? juce::jlimit (0, mf::kNumLayers - 1, (int) a[i]) : 0;
}

juce::String hexSeed (juce::uint64 s)
{
    return juce::String::toHexString ((juce::int64) s).toUpperCase().paddedLeft ('0', 16);
}

const juce::StringArray& audioExtensions()
{
    static const juce::StringArray ext { ".wav", ".wave", ".aif", ".aiff", ".flac", ".mp3", ".ogg" };
    return ext;
}
} // namespace

MutaformEditor::MutaformEditor (MutaformProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // Relays must exist before the browser is created (they contribute options).
    for (auto* param : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr)
            continue;
        const auto id = ranged->getParameterID();
        if (dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr)
            toggleRelays.push_back (std::make_unique<juce::WebToggleButtonRelay> (id));
        else if (dynamic_cast<juce::AudioParameterChoice*> (ranged) != nullptr)
            comboRelays.push_back (std::make_unique<juce::WebComboBoxRelay> (id));
        else
            sliderRelays.push_back (std::make_unique<juce::WebSliderRelay> (id));
    }

    browser = std::make_unique<juce::WebBrowserComponent> (makeBrowserOptions());

    auto paramFor = [this] (const juce::String& id) -> juce::RangedAudioParameter&
    {
        auto* rp = processor.apvts.getParameter (id);
        jassert (rp != nullptr);
        return *rp;
    };
    {
        size_t s = 0, t = 0, c = 0;
        for (auto* param : processor.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
            if (ranged == nullptr)
                continue;
            const auto id = ranged->getParameterID();
            if (dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr)
                toggleAttachments.push_back (std::make_unique<juce::WebToggleButtonParameterAttachment> (paramFor (id), *toggleRelays[t++], nullptr));
            else if (dynamic_cast<juce::AudioParameterChoice*> (ranged) != nullptr)
                comboAttachments.push_back (std::make_unique<juce::WebComboBoxParameterAttachment> (paramFor (id), *comboRelays[c++], nullptr));
            else
                sliderAttachments.push_back (std::make_unique<juce::WebSliderParameterAttachment> (paramFor (id), *sliderRelays[s++], nullptr));
        }
    }

    addAndMakeVisible (*browser);
    browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    processor.addChangeListener (this);

    setResizable (true, true);
    setResizeLimits (450, 600, 1200, 1600);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (600.0 / 800.0);
    setSize (600, 800);
}

MutaformEditor::~MutaformEditor()
{
    processor.removeChangeListener (this);
}

void MutaformEditor::addNative (juce::WebBrowserComponent::Options& o, const char* name,
                                std::function<juce::var (const Args&)> fn)
{
    o = o.withNativeFunction (name, [f = std::move (fn)] (const Args& args, NativeCompletion complete)
                              {
                                  complete (f (args));
                              });
}

juce::WebBrowserComponent::Options MutaformEditor::makeBrowserOptions()
{
    using Opts = juce::WebBrowserComponent::Options;
    auto o = Opts {}
                 .withBackend (Opts::Backend::webview2)
                 .withWinWebView2Options (Opts::WinWebView2 {}
                                              .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                                                       .getChildFile ("Mutaform_WebView2"))
                                              .withBackgroundColour (juce::Colour (0xff1a1219))
                                              .withStatusBarDisabled()
                                              .withBuiltInErrorPageDisabled())
                 .withNativeIntegrationEnabled()
                 .withKeepPageLoadedWhenBrowserIsHidden()
                 .withResourceProvider ([this] (const juce::String& url) { return getResource (url); });

    for (auto& r : sliderRelays) o = o.withOptionsFrom (*r);
    for (auto& r : toggleRelays) o = o.withOptionsFrom (*r);
    for (auto& r : comboRelays)  o = o.withOptionsFrom (*r);

    auto* proc = &processor;

    addNative (o, "log", [] (const Args& a)
    {
        juce::Logger::writeToLog ("[ui] " + (a.isEmpty() ? juce::String() : a[0].toString()));
        return juce::var();
    });
    addNative (o, "getModel", [this] (const Args&) { return modelToVar(); });
    addNative (o, "getPeaks", [this] (const Args& a) { return peaksFor (layerArg (a), a.size() > 1 ? juce::jlimit (16, 4096, (int) a[1]) : 512); });

    addNative (o, "loadSampleData", [proc] (const Args& a)
    {
        juce::DynamicObject::Ptr res = new juce::DynamicObject();
        const int layer = layerArg (a);
        const auto name = a.size() > 1 ? a[1].toString() : juce::String ("sample.wav");
        juce::MemoryBlock mb;
        juce::String err;
        if (a.size() < 3 || ! mb.fromBase64Encoding (a[2].toString()) || mb.getSize() == 0)
            err = "Could not read dropped file";
        else if (mb.getSize() > 200u * 1024u * 1024u)
            err = "File too large";
        else
            proc->loadSampleMemory (layer, mb.getData(), mb.getSize(), name, err);
        res->setProperty ("ok", err.isEmpty());
        res->setProperty ("error", err);
        return juce::var (res.get());
    });
    addNative (o, "browseSample", [this] (const Args& a) { browseForSample (layerArg (a)); return juce::var(); });
    addNative (o, "clearLayer", [proc] (const Args& a) { proc->clearLayer (layerArg (a)); return juce::var(); });
    addNative (o, "selectLayer", [this] (const Args& a) { selectedLayer = layerArg (a); return juce::var(); });

    addNative (o, "evolve", [proc] (const Args& a)
    {
        juce::uint64 seed = 0;
        if (a.size() > 1 && a[1].toString().trim().isNotEmpty())
            seed = (juce::uint64) a[1].toString().trim().getHexValue64();
        return juce::var (proc->evolveLayer (layerArg (a), seed));
    });
    addNative (o, "evolveAll", [proc] (const Args& a)
    {
        const auto mask = a.size() > 0 ? (juce::uint32) (int) a[0] : 0x1fu;
        return juce::var (proc->evolveGroup (mask == 0 ? 0x1fu : mask));
    });
    addNative (o, "rollDice", [proc] (const Args&) { proc->rollDice(); return juce::var(); });
    addNative (o, "undoDice", [proc] (const Args&) { return juce::var (proc->undoDice()); });
    addNative (o, "stepLevel", [proc] (const Args& a) { return juce::var (proc->stepGeneration (a.size() > 0 ? (int) a[0] : 1)); });
    addNative (o, "setLocked", [proc] (const Args& a) { proc->setLocked (layerArg (a), a.size() > 1 && (bool) a[1]); return juce::var(); });
    addNative (o, "toggleFavorite", [proc] (const Args& a) { proc->toggleFavorite (layerArg (a), a.size() > 1 ? (int) a[1] : -1); return juce::var(); });
    addNative (o, "resetLayer", [proc] (const Args& a) { return juce::var (proc->jumpToHistory (layerArg (a), 0)); });
    addNative (o, "exportLayer", [this] (const Args& a) { exportLayer (layerArg (a)); return juce::var(); });
    addNative (o, "undo", [proc] (const Args& a) { return juce::var (proc->undo (layerArg (a))); });
    addNative (o, "redo", [proc] (const Args& a) { return juce::var (proc->redo (layerArg (a))); });
    addNative (o, "jumpHistory", [proc] (const Args& a) { return juce::var (proc->jumpToHistory (layerArg (a), a.size() > 1 ? (int) a[1] : 0)); });
    addNative (o, "setMultipoints", [proc] (const Args& a)
    {
        std::vector<float> pts;
        if (a.size() > 1)
            if (auto* arr = a[1].getArray())
                for (auto& v : *arr)
                    pts.push_back ((float) (double) v);
        proc->setMultipoints (layerArg (a), std::move (pts));
        return juce::var();
    });
    addNative (o, "noteOn", [proc] (const Args& a)
    {
        proc->getEngine().queueNote (a.size() > 0 ? (int) a[0] : mf::kRootNote, a.size() > 1 ? (float) (double) a[1] : 0.8f);
        return juce::var();
    });
    addNative (o, "noteOff", [proc] (const Args& a)
    {
        proc->getEngine().queueNote (a.size() > 0 ? (int) a[0] : mf::kRootNote, 0.0f);
        return juce::var();
    });
    addNative (o, "listPresets", [proc] (const Args&)
    {
        juce::Array<juce::var> list;
        auto add = [&list] (const juce::String& id, const juce::String& name, bool factory)
        {
            juce::DynamicObject::Ptr p = new juce::DynamicObject();
            p->setProperty ("id", id);
            p->setProperty ("name", name);
            p->setProperty ("factory", factory);
            list.add (juce::var (p.get()));
        };
        for (int i = 0; i < mf::factory::count(); ++i)
            add ("factory:" + juce::String (i), mf::factory::name (i), true);
        for (auto& dir : mf::listUserPresets (proc->presetRoot))
        {
            juce::String err;
            auto name = juce::JSON::parse (dir.getChildFile ("preset.json").loadFileAsString())["name"].toString();
            add ("user:" + dir.getFileName(), name.isNotEmpty() ? name : dir.getFileName(), false);
        }
        return juce::var (list);
    });
    addNative (o, "getCurrentPreset", [proc] (const Args&) { return juce::var (proc->getCurrentPresetName()); });
    addNative (o, "loadPreset", [proc] (const Args& a)
    {
        juce::String err;
        const bool ok = proc->loadPresetById (a.isEmpty() ? juce::String() : a[0].toString(), err);
        juce::DynamicObject::Ptr r = new juce::DynamicObject();
        r->setProperty ("ok", ok);
        r->setProperty ("error", err);
        return juce::var (r.get());
    });
    addNative (o, "savePreset", [proc] (const Args& a)
    {
        juce::String err;
        const bool ok = proc->saveUserPreset (a.isEmpty() ? juce::String() : a[0].toString(), err);
        juce::DynamicObject::Ptr r = new juce::DynamicObject();
        r->setProperty ("ok", ok);
        r->setProperty ("error", err);
        return juce::var (r.get());
    });
    addNative (o, "randomize", [proc] (const Args&) { proc->randomizeParameters(); return juce::var (proc->getCurrentPresetName()); });
    addNative (o, "getMeters", [proc] (const Args&)
    {
        juce::DynamicObject::Ptr m = new juce::DynamicObject();
        juce::Array<juce::var> ph;
        for (int l = 0; l < mf::kNumLayers; ++l)
            ph.add (proc->getEngine().getPlayhead (l));
        m->setProperty ("playheads", ph);
        m->setProperty ("voices", proc->getEngine().getNumActiveVoices());
        m->setProperty ("limiter", proc->getFx().getLimiterGain());
        return juce::var (m.get());
    });

    return o;
}

void MutaformEditor::emit (const juce::String& event, const juce::var& payload)
{
    if (browser != nullptr)
        browser->emitEventIfBrowserIsVisible (juce::Identifier (event), payload);
}

void MutaformEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    emit ("modelChanged", modelToVar());
}

juce::var MutaformEditor::modelToVar() const
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    juce::Array<juce::var> ls;
    for (int i = 0; i < mf::kNumLayers; ++i)
    {
        const auto& L = processor.getLayer (i);
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        const auto cur = L.current();
        o->setProperty ("loaded", L.hasSample());
        o->setProperty ("name", cur != nullptr ? cur->name : juce::String());
        o->setProperty ("seconds", cur != nullptr ? cur->seconds() : 0.0);
        o->setProperty ("sampleRate", cur != nullptr ? cur->sampleRate : 0.0);
        o->setProperty ("revision", L.revision);
        o->setProperty ("busy", L.busy);
        o->setProperty ("index", L.index);
        o->setProperty ("canUndo", processor.canUndo (i));
        o->setProperty ("canRedo", processor.canRedo (i));
        o->setProperty ("error", L.lastError);
        o->setProperty ("locked", L.locked);
        o->setProperty ("level", processor.getLevel (i));
        if (L.index >= 0 && L.history[(size_t) L.index].hasFeatures)
        {
            const auto& f = L.history[(size_t) L.index].features;
            o->setProperty ("brightness", f.brightness);
            o->setProperty ("punch", f.punch);
            o->setProperty ("motion", f.motion);
        }
        juce::Array<juce::var> mp;
        for (auto v : L.multipoints)
            mp.add (v);
        o->setProperty ("multipoints", mp);
        juce::Array<juce::var> hist;
        for (const auto& e : L.history)
        {
            juce::DynamicObject::Ptr h = new juce::DynamicObject();
            h->setProperty ("seed", e.seed == 0 ? juce::String() : hexSeed (e.seed));
            h->setProperty ("chaos", juce::roundToInt (e.chaos * 100.0f));
            h->setProperty ("label", e.label);
            h->setProperty ("name", e.seed == 0 ? juce::String ("ORIGINAL") : mf::variationName (e.seed));
            h->setProperty ("fav", e.favorite);
            h->setProperty ("tags", e.hasFeatures ? e.features.tags.joinIntoString (" ") : juce::String());
            hist.add (juce::var (h.get()));
        }
        o->setProperty ("history", hist);
        ls.add (juce::var (o.get()));
    }
    root->setProperty ("layers", ls);
    root->setProperty ("version", JucePlugin_VersionString);
    root->setProperty ("preset", processor.getCurrentPresetName());
    root->setProperty ("level", processor.getLevel());
    root->setProperty ("canPrevLevel", processor.canStepGeneration (-1));
    root->setProperty ("canNextLevel", processor.canStepGeneration (1));
    root->setProperty ("canUndoDice", processor.canUndoDice());
    juce::uint64 h = 1469598103934665603ull;
    for (int i = 0; i < mf::kNumLayers; ++i)
    {
        const auto& L = processor.getLayer (i);
        const auto s = L.index >= 0 ? L.history[(size_t) L.index].seed : 0;
        h = (h ^ (s + (juce::uint64) i * 7919 + (juce::uint64) (L.hasSample() ? 1 : 0))) * 1099511628211ull;
    }
    root->setProperty ("creatureSeed", juce::String::toHexString ((juce::int64) h));
    return juce::var (root.get());
}

juce::var MutaformEditor::peaksFor (int layer, int bins) const
{
    juce::Array<juce::var> arr;
    if (auto s = processor.getLayer (layer).current())
        for (auto v : mf::computePeaks (*s, bins))
            arr.add (juce::roundToInt (v * 1000.0f) / 1000.0);
    return arr;
}

void MutaformEditor::exportLayer (int layer)
{
    auto sample = processor.getLayer (layer).current();
    if (sample == nullptr)
    {
        emit ("notify", juce::var ("Nothing to export"));
        return;
    }
    const auto& L = processor.getLayer (layer);
    const auto seed = L.history[(size_t) L.index].seed;
    const auto base = (seed == 0 ? sample->name : mf::variationName (seed)).replaceCharacter (' ', '_');
    chooser = std::make_unique<juce::FileChooser> ("Export one-shot",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (base + ".wav"),
                                                   "*.wav");
    juce::Component::SafePointer<MutaformEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, sample] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr)
                                  return;
                              auto f = fc.getResult();
                              if (f == juce::File())
                                  return;
                              f = f.withFileExtension (".wav");
                              safe->emit ("notify", juce::var (mf::writeWav (*sample, f) ? "Saved " + f.getFileName() : juce::String ("Export failed")));
                          });
}

void MutaformEditor::browseForSample (int layer)
{
    chooser = std::make_unique<juce::FileChooser> ("Load a one-shot", juce::File(), "*.wav;*.wave;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    juce::Component::SafePointer<MutaformEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe, layer] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr)
                                  return;
                              const auto f = fc.getResult();
                              if (! f.existsAsFile())
                                  return;
                              juce::String err;
                              if (! safe->processor.loadSampleFile (layer, f, err))
                                  safe->emit ("notify", juce::var (err));
                          });
}

bool MutaformEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        for (auto& e : audioExtensions())
            if (f.endsWithIgnoreCase (e))
                return true;
    return false;
}

void MutaformEditor::filesDropped (const juce::StringArray& files, int, int)
{
    int layer = selectedLayer;
    for (auto& f : files)
    {
        juce::String err;
        if (processor.loadSampleFile (layer, juce::File (f), err))
            layer = (layer + 1) % mf::kNumLayers;
        else
            emit ("notify", juce::var (err));
    }
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
    g.fillAll (juce::Colour (0xff1a1219));
}

void MutaformEditor::resized()
{
    browser->setBounds (getLocalBounds());
}
