#include "PluginProcessor.h"
#include "PluginEditor.h"

AntiEchoAudioProcessor::AntiEchoAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

AntiEchoAudioProcessor::~AntiEchoAudioProcessor()
{
    stopTimer();
    micTestCapture.stop();
    referenceCapture.stop();
}

juce::AudioProcessorValueTreeState::ParameterLayout AntiEchoAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "delayMs", 1 }, "Delay (ms)",
        NormalisableRange<float> (0.0f, 300.0f, 0.1f), 15.0f));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "refGain", 1 }, "Anti-phase Gain",
        NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "micGain", 1 }, "Mic Gain",
        NormalisableRange<float> (0.0f, 3.0f, 0.01f), 1.0f));

    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "bypass", 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

void AntiEchoAudioProcessor::restartReferenceCapture()
{
    referenceCapture.stop();
    double sr = 48000.0;
    referenceOk.store (referenceCapture.start (CaptureMode::RenderLoopback, selectedOutputDeviceId, sr));
    referenceSampleRate = sr;
}

void AntiEchoAudioProcessor::setOutputDevice (const juce::String& deviceId)
{
    selectedOutputDeviceId = deviceId;
    restartReferenceCapture();
}

void AntiEchoAudioProcessor::setInputDevice (const juce::String& deviceId)
{
    selectedInputDeviceId = deviceId;
    if (testModeEnabled.load())
    {
        // 実行中なら選び直したデバイスで再起動する
        setTestModeEnabled (false);
        setTestModeEnabled (true);
    }
}

void AntiEchoAudioProcessor::setTestModeEnabled (bool shouldBeEnabled)
{
    if (shouldBeEnabled == testModeEnabled.load())
        return;

    if (shouldBeEnabled)
    {
        double sr = 48000.0;
        micTestOk.store (micTestCapture.start (CaptureMode::InputDevice, selectedInputDeviceId, sr));
        micTestSampleRate = sr;
        micTestReadPos = micTestCapture.getWritePosition();
        testModeEnabled.store (true);
        startTimerHz (60);
    }
    else
    {
        testModeEnabled.store (false);
        stopTimer();
        micTestCapture.stop();
        micTestOk.store (false);
    }
}

void AntiEchoAudioProcessor::timerCallback()
{
    if (! testModeEnabled.load() || ! micTestOk.load() || ! referenceOk.load())
        return;

    const int64_t currentMicWrite = micTestCapture.getWritePosition();
    int64_t newSamples = currentMicWrite - micTestReadPos;
    if (newSamples <= 0)
        return;

    // タイマーが遅れてもGUIが固まらないよう、1回あたりの処理量に上限を設ける
    const int64_t maxBatch = (int64_t) (micTestSampleRate * 0.5); // 最大0.5秒分
    if (newSamples > maxBatch)
    {
        micTestReadPos = currentMicWrite - maxBatch;
        newSamples = maxBatch;
    }

    const float delayMs = apvts.getRawParameterValue ("delayMs")->load();
    const float refGain = apvts.getRawParameterValue ("refGain")->load();
    const float micGain = apvts.getRawParameterValue ("micGain")->load();

    const double rateRatio = referenceSampleRate / micTestSampleRate;
    const int64_t delaySamplesRef = (int64_t) std::llround ((delayMs / 1000.0) * referenceSampleRate);
    const int64_t currentRefWrite = referenceCapture.getWritePosition();

    float rmsMicAcc = 0.0f, rmsOutAcc = 0.0f;

    for (int64_t n = 0; n < newSamples; ++n)
    {
        const int64_t micPos = micTestReadPos + n;
        const float micRaw = micTestCapture.readSample (0, micPos) * micGain;

        const int64_t samplesFromNow = newSamples - n;
        const int64_t refPos = currentRefWrite - (int64_t) std::llround (samplesFromNow * rateRatio) - delaySamplesRef;
        const float ref = referenceCapture.readSample (0, refPos);

        const float out = micRaw - refGain * ref;

        micScope.push (micRaw);
        outScope.push (out);

        rmsMicAcc += micRaw * micRaw;
        rmsOutAcc += out * out;
    }

    micLevel.store (std::sqrt (rmsMicAcc / (float) newSamples));
    outLevel.store (std::sqrt (rmsOutAcc / (float) newSamples));

    micTestReadPos = currentMicWrite;
}

void AntiEchoAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    pluginSampleRate = sampleRate;
    hostSamplePos = 0;
    restartReferenceCapture();
}

void AntiEchoAudioProcessor::releaseResources()
{
    referenceCapture.stop();
    referenceOk.store (false);
}

void AntiEchoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    const bool bypass = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const float delayMs = apvts.getRawParameterValue ("delayMs")->load();
    const float refGain = apvts.getRawParameterValue ("refGain")->load();
    const float micGain = apvts.getRawParameterValue ("micGain")->load();

    const bool testMode = testModeEnabled.load(); // Testモード中はスコープをTest側に任せる

    if (bypass || ! referenceOk.load())
    {
        hostSamplePos += numSamples;
        return;
    }

    const double rateRatio = referenceSampleRate / pluginSampleRate;
    const int64_t delaySamplesRef = (int64_t) std::llround ((delayMs / 1000.0) * referenceSampleRate);
    const int64_t currentWritePos = referenceCapture.getWritePosition();

    float micRmsAcc = 0.0f;
    float outRmsAcc = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        const int64_t samplesFromNow = (int64_t) (numSamples - n);
        const int64_t refPos = currentWritePos - (int64_t) std::llround (samplesFromNow * rateRatio) - delaySamplesRef;

        const float refL = referenceCapture.readSample (0, refPos);
        const float refR = referenceCapture.readSample (1, refPos);

        float firstChMic = 0.0f, firstChOut = 0.0f;

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            const float micSample = data[n] * micGain;
            const float refSample = (ch == 0 ? refL : refR);

            const float outSample = micSample - refGain * refSample;
            data[n] = outSample;

            if (ch == 0)
            {
                firstChMic = micSample;
                firstChOut = outSample;
                micRmsAcc += micSample * micSample;
                outRmsAcc += outSample * outSample;
            }
        }

        if (! testMode)
        {
            micScope.push (firstChMic);
            outScope.push (firstChOut);
        }
    }

    if (! testMode && numSamples > 0)
    {
        micLevel.store (std::sqrt (micRmsAcc / (float) numSamples));
        outLevel.store (std::sqrt (outRmsAcc / (float) numSamples));
    }

    hostSamplePos += numSamples;
}

juce::AudioProcessorEditor* AntiEchoAudioProcessor::createEditor()
{
    return new AntiEchoAudioProcessorEditor (*this);
}

void AntiEchoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("outputDeviceId", selectedOutputDeviceId);
    xml->setAttribute ("inputDeviceId", selectedInputDeviceId);
    copyXmlToBinary (*xml, destData);
}

void AntiEchoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        selectedOutputDeviceId = xml->getStringAttribute ("outputDeviceId", "");
        selectedInputDeviceId  = xml->getStringAttribute ("inputDeviceId", "");
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AntiEchoAudioProcessor();
}
