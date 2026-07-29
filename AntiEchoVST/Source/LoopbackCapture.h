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

/** 列挙された再生デバイス1件分の情報 */
struct RenderDeviceInfo
{
    juce::String id;    // WASAPIのデバイスID(内部識別用、永続化にも使う)
    juce::String name;  // 画面表示用のわかりやすい名前
};

/**
 * 指定した再生デバイス(スピーカー/ヘッドホン出力)をWASAPIループバックで
 * 横取りし、float型のリングバッファに書き込み続けるクラス。
 *
 * - 別スレッドで動作し、書き込みは単一スレッドのみ(SPSC)
 * - 読み出し側(オーディオ処理スレッド)はロックなしで読める
 * - チャンネル数はステレオ(2ch)固定、サンプルレートは実行時に取得
 */
class LoopbackCapture
{
public:
    LoopbackCapture();
    ~LoopbackCapture();

    /** 利用可能な再生デバイス一覧を取得する(静的関数、いつでも呼べる) */
    static std::vector<RenderDeviceInfo> getAvailableDevices();

    /**
     * キャプチャ開始。
     * deviceId が空文字列なら「既定の再生デバイス」を使う。
     * 指定があればそのデバイスIDのものを使う(見つからなければ既定にフォールバック)。
     * 成功したらtrue。sampleRateOutにデバイスの実際のサンプルレートが入る
     */
    bool start (const juce::String& deviceId, double& sampleRateOut);
    void stop();

    bool isRunning() const noexcept { return running.load(); }

    // 現在使用中のデバイス名(GUI表示用)
    juce::String getCurrentDeviceName() const { return currentDeviceName; }

    // リングバッファの容量(サンプル数/チャンネル)
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

    // リングバッファ本体(チャンネルごと)
    std::vector<float> ring[numChannels];
    std::atomic<int64_t> writePos { 0 };

    double deviceSampleRate = 48000.0;

#if JUCE_WINDOWS
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    Microsoft::WRL::ComPtr<IMMDevice> device;
    Microsoft::WRL::ComPtr<IAudioClient> audioClient;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> captureClient;
    WAVEFORMATEX* mixFormat = nullptr;
#endif
};
