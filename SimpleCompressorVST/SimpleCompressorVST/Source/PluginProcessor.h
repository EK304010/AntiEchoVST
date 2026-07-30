#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>

/**
 * SpectrumAnalyzerState
 * ---------------------
 * オーディオスレッドでサンプルを貯めてFFT用バッファへコピーするだけの軽い処理を行い、
 * 実際の窓掛け+FFT計算はGUIスレッド(Timer)側で行う、JUCE公式チュートリアルと同じ設計。
 */
class SpectrumAnalyzerState
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;   // 2048
    static constexpr int scopeSize = fftSize / 2;   // 1024 (使用する周波数ビン数)

    SpectrumAnalyzerState()
        : forwardFFT (fftOrder),
          window (fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        fifo.fill (0.0f);
        fftData.fill (0.0f);
    }

    /** オーディオスレッドから1サンプルずつ呼ぶ */
    void pushSample (float sample) noexcept
    {
        if (fifoIndex == fftSize)
        {
            if (! nextFftBlockReady.load (std::memory_order_acquire))
            {
                std::copy (fifo.begin(), fifo.end(), fftData.begin());
                std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
                nextFftBlockReady.store (true, std::memory_order_release);
            }
            fifoIndex = 0;
        }
        fifo[(size_t) fifoIndex++] = sample;
    }

    /** GUIスレッドから定期的に呼ぶ。新しいデータがあれば計算してtrueを返す */
    bool computeIfReady (std::array<float, scopeSize>& outMagnitudesDb)
    {
        if (! nextFftBlockReady.load (std::memory_order_acquire))
            return false;

        window.multiplyWithWindowingTable (fftData.data(), fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform (fftData.data());

        for (int i = 0; i < scopeSize; ++i)
        {
            const float mag = fftData[(size_t) i] / (float) fftSize;
            outMagnitudesDb[(size_t) i] = juce::Decibels::gainToDecibels (mag, -100.0f);
        }

        nextFftBlockReady.store (false, std::memory_order_release);
        return true;
    }

private:
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, (size_t) fftSize> fifo;
    std::array<float, (size_t) fftSize * 2> fftData;
    int fifoIndex = 0;
    std::atomic<bool> nextFftBlockReady { false };
};

/**
 * SimpleCompressorVST
 * --------------------
 * Threshold / Ratio / Knee / Attack / Release による標準的なフィードフォワード型
 * コンプレッサー + Input Gain + Auto/Manual Makeup Gain + Mix(Dry/Wet) +
 * Output Volume + Safety Limiter(0dBFSでのハードクリップ防止)を備えたVST3。
 */
class SimpleCompressorAudioProcessor : public juce::AudioProcessor
{
public:
    SimpleCompressorAudioProcessor();
    ~SimpleCompressorAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SimpleCompressorVST"; }
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

    // GUI用のメーター類(ロックなしで読める)
    std::atomic<float> inputLevelDb  { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };
    std::atomic<float> gainReductionDb { 0.0f };

    // --- スペクトラムアナライザ(入力/出力それぞれ) ---
    SpectrumAnalyzerState analyzerIn;
    SpectrumAnalyzerState analyzerOut;

    // --- EQカーブ描画用に、現在のフィルタ係数を取得する ---
    juce::dsp::IIR::Coefficients<float>::Ptr getLowShelfCoeffs()  const { return lowShelf.state; }
    juce::dsp::IIR::Coefficients<float>::Ptr getPeak1Coeffs()     const { return peak1.state; }
    juce::dsp::IIR::Coefficients<float>::Ptr getPeak2Coeffs()     const { return peak2.state; }
    juce::dsp::IIR::Coefficients<float>::Ptr getHighShelfCoeffs() const { return highShelf.state; }
    double getSampleRateForCurve() const { return currentSampleRate; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // 1サンプル分の静的圧縮カーブ(ソフトニー対応)を計算し、dBでの減衰量を返す(常に0以下)
    static float computeStaticGainReductionDb (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept;

    void updateEqCoefficients();
    void applyEq (juce::AudioBuffer<float>& buffer);

    double currentSampleRate = 48000.0;

    // 包絡線検出(dB領域、アタック/リリース別係数)。ステレオはリンクさせて1本にする。
    float envelopeDb = -100.0f;
    float currentGainReductionDb = 0.0f;

    // --- 4バンドEQ(Low Shelf / Peak x2 / High Shelf) ---
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowShelf, peak1, peak2, highShelf;
    float lastLowShelfFreq = -1, lastLowShelfGain = 0;
    float lastPeak1Freq = -1, lastPeak1Gain = 0, lastPeak1Q = 0;
    float lastPeak2Freq = -1, lastPeak2Gain = 0, lastPeak2Q = 0;
    float lastHighShelfFreq = -1, lastHighShelfGain = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleCompressorAudioProcessor)
};
