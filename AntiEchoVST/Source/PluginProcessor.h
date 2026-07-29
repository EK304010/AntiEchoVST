#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LoopbackCapture.h"

/**
 * AntiEchoVST
 * -----------
 * Equalizer APOの「録音(マイク)デバイス」チェーンに読み込んで使う前提のVST3。
 * 自前でWASAPIループバック(指定した再生デバイスの出力)を参照信号として取得し、
 * 「遅延 + 逆位相 + ゲイン」でマイク入力に加算することで、
 * スピーカーの再生音がマイクに回り込む成分を打ち消す(簡易AEC)。
 */

/** GUIがロックなしで読める小さなスコープ用リングバッファ(1ch分, float) */
class ScopeRingBuffer
{
public:
    static constexpr int capacity = 4096;

    void push (float sample) noexcept
    {
        buffer[(size_t) (writeIndex.load (std::memory_order_relaxed) % capacity)] = sample;
        writeIndex.fetch_add (1, std::memory_order_release);
    }

    // 直近 numSamples 個を古い順に dest へコピーする(GUIスレッドから呼ぶ)
    void copyLatest (float* dest, int numSamples) const noexcept
    {
        const int64_t wp = writeIndex.load (std::memory_order_acquire);
        for (int i = 0; i < numSamples; ++i)
        {
            int64_t pos = wp - numSamples + i;
            if (pos < 0) { dest[i] = 0.0f; continue; }
            dest[i] = buffer[(size_t) (pos % capacity)];
        }
    }

private:
    float buffer[capacity] = {};
    std::atomic<int64_t> writeIndex { 0 };
};

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

    // --- 参照デバイス(スピーカー)の選択 ---
    std::vector<RenderDeviceInfo> getAvailableOutputDevices() const { return LoopbackCapture::getAvailableDevices(); }
    void setOutputDevice (const juce::String& deviceId);
    juce::String getSelectedDeviceId() const { return selectedDeviceId; }
    juce::String getCurrentDeviceName() const { return loopback.getCurrentDeviceName(); }

    juce::AudioProcessorValueTreeState apvts;

    // エディタ用に現在値をのぞける状態(簡易メーター表示)
    std::atomic<float> micLevel { 0.0f };
    std::atomic<float> outLevel { 0.0f };
    std::atomic<bool>  loopbackOk { false };

    // 「実際に処理できているか確認できるウィンドウ」用の波形スコープ
    ScopeRingBuffer micScope;
    ScopeRingBuffer outScope;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void restartLoopback();

    LoopbackCapture loopback;
    double loopbackSampleRate = 48000.0;
    double pluginSampleRate = 48000.0;

    juce::String selectedDeviceId; // 空文字列 = 既定デバイスを使う

    // ホスト(マイク側)のサンプル位置カウンタ(このプラグインが処理した総サンプル数)
    int64_t hostSamplePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AntiEchoAudioProcessor)
};
