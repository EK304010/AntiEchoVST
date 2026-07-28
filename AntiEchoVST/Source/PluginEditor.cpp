#include "PluginEditor.h"

namespace
{
    void setupSlider (juce::Slider& s, juce::Label& l, juce::Component& parent, const juce::String& text)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
        parent.addAndMakeVisible (s);

        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::left);
        parent.addAndMakeVisible (l);
    }
}

AntiEchoAudioProcessorEditor::AntiEchoAudioProcessorEditor (AntiEchoAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setupSlider (delaySlider,   delayLabel,   *this, "Delay (ms)");
    setupSlider (refGainSlider, refGainLabel, *this, "Anti-phase Gain");
    setupSlider (micGainSlider, micGainLabel, *this, "Mic Gain");

    addAndMakeVisible (bypassButton);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (meterLabel);

    statusLabel.setJustificationType (juce::Justification::left);
    meterLabel.setJustificationType (juce::Justification::left);

    auto& apvts = processor.apvts;
    delayAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "delayMs", delaySlider);
    refGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "refGain", refGainSlider);
    micGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "micGain", micGainSlider);
    bypassAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "bypass", bypassButton);

    setSize (420, 260);
    startTimerHz (10);
}

AntiEchoAudioProcessorEditor::~AntiEchoAudioProcessorEditor()
{
    stopTimer();
}

void AntiEchoAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff161b22));
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText ("AntiEcho VST - \xe9\x80\x86\xe4\xbd\x8d\xe7\x9b\xb8\xe3\x83\x8e\xe3\x82\xa4\xe3\x82\xba\xe3\x82\xad\xe3\x83\xa3\xe3\x83\xb3\xe3\x82\xbb\xe3\x83\xab",
                        getLocalBounds().removeFromTop (36), juce::Justification::centred, 1);
}

void AntiEchoAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (36); // title

    auto row = [&area] (int h) { return area.removeFromTop (h); };

    delayLabel.setBounds (row (18));
    delaySlider.setBounds (row (28));
    area.removeFromTop (6);

    refGainLabel.setBounds (row (18));
    refGainSlider.setBounds (row (28));
    area.removeFromTop (6);

    micGainLabel.setBounds (row (18));
    micGainSlider.setBounds (row (28));
    area.removeFromTop (10);

    bypassButton.setBounds (row (24));
    area.removeFromTop (6);
    statusLabel.setBounds (row (20));
    meterLabel.setBounds (row (20));
}

void AntiEchoAudioProcessorEditor::timerCallback()
{
    const bool ok = processor.loopbackOk.load();
    statusLabel.setText (juce::String ("Loopback reference: ") + (ok ? "OK" : "NG (Windows\xe4\xbb\xa5\xe5\xa4\x96 or \xe5\x88\x9d\xe6\x9c\x9f\xe5\x8c\x96\xe5\xa4\xb1\xe6\x95\x97)"),
                         juce::dontSendNotification);

    const float mic = processor.micLevel.load();
    const float out = processor.outLevel.load();
    meterLabel.setText (juce::String::formatted ("mic RMS: %.3f   out RMS: %.3f", mic, out),
                         juce::dontSendNotification);
}
