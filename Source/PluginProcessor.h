#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/CompressorCore.h"

class KompresorC3AudioProcessor : public juce::AudioProcessor,
                                  private juce::AudioProcessorValueTreeState::Listener
{
public:
    KompresorC3AudioProcessor();
    ~KompresorC3AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }
    const juce::String getName() const override            { return JucePlugin_Name; }
    bool acceptsMidi() const override                       { return false; }
    bool producesMidi() const override                      { return false; }
    bool isMidiEffect() const override                      { return false; }
    double getTailLengthSeconds() const override            { return 0.0; }

    int getNumPrograms() override                           { return 1; }
    int getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                   {}
    const juce::String getProgramName (int) override        { return "Kompressor K3"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> grDb       { 0.0f };
    std::atomic<float> grPeakDb   { 0.0f };
    std::atomic<float> inLevelDb  { -100.0f };
    std::atomic<float> outLevelDb { -100.0f };

    static constexpr int spectrumFifoSize = 16384;

    void pushSpectrumSamples (const float* samples, int num);
    int pullSpectrumSamples (float* dest, int maxSamples);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void applyStylePreset (int styleIndex);

    kmp::Compressor compressor;
    float peakHoldDb = 0.0f;
    std::shared_ptr<std::atomic<bool>> stateLoading;

    juce::AbstractFifo spectrumFifo { spectrumFifoSize };
    std::vector<float> spectrumBuffer = std::vector<float> ((size_t) spectrumFifoSize);

    std::atomic<float>* thresholdParam  = nullptr;
    std::atomic<float>* ratioParam      = nullptr;
    std::atomic<float>* attackParam     = nullptr;
    std::atomic<float>* releaseParam    = nullptr;
    std::atomic<float>* kneeParam       = nullptr;
    std::atomic<float>* rangeParam      = nullptr;
    std::atomic<float>* makeupParam     = nullptr;
    std::atomic<float>* mixParam        = nullptr;
    std::atomic<float>* lookaheadParam  = nullptr;
    std::atomic<float>* hpfFreqParam    = nullptr;
    std::atomic<float>* hpfOnParam      = nullptr;
    std::atomic<float>* detectorModeParam = nullptr;
    std::atomic<float>* autoRelParam    = nullptr;
    std::atomic<float>* scExtParam      = nullptr;
    std::atomic<float>* stereoLinkParam = nullptr;
    std::atomic<float>* styleParam      = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KompresorC3AudioProcessor)
};
