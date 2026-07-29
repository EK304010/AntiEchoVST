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
    loopback.stop();
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

void AntiEchoAudioProcessor::restartLoopback()
{
    loopback.stop();
    double lbRate = 48000.0;
    loopbackOk.store (loopback.start (selectedDeviceId, lbRate));
    loopbackSampleRate = lbRate;
}

void AntiEchoAudioProcessor::setOutputDevice (const juce::String& deviceId)
{
    selectedDeviceId = deviceId;
    restartLoopback();
}

void AntiEchoAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    pluginSampleRate = sampleRate;
    hostSamplePos = 0;
    restartLoopback();
}

void AntiEchoAudioProcessor::releaseResources()
{
    loopback.stop();
    loopbackOk.store (false);
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

    if (bypass || ! loopbackOk.load())
    {
        // バイパス中もスコープには生のマイク音を流しておく(確認しやすいように)
        if (numCh > 0)
        {
            const float* data = buffer.getReadPointer (0);
            for (int n = 0; n < numSamples; ++n)
            {
                micScope.push (data[n]);
                outScope.push (data[n]);
            }
        }
        hostSamplePos += numSamples;
        return;
    }

    const double rateRatio = loopbackSampleRate / pluginSampleRate;
    const int64_t delaySamplesLoopback = (int64_t) std::llround ((delayMs / 1000.0) * loopbackSampleRate);
    const int64_t currentWritePos = loopback.getWritePosition();

    float micRmsAcc = 0.0f;
    float outRmsAcc = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        const int64_t samplesFromNow = (int64_t) (numSamples - n);
        const int64_t refPos = currentWritePos - (int64_t) std::llround (samplesFromNow * rateRatio) - delaySamplesLoopback;

        const float refL = loopback.readSample (0, refPos);
        const float refR = loopback.readSample (1, refPos);

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

        micScope.push (firstChMic);
        outScope.push (firstChOut);
    }

    if (numSamples > 0)
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
    xml->setAttribute ("outputDeviceId", selectedDeviceId);
    copyXmlToBinary (*xml, destData);
}

void AntiEchoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        selectedDeviceId = xml->getStringAttribute ("outputDeviceId", "");
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AntiEchoAudioProcessor();
}
