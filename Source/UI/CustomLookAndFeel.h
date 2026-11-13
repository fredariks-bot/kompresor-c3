#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class KompresorLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static const juce::Colour background;
    static const juce::Colour panel;
    static const juce::Colour line;
    static const juce::Colour text;
    static const juce::Colour textDim;
    static const juce::Colour accent;
    static const juce::Colour accentDim;
    static const juce::Colour grColour;

    KompresorLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::buttonColourId, panel);
        setColour (juce::ComboBox::arrowColourId, textDim);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDim);
        setColour (juce::PopupMenu::highlightedTextColourId, text);
        setColour (juce::ToggleButton::textColourId, text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const auto cx = bounds.getCentreX();
        const auto cy = bounds.getCentreY();
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 6.0f;
        const float trackWidth = 4.0f;

        juce::Path track;
        track.addCentredArc (cx, cy, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (line);
        g.strokePath (track, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (sliderPos > 0.001f)
        {
            juce::Path value;
            value.addCentredArc (cx, cy, radius, radius, 0.0f, rotaryStartAngle,
                                 rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * sliderPos, true);
            g.setColour (accent);
            g.strokePath (value, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const auto bodyRadius = radius - trackWidth - 3.0f;

        g.setGradientFill (juce::ColourGradient (panel.brighter (0.12f), cx, cy - bodyRadius,
                                                 panel.darker (0.25f), cx, cy + bodyRadius, false));
        g.fillEllipse (cx - bodyRadius, cy - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
        g.setColour (line);
        g.drawEllipse (cx - bodyRadius, cy - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.0f);

        const auto angle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * sliderPos;
        juce::Path pointer;
        pointer.startNewSubPath (cx, cy - bodyRadius * 0.25f);
        pointer.lineTo (cx, cy - bodyRadius * 0.85f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle, cx, cy));
        g.setColour (text);
        g.strokePath (pointer, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = button.getToggleState();
        const float corner = 4.0f;

        g.setColour (on ? accentDim : panel);
        g.fillRoundedRectangle (bounds, corner);
        g.setColour (on ? accent : line);
        g.drawRoundedRectangle (bounds, corner, 1.2f);

        g.setColour (on ? text : textDim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
        juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
        const float corner = 4.0f;

        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, corner);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, corner, 1.2f);

        juce::Path arrow;
        const auto ax = bounds.getRight() - 14.0f;
        const auto ay = bounds.getCentreY();
        arrow.startNewSubPath (ax - 4.0f, ay - 3.0f);
        arrow.lineTo (ax + 4.0f, ay - 3.0f);
        arrow.lineTo (ax, ay + 3.0f);
        arrow.closeSubPath();
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (14.0f)); }
    juce::Font getLabelFont (juce::Label&) override { return juce::Font (juce::FontOptions (12.0f)); }
};

const juce::Colour KompresorLookAndFeel::background { 0xff121518 };
const juce::Colour KompresorLookAndFeel::panel      { 0xff1d2126 };
const juce::Colour KompresorLookAndFeel::line       { 0xff2e343b };
const juce::Colour KompresorLookAndFeel::text       { 0xffe8ecef };
const juce::Colour KompresorLookAndFeel::textDim    { 0xff8a939c };
const juce::Colour KompresorLookAndFeel::accent     { 0xff3fc1c9 };
const juce::Colour KompresorLookAndFeel::accentDim  { 0xff1d4a4e };
const juce::Colour KompresorLookAndFeel::grColour   { 0xff3fc1c9 };
