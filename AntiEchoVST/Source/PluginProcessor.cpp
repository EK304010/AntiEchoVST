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

void AntiEchoAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    pluginSampleRate = sampleRate;
    hostSamplePos = 0;

    double lbRate = 48000.0;
    loopbackOk.store (loopback.start (lbRate));
    loopbackSampleRate = lbRate;
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
        hostSamplePos += numSamples;
        return;
    }

    const double rateRatio = loopbackSampleRate / pluginSampleRate; // ループバックとプラグインのSR比
    const int64_t delaySamplesLoopback = (int64_t) std::llround ((delayMs / 1000.0) * loopbackSampleRate);

    // 現在のループバック書き込み位置を基準に、「今から delayMs 前」の位置から遡って読む。
    const int64_t currentWritePos = loopback.getWritePosition();

    float micRmsAcc = 0.0f;
    float outRmsAcc = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        // このサンプルがブロック内で処理される「相対的な新しさ」を考慮し、
        // ブロックの末尾ほど現在時刻に近く、先頭ほど過去になる分を補正
        const int64_t samplesFromNow = (int64_t) (numSamples - n);
        const int64_t refPos = currentWritePos - (int64_t) std::llround (samplesFromNow * rateRatio) - delaySamplesLoopback;

        const float refL = loopback.readSample (0, refPos);
        const float refR = loopback.readSample (1, refPos);

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            const float micSample = data[n] * micGain;
            const float refSample = (ch == 0 ? refL : refR);

            // 逆位相を加算 = 減算 と同義。refGainで強さを調整
            const float outSample = micSample - refGain * refSample;

            data[n] = outSample;

            if (ch == 0)
            {
                micRmsAcc += micSample * micSample;
                outRmsAcc += outSample * outSample;
            }
        }
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
    if (auto state = apvts.copyState(); true)
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void AntiEchoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// このファイルはVST3のみをビルドするため、プラグインのファクトリ関数は
// JUCEのjuce_audio_plugin_clientモジュールが自動生成します。
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AntiEchoAudioProcessor();
}
