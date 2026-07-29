#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "WasapiCapture.h"

/**
 * AntiEchoVST
 * -----------
 * Equalizer APOの「録音(マイク)デバイス」チェーンに読み込んで使う前提のVST3。
 * 自前でWASAPIループバック(指定した再生デバイスの出力)を参照信号として取得し、
 * 「遅延 + 逆位相 + ゲイン」でマイク入力に加算することで、
 * スピーカーの再生音がマイクに回り込む成分を打ち消す(簡易AEC)。
 *
 * 注意: Equalizer APOのConfiguration Editorから開くプラグイン設定画面(このGUI)は、
 * 実際に流れている音声を受け取れない「編集専用インスタンス」です。実際の処理は
 * バックグラウンドの別インスタンスで行われるため、このGUIのメーターは通常0のままです。
 * 動作確認のために「Testモード」(プラグイン自身がマイクも直接開いて即座に処理する
 * スタンドアロン確認機能)を用意しています。
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

class AntiEchoAudioProcessor : public juce::AudioProcessor,
                                private juce::Timer
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
    std::vector<AudioDeviceInfo> getAvailableOutputDevices() const { return WasapiCapture::getAvailableDevices (CaptureMode::RenderLoopback); }
    void setOutputDevice (const juce::String& deviceId);
    juce::String getSelectedDeviceId() const { return selectedOutputDeviceId; }
    juce::String getCurrentOutputDeviceName() const { return referenceCapture.getCurrentDeviceName(); }

    // --- マイクデバイスの選択(Testモード用) ---
    std::vector<AudioDeviceInfo> getAvailableInputDevices() const { return WasapiCapture::getAvailableDevices (CaptureMode::InputDevice); }
    void setInputDevice (const juce::String& deviceId);
    juce::String getSelectedInputDeviceId() const { return selectedInputDeviceId; }
    juce::String getCurrentInputDeviceName() const { return micTestCapture.getCurrentDeviceName(); }

    // --- Testモード(プラグイン自身がマイクも直接開いて即座に確認する) ---
    void setTestModeEnabled (bool shouldBeEnabled);
    bool isTestModeEnabled() const { return testModeEnabled.load(); }

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> micLevel { 0.0f };
    std::atomic<float> outLevel { 0.0f };
    std::atomic<bool>  referenceOk { false };
    std::atomic<bool>  micTestOk { false };

    ScopeRingBuffer micScope;
    ScopeRingBuffer outScope;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void restartReferenceCapture();
    void timerCallback() override; // Testモード時にマイクを直接処理してスコープへ反映

    WasapiCapture referenceCapture; // スピーカー(参照信号)
    WasapiCapture micTestCapture;   // Testモード専用マイク直接キャプチャ

    double referenceSampleRate = 48000.0;
    double pluginSampleRate = 48000.0;
    double micTestSampleRate = 48000.0;

    juce::String selectedOutputDeviceId; // 空文字列 = 既定デバイス
    juce::String selectedInputDeviceId;  // 空文字列 = 既定デバイス

    std::atomic<bool> testModeEnabled { false };
    int64_t micTestReadPos = 0;

    int64_t hostSamplePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AntiEchoAudioProcessor)
};
