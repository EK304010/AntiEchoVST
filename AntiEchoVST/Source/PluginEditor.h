#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

/** マイク入力と処理後の波形を並べて表示する、動作確認用のスコープ */
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

        // 中心線
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawLine (0, bounds.getHeight() * 0.5f, bounds.getWidth(), bounds.getHeight() * 0.5f);

        drawTrace (micBuf, juce::Colour (0xff8b949e), 0.85f); // マイク生音(グレー)
        drawTrace (outBuf, juce::Colour (0xff4fd1c5), 0.95f); // 処理後(アクセントカラー)
    }

private:
    void timerCallback() override { repaint(); }
    AntiEchoAudioProcessor& processor;
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
    void refreshDeviceList();

    AntiEchoAudioProcessor& processor;

    juce::Slider delaySlider, refGainSlider, micGainSlider;
    juce::Label  delayLabel, refGainLabel, micGainLabel, statusLabel, meterLabel, deviceLabel, scopeLegend;
    juce::ToggleButton bypassButton;
    juce::ComboBox deviceCombo;
    juce::TextButton refreshDevicesButton;
    ScopeComponent scope;

    std::vector<RenderDeviceInfo> deviceList;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> refGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> micGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AntiEchoAudioProcessorEditor)
};
