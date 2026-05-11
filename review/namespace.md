# namespace Review

- 評価: `Good`

## Good: 公開 API と内部実装の名前空間分離はできている
- 優先度: Low
- 対象: `include/*`, `src/Internal.hpp`, `src/Mixer.hpp`, `src/Repository.hpp`, `src/Loader.hpp`, `src/Sound.hpp`
- 詳細と根拠:
  - 公開側は `namespace Audio` に限定され、内部は `namespace Audio::Internal` へ退避されています。
  - `vendor/miniaudio.h` は公開ヘッダに含まれておらず、利用者が backend 依存を意識せずに済みます。
- 改善案/方針:
  - この分離は維持して問題ありません。

## Good: 公開ヘッダの依存は比較的最小限に抑えられている
- 優先度: Low
- 対象: `include/AudioLib.hpp`, `include/Handle.hpp`, `include/PlaybackHandle.hpp`, `include/TrackHandle.hpp`
- 詳細と根拠:
  - 公開ヘッダは標準ヘッダと自前の handle 型だけで成立しており、`miniaudio` や内部クラスを露出していません。
  - ライブラリ利用者が include すべきファイル量も過剰ではありません。
- 改善案/方針:
  - もしさらに詰めるなら、用途別の include 導線を README 側で整理する程度で十分です。

## Warning: 物理ファイルパス依存の include はライブラリとしては脆い
- 優先度: Medium
- 対象: `src/AudioLib.cpp`, `src/Handle.cpp`, `src/PlaybackHandle.cpp`, `src/TrackHandle.cpp`
- 詳細と根拠:
  - `#include "include/AudioLib.hpp"` や `#include "src/Internal.hpp"` のように、ソースから見た相対ディレクトリ構造を前提に include しています。
  - これはビルド設定や配置換えに弱く、ライブラリ分離や install 時の見通しを悪くします。
- 改善案/方針:
  - 公開ヘッダは `#include "AudioLib.hpp"`、内部ヘッダはプロジェクト内 include path で解決する形に揃えた方が移植しやすいです。