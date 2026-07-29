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
        l.setFont (juce::Font (12.0f));
        parent.addAndMakeVisible (l);
    }
}

AntiEchoAudioProcessorEditor::AntiEchoAudioProcessorEditor (AntiEchoAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p), scope (p)
{
    // --- Speaker group ---
    addAndMakeVisible (speakerGroup);
    addAndMakeVisible (outputDeviceCombo);
    outputDeviceCombo.onChange = [this]
    {
        const int idx = outputDeviceCombo.getSelectedId() - 1;
        if (idx == 0)
            processor.setOutputDevice ({});
        else if (idx > 0 && (size_t) (idx - 1) < outputDeviceList.size())
            processor.setOutputDevice (outputDeviceList[(size_t) (idx - 1)].id);
    };
    refreshOutputButton.onClick = [this] { refreshOutputDeviceList(); };
    addAndMakeVisible (refreshOutputButton);
    speakerStatusLabel.setFont (juce::Font (11.0f));
    speakerStatusLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (speakerStatusLabel);

    // --- Microphone group ---
    addAndMakeVisible (micGroup);
    addAndMakeVisible (inputDeviceCombo);
    inputDeviceCombo.onChange = [this]
    {
        const int idx = inputDeviceCombo.getSelectedId() - 1;
        if (idx == 0)
            processor.setInputDevice ({});
        else if (idx > 0 && (size_t) (idx - 1) < inputDeviceList.size())
            processor.setInputDevice (inputDeviceList[(size_t) (idx - 1)].id);
    };
    refreshInputButton.onClick = [this] { refreshInputDeviceList(); };
    addAndMakeVisible (refreshInputButton);

    testModeButton.setClickingTogglesState (true);
    testModeButton.onClick = [this]
    {
        const bool nowOn = testModeButton.getToggleState();
        processor.setTestModeEnabled (nowOn);
        testModeButton.setButtonText (nowOn ? "Stop Test" : "Start Test");
        testModeButton.setColour (juce::TextButton::buttonColourId,
                                   nowOn ? juce::Colour (0xffff6b6b) : juce::Colour (0xff30363d));
    };
    addAndMakeVisible (testModeButton);

    micStatusLabel.setFont (juce::Font (11.0f));
    micStatusLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (micStatusLabel);

    // --- Parameters ---
    setupSlider (delaySlider,   delayLabel,   *this, "Delay (ms)");
    setupSlider (refGainSlider, refGainLabel, *this, "Anti-phase Gain");
    setupSlider (micGainSlider, micGainLabel, *this, "Mic Gain");
    addAndMakeVisible (bypassButton);

    // --- Scope ---
    scopeLegend.setText ("grey = raw mic input   |   teal = after cancellation", juce::dontSendNotification);
    scopeLegend.setFont (juce::Font (11.0f));
    scopeLegend.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (scopeLegend);
    addAndMakeVisible (scope);

    meterLabel.setFont (juce::Font (12.0f));
    addAndMakeVisible (meterLabel);

    auto& apvts = processor.apvts;
    delayAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "delayMs", delaySlider);
    refGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "refGain", refGainSlider);
    micGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "micGain", micGainSlider);
    bypassAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "bypass", bypassButton);

    refreshOutputDeviceList();
    refreshInputDeviceList();
    testModeButton.setToggleState (processor.isTestModeEnabled(), juce::dontSendNotification);
    testModeButton.setButtonText (processor.isTestModeEnabled() ? "Stop Test" : "Start Test");

    setSize (460, 560);
    startTimerHz (10);
}

AntiEchoAudioProcessorEditor::~AntiEchoAudioProcessorEditor()
{
    stopTimer();
}

void AntiEchoAudioProcessorEditor::refreshOutputDeviceList()
{
    outputDeviceList = processor.getAvailableOutputDevices();

    outputDeviceCombo.clear (juce::dontSendNotification);
    outputDeviceCombo.addItem ("Default playback device", 1);

    int idToSelect = 1;
    const auto currentId = processor.getSelectedDeviceId();

    for (size_t i = 0; i < outputDeviceList.size(); ++i)
    {
        const int itemId = (int) i + 2;
        outputDeviceCombo.addItem (outputDeviceList[i].name, itemId);
        if (currentId.isNotEmpty() && outputDeviceList[i].id == currentId)
            idToSelect = itemId;
    }

    outputDeviceCombo.setSelectedId (idToSelect, juce::dontSendNotification);
}

void AntiEchoAudioProcessorEditor::refreshInputDeviceList()
{
    inputDeviceList = processor.getAvailableInputDevices();

    inputDeviceCombo.clear (juce::dontSendNotification);
    inputDeviceCombo.addItem ("Default recording device", 1);

    int idToSelect = 1;
    const auto currentId = processor.getSelectedInputDeviceId();

    for (size_t i = 0; i < inputDeviceList.size(); ++i)
    {
        const int itemId = (int) i + 2;
        inputDeviceCombo.addItem (inputDeviceList[i].name, itemId);
        if (currentId.isNotEmpty() && inputDeviceList[i].id == currentId)
            idToSelect = itemId;
    }

    inputDeviceCombo.setSelectedId (idToSelect, juce::dontSendNotification);
}

void AntiEchoAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff161b22));
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText ("AntiEcho VST", getLocalBounds().removeFromTop (32), juce::Justification::centred, 1);
}

void AntiEchoAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (32); // title
    area.removeFromTop (8);

    auto row = [&area] (int h) { return area.removeFromTop (h); };

    // --- Speaker group box ---
    {
        auto boxArea = row (78);
        speakerGroup.setBounds (boxArea);
        auto inner = boxArea.reduced (10, 6);
        inner.removeFromTop (16); // space for the group title drawn by GroupBox
        {
            auto r = inner.removeFromTop (26);
            refreshOutputButton.setBounds (r.removeFromRight (72));
            r.removeFromRight (6);
            outputDeviceCombo.setBounds (r);
        }
        inner.removeFromTop (4);
        speakerStatusLabel.setBounds (inner.removeFromTop (16));
    }
    area.removeFromTop (10);

    // --- Microphone group box ---
    {
        auto boxArea = row (100);
        micGroup.setBounds (boxArea);
        auto inner = boxArea.reduced (10, 6);
        inner.removeFromTop (16);
        {
            auto r = inner.removeFromTop (26);
            refreshInputButton.setBounds (r.removeFromRight (72));
            r.removeFromRight (6);
            inputDeviceCombo.setBounds (r);
        }
        inner.removeFromTop (4);
        testModeButton.setBounds (inner.removeFromTop (26));
        inner.removeFromTop (4);
        micStatusLabel.setBounds (inner.removeFromTop (16));
    }
    area.removeFromTop (12);

    // --- Parameters ---
    delayLabel.setBounds (row (16));
    delaySlider.setBounds (row (26));
    area.removeFromTop (6);

    refGainLabel.setBounds (row (16));
    refGainSlider.setBounds (row (26));
    area.removeFromTop (6);

    micGainLabel.setBounds (row (16));
    micGainSlider.setBounds (row (26));
    area.removeFromTop (8);

    bypassButton.setBounds (row (22));
    area.removeFromTop (8);

    // --- Scope ---
    scopeLegend.setBounds (row (14));
    scope.setBounds (row (110));
    area.removeFromTop (8);
    meterLabel.setBounds (row (18));
}

void AntiEchoAudioProcessorEditor::timerCallback()
{
    const bool refOk = processor.referenceOk.load();
    speakerStatusLabel.setText (
        juce::String ("Status: ") + (refOk ? ("connected to " + processor.getCurrentOutputDeviceName())
                                            : "not connected (Windows only, or failed to start)"),
        juce::dontSendNotification);

    const bool micOk = processor.micTestOk.load();
    const bool testOn = processor.isTestModeEnabled();
    micStatusLabel.setText (
        testOn ? (juce::String ("Status: ") + (micOk ? ("listening to " + processor.getCurrentInputDeviceName())
                                                       : "failed to start"))
               : "Status: idle (press Start Test to check if cancellation is actually working)",
        juce::dontSendNotification);

    const float mic = processor.micLevel.load();
    const float out = processor.outLevel.load();
    meterLabel.setText (juce::String::formatted ("mic RMS: %.3f    out RMS: %.3f", mic, out),
                         juce::dontSendNotification);
}
