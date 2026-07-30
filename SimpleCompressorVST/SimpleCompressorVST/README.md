# 🎚️ SimpleCompressorVST

**Threshold / Compressor / Volume** を中心にした、よくあるダイナミクス系VST3プラグイン。
入出力の実測スペクトラムを見ながらEQもかけられます。

![platform](https://img.shields.io/badge/platform-Windows-0078D6?logo=windows&logoColor=white)
![format](https://img.shields.io/badge/format-VST3-3d3d3d)
![build](https://img.shields.io/badge/build-CMake%20%2B%20JUCE-5cb85c)

## ✨ 機能

### ダイナミクス
- **Threshold / Ratio / Knee(ソフトニー)/ Attack / Release** — 標準的なフィードフォワード型コンプレッサー
- **Auto Makeup Gain** — スレッショルドに応じて自動でメイクアップを補正(+手動トリムも可)
- **Gain Reductionメーター** — 今どれだけ圧縮されているか一目で分かる

### 入出力
- **Input Gain / Output Volume** — 前段・後段のゲイン調整
- **Mix (Dry/Wet)** — パラレルコンプレッションにも対応
- **Safety Limiter** — 0dBFS付近でのハードクリップ防止
- **入力/出力レベルメーター**

### EQ + スペクトラムアナライザ
- **4バンドEQ**(Low Shelf / Peak x2 / High Shelf)
- **EQ Position(Pre-Comp / Post-Comp)** — コンプの前段・後段どちらにかけるか切り替え可能
- **リアルタイム スペクトラムアナライザ** — 入力(グレー)と出力(青)の実測周波数特性を重ねて表示
- **EQカーブのオーバーレイ(オレンジ)** — 今かけているEQの効きを実測スペクトラムと同時に確認しながら調整できる

## 🛠️ ビルド方法

Windows / macOS 両対応(WASAPI等のプラットフォーム固有APIは使っていません)。

```powershell
cmake -B build -A x64
cmake --build build --config Release
```

初回はJUCEを [github.com/juce-framework/JUCE](https://github.com/juce-framework/JUCE) から自動取得します。
成功すると `C:\Program Files\Common Files\VST3\SimpleCompressorVST.vst3` に自動配置されます。

`.github/workflows/main.yml` により、GitHub Actions(Windowsランナー)上でも自動ビルドできます。

## 🎛️ 使い方のヒント

1. まず **Threshold** を下げて音が圧縮され始める点を探す
2. **Ratio** で圧縮の強さ、**Attack/Release** で反応速度を調整
3. **Auto Makeup Gain** をONにしておけば、音量感の目減りをある程度自動で補正してくれる
4. EQは **スペクトラムアナライザを見ながら**、ブーストしたい/削りたい帯域にバンドを合わせる。
   出力(青)の山や谷を見ながら調整すると分かりやすい
5. 最終的な音量は **Output Volume** で微調整し、**Safety Limiter** はONのままにしておくと安心

## ⚠️ 注意

- EQのソフトニーは Low/High Shelf は Q=0.707 固定(標準的なシェルフ特性)、Peakバンドのみ Q調整可能
- スペクトラムアナライザは FFTサイズ2048・Hann窓で、目安として見る用途です(測定器ではありません)
