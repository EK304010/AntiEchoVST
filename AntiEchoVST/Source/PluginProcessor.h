#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LoopbackCapture.h"

/**
 * AntiEchoVST
 * -----------
 * Equalizer APOの「録音(マイク)デバイス」チェーンに読み込んで使う前提のVST3。
 * 自前でWASAPIループバック(既定の再生デバイスの出力)を参照信号として取得し、
 * 「遅延 + 逆位相 + ゲイン」でマイク入力に加算することで、
 * スピーカーの再生音がマイクに回り込む成分を打ち消す(簡易AEC)。
 */
class AntiEchoAudioProcessor : public juce::AudioProcessor
{
public:
    AntiEchoAudioProcessor();
    ~AntiEchoAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AntiEchoVST"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // エディタ用に現在値をのぞける状態(簡易メーター表示)
    std::atomic<float> micLevel { 0.0f };
    std::atomic<float> outLevel { 0.0f };
    std::atomic<bool>  loopbackOk { false };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    LoopbackCapture loopback;
    double loopbackSampleRate = 48000.0;
    double pluginSampleRate = 48000.0;

    // ホスト(マイク側)のサンプル位置カウンタ(このプラグインが処理した総サンプル数)
    int64_t hostSamplePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AntiEchoAudioProcessor)
};
