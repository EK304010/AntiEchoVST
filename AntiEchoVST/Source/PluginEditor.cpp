#include "PluginEditor.h"

namespace
{
    // 日本語文字列は必ずこの関数経由で juce::String 化する(文字化け対策)。
    // u8"" はC++コンパイラのソースファイル文字コード設定に関わらず常にUTF-8バイト列を
    // 保証するため、CharPointer_UTF8でラップして渡せば確実にUTF-8として解釈される。
    juce::String jtr (const char* utf8Text)
    {
        return juce::String (juce::CharPointer_UTF8 (utf8Text));
    }

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
    : juce::AudioProcessorEditor (&p), processor (p), scope (p)
{
    setupSlider (delaySlider,   delayLabel,   *this, jtr (u8"Delay (ms) \xe9\x81\x85\xe5\xbb\xb6"));
    setupSlider (refGainSlider, refGainLabel, *this, jtr (u8"\xe9\x80\x86\xe4\xbd\x8d\xe7\x9b\xb8\xe3\x82\xb2\xe3\x82\xa4\xe3\x83\xb3 (Anti-phase Gain)"));
    setupSlider (micGainSlider, micGainLabel, *this, jtr (u8"\xe3\x83\x9e\xe3\x82\xa4\xe3\x82\xafGain (Mic Gain)"));

    bypassButton.setButtonText (jtr (u8"\xe3\x83\x90\xe3\x82\xa4\xe3\x83\x91\xe3\x82\xb9 (Bypass)"));
    addAndMakeVisible (bypassButton);

    deviceLabel.setText (jtr (u8"\xe5\x8f\x82\xe7\x85\xa7\xe4\xbf\xa1\xe5\x8f\xb7\xe3\x81\xab\xe4\xbd\xbf\xe3\x81\x86\xe5\x87\xba\xe5\x8a\x9b\xe3\x83\x87\xe3\x83\x90\xe3\x82\xa4\xe3\x82\xb9(\xe3\x82\xb9\xe3\x83\x94\xe3\x83\xbc\xe3\x82\xab\xe3\x83\xbc/\xe3\x83\x98\xe3\x83\x83\xe3\x83\x89\xe3\x83\x9b\xe3\x83\xb3)"), juce::dontSendNotification);
    addAndMakeVisible (deviceLabel);

    addAndMakeVisible (deviceCombo);
    deviceCombo.onChange = [this]
    {
        const int idx = deviceCombo.getSelectedId() - 1;
        if (idx == 0)
            processor.setOutputDevice ({}); // 先頭は「既定のデバイス」
        else if (idx > 0 && (size_t) (idx - 1) < deviceList.size())
            processor.setOutputDevice (deviceList[(size_t) (idx - 1)].id);
    };

    refreshDevicesButton.setButtonText (jtr (u8"\xe6\x9b\xb4\xe6\x96\xb0 (Refresh)"));
    refreshDevicesButton.onClick = [this] { refreshDeviceList(); };
    addAndMakeVisible (refreshDevicesButton);

    scopeLegend.setText (jtr (u8"\xe7\x81\xb0\xe8\x89\xb2=\xe3\x83\x9e\xe3\x82\xa4\xe3\x82\xaf\xe7\x94\x9f\xe9\x9f\xb3\xe3\x80\x80\xe6\xb0\xb4\xe8\x89\xb2=\xe5\x87\xa6\xe7\x90\x86\xe5\xbe\x8c(\xe3\x82\xad\xe3\x83\xa3\xe3\x83\xb3\xe3\x82\xbb\xe3\x83\xab\xe5\xbe\x8c)"), juce::dontSendNotification);
    scopeLegend.setFont (juce::Font (11.0f));
    scopeLegend.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (scopeLegend);

    addAndMakeVisible (scope);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (meterLabel);

    statusLabel.setJustificationType (juce::Justification::left);
    meterLabel.setJustificationType (juce::Justification::left);

    auto& apvts = processor.apvts;
    delayAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "delayMs", delaySlider);
    refGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "refGain", refGainSlider);
    micGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "micGain", micGainSlider);
    bypassAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "bypass", bypassButton);

    refreshDeviceList();

    setSize (460, 460);
    startTimerHz (10);
}

AntiEchoAudioProcessorEditor::~AntiEchoAudioProcessorEditor()
{
    stopTimer();
}

void AntiEchoAudioProcessorEditor::refreshDeviceList()
{
    deviceList = processor.getAvailableOutputDevices();

    deviceCombo.clear (juce::dontSendNotification);
    deviceCombo.addItem (jtr (u8"\xe6\x97\xa2\xe5\xae\x9a\xe3\x81\xae\xe3\x83\x87\xe3\x83\x90\xe3\x82\xa4\xe3\x82\xb9 (Default)"), 1);

    int idToSelect = 1;
    const auto currentId = processor.getSelectedDeviceId();

    for (size_t i = 0; i < deviceList.size(); ++i)
    {
        const int itemId = (int) i + 2; // 1は既定デバイス用に予約
        deviceCombo.addItem (deviceList[i].name, itemId);
        if (currentId.isNotEmpty() && deviceList[i].id == currentId)
            idToSelect = itemId;
    }

    deviceCombo.setSelectedId (idToSelect, juce::dontSendNotification);
}

void AntiEchoAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff161b22));
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText (jtr (u8"AntiEcho VST - \xe9\x80\x86\xe4\xbd\x8d\xe7\x9b\xb8\xe3\x83\x8e\xe3\x82\xa4\xe3\x82\xba\xe3\x82\xad\xe3\x83\xa3\xe3\x83\xb3\xe3\x82\xbb\xe3\x83\xab"),
                        getLocalBounds().removeFromTop (32), juce::Justification::centred, 1);
}

void AntiEchoAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (32); // title
    area.removeFromTop (8);

    auto row = [&area] (int h) { return area.removeFromTop (h); };

    // --- デバイス選択 ---
    deviceLabel.setBounds (row (16));
    {
        auto r = row (26);
        refreshDevicesButton.setBounds (r.removeFromRight (72));
        r.removeFromRight (6);
        deviceCombo.setBounds (r);
    }
    area.removeFromTop (10);

    // --- パラメータ ---
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

    // --- 動作確認用スコープ(実際に処理できているか確認する窓) ---
    scopeLegend.setBounds (row (14));
    scope.setBounds (row (110));
    area.removeFromTop (8);

    statusLabel.setBounds (row (18));
    meterLabel.setBounds (row (18));
}

void AntiEchoAudioProcessorEditor::timerCallback()
{
    const bool ok = processor.loopbackOk.load();
    const juce::String devName = processor.getCurrentDeviceName();

    statusLabel.setText (
        jtr (u8"\xe5\x8f\x82\xe7\x85\xa7\xe4\xbf\xa1\xe5\x8f\xb7: ") + (ok ? devName : jtr (u8"\xe5\x8f\x96\xe5\xbe\x97\xe5\xa4\xb1\xe6\x95\x97 (Windows\xe4\xbb\xa5\xe5\xa4\x96 or \xe5\x88\x9d\xe6\x9c\x9f\xe5\x8c\x96\xe5\xa4\xb1\xe6\x95\x97)")),
        juce::dontSendNotification);

    const float mic = processor.micLevel.load();
    const float out = processor.outLevel.load();
    meterLabel.setText (juce::String::formatted ("mic RMS: %.3f   out RMS: %.3f", mic, out),
                         juce::dontSendNotification);
}
