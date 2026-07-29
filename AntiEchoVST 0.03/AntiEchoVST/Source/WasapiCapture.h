#pragma once
#include <atomic>
#include <vector>
#include <thread>
#include <memory>
#include <juce_core/juce_core.h>

#if JUCE_WINDOWS
 #include <windows.h>
 #include <mmdeviceapi.h>
 #include <audioclient.h>
 #include <wrl/client.h>
#endif

/** 列挙されたデバイス1件分の情報 */
struct AudioDeviceInfo
{
    juce::String id;    // WASAPIのデバイスID(内部識別用、永続化にも使う)
    juce::String name;  // 画面表示用のわかりやすい名前
};

enum class CaptureMode
{
    RenderLoopback, // 再生デバイスの出力音を横取りする(参照信号=スピーカー用)
    InputDevice     // マイク等の入力デバイスをそのままキャプチャする
};

/**
 * WASAPI経由でオーディオを取得し、float型のリングバッファに書き込み続けるクラス。
 * CaptureMode::RenderLoopback   : 再生デバイスの出力(参照信号=スピーカー)用
 * CaptureMode::InputDevice      : マイク等の入力デバイスの生音取得用
 *
 * - 別スレッドで動作し、書き込みは単一スレッドのみ(SPSC)
 * - 読み出し側(オーディオ処理スレッド/GUIタイマー)はロックなしで読める
 */
class WasapiCapture
{
public:
    WasapiCapture();
    ~WasapiCapture();

    /** 利用可能なデバイス一覧を取得する(静的関数、いつでも呼べる) */
    static std::vector<AudioDeviceInfo> getAvailableDevices (CaptureMode mode);

    /**
     * キャプチャ開始。
     * deviceId が空文字列なら「既定のデバイス」を使う。
     * 成功したらtrue。sampleRateOutにデバイスの実際のサンプルレートが入る
     */
    bool start (CaptureMode mode, const juce::String& deviceId, double& sampleRateOut);
    void stop();

    bool isRunning() const noexcept { return running.load(); }

    juce::String getCurrentDeviceName() const { return currentDeviceName; }

    int getBufferCapacity() const noexcept { return (int) ringCapacity; }

    // 現在の書き込み位置(モノトニックに増加し続けるサンプルカウンタ)
    int64_t getWritePosition() const noexcept { return writePos.load (std::memory_order_acquire); }

    // 指定したグローバル位置(サンプルカウンタ)のサンプルを読む。
    // まだ書き込まれていない/既に上書きされた位置の場合は0.0fを返す
    float readSample (int channel, int64_t globalPos) const noexcept;

private:
    void captureThreadFunc();

    std::atomic<bool> running { false };
    std::thread thread_;
    juce::String currentDeviceName;

    static constexpr int numChannels = 2;
    static constexpr size_t ringCapacity = 1 << 19; // 約524288サンプル(48kHzで約11秒)

    std::vector<float> ring[numChannels];
    std::atomic<int64_t> writePos { 0 };

    double deviceSampleRate = 48000.0;
    bool loopbackMode = true;

#if JUCE_WINDOWS
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    Microsoft::WRL::ComPtr<IMMDevice> device;
    Microsoft::WRL::ComPtr<IAudioClient> audioClient;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> captureClient;
    WAVEFORMATEX* mixFormat = nullptr;
#endif
};
