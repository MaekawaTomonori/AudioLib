# Encapsulate Review

- 評価: `Warning`

## Good: 公開 API から miniaudio の詳細を隠せている
- 優先度: Low
- 対象: `include/AudioLib.hpp`, `include/Handle.hpp`, `include/PlaybackHandle.hpp`, `include/TrackHandle.hpp`
- 詳細と根拠:
  - 公開ヘッダは `Audio::Handle` / `Audio::PlaybackHandle` / `Audio::TrackHandle` を介して操作させており、`ma_engine` や `ma_sound` は露出していません。
  - `Audio::Internal` 名前空間に `Mixer`, `Repository`, `Loader`, `Sound` を隔離しており、ユーザーに内部構造を直接触らせない設計になっています。
- 改善案/方針:
  - この方向性は維持してよいです。ライブラリの使い勝手という意味では妥当です。

## Warning: `Internal::GetMixer()` / `GetRepository()` が実質グローバル可変状態になっている
- 優先度: High
- 対象: `src/Internal.cpp`, `src/Internal.hpp`, `src/Handle.cpp`, `src/AudioLib.cpp`, `src/PlaybackHandle.cpp`, `src/TrackHandle.cpp`
- 詳細と根拠:
  - `src/Internal.cpp:4-10` で `Repository` と `Mixer` を無名 namespace の静的 `unique_ptr` として保持しています。
  - 各公開 API はほぼすべて `Internal::GetMixer()` / `Internal::GetRepository()` に直接依存しており、依存先が暗黙です。
  - カプセル化は「隠す」だけでなく「責務と依存関係を閉じ込める」ことも含みます。この構造だと初期化順、終了順、テスト差し替え、将来の複数コンテキスト化が非常にやりにくくなります。
- 改善案/方針:
  - `AudioContext` のような所有者クラスを 1 つ作り、その中に `Mixer` と `Repository` を閉じ込める構成が素直です。
  - グローバル関数 API を残したい場合でも、内部では明示的なコンテキストを 1 箇所に束ね、アクセサ関数の乱用を減らした方が保守しやすいです。

## Warning: ライブラリ内部の状態管理が API から見えず、失敗時の判断材料が足りない
- 優先度: Medium
- 対象: `include/Handle.hpp`, `src/Handle.cpp`
- 詳細と根拠:
  - `Handle` はコンストラクタでロードまで実行しますが、`Handle` 自身に `IsValid()` がありません。
  - `src/Handle.cpp:18-20` ではロード失敗時に `handle_ == 0` のまま終わり得ますが、利用者は `Play()` するまで失敗を検知できません。
  - 「内部を知らなくても動く」が目標でも、失敗時にまったく状態が見えないのは使いやすさより不親切です。
- 改善案/方針:
  - `Handle::IsValid()` を公開し、ロード失敗を利用者が即時に判定できるようにしてください。
  - さらに進めるなら `expected` 相当や `Result` 型、または `Load()` 関数分離で失敗理由を返す設計が望ましいです。