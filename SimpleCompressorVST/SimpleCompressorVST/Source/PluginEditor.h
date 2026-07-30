#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

/** Simple bordered group box with a title, matching common plugin UIs (light theme). */
class GroupBox : public juce::Component
{
public:
    explicit GroupBox (const juce::String& titleText) : title (titleText) {}

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xffffffff));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (juce::Colour (0xffd0d0d0));
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
        g.setColour (juce::Colour (0xff2f6fed));
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText (title, getLocalBounds().reduced (10, 4).removeFromTop (18), juce::Justification::left);
    }

private:
    juce::String title;
};

/** Small horizontal level/gain-reduction meter bar. */
class MeterBar : public juce::Component
{
public:
    void setRange (float minDb_, float maxDb_) { minDb = minDb_; maxDb = maxDb_; }
    void setValueDb (float db) { value = db; repaint(); }
    void setIsReduction (bool b) { isReduction = b; }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xffeeeeee));
        g.fillRoundedRectangle (bounds, 3.0f);

        const float norm = juce::jlimit (0.0f, 1.0f, (value - minDb) / (maxDb - minDb));
        auto fillBounds = bounds;

        if (isReduction)
        {
            // ゲインリダクションは右端(0dB)から左へ伸びる
            fillBounds = fillBounds.removeFromRight (fillBounds.getWidth() * norm);
            g.setColour (juce::Colour (0xffff6b6b));
        }
        else
        {
            fillBounds = fillBounds.removeFromLeft (fillBounds.getWidth() * norm);
            g.setColour (juce::Colour (0xff2f6fed));
        }
        g.fillRoundedRectangle (fillBounds, 3.0f);

        g.setColour (juce::Colour (0xffcccccc));
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
    }

private:
    float minDb = -60.0f, maxDb = 6.0f, value = -100.0f;
    bool isReduction = false;
};

/** Real-time input/output spectrum + EQ curve overlay, updated via Timer. */
class SpectrumComponent : public juce::Component,
                           private juce::Timer
{
public:
    explicit SpectrumComponent (SimpleCompressorAudioProcessor& p) : processor (p)
    {
        magsIn.fill (-100.0f);
        magsOut.fill (-100.0f);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xfffafafa));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (juce::Colour (0xffd0d0d0));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

        constexpr float minFreq = 20.0f, maxFreq = 20000.0f;
        constexpr float minDb = -80.0f, maxDb = 12.0f;

        auto freqToX = [&] (float freq)
        {
            const float logMin = std::log10 (minFreq), logMax = std::log10 (maxFreq);
            return bounds.getX() + bounds.getWidth() * (std::log10 (freq) - logMin) / (logMax - logMin);
        };
        auto dbToY = [&] (float db)
        {
            const float norm = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
            return bounds.getBottom() - norm * bounds.getHeight();
        };

        // --- grid ---
        g.setColour (juce::Colours::black.withAlpha (0.08f));
        for (float f : { 100.0f, 1000.0f, 10000.0f })
            g.drawVerticalLine ((int) freqToX (f), bounds.getY(), bounds.getBottom());
        for (float db : { -60.0f, -40.0f, -20.0f, 0.0f })
            g.drawHorizontalLine ((int) dbToY (db), bounds.getX(), bounds.getRight());

        const double sr = processor.getSampleRateForCurve();
        const double binHz = sr / (double) SpectrumAnalyzerState::fftSize;

        auto drawSpectrum = [&] (const std::array<float, SpectrumAnalyzerState::scopeSize>& mags, juce::Colour colour)
        {
            juce::Path path;
            bool started = false;
            for (int i = 1; i < SpectrumAnalyzerState::scopeSize; ++i)
            {
                const float freq = (float) (i * binHz);
                if (freq < minFreq || freq > maxFreq) continue;
                const float x = freqToX (freq);
                const float y = dbToY (mags[(size_t) i]);
                if (! started) { path.startNewSubPath (x, y); started = true; }
                else path.lineTo (x, y);
            }
            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (1.2f));
        };

        if (processor.analyzerIn.computeIfReady (magsIn)) {}
        if (processor.analyzerOut.computeIfReady (magsOut)) {}

        drawSpectrum (magsIn, juce::Colour (0xffaaaaaa).withAlpha (0.85f));   // input = grey
        drawSpectrum (magsOut, juce::Colour (0xff2f6fed).withAlpha (0.95f)); // output = blue

        // --- EQ curve overlay (analytic, from current filter coefficients) ---
        {
            juce::Path eqPath;
            const int numPts = 200;
            bool started = false;
            for (int i = 0; i < numPts; ++i)
            {
                const float t = (float) i / (float) (numPts - 1);
                const float freq = std::pow (10.0f, std::log10 (minFreq) + t * (std::log10 (maxFreq) - std::log10 (minFreq)));

                double magDb = 0.0;
                if (auto c = processor.getLowShelfCoeffs())  magDb += juce::Decibels::gainToDecibels (c->getMagnitudeForFrequency (freq, sr));
                if (auto c = processor.getPeak1Coeffs())     magDb += juce::Decibels::gainToDecibels (c->getMagnitudeForFrequency (freq, sr));
                if (auto c = processor.getPeak2Coeffs())     magDb += juce::Decibels::gainToDecibels (c->getMagnitudeForFrequency (freq, sr));
                if (auto c = processor.getHighShelfCoeffs()) magDb += juce::Decibels::gainToDecibels (c->getMagnitudeForFrequency (freq, sr));

                const float x = freqToX (freq);
                const float y = dbToY ((float) magDb);
                if (! started) { eqPath.startNewSubPath (x, y); started = true; }
                else eqPath.lineTo (x, y);
            }
            g.setColour (juce::Colour (0xffff9500));
            g.strokePath (eqPath, juce::PathStrokeType (2.0f));
        }
    }

private:
    void timerCallback() override { repaint(); }

    SimpleCompressorAudioProcessor& processor;
    std::array<float, SpectrumAnalyzerState::scopeSize> magsIn;
    std::array<float, SpectrumAnalyzerState::scopeSize> magsOut;
};

class SimpleCompressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit SimpleCompressorAudioProcessorEditor (SimpleCompressorAudioProcessor&);
    ~SimpleCompressorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::Slider& addSlider (const juce::String& paramId, const juce::String& labelText);
    juce::Label& addLabelFor (juce::Component& parent);

    SimpleCompressorAudioProcessor& processor;

    // --- I/O group ---
    GroupBox ioGroup { "INPUT / OUTPUT" };
    juce::Slider inputGainSlider, outputVolumeSlider, mixSlider;
    juce::Label inputGainLabel, outputVolumeLabel, mixLabel;
    juce::ToggleButton safetyLimiterButton { "Safety Limiter" };
    juce::ToggleButton bypassButton { "Bypass" };
    MeterBar inputMeter, outputMeter;
    juce::Label inputMeterLabel, outputMeterLabel;

    // --- Dynamics group ---
    GroupBox dynamicsGroup { "DYNAMICS (Threshold / Compressor)" };
    juce::Slider thresholdSlider, ratioSlider, kneeSlider, attackSlider, releaseSlider;
    juce::Label thresholdLabel, ratioLabel, kneeLabel, attackLabel, releaseLabel;
    juce::ToggleButton autoMakeupButton { "Auto Makeup" };
    juce::Slider makeupSlider;
    juce::Label makeupLabel;
    MeterBar grMeter;
    juce::Label grMeterLabel;

    // --- EQ group ---
    GroupBox eqGroup { "EQ (live input/output spectrum)" };
    juce::ToggleButton eqBypassButton { "EQ Bypass" };
    juce::ComboBox eqPositionCombo;
    juce::Slider lsFreqSlider, lsGainSlider;
    juce::Slider p1FreqSlider, p1GainSlider, p1QSlider;
    juce::Slider p2FreqSlider, p2GainSlider, p2QSlider;
    juce::Slider hsFreqSlider, hsGainSlider;
    juce::Label lsLabel, p1Label, p2Label, hsLabel;
    juce::Label lsFreqCap, lsGainCap, p1FreqCap, p1GainCap, p1QCap,
                p2FreqCap, p2GainCap, p2QCap, hsFreqCap, hsGainCap;
    SpectrumComponent spectrum;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> eqPositionAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleCompressorAudioProcessorEditor)
};
