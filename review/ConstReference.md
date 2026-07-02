# ConstReference Review

- 評価: `Warning`

## Good: 文字列引数に `std::string_view` を採用している
- 優先度: Low
- 対象: `include/Handle.hpp`, `src/Loader.hpp`, `src/Repository.hpp`
- 詳細と根拠:
  - `Handle(std::string_view _path)`, `Loader::Load(std::string_view _path)`, `Repository::Contains(std::string_view _name)` など、文字列受けには `std::string_view` を選べています。
  - 読み取り専用入力に対して所有権を要求しない設計は妥当です。
- 改善案/方針:
  - これは維持して問題ありません。

## Warning: `float` と `bool` を `const&` で受けており、x64 前提では逆効果
- 優先度: High
- 対象: `include/Handle.hpp`, `include/PlaybackHandle.hpp`, `src/Handle.cpp`, `src/PlaybackHandle.cpp`
- 詳細と根拠:
  - `include/Handle.hpp:28-32` で `float` / `bool` / `TrackHandle` を `const&` 受けしています。
  - `include/PlaybackHandle.hpp:33,38,43,48,53` も同様に `float` / `bool` を `const&` 受けしています。
  - `prompt.md` にある通り、x64 では 8 byte 以下の組み込み型は値渡しが基本で、参照化するとかえって間接参照が増えます。
- 改善案/方針:
  - `float` と `bool` は値渡しへ変更してください。
  - `TrackHandle` も内部が `uint64_t` 1 個なので値渡しで十分です。

## Warning: `Repository` が `string_view` を受けつつ毎回 `std::string` 化している
- 優先度: Medium
- 対象: `src/Repository.cpp`
- 詳細と根拠:
  - `src/Repository.cpp:7`, `14`, `25`, `29` で `std::string(_name)` を都度生成しています。
  - `string_view` の利点を受け口で潰しており、ホットパス化した場合に無駄な確保が増えます。
- 改善案/方針:
  - 受け口を `std::string_view` にするのはよいですが、検索側は透過比較を使うか、所有側/検索側の型戦略を揃えて不要変換を減らしてください。