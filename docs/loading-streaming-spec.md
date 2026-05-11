# Loading And Streaming Spec

## 0. 目的

この仕様は次の 3 点を同時に満たすためのものです。

- 公開 API をシンプルなまま保つ
- ロードを内部で並列化する
- static / streaming を内部で自動選択する

ユーザーには「`Handle(path)` を作って `Play()` する」体験を維持しつつ、内部で非同期化と最適化を進める。

## 1. 基本方針

- `Audio::Handle(path)` は同期ブロックしない
- `Handle` はロード状態に関係なく返す
- ロード状態は当面公開しない
- `Play()` が未ロードのアセットに対して呼ばれても安全に無視する
- 無視時は初回のみログを出す
- 同一パスのロード要求は 1 回の実ロードに統合する
- static / streaming の選択は内部で行う

## 2. ユーザー向け挙動

### 2.1 Handle

- `Handle(path)` は常に生成できる
- ファイルが存在しない場合や decode 不能でも `Handle` 自体は返す
- ユーザーは状態確認 API を使わない
- 問題はログで確認する

### 2.2 Play

- 未ロード時の `Play()` は安全に no-op
- ロード完了後に再度 `Play()` された場合は通常再生
- 自動再生予約はしない
- 失敗ログはファイル単位で初回のみ

### 2.3 Unload

将来 `Handle::Unload()` を追加する。

- `Unload()` は強制的にアセット実体を repository から外す
- 他の `Handle` が同じパスを参照していても実体は消える
- ただし再生中インスタンスは最後まで鳴らす
- `Unload()` 後に `Play()` された場合は再ロードを試みる

## 3. 内部モデル

## 3.1 Asset Key

初期段階の同一性判定は文字列一致のみ。

- key: `std::string path`
- 将来は絶対パス化や正規化を検討

## 3.2 Asset State

状態は内部のみで使う。

```cpp
enum class AssetState {
    Unloaded,
    Loading,
    Ready,
    Failed
};
```

## 3.3 Asset Type

```cpp
enum class AssetStorageKind {
    StaticDecoded,
    StreamingPrepared
};
```

### StaticDecoded

- 全 PCM を保持
- 既存 `Sound` 系の延長

### StreamingPrepared

- ファイルパス
- probe メタデータ
- streaming 再生に必要な初期情報

## 4. 必要なメタデータ

ロード方針判定のため、フル decode 前に `Probe` を行う。

```cpp
struct AssetMeta {
    uint32_t sampleRate = 0;
    uint32_t channels = 0;
    uint64_t totalFrames = 0;
    double   durationSec = 0.0;
    uint64_t estimatedDecodedBytes = 0;
    bool     probeSucceeded = false;
};
```

## 5. ロードフロー

## 5.1 Handle(path)

`Handle(path)` 呼び出し時の内部フロー:

1. path を key にして repository を引く
2. `Ready` なアセットがあればそれを参照する `Handle` を返す
3. `Loading` 中なら同じ pending entry を参照する `Handle` を返す
4. `Unloaded` / 未登録なら pending entry を作って load queue へ積む
5. `Handle` を返す

## 5.2 実ロード

worker thread 側の流れ:

1. `Probe(path)`
2. `LoadPolicy::Decide(meta)`
3. `StaticLoad(path)` または `PrepareStreaming(path)`
4. repository へ commit
5. `AssetState` を `Ready` または `Failed` へ更新

## 5.3 重複ロード統合

- 同一 path は 1 つの pending entry を共有する
- 2 回目以降の要求は実ロードを増やさない
- `Handle` はすべて同一の内部 asset entry を指す

## 6. 並列ロード

## 6.1 Thread Pool

- 内部に固定サイズ thread pool を持つ
- 並列数は自動決定
- 初期値は `min(4, hardware_concurrency)` を想定

## 6.2 優先度

- 初期実装では優先度なし
- 将来追加余地は残す

## 6.3 キャンセル

- 初期実装では不要

## 7. 自動 static / streaming 判定

## 7.1 方針

- ユーザーにはカテゴリやヒントを強制しない
- 判定は完全自動寄り
- 主目的は短期的には再生体験の高速化、長期的にはメモリ効率

## 7.2 判定入力

- `durationSec`
- `estimatedDecodedBytes`
- `channels`
- `sampleRate`

## 7.3 初期判定ルール

初期ルールは単純でよい。

- 短くて軽いものは static
- 長くて重いものは streaming
- `SE` でも大きければ streaming
- `BGM` でも十分小さければ static になり得る

実際の閾値は実装時に定数として切り出す。

```cpp
struct LoadThresholds {
    double   streamingMinDurationSec = 10.0;
    uint64_t streamingMinDecodedBytes = 16ull * 1024ull * 1024ull;
};
```

## 8. Static 再生

- 既存 `ma_engine + ma_audio_buffer_ref + ma_sound` を当面活かす
- `Sound` あるいは後継 `StaticAudioAsset` に PCM を保持

## 9. Streaming 再生

## 9.1 目的

- 長尺アセットを全 PCM 化せず再生する
- `Pause/Resume/Stop/Loop` は static 再生に近い触り心地にする

## 9.2 初期要件

- `Pause`
- `Resume`
- `Stop`
- `Loop`
- `Mute`
- `Volume`
- `Pan`
- `Seek(seconds)` は static と同タイミングで実装
- `Pitch` は後回し

## 9.3 実装方針

段階導入とする。

1. static 再生は既存経路を維持
2. streaming 再生のみ専用 voice を追加
3. 必要があれば後に抽象化する

## 9.4 Streaming Voice

```cpp
struct StreamingVoiceConfig {
    uint32_t prefetchFrames = 0;
    uint32_t ringBufferFrames = 0;
    float    refillLowWatermark = 0.25f;
    bool     loop = false;
};
```

概念:

- voice ごとに decoder を持つ
- PCM ring buffer を持つ
- low watermark を切ったら refill を要求
- loop 時はシームレス性を将来改善

## 10. Playback と Asset の寿命分離

`Unload()` や repository からの削除があっても、再生中インスタンスは鳴り切る必要がある。

そのため、再生インスタンスは asset 実体への独立参照を持つ。

- repository から消えても playback は生きる
- playback 終了後に最後の参照が消える
- static / streaming の両方で同じ考え方を取る

## 11. ログ

初期段階では `cout` 系で十分。

ログ対象:

- 初回ロード失敗
- 未ロードアセットへの初回 `Play()`
- streaming 準備失敗

将来:

- `Log::Send(msg)` への接続
- レベルは `Info / Warning / Error`

## 12. 将来拡張

- path 正規化
- ユーザー向け logger 差し替え
- 明示 `LoadHint`
- ロード優先度
- 進捗取得
- キャッシュ解放ポリシー
- `Handle::Unload()`

## 13. 実装順

1. `Probe` 導入
2. thread-safe repository / pending registry
3. 非同期ロード
4. 自動判定
5. static / streaming 分岐
6. streaming voice
7. `Seek(seconds)` の導入
