#include "PluginProcessor.h"
#include "PluginEditor.h"

SimpleCompressorAudioProcessor::SimpleCompressorAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

SimpleCompressorAudioProcessor::~SimpleCompressorAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout SimpleCompressorAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "inputGain", 1 }, "Input Gain (dB)",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "threshold", 1 }, "Threshold (dB)",
        NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -18.0f));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "ratio", 1 }, "Ratio",
        NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.5f), 4.0f));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "knee", 1 }, "Knee (dB)",
        NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "attack", 1 }, "Attack (ms)",
        NormalisableRange<float> (0.1f, 200.0f, 0.01f, 0.4f), 10.0f));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "release", 1 }, "Release (ms)",
        NormalisableRange<float> (5.0f, 1000.0f, 0.1f, 0.4f), 100.0f));

    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "autoMakeup", 1 }, "Auto Makeup Gain", true));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "makeup", 1 }, "Makeup Trim (dB)",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix (%)",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "outputVolume", 1 }, "Output Volume (dB)",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "safetyLimiter", 1 }, "Safety Limiter", true));

    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "bypass", 1 }, "Bypass", false));

    // --- EQ ---
    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "eqBypass", 1 }, "EQ Bypass", false));

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "eqPosition", 1 }, "EQ Position",
        juce::StringArray { "Pre-Comp", "Post-Comp" }, 1));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "lsFreq", 1 }, "Low Shelf Freq (Hz)",
        NormalisableRange<float> (20.0f, 500.0f, 0.1f, 0.4f), 100.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "lsGain", 1 }, "Low Shelf Gain (dB)",
        NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "p1Freq", 1 }, "Peak 1 Freq (Hz)",
        NormalisableRange<float> (100.0f, 5000.0f, 0.1f, 0.4f), 500.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "p1Gain", 1 }, "Peak 1 Gain (dB)",
        NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "p1Q", 1 }, "Peak 1 Q",
        NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f), 1.0f));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "p2Freq", 1 }, "Peak 2 Freq (Hz)",
        NormalisableRange<float> (500.0f, 10000.0f, 0.1f, 0.4f), 2000.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "p2Gain", 1 }, "Peak 2 Gain (dB)",
        NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "p2Q", 1 }, "Peak 2 Q",
        NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f), 1.0f));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "hsFreq", 1 }, "High Shelf Freq (Hz)",
        NormalisableRange<float> (2000.0f, 20000.0f, 0.1f, 0.4f), 8000.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "hsGain", 1 }, "High Shelf Gain (dB)",
        NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

float SimpleCompressorAudioProcessor::computeStaticGainReductionDb (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept
{
    const float halfKnee = kneeDb * 0.5f;
    const float overshoot = levelDb - thresholdDb;

    if (overshoot <= -halfKnee)
        return 0.0f; // スレッショルド以下:無圧縮

    float compressedLevelDb;

    if (overshoot >= halfKnee || kneeDb <= 0.0001f)
    {
        // ハードニー領域(または knee=0):通常のレシオ計算
        compressedLevelDb = thresholdDb + overshoot / ratio;
    }
    else
    {
        // ソフトニー領域:2次曲線で滑らかに補間(Reiss & McPherson の定式化)
        const float x = overshoot + halfKnee; // 0 .. kneeDb
        const float gainAtX = (1.0f / ratio - 1.0f) * (x * x) / (2.0f * kneeDb);
        compressedLevelDb = levelDb + gainAtX;
    }

    return compressedLevelDb - levelDb; // 常に <= 0
}

void SimpleCompressorAudioProcessor::updateEqCoefficients()
{
    const float lsFreq = apvts.getRawParameterValue ("lsFreq")->load();
    const float lsGain = apvts.getRawParameterValue ("lsGain")->load();
    const float p1Freq = apvts.getRawParameterValue ("p1Freq")->load();
    const float p1Gain = apvts.getRawParameterValue ("p1Gain")->load();
    const float p1Q    = apvts.getRawParameterValue ("p1Q")->load();
    const float p2Freq = apvts.getRawParameterValue ("p2Freq")->load();
    const float p2Gain = apvts.getRawParameterValue ("p2Gain")->load();
    const float p2Q    = apvts.getRawParameterValue ("p2Q")->load();
    const float hsFreq = apvts.getRawParameterValue ("hsFreq")->load();
    const float hsGain = apvts.getRawParameterValue ("hsGain")->load();

    const double sr = currentSampleRate;

    if (lsFreq != lastLowShelfFreq || lsGain != lastLowShelfGain)
    {
        *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, lsFreq, 0.707f, juce::Decibels::decibelsToGain (lsGain));
        lastLowShelfFreq = lsFreq; lastLowShelfGain = lsGain;
    }
    if (p1Freq != lastPeak1Freq || p1Gain != lastPeak1Gain || p1Q != lastPeak1Q)
    {
        *peak1.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, p1Freq, p1Q, juce::Decibels::decibelsToGain (p1Gain));
        lastPeak1Freq = p1Freq; lastPeak1Gain = p1Gain; lastPeak1Q = p1Q;
    }
    if (p2Freq != lastPeak2Freq || p2Gain != lastPeak2Gain || p2Q != lastPeak2Q)
    {
        *peak2.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, p2Freq, p2Q, juce::Decibels::decibelsToGain (p2Gain));
        lastPeak2Freq = p2Freq; lastPeak2Gain = p2Gain; lastPeak2Q = p2Q;
    }
    if (hsFreq != lastHighShelfFreq || hsGain != lastHighShelfGain)
    {
        *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, hsFreq, 0.707f, juce::Decibels::decibelsToGain (hsGain));
        lastHighShelfFreq = hsFreq; lastHighShelfGain = hsGain;
    }
}

void SimpleCompressorAudioProcessor::applyEq (juce::AudioBuffer<float>& buffer)
{
    if (apvts.getRawParameterValue ("eqBypass")->load() > 0.5f)
        return;

    updateEqCoefficients();

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    lowShelf.process (ctx);
    peak1.process (ctx);
    peak2.process (ctx);
    highShelf.process (ctx);
}

void SimpleCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    envelopeDb = -100.0f;
    currentGainReductionDb = 0.0f;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    lowShelf.prepare (spec);
    peak1.prepare (spec);
    peak2.prepare (spec);
    highShelf.prepare (spec);

    lastLowShelfFreq = lastPeak1Freq = lastPeak2Freq = lastHighShelfFreq = -1.0f; // 強制的に初回更新させる
    updateEqCoefficients();
}

void SimpleCompressorAudioProcessor::releaseResources() {}

void SimpleCompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // 入力スペクトラム(このブロックで実際に処理する前の生の音)を先に送っておく
    for (int n = 0; n < numSamples; ++n)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            mono += buffer.getReadPointer (ch)[n];
        if (numCh > 0) mono /= (float) numCh;
        analyzerIn.pushSample (mono);
    }

    const bool bypass = apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    if (bypass)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                mono += buffer.getReadPointer (ch)[n];
            if (numCh > 0) mono /= (float) numCh;
            analyzerOut.pushSample (mono);
        }
        return;
    }

    const bool eqIsPre = apvts.getRawParameterValue ("eqPosition")->load() < 0.5f; // 0=Pre-Comp, 1=Post-Comp

    if (eqIsPre)
        applyEq (buffer);

    const float inputGainDb   = apvts.getRawParameterValue ("inputGain")->load();
    const float thresholdDb   = apvts.getRawParameterValue ("threshold")->load();
    const float ratio         = apvts.getRawParameterValue ("ratio")->load();
    const float kneeDb        = apvts.getRawParameterValue ("knee")->load();
    const float attackMs      = apvts.getRawParameterValue ("attack")->load();
    const float releaseMs     = apvts.getRawParameterValue ("release")->load();
    const bool  autoMakeup    = apvts.getRawParameterValue ("autoMakeup")->load() > 0.5f;
    const float makeupTrimDb  = apvts.getRawParameterValue ("makeup")->load();
    const float mixPct        = apvts.getRawParameterValue ("mix")->load();
    const float outputVolDb   = apvts.getRawParameterValue ("outputVolume")->load();
    const bool  safetyLimiter = apvts.getRawParameterValue ("safetyLimiter")->load() > 0.5f;

    const float inputGainLin = juce::Decibels::decibelsToGain (inputGainDb);
    const float outputVolLin = juce::Decibels::decibelsToGain (outputVolDb);
    const float mix = juce::jlimit (0.0f, 1.0f, mixPct / 100.0f);

    float autoMakeupDb = 0.0f;
    if (autoMakeup)
        autoMakeupDb = -0.5f * computeStaticGainReductionDb (0.0f, thresholdDb, ratio, kneeDb);
    const float makeupLin = juce::Decibels::decibelsToGain (autoMakeupDb + makeupTrimDb);

    const float attackCoeff  = std::exp (-1.0f / (0.001f * attackMs  * (float) currentSampleRate));
    const float releaseCoeff = std::exp (-1.0f / (0.001f * releaseMs * (float) currentSampleRate));

    float peakInAcc = 0.0f, peakOutAcc = 0.0f, grMinDb = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        float linkedAbs = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            linkedAbs = std::max (linkedAbs, std::abs (buffer.getReadPointer (ch)[n]) * inputGainLin);

        const float levelDb = juce::Decibels::gainToDecibels (linkedAbs, -100.0f);
        peakInAcc = std::max (peakInAcc, linkedAbs);

        const float targetGrDb = computeStaticGainReductionDb (levelDb, thresholdDb, ratio, kneeDb);

        const float coeff = (targetGrDb < currentGainReductionDb) ? attackCoeff : releaseCoeff;
        currentGainReductionDb = coeff * currentGainReductionDb + (1.0f - coeff) * targetGrDb;
        grMinDb = std::min (grMinDb, currentGainReductionDb);

        const float compGainLin = juce::Decibels::decibelsToGain (currentGainReductionDb);
        const float totalGain = compGainLin * makeupLin * outputVolLin;

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            const float dry = data[n];
            const float inSample = dry * inputGainLin;
            const float wet = inSample * totalGain;

            float out = dry * (1.0f - mix) + wet * mix;

            if (safetyLimiter)
                out = juce::jlimit (-0.98f, 0.98f, out);

            data[n] = out;

            if (ch == 0)
                peakOutAcc = std::max (peakOutAcc, std::abs (out));
        }
    }

    if (! eqIsPre)
        applyEq (buffer);

    inputLevelDb.store (juce::Decibels::gainToDecibels (peakInAcc, -100.0f));
    outputLevelDb.store (juce::Decibels::gainToDecibels (peakOutAcc, -100.0f));
    gainReductionDb.store (grMinDb);

    // 最終的な出力スペクトラムを送出(EQ位置がPost-Compの場合はEQ適用後の音になる)
    for (int n = 0; n < numSamples; ++n)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            mono += buffer.getReadPointer (ch)[n];
        if (numCh > 0) mono /= (float) numCh;
        analyzerOut.pushSample (mono);
    }
}

juce::AudioProcessorEditor* SimpleCompressorAudioProcessor::createEditor()
{
    return new SimpleCompressorAudioProcessorEditor (*this);
}

void SimpleCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SimpleCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleCompressorAudioProcessor();
}
