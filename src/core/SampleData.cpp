#include "SampleData.h"

namespace mf
{
juce::AudioFormatManager& audioFormats()
{
    static juce::AudioFormatManager mgr;
    static const bool registered = [] { mgr.registerBasicFormats(); return true; }();
    juce::ignoreUnused (registered);
    return mgr;
}

std::shared_ptr<SampleData> readSample (juce::AudioFormatReader& reader, const juce::String& name, juce::String& error)
{
    if (reader.sampleRate <= 0 || reader.lengthInSamples <= 0 || reader.numChannels == 0)
    {
        error = "Empty or invalid audio file";
        return nullptr;
    }

    const auto maxLen = (juce::int64) (kMaxSampleSeconds * reader.sampleRate);
    const auto len = (int) std::min (reader.lengthInSamples, maxLen);

    auto s = std::make_shared<SampleData>();
    s->sampleRate = reader.sampleRate;
    s->name = name;
    s->audio.setSize (2, len);
    s->audio.clear();

    const int srcCh = (int) std::min<unsigned int> (reader.numChannels, 2u);
    juce::AudioBuffer<float> tmp (srcCh, len);
    if (! reader.read (&tmp, 0, len, 0, true, srcCh > 1))
    {
        error = "Could not decode audio data";
        return nullptr;
    }
    s->audio.copyFrom (0, 0, tmp, 0, 0, len);
    s->audio.copyFrom (1, 0, tmp, srcCh > 1 ? 1 : 0, 0, len);

    // sanitise
    for (int c = 0; c < 2; ++c)
    {
        auto* d = s->audio.getWritePointer (c);
        for (int i = 0; i < len; ++i)
            if (! std::isfinite (d[i]))
                d[i] = 0.0f;
    }
    return s;
}

std::shared_ptr<SampleData> loadSampleFromFile (const juce::File& file, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader (audioFormats().createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Unsupported or unreadable file: " + file.getFileName();
        return nullptr;
    }
    return readSample (*reader, file.getFileNameWithoutExtension(), error);
}

std::shared_ptr<SampleData> loadSampleFromMemory (const void* data, size_t size, const juce::String& name, juce::String& error)
{
    auto stream = std::make_unique<juce::MemoryInputStream> (data, size, true);
    std::unique_ptr<juce::AudioFormatReader> reader (audioFormats().createReaderFor (std::move (stream)));
    if (reader == nullptr)
    {
        error = "Unsupported audio format";
        return nullptr;
    }
    return readSample (*reader, juce::File::createFileWithoutCheckingPath (name).getFileNameWithoutExtension(), error);
}

juce::MemoryBlock encodeSample (const SampleData& s)
{
    juce::MemoryBlock out;
    {
        juce::MemoryOutputStream mos (out, false);
        mos.writeInt (0x4d465331); // 'MFS1'
        mos.writeDouble (s.sampleRate);
        mos.writeString (s.name);
        mos.writeInt (s.length());
        juce::GZIPCompressorOutputStream gz (mos, 6);
        for (int c = 0; c < 2; ++c)
            gz.write (s.audio.getReadPointer (c), sizeof (float) * (size_t) s.length());
        gz.flush();
    }
    return out;
}

std::shared_ptr<SampleData> decodeSample (const juce::MemoryBlock& block)
{
    juce::MemoryInputStream mis (block, false);
    if (mis.readInt() != 0x4d465331)
        return nullptr;
    auto s = std::make_shared<SampleData>();
    s->sampleRate = mis.readDouble();
    s->name = mis.readString();
    const int len = mis.readInt();
    if (len <= 0 || len > 48000 * 200 || s->sampleRate <= 0)
        return nullptr;
    s->audio.setSize (2, len);
    juce::GZIPDecompressorInputStream gz (mis);
    for (int c = 0; c < 2; ++c)
    {
        const auto bytes = (int) (sizeof (float) * (size_t) len);
        if (gz.read (s->audio.getWritePointer (c), bytes) != bytes)
            return nullptr;
    }
    return s;
}

bool writeWav (const SampleData& s, const juce::File& file)
{
    file.deleteFile();
    auto os = file.createOutputStream();
    if (os == nullptr)
        return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (os.release());
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                   .withSampleRate (s.sampleRate)
                                                   .withNumChannels (2)
                                                   .withBitsPerSample (32)
                                                   .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    if (writer == nullptr)
        return false;
    return writer->writeFromAudioSampleBuffer (s.audio, 0, s.length());
}

std::vector<float> computePeaks (const SampleData& s, int numBins)
{
    std::vector<float> out ((size_t) numBins * 2, 0.0f);
    const int len = s.length();
    if (len <= 0 || numBins <= 0)
        return out;
    const auto* l = s.audio.getReadPointer (0);
    const auto* r = s.audio.getReadPointer (1);
    for (int b = 0; b < numBins; ++b)
    {
        const int i0 = (int) ((juce::int64) b * len / numBins);
        const int i1 = std::max (i0 + 1, (int) ((juce::int64) (b + 1) * len / numBins));
        float mn = 1.0f, mx = -1.0f;
        for (int i = i0; i < i1 && i < len; ++i)
        {
            const float v = 0.5f * (l[i] + r[i]);
            mn = std::min (mn, v);
            mx = std::max (mx, v);
        }
        if (mn > mx) mn = mx = 0.0f;
        out[(size_t) b * 2] = mn;
        out[(size_t) b * 2 + 1] = mx;
    }
    return out;
}

float normalise (juce::AudioBuffer<float>& audio, float targetPeak)
{
    const int n = audio.getNumSamples();
    if (n == 0)
        return 1.0f;
    for (int c = 0; c < audio.getNumChannels(); ++c)
    {
        auto* d = audio.getWritePointer (c);
        double mean = 0.0;
        for (int i = 0; i < n; ++i)
            mean += d[i];
        mean /= n;
        for (int i = 0; i < n; ++i)
        {
            d[i] -= (float) mean;
            if (! std::isfinite (d[i]))
                d[i] = 0.0f;
        }
    }
    float pk = 0.0f;
    for (int c = 0; c < audio.getNumChannels(); ++c)
        pk = std::max (pk, audio.getMagnitude (c, 0, n));
    if (pk < 1.0e-6f)
        return 1.0f;
    const float g = targetPeak / pk;
    audio.applyGain (g);
    return g;
}
} // namespace mf
