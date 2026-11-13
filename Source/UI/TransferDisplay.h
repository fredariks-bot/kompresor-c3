#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../PluginProcessor.h"
#include "CustomLookAndFeel.h"

class TransferDisplay : public juce::Component,
                        private juce::Timer
{
public:
    explicit TransferDisplay (KompresorC3AudioProcessor& processorRef)
        : proc (processorRef)
    {
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);
        fftData.assign ((size_t) fftSize * 2, 0.0f);
        spectrumDb.assign ((size_t) fftSize / 2 + 1, -90.0f);
        window.resize (fftSize);

        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f * (1.0f - std::cos (2.0 * juce::MathConstants<double>::pi * i / (fftSize - 1)));

        setOpaque (true);
        startTimerHz (30);
    }

    ~TransferDisplay() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll (KompresorLookAndFeel::panel.darker (0.15f));

        drawGrid (g, bounds);
        drawSpectrum (g, bounds);
        drawCurve (g, bounds);
        drawGainReduction (g, bounds);
        drawReadouts (g, bounds);
    }

private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;

    KompresorC3AudioProcessor& proc;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftData, window, samples;
    std::vector<float> spectrumDb;
    float grPeakHold = 0.0f;

    static float mapFreqToX (float freq, float width)
    {
        return width * (std::log (freq / 20.0f) / std::log (1000.0f));
    }

    static float mapDbToY (float db, float height)
    {
        return (1.0f - (db + 60.0f) / 60.0f) * height;
    }

    void timerCallback() override
    {
        pullSpectrum();
        grPeakHold = juce::jmax (grPeakHold * 0.97f, proc.grDb.load());
        repaint();
    }

    void pullSpectrum()
    {
        constexpr int chunk = 1024;
        float temp[chunk];
        const int got = proc.pullSpectrumSamples (temp, chunk);
        samples.insert (samples.end(), temp, temp + got);

        if ((int) samples.size() > fftSize)
            samples.erase (samples.begin(), samples.end() - fftSize);

        if ((int) samples.size() < fftSize)
            return;

        for (int i = 0; i < fftSize; ++i)
        {
            fftData[(size_t) i] = samples[(size_t) i] * window[(size_t) i];
            fftData[(size_t) (fftSize + i)] = 0.0f;
        }

        fft->performRealOnlyForwardTransform (fftData.data(), true);

        for (int i = 0; i <= fftSize / 2; ++i)
        {
            const float re = fftData[(size_t) (2 * i)];
            const float im = fftData[(size_t) (2 * i + 1)];
            const float mag = std::sqrt (re * re + im * im) * 2.0f / (float) fftSize;
            const float db = juce::jlimit (-90.0f, 0.0f, 20.0f * std::log10 (mag + 1.0e-7f));
            auto& smoothed = spectrumDb[(size_t) i];
            smoothed = juce::jmax (db, smoothed - 0.6f);
        }
    }

    void drawGrid (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.saveState();
        g.reduceClipRegion (bounds.toNearestInt());

        for (float db = -12.0f; db >= -60.0f; db -= 12.0f)
        {
            const auto y = bounds.getY() + mapDbToY (db, bounds.getHeight());
            g.setColour (KompresorLookAndFeel::line.withAlpha (0.6f));
            g.drawHorizontalLine ((int) y, bounds.getX(), bounds.getRight());
            g.drawVerticalLine ((int) (bounds.getX() + mapDbToY (db, bounds.getWidth())), bounds.getY(), bounds.getBottom());
        }

        for (float freq : { 100.0f, 1000.0f, 10000.0f })
        {
            const auto x = bounds.getX() + mapFreqToX (freq, bounds.getWidth());
            g.setColour (KompresorLookAndFeel::line.withAlpha (0.4f));
            g.drawVerticalLine ((int) x, bounds.getY(), bounds.getBottom());
        }

        g.restoreState();
    }

    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        const int numBins = fftSize / 2;
        juce::Path path;
        bool started = false;
        float lastX = bounds.getX();

        for (int i = 1; i <= numBins; ++i)
        {
            const float freq = (float) i * (float) proc.getSampleRate() / (float) fftSize;

            if (freq < 20.0f || freq > 20000.0f)
                continue;

            const auto x = bounds.getX() + mapFreqToX (freq, bounds.getWidth());
            const auto y = bounds.getBottom() - (spectrumDb[(size_t) i] + 90.0f) / 90.0f * bounds.getHeight();
            lastX = x;

            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }
        }

        if (! started)
            return;

        auto fillPath = path;
        fillPath.lineTo (lastX, bounds.getBottom());
        fillPath.lineTo (bounds.getX(), bounds.getBottom());
        fillPath.closeSubPath();

        g.setColour (KompresorLookAndFeel::accent.withAlpha (0.10f));
        g.fillPath (fillPath);
        g.setColour (KompresorLookAndFeel::accent.withAlpha (0.45f));
        g.strokePath (path, juce::PathStrokeType (1.2f));
    }

    void drawCurve (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        const float threshold = proc.apvts.getRawParameterValue ("threshold")->load();
        const float ratio     = proc.apvts.getRawParameterValue ("ratio")->load();
        const float knee      = proc.apvts.getRawParameterValue ("knee")->load();
        const float range     = proc.apvts.getRawParameterValue ("range")->load();

        juce::Path unity;
        unity.startNewSubPath (bounds.getX(), bounds.getY());
        unity.lineTo (bounds.getRight(), bounds.getBottom());
        g.setColour (KompresorLookAndFeel::textDim.withAlpha (0.25f));
        g.strokePath (unity, juce::PathStrokeType (1.0f));

        juce::Path curve;
        const int steps = (int) bounds.getWidth();

        for (int px = 0; px <= steps; ++px)
        {
            const float inDb = -60.0f + 60.0f * px / juce::jmax (1.0f, (float) steps);
            float outDb = inDb + kmp::computeGainDb (inDb, threshold, ratio, knee);
            outDb = juce::jmax (outDb, inDb - range);
            const auto x = bounds.getX() + bounds.getWidth() * px / juce::jmax (1.0f, (float) steps);
            const auto y = bounds.getY() + mapDbToY (outDb, bounds.getHeight());

            if (px == 0)
                curve.startNewSubPath (x, y);
            else
                curve.lineTo (x, y);
        }

        g.setColour (KompresorLookAndFeel::text.withAlpha (0.95f));
        g.strokePath (curve, juce::PathStrokeType (2.2f));

        const float inLevelDb = juce::jlimit (-60.0f, 0.0f, proc.inLevelDb.load());
        const float outDb = juce::jmax (inLevelDb + kmp::computeGainDb (inLevelDb, threshold, ratio, knee),
                                        inLevelDb - range);
        const auto dotX = bounds.getX() + bounds.getWidth() * (inLevelDb + 60.0f) / 60.0f;
        const auto dotY = bounds.getY() + mapDbToY (outDb, bounds.getHeight());

        g.setColour (KompresorLookAndFeel::accent.withAlpha (0.35f));
        g.fillEllipse (dotX - 9.0f, dotY - 9.0f, 18.0f, 18.0f);
        g.setColour (KompresorLookAndFeel::accent);
        g.fillEllipse (dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f);
    }

    void drawGainReduction (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        const auto barArea = bounds.removeFromRight (16.0f).reduced (3.0f, 6.0f);
        const float gr = proc.grDb.load();
        const float peak = juce::jmax (gr, grPeakHold);
        const auto grHeight = barArea.getHeight() * juce::jlimit (0.0f, 1.0f, gr / 24.0f);
        const auto peakY = barArea.getBottom() - barArea.getHeight() * juce::jlimit (0.0f, 1.0f, peak / 24.0f);

        g.setColour (KompresorLookAndFeel::line);
        g.fillRect (barArea);

        juce::ColourGradient gradient (KompresorLookAndFeel::grColour, barArea.getCentreX(), barArea.getBottom(),
                                       KompresorLookAndFeel::grColour.darker (0.4f), barArea.getCentreX(), barArea.getY(), false);
        g.setGradientFill (gradient);
        g.fillRect (barArea.getX(), barArea.getBottom() - grHeight, barArea.getWidth(), grHeight);

        g.setColour (KompresorLookAndFeel::text);
        g.fillRect (barArea.getX(), peakY - 1.0f, barArea.getWidth(), 2.0f);
    }

    void drawReadouts (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour (KompresorLookAndFeel::textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("dB", bounds.getX() + 6.0f, bounds.getY() + 4.0f, 40.0f, 16.0f, juce::Justification::topLeft);
        g.drawText ("in", bounds.getX() + 6.0f, bounds.getBottom() - 34.0f, 20.0f, 16.0f, juce::Justification::bottomLeft);
        g.drawText ("out", bounds.getRight() - 40.0f, bounds.getBottom() - 34.0f, 30.0f, 16.0f, juce::Justification::bottomRight);

        const float gr = proc.grDb.load();
        g.setColour (KompresorLookAndFeel::grColour);
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (14.0f, (float) getHeight() * 0.045f), juce::Font::bold)));
        g.drawText (gr < 0.05f ? "-" : juce::String (gr, 1) + " dB",
                    bounds.getX(), bounds.getY() + 4.0f, bounds.getWidth() - 28.0f, 22.0f,
                    juce::Justification::topRight);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransferDisplay)
};
