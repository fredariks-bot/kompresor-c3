#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <cstdio>

#include "../Source/PluginProcessor.h"

static int failures = 0;

static void check (const char* name, bool ok)
{
    std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", name);

    if (! ok)
        ++failures;
}

static void fillSine (juce::AudioBuffer<float>& buf, int startChannel, int numChannels, double sr)
{
    for (int c = 0; c < numChannels; ++c)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            buf.setSample (startChannel + c, i, 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * i / sr));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    {
        KompresorC3AudioProcessor proc;
        proc.setRateAndBufferSizeDetails (48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        juce::MidiBuffer midi;

        // 1) stereo main, sidechain bus disabled (default)
        {
            juce::AudioBuffer<float> buf (2, 512);
            fillSine (buf, 0, 2, 48000.0);
            proc.processBlock (buf, midi);

            float maxAbs = 0.0f;

            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    maxAbs = std::max (maxAbs, std::abs (buf.getSample (c, i)));

            check ("stereo processing produces output", maxAbs > 0.01f);
        }

        // 2) sidechain input bus enabled
        {
            auto layouts = proc.getBusesLayout();
            layouts.inputBuses.set (1, juce::AudioChannelSet::stereo());
            check ("sidechain layout accepted", proc.setBusesLayout (layouts));

            juce::AudioBuffer<float> buf (4, 512);
            fillSine (buf, 0, 2, 48000.0);
            fillSine (buf, 2, 2, 48000.0);
            proc.processBlock (buf, midi);
            check ("processing with active sidechain bus", true);
        }

        // 3) sidechain enabled but external SC off again, mono main
        {
            auto layouts = proc.getBusesLayout();
            layouts.inputBuses.set (0, juce::AudioChannelSet::mono());
            layouts.inputBuses.set (1, juce::AudioChannelSet::mono());
            layouts.outputBuses.set (0, juce::AudioChannelSet::mono());
            check ("mono layout accepted", proc.setBusesLayout (layouts));

            juce::AudioBuffer<float> buf (2, 512);
            fillSine (buf, 0, 1, 48000.0);
            fillSine (buf, 1, 1, 48000.0);
            proc.processBlock (buf, midi);
            check ("mono processing with sidechain", true);
        }

        // 4) zero-sample buffer
        {
            juce::AudioBuffer<float> buf (2, 0);
            proc.processBlock (buf, midi);
            check ("zero-sample buffer survives", true);
        }

        // 5) state roundtrip
        {
            juce::MemoryBlock mb;
                proc.getStateInformation (mb);
            check ("state saved", mb.getSize() > 0);
                proc.setStateInformation (mb.getData(), (int) mb.getSize());
                check ("state reloaded", true);
        }

        // 6) editor can be created and destroyed headless (no window shown)
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            check ("editor created", ed != nullptr);

            if (ed != nullptr)
            {
                ed->setSize (980, 600);
                juce::Image img (juce::Image::ARGB, 980, 600, true);
                juce::Graphics g (img);
                ed->paint (g);
                check ("editor paints offscreen", true);
            }
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL HOST TESTS PASSED" : "HOST TESTS FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
