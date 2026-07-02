# RawPointer Review

- 評価: `Critical`

## Critical: `Mixer` が所有権を伴う raw pointer を手動管理している
- 優先度: Highest
- 対象: `src/Mixer.hpp`, `src/Mixer.cpp`
- 詳細と根拠:
  - `src/Mixer.hpp:18-20` では `ma_engine*` と `shared_ptr<PlaybackInstance>`, `shared_ptr<TrackInstance>` を持ちます。
  - `src/Mixer.cpp:17`, `32`, `49`, `118`, `217` で `ma_sound*`, `ma_sound_group*`, `ma_engine*` を `new` し、`22-28`, `34-38`, `50-53`, `64-65` で手動 `delete` しています。
  - 例外安全と保守性の面で明確に不利です。いまは shared_ptr のデストラクタに片寄せして破綻していませんが、早期 return や将来の分岐追加に非常に弱いです。
  - `prompt.md` の「内部使用は許容するが、ライフタイムが明確であること」に対し、この実装は人間が頭の中で追わないと安全性を証明できません。
- 改善案/方針:
  - `std::unique_ptr<T, Deleter>` か、自前 RAII wrapper を導入してください。
  - C API のハンドルでも、C++ 側では必ず所有クラスに包んだ方が安全です。

## Warning: `shared_ptr` を使う理由が薄く、所有モデルをぼかしている
- 優先度: High
- 対象: `src/Mixer.hpp`, `src/Mixer.cpp`
- 詳細と根拠:
  - `sounds_` と `tracks_` はどちらも `unordered_map` が唯一の所有者で、外へ `shared_ptr` を渡していません。
  - にもかかわらず `std::shared_ptr` を使っており、`src/Mixer.cpp:80-99` のような「一時退避のための共有所有」まで発生しています。
  - これは `unique_ptr` で十分な場所を `shared_ptr` にしている例で、所有権を読みづらくします。
- 改善案/方針:
  - 基本は `unique_ptr` にしてください。
  - 再利用のための一時退避が必要なら `std::unique_ptr` の move で十分です。

## Good: 公開 API に raw pointer を漏らしていない
- 優先度: Low
- 対象: `include/*`
- 詳細と根拠:
  - ユーザーが触る公開ヘッダには raw pointer が存在せず、所有権を外へ漏らしていません。
- 改善案/方針:
  - 内部実装の RAII 化まで進めれば、設計としてかなり安定します。