#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

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

    AntiEchoAudioProcessor& processor;

    juce::Slider delaySlider, refGainSlider, micGainSlider;
    juce::Label  delayLabel, refGainLabel, micGainLabel, statusLabel, meterLabel;
    juce::ToggleButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> refGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> micGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AntiEchoAudioProcessorEditor)
};
