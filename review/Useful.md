# Useful Review

- 評価: `Critical`

## Critical: `Shutdown()` 後に `Initialize()` が成功したように見えて何も初期化しない
- 優先度: Highest
- 対象: `src/AudioLib.cpp`
- 詳細と根拠:
  - `src/AudioLib.cpp:14-17` は `std::call_once` を通した最初の 1 回しか `Mixer::Init()` を呼びません。
  - `src/AudioLib.cpp:20-23` の `Shutdown()` 後に再度 `Initialize()` を呼んでも内部エンジンは復活しませんが、戻り値は `true` のままです。
  - これは利用者目線で最悪です。成功したと信じて後続 API を呼ぶと、再生だけが silently fail します。
- 改善案/方針:
  - `Initialize()` の成功条件を正しく返してください。
  - 多重初期化/多重終了の契約を明示し、違反時も沈黙させず失敗として返すべきです。

## Warning: ロード失敗を利用者が即座に判定できない
- 優先度: High
- 対象: `include/Handle.hpp`, `src/Handle.cpp`, `src/Loader.cpp`, `src/Repository.cpp`
- 詳細と根拠:
  - `Handle` コンストラクタはロード失敗時にも例外を投げず、`handle_ == 0` の無効状態で終わります。
  - しかし `Handle` には `IsValid()` がなく、利用者は `Play()` が失敗するまで異常に気づけません。
  - 「使いやすいラッパ」を目指すなら、失敗を遅延させるより入口で分かる方が圧倒的に親切です。
- 改善案/方針:
  - `Handle::IsValid()` を追加してください。
  - 可能なら `LoadResult` のように失敗理由も返せる設計にすると実運用で助かります。

## Warning: コメントの文字化けがあり、保守性を落としている
- 優先度: Medium
- 対象: `src/Repository.hpp`, `src/Repository.cpp`, `src/Sound.hpp`
- 詳細と根拠:
  - 上記ファイルのコメントに文字化けが混在しています。
  - ライブラリ利用者には見えない内部コメントでも、将来の保守担当にとっては重要な設計情報です。読めないコメントは実質コメントなしと同じです。
- 改善案/方針:
  - UTF-8 へ統一し、既存コメントを読める状態に戻してください。
  - ついでに「何を保証する型か」「ライフタイムはどこが持つか」を簡潔に書き直すと有益です。

## Good: 公開 API は薄く、最低限の操作に絞れている
- 優先度: Low
- 対象: `include/AudioLib.hpp`, `include/Handle.hpp`, `include/PlaybackHandle.hpp`, `include/TrackHandle.hpp`
- 詳細と根拠:
  - 初期化、ロード、再生、停止、トラック制御という主要操作は最小限に絞られています。
  - 内部 miniaudio 知識を利用者へ要求しないという目標には沿っています。
- 改善案/方針:
  - 今後は API 数を増やすより、異常系と再初期化の契約を固める方が優先です。