# Functions Review

- 評価: `Warning`

## Good: API の粒度は小さく、利用者視点では追いやすい
- 優先度: Low
- 対象: `include/AudioLib.hpp`, `include/Handle.hpp`, `include/PlaybackHandle.hpp`, `include/TrackHandle.hpp`
- 詳細と根拠:
  - `Initialize`, `Shutdown`, `CreateTrack`, `DestroyTrack`, `Play`, `Pause`, `Resume` など、公開関数は役割ごとに分かれており、用途が読み取りやすいです。
  - 音声ライブラリとして「知らなくても動く」に寄せた API 形状にはなっています。
- 改善案/方針:
  - 公開 API の薄さは維持してよいです。

## Warning: 停止処理の重複が多く、将来の挙動不一致を招きやすい
- 優先度: High
- 対象: `src/Mixer.cpp`
- 詳細と根拠:
  - `src/Mixer.cpp:145-152`, `154-163`, `239-247` で、`ma_sound_stop` と PCM frame の巻き戻し、`state` 更新がほぼ同じ形で重複しています。
  - 音声ライブラリは pause/stop/reuse の違いがバグに直結しやすく、重複実装は修正漏れの温床です。
  - 現時点でも「停止後の再利用」「Pause との差異」「Track 側の一括停止」が別々に散らばっており、挙動の整合性確認コストが高いです。
- 改善案/方針:
  - `StopInstance(PlaybackInstance&, StopMode)` のような内部 helper に寄せ、状態遷移を 1 箇所で定義してください。
  - `Cleanup()` や再利用判定も同じ状態遷移モデルにぶら下げる方が安全です。

## Warning: `Handle` のコンストラクタがロード、副作用、キャッシュ解決を兼ねていて責務が重い
- 優先度: Medium
- 対象: `src/Handle.cpp`, `include/Handle.hpp`
- 詳細と根拠:
  - `src/Handle.cpp:7-20` では、既存キャッシュ確認、ロード、登録までをコンストラクタ内で一気に行っています。
  - コンストラクタで重い処理と失敗可能処理を行う設計は、関数境界としての見通しを悪くし、エラー通知方法も貧弱になります。
- 改善案/方針:
  - `Handle` は軽量ハンドルに徹し、`Load(path)` や `CreateHandle(path)` のような明示関数へ寄せた方が責務分離しやすいです。