#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "UI/CustomLookAndFeel.h"
#include "UI/TransferDisplay.h"

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
          const juce::String& name, const juce::String& suffix)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
        slider.setTextValueSuffix (suffix);
        addAndMakeVisible (slider);

        label.setText (name.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        label.setBounds (bounds.removeFromTop (16));
        slider.setBounds (bounds);
    }

    juce::Slider slider;
    juce::Label label;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class KompresorC3AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit KompresorC3AudioProcessorEditor (KompresorC3AudioProcessor& processorRef)
        : AudioProcessorEditor (processorRef), proc (processorRef)
    {
        setLookAndFeel (&lookAndFeel);

        display = std::make_unique<TransferDisplay> (proc);
        addAndMakeVisible (display.get());

        styleBox.addItemList (getStyleNames(), 1);
        addAndMakeVisible (styleBox);
        styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "style", styleBox);

        detectorBox.addItemList ({ "Peak", "RMS" }, 1);
        addAndMakeVisible (detectorBox);
        detectorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "detectorMode", detectorBox);

        auto makeLabel = [this] (const juce::String& text)
        {
            auto label = std::make_unique<juce::Label> (juce::String(), text);
            label->setJustificationType (juce::Justification::centredLeft);
            label->setColour (juce::Label::textColourId, KompresorLookAndFeel::textDim);
            addAndMakeVisible (label.get());
            labels.push_back (std::move (label));
        };

        makeLabel ("STYLE");
        makeLabel ("DETECTOR");

        auto addToggle = [this] (const char* paramId, const juce::String& text)
        {
            auto button = std::make_unique<juce::ToggleButton> (text);
            addAndMakeVisible (button.get());
            buttonAttachments.push_back (
                std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, paramId, *button));
            toggles.push_back (std::move (button));
        };

        addToggle ("scExternal", "EXT SC");
        addToggle ("hpfOn", "SC HPF");
        addToggle ("autoRelease", "AUTO REL");

        auto addKnob = [this] (const char* paramId, const juce::String& name, const juce::String& suffix)
        {
            auto knob = std::make_unique<Knob> (proc.apvts, paramId, name, suffix);
            addAndMakeVisible (knob.get());
            knobs.push_back (std::move (knob));
        };

        addKnob ("threshold", "Threshold", " dB");
        addKnob ("ratio",     "Ratio",     " : 1");
        addKnob ("attack",    "Attack",    " ms");
        addKnob ("release",   "Release",   " ms");
        addKnob ("knee",      "Knee",      " dB");
        addKnob ("range",     "Range",     " dB");
        addKnob ("makeup",    "Makeup",    " dB");
        addKnob ("mix",       "Mix",       " %");
        addKnob ("lookahead", "Lookahead", " ms");
        addKnob ("hpfFreq",   "SC HPF",    " Hz");

        setSize (980, 600);
        setResizable (false, false);
    }

    ~KompresorC3AudioProcessorEditor() override
    {
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (KompresorLookAndFeel::background);

        g.setColour (KompresorLookAndFeel::text);
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawText ("KOMPRESOR C3", 16.0f, (float) getHeight() - 30.0f, 200.0f, 20.0f, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (16);
        display->setBounds (bounds.removeFromTop (330));

        auto controlRow = bounds.removeFromTop (52).reduced (0, 8);

        auto styleArea = controlRow.removeFromLeft (160);
        labels[0]->setBounds (styleArea.removeFromLeft (62));
        styleBox.setBounds (styleArea.withTrimmedRight (8));

        auto detectorArea = controlRow.removeFromLeft (160);
        labels[1]->setBounds (detectorArea.removeFromLeft (76));
        detectorBox.setBounds (detectorArea.withTrimmedRight (8));

        for (auto& toggle : toggles)
            toggle->setBounds (controlRow.removeFromLeft (100).reduced (4, 2));

        auto knobArea = bounds.removeFromTop (176);
        const int cellWidth = knobArea.getWidth() / (int) knobs.size();

        for (auto& knob : knobs)
            knob->setBounds (knobArea.removeFromLeft (cellWidth).reduced (4, 6));
    }

private:
    static juce::StringArray getStyleNames()
    {
        return { "Clean", "Vocal", "Punch", "Bus", "Master", "Opto", "Pump", "Limit" };
    }

    KompresorC3AudioProcessor& proc;
    KompresorLookAndFeel lookAndFeel;

    std::unique_ptr<TransferDisplay> display;
    juce::ComboBox styleBox, detectorBox;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<Knob>> knobs;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttachment, detectorAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KompresorC3AudioProcessorEditor)
};
