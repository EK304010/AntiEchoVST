# 🔇 AntiEchoVST

**スピーカーの再生音がマイクに回り込む成分を、逆位相合成でリアルタイムに打ち消すVST3プラグイン。**
Equalizer APO のマイク(録音)チェーンに読み込んで使うことを想定した、簡易AEC(Acoustic Echo Canceller)です。

![platform](https://img.shields.io/badge/platform-Windows-0078D6?logo=windows&logoColor=white)
![format](https://img.shields.io/badge/format-VST3-3d3d3d)
![build](https://img.shields.io/badge/build-CMake%20%2B%20JUCE-5cb85c)
![status](https://img.shields.io/badge/status-experimental-orange)

---

## ✨ 特徴

- 🎙️ **自前でWASAPIループバック取得** — Equalizer APOがVSTに渡してくれない「今スピーカーに送られている音」を、プラグイン自身が横取りして参照信号にする
- 🎚️ **Delay / Anti-phase Gain / Mic Gain** の3パラメータをリアルタイム調整可能
- 🔀 **スピーカー(参照信号)とマイクをそれぞれ独立して選択可能** — GUI上で「SPEAKER」枠と「MICROPHONE」枠に分かれている
- 🧪 **Test Mode内蔵** — Equalizer APOのConfiguration Editorから開くプラグイン画面は実際の音声を受け取れない編集専用インスタンスのため、常にメーターが0のままになります。「Start Test」を押すとプラグイン自身が選択したマイクを直接開いて、その場で本当に打ち消せているか確認できます
- 📈 **波形スコープ内蔵** — マイク生音(グレー)と処理後(ティール)の波形を重ねて表示
- 🧠 状態(選択デバイス・パラメータ)はプリセットとして保存/復元される
- 🌐 GUIは文字化け対策のため**英語表記**です

## 🖼️ スクリーンショット

```
┌──────────────────────────────────────────┐
│  AntiEcho VST - 逆位相ノイズキャンセル         │
├──────────────────────────────────────────┤
│  参照信号に使う出力デバイス   [▼ Default ] [更新] │
│                                            │
│  Delay (ms)          ────●──────  15.0ms  │
│  Anti-phase Gain     ──────●────   1.00   │
│  Mic Gain            ────●──────   1.00   │
│  ☐ Bypass                                 │
│                                            │
│  灰色=マイク生音  水色=処理後(キャンセル後)     │
│  ╱╲╱╲___________________________________  │ ← 波形スコープ
│  参照信号: Speakers (Realtek)              │
│  mic RMS: 0.081   out RMS: 0.014           │
└──────────────────────────────────────────┘
```

## 🧩 動作原理

```
[既定/選択した再生デバイス] ──(WASAPI loopback)──▶ [遅延] ──▶ [逆位相ゲイン] ──┐
                                                                         ▼
[マイク入力(Equalizer APOのcaptureチェーン)] ───────────────────────────▶ (減算) ──▶ 出力
```

1. 別スレッドで動く `LoopbackCapture` が、選択した再生デバイスの出力音を横取りしてリングバッファに貯め続ける
2. VSTの `processBlock`(= マイクの音)が呼ばれるたびに、リングバッファから「Delay ms 前」のサンプルを取り出す
3. それを `Anti-phase Gain` 倍して**減算**(= 逆位相を加算するのと同義)し、回り込み成分を打ち消す

## 🛠️ ビルド方法

Windows専用です(WASAPIループバックを使うため)。

```powershell
cmake -B build -A x64
cmake --build build --config Release
```

初回はJUCEを [github.com/juce-framework/JUCE](https://github.com/juce-framework/JUCE) から自動取得します。
成功すると `C:\Program Files\Common Files\VST3\AntiEchoVST.vst3` に自動配置されます。

> 💡 手元にVisual Studio環境が無くても、このリポジトリの `.github/workflows/build.yml` により
> GitHub Actions(Windowsランナー)上で自動ビルドできます。Actionsタブの実行結果からビルド済み
> `.vst3` をArtifactとしてダウンロードできます。

## 🎛️ Equalizer APOでの使い方

標準版のEqualizer APOは `VSTPlugin: Library` でVST2(`.dll`)しか読み込めません。VST3を使うには、
ネイティブVST3対応のフォーク版(例: [`EqAPO64_with_VST3_support`](https://github.com/Mixomo/EqAPO64_with_VST3_support))
を使い、設定ファイルに以下のように書きます。

```
VST3: Dll "C:\Program Files\Common Files\VST3\AntiEchoVST.vst3" Data ""
```

1. マイク(録音)デバイスのチェーンにVSTフィルタとして追加
2. GUI上部「SPEAKER」枠で参照デバイス(実際にノイズを鳴らしているスピーカー/ヘッドホン)を選択
3. **重要**: このGUI(プラグイン設定画面)は実際の音声を受け取れない編集専用インスタンスです。
   「MICROPHONE」枠でテスト用マイクを選び、「Start Test」を押すと、プラグイン自身がそのマイクを
   直接開いて即座に処理し、波形スコープ・RMSメーターに反映されます。ここで本当に打ち消せているか確認してください
4. パラメータが決まったら「Stop Test」を押してから閉じ、実際の通話・録音アプリ側で最終確認してください
   (Test Mode中はマイクを二重に掴むため、実際の通話アプリと同時使用はしないでください)

## ⚠️ 既知の限界

- 固定遅延モデルのため、非定常なノイズや複雑な部屋の反響には完全に追従できません
- サンプルレート変換・ドリフト補正なしの簡易実装です
- 参照デバイスは選択したデバイス1台固定です(複数デバイス同時ミックスには非対応)

## 🚀 発展させるなら

- [ ] リサンプリング対応(`juce::dsp::Resampler` / `LagrangeInterpolator`)
- [ ] NLMS等の適応フィルタでDelay/Gainを自動追従
- [ ] 部屋の伝達特性を畳み込みでモデル化

---

<sub>Built with [JUCE](https://juce.com/) 🧃 — an experimental project, use at your own risk.</sub>
