# Audio Library Review

レビュー日: 2026-03-25

## Findings

### 1. `Pause()` した再生が次の `Play()` で破棄または再利用され、ポーズ状態を維持できない
- 重要度: High
- 根拠:
  - [`src/Mixer.cpp#L75`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Mixer.cpp#L75) から [`src/Mixer.cpp#L96`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Mixer.cpp#L96) では、`ma_sound_is_playing(...) == false` のインスタンスを「停止済み」とみなして再利用しています。
  - [`src/Mixer.cpp#L124`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Mixer.cpp#L124) から [`src/Mixer.cpp#L132`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Mixer.cpp#L132) でも同じ条件でクリーンアップしています。
  - [`src/Mixer.cpp#L158`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Mixer.cpp#L158) から [`src/Mixer.cpp#L162`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Mixer.cpp#L162) の `Pause()` は `ma_sound_stop()` を呼ぶだけなので、ポーズ中も `is_playing == false` になります。
- 影響:
  - 別の音を再生した瞬間に、ポーズ中の `PlaybackHandle` が内部マップから消える可能性があります。
  - 同じ音源を再生した場合、ポーズ中のインスタンスが別 ID として再利用され、元の `PlaybackHandle::Resume()` が効かなくなります。
  - API コメントの「Pause/Resume」が実質的に保証されません。
- 改善案:
  - `PlaybackInstance` に `Paused` / `Stopped` / `Playing` の状態を明示的に持たせ、再利用や `Cleanup()` の対象を `Stopped` のみに限定してください。
  - `Pause()` は停止扱いにしない設計へ寄せるか、少なくとも `Cleanup()` と再利用ロジックで paused を除外してください。

### 2. トラック削除時に `ma_sound_group` が先に破棄され、ぶら下がる再生インスタンスが残る
- 重要度: High
- 根拠:
  - [`src/Mixer.cpp#L216`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Mixer.cpp#L216) から [`src/Mixer.cpp#L219`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Mixer.cpp#L219) の `DestroyTrack()` は `StopAllInTrack()` のあとすぐ `tracks_.erase(_trackId)` しています。
  - `TrackInstance` のデストラクタは [`src/Mixer.cpp#L31`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Mixer.cpp#L31) から [`src/Mixer.cpp#L35`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Mixer.cpp#L35) で `ma_sound_group_uninit()` を実行します。
  - 一方で、同じトラックに属する `PlaybackInstance` は `sounds_` に残り続けます。`StopAllInTrack()` は停止するだけで削除していません。
- 影響:
  - 停止済みの再生インスタンスが、すでに破棄済みの group に紐づいたまま残ります。
  - その後に古い `PlaybackHandle` で `Resume()` した場合や、後続の破棄順によっては未定義動作やクラッシュの温床になります。
- 改善案:
  - `DestroyTrack()` では対象トラックの playback を `sounds_` から確実に除去してから group を破棄してください。
  - もしくは track ごとに playback 所有を持たせ、group の寿命が playback より短くならない構造にした方が安全です。

### 3. `Shutdown()` の仕様と実装が一致しておらず、デコード済み音声がプロセス終了まで残る
- 重要度: Medium
- 根拠:
  - 公開 API コメントでは [`include/AudioLib.hpp#L11`](d:/repos/PortfolioGame/Engine/Lib/Audio/include/AudioLib.hpp#L11) から [`include/AudioLib.hpp#L12`](d:/repos/PortfolioGame/Engine/Lib/Audio/include/AudioLib.hpp#L12) で「all audio resources を release」と説明しています。
  - しかし実装は [`src/AudioLib.cpp#L11`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/AudioLib.cpp#L11) から [`src/AudioLib.cpp#L13`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/AudioLib.cpp#L13) で `Mixer::Shutdown()` を呼ぶだけです。
  - `Repository` は [`src/Internal.cpp#L3`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Internal.cpp#L3) から [`src/Internal.cpp#L10`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Internal.cpp#L10) の静的 `unique_ptr` として生存し、保持データは [`src/Repository.hpp#L13`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Repository.hpp#L13) から [`src/Repository.hpp#L16`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Repository.hpp#L16) にある通り解放されません。
- 影響:
  - 一度読み込んだ PCM が `Shutdown()` 後も残り、長時間動作や再初期化を繰り返す用途でメモリを占有し続けます。
  - API 利用者がコメントを信用すると、メモリ解放タイミングを誤解します。
- 改善案:
  - `Repository::Clear()` を追加し、`Audio::Shutdown()` から明示的に呼んでください。
  - もしキャッシュ維持が意図なら、`Shutdown()` コメントを修正し、別途 `UnloadAll()` などの API を用意する方が分かりやすいです。

### 4. `Initialize()` が失敗を返せず、複数回呼び出しにも無防備
- 重要度: Medium
- 根拠:
  - [`src/AudioLib.cpp#L7`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/AudioLib.cpp#L7) から [`src/AudioLib.cpp#L9`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/AudioLib.cpp#L9) は `Mixer::Init()` の戻り値を捨てています。
  - [`src/Mixer.cpp#L44`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Mixer.cpp#L44) から [`src/Mixer.cpp#L51`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Mixer.cpp#L51) の `Init()` は、既存 `engine_` があるかを確認せず新規 `ma_engine` を確保しています。
- 影響:
  - 初期化失敗時に利用者は検知できず、その後の API 呼び出しが「ただ無効ハンドルを返す」状態になります。
  - 二重 `Initialize()` で古い `engine_` をリークさせる可能性があります。
- 改善案:
  - `bool Audio::Initialize()` へ変更し、失敗を呼び出し元へ返してください。
  - `Mixer::Init()` は idempotent にするか、初期化済みなら false/true を明確に返すようにしてください。

### 5. `Repository::FindName()` の返す `string_view` は将来の再ハッシュで無効化されうる
- 重要度: Low
- 根拠:
  - [`src/Repository.cpp#L33`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Repository.cpp#L33) から [`src/Repository.cpp#L38`](d:/repos/PortfolioGame/Engine/Lib/Audio/src/Repository.cpp#L38) は `unordered_map<std::string, uint64_t>` のキー文字列を `std::string_view` で返しています。
- 影響:
  - 呼び出し側がその `string_view` を保持すると、`Register()` による rehash 後にダングリング参照になる可能性があります。
- 改善案:
  - 戻り値を `std::string` にするか、`id -> name` の逆引きマップを別途持って寿命を安定させてください。

## Improvements

### 優先度高
- playback の状態管理を明示化し、`Pause` と `Stop` を内部的に区別する
- track 削除時の所有順を修正し、playback を先に破棄する
- `Shutdown()` で Repository も明示的に解放するか、仕様コメントを修正する

### 優先度中
- `Initialize()` の戻り値追加と二重初期化ガード
- 主要 API の異常系テスト追加
  - pause -> play another -> resume
  - destroy track -> stale playback handle access
  - initialize twice / shutdown twice
  - shutdown 後の再初期化でメモリが増え続けないこと

### 優先度低
- 文字化けしているコメントのエンコーディング統一
- `const float&` / `const bool&` のような軽量値型の参照受けを値渡しに整理

## Summary

現状は「基本再生」はできそうですが、`Pause/Resume` と track 破棄まわりにライブラリとしては見逃しにくい寿命バグがあります。まずは playback 状態管理と track 破棄順を直し、そのあとに `Shutdown()` と `Initialize()` の API 契約を固めるのがよいです。
