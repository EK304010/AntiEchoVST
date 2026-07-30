#include "PluginEditor.h"

namespace
{
    void setupHSlider (juce::Slider& s, juce::Label& l, juce::Component& parent, const juce::String& text)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 18);
        parent.addAndMakeVisible (s);

        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font (11.0f));
        parent.addAndMakeVisible (l);
    }
}

SimpleCompressorAudioProcessorEditor::SimpleCompressorAudioProcessorEditor (SimpleCompressorAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p), spectrum (p)
{
    auto& apvts = processor.apvts;

    // ================= I/O group =================
    addAndMakeVisible (ioGroup);
    setupHSlider (inputGainSlider, inputGainLabel, *this, "Input Gain (dB)");
    setupHSlider (outputVolumeSlider, outputVolumeLabel, *this, "Output Volume (dB)");
    setupHSlider (mixSlider, mixLabel, *this, "Mix (%)");
    addAndMakeVisible (safetyLimiterButton);
    addAndMakeVisible (bypassButton);
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);
    inputMeterLabel.setText ("In", juce::dontSendNotification);
    outputMeterLabel.setText ("Out", juce::dontSendNotification);
    inputMeterLabel.setFont (juce::Font (10.0f));
    outputMeterLabel.setFont (juce::Font (10.0f));
    addAndMakeVisible (inputMeterLabel);
    addAndMakeVisible (outputMeterLabel);
    inputMeter.setRange (-60.0f, 6.0f);
    outputMeter.setRange (-60.0f, 6.0f);

    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "inputGain", inputGainSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "outputVolume", outputVolumeSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "mix", mixSlider));
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "safetyLimiter", safetyLimiterButton));
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "bypass", bypassButton));

    // ================= Dynamics group =================
    addAndMakeVisible (dynamicsGroup);
    setupHSlider (thresholdSlider, thresholdLabel, *this, "Threshold (dB)");
    setupHSlider (ratioSlider, ratioLabel, *this, "Ratio");
    setupHSlider (kneeSlider, kneeLabel, *this, "Knee (dB)");
    setupHSlider (attackSlider, attackLabel, *this, "Attack (ms)");
    setupHSlider (releaseSlider, releaseLabel, *this, "Release (ms)");
    setupHSlider (makeupSlider, makeupLabel, *this, "Makeup Trim (dB)");
    addAndMakeVisible (autoMakeupButton);
    addAndMakeVisible (grMeter);
    grMeterLabel.setText ("Gain Reduction", juce::dontSendNotification);
    grMeterLabel.setFont (juce::Font (10.0f));
    addAndMakeVisible (grMeterLabel);
    grMeter.setIsReduction (true);
    grMeter.setRange (-24.0f, 0.0f);

    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "threshold", thresholdSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "ratio", ratioSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "knee", kneeSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "attack", attackSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "release", releaseSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "makeup", makeupSlider));
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "autoMakeup", autoMakeupButton));

    // ================= EQ group =================
    addAndMakeVisible (eqGroup);
    addAndMakeVisible (eqBypassButton);
    addAndMakeVisible (eqPositionCombo);
    eqPositionCombo.addItem ("EQ: Pre-Comp", 1);
    eqPositionCombo.addItem ("EQ: Post-Comp", 2);
    eqPositionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "eqPosition", eqPositionCombo);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "eqBypass", eqBypassButton));

    auto setupBandLabel = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font (11.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, juce::Colour (0xff555555));
        addAndMakeVisible (l);
    };
    setupBandLabel (lsLabel, "Low Shelf");
    setupBandLabel (p1Label, "Peak 1");
    setupBandLabel (p2Label, "Peak 2");
    setupBandLabel (hsLabel, "High Shelf");

    setupHSlider (lsFreqSlider, lsFreqCap, *this, "Freq (Hz)");
    setupHSlider (lsGainSlider, lsGainCap, *this, "Gain (dB)");
    setupHSlider (p1FreqSlider, p1FreqCap, *this, "Freq (Hz)");
    setupHSlider (p1GainSlider, p1GainCap, *this, "Gain (dB)");
    setupHSlider (p1QSlider,    p1QCap,    *this, "Q");
    setupHSlider (p2FreqSlider, p2FreqCap, *this, "Freq (Hz)");
    setupHSlider (p2GainSlider, p2GainCap, *this, "Gain (dB)");
    setupHSlider (p2QSlider,    p2QCap,    *this, "Q");
    setupHSlider (hsFreqSlider, hsFreqCap, *this, "Freq (Hz)");
    setupHSlider (hsGainSlider, hsGainCap, *this, "Gain (dB)");

    addAndMakeVisible (spectrum);

    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "lsFreq", lsFreqSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "lsGain", lsGainSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "p1Freq", p1FreqSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "p1Gain", p1GainSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "p1Q", p1QSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "p2Freq", p2FreqSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "p2Gain", p2GainSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "p2Q", p2QSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "hsFreq", hsFreqSlider));
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "hsGain", hsGainSlider));

    setSize (620, 815);
    startTimerHz (15);
}

SimpleCompressorAudioProcessorEditor::~SimpleCompressorAudioProcessorEditor()
{
    stopTimer();
}

void SimpleCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xfff2f2f2));
    g.setColour (juce::Colour (0xff2b2b2b));
    g.setFont (juce::Font (18.0f, juce::Font::bold));
    g.drawFittedText ("SimpleCompressorVST", getLocalBounds().removeFromTop (32), juce::Justification::centred, 1);
}

void SimpleCompressorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (14);
    area.removeFromTop (32);
    area.removeFromTop (8);

    auto row = [&area] (int h) { return area.removeFromTop (h); };

    // ---------- I/O group ----------
    {
        auto box = row (110);
        ioGroup.setBounds (box);
        auto inner = box.reduced (10, 6);
        inner.removeFromTop (16);

        auto sliderRow = [&inner] (int h) { return inner.removeFromTop (h); };
        inputGainLabel.setBounds (sliderRow (14));
        inputGainSlider.setBounds (sliderRow (20));
        outputVolumeLabel.setBounds (sliderRow (14));
        outputVolumeSlider.setBounds (sliderRow (20));

        auto bottomRow = inner;
        auto left = bottomRow.removeFromLeft (bottomRow.getWidth() / 2);
        mixLabel.setBounds (left.removeFromTop (14));
        mixSlider.setBounds (left.removeFromTop (20));

        auto right = bottomRow;
        safetyLimiterButton.setBounds (right.removeFromTop (20));
        bypassButton.setBounds (right.removeFromTop (20));
    }
    area.removeFromTop (6);
    {
        auto meterRow = row (18);
        inputMeterLabel.setBounds (meterRow.removeFromLeft (28));
        inputMeter.setBounds (meterRow.removeFromLeft ((meterRow.getWidth() - 8) / 2));
        meterRow.removeFromLeft (8);
        outputMeterLabel.setBounds (meterRow.removeFromLeft (28));
        outputMeter.setBounds (meterRow);
    }
    area.removeFromTop (10);

    // ---------- Dynamics group ----------
    {
        auto box = row (190);
        dynamicsGroup.setBounds (box);
        auto inner = box.reduced (10, 6);
        inner.removeFromTop (16);

        auto sliderRow = [&inner] (int h) { return inner.removeFromTop (h); };
        thresholdLabel.setBounds (sliderRow (14));
        thresholdSlider.setBounds (sliderRow (20));
        ratioLabel.setBounds (sliderRow (14));
        ratioSlider.setBounds (sliderRow (20));
        kneeLabel.setBounds (sliderRow (14));
        kneeSlider.setBounds (sliderRow (20));
        attackLabel.setBounds (sliderRow (14));
        attackSlider.setBounds (sliderRow (20));
        releaseLabel.setBounds (sliderRow (14));
        releaseSlider.setBounds (sliderRow (20));

        inner.removeFromTop (2);
        auto lastRow = inner;
        auto left = lastRow.removeFromLeft (lastRow.getWidth() * 2 / 3);
        makeupLabel.setBounds (left.removeFromTop (14));
        makeupSlider.setBounds (left.removeFromTop (20));
        auto right = lastRow;
        autoMakeupButton.setBounds (right.removeFromTop (20));
    }
    area.removeFromTop (6);
    {
        auto meterRow = row (18);
        grMeterLabel.setBounds (meterRow.removeFromLeft (90));
        grMeter.setBounds (meterRow);
    }
    area.removeFromTop (10);

    // ---------- EQ group ----------
    {
        auto box = row (185);
        eqGroup.setBounds (box);
        auto inner = box.reduced (10, 6);
        inner.removeFromTop (16);

        {
            auto r = inner.removeFromTop (22);
            eqBypassButton.setBounds (r.removeFromLeft (110));
            r.removeFromLeft (8);
            eqPositionCombo.setBounds (r.removeFromLeft (140));
        }
        inner.removeFromTop (4);

        const int colW = inner.getWidth() / 4;
        auto lsCol = inner.removeFromLeft (colW).reduced (4, 0);
        auto p1Col = inner.removeFromLeft (colW).reduced (4, 0);
        auto p2Col = inner.removeFromLeft (colW).reduced (4, 0);
        auto hsCol = inner.reduced (4, 0);

        auto layoutBand = [] (juce::Rectangle<int> col, juce::Label& title,
                               juce::Label* cap1, juce::Slider* s1,
                               juce::Label* cap2, juce::Slider* s2,
                               juce::Label* cap3, juce::Slider* s3)
        {
            title.setBounds (col.removeFromTop (16));
            if (s1) { if (cap1) cap1->setBounds (col.removeFromTop (12)); s1->setBounds (col.removeFromTop (20)); }
            if (s2) { if (cap2) cap2->setBounds (col.removeFromTop (12)); s2->setBounds (col.removeFromTop (20)); }
            if (s3) { if (cap3) cap3->setBounds (col.removeFromTop (12)); s3->setBounds (col.removeFromTop (20)); }
        };

        layoutBand (lsCol, lsLabel, &lsFreqCap, &lsFreqSlider, &lsGainCap, &lsGainSlider, nullptr, nullptr);
        layoutBand (p1Col, p1Label, &p1FreqCap, &p1FreqSlider, &p1GainCap, &p1GainSlider, &p1QCap, &p1QSlider);
        layoutBand (p2Col, p2Label, &p2FreqCap, &p2FreqSlider, &p2GainCap, &p2GainSlider, &p2QCap, &p2QSlider);
        layoutBand (hsCol, hsLabel, &hsFreqCap, &hsFreqSlider, &hsGainCap, &hsGainSlider, nullptr, nullptr);
    }
    area.removeFromTop (8);

    spectrum.setBounds (area.removeFromTop (180));
}

void SimpleCompressorAudioProcessorEditor::timerCallback()
{
    inputMeter.setValueDb (processor.inputLevelDb.load());
    outputMeter.setValueDb (processor.outputLevelDb.load());
    grMeter.setValueDb (processor.gainReductionDb.load());
}
