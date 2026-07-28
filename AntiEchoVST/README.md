# AntiEchoVST

スピーカーの再生音がマイクに回り込む成分を、WASAPIループバックで取得した参照信号を
「遅延 + 逆位相 + ゲイン」でマイク入力に加算することで打ち消す、簡易AEC(音響エコーキャンセラ)のVST3プラグインです。

**Windows専用**です(WASAPIループバックを使うため)。この環境(Linuxサンドボックス)ではビルドできないので、
お手元のWindows PCでビルドしてください。

## 必要なもの

- Windows 10/11
- [Visual Studio 2022](https://visualstudio.microsoft.com/) (Community版で可、「C++によるデスクトップ開発」ワークロードを入れる)
- [CMake](https://cmake.org/download/) 3.22以上
- インターネット接続(初回ビルド時にJUCEを自動ダウンロードします)

## ビルド手順

```powershell
cd AntiEchoVST
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

ビルドが成功すると、`COPY_PLUGIN_AFTER_BUILD` が有効なため、通常は

```
C:\Program Files\Common Files\VST3\AntiEchoVST.vst3
```

に自動でインストールされます(管理者権限が必要な場合があります。失敗したら
`build\AntiEchoVST_artefacts\Release\VST3\AntiEchoVST.vst3` を手動でこのフォルダにコピーしてください)。

## Equalizer APOへの組み込み方

1. Equalizer APOをインストールする際、**マイク(録音)デバイス**にもチェックを入れてインストールしておく
   (インストーラでデバイス選択時にマイクを含めるのがポイント)
2. `Configuration Editor` を開き、対象のマイクデバイスのタブを選択
3. フィルタ追加で「VST Plugin」を選び、`AntiEchoVST.vst3` を指定
4. GUIが開くので、まずは以下から調整:
   - **Delay (ms)**: スピーカー→マイクの空気伝搬時間の目安(数ms〜数十ms程度)。動画通話中に
     ハウリング的な違和感がなくなる/一番静かになる点を探る
   - **Anti-phase Gain**: 1.0を基準に微調整(部屋の反響や距離で最適値が変わる)
   - **Mic Gain**: マイクの入力レベル自体の調整
5. 「mic RMS」「out RMS」の数値を見ながら、out RMSが小さくなるようDelayとAnti-phase Gainを追い込む

## 動作原理の補足

- `LoopbackCapture` が別スレッドで既定の再生デバイスの出力を横取りし、リングバッファに貯め続けます
- `processBlock`(マイク側のオーディオコールバック)は、このリングバッファから
  「delayMs だけ過去」の再生音サンプルを読み出し、`micSample - refGain * refSample` として減算(=逆位相加算)します
- リングバッファは約11秒分あるので、多少のスレッド間ジッタは吸収できますが、
  **再生デバイスと録音デバイスのサンプルレートが異なる場合はリサンプリングをしていないため精度が落ちます**。
  Windowsのサウンド設定で両デバイスのサンプルレート(例: 48000Hz)を揃えることを推奨します

## 既知の限界

- 単純な固定遅延モデルのため、非定常なノイズや部屋の反響が複雑な環境では完全には打ち消せません
- サンプルレート変換なし、ドリフト補正なしの簡易実装です。長時間使うとわずかにズレが蓄積する可能性があります
- WASAPIループバックの取得デバイスは「既定の再生デバイス」固定です。マルチデバイス環境で
  出力先を切り替える場合はコード内の `eConsole` ロール指定などを調整してください

## 発展させるなら

- サンプルレート変換(リニア補間 or `juce::dsp::Resampler` / `juce::LagrangeInterpolator`)の追加
- NLMS等の適応フィルタでDelay/Gainを自動追従させる(固定パラメータより効果的)
- ステレオ→モノラル参照信号への畳み込み等、より現実の部屋の伝達特性に近いモデル化
