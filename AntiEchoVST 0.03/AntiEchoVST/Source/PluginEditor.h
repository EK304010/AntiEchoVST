#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

/** Live scope showing raw mic vs cancelled output waveforms, for visual confirmation. */
class ScopeComponent : public juce::Component,
                       private juce::Timer
{
public:
    explicit ScopeComponent (AntiEchoAudioProcessor& p) : processor (p)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d1117));
        g.fillRoundedRectangle (bounds, 6.0f);

        const int numPts = 512;
        float micBuf[numPts];
        float outBuf[numPts];
        processor.micScope.copyLatest (micBuf, numPts);
        processor.outScope.copyLatest (outBuf, numPts);

        auto drawTrace = [&] (const float* data, juce::Colour colour, float alpha)
        {
            juce::Path path;
            const float w = bounds.getWidth();
            const float h = bounds.getHeight();
            const float midY = h * 0.5f;

            for (int i = 0; i < numPts; ++i)
            {
                const float x = w * (float) i / (float) (numPts - 1);
                const float y = midY - data[i] * midY * 0.9f;
                if (i == 0) path.startNewSubPath (x, y);
                else path.lineTo (x, y);
            }
            g.setColour (colour.withAlpha (alpha));
            g.strokePath (path, juce::PathStrokeType (1.5f));
        };

        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawLine (0, bounds.getHeight() * 0.5f, bounds.getWidth(), bounds.getHeight() * 0.5f);

        drawTrace (micBuf, juce::Colour (0xff8b949e), 0.85f); // raw mic (grey)
        drawTrace (outBuf, juce::Colour (0xff4fd1c5), 0.95f); // cancelled output (teal)
    }

private:
    void timerCallback() override { repaint(); }
    AntiEchoAudioProcessor& processor;
};

/** A labelled box grouping related controls, drawn with a border + title. */
class GroupBox : public juce::Component
{
public:
    explicit GroupBox (const juce::String& titleText) : title (titleText) {}

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xff21262d));
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
        g.setColour (juce::Colour (0xff4fd1c5));
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText (title, getLocalBounds().reduced (10, 4).removeFromTop (18),
                    juce::Justification::left);
    }

private:
    juce::String title;
};

class AntiEchoAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit AntiEchoAudioProcessorEditor (AntiEchoAudioProcessor&);
    ~AntiEchoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshOutputDeviceList();
    void refreshInputDeviceList();

    AntiEchoAudioProcessor& processor;

    // --- Speaker (reference signal) group ---
    GroupBox speakerGroup { "SPEAKER (reference signal source)" };
    juce::ComboBox outputDeviceCombo;
    juce::TextButton refreshOutputButton { "Refresh" };
    juce::Label speakerStatusLabel;

    // --- Microphone (test mode) group ---
    GroupBox micGroup { "MICROPHONE (test mode only)" };
    juce::ComboBox inputDeviceCombo;
    juce::TextButton refreshInputButton { "Refresh" };
    juce::TextButton testModeButton { "Start Test" };
    juce::Label micStatusLabel;

    // --- Parameters ---
    juce::Slider delaySlider, refGainSlider, micGainSlider;
    juce::Label  delayLabel, refGainLabel, micGainLabel;
    juce::ToggleButton bypassButton { "Bypass" };

    // --- Scope / meters ---
    juce::Label scopeLegend;
    ScopeComponent scope;
    juce::Label meterLabel;

    std::vector<AudioDeviceInfo> outputDeviceList;
    std::vector<AudioDeviceInfo> inputDeviceList;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> refGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> micGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AntiEchoAudioProcessorEditor)
};
