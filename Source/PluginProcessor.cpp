#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::StringArray getStyleNames()
{
    return { "Clean", "Vocal", "Punch", "Bus", "Master", "Opto", "Pump", "Limit" };
}

juce::AudioProcessorValueTreeState::ParameterLayout KompresorC3AudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pid = [] (const char* id) { return ParameterID { id, 1 }; };

    layout.add (std::make_unique<AudioParameterFloat> (pid ("threshold"), "Threshold",
        NormalisableRange<float> (-60.0f, 0.0f, 0.1f, 1.0f), -20.0f));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("ratio"), "Ratio",
        NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.375f), 4.0f));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("attack"), "Attack",
        NormalisableRange<float> (0.1f, 200.0f, 0.0f, 0.23f), 10.0f));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("release"), "Release",
        NormalisableRange<float> (5.0f, 2000.0f, 0.0f, 0.23f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("knee"), "Knee",
        NormalisableRange<float> (0.0f, 24.0f, 0.1f, 1.0f), 6.0f));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("range"), "Range",
        NormalisableRange<float> (0.0f, 60.0f, 0.1f, 1.0f), 60.0f));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("makeup"), "Makeup",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f, 1.0f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("mix"), "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f, 1.0f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("lookahead"), "Lookahead",
        NormalisableRange<float> (0.0f, 20.0f, 0.1f, 1.0f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("hpfFreq"), "SC HPF Freq",
        NormalisableRange<float> (20.0f, 2000.0f, 0.0f, 0.29f), 120.0f));

    layout.add (std::make_unique<AudioParameterBool> (pid ("hpfOn"), "SC HPF", false));
    layout.add (std::make_unique<AudioParameterBool> (pid ("autoRelease"), "Auto Release", false));
    layout.add (std::make_unique<AudioParameterBool> (pid ("scExternal"), "External SC", false));
    layout.add (std::make_unique<AudioParameterBool> (pid ("stereoLink"), "Stereo Link", true));

    layout.add (std::make_unique<AudioParameterChoice> (pid ("detectorMode"), "Detector",
        StringArray { "Peak", "RMS" }, 0));

    layout.add (std::make_unique<AudioParameterChoice> (pid ("style"), "Style",
        getStyleNames(), 0));

    return layout;
}

KompresorC3AudioProcessor::KompresorC3AudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",      juce::AudioChannelSet::stereo(), true)
          .withInput  ("Sidechain",  juce::AudioChannelSet::stereo(), false)
          .withOutput ("Output",     juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    thresholdParam = apvts.getRawParameterValue ("threshold");
    ratioParam     = apvts.getRawParameterValue ("ratio");
    attackParam    = apvts.getRawParameterValue ("attack");
    releaseParam   = apvts.getRawParameterValue ("release");
    kneeParam      = apvts.getRawParameterValue ("knee");
    rangeParam     = apvts.getRawParameterValue ("range");
    makeupParam    = apvts.getRawParameterValue ("makeup");
    mixParam       = apvts.getRawParameterValue ("mix");
    lookaheadParam = apvts.getRawParameterValue ("lookahead");
    hpfFreqParam   = apvts.getRawParameterValue ("hpfFreq");
    hpfOnParam     = apvts.getRawParameterValue ("hpfOn");
    detectorModeParam = apvts.getRawParameterValue ("detectorMode");
    autoRelParam   = apvts.getRawParameterValue ("autoRelease");
    scExtParam     = apvts.getRawParameterValue ("scExternal");
    stereoLinkParam = apvts.getRawParameterValue ("stereoLink");
    styleParam     = apvts.getRawParameterValue ("style");

    apvts.addParameterListener ("style", this);
}

KompresorC3AudioProcessor::~KompresorC3AudioProcessor()
{
    apvts.removeParameterListener ("style", this);
}

void KompresorC3AudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID != "style" || (stateLoading != nullptr && stateLoading->load()))
        return;

    applyStylePreset ((int) newValue);
}

void KompresorC3AudioProcessor::applyStylePreset (int styleIndex)
{
    struct Preset
    {
        float threshold, ratio, attack, release, knee, range, makeup, mix, lookahead, hpfFreq;
        bool hpfOn, rms, autoRel;
    };

    static const Preset presets[] =
    {
        // thr    ratio  atk   rel    knee  range  makeup mix   look  hpf    hpfOn rms   auto
        { -20.0f,  4.0f, 10.0f, 100.0f,  6.0f, 60.0f,  0.0f, 100.0f, 0.0f, 120.0f, false, false, false },
        { -18.0f,  3.0f,  5.0f,  80.0f,  9.0f, 12.0f,  2.0f, 100.0f, 0.0f, 120.0f, true,  true,  true  },
        { -16.0f,  6.0f,  0.5f,  50.0f,  3.0f, 12.0f,  1.0f, 100.0f, 2.0f, 120.0f, false, false, false },
        { -22.0f,  2.0f, 30.0f, 300.0f, 12.0f,  6.0f,  0.0f, 100.0f, 5.0f,  60.0f, true,  true,  true  },
        { -14.0f,  2.0f, 20.0f, 200.0f,  8.0f,  3.0f,  0.0f, 100.0f, 1.0f,  80.0f, true,  false, true  },
        { -20.0f,  3.0f, 15.0f, 400.0f, 10.0f, 12.0f,  1.0f, 100.0f, 0.0f, 120.0f, false, false, false },
        { -25.0f,  8.0f,  0.1f,  30.0f,  0.0f, 24.0f,  3.0f, 100.0f, 0.0f, 120.0f, false, false, false },
        { -10.0f, 20.0f,  0.1f,  20.0f,  0.0f, 12.0f,  0.0f, 100.0f, 2.0f, 120.0f, false, false, true  }
    };

    const int idx = juce::jlimit (0, (int) std::size (presets) - 1, styleIndex);
    const auto& pr = presets[(size_t) idx];

    auto setFloat = [this] (const char* pid, float value)
    {
        if (auto* param = apvts.getParameter (pid))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    auto setBool = [this] (const char* pid, bool value)
    {
        if (auto* param = apvts.getParameter (pid))
            param->setValueNotifyingHost (value ? 1.0f : 0.0f);
    };

    setFloat ("threshold", pr.threshold);
    setFloat ("ratio",     pr.ratio);
    setFloat ("attack",    pr.attack);
    setFloat ("release",   pr.release);
    setFloat ("knee",      pr.knee);
    setFloat ("range",     pr.range);
    setFloat ("makeup",    pr.makeup);
    setFloat ("mix",       pr.mix);
    setFloat ("lookahead", pr.lookahead);
    setFloat ("hpfFreq",   pr.hpfFreq);
    setBool  ("hpfOn",     pr.hpfOn);
    setBool  ("rmsMode",   pr.rms);
    setBool  ("autoRelease", pr.autoRel);
}

void KompresorC3AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    compressor.prepare (sampleRate, samplesPerBlock);
    compressor.reset();
    peakHoldDb = 0.0f;
    spectrumFifo.reset();
}

bool KompresorC3AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    if (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo())
        return false;

    const auto& sc = layouts.getChannelSet (true, 1);

    if (sc.isDisabled())
        return true;

    return sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

void KompresorC3AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainIn = getBusBuffer (buffer, true, 0);
    auto outBus = getBusBuffer (buffer, false, 0);
    auto scBus  = getBusBuffer (buffer, true, 1);

    const int numSamples = buffer.getNumSamples();
    const int numOutChannels = outBus.getNumChannels();

    if (numSamples <= 0 || numOutChannels <= 0)
        return;

    const int numInChannels = mainIn.getNumChannels();

    if (numInChannels <= 0)
    {
        outBus.clear();
        return;
    }

    const int numScChannels = scBus.getNumChannels();
    const bool useExternalSc = scExtParam->load() > 0.5f && numScChannels > 0;

    kmp::CompressorParams p;
    p.thresholdDb   = thresholdParam->load();
    p.ratio         = ratioParam->load();
    p.attackMs      = attackParam->load();
    p.releaseMs     = releaseParam->load();
    p.kneeDb        = kneeParam->load();
    p.rangeDb       = rangeParam->load();
    p.makeupDb      = makeupParam->load();
    p.mix           = mixParam->load() / 100.0f;
    p.lookaheadMs   = lookaheadParam->load();
    p.hpfHz         = hpfFreqParam->load();
    p.hpfOn         = hpfOnParam->load() > 0.5f;
    p.rmsMode       = detectorModeParam->load() > 0.5f;
    p.autoRelease   = autoRelParam->load() > 0.5f;
    p.stereoLink    = stereoLinkParam->load() > 0.5f;

    compressor.setParams (p);

    const float* mainPtrs[2] =
    {
        mainIn.getReadPointer (0),
        mainIn.getNumChannels() > 1 ? mainIn.getReadPointer (1) : mainIn.getReadPointer (0)
    };

    const float* scPtrs[2] = { nullptr, nullptr };

    if (numScChannels > 0)
    {
        scPtrs[0] = scBus.getReadPointer (0);
        scPtrs[1] = numScChannels > 1 ? scBus.getReadPointer (1) : scPtrs[0];
    }

    float* outPtrs[2] =
    {
        outBus.getWritePointer (0),
        numOutChannels > 1 ? outBus.getWritePointer (1) : outBus.getWritePointer (0)
    };

    compressor.process (mainPtrs, numInChannels,
                        useExternalSc ? scPtrs : nullptr, useExternalSc ? numScChannels : 0,
                        outPtrs, numOutChannels, numSamples);

    const float gr = juce::jmax (0.0f, compressor.getMaxReductionDb());
    grDb.store (gr);
    peakHoldDb = juce::jmax (peakHoldDb - 0.2f, gr);
    grPeakDb.store (peakHoldDb);
    inLevelDb.store  (juce::jmin (compressor.getInputLevelDb (0), 0.0f));
    outLevelDb.store (juce::jmin (compressor.getOutputLevelDb (0), 0.0f));

    for (int i = 0; i < numSamples; ++i)
    {
        float mono = mainPtrs[0][i];

        if (numInChannels > 1)
            mono = 0.5f * (mono + mainPtrs[1][i]);

        pushSpectrumSamples (&mono, 1);
    }

    const int latency = compressor.getDelaySamples();
    setLatencySamples (latency);
}

void KompresorC3AudioProcessor::pushSpectrumSamples (const float* samples, int num)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    spectrumFifo.prepareToWrite (num, start1, size1, start2, size2);

    if (size1 > 0)
        std::copy_n (samples, size1, spectrumBuffer.data() + start1);

    if (size2 > 0)
        std::copy_n (samples + size1, size2, spectrumBuffer.data() + start2);

    spectrumFifo.finishedWrite (size1 + size2);
}

int KompresorC3AudioProcessor::pullSpectrumSamples (float* dest, int maxSamples)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    spectrumFifo.prepareToRead (maxSamples, start1, size1, start2, size2);

    if (size1 > 0)
        std::copy_n (spectrumBuffer.data() + start1, size1, dest);

    if (size2 > 0)
        std::copy_n (spectrumBuffer.data() + start2, size2, dest + size1);

    spectrumFifo.finishedRead (size1 + size2);
    return size1 + size2;
}

juce::AudioProcessorEditor* KompresorC3AudioProcessor::createEditor()
{
    return new KompresorC3AudioProcessorEditor (*this);
}

void KompresorC3AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void KompresorC3AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto flag = std::make_shared<std::atomic<bool>> (true);
        stateLoading = flag;
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        juce::MessageManager::callAsync ([flag] { flag->store (false); });
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KompresorC3AudioProcessor();
}
